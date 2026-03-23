# AgentOS Python SDK Telemetry
# Version: 1.0.0.7
# Last updated: 2026-03-21

"""
Telemetry module for observability and monitoring.

This module provides classes for collecting metrics, traces, and logs
with enhanced precision timing, multi-dimensional labels, and runtime health monitoring.

Example:
    >>> from agentos import Telemetry, SpanStatus
    >>> telemetry = Telemetry(service_name="my_service")
    >>> 
    >>> # Record metrics with labels
    >>> telemetry.record_metric("request_count", 1, labels={"method": "POST", "status": "200"})
    >>> 
    >>> # Create trace span
    >>> with telemetry.span("process_request") as span:
    ...     process_data()
    ...     span.set_attribute("data_size", 1024)
    >>> 
    >>> # Export data
    >>> export_data = telemetry.export_all()
"""

import time
import json
import os
import threading
import logging
from typing import Optional, Dict, Any, List, Callable
from dataclasses import dataclass, field, asdict
from enum import Enum
from functools import wraps
from contextlib import contextmanager
import timeit

from .exceptions import TelemetryError

logger = logging.getLogger(__name__)


class SpanStatus(Enum):
    """Status of a trace span."""
    OK = "ok"
    ERROR = "error"
    UNSET = "unset"


@dataclass
class MetricPoint:
    """A single metric data point with multi-dimensional labels.

    Attributes:
        name: Metric name
        value: Metric value (float for counters and gauges)
        timestamp: Unix timestamp in seconds (high precision)
        labels: Multi-dimensional labels for slicing/dicing
        service_name: Service that recorded this metric
        method_name: Optional method/function name
        status_code: Optional HTTP/gRPC status code

    Example:
        >>> point = MetricPoint(
        ...     name="request_latency_ms",
        ...     value=150.5,
        ...     labels={"endpoint": "/api/users", "method": "GET"},
        ...     status_code=200
        ... )
    """
    name: str
    value: float
    timestamp: float = field(default_factory=lambda: time.time())
    labels: Optional[Dict[str, str]] = None
    service_name: Optional[str] = None
    method_name: Optional[str] = None
    status_code: Optional[int] = None

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary with full context."""
        return {
            "name": self.name,
            "value": self.value,
            "timestamp": self.timestamp,
            "labels": self.labels or {},
            "service_name": self.service_name,
            "method_name": self.method_name,
            "status_code": self.status_code
        }

    def __post_init__(self):
        """Validate metric point."""
        if not self.name or not isinstance(self.name, str):
            raise ValueError("Metric name must be a non-empty string")
        if not isinstance(self.value, (int, float)):
            raise ValueError("Metric value must be numeric")


@dataclass
class Span:
    """A trace span representing an operation with high-precision timing.

    Attributes:
        trace_id: Unique trace identifier
        span_id: Unique span identifier
        name: Span name (operation/method)
        start_time: Start timestamp (seconds since epoch)
        end_time: End timestamp (None if not ended)
        status: Span status (OK/ERROR/UNSET)
        attributes: Custom key-value pairs
        parent_span_id: Parent span ID for nested traces
        duration_ns: Duration in nanoseconds (high precision)

    Example:
        >>> span = Span(trace_id="abc123", span_id="span456", name="process_data")
        >>> span.start()
        >>> # ... do work ...
        >>> span.end()
        >>> print(f"Duration: {span.duration_ns / 1e6:.2f} ms")
    """
    trace_id: str
    span_id: str
    name: str
    start_time: float
    end_time: Optional[float] = None
    status: SpanStatus = SpanStatus.UNSET
    attributes: Optional[Dict[str, Any]] = None
    parent_span_id: Optional[str] = None
    duration_ns: Optional[int] = None  # Nanosecond precision

    def __post_init__(self):
        if self.attributes is None:
            self.attributes = {}

    def set_attribute(self, key: str, value: Any):
        """Set a span attribute."""
        self.attributes[key] = value

    def start(self):
        """Start the span with high-precision timing."""
        self.start_time = time.time()
        # Use timeit.default_timer for high-precision duration measurement
        self._start_perf_counter = timeit.default_timer()

    def end(self):
        """End the span and calculate duration."""
        self.end_time = time.time()
        # Calculate duration in nanoseconds using perf_counter
        end_perf = timeit.default_timer()
        self.duration_ns = int((end_perf - self._start_perf_counter) * 1e9)

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary with full timing information."""
        return {
            "trace_id": self.trace_id,
            "span_id": self.span_id,
            "name": self.name,
            "start_time": self.start_time,
            "end_time": self.end_time,
            "status": self.status.value,
            "attributes": self.attributes,
            "parent_span_id": self.parent_span_id,
            "duration_ns": self.duration_ns,
            "duration_ms": self.duration_ns / 1e6 if self.duration_ns else None
        }


