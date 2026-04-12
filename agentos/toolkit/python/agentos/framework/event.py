# AgentOS Event Framework
# Version: 2.0.0
# Last updated: 2026-04-11

"""
事件驱动框架

提供松耦合的组件间通信机制，支持同步/异步事件处理、
消息路由和事件溯源，是AgentOS系统的神经系统。

核心组件:
- EventBus: 事件总线，发布/订阅核心
- EventRouter: 基于规则的事件路由器
- EventStore: 事件存储与溯源
- EventMiddleware: 事件中间件链

设计原则:
1. 解耦 - 发布者和订阅者互不依赖
2. 可靠 - 事件持久化和至少一次投递
3. 可扩展 - 中间件链支持灵活扩展
4. 可观测 - 事件追踪和性能监控
"""

import asyncio
import enum
import logging
import time
import uuid
from collections import defaultdict
from dataclasses import dataclass, field
from datetime import datetime
from typing import Any, Callable, Dict, List, Optional, Set, Type, TypeVar, Union

logger = logging.getLogger(__name__)

T = TypeVar('T')


class EventPriority(enum.IntEnum):
    """事件优先级"""
    LOWEST = 0
    LOW = 25
    NORMAL = 50
    HIGH = 75
    HIGHEST = 100
    CRITICAL = 200


class DispatchMode(enum.Enum):
    """事件分发模式"""
    PARALLEL = "parallel"        # 并行分发给所有订阅者
    SEQUENTIAL = "sequential"    # 按优先级顺序分发给订阅者
    FIRST_MATCH = "first_match"  # 只分发给第一个匹配的订阅者


@dataclass
class EventEnvelope:
    """事件信封，包装事件数据和元信息"""
    event_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    event_type: str = ""
    source: str = ""
    timestamp: datetime = field(default_factory=datetime.now)
    priority: EventPriority = EventPriority.NORMAL
    data: Any = None
    metadata: Dict[str, Any] = field(default_factory=dict)
    
    # 追踪信息
    trace_id: Optional[str] = None
    correlation_id: Optional[str] = None
    causation_id: Optional[str] = None
    
    # 投递信息
    published_at: Optional[float] = None
    delivery_count: int = 0
    
    def to_dict(self) -> Dict[str, Any]:
        return {
            "event_id": self.event_id,
            "event_type": self.event_type,
            "source": self.source,
            "timestamp": self.timestamp.isoformat(),
            "priority": self.priority.value,
            "data": self.data,
            "metadata": self.metadata,
            "trace_id": self.trace_id,
            "correlation_id": self.correlation_id
        }


@dataclass
class Subscription:
    """事件订阅"""
    subscription_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    event_type: str = ""
    handler: Optional[Callable] = None
    priority: EventPriority = EventPriority.NORMAL
    filter_fn: Optional[Callable[[EventEnvelope], bool]] = None
    
    # 订阅选项
    max_calls: Optional[int] = None
    call_count: int = 0
    active: bool = True
    
    # 错误处理
    error_policy: str = "log"  # log, raise, ignore


@dataclass
class HandlerResult:
    """处理器执行结果"""
    handler_id: str
    success: bool
    execution_time_ms: float = 0.0
    error: Optional[str] = None
    result: Any = None


class EventMiddleware:
    """
    事件中间件基类
    
    中间件可以在事件发布前后、处理前后插入自定义逻辑。
    """
    
    async def before_publish(self, envelope: EventEnvelope) -> Optional[EventEnvelope]:
        """发布前拦截，返回None取消发布"""
        return envelope
    
    async def after_publish(self, envelope: EventEnvelope) -> None:
        """发布后处理"""
        pass
    
    async def before_handle(
        self, envelope: EventEnvelope, subscription: Subscription
    ) -> Optional[EventEnvelope]:
        """处理前拦截，返回None跳过该处理器"""
        return envelope
    
    async def after_handle(
        self, envelope: EventEnvelope, subscription: Subscription, result: HandlerResult
    ) -> None:
        """处理后处理"""
        pass


