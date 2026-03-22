# AgentOS Python SDK - Telemetry Layer
# Version: 2.0.0.0

"""
Telemetry and observability utilities.

This package provides:
    - Metrics collection (Meter, MetricPoint)
    - Distributed tracing (Tracer, Span)
    - Logging setup utilities
"""

try:
    from .metrics import Meter, MetricPoint
    from .tracing import Tracer, Span, SpanStatus
    from .logging import setup_logging

    __all__ = [
        "Meter",
        "MetricPoint",
        "Tracer",
        "Span",
        "SpanStatus",
        "setup_logging",
    ]
except ImportError:
    __all__ = []
