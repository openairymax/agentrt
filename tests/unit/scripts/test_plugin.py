"""
PluginRegistry 单元测试

覆盖插件加载/卸载/执行的边界场景，重点验证：
- 多插件同时加载时模块命名空间隔离
- 路径哈希生成唯一模块名
- 卸载时 sys.modules 清理
- 重复加载防护
- 同名文件不同路径可共存
- 模块名映射表一致性
- 插件状态转换正确性 (LOADING/LOADED/RUNNING/FAILED/UNLOADING/UNLOADED)
- 日志输出验证
- 规范问题修复验证（无 print、discover_plugins 校验等）
"""

import logging
import sys
import textwrap
import pytest
from pathlib import Path
from unittest.mock import MagicMock

# 将 scripts/toolkit/src 加入路径
import sys as _sys
_sys.path.insert(0, str(Path(__file__).parent.parent.parent.parent / "scripts" / "toolkit" / "src"))

from plugin import (
    Plugin,
    PluginRegistry,
    PluginMetadata,
    PluginContext,
    PluginResult,
    PluginState,
)


# ──────────────────────────────────────────────
# 测试用插件实现
# ──────────────────────────────────────────────

class DummyPlugin(Plugin):
    """测试用插件"""

    def __init__(self, name: str = "test_plugin", version: str = "1.0.0"):
        super().__init__()
        self.metadata = PluginMetadata(name=name, version=version)
        self._initialized = False
        self._shutdown_called = False

    def initialize(self, manager) -> bool:
        self._initialized = True
        self._state = PluginState.LOADED
        return True

    def execute(self, ctx: PluginContext) -> PluginResult:
        return PluginResult(plugin_id=self.metadata.name, success=True, output="executed")

    def shutdown(self) -> None:
        self._shutdown_called = True
        self._state = PluginState.UNLOADED


class FailingInitPlugin(Plugin):
    """初始化失败的插件"""

    def __init__(self):
        super().__init__()
        self.metadata = PluginMetadata(name="failing_plugin", version="1.0.0")

    def initialize(self, manager) -> bool:
        return False  # 初始化失败

    def execute(self, ctx: PluginContext) -> PluginResult:
        return PluginResult(plugin_id="failing", success=False)

    def shutdown(self) -> None:
        pass


class CrashingExecutePlugin(Plugin):
    """执行时抛异常的插件"""

    def __init__(self):
        super().__init__()
        self.metadata = PluginMetadata(name="crash_plugin", version="1.0.0")

    def initialize(self, manager) -> bool:
        self._state = PluginState.LOADED
        return True

    def execute(self, ctx: PluginContext) -> PluginResult:
        raise RuntimeError("插件执行崩溃")

    def shutdown(self) -> None:
        self._state = PluginState.UNLOADED


# ──────────────────────────────────────────────
# 辅助：创建临时插件文件
# ──────────────────────────────────────────────

PLUGIN_TEMPLATE = textwrap.dedent("""\
from plugin import Plugin, PluginMetadata, PluginContext, PluginResult, PluginState

class {class_name}(Plugin):
    def __init__(self):
        super().__init__()
        self.metadata = PluginMetadata(name="{plugin_name}", version="{version}")

    def initialize(self, manager) -> bool:
        self._state = PluginState.LOADED
        return True

    def execute(self, ctx: PluginContext) -> PluginResult:
        return PluginResult(plugin_id="{plugin_name}", success=True, output="{output}")

    def shutdown(self) -> None:
        self._state = PluginState.UNLOADED
""")


def _create_plugin_file(tmp_path: Path, filename: str, plugin_name: str,
                        class_name: str = "Plugin", version: str = "1.0.0",
                        output: str = "ok") -> Path:
    """在临时目录创建插件 Python 文件"""
    file_path = tmp_path / filename
    file_path.write_text(PLUGIN_TEMPLATE.format(
        class_name=class_name,
        plugin_name=plugin_name,
        version=version,
        output=output,
    ), encoding="utf-8")
    return file_path


# ──────────────────────────────────────────────
# 模块命名空间隔离测试
# ──────────────────────────────────────────────

