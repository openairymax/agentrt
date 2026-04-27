# AgentOS Application Framework
# Version: 2.0.0
# Last updated: 2026-04-11

"""
智能体应用统一入口

整合生命周期管理、状态管理、配置管理和错误处理，
为所有AgentOS应用提供一致的开发体验。
同时集成技能框架、任务框架、事件框架和插件框架，
实现设计文档中的完整 IAgentApplication 接口。

使用示例:
    from agentos.framework import AgentApplication, AgentConfig

    async def main():
        app = AgentApplication(AgentConfig(
            name="my_agent",
            version="1.0.0",
            description="My first AgentOS application"
        ))

        await app.initialize()
        await app.start()

        result = await app.execute("Hello, World!")
        print(result)

        await app.stop()
        await app.destroy()

if __name__ == "__main__":
    import asyncio
    asyncio.run(main())
"""

import asyncio
import enum
import importlib
import importlib.util
import inspect
import logging
import sys
import time
import uuid
from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Type, TypeVar

from .lifecycle import LifecycleManager, AgentState, LifecycleEvent
from .state import StateManager, StateSnapshot
from .config import ConfigurationCenter, ConfigSource, ValidationResult
from .errors import ErrorHandlingFramework, ErrorLevel, ErrorContext, RecoveryStrategy
from .event import EventBus, EventRouter, EventStore, EventMiddleware, SystemEvents, EventPriority, DispatchMode
from .skill import BaseSkill, SkillRegistry, SkillExecutionEngine, SkillResultCache, SkillResult
from .task import TaskOrchestrationEngine, DependencyResolver, ConcurrencyController, FaultToleranceMechanism
from .plugin import PluginManager, PluginSandbox, BasePlugin

logger = logging.getLogger(__name__)

T = TypeVar('T')


class ErrorCode(enum.IntEnum):
    SUCCESS = 0
    INVALID_ARGUMENT = 1001
    NOT_INITIALIZED = 1002
    ALREADY_RUNNING = 1003
    TIMEOUT = 2001
    RESOURCE_EXHAUSTED = 2002
    PERMISSION_DENIED = 3001
    PLUGIN_NOT_FOUND = 4001
    DEPENDENCY_MISSING = 4002
    INTERNAL_ERROR = 5001


@dataclass
class AgentConfig:
    """智能体应用配置"""
    name: str = "agentos_app"
    version: str = "1.0.0"
    description: str = ""
    author: str = ""

    auto_start: bool = False
    graceful_shutdown_timeout: float = 30.0

    log_level: str = "INFO"
    log_file: Optional[str] = None

    enable_state_persistence: bool = False
    state_persistence_path: Optional[str] = None

    config_files: List[str] = field(default_factory=list)

    plugin_directories: List[str] = field(default_factory=list)
    sandbox_enabled: bool = True

    event_bus_type: str = "in_memory"
    event_enable_persistence: bool = False
    event_store_max_events: int = 10000

    skill_discovery_paths: List[str] = field(default_factory=list)
    skill_default_cache_ttl: float = 300.0

    task_max_concurrent: int = 10
    task_default_retry_count: int = 3
    task_default_timeout: float = 60.0

    custom_config: Dict[str, Any] = field(default_factory=dict)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "name": self.name,
            "version": self.version,
            "description": self.description,
            "author": self.author,
            "auto_start": self.auto_start,
            "graceful_shutdown_timeout": self.graceful_shutdown_timeout,
            "log_level": self.log_level,
            "enable_state_persistence": self.enable_state_persistence,
            "plugin_directories": self.plugin_directories,
            "task_max_concurrent": self.task_max_concurrent,
            "task_default_timeout": self.task_default_timeout,
            **self.custom_config
        }


@dataclass
class ExecutionFrame:
    """执行栈帧"""
    frame_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    component: str = ""
    action: str = ""
    started_at: datetime = field(default_factory=datetime.now)
    finished_at: Optional[datetime] = None
    metadata: Dict[str, Any] = field(default_factory=dict)


