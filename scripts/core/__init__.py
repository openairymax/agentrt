#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS Scripts 核心模块
# 提供插件系统、事件总线、遥测等核心功能

"""
AgentOS Scripts Core Module

提供构建、部署、测试等脚本的核心框架，包括：
- 插件系统：支持动态加载和扩展
- 事件总线：统一的遥测和事件处理
- 安全模块：输入净化和权限管理
- 配置引擎：模板化和可配置性

Usage:
    from agentos_scripts.core import PluginRegistry, EventBus
"""

__version__ = "1.0.0.6"
__author__ = "SPHARX Ltd."

from .plugin import PluginRegistry, Plugin, PluginMetadata
from .events import EventBus, Event, EventHandler
from .security import SecurityManager, InputValidator
from .telemetry import TelemetryCollector, Metrics

__all__ = [
    "PluginRegistry",
    "Plugin",
    "PluginMetadata",
    "EventBus",
    "Event",
    "EventHandler",
    "SecurityManager",
    "InputValidator",
    "TelemetryCollector",
    "Metrics",
]