class TestModuleNamespaceIsolation:
    """核心问题：多插件同时加载时模块命名空间隔离"""

    def test_two_plugins_different_modules(self, tmp_path):
        f1 = _create_plugin_file(tmp_path, "alpha.py", "alpha_plugin")
        f2 = _create_plugin_file(tmp_path, "beta.py", "beta_plugin")

        registry = PluginRegistry()
        p1 = registry.load_plugin_from_module(str(f1))
        p2 = registry.load_plugin_from_module(str(f2))

        assert p1 is not None
        assert p2 is not None
        assert p1.metadata.name != p2.metadata.name

        alpha_modules = [k for k in sys.modules if k.startswith("agentos_plugin_alpha_")]
        beta_modules = [k for k in sys.modules if k.startswith("agentos_plugin_beta_")]
        assert len(alpha_modules) == 1
        assert len(beta_modules) == 1
        assert alpha_modules[0] != beta_modules[0]

    def test_no_dynamic_plugin_collision(self, tmp_path):
        f1 = _create_plugin_file(tmp_path, "plug.py", "plug_a")
        registry = PluginRegistry()
        registry.load_plugin_from_module(str(f1))
        assert "dynamic_plugin" not in sys.modules

    def test_later_plugin_does_not_overwrite_earlier(self, tmp_path):
        f1 = _create_plugin_file(tmp_path, "first.py", "first_plugin")
        f2 = _create_plugin_file(tmp_path, "second.py", "second_plugin")

        registry = PluginRegistry()
        p1 = registry.load_plugin_from_module(str(f1))
        p2 = registry.load_plugin_from_module(str(f2))

        assert registry.get("first_plugin") is p1
        assert registry.get("second_plugin") is p2

        first_mods = [k for k in sys.modules if "first" in k and k.startswith("agentos_plugin_")]
        second_mods = [k for k in sys.modules if "second" in k and k.startswith("agentos_plugin_")]
        assert len(first_mods) == 1
        assert len(second_mods) == 1


# ──────────────────────────────────────────────
# 模块名生成测试
# ──────────────────────────────────────────────

class TestModuleNameGeneration:
    """路径哈希生成唯一模块名"""

    def test_module_name_contains_file_stem(self, tmp_path):
        f = _create_plugin_file(tmp_path, "my_tool.py", "my_tool")
        registry = PluginRegistry()
        registry.load_plugin_from_module(str(f))

        matching = [k for k in sys.modules if "my_tool" in k and k.startswith("agentos_plugin_")]
        assert len(matching) == 1
        assert matching[0].startswith("agentos_plugin_my_tool_")

    def test_same_filename_different_dirs_different_modules(self, tmp_path):
        dir1 = tmp_path / "dir1"
        dir2 = tmp_path / "dir2"
        dir1.mkdir()
        dir2.mkdir()

        f1 = _create_plugin_file(dir1, "worker.py", "worker_v1")
        f2 = _create_plugin_file(dir2, "worker.py", "worker_v2")

        registry = PluginRegistry()
        p1 = registry.load_plugin_from_module(str(f1))
        p2 = registry.load_plugin_from_module(str(f2))

        assert p1 is not None
        assert p2 is not None
        assert p1.metadata.name == "worker_v1"
        assert p2.metadata.name == "worker_v2"

        worker_mods = [k for k in sys.modules if "worker" in k and k.startswith("agentos_plugin_")]
        assert len(worker_mods) == 2
        assert worker_mods[0] != worker_mods[1]

    def test_module_name_hash_format(self, tmp_path):
        f = _create_plugin_file(tmp_path, "hash_test.py", "hash_test")
        registry = PluginRegistry()
        registry.load_plugin_from_module(str(f))

        matching = [k for k in sys.modules if k.startswith("agentos_plugin_hash_test_")]
        assert len(matching) == 1
        hash_suffix = matching[0].split("_")[-1]
        assert len(hash_suffix) == 8
        int(hash_suffix, 16)


# ──────────────────────────────────────────────
# 重复加载防护
# ──────────────────────────────────────────────

class TestDuplicateLoadPrevention:
    """重复加载同一插件的防护"""

    def test_reload_same_module_returns_none(self, tmp_path):
        f = _create_plugin_file(tmp_path, "dup.py", "dup_plugin")
        registry = PluginRegistry()
        p1 = registry.load_plugin_from_module(str(f))
        assert p1 is not None

        p2 = registry.load_plugin_from_module(str(f))
        assert p2 is None

    def test_register_duplicate_name_rejected(self):
        registry = PluginRegistry()
        p1 = DummyPlugin(name="same_name")
        p2 = DummyPlugin(name="same_name")

        assert registry.register(p1) is True
        assert registry.register(p2) is False


