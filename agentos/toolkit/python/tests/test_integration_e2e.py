# AgentOS SDK 端到端集成测试
# 验证: Config→Client→Modules→Plugin→ErrorHandling 完整链路
# Version: 1.0.0

import sys
import os
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from agentos.framework.plugin import (
    PluginRegistry, PluginManager, PluginState,
    PluginManifest, BasePlugin, get_plugin_registry,
)
from agentos.framework.plugins.logger_plugin import LoggerPlugin
from agentos.framework.plugins.metrics_plugin import MetricsPlugin
from agentos.exceptions import (
    AgentOSError, NetworkError, ValidationError,
    TimeoutError, CODE_NETWORK_ERROR, CODE_TIMEOUT,
    CODE_VALIDATION_ERROR, CODE_UNKNOWN, CODE_INTERNAL,
)
from agentos.framework.event import EventBus


class TestConfigClientIntegration(unittest.TestCase):
    """测试配置与客户端基础集成"""

    def test_plugin_manifest_config(self):
        manifest = PluginManifest(
            plugin_id="test_cfg",
            name="Test",
            version="1.0.0",
            entry_point="http://localhost:18789",
        )
        self.assertEqual(manifest.plugin_id, "test_cfg")
        self.assertEqual(manifest.entry_point, "http://localhost:18789")

    def test_manifest_with_dependencies(self):
        manifest = PluginManifest(
            plugin_id="dep_test",
            name="Dep Test",
            version="1.0.0",
            dependencies=[
                {"plugin_id": "core", "version_range": "^1.0"},
                {"plugin_id": "utils", "version_range": "^2.0", "optional": True},
            ],
        )
        self.assertEqual(len(manifest.dependencies), 2)
        self.assertTrue(manifest.dependencies[1].get("optional"))


class TestErrorHandlingIntegration(unittest.TestCase):
    """测试错误处理链路"""

    def test_error_hierarchy(self):
        err = ValidationError("bad input")
        self.assertIsInstance(err, AgentOSError)
        self.assertEqual(err.error_code, CODE_VALIDATION_ERROR)

    def test_error_cause_chain(self):
        root = ValueError("root cause")
        err = NetworkError("network failed", cause=root)
        self.assertIn("root cause", str(err))

    def test_error_code_formatting(self):
        self.assertTrue(CODE_INTERNAL.startswith("0x"))
        self.assertTrue(CODE_UNKNOWN.startswith("0x"))

    def test_timeout_error(self):
        err = TimeoutError("request timed out")
        self.assertEqual(err.error_code, CODE_TIMEOUT)

    def test_network_error(self):
        err = NetworkError("connection refused")
        self.assertEqual(err.error_code, CODE_NETWORK_ERROR)


class TestPluginStateIntegration(unittest.TestCase):
    """测试 Plugin 状态转换"""

    def setUp(self):
        self.registry = PluginRegistry()

    def test_full_state_lifecycle(self):
        pid = self.registry.register(LoggerPlugin)
        self.assertEqual(self.registry.get_state(pid), PluginState.DISCOVERED)

        self.registry.load(pid)
        self.assertEqual(self.registry.get_state(pid), PluginState.LOADED)

        self.registry.unload(pid)
        self.assertEqual(self.registry.get_state(pid), PluginState.UNLOADED)

    def test_manager_load_activate_cycle(self):
        pm = PluginManager(sandbox_enabled=False)
        manifest = PluginManifest(
            plugin_id="e2e_state_test",
            name="E2E State Test",
            version="1.0.0",
        )

        import asyncio
        info = asyncio.run(
            pm.load_plugin("", manifest_override=manifest)
        )
        self.assertIsNotNone(info)
        self.assertEqual(info.state, PluginState.LOADED)

        asyncio.run(pm.activate_plugin("e2e_state_test"))
        self.assertEqual(info.state, PluginState.ACTIVE)


class TestEventPluginIntegration(unittest.TestCase):
    """测试 Event ↔ Plugin 集成"""

    def test_registry_operations(self):
        registry = PluginRegistry()
        pid = registry.register(LoggerPlugin)
        plugins = registry.discover()
        self.assertEqual(len(plugins), 1)

        logging_plugins = registry.discover(capability="logging")
        self.assertEqual(len(logging_plugins), 1)


class TestFullSDKIntegration(unittest.TestCase):
    """完整SDK集成测试：Config→Error→State→Plugin→Event"""

    def test_full_initialization_chain(self):
        registry = get_plugin_registry()
        registry.register(LoggerPlugin)
        registry.register(MetricsPlugin)

        plugins = registry.discover()
        self.assertEqual(len(plugins), 2)

        logger_inst = registry.load("LoggerPlugin")
        self.assertIsNotNone(logger_inst)

        metrics_inst = registry.load("MetricsPlugin")
        self.assertIsNotNone(metrics_inst)

        logger_inst.log("INFO", "integration test")
        metrics_inst.increment("requests", 1)

        self.assertEqual(logger_inst.count(), 1)
        self.assertEqual(metrics_inst.get_counter("requests"), 1.0)

        registry.unload("LoggerPlugin")
        registry.unload("MetricsPlugin")

    def test_multi_capability_discovery(self):
        registry = PluginRegistry()
        registry.register(LoggerPlugin)
        registry.register(MetricsPlugin)

        log_plugins = registry.find_by_capability("logging")
        metric_plugins = registry.find_by_capability("metrics")

        self.assertIn("LoggerPlugin", log_plugins)
        self.assertIn("MetricsPlugin", metric_plugins)
        self.assertNotIn("LoggerPlugin", metric_plugins)

    def test_error_in_plugin_context(self):
        registry = PluginRegistry()
        instance = registry.register(LoggerPlugin)
        loaded = registry.load(instance)
        self.assertIsNotNone(loaded)

        loaded.log("ERROR", "test error message")
        entries = loaded.query(level="ERROR")
        self.assertEqual(len(entries), 1)
        self.assertIn("test error message", entries[0]["message"])


class TestCrossModuleDataFlow(unittest.TestCase):
    """跨模块数据流测试"""

    def test_metrics_logger_interaction(self):
        registry = PluginRegistry()
        registry.register(LoggerPlugin)
        registry.register(MetricsPlugin)

        logger = registry.load("LoggerPlugin")
        metrics = registry.load("MetricsPlugin")

        for i in range(5):
            logger.log("INFO", f"request-{i}")
            metrics.increment("total_requests", 1)

        self.assertEqual(logger.count(), 5)
        self.assertAlmostEqual(metrics.get_counter("total_requests"), 5.0)

    def test_plugin_info_persistence(self):
        registry = PluginRegistry()
        custom_manifest = PluginManifest(
            plugin_id="custom",
            name="Custom Plugin",
            version="2.0.0",
            description="A test plugin",
            capabilities=["custom_cap"],
            tags=["test", "e2e"],
        )
        registry.register(LoggerPlugin, manifest=custom_manifest)

        stored = registry.get_manifest("LoggerPlugin")
        self.assertEqual(stored.name, "Custom Plugin")
        self.assertEqual(stored.version, "2.0.0")
        self.assertIn("custom_cap", stored.capabilities)
        self.assertIn("test", stored.tags)


if __name__ == "__main__":
    unittest.main()
