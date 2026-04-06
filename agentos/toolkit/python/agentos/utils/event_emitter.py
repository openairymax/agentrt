# AgentOS Python SDK - Event Emitter
# Version: 3.0.0
# Last updated: 2026-04-05
#
# 事件订阅机制（EventEmitter 模式）
# 支持：事件发布/订阅、异步事件处理、事件过滤
# 遵循 ARCHITECTURAL_PRINCIPLES.md A-1（简约至上）和 E-2（可观测性）

import asyncio
import logging
import time
from typing import Any, Callable, Dict, List, Optional, Union
from dataclasses import dataclass, field
from functools import wraps
import threading
from collections import defaultdict
import weakref

logger = logging.getLogger(__name__)


@dataclass
class Event:
    """
    事件对象
    
    Attributes:
        name: 事件名称
        data: 事件数据
        timestamp: 时间戳
        source: 事件源
        metadata: 元数据
    """
    name: str
    data: Any
    timestamp: float = field(default_factory=time.time)
    source: Optional[str] = None
    metadata: Dict[str, Any] = field(default_factory=dict)
    
    def to_dict(self) -> dict:
        """转换为字典"""
        return {
            "name": self.name,
            "data": self.data,
            "timestamp": self.timestamp,
            "source": self.source,
            "metadata": self.metadata
        }


@dataclass
class EventHandler:
    """
    事件处理器
    
    Attributes:
        callback: 回调函数
        once: 是否只执行一次
        priority: 优先级（数字越小优先级越高）
        filter_func: 过滤函数
    """
    callback: Callable
    once: bool = False
    priority: int = 0
    filter_func: Optional[Callable[[Event], bool]] = None


class EventEmitter:
    """
    事件发射器（同步版本）
    
    功能:
        - 事件订阅（on/once）
        - 事件取消订阅（off）
        - 事件发布（emit）
        - 事件监听器管理
    
    Example:
        >>> emitter = EventEmitter()
        >>> 
        >>> # 订阅事件
        >>> def handler(event):
        ...     print(f"收到事件：{event.name}")
        >>> emitter.on("task.created", handler)
        >>> 
        >>> # 发布事件
        >>> emitter.emit("task.created", {"task_id": "123"})
        >>> 
        >>> # 取消订阅
        >>> emitter.off("task.created", handler)
    """
    
    def __init__(self):
        """初始化事件发射器"""
        self._listeners: Dict[str, List[EventHandler]] = defaultdict(list)
        self._lock = threading.RLock()
        self._event_count = 0
        self._max_listeners = 100
    
    def on(
        self,
        event_name: str,
        callback: Callable[[Event], None],
        priority: int = 0,
        filter_func: Optional[Callable[[Event], bool]] = None
    ):
        """
        订阅事件
        
        Args:
            event_name: 事件名称
            callback: 回调函数
            priority: 优先级（数字越小优先级越高）
            filter_func: 过滤函数（返回 True 才执行）
        
        Example:
            >>> def handler(event):
            ...     print(f"任务创建：{event.data}")
            >>> emitter.on("task.created", handler, priority=1)
        """
        with self._lock:
            # 检查监听器数量
            if len(self._listeners[event_name]) >= self._max_listeners:
                logger.warning(
                    f"事件 {event_name} 的监听器数量超过限制 ({self._max_listeners})"
                )
            
            handler = EventHandler(
                callback=callback,
                once=False,
                priority=priority,
                filter_func=filter_func
            )
            
            self._listeners[event_name].append(handler)
            # 按优先级排序
            self._listeners[event_name].sort(key=lambda h: h.priority)
    
    def once(
        self,
        event_name: str,
        callback: Callable[[Event], None],
        priority: int = 0
    ):
        """
        订阅一次性事件（执行后自动取消订阅）
        
        Args:
            event_name: 事件名称
            callback: 回调函数
            priority: 优先级
        
        Example:
            >>> def handler(event):
            ...     print("只执行一次")
            >>> emitter.once("task.completed", handler)
        """
        with self._lock:
            handler = EventHandler(
                callback=callback,
                once=True,
                priority=priority
            )
            self._listeners[event_name].append(handler)
            self._listeners[event_name].sort(key=lambda h: h.priority)
    
    def off(
        self,
        event_name: str,
        callback: Optional[Callable] = None
    ):
        """
        取消订阅
        
        Args:
            event_name: 事件名称
            callback: 回调函数（可选，不传则取消该事件的所有订阅）
        
        Example:
            >>> # 取消特定订阅
            >>> emitter.off("task.created", handler)
            >>> 
            >>> # 取消所有订阅
            >>> emitter.off("task.created")
        """
        with self._lock:
            if callback is None:
                # 取消所有订阅
                self._listeners[event_name] = []
            else:
                # 取消特定订阅
                self._listeners[event_name] = [
                    h for h in self._listeners[event_name]
                    if h.callback != callback
                ]
    
    def emit(
        self,
        event_name: str,
        data: Any = None,
        source: Optional[str] = None,
        metadata: Optional[Dict] = None,
        async_mode: bool = False
    ) -> int:
        """
        发布事件
        
        Args:
            event_name: 事件名称
            data: 事件数据
            source: 事件源
            metadata: 元数据
            async_mode: 是否异步执行
        
        Returns:
            int: 执行的监听器数量
        
        Example:
            >>> # 同步发布
            >>> count = emitter.emit("task.created", {"task_id": "123"})
            >>> print(f"{count} 个监听器已执行")
            >>> 
            >>> # 异步发布
            >>> emitter.emit("task.running", data, async_mode=True)
        """
        event = Event(
            name=event_name,
            data=data,
            source=source,
            metadata=metadata or {}
        )
        
        with self._lock:
            listeners = self._listeners[event_name].copy()
        
        executed = 0
        to_remove = []
        
        for handler in listeners:
            # 应用过滤器
            if handler.filter_func and not handler.filter_func(event):
                continue
            
            try:
                if async_mode:
                    # 异步执行
                    threading.Thread(
                        target=self._execute_handler,
                        args=(handler, event),
                        daemon=True
                    ).start()
                else:
                    # 同步执行
                    self._execute_handler(handler, event)
                
                executed += 1
                
                # 标记一次性监听器
                if handler.once:
                    to_remove.append(handler.callback)
                    
            except Exception as e:
                logger.error(
                    f"执行事件处理器失败 [{event_name}]: {e}",
                    exc_info=True
                )
        
        # 移除一次性监听器
        if to_remove:
            with self._lock:
                for callback in to_remove:
                    self.off(event_name, callback)
        
        self._event_count += 1
        return executed
    
    def _execute_handler(
        self,
        handler: EventHandler,
        event: Event
    ):
        """执行事件处理器"""
        handler.callback(event)
    
    def listener_count(self, event_name: str) -> int:
        """
        获取监听器数量
        
        Args:
            event_name: 事件名称
        
        Returns:
            int: 监听器数量
        """
        with self._lock:
            return len(self._listeners[event_name])
    
    def listeners(self, event_name: str) -> List[Callable]:
        """
        获取所有监听器
        
        Args:
            event_name: 事件名称
        
        Returns:
            List[Callable]: 监听器列表
        """
        with self._lock:
            return [h.callback for h in self._listeners[event_name]]
    
    def remove_all_listeners(self, event_name: Optional[str] = None):
        """
        移除所有监听器
        
        Args:
            event_name: 事件名称（可选，不传则移除所有事件）
        """
        with self._lock:
            if event_name:
                self._listeners[event_name] = []
            else:
                self._listeners.clear()
    
    def set_max_listeners(self, n: int):
        """
        设置最大监听器数量
        
        Args:
            n: 最大数量
        """
        self._max_listeners = n
    
    def get_stats(self) -> Dict[str, Any]:
        """
        获取统计信息
        
        Returns:
            dict: 统计信息
        """
        with self._lock:
            return {
                "total_events": self._event_count,
                "event_types": len(self._listeners),
                "listeners": {
                    name: len(handlers)
                    for name, handlers in self._listeners.items()
                    if handlers
                }
            }