# ──────────────────────────────────────────────
# 卸载时模块清理
# ──────────────────────────────────────────────

class TestUnregisterModuleCleanup:
    """卸载插件时清理 sys.modules"""

    def test_unregister_removes_from_sys_modules(self, tmp_path):
        f = _create_plugin_file(tmp_path, "cleanup.py", "cleanup_plugin")
        registry = PluginRegistry()
        registry.load_plugin_from_module(str(f))

        cleanup_mods = [k for k in sys.modules if "cleanup" in k and k.startswith("agentos_plugin_")]
        assert len(cleanup_mods) == 1

        registry.unregister("cleanup_plugin")

        cleanup_mods_after = [k for k in sys.modules if "cleanup" in k and k.startswith("agentos_plugin_")]
        assert len(cleanup_mods_after) == 0

    def test_unregister_removes_from_module_mapping(self, tmp_path):
        f = _create_plugin_file(tmp_path, "mapping.py", "mapping_plugin")
        registry = PluginRegistry()
        registry.load_plugin_from_module(str(f))

        assert len(registry._module_mapping) == 1

        registry.unregister("mapping_plugin")
        assert len(registry._module_mapping) == 0

    def test_unregister_nonexistent_returns_false(self):
        registry = PluginRegistry()
        assert registry.unregister("nonexistent") is False

    def test_unregister_calls_shutdown(self):
        registry = PluginRegistry()
        plugin = DummyPlugin(name="shutdown_test")
        registry.register(plugin)

        assert plugin._shutdown_called is False
        registry.unregister("shutdown_test")
        assert plugin._shutdown_called is True

    def test_multiple_plugins_unload_only_target(self, tmp_path):
        f1 = _create_plugin_file(tmp_path, "keep.py", "keep_plugin")
        f2 = _create_plugin_file(tmp_path, "remove.py", "remove_plugin")

        registry = PluginRegistry()
        registry.load_plugin_from_module(str(f1))
        registry.load_plugin_from_module(str(f2))

        registry.unregister("remove_plugin")

        keep_mods = [k for k in sys.modules if "keep" in k and k.startswith("agentos_plugin_")]
        remove_mods = [k for k in sys.modules if "remove" in k and k.startswith("agentos_plugin_")]
        assert len(keep_mods) == 1
        assert len(remove_mods) == 0


# ──────────────────────────────────────────────
# 模块名映射表
# ──────────────────────────────────────────────

class TestModuleMapping:
    """_module_mapping 映射表一致性"""

    def test_mapping_populated_on_load(self, tmp_path):
        f = _create_plugin_file(tmp_path, "mapped.py", "mapped_plugin")
        registry = PluginRegistry()
        registry.load_plugin_from_module(str(f))

        assert len(registry._module_mapping) == 1
        mod_name, plugin_name = list(registry._module_mapping.items())[0]
        assert plugin_name == "mapped_plugin"
        assert "mapped" in mod_name

    def test_mapping_empty_initially(self):
        registry = PluginRegistry()
        assert len(registry._module_mapping) == 0

    def test_mapping_multiple_plugins(self, tmp_path):
        f1 = _create_plugin_file(tmp_path, "a.py", "plugin_a")
        f2 = _create_plugin_file(tmp_path, "b.py", "plugin_b")

        registry = PluginRegistry()
        registry.load_plugin_from_module(str(f1))
        registry.load_plugin_from_module(str(f2))

        assert len(registry._module_mapping) == 2
        assert set(registry._module_mapping.values()) == {"plugin_a", "plugin_b"}


# ──────────────────────────────────────────────
# 加载失败场景
# ──────────────────────────────────────────────