@dataclass
class AgentContext:
    """统一的执行上下文，在所有框架间传递"""
    request_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    session_id: str = ""
    timestamp: datetime = field(default_factory=datetime.now)
    trace_id: str = field(default_factory=lambda: str(uuid.uuid4()))

    user_id: Optional[str] = None
    user_permissions: List[str] = field(default_factory=list)

    input_data: Any = None
    metadata: Dict[str, Any] = field(default_factory=dict)

    scratchpad: Dict[str, Any] = field(default_factory=dict)

    execution_stack: List[ExecutionFrame] = field(default_factory=list)

    def push_frame(self, component: str, action: str, **meta) -> ExecutionFrame:
        frame = ExecutionFrame(component=component, action=action, metadata=meta)
        self.execution_stack.append(frame)
        return frame

    def pop_frame(self) -> Optional[ExecutionFrame]:
        if self.execution_stack:
            frame = self.execution_stack.pop()
            frame.finished_at = datetime.now()
            return frame
        return None


@dataclass
class ExecutionContext:
    """执行上下文（向后兼容）"""
    context_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    timestamp: datetime = field(default_factory=datetime.now)
    request_id: Optional[str] = None
    session_id: Optional[str] = None
    trace_id: Optional[str] = None

    user_id: Optional[str] = None
    user_permissions: List[str] = field(default_factory=list)

    input_data: Any = None
    metadata: Dict[str, Any] = field(default_factory=dict)
    scratchpad: Dict[str, Any] = field(default_factory=dict)

    def to_agent_context(self) -> AgentContext:
        return AgentContext(
            request_id=self.request_id or self.context_id,
            session_id=self.session_id or "",
            trace_id=self.trace_id or str(uuid.uuid4()),
            user_id=self.user_id,
            user_permissions=self.user_permissions,
            input_data=self.input_data,
            metadata=self.metadata,
            scratchpad=self.scratchpad,
        )


@dataclass
class ExecutionResult:
    """执行结果"""
    success: bool
    output: Any = None
    error: Optional[ErrorContext] = None
    error_code: ErrorCode = ErrorCode.SUCCESS
    execution_time_ms: float = 0.0
    metadata: Dict[str, Any] = field(default_factory=dict)


