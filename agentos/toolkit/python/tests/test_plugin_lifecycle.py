# AgentOS PluginSDK 生命周期测试
# 验证: 注册→发现→加载→调用→卸载 完整生命周期

import asyncio
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


class SimplePlugin(BasePlugin):
    __version__ = "0.1.0"

    def __init__(self):
        super().__init__()
        self._loaded = False
        self._activated = False
        self._deactivated = False
        self._unloaded = False

    async def on_load(self, context):
        self._loaded = True

    async def on_activate(self, context):
        self._activated = True

    async def on_deactivate(self):
        self._deactivated = True

    async def on_unload(self):
        self._unloaded = True

    def get_capabilities(self):
        return ["test", "simple"]


class TestPluginRegistryLifecycle(unittest.TestCase):
    """测试 PluginRegistry 注册→发现→加载→调用→卸载 完整生命周期"""

    def setUp(self):
        self.registry = PluginRegistry()

    def test_register_discovers_plugin(self):
        pid = self.registry.register(LoggerPlugin)
        self.assertEqual(pid, "LoggerPlugin")
        discovered = self.registry.discover()
        self.assertEqual(len(discovered), 1)
        self.assertEqual(discovered[0].plugin_id, "LoggerPlugin")

    def test_register_multiple_plugins(self):
        self.registry.register(LoggerPlugin)
        self.registry.register(MetricsPlugin)
        self.assertEqual(len(self.registry.list_plugins()), 2)

    def test_register_rejects_non_baseplugin(self):
        with self.assertRaises(TypeError):
            self.registry.register(str)

    def test_register_rejects_duplicate(self):
        self.registry.register(LoggerPlugin)
        with self.assertRaises(ValueError):
            self.registry.register(LoggerPlugin)

    def test_discover_with_capability_filter(self):
        self.registry.register(LoggerPlugin)
        self.registry.register(MetricsPlugin)
        logging_plugins = self.registry.discover(capability="logging")
        self.assertEqual(len(logging_plugins), 1)
        self.assertEqual(logging_plugins[0].plugin_id, "LoggerPlugin")

    def test_load_creates_instance(self):
        self.registry.register(LoggerPlugin)
        instance = self.registry.load("LoggerPlugin")
        self.assertIsNotNone(instance)
        self.assertIsInstance(instance, LoggerPlugin)
        self.assertEqual(self.registry.get_state("LoggerPlugin"), PluginState.LOADED)

    def test_load_returns_same_instance(self):
        self.registry.register(LoggerPlugin)
        inst1 = self.registry.load("LoggerPlugin")
        inst2 = self.registry.load("LoggerPlugin")
        self.assertIs(inst1, inst2)

    def test_load_nonexistent_returns_none(self):
        result = self.registry.load("NonExistent")
        self.assertIsNone(result)

    def test_unload_removes_instance(self):
        self.registry.register(LoggerPlugin)
        self.registry.load("LoggerPlugin")
        result = self.registry.unload("LoggerPlugin")
        self.assertTrue(result)
        self.assertIsNone(self.registry.get("LoggerPlugin"))
        self.assertEqual(self.registry.get_state("LoggerPlugin"), PluginState.UNLOADED)

    def test_unload_nonexistent_returns_false(self):
        result = self.registry.unload("NonExistent")
        self.assertFalse(result)

    def test_unregister_removes_everything(self):
        self.registry.register(LoggerPlugin)
        self.registry.load("LoggerPlugin")
        result = self.registry.unregister("LoggerPlugin")
        self.assertTrue(result)
        self.assertNotIn("LoggerPlugin", self.registry.list_plugins())
        self.assertIsNone(self.registry.get_manifest("LoggerPlugin"))

    def test_find_by_capability(self):
        self.registry.register(LoggerPlugin)
        self.registry.register(MetricsPlugin)
        result = self.registry.find_by_capability("metrics")
        self.assertIn("MetricsPlugin", result)
        self.assertNotIn("LoggerPlugin", result)

    def test_get_manifest(self):
        self.registry.register(LoggerPlugin)
        manifest = self.registry.get_manifest("LoggerPlugin")
        self.assertIsNotNone(manifest)
        self.assertEqual(manifest.plugin_id, "LoggerPlugin")
        self.assertIn("logging", manifest.capabilities)

    def test_full_lifecycle(self):
        """完整生命周期: 注册→发现→加载→调用→卸载"""
        # 1. 注册
        pid = self.registry.register(LoggerPlugin)
        self.assertEqual(pid, "LoggerPlugin")

        # 2. 发现
        discovered = self.registry.discover()
        self.assertTrue(any(m.plugin_id == "LoggerPlugin" for m in discovered))

        # 3. 加载
        instance = self.registry.load("LoggerPlugin")
        self.assertIsNotNone(instance)
        self.assertEqual(self.registry.get_state("LoggerPlugin"), PluginState.LOADED)

        # 4. 调用
        count = instance.log("INFO", "test message")
        self.assertEqual(count, 1)
        entries = instance.query(level="INFO")
        self.assertEqual(len(entries), 1)
        self.assertEqual(entries[0]["message"], "test message")

        # 5. 卸载
        result = self.registry.unload("LoggerPlugin")
        self.assertTrue(result)
        self.assertIsNone(self.registry.get("LoggerPlugin"))
        self.assertEqual(self.registry.get_state("LoggerPlugin"), PluginState.UNLOADED)

    def test_full_lifecycle_metrics_plugin(self):
        """MetricsPlugin完整生命周期"""
        pid = self.registry.register(MetricsPlugin)
        self.assertEqual(pid, "MetricsPlugin")

        instance = self.registry.load("MetricsPlugin")
        self.assertIsNotNone(instance)

        instance.increment("requests", 5)
        self.assertEqual(instance.get_counter("requests"), 5.0)

        instance.set_gauge("cpu", 72.5)
        self.assertEqual(instance.get_gauge("cpu"), 72.5)

        result = self.registry.unload("MetricsPlugin")
        self.assertTrue(result)


