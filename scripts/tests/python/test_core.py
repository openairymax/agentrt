#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS Core 模块单元测试

"""
AgentOS Core 模块单元测试

测试覆盖：
- plugin.py: 插件系统
- events.py: 事件总线
- security.py: 安全模块
- telemetry.py: 遥测模块
"""

import asyncio
import json
import os
import sys
import tempfile
import time
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

# 添加项目路径
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "scripts"))

from scripts.core.plugin import (
    Plugin, PluginContext, PluginMetadata, PluginRegistry,
    PluginResult, PluginState
)
from scripts.core.events import (
    Event, EventBus, EventHandler, EventPriority, EventType
)
from scripts.core.security import (
    InputValidator, SecurityConfig, SecurityLevel, SecurityManager,
    ValidationResult
)
from scripts.core.telemetry import (
    MetricsCollector, Metric, MetricType, Timer
)


###############################################################################
# Plugin System Tests
###############################################################################

class TestPluginMetadata:
    """插件元数据测试"""

    def test_metadata_creation(self):
        """测试元数据创建"""
        metadata = PluginMetadata(
            name="test_plugin",
            version="1.0.0",
            author="Test Author",
            description="A test plugin"
        )
        assert metadata.name == "test_plugin"
        assert metadata.version == "1.0.0"
        assert metadata.author == "Test Author"

    def test_metadata_defaults(self):
        """测试默认值"""
        metadata = PluginMetadata(name="test")
        assert metadata.version == "1.0.0"
        assert metadata.dependencies == []
        assert metadata.tags == []


class TestPluginContext:
    """插件上下文测试"""

    def test_context_creation(self):
        """测试上下文创建"""
        ctx = PluginContext(
            plugin_id="test",
            working_dir="/tmp"
        )
        assert ctx.plugin_id == "test"
        assert ctx.working_dir == "/tmp"
        assert ctx.trace_id.startswith("test-")

    def test_context_with_parent_trace(self):
        """测试带父追踪ID的上下文"""
        ctx = PluginContext(
            plugin_id="child",
            working_dir="/tmp",
            parent_trace_id="parent-123"
        )
        assert ctx.parent_trace_id == "parent-123"


class MockPlugin(Plugin):
    """模拟插件用于测试"""

    def __init__(self):
        super().__init__()
        self.metadata = PluginMetadata(name="mock_plugin", version="1.0.0")
        self.init_called = False
        self.execute_called = False
        self.shutdown_called = False

    def initialize(self, config) -> bool:
        self.init_called = True
        return True

    def execute(self, ctx: PluginContext) -> PluginResult:
        self.execute_called = True
        return PluginResult(
            plugin_id=self.name,
            success=True,
            output={"result": "ok"}
        )

    def shutdown(self):
        self.shutdown_called = True


class TestPluginRegistry:
    """插件注册表测试"""

    def test_register_plugin(self):
        """测试插件注册"""
        registry = PluginRegistry()
        plugin = MockPlugin()
        result = registry.register(plugin)
        assert result is True
        assert registry.get("mock_plugin") is plugin

    def test_register_duplicate(self):
        """测试重复注册"""
        registry = PluginRegistry()
        plugin1 = MockPlugin()
        plugin2 = MockPlugin()
        registry.register(plugin1)
        result = registry.register(plugin2)
        assert result is False

    def test_unregister_plugin(self):
        """测试插件注销"""
        registry = PluginRegistry()
        plugin = MockPlugin()
        registry.register(plugin)
        result = registry.unregister("mock_plugin")
        assert result is True
        assert registry.get("mock_plugin") is None

    def test_execute_plugin(self):
        """测试插件执行"""
        registry = PluginRegistry()
        plugin = MockPlugin()
        registry.register(plugin)
        result = registry.execute_plugin("mock_plugin")
        assert result is not None
        assert result.success is True
        assert plugin.execute_called is True

    def test_discover_plugins(self):
        """测试插件发现"""
        registry = PluginRegistry()
        with tempfile.TemporaryDirectory() as tmpdir:
            plugin_meta = {"name": "discovered", "version": "1.0.0"}
            meta_file = Path(tmpdir) / "plugin.json"
            meta_file.write_text(json.dumps(plugin_meta))

            discovered = registry.discover_plugins(tmpdir)
            assert len(discovered) == 1
            assert discovered[0].name == "discovered"


###############################################################################
# Event System Tests
###############################################################################

class TestEvent:
    """事件测试"""

    def test_event_creation(self):
        """测试事件创建"""
        event = Event(type=EventType.BUILD_STARTED, source="test")
        assert event.type == EventType.BUILD_STARTED
        assert event.source == "test"
        assert event.id is not None

    def test_event_to_dict(self):
        """测试事件序列化"""
        event = Event(
            type=EventType.METRIC_RECORDED,
            source="test",
            data={"value": 42}
        )
        d = event.to_dict()
        assert d["type"] == "metric.recorded"
        assert d["data"]["value"] == 42

    def test_event_from_dict(self):
        """测试事件反序列化"""
        data = {
            "id": "test-123",
            "type": "build.completed",
            "priority": 1,
            "source": "test",
            "data": {},
            "trace_id": ""
        }
        event = Event.from_dict(data)
        assert event.id == "test-123"
        assert event.type == EventType.BUILD_COMPLETED