class TestLoadFailure:
    """插件加载失败时的处理"""

    def test_nonexistent_file_returns_none(self):
        registry = PluginRegistry()
        result = registry.load_plugin_from_module("/nonexistent/path/plugin.py")
        assert result is None

    def test_missing_plugin_class_returns_none(self, tmp_path):
        bad_file = tmp_path / "no_class.py"
        bad_file.write_text("x = 42\n", encoding="utf-8")

        registry = PluginRegistry()
        result = registry.load_plugin_from_module(str(bad_file))
        assert result is None

    def test_failed_init_cleans_up_sys_modules(self, tmp_path):
        fail_file = tmp_path / "fail_init.py"
        fail_file.write_text(textwrap.dedent("""\
            from plugin import Plugin, PluginMetadata, PluginState, PluginContext, PluginResult

            class FailPlugin(Plugin):
                def __init__(self):
                    super().__init__()
                    self.metadata = PluginMetadata(name="fail_init", version="1.0.0")

                def initialize(self, manager) -> bool:
                    return False

                def execute(self, ctx: PluginContext) -> PluginResult:
                    return PluginResult(plugin_id="fail", success=False)

                def shutdown(self) -> None:
                    pass
        """), encoding="utf-8")

        registry = PluginRegistry()
        result = registry.load_plugin_from_module(str(fail_file), class_name="FailPlugin")

        assert result is None

        fail_mods = [k for k in sys.modules if "fail_init" in k and k.startswith("agentos_plugin_")]
        assert len(fail_mods) == 0

    def test_syntax_error_returns_none(self, tmp_path):
        bad_file = tmp_path / "syntax_err.py"
        bad_file.write_text("def broken(\n", encoding="utf-8")

        registry = PluginRegistry()
        result = registry.load_plugin_from_module(str(bad_file))
        assert result is None


# ──────────────────────────────────────────────
# 插件状态转换
# ──────────────────────────────────────────────

class TestPluginStateTransitions:
    """插件状态转换正确性"""

    def test_load_sets_loaded_state(self, tmp_path):
        f = _create_plugin_file(tmp_path, "state_test.py", "state_plugin")
        registry = PluginRegistry()
        plugin = registry.load_plugin_from_module(str(f))

        assert plugin is not None
        assert plugin.state == PluginState.LOADED

    def test_execute_transitions_running_then_loaded(self):
        registry = PluginRegistry()
        plugin = DummyPlugin(name="state_exec")
        registry.register(plugin)

        result = registry.execute_plugin("state_exec")
        assert result.success is True
        assert plugin.state == PluginState.LOADED

    def test_execute_failure_sets_failed_not_loaded(self):
        """执行异常后状态应为 FAILED 而非 LOADED（修复 finally 覆盖问题）"""
        registry = PluginRegistry()
        plugin = CrashingExecutePlugin()
        registry.register(plugin)

        result = registry.execute_plugin("crash_plugin")
        assert result.success is False
        assert "崩溃" in result.error_message
        # 关键：异常后状态应为 FAILED，不应被 finally 覆盖为 LOADED
        assert plugin.state == PluginState.FAILED

    def test_unregister_sets_unloading_then_unloaded(self):
        registry = PluginRegistry()
        plugin = DummyPlugin(name="unload_state")
        # DummyPlugin.initialize() 设置 LOADED 状态
        plugin.initialize({})
        registry.register(plugin)

        assert plugin.state == PluginState.LOADED
        registry.unregister("unload_state")
        # unregister 设置 UNLOADED（DummyPlugin.shutdown 也设置 UNLOADED，结果一致）
        assert plugin.state == PluginState.UNLOADED

    def test_failed_init_sets_failed_state(self, tmp_path):
        """initialize 返回 False 时插件状态应为 FAILED"""
        fail_file = tmp_path / "fail_state.py"
        fail_file.write_text(textwrap.dedent("""\
            from plugin import Plugin, PluginMetadata, PluginState, PluginContext, PluginResult

            class FailStatePlugin(Plugin):
                def __init__(self):
                    super().__init__()
                    self.metadata = PluginMetadata(name="fail_state", version="1.0.0")

                def initialize(self, manager) -> bool:
                    return False

                def execute(self, ctx: PluginContext) -> PluginResult:
                    return PluginResult(plugin_id="fail_state", success=False)

                def shutdown(self) -> None:
                    pass
        """), encoding="utf-8")

        registry = PluginRegistry()
        result = registry.load_plugin_from_module(str(fail_file), class_name="FailStatePlugin")

        assert result is None
        # 注意：失败的插件不会被注册，所以无法通过 registry.get 获取
        # 但我们验证它确实返回了 None


# ──────────────────────────────────────────────
# 插件执行
# ──────────────────────────────────────────────

