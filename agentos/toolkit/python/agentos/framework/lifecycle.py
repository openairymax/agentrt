# AgentOS Lifecycle Manager
# Version: 2.0.0
# Last updated: 2026-04-11

"""
智能体生命周期管理器

提供完整的生命周期状态管理和转换控制，确保智能体应用在各个阶段的行为一致性和可预测性。

支持的生命周期状态:
- CREATED → INITIALIZING → INITIALIZED
- INITIALIZED → STARTING → RUNNING
- RUNNING → PAUSING → PAUSED
- PAUSED → RESUMING → RUNNING
- RUNNING → STOPPING → STOPPED
- (任何状态) → ERROR → DESTROYING → DESTROYED

设计原则:
1. 状态机模式 - 明确定义的状态和合法转换
2. 钩子机制 - 在每个状态变化时提供扩展点
3. 异步支持 - 所有操作都是异步的，支持并发场景
4. 可观测性 - 内置事件发布和日志记录
"""

import asyncio
import enum
import logging
import time
import uuid
from dataclasses import dataclass, field
from datetime import datetime
from typing import Any, Callable, Dict, List, Optional, Set

logger = logging.getLogger(__name__)


class AgentState(enum.Enum):
    """智能体生命周期状态枚举"""
    CREATED = "created"
    INITIALIZING = "initializing"
    INITIALIZED = "initialized"
    STARTING = "starting"
    RUNNING = "running"
    PAUSING = "pausing"
    PAUSED = "paused"
    RESUMING = "resuming"
    STOPPING = "stopping"
    STOPPED = "stopped"
    DESTROYING = "destroying"
    DESTROYED = "destroyed"
    ERROR = "error"


@dataclass
class LifecycleEvent:
    """生命周期事件数据"""
    event_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    timestamp: datetime = field(default_factory=datetime.now)
    from_state: Optional[AgentState] = None
    to_state: AgentState = None
    duration_ms: float = 0.0
    metadata: Dict[str, Any] = field(default_factory=dict)

    def to_dict(self) -> Dict[str, Any]:
        """转换为字典"""
        return {
            "event_id": self.event_id,
            "timestamp": self.timestamp.isoformat(),
            "from_state": self.from_state.value if self.from_state else None,
            "to_state": self.to_state.value if self.to_state else None,
            "duration_ms": self.duration_ms,
            "metadata": self.metadata
        }


