"""
Unit tests for AgentRT Python SDK.

This module contains comprehensive tests for the SDK components.
Run with: pytest tests/test_agent.py -v
"""

import unittest
from unittest.mock import Mock, patch, MagicMock
import json
import time

# Import all available public APIs
from agentrt import (
    AgentRT, AsyncAgentRT,
    Telemetry, Meter, Tracer, Span, SpanStatus,
    TaskStatus, TaskResult, SkillInfo, SkillResult,
    Memory, MemorySearchResult, Session, Skill,
    MemoryLayer, MemoryRecordType, MemoryInfo, SessionStatus, SkillStatus,
    generate_id, generate_timestamp, generate_hash,
    validate_json, sanitize_string,
    get_env_var, parse_timeout, merge_dicts,
    Timer, RateLimiter,
    AgentOSError, TaskError, AgentOSMemoryError, SessionError, SkillError,
    NetworkError, AgentOSTimeoutError, InitializationError, ValidationError, TelemetryError,
    ConfigError, SyscallError, RateLimitError,
)

# 从子模块直接导入（不在 __init__.py 的 __all__ 中）
from agentrt.task import Task
from agentrt.memory import Memory
from agentrt.session import Session
from agentrt.skill import Skill

# 向后兼容别名
AgentRT_MemoryError = AgentOSMemoryError
AgentRT_TimeoutError = AgentOSTimeoutError


class TestTypes(unittest.TestCase):
    """Test type definitions and data structures."""
    
    def test_task_status_enum(self):
        """Test TaskStatus enum values."""
        self.assertEqual(TaskStatus.PENDING.value, "pending")
        self.assertEqual(TaskStatus.RUNNING.value, "running")
        self.assertEqual(TaskStatus.COMPLETED.value, "completed")
        self.assertEqual(TaskStatus.FAILED.value, "failed")
        self.assertEqual(TaskStatus.CANCELLED.value, "cancelled")
    
    def test_task_result(self):
        """Test TaskResult dataclass."""
        result = TaskResult(
            id="task_123",
            status=TaskStatus.COMPLETED,
            output="test output",
            error=""
        )
        
        self.assertEqual(result.id, "task_123")
        self.assertEqual(result.status, TaskStatus.COMPLETED)
        self.assertEqual(result.output, "test output")
        self.assertEqual(result.error, "")
        self.assertTrue(result.status.is_terminal())
    
    def test_memory_info(self):
        """Test MemoryInfo dataclass."""
        info = MemoryInfo(
            record_id="mem_123",
            record_type=MemoryRecordType.RAW,
            data_size=1024
        )
        
        data = info.to_dict()
        self.assertEqual(data["record_type"], "RAW")
        
        info2 = MemoryInfo.from_dict(data)
        self.assertEqual(info2.record_type, MemoryRecordType.RAW)
    
    def test_skill_result(self):
        """Test SkillResult dataclass."""
        result = SkillResult(
            success=True,
            output={"data": "test"},
            error=""
        )
        
        self.assertTrue(result.success)
        self.assertIsNotNone(result.output)


class TestExceptions(unittest.TestCase):
    """Test exception classes."""
    
    def test_agentrt_error(self):
        """Test base AgentOSError."""
        from agentrt.exceptions import CODE_NOT_FOUND
        
        error = AgentOSError(
            message="Test error",
            error_code=CODE_NOT_FOUND,
            cause=None
        )
        
        self.assertEqual(error.error_code, CODE_NOT_FOUND)
        self.assertEqual(error.message, "Test error")
        
        # Test string representation
        error_str = str(error)
        self.assertIn(CODE_NOT_FOUND, error_str)
        self.assertIn("Test error", error_str)
    
    def test_timeout_error(self):
        """Test AgentOSTimeoutError."""
        error = AgentOSTimeoutError(operation="task wait")
        
        self.assertEqual(error.operation, "task wait")
    
    def test_initialization_error(self):
        """Test InitializationError."""
        from agentrt.exceptions import CODE_INTERNAL
        
        error = InitializationError(
            message="Failed to load library",
            cause=RuntimeError("lib not found")
        )
        
        self.assertEqual(error.error_code, CODE_INTERNAL)
        self.assertIn("library", error.message.lower())