class TestPluginManagerLifecycle(unittest.TestCase):
    """测试 PluginManager 异步生命周期"""

    def setUp(self):
        self.manager = PluginManager(sandbox_enabled=False)

    def _run(self, coro):
        return asyncio.get_event_loop().run_until_complete(coro)

    def test_load_and_activate(self):
        manifest = PluginManifest(
            plugin_id="test_plugin",
            name="Test Plugin",
            version="1.0.0",
            entry_point="",
        )
        info = self._run(self.manager.load_plugin("", manifest_override=manifest))
        self.assertIsNotNone(info)
        self.assertEqual(info.manifest.plugin_id, "test_plugin")
        self.assertEqual(info.state, PluginState.LOADED)

    def test_list_plugins_empty(self):
        plugins = self.manager.list_plugins()
        self.assertEqual(len(plugins), 0)

    def test_get_stats(self):
        stats = self.manager.get_stats()
        self.assertEqual(stats["total_plugins"], 0)
        self.assertTrue(stats["sandbox_enabled"] is False)


class TestPluginRegistryWithCustomManifest(unittest.TestCase):
    """测试自定义清单注册"""

    def setUp(self):
        self.registry = PluginRegistry()

    def test_register_with_custom_manifest(self):
        manifest = PluginManifest(
            plugin_id="custom_logger",
            name="Custom Logger",
            version="2.0.0",
            description="A custom logger plugin",
            capabilities=["logging", "custom"],
        )
        pid = self.registry.register(LoggerPlugin, manifest=manifest)
        self.assertEqual(pid, "LoggerPlugin")

        stored = self.registry.get_manifest("LoggerPlugin")
        self.assertEqual(stored.name, "Custom Logger")
        self.assertEqual(stored.version, "2.0.0")


class TestGetPluginRegistry(unittest.TestCase):
    """测试全局注册表单例"""

    def test_singleton(self):
        r1 = get_plugin_registry()
        r2 = get_plugin_registry()
        self.assertIs(r1, r2)


if __name__ == "__main__":
    unittest.main()