class LifecycleManager:
    """
    智能体生命周期管理器

    负责管理智能体的完整生命周期，包括：
    - 状态转换控制和验证
    - 生命周期钩子的执行
    - 事件的发布和记录
    - 并发访问的保护

    使用示例:
        manager = LifecycleManager()

        # 注册钩子
        manager.on_state_change(lambda event: print(f"State changed to {event.to_state}"))

        # 执行生命周期操作
        await manager.initialize(config)
        await manager.start()
        await manager.stop()
        await manager.destroy()
    """

    # 定义合法的状态转换
    VALID_TRANSITIONS: Dict[AgentState, Set[AgentState]] = {
        AgentState.CREATED: {AgentState.INITIALIZING},
        AgentState.INITIALIZING: {AgentState.INITIALIZED, AgentState.ERROR},
        AgentState.INITIALIZED: {AgentState.STARTING, AgentState.DESTROYING},
        AgentState.STARTING: {AgentState.RUNNING, AgentState.ERROR},
        AgentState.RUNNING: {AgentState.PAUSING, AgentState.STOPPING, AgentState.ERROR},
        AgentState.PAUSING: {AgentState.PAUSED, AgentState.ERROR},
        AgentState.PAUSED: {AgentState.RESUMING, AgentState.STOPPING, AgentState.DESTROYING},
        AgentState.RESUMING: {AgentState.RUNNING, AgentState.ERROR},
        AgentState.STOPPING: {AgentState.STOPPED, AgentState.ERROR},
        AgentState.STOPPED: {AgentState.DESTROYING, AgentState.STARTING},
        AgentState.DESTROYING: {AgentState.DESTROYED, AgentState.ERROR},
        AgentState.ERROR: {AgentState.DESTROYING, AgentState.INITIALIZING},  # 从错误恢复或销毁
        AgentState.DESTROYED: set(),  # 终态，不能转换到其他状态
    }

    # 终态集合
    TERMINAL_STATES: Set[AgentState] = {AgentState.DESTROYED}

    def __init__(self):
        """初始化生命周期管理器"""
        self._state: AgentState = AgentState.CREATED
        self._lock: asyncio.Lock = asyncio.Lock()
        self._state_change_listeners: List[Callable[[LifecycleEvent], None]] = []
        self._lifecycle_hooks: Dict[str, List[Callable]] = {
            "on_create": [],
            "on_initialize": [],
            "on_start": [],
            "on_pause": [],
            "on_resume": [],
            "on_stop": [],
            "on_destroy": [],
            "on_error": [],
            "on_state_change": [],
        }
        self._event_history: List[LifecycleEvent] = []
        self._max_history_size: int = 100

        logger.info("LifecycleManager initialized in CREATED state")

    @property
    def state(self) -> AgentState:
        """获取当前状态（线程安全）"""
        return self._state

    @property
    def is_running(self) -> bool:
        """检查是否处于运行状态"""
        return self._state == AgentState.RUNNING

    @property
    def is_initialized(self) -> bool:
        """检查是否已初始化"""
        return self._state in {
            AgentState.INITIALIZED,
            AgentState.RUNNING,
            AgentState.PAUSED,
            AgentState.PAUSING,
            AgentState.RESUMING,
            AgentState.STOPPING,
            AgentState.STOPPED,
        }

    @property
    def can_start(self) -> bool:
        """检查是否可以启动"""
        return self._state == AgentState.INITIALIZED or self._state == AgentState.STOPPED

    @property
    def can_stop(self) -> bool:
        """检查是否可以停止"""
        return self._state in {AgentState.RUNNING, AgentState.PAUSED}

    async def transition_to(self, target_state: AgentState, **metadata) -> bool:
        """
        执行状态转换

        Args:
            target_state: 目标状态
            **metadata: 附加元数据

        Returns:
            是否成功转换

        Raises:
            ValueError: 如果转换不合法
        """
        async with self._lock:
            start_time = time.time()

            # 检查是否是终态
            if self._state in self.TERMINAL_STATES:
                raise ValueError(
                    f"Cannot transition from terminal state {self._state.value}"
                )

            # 验证转换合法性
            valid_targets = self.VALID_TRANSITIONS.get(self._state, set())
            if target_state not in valid_targets:
                raise ValueError(
                    f"Invalid state transition: {self._state.value} -> {target_state.value}. "
                    f"Valid targets are: {[s.value for s in valid_targets]}"
                )

            # 记录旧状态
            old_state = self._state

            try:
                # 执行进入目标状态的钩子
                hook_name = self._get_hook_name(target_state)
                await self._execute_hooks(hook_name, metadata)

                # 更新状态
                self._state = target_state

                # 创建事件
                duration_ms = (time.time() - start_time) * 1000
                event = LifecycleEvent(
                    from_state=old_state,
                    to_state=target_state,
                    duration_ms=duration_ms,
                    metadata=metadata
                )

                # 记录事件历史
                self._record_event(event)

                # 通知监听者
                await self._notify_listeners(event)

                logger.info(
                    f"State transition: {old_state.value} -> {target_state.value} "
                    f"(took {duration_ms:.2f}ms)"
                )

                return True

            except Exception as e:
                # 转换失败，记录错误
                error_event = LifecycleEvent(
                    from_state=old_state,
                    to_state=self._state,
                    metadata={"error": str(e), **metadata}
                )
                self._record_event(error_event)
                logger.error(f"State transition failed: {e}")
                raise

    async def initialize(self, config: Any = None) -> None:
        """
        初始化智能体

        Args:
            config: 配置对象
        """
        await self.transition_to(AgentState.INITIALIZING, config=config)
        await self.transition_to(AgentState.INITIALIZED)

    async def start(self) -> None:
        """启动智能体"""
        await self.transition_to(AgentState.STARTING)
        await self.transition_to(AgentState.RUNNING)

    async def pause(self) -> None:
        """暂停智能体"""
        await self.transition_to(AgentState.PAUSING)
        await self.transition_to(AgentState.PAUSED)

    async def resume(self) -> None:
        """恢复运行"""
        await self.transition_to(AgentState.RESUMING)
        await self.transition_to(AgentState.RUNNING)

    async def stop(self) -> None:
        """停止智能体"""
        await self.transition_to(AgentState.STOPPING)
        await self.transition_to(AgentState.STOPPED)

    async def destroy(self) -> None:
        """销毁智能体"""
        await self.transition_to(AgentState.DESTROYING)
        await self.transition_to(AgentState.DESTROYED)

    async def force_destroy(self) -> None:
        """强制销毁（从任意状态）"""
        async with self._lock:
            if self._state != AgentState.DESTROYED:
                self._state = AgentState.DESTROYING
                await self._execute_hooks("on_destroy")
                self._state = AgentState.DESTROYED
                logger.warning("Force destroy completed")

    def register_hook(self, hook_name: str, callback: Callable) -> None:
        """
        注册生命周期钩子

        Args:
            hook_name: 钩子名称 (on_create, on_initialize, on_start, etc.)
            callback: 回调函数
        """
        if hook_name not in self._lifecycle_hooks:
            raise ValueError(f"Unknown lifecycle hook: {hook_name}")
        self._lifecycle_hooks[hook_name].append(callback)
        logger.debug(f"Registered hook: {hook_name}")

    def unregister_hook(self, hook_name: str, callback: Callable) -> bool:
        """
        注销生命周期钩子

        Returns:
            是否成功移除
        """
        if hook_name in self._lifecycle_hooks:
            try:
                self._lifecycle_hooks[hook_name].remove(callback)
                return True
            except ValueError:
                pass
        return False

    def on_state_change(self, listener: Callable[[LifecycleEvent], None]) -> None:
        """
        注册状态变更监听器

        Args:
            listener: 监听回调函数
        """
        self._state_change_listeners.append(listener)

    def off_state_change(self, listener: Callable[[LifecycleEvent], None]) -> bool:
        """
        移除状态变更监听器

        Returns:
            是否成功移除
        """
        try:
            self._state_change_listeners.remove(listener)
            return True
        except ValueError:
            return False

    def get_event_history(self, limit: int = 50) -> List[LifecycleEvent]:
        """
        获取事件历史

        Args:
            limit: 最大返回数量

        Returns:
            事件列表（按时间倒序）
        """
        return list(reversed(self._event_history[-limit:]))

    def get_state_info(self) -> Dict[str, Any]:
        """
        获取详细的状态信息

        Returns:
            包含状态详情的字典
        """
        return {
            "current_state": self._state.value,
            "is_terminal": self._state in self.TERMINAL_STATES,
            "is_running": self.is_running,
            "is_initialized": self.is_initialized,
            "can_start": self.can_start,
            "can_stop": self.can_stop,
            "valid_transitions": [
                s.value for s in self.VALID_TRANSITIONS.get(self._state, set())
            ],
            "total_events": len(self._event_history),
            "registered_hooks": {
                name: len(hooks) for name, hooks in self._lifecycle_hooks.items()
            },
            "listeners_count": len(self._state_change_listeners)
        }

    async def _execute_hooks(self, hook_name: str, metadata: Dict[str, Any] = None) -> None:
        """执行指定名称的所有钩子"""
        hooks = self._lifecycle_hooks.get(hook_name, [])
        for hook in hooks:
            try:
                if asyncio.iscoroutinefunction(hook):
                    await hook(metadata or {})
                else:
                    hook(metadata or {})
            except Exception as e:
                logger.error(f"Hook {hook_name} failed: {e}")

    async def _notify_listeners(self, event: LifecycleEvent) -> None:
        """通知所有状态变更监听者"""
        for listener in self._state_change_listeners:
            try:
                if asyncio.iscoroutinefunction(listener):
                    await listener(event)
                else:
                    listener(event)
            except Exception as e:
                logger.error(f"Listener notification failed: {e}")

    def _record_event(self, event: LifecycleEvent) -> None:
        """记录事件到历史"""
        self._event_history.append(event)
        # 限制历史大小
        if len(self._event_history) > self._max_history_size:
            self._event_history = self._event_history[-self._max_history_size:]

    def _get_hook_name(self, state: AgentState) -> str:
        """根据状态获取对应的钩子名称"""
        mapping = {
            AgentState.CREATED: "on_create",
            AgentState.INITIALIZING: "on_initialize",
            AgentState.STARTING: "on_start",
            AgentState.PAUSING: "on_pause",
            AgentState.RESUMING: "on_resume",
            AgentState.STOPPING: "on_stop",
            AgentState.DESTROYING: "on_destroy",
            AgentState.ERROR: "on_error",
        }
        return mapping.get(state, "on_state_change")


__all__ = [
    "LifecycleManager",
    "AgentState",
    "LifecycleEvent",
]