class TestUtilities(unittest.TestCase):
    """Test utility functions."""
    
    def test_generate_id(self):
        """Test ID generation."""
        id1 = generate_id()
        id2 = generate_id(prefix="task")

        self.assertIsInstance(id1, str)
        self.assertTrue(len(id1) > 0)
        self.assertTrue(id2.startswith("task_"))

        ids = set()
        for _ in range(100):
            ids.add(generate_id())
        self.assertGreater(len(ids), 90, "generate_id should produce mostly unique IDs")
    
    def test_generate_timestamp(self):
        """Test timestamp generation."""
        ts = generate_timestamp()
        self.assertIsInstance(ts, int)
        self.assertGreater(ts, 0)
    
    def test_generate_hash(self):
        """Test hash generation."""
        hash1 = generate_hash("test data")
        hash2 = generate_hash(b"test data")
        
        self.assertEqual(hash1, hash2)
        self.assertEqual(len(hash1), 64)  # SHA256 hex length
    
    def test_validate_json(self):
        """Test JSON validation."""
        self.assertTrue(validate_json('{"key": "value"}'))
        self.assertTrue(validate_json('[1, 2, 3]'))
        self.assertTrue(validate_json('"string"'))
        self.assertTrue(validate_json('123'))
        self.assertFalse(validate_json('not json'))
    
    def test_sanitize_string(self):
        """Test string sanitization."""
        # Test control character removal
        dirty = "test\x00data\x01with\x02control"
        clean = sanitize_string(dirty)
        self.assertNotIn("\x00", clean)
        
        # Test whitespace trimming
        padded = "  hello world  "
        sanitized = sanitize_string(padded)
        self.assertEqual(sanitized, "hello world")
    
    def test_parse_timeout(self):
        """Test timeout parsing."""
        self.assertEqual(parse_timeout(30), 30)
        self.assertEqual(parse_timeout(30.5), 30)
        self.assertEqual(parse_timeout("30s"), 30)
        self.assertEqual(parse_timeout("5m"), 300)
        self.assertEqual(parse_timeout("1h"), 3600)
        self.assertEqual(parse_timeout("invalid", default=10), 10)
    
    def test_merge_dicts(self):
        """Test dictionary merging."""
        base = {"a": 1, "b": {"c": 2}}
        override = {"b": {"d": 3}, "e": 4}
        
        merged = merge_dicts(base, override)
        self.assertEqual(merged["a"], 1)
        self.assertEqual(merged["b"]["c"], 2)
        self.assertEqual(merged["b"]["d"], 3)
        self.assertEqual(merged["e"], 4)
    
    def test_get_env_var(self):
        """Test environment variable retrieval."""
        import os
        
        # Test existing variable
        os.environ["TEST_VAR"] = "test_value"
        value = get_env_var("TEST_VAR")
        self.assertEqual(value, "test_value")
        
        # Test default value
        value = get_env_var("NON_EXISTENT_VAR", default="default")
        self.assertEqual(value, "default")
        
        # Test required variable
        with self.assertRaises(ValueError):
            get_env_var("NON_EXISTENT_VAR", required=True)
        
        # Cleanup
        del os.environ["TEST_VAR"]


class TestTimer(unittest.TestCase):
    """Test Timer context manager."""
    
    def test_timer_basic(self):
        """Test basic timer functionality."""
        with Timer("test") as timer:
            time.sleep(0.1)
        
        self.assertIsNotNone(timer.elapsed)
        self.assertGreaterEqual(timer.elapsed, 0.1)
        self.assertIsNotNone(timer.elapsed_ms())
    
    def test_timer_name(self):
        """Test timer with custom name."""
        with Timer("custom_operation") as timer:
            pass
        
        self.assertEqual(timer.name, "custom_operation")


