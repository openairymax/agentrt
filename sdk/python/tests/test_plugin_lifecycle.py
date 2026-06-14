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
        try:
            loop = asyncio.get_running_loop()
        except RuntimeError:
            pass
        else:
            import concurrent.futures
            with concurrent.futures.ThreadPoolExecutor() as pool:
                return pool.submit(asyncio.run, coro).result(timeout=30)
        return asyncio.run(coro)

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


class TestLifecycleHookInvocation(unittest.TestCase):
    """测试生命周期钩子确实被调用（通过副作用验证）"""

    def setUp(self):
        self.registry = PluginRegistry()
        self.registry.register(LoggerPlugin)
        self.registry.register(MetricsPlugin)

    def test_load_calls_on_load_via_side_effect(self):
        instance = self.registry.load("LoggerPlugin", {"max_entries": 50})
        self.assertIsNotNone(instance)
        self.assertEqual(instance._max_entries, 50)

    def test_unload_clears_logs_via_on_unload(self):
        instance = self.registry.load("LoggerPlugin")
        instance.log("INFO", "before unload")
        self.assertEqual(instance.count(), 1)

        self.registry.unload("LoggerPlugin")
        self.assertEqual(instance.count(), 0)  # on_unload cleared logs

    def test_unload_clears_counters_via_on_unload(self):
        instance = self.registry.load("MetricsPlugin")
        instance.increment("test_counter", 42)
        self.assertAlmostEqual(instance.get_counter("test_counter"), 42.0)

        self.registry.unload("MetricsPlugin")
        self.assertAlmostEqual(instance.get_counter("test_counter"), 0.0)  # on_unload cleared

    def test_error_state_on_failed_load(self):
        class BrokenPlugin(BasePlugin):
            async def on_load(self, context):
                raise RuntimeError("broken")
            async def on_activate(self, context): pass
            async def on_deactivate(self): pass
            async def on_unload(self): pass
            async def on_error(self, error): pass
            def get_capabilities(self): return ['broken']

        pid = self.registry.register(BrokenPlugin)
        result = self.registry.load(pid)
        self.assertIsNone(result)
        self.assertEqual(self.registry.get_state(pid), PluginState.ERROR)


class TestLoadUnloadCycle(unittest.TestCase):
    """测试加载→卸载→重载循环"""

    def setUp(self):
        self.registry = PluginRegistry()
        self.registry.register(LoggerPlugin)

    def test_reload_creates_new_instance(self):
        inst1 = self.registry.load("LoggerPlugin")
        self.registry.unload("LoggerPlugin")
        inst2 = self.registry.load("LoggerPlugin")

        self.assertIsNotNone(inst1)
        self.assertIsNotNone(inst2)
        self.assertNotEqual(id(inst1), id(inst2))

    def test_state_transitions_through_cycle(self):
        self.assertEqual(
            self.registry.get_state("LoggerPlugin"), PluginState.DISCOVERED
        )

        self.registry.load("LoggerPlugin")
        self.assertEqual(
            self.registry.get_state("LoggerPlugin"), PluginState.LOADED
        )

        self.registry.unload("LoggerPlugin")
        self.assertEqual(
            self.registry.get_state("LoggerPlugin"), PluginState.UNLOADED
        )

        self.registry.load("LoggerPlugin")
        self.assertEqual(
            self.registry.get_state("LoggerPlugin"), PluginState.LOADED
        )

    def test_double_unload_returns_false(self):
        self.registry.load("LoggerPlugin")
        result1 = self.registry.unload("LoggerPlugin")
        result2 = self.registry.unload("LoggerPlugin")

        self.assertTrue(result1)
        self.assertFalse(result2)


class TestErrorHandlingInHooks(unittest.TestCase):
    """测试钩子中的错误处理"""

    def setUp(self):
        self.registry = PluginRegistry()

        class FailingLoadPlugin(BasePlugin):
            async def on_load(self, context):
                raise RuntimeError("Intentional load failure")

            async def on_activate(self, context): pass
            async def on_deactivate(self): pass
            async def on_unload(self): pass
            async def on_error(self, error): pass
            def get_capabilities(self):
                return ['fail']

        self.FailingLoadPlugin = FailingLoadPlugin

    def test_load_failure_sets_error_state(self):
        pid = self.registry.register(self.FailingLoadPlugin)
        instance = self.registry.load(pid)
        self.assertIsNone(instance)
        self.assertEqual(self.registry.get_state(pid), PluginState.ERROR)

    def test_failed_load_does_not_add_to_instances(self):
        pid = self.registry.register(self.FailingLoadPlugin)
        self.registry.load(pid)
        self.assertIsNone(self.registry.get(pid))


class TestConcurrentAccess(unittest.TestCase):
    """测试并发访问安全性"""

    def test_concurrent_register(self):
        import threading
        registry = PluginRegistry()
        errors = []

        def register_plugin(i):
            try:
                class ConcurrentPlugin(BasePlugin):
                    def __init__(self):
                        super().__init__()
                        self._pid = f"concurrent_{i}"

                    @property
                    def plugin_id(self):
                        return self._pid

                    async def on_load(self, context): pass
                    async def on_activate(self, context): pass
                    async def on_deactivate(self): pass
                    async def on_unload(self): pass
                    async def on_error(self, error): pass
                    def get_capabilities(self):
                        return ['concurrent']

                registry.register(ConcurrentPlugin)
            except Exception as e:
                errors.append(e)

        threads = [threading.Thread(target=register_plugin, args=(i,)) for i in range(50)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()

        self.assertEqual(len(errors), 0, f"Concurrent registration had errors: {errors}")
        self.assertEqual(len(registry.list_plugins()), 50)


if __name__ == "__main__":
    unittest.main()
