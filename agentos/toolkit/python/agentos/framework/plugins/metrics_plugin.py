# AgentOS 示例插件 - 指标采集器
# Version: 1.0.0

"""
MetricsPlugin 示例插件

提供指标采集、聚合和查询功能，演示PluginSDK的高级用法。
"""

import time
from typing import Any, Dict, List, Optional

from agentos.framework.plugin import BasePlugin


class MetricsPlugin(BasePlugin):
    """指标采集器插件 - 提供计数器、计时器和直方图"""

    __version__ = "1.0.0"

    def __init__(self):
        super().__init__()
        self._counters: Dict[str, float] = {}
        self._gauges: Dict[str, float] = {}
        self._timers: Dict[str, List[float]] = {}

    async def on_load(self, context: Dict[str, Any]) -> None:
        pass

    async def on_activate(self, context: Dict[str, Any]) -> None:
        self._counters = {}
        self._gauges = {}
        self._timers = {}

    async def on_deactivate(self) -> None:
        pass

    async def on_unload(self) -> None:
        self._counters = {}
        self._gauges = {}
        self._timers = {}

    async def on_error(self, error: Exception) -> None:
        pass

    def get_capabilities(self) -> List[str]:
        return ["metrics", "counters", "gauges", "timers"]

    def increment(self, name: str, value: float = 1.0) -> float:
        """递增计数器"""
        self._counters[name] = self._counters.get(name, 0.0) + value
        return self._counters[name]

    def decrement(self, name: str, value: float = 1.0) -> float:
        """递减计数器"""
        self._counters[name] = self._counters.get(name, 0.0) - value
        return self._counters[name]

    def set_gauge(self, name: str, value: float) -> float:
        """设置仪表值"""
        self._gauges[name] = value
        return value

    def get_gauge(self, name: str) -> Optional[float]:
        """获取仪表值"""
        return self._gauges.get(name)

    def record_timing(self, name: str, duration_ms: float) -> None:
        """记录计时"""
        if name not in self._timers:
            self._timers[name] = []
        self._timers[name].append(duration_ms)

    def start_timer(self, name: str) -> float:
        """开始计时，返回开始时间戳"""
        return time.time()

    def stop_timer(self, name: str, start_time: float) -> float:
        """停止计时并记录"""
        duration_ms = (time.time() - start_time) * 1000
        self.record_timing(name, duration_ms)
        return duration_ms

    def get_counter(self, name: str) -> float:
        """获取计数器值"""
        return self._counters.get(name, 0.0)

    def get_timer_stats(self, name: str) -> Dict[str, float]:
        """获取计时器统计"""
        values = self._timers.get(name, [])
        if not values:
            return {"count": 0, "min": 0, "max": 0, "avg": 0, "p95": 0}
        sorted_values = sorted(values)
        p95_index = int(len(sorted_values) * 0.95)
        return {
            "count": len(sorted_values),
            "min": sorted_values[0],
            "max": sorted_values[-1],
            "avg": sum(sorted_values) / len(sorted_values),
            "p95": sorted_values[min(p95_index, len(sorted_values) - 1)],
        }

    def get_all_metrics(self) -> Dict[str, Any]:
        """获取所有指标"""
        return {
            "counters": dict(self._counters),
            "gauges": dict(self._gauges),
            "timers": {k: self.get_timer_stats(k) for k in self._timers},
        }
