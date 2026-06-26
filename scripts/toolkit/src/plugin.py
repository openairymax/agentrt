#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentRT 插件系统
# 支持动态加载和扩展脚本功能

"""
AgentRT 插件系统

提供灵活的插件架构，支持：
- 动态插件发现和加载
- 插件元数据管理
- 插件依赖解析
- 执行上下文隔离

Architecture:
    PluginRegistry : 插件注册表，管理所有已加载的插件
    Plugin        : 插件基类，定义插件接口
    PluginContext  : 插件执行上下文
"""

import importlib
import importlib.util
import json
import logging
import os
import sys
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from datetime import datetime
from enum import Enum
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Type

logger = logging.getLogger(__name__)


class PluginState(Enum):
    UNLOADED = "unloaded"
    LOADING = "loading"
    LOADED = "loaded"
    RUNNING = "running"
    STOPPED = "stopped"
    FAILED = "failed"
    UNLOADING = "unloading"


@dataclass
class PluginMetadata:
    name: str
    version: str
    author: str = ""
    description: str = ""
    dependencies: List[str] = field(default_factory=list)
    entry_point: str = ""
    tags: List[str] = field(default_factory=list)
    min_agentos_version: str = "0.1.0"
    loaded_at: Optional[datetime] = None


@dataclass
class PluginContext:
    plugin_id: str
    working_dir: str
    environment: Dict[str, str] = field(default_factory=dict)
    manager: Dict[str, Any] = field(default_factory=dict)
    trace_id: str = ""
    parent_trace_id: Optional[str] = None

    def __post_init__(self):
        if not self.trace_id:
            self.trace_id = f"{self.plugin_id}-{datetime.now().strftime('%Y%m%d%H%M%S%f')}"


@dataclass
class PluginResult:
    plugin_id: str
    success: bool
    output: Any = None
    error_message: str = ""
    execution_time_ms: float = 0
    metrics: Dict[str, Any] = field(default_factory=dict)


class Plugin(ABC):
    metadata: PluginMetadata

    def __init__(self):
        self._state = PluginState.UNLOADED
        self._context: Optional[PluginContext] = None

    @abstractmethod
    def initialize(self, manager: Dict[str, Any]) -> bool:
        pass

    @abstractmethod
    def execute(self, ctx: PluginContext) -> PluginResult:
        pass

    @abstractmethod
    def shutdown(self) -> None:
        pass

    def health_check(self) -> bool:
        return self._state == PluginState.LOADED

    @property
    def state(self) -> PluginState:
        return self._state

    @property
    def name(self) -> str:
        return self.metadata.name

    @property
    def version(self) -> str:
        return self.metadata.version