class LoggingMiddleware(EventMiddleware):
    """日志中间件"""
    
    async def before_publish(self, envelope: EventEnvelope) -> Optional[EventEnvelope]:
        logger.debug(
            f"Publishing event: {envelope.event_type} "
            f"(id={envelope.event_id[:8]}, source={envelope.source})"
        )
        return envelope
    
    async def after_handle(
        self, envelope: EventEnvelope, subscription: Subscription, result: HandlerResult
    ) -> None:
        status = "OK" if result.success else f"FAIL({result.error})"
        logger.debug(
            f"Event handled: {envelope.event_type} -> "
            f"handler={subscription.subscription_id[:8]}, {status}, "
            f"time={result.execution_time_ms:.2f}ms"
        )


class MetricsMiddleware(EventMiddleware):
    """指标收集中间件"""
    
    def __init__(self):
        self._publish_counts: Dict[str, int] = defaultdict(int)
        self._handle_counts: Dict[str, int] = defaultdict(int)
        self._handle_times: Dict[str, List[float]] = defaultdict(list)
        self._error_counts: Dict[str, int] = defaultdict(int)
    
    async def after_publish(self, envelope: EventEnvelope) -> None:
        self._publish_counts[envelope.event_type] += 1
    
    async def after_handle(
        self, envelope: EventEnvelope, subscription: Subscription, result: HandlerResult
    ) -> None:
        self._handle_counts[envelope.event_type] += 1
        self._handle_times[envelope.event_type].append(result.execution_time_ms)
        if not result.success:
            self._error_counts[envelope.event_type] += 1
    
    def get_metrics(self) -> Dict[str, Any]:
        metrics = {}
        for event_type in set(list(self._publish_counts.keys()) + list(self._handle_counts.keys())):
            times = self._handle_times.get(event_type, [])
            avg_time = sum(times) / len(times) if times else 0
            metrics[event_type] = {
                "published": self._publish_counts.get(event_type, 0),
                "handled": self._handle_counts.get(event_type, 0),
                "errors": self._error_counts.get(event_type, 0),
                "avg_handle_time_ms": round(avg_time, 3)
            }
        return metrics


