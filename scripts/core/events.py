#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS 事件总线系统
# 统一的遥测和事件处理框架

"""
AgentOS 事件总线

提供统一的事件处理架构，支持：
- 同步/异步事件处理
- 事件优先级和过滤
- 事件历史和回放
- 分布式事件追踪

Architecture:
    EventBus    : 事件总线中央调度器
    Event       : 事件数据模型
    EventHandler: 事件处理器基类
"""

import asyncio
import json
import threading
import time
from abc import ABC, abstractmethod
from collections import defaultdict, deque
from dataclasses import asdict, dataclass, field
from datetime import datetime
from enum import Enum
from typing import Any, Callable, Dict, List, Optional, Set
from uuid import uuid4


class EventType(Enum):
    """事件类型枚举"""
    BUILD_STARTED = "build.started"
    BUILD_COMPLETED = "build.completed"
    BUILD_FAILED = "build.failed"
    TEST_STARTED = "test.started"
    TEST_COMPLETED = "test.completed"
    TEST_FAILED = "test.failed"
    DEPLOY_STARTED = "deploy.started"
    DEPLOY_COMPLETED = "deploy.completed"
    DEPLOY_FAILED = "deploy.failed"
    DIAGNOSTIC_ISSUED = "diagnostic.issued"
    METRIC_RECORDED = "metric.recorded"
    CONFIG_CHANGED = "config.changed"
    ERROR_OCCURRED = "error.occurred"
    CUSTOM = "custom"


class EventPriority(Enum):
    """事件优先级"""
    LOW = 0
    NORMAL = 1
    HIGH = 2
    CRITICAL = 3


@dataclass
class Event:
    """事件数据模型"""
    id: str = field(default_factory=lambda: str(uuid4()))
    type: EventType = EventType.CUSTOM
    priority: EventPriority = EventPriority.NORMAL
    source: str = ""
    timestamp: str = field(default_factory=lambda: datetime.now().isoformat())
    data: Dict[str, Any] = field(default_factory=dict)
    trace_id: str = ""
    parent_trace_id: Optional[str] = None
    tags: Set[str] = field(default_factory=set)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "id": self.id,
            "type": self.type.value,
            "priority": self.priority.value,
            "source": self.source,
            "timestamp": self.timestamp,
            "data": self.data,
            "trace_id": self.trace_id,
            "parent_trace_id": self.parent_trace_id,
            "tags": list(self.tags)
        }

    def to_json(self) -> str:
        return json.dumps(self.to_dict())

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> "Event":
        return cls(
            id=data.get("id", str(uuid4())),
            type=EventType(data.get("type", "custom")),
            priority=EventPriority(data.get("priority", 1)),
            source=data.get("source", ""),
            timestamp=data.get("timestamp", datetime.now().isoformat()),
            data=data.get("data", {}),
            trace_id=data.get("trace_id", ""),
            parent_trace_id=data.get("parent_trace_id"),
            tags=set(data.get("tags", []))
        )


class EventHandler(ABC):
    """事件处理器基类"""

    def __init__(self, name: str, event_types: List[EventType] = None):
        self.name = name
        self.event_types: Set[EventType] = set(event_types or [])
        self.enabled = True

    @abstractmethod
    def handle(self, event: Event) -> bool:
        """处理事件，返回是否处理成功"""
        pass

    def can_handle(self, event: Event) -> bool:
        """检查是否能处理此事件"""
        if not self.enabled:
            return False
        if not self.event_types:
            return True
        return event.type in self.event_types

    def filter(self, event: Event) -> bool:
        """过滤事件，返回是否应该处理"""
        return self.can_handle(event)


@dataclass
class EventSubscription:
    """事件订阅"""
    handler: EventHandler
    event_types: Set[EventType]
    async_handler: bool = False
    filter_func: Optional[Callable[[Event], bool]] = None


