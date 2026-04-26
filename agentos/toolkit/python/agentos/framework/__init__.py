# AgentOS Application Framework
# Version: 2.0.0
# Last updated: 2026-04-11

"""
AgentOS 统一应用框架

提供智能体应用的完整生命周期管理、状态控制、配置管理和错误处理能力。
本框架是AgentOS应用开发的最高层抽象，为所有应用提供统一的基础设施支持。

核心模块:
1. 应用框架 (application.py) - 生命周期、状态、配置、错误处理统一入口
2. 技能框架 (skill.py) - 技能注册、版本管理、执行引擎、结果缓存
3. 任务框架 (task.py) - 工作流编排、依赖解析、并发控制、容错机制
4. 事件框架 (event.py) - 发布订阅、消息总线、事件溯源、中间件链
5. 插件框架 (plugin.py) - 动态加载、热插拔、沙箱隔离、依赖解析

使用示例:
    from agentos.framework import AgentApplication, AgentConfig

    async def main():
        app = AgentApplication(AgentConfig(name="my_agent"))
        await app.initialize()
        await app.start()

        result = await app.execute("Hello, AgentOS!")
        print(result)

        await app.stop()
        await app.destroy()

if __name__ == "__main__":
    import asyncio
    asyncio.run(main())
"""

from .lifecycle import LifecycleManager, AgentState, LifecycleEvent
from .state import StateManager, StateSnapshot
from .config import ConfigurationCenter, ConfigSource, ValidationResult
from .errors import ErrorHandlingFramework, ErrorLevel, ErrorContext, RecoveryStrategy
from .application import AgentApplication, AgentConfig, ExecutionContext, AgentContext, ExecutionResult, ExecutionFrame, ErrorCode
from .skill import (
    BaseSkill, SkillRegistry, VersionManager,
    SkillExecutionEngine, SkillResultCache,
    SkillMetadata, SkillResult, SkillExecutionResult, SkillStatus
)
from .task import (
    TaskOrchestrationEngine, DependencyResolver,
    ConcurrencyController, FaultToleranceMechanism,
    WorkflowDefinition, WorkflowResult, TaskNode,
    TaskExecutionResult, TaskStatus
)
from .event import (
    EventBus, EventRouter, EventStore,
    EventMiddleware, LoggingMiddleware, MetricsMiddleware, DeduplicationMiddleware,
    EventEnvelope, Subscription, HandlerResult,
    EventPriority, DispatchMode, SystemEvents
)
from .plugin import (
    PluginManager, PluginSandbox, PluginDependencyResolver,
    HotReloadMechanism, BasePlugin, PluginRegistry,
    PluginState, PluginManifest, PluginInfo,
    ResolutionResult, ReloadResult, get_plugin_registry,
)

__version__ = "2.0.0"
__author__ = "SPHARX Ltd."

__all__ = [
    # ===== 应用框架 =====
    # 核心类
    "AgentApplication",
    "AgentConfig",
    "ExecutionContext",
    "AgentContext",
    "ExecutionResult",
    "ExecutionFrame",
    "ErrorCode",

    # 生命周期管理
    "LifecycleManager",
    "AgentState",
    "LifecycleEvent",

    # 状态管理
    "StateManager",
    "StateSnapshot",

    # 配置管理
    "ConfigurationCenter",
    "ConfigSource",
    "ValidationResult",

    # 错误处理
    "ErrorHandlingFramework",
    "ErrorLevel",
    "ErrorContext",
    "RecoveryStrategy",

    # ===== 技能框架 =====
    # 基础类
    "BaseSkill",

    # 注册和发现
    "SkillRegistry",
    "VersionManager",

    # 执行和缓存
    "SkillExecutionEngine",
    "SkillResultCache",

    # 数据类型
    "SkillMetadata",
    "SkillResult",
    "SkillExecutionResult",
    "SkillStatus",

    # ===== 任务框架 =====
    # 编排引擎
    "TaskOrchestrationEngine",

    # 支撑组件
    "DependencyResolver",
    "ConcurrencyController",
    "FaultToleranceMechanism",

    # 数据类型
    "WorkflowDefinition",
    "WorkflowResult",
    "TaskNode",
    "TaskExecutionResult",
    "TaskStatus",

    # ===== 事件框架 =====
    "EventBus",
    "EventRouter",
    "EventStore",
    "EventMiddleware",
    "LoggingMiddleware",
    "MetricsMiddleware",
    "DeduplicationMiddleware",
    "EventEnvelope",
    "Subscription",
    "HandlerResult",
    "EventPriority",
    "DispatchMode",
    "SystemEvents",

    # ===== 插件框架 =====
    "PluginManager",
    "PluginSandbox",
    "PluginDependencyResolver",
    "HotReloadMechanism",
    "BasePlugin",
    "PluginRegistry",
    "PluginState",
    "PluginManifest",
    "PluginInfo",
    "ResolutionResult",
    "ReloadResult",
    "get_plugin_registry",
]
