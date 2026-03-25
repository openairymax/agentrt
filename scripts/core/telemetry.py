#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS 遥测模块
# 性能指标收集和监控系统

"""
AgentOS 遥测系统

提供全面的性能监控和指标收集，包括：
- 实时指标采集
- 性能基准追踪
- 历史趋势分析
- Prometheus 兼容格式
"""

import time
from collections import defaultdict
from dataclasses import dataclass, field
from datetime import datetime
from enum import Enum
from typing import Any, Callable, Dict, List, Optional


class MetricType(Enum):
    """指标类型"""
    COUNTER = "counter"
    GAUGE = "gauge"
    HISTOGRAM = "histogram"
    SUMMARY = "summary"


@dataclass
class Metric:
    """指标数据"""
    name: str
    value: float
    metric_type: MetricType
    labels: Dict[str, str] = field(default_factory=dict)
    timestamp: str = field(default_factory=lambda: datetime.now().isoformat())
    description: str = ""


class MetricsCollector:
    """指标收集器"""

    def __init__(self):
        self._counters: Dict[str, float] = defaultdict(float)
        self._gauges: Dict[str, float] = {}
        self._histograms: Dict[str, List[float]] = defaultdict(list)
        self._callbacks: List[Callable[[Metric], None]] = []

    def counter(self, name: str, value: float = 1, labels: Dict[str, str] = None) -> None:
        """增加计数器"""
        self._counters[name] += value
        metric = Metric(
            name=name,
            value=self._counters[name],
            metric_type=MetricType.COUNTER,
            labels=labels or {}
        )
        self._notify(metric)

    def gauge(self, name: str, value: float, labels: Dict[str, str] = None) -> None:
        """设置仪表值"""
        self._gauges[name] = value
        metric = Metric(
            name=name,
            value=value,
            metric_type=MetricType.GAUGE,
            labels=labels or {}
        )
        self._notify(metric)

    def histogram(self, name: str, value: float, labels: Dict[str, str] = None) -> None:
        """记录直方图值"""
        self._histograms[name].append(value)
        metric = Metric(
            name=name,
            value=value,
            metric_type=MetricType.HISTOGRAM,
            labels=labels or {}
        )
        self._notify(metric)

    def timing(self, name: str, duration_ms: float, labels: Dict[str, str] = None) -> None:
        """记录时间"""
        self.histogram(f"{name}.duration_ms", duration_ms, labels)

    def register_callback(self, callback: Callable[[Metric], None]) -> None:
        """注册指标回调"""
        self._callbacks.append(callback)

    def _notify(self, metric: Metric) -> None:
        """通知所有回调"""
        for callback in self._callbacks:
            try:
                callback(metric)
            except Exception:
                pass

    def get_metrics(self) -> List[Metric]:
        """获取所有指标"""
        metrics = []

        for name, value in self._counters.items():
            metrics.append(Metric(name=name, value=value, metric_type=MetricType.COUNTER))

        for name, value in self._gauges.items():
            metrics.append(Metric(name=name, value=value, metric_type=MetricType.GAUGE))

        for name, values in self._histograms.items():
            for value in values:
                metrics.append(Metric(name=name, value=value, metric_type=MetricType.HISTOGRAM))

        return metrics

    def export_prometheus(self) -> str:
        """导出 Prometheus 格式"""
        lines = []

        for name, value in self._counters.items():
            lines.append(f"# TYPE {name} counter")
            lines.append(f"{name} {value}")

        for name, value in self._gauges.items():
            lines.append(f"# TYPE {name} gauge")
            lines.append(f"{name} {value}")

        for name, values in self._histograms.items():
            lines.append(f"# TYPE {name} histogram")
            lines.append(f"{name}_sum {sum(values)}")
            lines.append(f"{name}_count {len(values)}")

        return "\n".join(lines)


class Timer:
    """计时器上下文管理器"""

    def __init__(self, collector: MetricsCollector, name: str, labels: Dict[str, str] = None):
        self.collector = collector
        self.name = name
        self.labels = labels
        self.start_time = None

    def __enter__(self):
        self.start_time = time.perf_counter()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        duration_ms = (time.perf_counter() - self.start_time) * 1000
        self.collector.timing(self.name, duration_ms, self.labels)
        return False


_global_collector: Optional[MetricsCollector] = None


def get_collector() -> MetricsCollector:
    """获取全局指标收集器"""
    global _global_collector
    if _global_collector is None:
        _global_collector = MetricsCollector()
    return _global_collector