class PluginRegistry:
    def __init__(self):
        self._plugins: Dict[str, Plugin] = {}
        self._metadata: Dict[str, PluginMetadata] = {}
        self._module_mapping: Dict[str, str] = {}  # 模块名 -> 插件名
        self._hooks: Dict[str, List[Callable]] = {
            "pre_load": [],
            "post_load": [],
            "pre_execute": [],
            "post_execute": [],
            "pre_unload": [],
            "post_unload": [],
        }

    def register_hook(self, event: str, callback: Callable) -> None:
        if event in self._hooks:
            self._hooks[event].append(callback)
            logger.debug("register_hook: 事件=%s, 已注册回调=%s", event, callback)
        else:
            logger.warning("register_hook: 未知事件=%s, 忽略注册", event)

    def _trigger_hooks(self, event: str, *args, **kwargs) -> None:
        for callback in self._hooks.get(event, []):
            try:
                callback(*args, **kwargs)
            except Exception as e:
                logger.error("Hook %s 回调执行失败: %s", event, e, exc_info=True)

    def register(self, plugin: Plugin) -> bool:
        name = plugin.metadata.name

        if name in self._plugins:
            logger.warning("register: 插件 '%s' 已注册，拒绝重复注册", name)
            return False

        self._plugins[name] = plugin
        self._metadata[name] = plugin.metadata
        self._trigger_hooks("post_load", plugin)
        logger.info(
            "register: 插件 '%s' v%s 注册成功, 当前已注册 %d 个插件",
            name, plugin.metadata.version, len(self._plugins),
        )
        return True

    def unregister(self, name: str) -> bool:
        if name not in self._plugins:
            logger.warning("unregister: 插件 '%s' 未注册，无法卸载", name)
            return False

        plugin = self._plugins[name]
        plugin._state = PluginState.UNLOADING
        self._trigger_hooks("pre_unload", plugin)

        try:
            plugin.shutdown()

            # 清理插件模块缓存，避免内存泄漏
            module_names_to_remove = [
                mod_name for mod_name, plugin_name in self._module_mapping.items()
                if plugin_name == name
            ]
            for mod_name in module_names_to_remove:
                sys.modules.pop(mod_name, None)
                del self._module_mapping[mod_name]
                logger.debug("unregister: 已清理 sys.modules 条目 '%s'", mod_name)

            del self._plugins[name]
            del self._metadata[name]
            plugin._state = PluginState.UNLOADED
            self._trigger_hooks("post_unload", plugin)
            logger.info(
                "unregister: 插件 '%s' 卸载完成, 清理了 %d 个模块缓存, 剩余 %d 个插件",
                name, len(module_names_to_remove), len(self._plugins),
            )
            return True
        except Exception as e:
            plugin._state = PluginState.FAILED
            logger.error("unregister: 插件 '%s' 卸载失败: %s", name, e, exc_info=True)
            return False

    def get(self, name: str) -> Optional[Plugin]:
        return self._plugins.get(name)

    def list_plugins(self) -> List[PluginMetadata]:
        return list(self._metadata.values())

    def discover_plugins(self, path: str) -> List[PluginMetadata]:
        discovered = []
        plugin_dir = Path(path)

        if not plugin_dir.exists():
            logger.warning("discover_plugins: 路径不存在 '%s'", path)
            return discovered

        logger.info("discover_plugins: 扫描目录 '%s'", path)

        for file in plugin_dir.glob("*.json"):
            try:
                with open(file, encoding="utf-8") as f:
                    data = json.load(f)

                # 验证必填字段
                plugin_name = data.get("name", file.stem)
                if not plugin_name or not plugin_name.strip():
                    logger.warning("discover_plugins: 文件 '%s' 缺少有效的 name 字段，跳过", file)
                    continue

                metadata = PluginMetadata(
                    name=plugin_name,
                    version=data.get("version", "1.0.0"),
                    author=data.get("author", ""),
                    description=data.get("description", ""),
                    dependencies=data.get("dependencies", []),
                    entry_point=data.get("entry_point", ""),
                    tags=data.get("tags", []),
                )
                discovered.append(metadata)
                logger.debug(
                    "discover_plugins: 发现插件 '%s' v%s (来源: %s)",
                    metadata.name, metadata.version, file.name,
                )
            except json.JSONDecodeError as e:
                logger.error("discover_plugins: 文件 '%s' JSON 解析失败: %s", file, e)
            except Exception as e:
                logger.error("discover_plugins: 文件 '%s' 读取失败: %s", file, e, exc_info=True)

        logger.info("discover_plugins: 扫描完成, 发现 %d 个插件", len(discovered))
        return discovered

    def load_plugin_from_module(self, module_path: str, class_name: str = "Plugin") -> Optional[Plugin]:
        resolved_path = str(Path(module_path).resolve())
        logger.info("load_plugin_from_module: 开始加载 module_path=%s, class_name=%s", module_path, class_name)

        try:
            # 基于文件路径哈希生成唯一模块名，确保每个插件独立命名空间
            # 微内核架构要求插件之间完全隔离，避免 sys.modules 命名冲突
            path_hash = hash(resolved_path) & 0xffffffff
            module_name = f"agentos_plugin_{Path(module_path).stem}_{path_hash:08x}"
            logger.debug("load_plugin_from_module: 生成模块名='%s' (path_hash=0x%08x)", module_name, path_hash)

            # 防止重复加载同一模块
            if module_name in sys.modules:
                logger.warning(
                    "load_plugin_from_module: 模块 '%s' 已在 sys.modules 中, 拒绝重复加载 (path=%s)",
                    module_name, module_path,
                )
                return None

            spec = importlib.util.spec_from_file_location(module_name, module_path)
            if not spec or not spec.loader:
                logger.error(
                    "load_plugin_from_module: 无法创建模块规格, module_path=%s (文件不存在或无法加载)",
                    module_path,
                )
                return None

            module = importlib.util.module_from_spec(spec)
            sys.modules[module_name] = module
            spec.loader.exec_module(module)
            logger.debug("load_plugin_from_module: 模块 '%s' 执行完成", module_name)

            plugin_class: Type[Plugin] = getattr(module, class_name, None)
            if not plugin_class:
                logger.error(
                    "load_plugin_from_module: 模块 '%s' 中未找到类 '%s', 清理模块缓存",
                    module_name, class_name,
                )
                sys.modules.pop(module_name, None)
                return None

            plugin = plugin_class()
            plugin._state = PluginState.LOADING
            logger.debug(
                "load_plugin_from_module: 实例化插件类 '%s' 成功, 开始初始化",
                class_name,
            )

            if plugin.initialize({}):
                plugin._state = PluginState.LOADED
                plugin.metadata.loaded_at = datetime.now()
                self.register(plugin)
                # 记录模块名 -> 插件名映射，便于卸载时清理和调试
                self._module_mapping[module_name] = plugin.metadata.name
                logger.info(
                    "load_plugin_from_module: 插件 '%s' v%s 加载成功, module=%s, loaded_at=%s",
                    plugin.metadata.name, plugin.metadata.version,
                    module_name, plugin.metadata.loaded_at.isoformat(),
                )
                return plugin
            else:
                plugin._state = PluginState.FAILED
                logger.error(
                    "load_plugin_from_module: 插件 '%s' initialize() 返回 False, 清理模块缓存",
                    getattr(plugin, 'metadata', None) and plugin.metadata.name or class_name,
                )
                sys.modules.pop(module_name, None)

        except Exception as e:
            logger.error(
                "load_plugin_from_module: 加载失败 module_path=%s: %s",
                module_path, e, exc_info=True,
            )

        return None

    def execute_plugin(self, name: str, manager: Dict[str, Any] = None) -> Optional[PluginResult]:
        plugin = self.get(name)
        if not plugin:
            logger.warning("execute_plugin: 插件 '%s' 未找到", name)
            return None

        ctx = PluginContext(
            plugin_id=name,
            working_dir=os.getcwd(),
            manager=manager or {},
        )
        logger.info(
            "execute_plugin: 开始执行插件 '%s', trace_id=%s",
            name, ctx.trace_id,
        )

        self._trigger_hooks("pre_execute", plugin, ctx)

        try:
            plugin._state = PluginState.RUNNING
            result = plugin.execute(ctx)
            self._trigger_hooks("post_execute", plugin, result)
            logger.info(
                "execute_plugin: 插件 '%s' 执行完成, success=%s, execution_time=%.1fms",
                name, result.success, result.execution_time_ms,
            )
            return result
        except Exception as e:
            plugin._state = PluginState.FAILED
            logger.error(
                "execute_plugin: 插件 '%s' 执行异常: %s", name, e, exc_info=True,
            )
            return PluginResult(
                plugin_id=name,
                success=False,
                error_message=str(e),
            )
        finally:
            # 仅在非 FAILED 状态下恢复为 LOADED
            if plugin._state != PluginState.FAILED:
                plugin._state = PluginState.LOADED


_global_registry: Optional[PluginRegistry] = None


def get_registry() -> PluginRegistry:
    global _global_registry
    if _global_registry is None:
        _global_registry = PluginRegistry()
        logger.debug("get_registry: 创建全局 PluginRegistry 实例")
    return _global_registry
