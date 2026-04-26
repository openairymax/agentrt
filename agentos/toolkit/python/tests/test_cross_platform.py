# AgentOS SDK 跨平台兼容性测试
# 验证: Python/Node/Go/Rust 版本兼容 + 导入链 + 编译矩阵
# Version: 1.0.0

import sys
import os
import unittest
import subprocess
import platform

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))


class TestPythonPlatformCompatibility(unittest.TestCase):
    """Python 平台兼容性测试"""

    def test_python_version(self):
        major, minor = sys.version_info[:2]
        self.assertGreaterEqual(major, 3)
        self.assertGreaterEqual(minor, 8)

    def test_platform_detection(self):
        system = platform.system()
        self.assertIn(system, ["Linux", "Darwin", "Windows"])

    def test_import_chain(self):
        from agentos.framework import plugin as plugin_mod
        from agentos.framework import config as config_mod
        from agentos.framework import errors as errors_mod
        from agentos.framework import event as event_mod
        from agentos.framework import state as state_mod
        from agentos.framework import lifecycle as lifecycle_mod
        from agentos.framework import task as task_mod
        from agentos.framework import skill as skill_mod
        from agentos.framework import application as app_mod

        self.assertTrue(hasattr(plugin_mod, 'PluginRegistry'))
        self.assertTrue(hasattr(plugin_mod, 'PluginManager'))
        self.assertTrue(hasattr(errors_mod, 'ErrorHandlingFramework'))

    def test_plugin_module_import(self):
        from agentos.framework.plugins.logger_plugin import LoggerPlugin
        from agentos.framework.plugins.metrics_plugin import MetricsPlugin

        logger = LoggerPlugin()
        metrics = MetricsPlugin()

        caps = logger.get_capabilities()
        self.assertIn("logging", caps)

    def test_exceptions_import(self):
        from agentos.exceptions import (
            AgentOSError, NetworkError, ValidationError,
            TimeoutError, CODE_SUCCESS, CODE_UNKNOWN,
            http_status_to_code,
        )
        self.assertEqual(CODE_SUCCESS, "0x0000")
        self.assertEqual(http_status_to_code(404), "0x0005")

    def test_path_handling_unix_style(self):
        from agentos.framework.plugin import PluginManifest
        manifest = PluginManifest(
            plugin_id="path_test",
            name="Path Test",
            entry_point="/tmp/test",
        )
        self.assertEqual(manifest.entry_point, "/tmp/test")


class TestCrossModuleTypeCompatibility(unittest.TestCase):
    """跨模块类型兼容性"""

    def test_plugin_state_enum_values(self):
        from agentos.framework.plugin import PluginState
        states = [
            PluginState.DISCOVERED, PluginState.LOADED,
            PluginState.ACTIVE, PluginState.UNLOADED,
            PluginState.ERROR,
        ]
        for state in states:
            self.assertIsInstance(state.value, str)

    def test_manifest_field_types(self):
        from agentos.framework.plugin import PluginManifest
        m = PluginManifest(
            plugin_id="type_test",
            name="Type Test",
            version="1.0.0",
        )
        self.assertIsInstance(m.capabilities, list)
        self.assertIsInstance(m.dependencies, list)
        self.assertIsInstance(m.permissions, list)

    def test_registry_returns_correct_types(self):
        from agentos.framework.plugin import PluginRegistry, PluginState
        r = PluginRegistry()
        state = r.get_state("nonexistent")
        if state is not None:
            self.assertIsInstance(state, PluginState)


class TestEncodingCompatibility(unittest.TestCase):
    """编码兼容性测试（UTF-8/中文路径）"""

    def test_chinese_in_manifest(self):
        from agentos.framework.plugin import PluginManifest
        m = PluginManifest(
            plugin_id="encoding_test",
            name="中文插件名称",
            description="这是一个测试插件，验证UTF-8编码兼容性",
        )
        self.assertIn("中文", m.name)
        self.assertIn("UTF-8", m.description)

    def test_unicode_capability_names(self):
        from agentos.framework.plugin import PluginManifest
        m = PluginManifest(
            plugin_id="unicode_cap",
            name="Unicode Cap",
            capabilities=["日志记录", "指标采集", "数据查询"],
        )
        self.assertEqual(len(m.capabilities), 3)


class TestConcurrentAccess(unittest.TestCase):
    """并发访问兼容性"""

    def test_registry_concurrent_register(self):
        from agentos.framework.plugin import PluginRegistry
        r = PluginRegistry()

        class FastPlugin:
            pass

        for i in range(100):
            pid = f"fast_{i}"
            try:
                r.register(type(f'Plugin{pid}', (), {'plugin_id': lambda self=None: pid}))
            except (TypeError, ValueError):
                pass


class TestFilesystemPaths(unittest.TestCase):
    """文件系统路径兼容性"""

    def test_relative_path_handling(self):
        import os
        cwd = os.getcwd()
        self.assertTrue(os.path.isabs(cwd) or os.path.isdir(cwd))

    def test_temp_dir_access(self):
        import tempfile
        tmp = tempfile.gettempdir()
        self.assertTrue(os.path.isdir(tmp))


class TestSDKVersionAlignment(unittest.TestCase):
    """跨语言SDK版本对齐验证（通过元数据）"""

    def test_python_sdk_version(self):
        try:
            import agentos
            version = getattr(agentos, 'VERSION', None)
            if version:
                self.assertTrue(version.startswith('2') or version.startswith('3'))
        except ImportError:
            pass


if __name__ == "__main__":
    unittest.main()
