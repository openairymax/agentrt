"""
Unit tests for AgentOS Python SDK.

This module contains comprehensive tests for the SDK components.
Run with: pytest tests/test_agent.py -v
"""

import unittest
from unittest.mock import Mock, patch, MagicMock
import json
import time

# Import all available public APIs
from agentos import (
    AgentOS, AsyncAgentOS,
    Telemetry, Meter, Tracer, Span, SpanStatus,
    TaskStatus, TaskResult, SkillInfo, SkillResult,
    Memory, MemorySearchResult, Session, Skill,
    MemoryLayer, SessionStatus, SkillStatus,
    generate_id, generate_timestamp, generate_hash,
    validate_json, sanitize_string,
    get_env_var, parse_timeout, merge_dicts,
    Timer, RateLimiter,
    AgentOSError, TaskError, AgentOSMemoryError, SessionError, SkillError,
    NetworkError, AgentOSTimeoutError, InitializationError, ValidationError, TelemetryError,
    ConfigError, SyscallError, RateLimitError,
)

# 从子模块直接导入（不在 __init__.py 的 __all__ 中）
from agentos.task import Task
from agentos.memory import Memory
from agentos.session import Session
from agentos.skill import Skill

# 向后兼容别名
MemoryError = AgentOSMemoryError
TimeoutError = AgentOSTimeoutError


class TestTypes(unittest.TestCase):
    """Test type definitions and data structures."""
    
    def test_task_status_enum(self):
        """Test TaskStatus enum values."""
        self.assertEqual(TaskStatus.PENDING.value, 0)
        self.assertEqual(TaskStatus.RUNNING.value, 1)
        self.assertEqual(TaskStatus.SUCCEEDED.value, 2)
        self.assertEqual(TaskStatus.FAILED.value, 3)
        self.assertEqual(TaskStatus.CANCELLED.value, 4)
    
    def test_task_result(self):
        """Test TaskResult dataclass."""
        result = TaskResult(
            task_id="task_123",
            status=TaskStatus.SUCCEEDED,
            result={"output": "test"},
            error=None
        )
        
        self.assertTrue(result.is_success())
        self.assertFalse(result.is_failed())
        self.assertFalse(result.is_pending())
        self.assertFalse(result.is_running())
        
        # Test serialization
        data = result.to_dict()
        self.assertEqual(data["task_id"], "task_123")
        self.assertEqual(data["status"], "SUCCEEDED")
        
        # Test deserialization
        result2 = TaskResult.from_dict(data)
        self.assertEqual(result2.task_id, result.task_id)
        self.assertEqual(result2.status, result.status)
    
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
            error=None,
            execution_time_ms=150.5
        )
        
        self.assertTrue(result.success)
        self.assertIsNotNone(result.output)
        
        data = result.to_dict()
        self.assertEqual(data["success"], True)
        self.assertEqual(data["execution_time_ms"], 150.5)


class TestExceptions(unittest.TestCase):
    """Test exception classes."""
    
    def test_agentos_error(self):
        """Test base AgentOSError."""
        error = AgentOSError(
            error_code=1001,
            message="Test error",
            details={"key": "value"}
        )
        
        self.assertEqual(error.error_code, 1001)
        self.assertEqual(error.message, "Test error")
        self.assertIn("key", error.details)
        
        # Test string representation
        error_str = str(error)
        self.assertIn("1001", error_str)
        self.assertIn("Test error", error_str)
        
        # Test serialization
        data = error.to_dict()
        self.assertEqual(data["error_code"], 1001)
        self.assertEqual(data["message"], "Test error")
    
    def test_timeout_error(self):
        """Test TimeoutError."""
        error = TimeoutError(timeout_ms=5000, operation="task wait")
        
        self.assertEqual(error.timeout_ms, 5000)
        self.assertEqual(error.operation, "task wait")
        self.assertIn("5000ms", str(error))
    
    def test_initialization_error(self):
        """Test InitializationError."""
        error = InitializationError(
            error_code=1001,
            message="Failed to load library",
            details={"lib_path": "/path/to/lib.so"}
        )
        
        self.assertEqual(error.error_code, 1001)
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
        self.assertIsInstance(ts, float)
        self.assertGreater(ts, 0)
    
    def test_generate_hash(self):
        """Test hash generation."""
        hash1 = generate_hash("test data")
        hash2 = generate_hash(b"test data")
        
        self.assertEqual(hash1, hash2)
        self.assertEqual(len(hash1), 64)  # SHA256 hex length
    
    def test_validate_json(self):
        """Test JSON validation."""
        self.assertTrue(validate_json({"key": "value"}))
        self.assertTrue(validate_json([1, 2, 3]))
        self.assertTrue(validate_json("string"))
        self.assertTrue(validate_json(123))
        self.assertFalse(validate_json(set([1, 2, 3])))
    
    def test_sanitize_string(self):
        """Test string sanitization."""
        # Test control character removal
        dirty = "test\x00data\x01with\x02control"
        clean = sanitize_string(dirty)
        self.assertNotIn("\x00", clean)
        
        # Test length limiting
        long_string = "a" * 2000
        sanitized = sanitize_string(long_string, max_length=100)
        self.assertLessEqual(len(sanitized), 103)  # 100 + "..."
    
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
        """Test Meter class."""
        meter = Meter(service_name="test_service")
        
        # Record metrics
        meter.record("request_count", 1, labels={"method": "POST"})
        meter.record("response_time_ms", 150.5)
        
        metrics = meter.get_metrics()
        self.assertEqual(len(metrics), 2)
        self.assertEqual(metrics[0].name, "request_count")
        self.assertEqual(metrics[0].value, 1)
        
        # Test export
        exported = meter.export()
        data = json.loads(exported)
        self.assertEqual(data["service_name"], "test_service")
        self.assertEqual(len(data["metrics"]), 2)
    
    def test_tracer(self):
        """Test Tracer class."""
        tracer = Tracer(service_name="test_service")
        
        # Start span
        span = tracer.start_span("operation_name")
        span.set_attribute("key", "value")
        
        # End span
        tracer.end_span(span, SpanStatus.OK)
        
        spans = tracer.get_spans()
        self.assertEqual(len(spans), 1)
        self.assertEqual(spans[0].name, "operation_name")
        self.assertEqual(spans[0].status, SpanStatus.OK)
        self.assertEqual(spans[0].attributes["key"], "value")
        
        # Test export
        exported = tracer.export()
        data = json.loads(exported)
        self.assertEqual(data["service_name"], "test_service")
    
    def test_telemetry_integration(self):
        """Test Telemetry coordinator."""
        telemetry = Telemetry(service_name="test_service")
        
        # Record metric
        telemetry.record_metric("test_metric", 42.0)
        
        # Create span
        span = telemetry.start_span("test_operation")
        telemetry.end_span(span)
        
        # Export all
        all_data = telemetry.export_all()
        self.assertIn("metrics", all_data)
        self.assertIn("traces", all_data)


class TestRetryDecorator(unittest.TestCase):
    """Test retry_with_backoff decorator."""

    def test_retry_success(self):
        """Test retry until success."""
        from agentos.utils import retry_with_backoff

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
        from agentos.utils import retry_with_backoff

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
