# AgentOS Python SDK - OpenTelemetry 集成
# Version: 3.0.0
# Last updated: 2026-04-05
#
# OpenTelemetry 可观测性集成
# 提供：分布式追踪、指标收集、日志关联
# 遵循 ARCHITECTURAL_PRINCIPLES.md E-2（可观测性）

import time
import contextlib
from typing import Any, Dict, Optional, Callable
from functools import wraps
import logging

logger = logging.getLogger(__name__)


class TelemetryConfig:
    """
    遥测配置
    
    Attributes:
        service_name: 服务名称
        service_version: 服务版本
        enabled: 是否启用
        export_endpoint: 导出端点
        sample_rate: 采样率 (0.0-1.0)
    """
    
    def __init__(
        self,
        service_name: str = "agentos-toolkit",
        service_version: str = "3.0.0",
        enabled: bool = True,
        export_endpoint: Optional[str] = None,
        sample_rate: float = 1.0
    ):
        self.service_name = service_name
        self.service_version = service_version
        self.enabled = enabled
        self.export_endpoint = export_endpoint
        self.sample_rate = sample_rate


class Span:
    """
    追踪 Span
    
    表示一个操作或请求的生命周期
    
    Attributes:
        name: Span 名称
        kind: Span 类型
        start_time: 开始时间
        end_time: 结束时间
        attributes: 属性
        events: 事件列表
        status: 状态
    """
    
    def __init__(
        self,
        name: str,
        kind: str = "internal",
        parent: Optional['Span'] = None
    ):
        self.name = name
        self.kind = kind
        self.parent = parent
        self.start_time: Optional[float] = None
        self.end_time: Optional[float] = None
        self.attributes: Dict[str, Any] = {}
        self.events: list = []
        self.status: str = "unset"
        self.span_id: str = f"span-{time.time_ns()}"
        self.trace_id: str = parent.trace_id if parent else f"trace-{time.time_ns()}"
    
    def start(self):
        """启动 Span"""
        self.start_time = time.time()
        self.add_event("span_started")
        return self
    
    def end(self):
        """结束 Span"""
        self.end_time = time.time()
        self.add_event("span_ended")
        self.status = "ok"
    
    def set_attribute(self, key: str, value: Any):
        """
        设置属性
        
        Args:
            key: 属性键
            value: 属性值
        """
        self.attributes[key] = value
    
    def add_event(self, name: str, attributes: Optional[Dict] = None):
        """
        添加事件
        
        Args:
            name: 事件名称
            attributes: 事件属性
        """
        self.events.append({
            "name": name,
            "timestamp": time.time(),
            "attributes": attributes or {}
        })
    
    def set_status(self, status: str):
        """
        设置状态
        
        Args:
            status: 状态 (ok/error/unset)
        """
        self.status = status
    
    def record_exception(self, exception: Exception):
        """
        记录异常
        
        Args:
            exception: 异常对象
        """
        self.set_attribute("error.type", type(exception).__name__)
        self.set_attribute("error.message", str(exception))
        self.set_status("error")
        self.add_event("exception", {
            "type": type(exception).__name__,
            "message": str(exception)
        })
    
    def to_dict(self) -> Dict[str, Any]:
        """转换为字典"""
        return {
            "span_id": self.span_id,
            "trace_id": self.trace_id,
            "name": self.name,
            "kind": self.kind,
            "start_time": self.start_time,
            "end_time": self.end_time,
            "duration_ms": (self.end_time - self.start_time) * 1000 if self.end_time and self.start_time else None,
            "attributes": self.attributes,
            "events": self.events,
            "status": self.status,
            "parent_span_id": self.parent.span_id if self.parent else None
        }


class Tracer:
    """
    追踪器
    
    创建和管理 Span
    
    Example:
        >>> tracer = Tracer()
        >>> with tracer.start_span("operation") as span:
        ...     # 执行业务逻辑
        ...     span.set_attribute("key", "value")
    """
    
    def __init__(self, config: TelemetryConfig):
        self.config = config
        self.active_span: Optional[Span] = None
        self.exported_spans: list = []
    
    @contextlib.contextmanager
    def start_span(
        self,
        name: str,
        kind: str = "internal",
        parent: Optional[Span] = None
    ):
        """
        创建并启动 Span
        
        Args:
            name: Span 名称
            kind: Span 类型
            parent: 父 Span
        
        Yields:
            Span: Span 对象
        
        Example:
            >>> tracer = Tracer(config)
            >>> with tracer.start_span("task_submit") as span:
            ...     result = submit_task()
            ...     span.set_attribute("task_id", result.id)
        """
        if not self.config.enabled:
            yield None
            return
        
        span = Span(name, kind, parent or self.active_span)
        old_active = self.active_span
        self.active_span = span
        
        try:
            span.start()
            yield span
        except Exception as e:
            span.record_exception(e)
            raise
        finally:
            span.end()
            self.active_span = old_active
            self.export_span(span)
    
    def export_span(self, span: Span):
        """
        导出 Span
        
        Args:
            span: Span 对象
        """
        if not self.config.enabled:
            return
        
        self.exported_spans.append(span.to_dict())
        
        # 实际应用中这里会发送到 OpenTelemetry Collector
        logger.debug(f"Export span: {span.name} ({span.span_id})")
    
    def get_exported_spans(self) -> list:
        """获取已导出的 Span"""
        return self.exported_spans
    
    def clear_spans(self):
        """清除 Span"""
        self.exported_spans.clear()