class Meter:
    """Metrics collector for recording measurements with multi-dimensional labels.

    Attributes:
        service_name: Name of the service
        metrics: List of recorded metric points
        _lock: Thread lock for thread-safe recording

    Example:
        >>> meter = Meter(service_name="api_gateway")
        >>> meter.record("request_count", 1, labels={"method": "POST", "status": "200"})
        >>> meter.record("latency_ms", 150.5, labels={"endpoint": "/users"})
    """

    def __init__(self, service_name: str):
        """
        Initialize a meter.

        Args:
            service_name: Name of the service
        """
        self.service_name = service_name
        self.metrics: List[MetricPoint] = []
        self._lock = threading.Lock()

    def record(self, name: str, value: float, labels: Optional[Dict[str, str]] = None,
               method_name: Optional[str] = None, status_code: Optional[int] = None):
        """
        Record a metric value with multi-dimensional labels.

        Args:
            name: Metric name
            value: Metric value
            labels: Optional labels for slicing/dicing
            method_name: Optional method/function name
            status_code: Optional HTTP/gRPC status code

        Example:
            >>> meter.record(
            ...     name="request_duration_ms",
            ...     value=125.3,
            ...     labels={"endpoint": "/api/users", "method": "GET"},
            ...     status_code=200
            ... )
        """
        point = MetricPoint(
            name=name,
            value=value,
            labels=labels or {},
            service_name=self.service_name,
            method_name=method_name,
            status_code=status_code
        )

        with self._lock:
            self.metrics.append(point)

        logger.debug(
            f"Recorded metric: {name}={value} (service={self.service_name})")

    def get_metrics(self) -> List[MetricPoint]:
        """Get all recorded metrics."""
        with self._lock:
            return list(self.metrics)

    def clear(self):
        """Clear all metrics."""
        with self._lock:
            self.metrics.clear()

    def export(self) -> str:
        """
        Export metrics as JSON.

        Returns:
            JSON string of metrics with full context
        """
        data = {
            "service_name": self.service_name,
            "recorded_at": time.time(),
            "metric_count": len(self.metrics),
            "metrics": [m.to_dict() for m in self.metrics]
        }
        return json.dumps(data, indent=2)


class Tracer:
    """Tracer for creating and managing spans."""

    def __init__(self, service_name: str):
        """
        Initialize a tracer.

        Args:
            service_name: Name of the service
        """
        self.service_name = service_name
        self.spans: List[Span] = []

    def start_span(
        self,
        name: str,
        trace_id: Optional[str] = None,
        parent_span_id: Optional[str] = None
    ) -> Span:
        """
        Start a new span.

        Args:
            name: Span name
            trace_id: Trace ID (auto-generated if not provided)
            parent_span_id: Parent span ID

        Returns:
            The created span
        """
        import uuid

        if trace_id is None:
            trace_id = str(uuid.uuid4())

        span_id = str(uuid.uuid4())
        span = Span(
            trace_id=trace_id,
            span_id=span_id,
            name=name,
            start_time=time.time(),
            parent_span_id=parent_span_id
        )
        span.start()
        self.spans.append(span)
        return span

    def end_span(self, span: Span, status: SpanStatus = SpanStatus.OK):
        """
        End a span.

        Args:
            span: The span to end
            status: Span status
        """
        span.end()
        span.status = status

    def get_spans(self) -> List[Span]:
        """Get all recorded spans."""
        return self.spans

    def clear(self):
        """Clear all spans."""
        self.spans.clear()

    def export(self) -> str:
        """
        Export traces as JSON.

        Returns:
            JSON string of traces
        """
        data = {
            "service_name": self.service_name,
            "traces": [s.to_dict() for s in self.spans]
        }
        return json.dumps(data)