class AsyncEventEmitter:
    """
    异步事件发射器（asyncio 版本）
    
    功能:
        - 异步事件订阅（on/once）
        - 异步事件发布（emit）
        - 并发事件处理
    
    Example:
        >>> emitter = AsyncEventEmitter()
        >>> 
        >>> async def handler(event):
        ...     print(f"异步处理：{event.data}")
        >>> emitter.on("task.created", handler)
        >>> 
        >>> # 异步发布
        >>> await emitter.emit("task.created", {"task_id": "123"})
    """
    
    def __init__(self):
        """初始化异步事件发射器"""
        self._listeners: Dict[str, List[EventHandler]] = defaultdict(list)
        self._lock = asyncio.Lock()
        self._event_count = 0
        self._max_listeners = 100
    
    def on(
        self,
        event_name: str,
        callback: Union[
            Callable[[Event], None],
            Callable[[Event], asyncio.coroutine]
        ],
        priority: int = 0,
        filter_func: Optional[Callable[[Event], bool]] = None
    ):
        """
        订阅事件
        
        Args:
            event_name: 事件名称
            callback: 回调函数（可以是异步函数）
            priority: 优先级
            filter_func: 过滤函数
        """
        handler = EventHandler(
            callback=callback,
            once=False,
            priority=priority,
            filter_func=filter_func
        )
        self._listeners[event_name].append(handler)
        self._listeners[event_name].sort(key=lambda h: h.priority)
    
    def once(
        self,
        event_name: str,
        callback: Callable,
        priority: int = 0
    ):
        """订阅一次性事件"""
        handler = EventHandler(
            callback=callback,
            once=True,
            priority=priority
        )
        self._listeners[event_name].append(handler)
        self._listeners[event_name].sort(key=lambda h: h.priority)
    
    def off(
        self,
        event_name: str,
        callback: Optional[Callable] = None
    ):
        """取消订阅"""
        if callback is None:
            self._listeners[event_name] = []
        else:
            self._listeners[event_name] = [
                h for h in self._listeners[event_name]
                if h.callback != callback
            ]
    
    async def emit(
        self,
        event_name: str,
        data: Any = None,
        source: Optional[str] = None,
        metadata: Optional[Dict] = None,
        concurrent: bool = True
    ) -> int:
        """
        发布事件（异步）
        
        Args:
            event_name: 事件名称
            data: 事件数据
            source: 事件源
            metadata: 元数据
            concurrent: 是否并发执行监听器
        
        Returns:
            int: 执行的监听器数量
        """
        event = Event(
            name=event_name,
            data=data,
            source=source,
            metadata=metadata or {}
        )
        
        async with self._lock:
            listeners = self._listeners[event_name].copy()
        
        executed = 0
        to_remove = []
        
        async def execute_handler(handler: EventHandler):
            """执行单个处理器"""
            if handler.filter_func and not handler.filter_func(event):
                return False
            
            try:
                if asyncio.iscoroutinefunction(handler.callback):
                    await handler.callback(event)
                else:
                    handler.callback(event)
                
                if handler.once:
                    to_remove.append(handler.callback)
                
                return True
            except Exception as e:
                logger.error(
                    f"异步执行事件处理器失败 [{event_name}]: {e}",
                    exc_info=True
                )
                return False
        
        if concurrent:
            # 并发执行所有监听器
            tasks = [execute_handler(h) for h in listeners]
            results = await asyncio.gather(*tasks, return_exceptions=True)
            executed = sum(1 for r in results if r is True)
        else:
            # 顺序执行
            for handler in listeners:
                if await execute_handler(handler):
                    executed += 1
        
        # 移除一次性监听器
        if to_remove:
            async with self._lock:
                for callback in to_remove:
                    self.off(event_name, callback)
        
        self._event_count += 1
        return executed
    
    def listener_count(self, event_name: str) -> int:
        """获取监听器数量"""
        return len(self._listeners[event_name])
    
    def get_stats(self) -> Dict[str, Any]:
        """获取统计信息"""
        return {
            "total_events": self._event_count,
            "event_types": len(self._listeners),
            "listeners": {
                name: len(handlers)
                for name, handlers in self._listeners.items()
                if handlers
            }
        }