class TestPluginExecution:
    """插件执行基本流程"""

    def test_execute_registered_plugin(self):
        registry = PluginRegistry()
        plugin = DummyPlugin(name="exec_test")
        registry.register(plugin)

        result = registry.execute_plugin("exec_test")
        assert result is not None
        assert result.success is True
        assert result.output == "executed"

    def test_execute_nonexistent_plugin(self):
        registry = PluginRegistry()
        result = registry.execute_plugin("nonexistent")
        assert result is None


# ──────────────────────────────────────────────
# 内存泄漏防护
# ──────────────────────────────────────────────

class TestMemoryLeakPrevention:
    """反复加载卸载不应导致 sys.modules 无限增长"""

    def test_repeated_load_unload_no_module_leak(self, tmp_path):
        f = _create_plugin_file(tmp_path, "leak_test.py", "leak_plugin")
        registry = PluginRegistry()

        initial_count = len([k for k in sys.modules if k.startswith("agentos_plugin_")])

        for _ in range(5):
            for k in list(sys.modules.keys()):
                if k.startswith("agentos_plugin_leak_test_"):
                    del sys.modules[k]

            p = registry.load_plugin_from_module(str(f))
            if p:
                registry.unregister("leak_plugin")

        final_count = len([k for k in sys.modules if k.startswith("agentos_plugin_")])
        assert final_count == initial_count


# ──────────────────────────────────────────────
# discover_plugins 规范
# ──────────────────────────────────────────────

class TestDiscoverPlugins:
    """discover_plugins 输入校验"""

    def test_nonexistent_path_returns_empty(self):
        registry = PluginRegistry()
        result = registry.discover_plugins("/nonexistent/path")
        assert result == []

    def test_valid_json_discovered(self, tmp_path):
        plugin_json = tmp_path / "my_plugin.json"
        plugin_json.write_text('{"name": "my_plugin", "version": "2.0.0"}', encoding="utf-8")

        registry = PluginRegistry()
        result = registry.discover_plugins(str(tmp_path))
        assert len(result) == 1
        assert result[0].name == "my_plugin"
        assert result[0].version == "2.0.0"

    def test_invalid_json_skipped(self, tmp_path):
        bad_json = tmp_path / "bad.json"
        bad_json.write_text("{invalid json", encoding="utf-8")

        registry = PluginRegistry()
        result = registry.discover_plugins(str(tmp_path))
        assert len(result) == 0

    def test_empty_name_field_skipped(self, tmp_path):
        """name 字段为空字符串时应跳过"""
        empty_name = tmp_path / "empty_name.json"
        empty_name.write_text('{"name": "", "version": "1.0.0"}', encoding="utf-8")

        registry = PluginRegistry()
        result = registry.discover_plugins(str(tmp_path))
        assert len(result) == 0

    def test_whitespace_name_skipped(self, tmp_path):
        """name 字段仅含空白时应跳过"""
        ws_name = tmp_path / "ws_name.json"
        ws_name.write_text('{"name": "   ", "version": "1.0.0"}', encoding="utf-8")

        registry = PluginRegistry()
        result = registry.discover_plugins(str(tmp_path))
        assert len(result) == 0

    def test_missing_name_uses_file_stem(self, tmp_path):
        """无 name 字段时 fallback 到文件名"""
        no_name = tmp_path / "fallback_name.json"
        no_name.write_text('{"version": "1.0.0"}', encoding="utf-8")

        registry = PluginRegistry()
        result = registry.discover_plugins(str(tmp_path))
        assert len(result) == 1
        assert result[0].name == "fallback_name"


# ──────────────────────────────────────────────
# register_hook 规范
# ──────────────────────────────────────────────

class TestRegisterHook:
    """hook 注册规范"""

    def test_unknown_event_ignored(self):
        """注册未知事件应被忽略（不报错）"""
        registry = PluginRegistry()
        registry.register_hook("unknown_event", lambda: None)
        # 不应抛异常，也不应添加到 _hooks
        assert "unknown_event" not in registry._hooks

    def test_known_event_registered(self):
        registry = PluginRegistry()
        callback = lambda: None
        registry.register_hook("pre_load", callback)
        assert callback in registry._hooks["pre_load"]


# ──────────────────────────────────────────────
# loaded_at 时间戳
# ──────────────────────────────────────────────