class Telemetry:
    """Main telemetry coordinator combining metrics and traces.

    This class provides comprehensive observability with:
    - Multi-dimensional metrics collection
    - High-precision distributed tracing
    - Runtime health monitoring (memory, GC, CPU)
    - Thread-safe operations

    Attributes:
        service_name: Name of the service
        meter: Metrics collector
        tracer: Distributed tracer
        _runtime_monitoring: Whether runtime monitoring is enabled
        _monitor_thread: Background monitoring thread (if enabled)

    Example:
        >>> telemetry = Telemetry(service_name="my_app", enable_runtime_monitoring=True)
        >>> 
        >>> # Record custom metrics
        >>> telemetry.record_metric("business_events", 1, labels={"type": "order"})
        >>> 
        >>> # Use context manager for spans
        >>> with telemetry.span("process_order") as span:
        ...     process_order()
        ...     span.set_attribute("order_id", "12345")
        >>> 
        >>> # Get runtime health
        >>> health = telemetry.get_runtime_health()
        >>> print(f"Memory: {health['memory_mb']:.1f} MB, GC count: {health['gc_count']}")
    """

    def __init__(self, service_name: str, enable_runtime_monitoring: bool = False):
        """
        Initialize telemetry.

        Args:
            service_name: Name of the service
            enable_runtime_monitoring: Enable automatic runtime metrics collection

        Example:
            >>> telemetry = Telemetry("api_gateway", enable_runtime_monitoring=True)
        """
        self.service_name = service_name
        self.meter = Meter(service_name)
        self.tracer = Tracer(service_name)
        self._runtime_monitoring = enable_runtime_monitoring
        self._monitor_thread: Optional[threading.Thread] = None
        self._stop_monitoring = threading.Event()

        if enable_runtime_monitoring:
            self._start_runtime_monitoring()

    def _start_runtime_monitoring(self):
        """Start background runtime health monitoring."""
        self._runtime_metrics: Dict[str, Any] = {}
        self._monitor_thread = threading.Thread(
            target=self._runtime_monitor_loop,
            daemon=True,
            name="TelemetryRuntimeMonitor"
        )
        self._monitor_thread.start()
        logger.info("Runtime health monitoring started")

    def _runtime_monitor_loop(self):
        """Background loop to collect runtime metrics."""
        import gc

        while not self._stop_monitoring.is_set():
            try:
                # Collect Python runtime metrics
                import sys
                self._runtime_metrics = {
                    "timestamp": time.time(),
                    "memory_mb": self._get_memory_usage_mb(),
                    "gc_count": len(gc.get_count()),
                    "gc_objects": len(gc.get_objects()),
                    "thread_count": threading.active_count(),
                }

                # Record as metrics
                self.meter.record("runtime.memory_mb",
                                  self._runtime_metrics["memory_mb"])
                self.meter.record("runtime.gc_objects",
                                  self._runtime_metrics["gc_objects"])
                self.meter.record("runtime.thread_count",
                                  self._runtime_metrics["thread_count"])

                logger.debug(
                    f"Runtime metrics collected: {self._runtime_metrics}")

            except Exception as e:
                logger.error(f"Error collecting runtime metrics: {e}")

            # Collect every 5 seconds
            self._stop_monitoring.wait(timeout=5.0)

    @staticmethod
    def _get_memory_usage_mb() -> float:
        """Get current memory usage in MB."""
        try:
            import resource
            # Get resident set size in KB, convert to MB
            return resource.getrusage(resource.RUSAGE_SELF).ru_maxrss / 1024.0
        except ImportError:
            # Fallback for Windows
            import psutil
            process = psutil.Process(os.getpid())
            return process.memory_info().rss / (1024.0 * 1024.0)

    def get_runtime_health(self) -> Dict[str, Any]:
        """
        Get current runtime health metrics.

        Returns:
            Dictionary with memory, GC, and thread information

        Example:
            >>> health = telemetry.get_runtime_health()
            >>> print(f"Memory: {health['memory_mb']:.1f} MB")
        """
        if not self._runtime_monitoring:
            return {
                "enabled": False,
                "message": "Runtime monitoring is disabled"
            }

        return {
            "enabled": True,
            **getattr(self, '_runtime_metrics', {})
        }

    def record_metric(self, name: str, value: float, labels: Optional[Dict[str, str]] = None,
                      method_name: Optional[str] = None, status_code: Optional[int] = None):
        """
        Record a metric with multi-dimensional labels.

        Args:
            name: Metric name
            value: Metric value
            labels: Optional labels
            method_name: Optional method name
            status_code: Optional status code
        """
        self.meter.record(name, value, labels, method_name, status_code)

    def start_span(self, name: str, **kwargs) -> Span:
        """
        Start a trace span.

        Args:
            name: Span name
            **kwargs: Additional arguments (trace_id, parent_span_id)

        Returns:
            The created span (already started)

        Example:
            >>> span = telemetry.start_span("process_data")
            >>> try:
            ...     do_work()
            ...     telemetry.end_span(span, SpanStatus.OK)
            ... except Exception as e:
            ...     telemetry.end_span(span, SpanStatus.ERROR)
        """
        span = self.tracer.start_span(name, **kwargs)
        span.start()  # Start high-precision timing
        return span

    def end_span(self, span: Span, status: SpanStatus = SpanStatus.OK):
        """
        End a trace span.

        Args:
            span: The span to end
            status: Span status (OK/ERROR/UNSET)
        """
        self.tracer.end_span(span, status)

    @contextmanager
    def span(self, name: str, **kwargs):
        """
        Context manager for automatic span lifecycle management.

        Args:
            name: Span name
            **kwargs: Additional arguments

        Yields:
            Span object with active timing

        Example:
            >>> with telemetry.span("database_query") as span:
            ...     result = db.query(sql)
            ...     span.set_attribute("query_length", len(sql))
        """
        span = self.start_span(name, **kwargs)
        try:
            yield span
            self.end_span(span, SpanStatus.OK)
        except Exception as e:
            span.set_attribute("error.message", str(e))
            span.set_attribute("error.type", type(e).__name__)
            self.end_span(span, SpanStatus.ERROR)
            raise

    def timed(self, metric_name: str, labels: Optional[Dict[str, str]] = None):
        """
        Decorator to time function execution and record as metric.

        Args:
            metric_name: Metric name for timing
            labels: Optional labels

        Returns:
            Decorated function

        Example:
            >>> @telemetry.timed("function_duration_ms", labels={"function": "process"})
            ... def process_data():
            ...     time.sleep(0.1)
        """
        def decorator(func: Callable):
            @wraps(func)
            def wrapper(*args, **kwargs):
                start_time = timeit.default_timer()
                try:
                    result = func(*args, **kwargs)
                    self.meter.record(
                        f"{metric_name}.success",
                        1,
                        labels=labels,
                        method_name=func.__qualname__,
                        status_code=200
                    )
                    return result
                except Exception as e:
                    self.meter.record(
                        f"{metric_name}.error",
                        1,
                        labels=labels,
                        method_name=func.__qualname__,
                        status_code=500
                    )
                    raise
                finally:
                    duration_ms = (timeit.default_timer() - start_time) * 1000
                    self.meter.record(
                        metric_name,
                        duration_ms,
                        labels=labels,
                        method_name=func.__qualname__
                    )
            return wrapper
        return decorator

    def export_all(self) -> Dict[str, str]:
        """
        Export all telemetry data.

        Returns:
            Dictionary with metrics and traces JSON
        """
        return {
            "metrics": self.meter.export(),
            "traces": self.tracer.export()
        }