class MockEventHandler(EventHandler):
    """模拟事件处理器"""

    def __init__(self):
        super().__init__("mock_handler", [EventType.ERROR_OCCURRED])
        self.handled_events = []

    def handle(self, event: Event) -> bool:
        self.handled_events.append(event)
        return True


class TestEventBus:
    """事件总线测试"""

    def test_publish_subscribe(self):
        """测试发布订阅"""
        bus = EventBus()
        handler = MockEventHandler()
        bus.subscribe(handler)

        event = Event(type=EventType.ERROR_OCCURRED, source="test")
        bus.publish(event)

        assert len(handler.handled_events) == 1
        assert handler.handled_events[0].id == event.id

    def test_unsubscribe(self):
        """测试取消订阅"""
        bus = EventBus()
        handler = MockEventHandler()
        bus.subscribe(handler)
        bus.unsubscribe("mock_handler")

        event = Event(type=EventType.ERROR_OCCURRED, source="test")
        bus.publish(event)

        assert len(handler.handled_events) == 0

    def test_history(self):
        """测试事件历史"""
        bus = EventBus(max_history=10)
        for i in range(5):
            bus.publish(Event(type=EventType.BUILD_STARTED, source="test"))

        history = bus.get_history()
        assert len(history) == 5

        history = bus.get_history(event_type=EventType.BUILD_STARTED)
        assert len(history) == 5

    def test_statistics(self):
        """测试统计信息"""
        bus = EventBus()
        for i in range(3):
            bus.publish(Event(type=EventType.BUILD_STARTED, source="test"))
        bus.publish(Event(type=EventType.ERROR_OCCURRED, source="test"))

        stats = bus.get_statistics()
        assert stats["total_events"] == 4
        assert stats["subscriptions"] == 0


###############################################################################
# Security Tests
###############################################################################

class TestSecurityManager:
    """安全管理器测试"""

    def test_validate_path_safe(self):
        """测试安全路径"""
        manager = SecurityManager()
        result = manager.validate_path("/tmp/agentos_test")
        assert result.valid is True

    def test_validate_path_traversal(self):
        """测试路径遍历攻击"""
        manager = SecurityManager()
        result = manager.validate_path("/tmp/../../../etc/passwd")
        assert result.valid is False

    def test_validate_path_too_long(self):
        """测试过长路径"""
        manager = SecurityManager(SecurityConfig(max_path_length=100))
        long_path = "/tmp/" + "a" * 200
        result = manager.validate_path(long_path)
        assert result.valid is False

    def test_sanitize_string(self):
        """测试字符串净化"""
        manager = SecurityManager()
        result = manager.sanitize_string("  hello\x00world  ")
        assert "\x00" not in result
        assert result == "  helloworld  "

    def test_sanitize_for_shell(self):
        """测试 shell 净化"""
        manager = SecurityManager()
        result = manager.sanitize_for_shell("hello world")
        assert result.startswith("'") or result.startswith('"')


class TestInputValidator:
    """输入验证器测试"""

    def test_validate_email(self):
        """测试邮箱验证"""
        validator = InputValidator()
        assert validator.validate_email("test@example.com").valid is True
        assert validator.validate_email("invalid").valid is False

    def test_validate_url(self):
        """测试 URL 验证"""
        validator = InputValidator()
        assert validator.validate_url("https://example.com").valid is True
        assert validator.validate_url("not-a-url").valid is False

    def test_validate_version(self):
        """测试版本验证"""
        validator = InputValidator()
        assert validator.validate_version("1.0.0").valid is True
        assert validator.validate_version("1.0.0-beta").valid is True
        assert validator.validate_version("invalid").valid is False

    def test_validate_port(self):
        """测试端口验证"""
        validator = InputValidator()
        assert validator.validate_port(8080).valid is True
        assert validator.validate_port(0).valid is False
        assert validator.validate_port(70000).valid is False


###############################################################################
# Telemetry Tests
###############################################################################

class TestMetricsCollector:
    """指标收集器测试"""

    def test_counter(self):
        """测试计数器"""
        collector = MetricsCollector()
        collector.counter("requests", 1)
        collector.counter("requests", 1)
        collector.counter("requests", 1)
        assert collector._counters["requests"] == 3

    def test_gauge(self):
        """测试仪表"""
        collector = MetricsCollector()
        collector.gauge("cpu_usage", 75.5)
        assert collector._gauges["cpu_usage"] == 75.5

    def test_histogram(self):
        """测试直方图"""
        collector = MetricsCollector()
        collector.histogram("latency", 10.5)
        collector.histogram("latency", 20.5)
        assert len(collector._histograms["latency"]) == 2

    def test_timing(self):
        """测试计时"""
        collector = MetricsCollector()
        collector.timing("operation", 150.0)
        assert "operation.duration_ms" in collector._histograms

    def test_export_prometheus(self):
        """测试 Prometheus 导出"""
        collector = MetricsCollector()
        collector.counter("requests", 100)
        collector.gauge("cpu_usage", 50.0)

        output = collector.export_prometheus()
        assert "# TYPE requests counter" in output
        assert "# TYPE cpu_usage gauge" in output

    def test_timer_context(self):
        """测试计时器上下文"""
        collector = MetricsCollector()
        with Timer(collector, "test_operation"):
            time.sleep(0.01)

        assert "test_operation.duration_ms" in collector._histograms


###############################################################################
# 运行测试
###############################################################################

if __name__ == "__main__":
    pytest.main([__file__, "-v"])