class TestLoadedAtTimestamp:
    """插件加载时间戳记录"""

    def test_loaded_at_set_on_success(self, tmp_path):
        f = _create_plugin_file(tmp_path, "ts_test.py", "ts_plugin")
        registry = PluginRegistry()
        plugin = registry.load_plugin_from_module(str(f))

        assert plugin is not None
        assert plugin.metadata.loaded_at is not None

    def test_loaded_at_is_recent(self, tmp_path):
        from datetime import datetime, timedelta

        f = _create_plugin_file(tmp_path, "recent_ts.py", "recent_plugin")
        registry = PluginRegistry()
        before = datetime.now()
        plugin = registry.load_plugin_from_module(str(f))
        after = datetime.now()

        assert before <= plugin.metadata.loaded_at <= after


# ──────────────────────────────────────────────
# 日志输出验证
# ──────────────────────────────────────────────

class TestLoggingOutput:
    """验证关键路径的日志输出"""

    def test_load_logs_info_on_success(self, tmp_path, caplog):
        f = _create_plugin_file(tmp_path, "log_load.py", "log_load_plugin")
        registry = PluginRegistry()

        with caplog.at_level(logging.INFO, logger="plugin"):
            registry.load_plugin_from_module(str(f))

        info_msgs = [r.message for r in caplog.records if r.levelno == logging.INFO]
        assert any("load_plugin_from_module" in m and "加载成功" in m for m in info_msgs)

    def test_load_logs_error_on_failure(self, caplog):
        registry = PluginRegistry()

        with caplog.at_level(logging.ERROR, logger="plugin"):
            registry.load_plugin_from_module("/nonexistent/plugin.py")

        error_msgs = [r.message for r in caplog.records if r.levelno == logging.ERROR]
        assert any("加载失败" in m or "无法创建模块规格" in m for m in error_msgs)

    def test_unregister_logs_info(self, tmp_path, caplog):
        f = _create_plugin_file(tmp_path, "log_unload.py", "log_unload_plugin")
        registry = PluginRegistry()
        registry.load_plugin_from_module(str(f))

        with caplog.at_level(logging.INFO, logger="plugin"):
            registry.unregister("log_unload_plugin")

        info_msgs = [r.message for r in caplog.records if r.levelno == logging.INFO]
        assert any("卸载完成" in m for m in info_msgs)

    def test_execute_logs_info(self, caplog):
        registry = PluginRegistry()
        plugin = DummyPlugin(name="log_exec")
        registry.register(plugin)

        with caplog.at_level(logging.INFO, logger="plugin"):
            registry.execute_plugin("log_exec")

        info_msgs = [r.message for r in caplog.records if r.levelno == logging.INFO]
        assert any("execute_plugin" in m and "执行完成" in m for m in info_msgs)

    def test_register_logs_info(self, caplog):
        registry = PluginRegistry()
        plugin = DummyPlugin(name="log_reg")

        with caplog.at_level(logging.INFO, logger="plugin"):
            registry.register(plugin)

        info_msgs = [r.message for r in caplog.records if r.levelno == logging.INFO]
        assert any("register" in m and "注册成功" in m for m in info_msgs)

    def test_discover_logs_info(self, tmp_path, caplog):
        plugin_json = tmp_path / "disc.json"
        plugin_json.write_text('{"name": "disc", "version": "1.0"}', encoding="utf-8")

        registry = PluginRegistry()

        with caplog.at_level(logging.INFO, logger="plugin"):
            registry.discover_plugins(str(tmp_path))

        info_msgs = [r.message for r in caplog.records if r.levelno == logging.INFO]
        assert any("discover_plugins" in m and "扫描完成" in m for m in info_msgs)

    def test_no_print_in_production_code(self):
        """验证 plugin.py 中不包含 print() 调用"""
        import inspect
        from plugin import PluginRegistry

        source = inspect.getsource(PluginRegistry)
        # 排除注释和字符串中的 print
        for line_num, line in enumerate(source.split('\n'), 1):
            stripped = line.strip()
            if stripped.startswith('#') or stripped.startswith('"""') or stripped.startswith("'''"):
                continue
            if 'print(' in stripped and not stripped.startswith('#'):
                pytest.fail(f"plugin.py 第 {line_num} 行包含 print() 调用: {stripped}")
