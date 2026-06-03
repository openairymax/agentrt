#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS 插件系统
# 支持动态加载和扩展脚本功能

"""
AgentOS 插件系统

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
import os
import sys
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from datetime import datetime
from enum import Enum
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Type


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

    def _trigger_hooks(self, event: str, *args, **kwargs) -> None:
        for callback in self._hooks.get(event, []):
            try:
                callback(*args, **kwargs)
            except Exception as e:
                print(f"Hook {event} failed: {e}")

    def register(self, plugin: Plugin) -> bool:
        name = plugin.metadata.name

        if name in self._plugins:
            print(f"Plugin {name} already registered")
            return False

        self._plugins[name] = plugin
        self._metadata[name] = plugin.metadata
        self._trigger_hooks("post_load", plugin)
        return True

    def unregister(self, name: str) -> bool:
        if name not in self._plugins:
            return False

        plugin = self._plugins[name]
        self._trigger_hooks("pre_unload", plugin)

        try:
            plugin.shutdown()
            del self._plugins[name]
            del self._metadata[name]
            self._trigger_hooks("post_unload", plugin)
            return True
        except Exception as e:
            print(f"Failed to unload plugin {name}: {e}")
            return False

    def get(self, name: str) -> Optional[Plugin]:
        return self._plugins.get(name)

    def list_plugins(self) -> List[PluginMetadata]:
        return list(self._metadata.values())

    def discover_plugins(self, path: str) -> List[PluginMetadata]:
        discovered = []
        plugin_dir = Path(path)

        if not plugin_dir.exists():
            return discovered

        for file in plugin_dir.glob("*.json"):
            try:
                with open(file) as f:
                    data = json.load(f)
                    metadata = PluginMetadata(
                        name=data.get("name", file.stem),
                        version=data.get("version", "1.0.0"),
                        author=data.get("author", ""),
                        description=data.get("description", ""),
                        dependencies=data.get("dependencies", []),
                        entry_point=data.get("entry_point", ""),
                        tags=data.get("tags", [])
                    )
                    discovered.append(metadata)
            except Exception as e:
                print(f"Failed to load plugin metadata from {file}: {e}")

        return discovered

    def load_plugin_from_module(self, module_path: str, class_name: str = "Plugin") -> Optional[Plugin]:
        try:
            # 使用模块路径生成唯一模块名，避免多个插件共享同一 sys.modules 条目
            # 微内核架构要求每个插件运行在独立的命名空间中
            module_name = f"agentos_plugin_{Path(module_path).stem}_{id(self)}_{len(self._plugins)}"
            spec = importlib.util.spec_from_file_location(module_name, module_path)
            if not spec or not spec.loader:
                return None

            module = importlib.util.module_from_spec(spec)
            sys.modules[module_name] = module
            spec.loader.exec_module(module)

            plugin_class: Type[Plugin] = getattr(module, class_name, None)
            if not plugin_class:
                return None

            plugin = plugin_class()
            if plugin.initialize({}):
                self.register(plugin)
                return plugin

        except Exception as e:
            print(f"Failed to load plugin from {module_path}: {e}")

        return None

    def execute_plugin(self, name: str, manager: Dict[str, Any] = None) -> Optional[PluginResult]:
        plugin = self.get(name)
        if not plugin:
            print(f"Plugin {name} not found")
            return None

        ctx = PluginContext(
            plugin_id=name,
            working_dir=os.getcwd(),
            manager=manager or {}
        )

        self._trigger_hooks("pre_execute", plugin, ctx)

        try:
            plugin._state = PluginState.RUNNING
            result = plugin.execute(ctx)
            self._trigger_hooks("post_execute", plugin, result)
            return result
        except Exception as e:
            plugin._state = PluginState.FAILED
            return PluginResult(
                plugin_id=name,
                success=False,
                error_message=str(e)
            )
        finally:
            plugin._state = PluginState.LOADED


_global_registry: Optional[PluginRegistry] = None


def get_registry() -> PluginRegistry:
    global _global_registry
    if _global_registry is None:
        _global_registry = PluginRegistry()
    return _global_registry