# 装饰器：自动发布事件
def emit_event(event_name: str, emitter: Optional[EventEmitter] = None):
    """
    装饰器：函数执行后自动发布事件
    
    Args:
        event_name: 事件名称
        emitter: 事件发射器（可选，默认创建全局实例）
    
    Example:
        >>> @emit_event("task.created")
        >>> def create_task(description):
        ...     task = Task(description)
        ...     return task
    """
    def decorator(func: Callable):
        @wraps(func)
        def wrapper(*args, **kwargs):
            result = func(*args, **kwargs)
            
            # 获取或创建发射器
            em = emitter or get_global_emitter()
            
            # 发布事件
            em.emit(
                event_name,
                data={
                    "function": func.__name__,
                    "args": args,
                    "kwargs": kwargs,
                    "result": result
                }
            )
            
            return result
        
        return wrapper
    return decorator


# 全局事件发射器实例
_global_emitter: Optional[EventEmitter] = None
_global_async_emitter: Optional[AsyncEventEmitter] = None


def get_global_emitter() -> EventEmitter:
    """获取全局事件发射器"""
    global _global_emitter
    if _global_emitter is None:
        _global_emitter = EventEmitter()
    return _global_emitter


def get_global_async_emitter() -> AsyncEventEmitter:
    """获取全局异步事件发射器"""
    global _global_async_emitter
    if _global_async_emitter is None:
        _global_async_emitter = AsyncEventEmitter()
    return _global_async_emitter


# 预定义事件常量
class BuiltinEvents:
    """内置事件常量"""
    
    # 任务事件
    TASK_CREATED = "task.created"
    TASK_STARTED = "task.started"
    TASK_PROGRESS = "task.progress"
    TASK_COMPLETED = "task.completed"
    TASK_FAILED = "task.failed"
    TASK_CANCELLED = "task.cancelled"
    
    # 记忆事件
    MEMORY_WRITTEN = "memory.written"
    MEMORY_SEARCHED = "memory.searched"
    MEMORY_DELETED = "memory.deleted"
    
    # 会话事件
    SESSION_CREATED = "session.created"
    SESSION_CLOSED = "session.closed"
    SESSION_EXPIRED = "session.expired"
    
    # 技能事件
    SKILL_LOADED = "skill.loaded"
    SKILL_UNLOADED = "skill.unloaded"
    SKILL_EXECUTED = "skill.executed"
    
    # 系统事件
    SYSTEM_STARTED = "system.started"
    SYSTEM_STOPPED = "system.stopped"
    SYSTEM_ERROR = "system.error"