class DeduplicationMiddleware(EventMiddleware):
    """事件去重中间件"""
    
    def __init__(self, window_seconds: float = 60.0, max_ids: int = 10000):
        self._window = window_seconds
        self._max_ids = max_ids
        self._seen: Dict[str, float] = {}
    
    async def before_publish(self, envelope: EventEnvelope) -> Optional[EventEnvelope]:
        now = time.time()
        
        # 清理过期条目
        expired = [eid for eid, ts in self._seen.items() if now - ts > self._window]
        for eid in expired:
            del self._seen[eid]
        
        # 检查重复
        if envelope.event_id in self._seen:
            logger.debug(f"Duplicate event detected: {envelope.event_id[:8]}")
            return None
        
        self._seen[envelope.event_id] = now
        
        # 限制大小
        if len(self._seen) > self._max_ids:
            oldest = sorted(self._seen.items(), key=lambda x: x[1])[:len(self._seen) // 2]
            for eid, _ in oldest:
                del self._seen[eid]
        
        return envelope


class EventRouter:
    """
    基于规则的事件路由器
    
    支持条件表达式路由，将事件分发到指定的处理器或目标。
    """
    
    @dataclass
    class RoutingRule:
        rule_id: str
        name: str
        condition: Callable[[EventEnvelope], bool]
        target_subscriptions: List[str] = field(default_factory=list)
        priority: int = 0
        active: bool = True
    
    def __init__(self):
        self._rules: Dict[str, 'EventRouter.RoutingRule'] = {}
    
    def add_rule(
        self,
        rule_id: str,
        name: str,
        condition: Callable[[EventEnvelope], bool],
        target_subscriptions: Optional[List[str]] = None,
        priority: int = 0
    ) -> None:
        self._rules[rule_id] = self.RoutingRule(
            rule_id=rule_id,
            name=name,
            condition=condition,
            target_subscriptions=target_subscriptions or [],
            priority=priority
        )
    
    def remove_rule(self, rule_id: str) -> bool:
        if rule_id in self._rules:
            del self._rules[rule_id]
            return True
        return False
    
    def route(self, envelope: EventEnvelope) -> List[str]:
        """
        路由事件到目标订阅
        
        Returns:
            匹配的订阅ID列表
        """
        matched = []
        
        sorted_rules = sorted(self._rules.values(), key=lambda r: r.priority, reverse=True)
        
        for rule in sorted_rules:
            if not rule.active:
                continue
            try:
                if rule.condition(envelope):
                    matched.extend(rule.target_subscriptions)
            except Exception as e:
                logger.error(f"Routing rule '{rule.rule_id}' error: {e}")
        
        return list(dict.fromkeys(matched))  # 去重保序


class EventStore:
    """
    事件存储与溯源
    
    提供事件的持久化存储、查询和重放能力。
    """
    
    def __init__(self, max_events: int = 10000):
        self._events: List[EventEnvelope] = []
        self._max_events = max_events
        self._by_type: Dict[str, List[int]] = defaultdict(list)
        self._by_trace: Dict[str, List[int]] = defaultdict(list)
    
    async def append(self, envelope: EventEnvelope) -> None:
        """追加事件"""
        idx = len(self._events)
        self._events.append(envelope)
        self._by_type[envelope.event_type].append(idx)
        if envelope.trace_id:
            self._by_trace[envelope.trace_id].append(idx)
        
        if len(self._events) > self._max_events:
            removed = self._events[:len(self._events) - self._max_events]
            self._events = self._events[-self._max_events:]
            self._rebuild_index()
    
    async def query(
        self,
        event_type: Optional[str] = None,
        source: Optional[str] = None,
        trace_id: Optional[str] = None,
        start_time: Optional[datetime] = None,
        end_time: Optional[datetime] = None,
        limit: int = 100
    ) -> List[EventEnvelope]:
        """查询事件"""
        if trace_id and trace_id in self._by_trace:
            indices = self._by_trace[trace_id]
            results = [self._events[i] for i in indices if i < len(self._events)]
        elif event_type and event_type in self._by_type:
            indices = self._by_type[event_type]
            results = [self._events[i] for i in indices if i < len(self._events)]
        else:
            results = list(self._events)
        
        # 过滤
        if source:
            results = [e for e in results if e.source == source]
        if start_time:
            results = [e for e in results if e.timestamp >= start_time]
        if end_time:
            results = [e for e in results if e.timestamp <= end_time]
        
        return results[-limit:]
    
    async def replay(
        self,
        event_type: Optional[str] = None,
        trace_id: Optional[str] = None,
        handler: Optional[Callable[[EventEnvelope], None]] = None
    ) -> int:
        """
        重放事件
        
        Returns:
            重放的事件数量
        """
        events = await self.query(event_type=event_type, trace_id=trace_id, limit=self._max_events)
        
        count = 0
        for event in events:
            if handler:
                try:
                    if asyncio.iscoroutinefunction(handler):
                        await handler(event)
                    else:
                        handler(event)
                except Exception as e:
                    logger.error(f"Replay handler error for event {event.event_id[:8]}: {e}")
            count += 1
        
        return count
    
    def get_stats(self) -> Dict[str, Any]:
        return {
            "total_events": len(self._events),
            "max_events": self._max_events,
            "event_types": dict((k, len(v)) for k, v in self._by_type.items()),
            "trace_count": len(self._by_trace)
        }
    
    def _rebuild_index(self) -> None:
        self._by_type.clear()
        self._by_trace.clear()
        for i, event in enumerate(self._events):
            self._by_type[event.event_type].append(i)
            if event.trace_id:
                self._by_trace[event.trace_id].append(i)


class EventBus:
    """
    事件总线
    
    AgentOS事件系统的核心组件，提供发布/订阅模式的事件通信。
    支持同步/异步处理器、中间件链、事件路由和持久化。
    
    使用示例:
        bus = EventBus()
        
        # 订阅事件
        async def on_agent_started(data):
            print(f"Agent started: {data}")
        
        bus.subscribe("agent.started", on_agent_started)
        
        # 发布事件
        await bus.publish("agent.started", {"name": "my_agent"})
        
        # 取消订阅
        bus.unsubscribe("agent.started", on_agent_started)
    """
    
    def __init__(
        self,
        dispatch_mode: DispatchMode = DispatchMode.PARALLEL,
        enable_store: bool = False,
        store_max_events: int = 10000
    ):
        self._subscriptions: Dict[str, List[Subscription]] = defaultdict(list)
        self._middleware: List[EventMiddleware] = []
        self._router: Optional[EventRouter] = None
        self._store: Optional[EventStore] = None
        self._dispatch_mode = dispatch_mode
        
        if enable_store:
            self._store = EventStore(max_events=store_max_events)
        
        self._stats = {
            "total_published": 0,
            "total_delivered": 0,
            "total_errors": 0,
        }
        
        # 添加默认日志中间件
        self.add_middleware(LoggingMiddleware())
        
        logger.info(f"EventBus initialized (mode={dispatch_mode.value}, store={enable_store})")
    
    def subscribe(
        self,
        event_type: str,
        handler: Callable,
        priority: EventPriority = EventPriority.NORMAL,
        filter_fn: Optional[Callable[[EventEnvelope], bool]] = None,
        max_calls: Optional[int] = None
    ) -> str:
        """
        订阅事件
        
        Args:
            event_type: 事件类型（支持通配符 * 和 #）
            handler: 事件处理回调
            priority: 处理优先级
            filter_fn: 过滤函数
            max_calls: 最大调用次数
            
        Returns:
            订阅ID
        """
        sub = Subscription(
            event_type=event_type,
            handler=handler,
            priority=priority,
            filter_fn=filter_fn,
            max_calls=max_calls
        )
        
        self._subscriptions[event_type].append(sub)
        
        logger.debug(f"Subscribed to '{event_type}' (id={sub.subscription_id[:8]})")
        return sub.subscription_id
    
    def unsubscribe(self, subscription_id: str) -> bool:
        """
        取消订阅
        
        Returns:
            是否成功取消
        """
        for event_type, subs in self._subscriptions.items():
            for i, sub in enumerate(subs):
                if sub.subscription_id == subscription_id:
                    subs.pop(i)
                    logger.debug(f"Unsubscribed {subscription_id[:8]} from '{event_type}'")
                    return True
        return False
    
    def unsubscribe_all(self, event_type: Optional[str] = None) -> int:
        """
        取消所有订阅
        
        Args:
            event_type: 指定事件类型（None则取消所有）
            
        Returns:
            取消的订阅数量
        """
        if event_type:
            count = len(self._subscriptions.get(event_type, []))
            self._subscriptions[event_type] = []
            return count
        else:
            count = sum(len(subs) for subs in self._subscriptions.values())
            self._subscriptions.clear()
            return count
    
    async def publish(
        self,
        event_type: str,
        data: Any = None,
        source: str = "",
        priority: EventPriority = EventPriority.NORMAL,
        metadata: Optional[Dict[str, Any]] = None,
        trace_id: Optional[str] = None,
        correlation_id: Optional[str] = None
    ) -> str:
        """
        发布事件
        
        Args:
            event_type: 事件类型
            data: 事件数据
            source: 事件来源
            priority: 事件优先级
            metadata: 附加元数据
            trace_id: 追踪ID
            correlation_id: 关联ID
            
        Returns:
            事件ID
        """
        envelope = EventEnvelope(
            event_type=event_type,
            source=source,
            priority=priority,
            data=data,
            metadata=metadata or {},
            trace_id=trace_id,
            correlation_id=correlation_id
        )
        
        # 执行发布前中间件
        for middleware in self._middleware:
            try:
                result = await middleware.before_publish(envelope)
                if result is None:
                    logger.debug(f"Event publishing cancelled by middleware: {event_type}")
                    return envelope.event_id
                envelope = result
            except Exception as e:
                logger.error(f"Middleware before_publish error: {e}")
        
        envelope.published_at = time.time()
        
        # 查找匹配的订阅者
        matched_subs = self._find_matching_subscriptions(event_type)
        
        # 路由过滤
        if self._router:
            routed_ids = self._router.route(envelope)
            if routed_ids:
                matched_subs = [s for s in matched_subs if s.subscription_id in routed_ids]
        
        # 按优先级排序
        matched_subs.sort(key=lambda s: s.priority, reverse=True)
        
        # 分发事件
        results = await self._dispatch(envelope, matched_subs)
        
        # 持久化
        if self._store:
            await self._store.append(envelope)
        
        # 执行发布后中间件
        for middleware in self._middleware:
            try:
                await middleware.after_publish(envelope)
            except Exception as e:
                logger.error(f"Middleware after_publish error: {e}")
        
        # 更新统计
        self._stats["total_published"] += 1
        self._stats["total_delivered"] += sum(1 for r in results if r.success)
        self._stats["total_errors"] += sum(1 for r in results if not r.success)
        
        return envelope.event_id
    
    async def publish_and_wait(
        self,
        event_type: str,
        data: Any = None,
        timeout: float = 30.0,
        **kwargs
    ) -> List[Any]:
        """
        发布事件并等待所有处理器完成
        
        Returns:
            所有处理器的返回值列表
        """
        results = []
        
        def collector_handler(envelope_data):
            results.append(envelope_data)
        
        temp_sub_id = self.subscribe(event_type, collector_handler)
        
        try:
            await self.publish(event_type, data, **kwargs)
            await asyncio.sleep(min(timeout, 0.1))
        finally:
            self.unsubscribe(temp_sub_id)
        
        return results
    
    def add_middleware(self, middleware: EventMiddleware) -> None:
        """添加中间件"""
        self._middleware.append(middleware)
    
    def remove_middleware(self, middleware_type: Type[EventMiddleware]) -> bool:
        """移除指定类型的中间件"""
        for i, mw in enumerate(self._middleware):
            if isinstance(mw, middleware_type):
                self._middleware.pop(i)
                return True
        return False
    
    def set_router(self, router: EventRouter) -> None:
        """设置事件路由器"""
        self._router = router
    
    def get_store(self) -> Optional[EventStore]:
        """获取事件存储"""
        return self._store
    
    def get_subscriptions(self, event_type: Optional[str] = None) -> List[Subscription]:
        """获取订阅列表"""
        if event_type:
            return list(self._subscriptions.get(event_type, []))
        else:
            all_subs = []
            for subs in self._subscriptions.values():
                all_subs.extend(subs)
            return all_subs
    
    def get_stats(self) -> Dict[str, Any]:
        """获取统计信息"""
        stats = dict(self._stats)
        stats["total_subscriptions"] = sum(len(s) for s in self._subscriptions.values())
        stats["event_types"] = list(self._subscriptions.keys())
        stats["middleware_count"] = len(self._middleware)
        stats["has_router"] = self._router is not None
        stats["has_store"] = self._store is not None
        
        if self._store:
            stats["store_stats"] = self._store.get_stats()
        
        # 收集指标中间件数据
        for mw in self._middleware:
            if isinstance(mw, MetricsMiddleware):
                stats["event_metrics"] = mw.get_metrics()
        
        return stats
    
    def _find_matching_subscriptions(self, event_type: str) -> List[Subscription]:
        """查找匹配事件类型的订阅"""
        matched = []
        
        # 精确匹配
        matched.extend(self._subscriptions.get(event_type, []))
        
        # 通配符匹配
        for pattern, subs in self._subscriptions.items():
            if pattern == event_type:
                continue
            if self._match_pattern(pattern, event_type):
                matched.extend(subs)
        
        # 过滤非活跃订阅
        return [s for s in matched if s.active]
    
    def _match_pattern(self, pattern: str, event_type: str) -> bool:
        """匹配事件类型模式"""
        if pattern == "*":
            return True
        if pattern.endswith(".*"):
            prefix = pattern[:-2]
            return event_type.startswith(prefix + ".")
        if "*" in pattern:
            import fnmatch
            return fnmatch.fnmatch(event_type, pattern)
        return False
    
    async def _dispatch(
        self,
        envelope: EventEnvelope,
        subscriptions: List[Subscription]
    ) -> List[HandlerResult]:
        """分发事件到订阅者"""
        results = []
        
        if self._dispatch_mode == DispatchMode.PARALLEL:
            tasks = [
                self._invoke_handler(envelope, sub)
                for sub in subscriptions
            ]
            results = await asyncio.gather(*tasks, return_exceptions=True)
            results = [
                r if isinstance(r, HandlerResult) else HandlerResult(
                    handler_id="unknown", success=False, error=str(r)
                )
                for r in results
            ]
        
        elif self._dispatch_mode == DispatchMode.SEQUENTIAL:
            for sub in subscriptions:
                result = await self._invoke_handler(envelope, sub)
                results.append(result)
        
        elif self._dispatch_mode == DispatchMode.FIRST_MATCH:
            for sub in subscriptions:
                result = await self._invoke_handler(envelope, sub)
                results.append(result)
                if result.success:
                    break
        
        return results
    
    async def _invoke_handler(
        self,
        envelope: EventEnvelope,
        subscription: Subscription
    ) -> HandlerResult:
        """调用事件处理器"""
        start_time = time.time()
        
        # 处理前中间件
        for middleware in self._middleware:
            try:
                result = await middleware.before_handle(envelope, subscription)
                if result is None:
                    return HandlerResult(
                        handler_id=subscription.subscription_id,
                        success=True,
                        execution_time_ms=0
                    )
                envelope = result
            except Exception as e:
                logger.error(f"Middleware before_handle error: {e}")
        
        # 过滤检查
        if subscription.filter_fn:
            try:
                if not subscription.filter_fn(envelope):
                    return HandlerResult(
                        handler_id=subscription.subscription_id,
                        success=True,
                        execution_time_ms=0
                    )
            except Exception as e:
                logger.error(f"Filter function error: {e}")
        
        # 调用次数检查
        if subscription.max_calls is not None:
            if subscription.call_count >= subscription.max_calls:
                subscription.active = False
                return HandlerResult(
                    handler_id=subscription.subscription_id,
                    success=True,
                    execution_time_ms=0
                )
        
        # 执行处理器
        handler_result = HandlerResult(
            handler_id=subscription.subscription_id,
            success=True
        )
        
        try:
            if asyncio.iscoroutinefunction(subscription.handler):
                ret = await subscription.handler(envelope)
            else:
                ret = subscription.handler(envelope)
            
            handler_result.result = ret
            subscription.call_count += 1
            envelope.delivery_count += 1
            
        except Exception as e:
            handler_result.success = False
            handler_result.error = str(e)
            
            if subscription.error_policy == "raise":
                raise
            elif subscription.error_policy == "log":
                logger.error(
                    f"Event handler error for '{envelope.event_type}': {e}"
                )
        
        handler_result.execution_time_ms = (time.time() - start_time) * 1000
        
        # 处理后中间件
        for middleware in self._middleware:
            try:
                await middleware.after_handle(envelope, subscription, handler_result)
            except Exception as e:
                logger.error(f"Middleware after_handle error: {e}")
        
        return handler_result


# 预定义事件类型
class SystemEvents:
    """系统预定义事件类型"""
    
    # 生命周期事件
    AGENT_CREATED = "agent.created"
    AGENT_INITIALIZED = "agent.initialized"
    AGENT_STARTED = "agent.started"
    AGENT_STOPPED = "agent.stopped"
    AGENT_PAUSED = "agent.paused"
    AGENT_RESUMED = "agent.resumed"
    AGENT_ERROR = "agent.error"
    AGENT_DESTROYED = "agent.destroyed"
    
    # 任务事件
    TASK_SUBMITTED = "task.submitted"
    TASK_STARTED = "task.started"
    TASK_COMPLETED = "task.completed"
    TASK_FAILED = "task.failed"
    TASK_CANCELLED = "task.cancelled"
    TASK_PROGRESS = "task.progress"
    
    # 技能事件
    SKILL_REGISTERED = "skill.registered"
    SKILL_UNREGISTERED = "skill.unregistered"
    SKILL_EXECUTED = "skill.executed"
    SKILL_FAILED = "skill.failed"
    SKILL_CACHED = "skill.cached"
    
    # 记忆事件
    MEMORY_WRITTEN = "memory.written"
    MEMORY_READ = "memory.read"
    MEMORY_DELETED = "memory.deleted"
    MEMORY_FUSED = "memory.fused"
    
    # 配置事件
    CONFIG_LOADED = "config.loaded"
    CONFIG_CHANGED = "config.changed"
    CONFIG_VALIDATED = "config.validated"
    
    # 插件事件
    PLUGIN_LOADED = "plugin.loaded"
    PLUGIN_ACTIVATED = "plugin.activated"
    PLUGIN_DEACTIVATED = "plugin.deactivated"
    PLUGIN_UNLOADED = "plugin.unloaded"
    PLUGIN_ERROR = "plugin.error"
    
    # 应用事件
    APPLICATION_STARTED = "application.started"
    APPLICATION_STOPPING = "application.stopping"
    APPLICATION_STOPPED = "application.stopped"
    
    # 执行事件
    EXECUTION_BEFORE = "execution.before"
    EXECUTION_AFTER = "execution.after"
    EXECUTION_ERROR = "execution.error"


__all__ = [
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
]