class TestRateLimiter(unittest.TestCase):
    """Test RateLimiter class."""
    
    def test_acquire_basic(self):
        """Test basic token acquisition."""
        limiter = RateLimiter(rate=10, capacity=5)
        
        # Should be able to acquire up to capacity
        for _ in range(5):
            self.assertTrue(limiter.acquire())
        
        # Next acquire should fail
        self.assertFalse(limiter.acquire())
    
    def test_token_refill(self):
        """Test token refill over time."""
        limiter = RateLimiter(rate=100, capacity=5)
        
        # Exhaust tokens
        for _ in range(5):
            limiter.acquire()
        
        # Wait for refill
        time.sleep(0.1)  # Should add ~10 tokens
        
        # Should be able to acquire again
        self.assertTrue(limiter.acquire())
    
    def test_wait_for_token(self):
        """Test waiting for tokens."""
        limiter = RateLimiter(rate=100, capacity=1)
        
        # Exhaust tokens
        limiter.acquire()
        
        # Wait for token (should succeed quickly)
        start = time.time()
        result = limiter.wait_for_token(timeout=1.0)
        elapsed = time.time() - start
        
        self.assertTrue(result)
        self.assertLess(elapsed, 0.5)


class TestTelemetry(unittest.TestCase):
    """Test telemetry components."""
    
    def test_meter(self):
        """Test Meter (Metrics) class."""
        from agentrt.telemetry import TelemetryConfig
        
        meter = Meter()
        
        # Record metrics via counter
        meter.increment_counter("request_count", 1)
        meter.increment_counter("request_count", 2)
        
        stats = meter.get_stats("request_count")
        self.assertEqual(stats["value"], 3)
        
        # Test gauge
        meter.set_gauge("cpu_usage", 72.5)
        gauge_stats = meter.get_stats("cpu_usage")
        self.assertEqual(gauge_stats["value"], 72.5)
        
        # Test export
        exported = meter.export_metrics()
        self.assertIn("counters", exported)
        self.assertIn("gauges", exported)
    
    def test_tracer(self):
        """Test Tracer class."""
        from agentrt.telemetry import TelemetryConfig
        
        config = TelemetryConfig(service_name="test_service")
        tracer = Tracer(config)
        
        # Start and use span
        with tracer.start_span("operation_name") as span:
            span.set_attribute("key", "value")
        
        spans = tracer.get_exported_spans()
        self.assertEqual(len(spans), 1)
        self.assertEqual(spans[0]["name"], "operation_name")
        self.assertEqual(spans[0]["attributes"]["key"], "value")
        self.assertEqual(spans[0]["status"], "ok")
    
    def test_telemetry_integration(self):
        """Test Telemetry coordinator."""
        from agentrt.telemetry import TelemetryConfig
        
        config = TelemetryConfig(service_name="test_service")
        telemetry = Telemetry(config)
        
        # Record metric
        telemetry.metrics.increment_counter("test_metric", 42)
        
        # Create span
        with telemetry.tracer.start_span("test_operation") as span:
            span.set_attribute("key", "value")
        
        # Export all
        all_data = telemetry.export_all()
        self.assertIn("traces", all_data)
        self.assertIn("metrics", all_data)


class TestRetryDecorator(unittest.TestCase):
    """Test retry_with_backoff decorator."""

    def test_retry_success(self):
        """Test retry until success."""
        from agentrt.utils.core import retry_with_backoff

        call_count = 0

        @retry_with_backoff(max_retries=3, base_delay=0.01)
        def flaky_function():
            nonlocal call_count
            call_count += 1
            if call_count < 3:
                raise Exception("Temporary error")
            return "success"

        result = flaky_function()
        self.assertEqual(result, "success")
        self.assertEqual(call_count, 3)

    def test_retry_exhausted(self):
        """Test retry exhaustion."""
        from agentrt.utils.core import retry_with_backoff

        call_count = 0

        @retry_with_backoff(max_retries=2, base_delay=0.01)
        def failing_function():
            nonlocal call_count
            call_count += 1
            raise Exception("Always fails")

        with self.assertRaises(Exception):
            failing_function()

        self.assertEqual(call_count, 3)


class TestSyscallBinding(unittest.TestCase):
    """Test SyscallBinding class (mocked)."""

    def test_syscall_import_available(self):
        """Test that SyscallError is available in exceptions."""
        self.assertIsNotNone(SyscallError)
        self.assertTrue(issubclass(SyscallError, AgentOSError))


if __name__ == "__main__":
    unittest.main(verbosity=2)