class Metrics:
    """
    指标收集器
    
    收集和导出性能指标
    
    Example:
        >>> metrics = Metrics()
        >>> metrics.record("task_latency", 150.5)
        >>> stats = metrics.get_stats("task_latency")
    """
    
    def __init__(self):
        self.counters: Dict[str, int] = {}
        self.gauges: Dict[str, float] = {}
        self.histograms: Dict[str, list] = {}
    
    def increment_counter(self, name: str, value: int = 1):
        """
        增加计数器
        
        Args:
            name: 计数器名称
            value: 增量
        """
        if name not in self.counters:
            self.counters[name] = 0
        self.counters[name] += value
    
    def set_gauge(self, name: str, value: float):
        """
        设置仪表值
        
        Args:
            name: 仪表名称
            value: 值
        """
        self.gauges[name] = value
    
    def record_histogram(self, name: str, value: float):
        """
        记录直方图值
        
        Args:
            name: 直方图名称
            value: 观测值
        """
        if name not in self.histograms:
            self.histograms[name] = []
        self.histograms[name].append(value)
    
    def get_stats(self, name: str) -> Dict[str, Any]:
        """
        获取统计信息
        
        Args:
            name: 指标名称
        
        Returns:
            dict: 统计信息
        """
        if name in self.histograms:
            import statistics
            data = self.histograms[name]
            return {
                "count": len(data),
                "min": min(data),
                "max": max(data),
                "avg": statistics.mean(data),
                "median": statistics.median(data)
            }
        elif name in self.counters:
            return {"value": self.counters[name]}
        elif name in self.gauges:
            return {"value": self.gauges[name]}
        return {}
    
    def export_metrics(self) -> Dict[str, Any]:
        """导出所有指标"""
        return {
            "counters": self.counters.copy(),
            "gauges": self.gauges.copy(),
            "histograms": {
                name: self.get_stats(name)
                for name in self.histograms
            }
        }


class TelemetryManager:
    """
    遥测管理器（统一管理）
    
    整合 Tracer 和 Metrics
    
    Example:
        >>> telemetry = TelemetryManager()
        >>> 
        >>> # 追踪操作
        >>> @telemetry.trace("submit_task")
        >>> def submit_task(content):
        ...     # 业务逻辑
        ...     pass
        >>> 
        >>> # 记录指标
        >>> telemetry.metrics.increment_counter("tasks_submitted")
    """
    
    def __init__(self, config: Optional[TelemetryConfig] = None):
        self.config = config or TelemetryConfig()
        self.tracer = Tracer(self.config)
        self.metrics = Metrics()
        
        # 设置全局日志处理器
        self._setup_logging()
    
    def trace(self, operation_name: str):
        """
        装饰器：追踪函数调用
        
        Args:
            operation_name: 操作名称
        
        Returns:
            decorator: 装饰器函数
        
        Example:
            >>> @telemetry.trace("memory_write")
            >>> def write_memory(content, level="L1"):
            ...     return memory_mgr.write(content, level)
        """
        def decorator(func: Callable):
            @wraps(func)
            def wrapper(*args, **kwargs):
                with self.tracer.start_span(f"{func.__name__}") as span:
                    if span:
                        span.set_attribute("function", func.__name__)
                        span.set_attribute("args", str(args)[:200])
                        span.set_attribute("kwargs", str(kwargs)[:200])
                    
                    try:
                        result = func(*args, **kwargs)
                        if span:
                            span.set_attribute("success", True)
                        return result
                    except Exception as e:
                        if span:
                            span.record_exception(e)
                        raise
            
            return wrapper
        return decorator
    
    def _setup_logging(self):
        """设置日志处理"""
        class TelemetryHandler(logging.Handler):
            def __init__(self, telemetry_manager):
                super().__init__()
                self.telemetry = telemetry_manager
            
            def emit(self, record):
                log_entry = self.format(record)
                self.telemetry.metrics.increment_counter(f"log_{record.levelname.lower()}")
        
        handler = TelemetryHandler(self)
        handler.setLevel(logging.INFO)
        logging.getLogger("agentos").addHandler(handler)
    
    def get_health_status(self) -> Dict[str, Any]:
        """
        获取健康状态
        
        Returns:
            dict: 健康状态
        """
        return {
            "status": "healthy",
            "telemetry_enabled": self.config.enabled,
            "spans_exported": len(self.tracer.get_exported_spans()),
            "metrics": self.metrics.export_metrics()
        }
    
    def export_all(self) -> Dict[str, Any]:
        """
        导出所有遥测数据
        
        Returns:
            dict: 遥测数据
        """
        return {
            "traces": self.tracer.get_exported_spans(),
            "metrics": self.metrics.export_metrics(),
            "timestamp": time.time()
        }


# 全局遥测实例
_telemetry_instance: Optional[TelemetryManager] = None


def get_telemetry() -> TelemetryManager:
    """获取全局遥测实例"""
    global _telemetry_instance
    if _telemetry_instance is None:
        _telemetry_instance = TelemetryManager()
    return _telemetry_instance


def init_telemetry(config: Optional[TelemetryConfig] = None):
    """
    初始化遥测
    
    Args:
        config: 遥测配置
    """
    global _telemetry_instance
    _telemetry_instance = TelemetryManager(config or TelemetryConfig())
    return _telemetry_instance