class AgentApplication:
    """
    智能体应用统一入口

    整合所有框架组件，提供：
    - 完整的生命周期管理（initialize/start/pause/resume/stop/destroy）
    - 集中式状态管理
    - 统一的配置管理
    - 结构化错误处理
    - 事件发布能力
    - 技能注册与执行
    - 任务编排引擎
    - 插件动态扩展
    - 可观测性支持

    设计原则:
    1. 一切皆服务 - 所有功能通过统一接口访问
    2. 声明式配置 - 通过配置驱动行为
    3. 渐进式复杂度 - 简单场景简单使用，复杂场景灵活扩展
    4. 开箱即用 - 合理的默认值，最小化配置需求
    """

    def __init__(self, config: Optional[AgentConfig] = None):
        self._config: AgentConfig = config or AgentConfig()

        self._lifecycle: LifecycleManager = LifecycleManager()
        self._state_manager: StateManager = StateManager(
            max_history_size=1000,
            enable_persistence=self._config.enable_state_persistence,
            persistence_path=self._config.state_persistence_path
        )
        self._config_center: ConfigurationCenter = ConfigurationCenter()
        self._error_framework: ErrorHandlingFramework = ErrorHandlingFramework(
            default_recovery_strategy=DEFAULT_RETRY_STRATEGY
        )

        self._event_bus: EventBus = EventBus(
            dispatch_mode=DispatchMode.PARALLEL,
            enable_store=self._config.event_enable_persistence,
            store_max_events=self._config.event_store_max_events
        )
        self._skill_registry: SkillRegistry = SkillRegistry()
        self._skill_engine: SkillExecutionEngine = SkillExecutionEngine(
            registry=self._skill_registry,
            default_timeout=self._config.task_default_timeout,
            max_concurrent=self._config.task_max_concurrent
        )
        self._skill_cache: SkillResultCache = SkillResultCache(
            default_ttl=self._config.skill_default_cache_ttl
        )
        self._task_engine: TaskOrchestrationEngine = TaskOrchestrationEngine(
            skill_engine=self._skill_engine
        )
        self._plugin_manager: PluginManager = PluginManager(
            sandbox_enabled=self._config.sandbox_enabled,
            plugin_directories=self._config.plugin_directories
        )

        self._app_id: str = str(uuid.uuid4())
        self._created_at: datetime = datetime.now()

        self._event_listeners: Dict[str, List[Callable]] = {}

        self._register_internal_hooks()
        self._wire_framework_events()

        logger.info(f"AgentApplication created: {self._config.name} (id={self._app_id})")

    @property
    def config(self) -> AgentConfig:
        return self._config

    @property
    def state(self) -> AgentState:
        return self._lifecycle.state

    @property
    def is_running(self) -> bool:
        return self._lifecycle.is_running

    @property
    def is_initialized(self) -> bool:
        return self._lifecycle.is_initialized

    @property
    def lifecycle(self) -> LifecycleManager:
        return self._lifecycle

    @property
    def state_manager(self) -> StateManager:
        return self._state_manager

    @property
    def config_center(self) -> ConfigurationCenter:
        return self._config_center

    @property
    def error_handler(self) -> ErrorHandlingFramework:
        return self._error_framework

    @property
    def event_bus(self) -> EventBus:
        return self._event_bus

    @property
    def skill_registry(self) -> SkillRegistry:
        return self._skill_registry

    @property
    def skill_engine(self) -> SkillExecutionEngine:
        return self._skill_engine

    @property
    def skill_cache(self) -> SkillResultCache:
        return self._skill_cache

    @property
    def task_engine(self) -> TaskOrchestrationEngine:
        return self._task_engine

    @property
    def plugin_manager(self) -> PluginManager:
        return self._plugin_manager

    async def initialize(self) -> None:
        """
        初始化应用

        加载配置、初始化组件、准备运行环境。
        必须在start()之前调用。
        """
        logger.info(f"Initializing {self._config.name} v{self._config.version}")

        try:
            self._setup_logging()
            await self._load_configurations()
            await self._setup_default_config()

            await self._state_manager.set("app.id", self._app_id)
            await self._state_manager.set("app.name", self._config.name)
            await self._state_manager.set("app.version", self._config.version)
            await self._state_manager.set("app.created_at", self._created_at.isoformat())

            await self._lifecycle.initialize({
                "config": self._config.to_dict(),
                "app_id": self._app_id
            })

            await self._event_bus.publish(
                SystemEvents.AGENT_INITIALIZED,
                data={"name": self._config.name, "version": self._config.version},
                source=self._config.name
            )

            await self._discover_skills()

            await self._load_plugins()

            logger.info(f"{self._config.name} initialized successfully")

        except Exception as e:
            logger.error(f"Initialization failed: {e}")
            raise

    async def start(self) -> None:
        """
        启动应用

        开始接受请求和处理任务。
        必须在initialize()之后调用。
        """
        if not self.is_initialized:
            raise RuntimeError("Application must be initialized before starting")

        logger.info(f"Starting {self._config.name}")

        await self._lifecycle.start()

        await self.emit("application.started", {
            "name": self._config.name,
            "version": self._config.version,
            "started_at": datetime.now().isoformat()
        })

        await self._event_bus.publish(
            SystemEvents.AGENT_STARTED,
            data={"name": self._config.name},
            source=self._config.name
        )

        logger.info(f"{self._config.name} started successfully")

    async def pause(self) -> None:
        """暂停应用"""
        if not self.is_running:
            logger.warning(f"Cannot pause: not running (state={self._lifecycle.state.value})")
            return

        logger.info(f"Pausing {self._config.name}")
        await self._lifecycle.pause()

        await self._event_bus.publish(
            SystemEvents.AGENT_PAUSED,
            data={"name": self._config.name},
            source=self._config.name
        )

        logger.info(f"{self._config.name} paused")

    async def resume(self) -> None:
        """恢复应用"""
        if self._lifecycle.state != AgentState.PAUSED:
            logger.warning(f"Cannot resume: not paused (state={self._lifecycle.state.value})")
            return

        logger.info(f"Resuming {self._config.name}")
        await self._lifecycle.resume()

        await self._event_bus.publish(
            SystemEvents.AGENT_RESUMED,
            data={"name": self._config.name},
            source=self._config.name
        )

        logger.info(f"{self._config.name} resumed")

    async def stop(self) -> None:
        """停止应用"""
        if not self._lifecycle.can_stop:
            logger.warning(f"Cannot stop in current state: {self._lifecycle.state.value}")
            return

        logger.info(f"Stopping {self._config.name}")

        await self.emit("application.stopping", {"reason": "manual"})

        await self._lifecycle.stop()

        await self._event_bus.publish(
            SystemEvents.AGENT_STOPPED,
            data={"name": self._config.name},
            source=self._config.name
        )

        logger.info(f"{self._config.name} stopped successfully")

    async def destroy(self) -> None:
        """销毁应用"""
        logger.info(f"Destroying {self._config.name}")

        try:
            if self._lifecycle.can_stop:
                await asyncio.wait_for(
                    self.stop(),
                    timeout=self._config.graceful_shutdown_timeout
                )
        except asyncio.TimeoutError:
            logger.warning("Graceful shutdown timed out, forcing destroy")

        final_snapshot = await self._state_manager.snapshot(metadata={"reason": "destroy"})

        await self._unload_plugins()

        await self._lifecycle.destroy()

        await self._event_bus.publish(
            SystemEvents.AGENT_DESTROYED,
            data={"name": self._config.name, "final_snapshot": final_snapshot.snapshot_id},
            source=self._config.name
        )

        logger.info(
            f"{self._config.name} destroyed. "
            f"Final state snapshot: {final_snapshot.snapshot_id}"
        )

    async def execute(
        self,
        input_data: Any,
        context: Optional[ExecutionContext] = None
    ) -> ExecutionResult:
        """
        执行主要任务

        完整集成五大框架的执行流程：
        1. Agent Framework: 接收输入、状态检查、事件发布
        2. Task Framework: 解析任务意图、编排执行计划
        3. Skill Framework: 查找并执行所需技能
        4. Event Framework: 收集和处理事件
        5. Plugin Framework: 可选的扩展功能增强

        Args:
            input_data: 输入数据（字符串、字典或任意对象）
            context: 执行上下文（可选）

        Returns:
            执行结果
        """
        start_time = time.perf_counter()

        if not self.is_running:
            return ExecutionResult(
                success=False,
                error=ErrorContext(
                    level=ErrorLevel.FATAL,
                    message=f"Application is not running (current state: {self._lifecycle.state.value})"
                ),
                error_code=ErrorCode.NOT_INITIALIZED
            )

        ctx = context or ExecutionContext(input_data=input_data)
        agent_ctx = ctx.to_agent_context()
        agent_ctx.input_data = input_data

        try:
            agent_ctx.push_frame("agent", "execute", input_type=type(input_data).__name__)

            await self._event_bus.publish(
                SystemEvents.EXECUTION_BEFORE,
                data={
                    "context_id": agent_ctx.request_id,
                    "input_type": type(input_data).__name__,
                    "trace_id": agent_ctx.trace_id
                },
                source=self._config.name,
                trace_id=agent_ctx.trace_id
            )

            result_output = await self._execute_internal(input_data, agent_ctx)

            execution_time = (time.perf_counter() - start_time) * 1000

            agent_ctx.pop_frame()

            await self._event_bus.publish(
                SystemEvents.EXECUTION_AFTER,
                data={
                    "context_id": agent_ctx.request_id,
                    "success": True,
                    "execution_time_ms": execution_time,
                    "trace_id": agent_ctx.trace_id
                },
                source=self._config.name,
                trace_id=agent_ctx.trace_id
            )

            return ExecutionResult(
                success=True,
                output=result_output,
                execution_time_ms=execution_time
            )

        except Exception as e:
            error_ctx = await self._error_framework.handle_error(e, {
                "input_data": str(input_data)[:200],
                "trace_id": agent_ctx.trace_id
            })
            execution_time = (time.perf_counter() - start_time) * 1000

            agent_ctx.pop_frame()

            await self._event_bus.publish(
                SystemEvents.EXECUTION_ERROR,
                data={
                    "context_id": agent_ctx.request_id,
                    "error": str(e),
                    "trace_id": agent_ctx.trace_id
                },
                source=self._config.name,
                trace_id=agent_ctx.trace_id
            )

            return ExecutionResult(
                success=False,
                error=error_ctx,
                error_code=ErrorCode.INTERNAL_ERROR,
                execution_time_ms=execution_time
            )

    async def _execute_internal(self, input_data: Any, ctx: AgentContext) -> Any:
        """
        内部执行逻辑

        根据输入数据的类型和内容，智能选择执行路径：
        - 如果是技能调用请求，通过Skill Framework执行
        - 如果是工作流定义，通过Task Framework编排
        - 如果是简单输入，尝试匹配已注册技能或返回默认处理
        """
        if isinstance(input_data, dict):
            skill_name = input_data.get("skill")
            task_workflow = input_data.get("workflow")

            if skill_name:
                return await self._execute_via_skill(skill_name, input_data.get("parameters", {}), ctx)
            elif task_workflow:
                return await self._execute_via_task(task_workflow, input_data.get("variables", {}), ctx)

        skill_result = await self._try_auto_skill(input_data, ctx)
        if skill_result is not None:
            return skill_result

        return f"Processed by {self._config.name}: {input_data}"

    async def _execute_via_skill(
        self,
        skill_name: str,
        parameters: Dict[str, Any],
        ctx: AgentContext
    ) -> Any:
        """通过技能框架执行"""
        ctx.push_frame("skill", "execute", skill_name=skill_name)

        cached = await self._skill_cache.get(skill_name, parameters)
        if cached:
            ctx.pop_frame()
            return cached.output

        result = await self._skill_engine.execute(skill_name, parameters)

        if result.success:
            await self._skill_cache.set(skill_name, parameters, result)

        await self._event_bus.publish(
            SystemEvents.SKILL_EXECUTED if result.success else SystemEvents.SKILL_FAILED,
            data={
                "skill_name": skill_name,
                "success": result.success,
                "execution_time_ms": result.execution_time_ms,
                "trace_id": ctx.trace_id
            },
            source=self._config.name,
            trace_id=ctx.trace_id
        )

        ctx.pop_frame()

        if not result.success:
            raise RuntimeError(f"Skill '{skill_name}' failed: {result.error}")

        return result.output

    async def _execute_via_task(
        self,
        workflow_id: str,
        variables: Dict[str, Any],
        ctx: AgentContext
    ) -> Any:
        """通过任务框架执行"""
        ctx.push_frame("task", "execute_workflow", workflow_id=workflow_id)

        await self._event_bus.publish(
            SystemEvents.TASK_STARTED,
            data={"workflow_id": workflow_id, "trace_id": ctx.trace_id},
            source=self._config.name,
            trace_id=ctx.trace_id
        )

        result = await self._task_engine.execute_workflow(workflow_id, variables)

        await self._event_bus.publish(
            SystemEvents.TASK_COMPLETED if result.success else SystemEvents.TASK_FAILED,
            data={
                "workflow_id": workflow_id,
                "success": result.success,
                "completed_tasks": result.completed_tasks,
                "failed_tasks": result.failed_tasks,
                "execution_time_ms": result.execution_time_ms,
                "trace_id": ctx.trace_id
            },
            source=self._config.name,
            trace_id=ctx.trace_id
        )

        ctx.pop_frame()

        if not result.success:
            raise RuntimeError(f"Workflow '{workflow_id}' failed: {result.error}")

        return result.output

    async def _try_auto_skill(self, input_data: Any, ctx: AgentContext) -> Optional[Any]:
        """尝试自动匹配技能执行"""
        if isinstance(input_data, str):
            search_results = self._skill_registry.search_skills(input_data)
            if search_results:
                best_match = search_results[0]
                skill_name = best_match.name
                try:
                    return await self._execute_via_skill(
                        skill_name,
                        {"input": input_data},
                        ctx
                    )
                except Exception as e:
                    logger.debug(f"Auto-skill '{skill_name}' failed: {e}")

        return None

    async def run_async(self, func: Callable[..., T], *args, **kwargs) -> T:
        """在应用上下文中异步执行函数（带错误处理）"""
        result = await self._error_framework.execute_with_error_handling(func, *args, **kwargs)

        if isinstance(result, ExecutionResult):
            if not result.success:
                raise RuntimeError(str(result.error))
            return result.output

        return result

    def on(self, event_name: str, callback: Callable) -> None:
        """注册事件监听器"""
        if event_name not in self._event_listeners:
            self._event_listeners[event_name] = []
        self._event_listeners[event_name].append(callback)

    def off(self, event_name: str, callback: Optional[Callable] = None) -> None:
        """移除事件监听器"""
        if event_name in self._event_listeners:
            if callback is None:
                del self._event_listeners[event_name]
            else:
                try:
                    self._event_listeners[event_name].remove(callback)
                except ValueError:
                    pass

    async def emit(self, event_name: str, data: Any = None) -> int:
        """发出事件（同时通知本地监听器和EventBus）"""
        listeners = self._event_listeners.get(event_name, [])
        count = 0

        for listener in listeners:
            try:
                if asyncio.iscoroutinefunction(listener):
                    await listener(data)
                else:
                    listener(data)
                count += 1
            except Exception as e:
                logger.error(f"Event listener error for '{event_name}': {e}")

        await self._event_bus.publish(event_name, data=data, source=self._config.name)

        return count

    async def get_info(self) -> Dict[str, Any]:
        """获取应用的详细信息"""
        return {
            "app_id": self._app_id,
            "name": self._config.name,
            "version": self._config.version,
            "description": self._config.description,
            "author": self._config.author,
            "created_at": self._created_at.isoformat(),
            "current_state": self._lifecycle.state.value,
            "is_running": self.is_running,
            "uptime_seconds": self._calculate_uptime(),
            "lifecycle_info": self._lifecycle.get_state_info(),
            "state_stats": self._state_manager.get_stats(),
            "config_sources": self._config_center.get_sources_info(),
            "error_stats": self._error_framework.get_error_statistics(),
            "event_bus_stats": self._event_bus.get_stats(),
            "skill_stats": self._skill_registry.get_stats(),
            "skill_cache_stats": self._skill_cache.get_stats(),
            "plugin_stats": self._plugin_manager.get_stats(),
            "registered_events": list(self._event_listeners.keys())
        }

    async def health_check(self) -> Dict[str, Any]:
        """健康检查"""
        status = "healthy"
        details = {}

        if not self.is_running and self._lifecycle.state != AgentState.PAUSED:
            status = "unhealthy"
            details["lifecycle"] = f"Unexpected state: {self._lifecycle.state.value}"
        else:
            details["lifecycle"] = "ok"

        config_validation = await self._config_center.validate()
        if not config_validation.is_valid:
            status = "degraded"
            details["config"] = f"Validation errors: {len(config_validation.errors)}"
        else:
            details["config"] = "ok"

        error_stats = self._error_framework.get_error_statistics()
        recent_fatal_errors = len(self._error_framework.get_recent_errors(level_filter=ErrorLevel.FATAL))
        if recent_fatal_errors > 5:
            status = "unhealthy"
            details["errors"] = f"Too many fatal errors: {recent_fatal_errors}"
        elif recent_fatal_errors > 0:
            status = "degraded"
            details["errors"] = f"Some fatal errors: {recent_fatal_errors}"
        else:
            details["errors"] = "ok"

        event_stats = self._event_bus.get_stats()
        if event_stats.get("total_errors", 0) > 10:
            status = "degraded"
            details["events"] = f"High event error count: {event_stats['total_errors']}"
        else:
            details["events"] = "ok"

        plugin_stats = self._plugin_manager.get_stats()
        error_plugins = plugin_stats.get("state_distribution", {}).get("error", 0)
        if error_plugins > 0:
            details["plugins"] = f"{error_plugins} plugins in error state"
            if status == "healthy":
                status = "degraded"
        else:
            details["plugins"] = "ok"

        return {
            "status": status,
            "timestamp": datetime.now().isoformat(),
            "details": details
        }

    def _register_internal_hooks(self) -> None:
        """注册内部生命周期钩子"""
        self._lifecycle.register_hook("on_start", lambda meta: logger.debug("Starting..."))
        self._lifecycle.register_hook("on_stop", lambda meta: logger.debug("Stopping..."))
        self._lifecycle.register_hook("on_destroy", lambda meta: logger.debug("Destroying..."))

    def _wire_framework_events(self) -> None:
        """连接框架间的内部事件"""
        self._lifecycle.on_state_change(self._on_lifecycle_state_change)

    async def _on_lifecycle_state_change(self, event: LifecycleEvent) -> None:
        """生命周期状态变更时发布到EventBus"""
        event_type_map = {
            AgentState.INITIALIZING: SystemEvents.AGENT_INITIALIZED,
            AgentState.RUNNING: SystemEvents.AGENT_STARTED,
            AgentState.PAUSED: SystemEvents.AGENT_PAUSED,
            AgentState.STOPPED: SystemEvents.AGENT_STOPPED,
            AgentState.DESTROYED: SystemEvents.AGENT_DESTROYED,
            AgentState.ERROR: SystemEvents.AGENT_ERROR,
        }
        event_type = event_type_map.get(event.to_state)
        if event_type:
            await self._event_bus.publish(
                event_type,
                data={
                    "from_state": event.from_state.value if event.from_state else None,
                    "to_state": event.to_state.value,
                    "duration_ms": event.duration_ms,
                },
                source=f"lifecycle:{self._config.name}"
            )

    def _setup_logging(self) -> None:
        """配置日志系统"""
        log_level = getattr(logging, self._config.log_level.upper(), logging.INFO)

        handlers = [logging.StreamHandler()]
        if self._config.log_file:
            handlers.append(logging.FileHandler(self._config.log_file))

        logging.basicConfig(
            level=log_level,
            format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
            handlers=handlers
        )

    async def _load_configurations(self) -> None:
        """加载配置文件"""
        for config_file in self._config.config_files:
            try:
                await self._config_center.load_from_file(config_file)
            except Exception as e:
                logger.warning(f"Failed to load config file {config_file}: {e}")

    async def _setup_default_config(self) -> None:
        """设置默认配置"""
        defaults = {
            "app.name": self._config.name,
            "app.version": self._config.version,
            "debug": False,
            "log_level": self._config.log_level,
        }
        await self._config_center.update(defaults, source="defaults")

    async def _discover_skills(self) -> None:
        """自动发现技能

        扫描指定目录下的Python文件，导入模块并检查是否包含BaseSkill子类，
        将发现的技能注册到SkillRegistry。
        """
        for path in self._config.skill_discovery_paths:
            try:
                dir_path = Path(path)
                if not dir_path.exists():
                    logger.warning(f"Skill discovery path does not exist: {path}")
                    continue

                logger.info(f"Discovering skills in: {path}")

                for py_file in dir_path.rglob("*.py"):
                    if py_file.name.startswith("_"):
                        continue

                    try:
                        module_name = f"agentos_skill_{py_file.stem}"
                        spec = importlib.util.spec_from_file_location(
                            module_name, str(py_file)
                        )
                        if not spec or not spec.loader:
                            logger.debug(f"Cannot load spec for: {py_file}")
                            continue

                        module = importlib.util.module_from_spec(spec)
                        sys.modules[module_name] = module
                        spec.loader.exec_module(module)

                        for attr_name, attr_obj in inspect.getmembers(module, inspect.isclass):
                            if (
                                issubclass(attr_obj, BaseSkill)
                                and attr_obj is not BaseSkill
                                and not inspect.isabstract(attr_obj)
                            ):
                                try:
                                    registered = await self._skill_registry.register(attr_obj)
                                    if registered:
                                        logger.info(
                                            f"Discovered and registered skill: "
                                            f"{attr_obj.__name__} from {py_file}"
                                        )
                                except Exception as reg_err:
                                    logger.warning(
                                        f"Failed to register skill {attr_obj.__name__} "
                                        f"from {py_file}: {reg_err}"
                                    )
                    except Exception as import_err:
                        logger.warning(
                            f"Failed to import module from {py_file}: {import_err}"
                        )

            except Exception as e:
                logger.warning(f"Skill discovery failed for {path}: {e}")

    async def _load_plugins(self) -> None:
        """加载插件"""
        for plugin_dir in self._config.plugin_directories:
            try:
                dir_path = Path(plugin_dir)
                if dir_path.exists():
                    for item in dir_path.iterdir():
                        if item.is_dir() or item.suffix == '.py':
                            try:
                                await self._plugin_manager.load_plugin(str(item))
                            except Exception as e:
                                logger.warning(f"Failed to load plugin {item}: {e}")
            except Exception as e:
                logger.warning(f"Plugin directory scan failed for {plugin_dir}: {e}")

    async def _unload_plugins(self) -> None:
        """卸载所有插件"""
        for plugin_info in self._plugin_manager.list_plugins():
            try:
                await self._plugin_manager.unload_plugin(plugin_info.manifest.plugin_id)
            except Exception as e:
                logger.warning(f"Failed to unload plugin {plugin_info.manifest.plugin_id}: {e}")

    def _calculate_uptime(self) -> float:
        """计算运行时间（秒）"""
        if self._lifecycle.state == AgentState.CREATED:
            return 0.0

        events = self._lifecycle.get_event_history(limit=50)
        start_event = next((e for e in reversed(events) if e.to_state == AgentState.RUNNING), None)

        if start_event:
            return (datetime.now() - start_event.timestamp).total_seconds()

        return 0.0


from .errors import DEFAULT_RETRY_STRATEGY


__all__ = [
    "AgentApplication",
    "AgentConfig",
    "AgentContext",
    "ExecutionContext",
    "ExecutionResult",
    "ExecutionFrame",
    "ErrorCode",
]