class EventBus:
    """事件总线"""

    def __init__(self, max_history: int = 1000):
        self._subscriptions: List[EventSubscription] = []
        self._history: deque = deque(maxlen=max_history)
        self._lock = threading.RLock()
        self._async_mode = False
        self._event_queue: asyncio.Queue = None
        self._processing = False

    def subscribe(self, handler: EventHandler, async_handler: bool = False) -> None:
        """订阅事件"""
        with self._lock:
            subscription = EventSubscription(
                handler=handler,
                event_types=handler.event_types.copy(),
                async_handler=async_handler
            )
            self._subscriptions.append(subscription)

    def unsubscribe(self, handler_name: str) -> None:
        """取消订阅"""
        with self._lock:
            self._subscriptions = [
                s for s in self._subscriptions
                if s.handler.name != handler_name
            ]

    def publish(self, event: Event) -> None:
        """发布事件（同步）"""
        with self._lock:
            self._history.append(event)

        for subscription in self._subscriptions:
            if subscription.handler.filter(event):
                try:
                    if subscription.async_handler:
                        asyncio.create_task(self._async_handle(subscription, event))
                    else:
                        subscription.handler.handle(event)
                except Exception as e:
                    print(f"Handler {subscription.handler.name} failed: {e}")

    async def publish_async(self, event: Event) -> None:
        """发布事件（异步）"""
        self.publish(event)

    async def _async_handle(self, subscription: EventSubscription, event: Event) -> None:
        """异步处理"""
        try:
            if asyncio.iscoroutinefunction(subscription.handler.handle):
                await subscription.handler.handle(event)
            else:
                subscription.handler.handle(event)
        except Exception as e:
            print(f"Async handler {subscription.handler.name} failed: {e}")

    def get_history(
        self,
        event_type: EventType = None,
        limit: int = 100
    ) -> List[Event]:
        """获取事件历史"""
        with self._lock:
            history = list(self._history)

        if event_type:
            history = [e for e in history if e.type == event_type]

        return history[-limit:]

    def clear_history(self) -> None:
        """清除事件历史"""
        with self._lock:
            self._history.clear()

    def start_async_processing(self) -> None:
        """启动异步事件处理"""
        if self._async_mode:
            return

        self._async_mode = True
        self._event_queue = asyncio.Queue()
        asyncio.create_task(self._process_events())

    async def _process_events(self) -> None:
        """事件处理循环"""
        self._processing = True
        while self._processing:
            try:
                event = await asyncio.wait_for(self._event_queue.get(), timeout=1.0)
                self.publish(event)
            except asyncio.TimeoutError:
                continue
            except Exception as e:
                print(f"Event processing error: {e}")

    def stop_async_processing(self) -> None:
        """停止异步事件处理"""
        self._processing = False

    def get_statistics(self) -> Dict[str, Any]:
        """获取事件统计"""
        with self._lock:
            stats = {
                "total_events": len(self._history),
                "subscriptions": len(self._subscriptions),
                "handlers": [s.handler.name for s in self._subscriptions],
                "by_type": defaultdict(int)
            }

            for event in self._history:
                stats["by_type"][event.type.value] += 1

            return dict(stats)


class TelemetryEventHandler(EventHandler):
    """遥测事件处理器"""

    def __init__(self):
        super().__init__("telemetry", [EventType.METRIC_RECORDED, EventType.ERROR_OCCURRED])
        self._metrics: Dict[str, List[float]] = defaultdict(list)
        self._errors: List[Event] = []

    def handle(self, event: Event) -> bool:
        if event.type == EventType.METRIC_RECORDED:
            metric_name = event.data.get("name")
            value = event.data.get("value")
            if metric_name and value is not None:
                self._metrics[metric_name].append(value)
        elif event.type == EventType.ERROR_OCCURRED:
            self._errors.append(event)
        return True

    def get_metrics(self) -> Dict[str, Dict[str, float]]:
        """获取指标统计"""
        result = {}
        for name, values in self._metrics.items():
            if values:
                result[name] = {
                    "count": len(values),
                    "sum": sum(values),
                    "avg": sum(values) / len(values),
                    "min": min(values),
                    "max": max(values)
                }
        return result


_global_event_bus: Optional[EventBus] = None


def get_event_bus() -> EventBus:
    """获取全局事件总线"""
    global _global_event_bus
    if _global_event_bus is None:
        _global_event_bus = EventBus()
    return _global_event_bus
