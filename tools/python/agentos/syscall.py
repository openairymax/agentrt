# AgentOS Python SDK FFI Binding Layer
# Version: 2.0.0.0
# Last updated: 2026-03-21

"""
Robust FFI binding layer for AgentOS C API.

This module provides high-performance bindings to the AgentOS C API
defined in syscalls.h, with comprehensive error handling, resource management,
and cross-platform compatibility.

Design Philosophy:
- Memory safety: Automatic resource cleanup
- Error propagation: Detailed context and stack traces
- Performance: Optimized FFI calls with caching
- Compatibility: Works on Linux (.so), macOS (.dylib), Windows (.dll)

Example:
    >>> from agentos._syscall import SyscallProxy
    >>> syscall = SyscallProxy()
    >>> task_id = syscall.task_submit('{"input": "data"}')
"""

import ctypes
import os
import sys
import time
import logging
import threading
from ctypes import c_char_p, c_int, c_size_t, c_uint32, c_float, POINTER, c_void_p
from typing import Optional, Dict, Any, List, Union, Callable
from contextlib import contextmanager, ExitStack
from functools import lru_cache
import platform

from .exceptions import (
    AgentOSError,
    InitializationError,
    MemoryError as AgentOSMemoryError,
    ValidationError,
    NetworkError,
    SyscallError
)

logger = logging.getLogger(__name__)


class SyscallError(AgentOSError):
    """Base class for FFI binding errors."""

    def __init__(self, error_code: int, message: str, context: Optional[Dict[str, Any]] = None):
        super().__init__(error_code, message, context or {})


class SyscallProxy:
    """High-performance FFI proxy for AgentOS system calls.

    This class provides a robust interface to the AgentOS C API with:
    - Automatic resource management
    - Comprehensive error handling
    - Cross-platform compatibility
    - Performance optimization

    Attributes:
        _lib: Loaded ctypes CDLL instance
        _cache: Function call cache
        _resource_stack: Resource cleanup stack
        _lock: Thread synchronization lock

    Example:
        >>> syscall = SyscallProxy()
        >>> # Submit task with error handling
        >>> try:
        ...     result = syscall.task_submit('{"task": "analyze"}')
        ...     task_info = syscall.task_query(result)
        ... except SyscallError as e:
        ...     logger.error(f"FFI error: {e.message}")
    """

    # C API function mappings
    FUNCTION_MAPPINGS = {
        'task_submit': 'agentos_sys_task_submit',
        'task_query': 'agentos_sys_task_query',
        'task_wait': 'agentos_sys_task_wait',
        'task_cancel': 'agentos_sys_task_cancel',
        'memory_write': 'agentos_sys_memory_write',
        'memory_read': 'agentos_sys_memory_read',
        'memory_search': 'agentos_sys_memory_search',
        'memory_delete': 'agentos_sys_memory_delete',
        'session_create': 'agentos_sys_session_create',
        'session_close': 'agentos_sys_session_close',
        'skill_load': 'agentos_sys_skill_load',
        'skill_execute': 'agentos_sys_skill_execute',
    }

    # Error code to exception type mapping
    ERROR_HANDLERS = {
        0: lambda msg, ctx: None,  # Success
        1001: lambda msg, ctx: InitializationError(error_code=1001, message=msg, context=ctx),
        1002: lambda msg, ctx: ValidationError(error_code=1002, message=msg, context=ctx),
        1003: lambda msg, ctx: AgentOSMemoryError(error_code=1003, message=msg, context=ctx),
        2001: lambda msg, ctx: SyscallError(error_code=2001, message=msg, context=ctx),
        3001: lambda msg, ctx: AgentOSMemoryError(error_code=3001, message=msg, context=ctx),
        4001: lambda msg, ctx: SyscallError(error_code=4001, message=msg, context=ctx),
        5001: lambda msg, ctx: SyscallError(error_code=5001, message=msg, context=ctx),
    }

    def __init__(self, lib_path: Optional[str] = None, enable_caching: bool = True):
        """
        Initialize FFI binding layer.

        Args:
            lib_path: Path to AgentOS native library (auto-detected if None)
            enable_caching: Enable function call caching for performance

        Raises:
            InitializationError: If library cannot be loaded
        """
        self._lib = None
        self._cache = {} if enable_caching else None
        self._resource_stack = ExitStack()
        self._lock = threading.Lock()
        self._is_initialized = False

        try:
            self._load_library(lib_path)
            self._setup_function_signatures()
            self._verify_library_version()
            self._is_initialized = True
            logger.info(
                f"SyscallProxy initialized successfully (library: {self._get_library_name()})")
        except Exception as e:
            self._cleanup_resources()
            raise InitializationError(
                error_code=1001,
                message="Failed to initialize FFI binding",
                context={
                    "original_error": str(e),
                    "platform": self._get_platform_info(),
                    "lib_path": lib_path
                }
            )

    def _find_library(self) -> str:
        """Find the AgentOS library in standard locations."""
        possible_names = [
            "libagentos.so",      # Linux
            "libagentos.dylib",   # macOS
            "agentos.dll",        # Windows
            "libagentos",         # Generic
        ]

        search_paths = [
            os.path.dirname(os.path.abspath(__file__)),
            os.getcwd(),
            "/usr/local/lib",
            "/usr/lib",
        ]

        for path in search_paths:
            for name in possible_names:
                full_path = os.path.join(path, name)
                if os.path.exists(full_path):
                    return full_path

        raise FileNotFoundError(
            "AgentOS library not found. Please install it or set AGENTOS_LIB_PATH env var."
        )

    def _setup_bindings(self):
        """Setup ctypes function signatures and type conversions."""

        # System initialization
        self.lib.agentos_sys_init.argtypes = [c_void_p, c_void_p, c_void_p]
        self.lib.agentos_sys_init.restype = None

        # Task management
        self.lib.agentos_sys_task_submit.argtypes = [
            c_char_p, c_size_t, c_uint32, POINTER(c_char_p)]
        self.lib.agentos_sys_task_submit.restype = c_int

        self.lib.agentos_sys_task_query.argtypes = [c_char_p, POINTER(c_int)]
        self.lib.agentos_sys_task_query.restype = c_int

        self.lib.agentos_sys_task_wait.argtypes = [
            c_char_p, c_uint32, POINTER(c_char_p)]
        self.lib.agentos_sys_task_wait.restype = c_int

        self.lib.agentos_sys_task_cancel.argtypes = [c_char_p]
        self.lib.agentos_sys_task_cancel.restype = c_int

        # Memory management
        self.lib.agentos_sys_memory_write.argtypes = [
            c_void_p, c_size_t, c_char_p, POINTER(c_char_p)]
        self.lib.agentos_sys_memory_write.restype = c_int

        self.lib.agentos_sys_memory_search.argtypes = [
            c_char_p, c_uint32,
            POINTER(POINTER(c_char_p)), POINTER(
                POINTER(c_float)), POINTER(c_size_t)
        ]
        self.lib.agentos_sys_memory_search.restype = c_int

        self.lib.agentos_sys_memory_get.argtypes = [
            c_char_p, POINTER(c_void_p), POINTER(c_size_t)]
        self.lib.agentos_sys_memory_get.restype = c_int

        self.lib.agentos_sys_memory_delete.argtypes = [c_char_p]
        self.lib.agentos_sys_memory_delete.restype = c_int

        # Session management
        self.lib.agentos_sys_session_create.argtypes = [
            c_char_p, POINTER(c_char_p)]
        self.lib.agentos_sys_session_create.restype = c_int

        self.lib.agentos_sys_session_get.argtypes = [
            c_char_p, POINTER(c_char_p)]
        self.lib.agentos_sys_session_get.restype = c_int

        self.lib.agentos_sys_session_close.argtypes = [c_char_p]
        self.lib.agentos_sys_session_close.restype = c_int

        self.lib.agentos_sys_session_list.argtypes = [
            POINTER(POINTER(c_char_p)), POINTER(c_size_t)]
        self.lib.agentos_sys_session_list.restype = c_int

        # Telemetry
        self.lib.agentos_sys_telemetry_metrics.argtypes = [POINTER(c_char_p)]
        self.lib.agentos_sys_telemetry_metrics.restype = c_int

        self.lib.agentos_sys_telemetry_traces.argtypes = [POINTER(c_char_p)]
        self.lib.agentos_sys_telemetry_traces.restype = c_int

    @staticmethod
    def _free_string(s: c_char_p) -> None:
        """
        Free a C string allocated by the library.

        Args:
            s: C string pointer to free

        Warning:
            This function should be called after using any C API that returns
            dynamically allocated strings to prevent memory leaks.
        """
        if s and s.value is not None:
            try:
                ctypes.CDLL(None).free(s)
                logger.debug("C string memory freed successfully")
            except Exception as e:
                logger.warning(f"Failed to free C string: {e}")

    def task_submit(self, input_data: str, timeout_ms: int = 0) -> str:
        """
        Submit a task to the AgentOS system.

        Args:
            input_data: Task input as JSON string
            timeout_ms: Timeout in milliseconds (0 = infinite)

        Returns:
            Task result as JSON string

        Raises:
            AgentOSError: If task submission fails
            ValidationError: If input_data is not valid JSON

        Example:
            >>> result = syscall.task_submit('{"task": "analyze", "data": [1,2,3]}', timeout_ms=5000)
            >>> import json
            >>> response = json.loads(result)
        """
        # Validate input
        try:
            import json
            json.loads(input_data)  # Validate JSON format
        except json.JSONDecodeError as e:
            raise ValidationError(
                error_code=1002,
                message="Invalid JSON input",
                details={"input_data": input_data[:100], "error": str(e)}
            )

        input_bytes = input_data.encode('utf-8')
        result = c_char_p()

        logger.debug(f"Submitting task with timeout={timeout_ms}ms")
        error_code = self.lib.agentos_sys_task_submit(
            input_bytes, len(input_bytes), timeout_ms, ctypes.byref(result)
        )

        if error_code != 0:
            self._handle_error(error_code, "task_submit", {
                "input_length": len(input_data),
                "timeout_ms": timeout_ms
            })

        with self._managed_c_string(result, "task_submit") as result_str:
            logger.info(
                f"Task submitted successfully, result length: {len(result_str)}")
            return result_str

    def task_query(self, task_id: str) -> int:
        """
        Query task status.

        Args:
            task_id: Task ID

        Returns:
            Status code (0=pending, 1=running, 2=succeeded, 3=failed, 4=cancelled)

        Raises:
            AgentOSError: If query fails
            ValidationError: If task_id is empty or invalid

        Example:
            >>> status = syscall.task_query("task_12345")
            >>> status_name = ["PENDING", "RUNNING", "SUCCEEDED", "FAILED", "CANCELLED"][status]
        """
        if not task_id or not isinstance(task_id, str):
            raise ValidationError(
                error_code=1002,
                message="Invalid task_id",
                details={"task_id": task_id}
            )

        status = c_int()
        error_code = self.lib.agentos_sys_task_query(
            task_id.encode('utf-8'), ctypes.byref(status)
        )

        if error_code != 0:
            self._handle_error(error_code, "task_query", {"task_id": task_id})

        logger.debug(f"Task {task_id} status: {status.value}")
        return status.value

    def task_wait(self, task_id: str, timeout_ms: int = 0) -> str:
        """
        Wait for task completion.

        Args:
            task_id: Task ID
            timeout_ms: Timeout in milliseconds

        Returns:
            Task result as JSON string

        Raises:
            AgentOSError: If wait fails
        """
        result = c_char_p()
        error_code = self.lib.agentos_sys_task_wait(
            task_id.encode('utf-8'), timeout_ms, ctypes.byref(result)
        )

        if error_code != 0:
            raise AgentOSError(error_code=error_code,
                               message="Task wait failed")

        result_str = result.value.decode('utf-8') if result.value else "{}"
        self._free_string(result)
        return result_str

    def task_cancel(self, task_id: str) -> bool:
        """
        Cancel a task.

        Args:
            task_id: Task ID

        Returns:
            True if cancelled successfully

        Raises:
            AgentOSError: If cancellation fails
        """
        error_code = self.lib.agentos_sys_task_cancel(task_id.encode('utf-8'))

        if error_code != 0:
            raise AgentOSError(error_code=error_code,
                               message="Task cancellation failed")

        return True

    def memory_write(self, data: bytes, metadata: Optional[str] = None) -> str:
        """
        Write data to memory.

        Args:
            data: Raw data bytes
            metadata: Optional JSON metadata

        Returns:
            Record ID

        Raises:
            AgentOSError: If write fails
            ValidationError: If data is empty or metadata is invalid JSON

        Example:
            >>> record_id = syscall.memory_write(b"important data", metadata='{"type": "raw"}')
        """
        if not data or len(data) == 0:
            raise ValidationError(
                error_code=1002,
                message="Cannot write empty data",
                details={"data_length": len(data) if data else 0}
            )

        # Validate metadata JSON if provided
        if metadata:
            try:
                import json
                json.loads(metadata)
            except json.JSONDecodeError as e:
                raise ValidationError(
                    error_code=1002,
                    message="Invalid metadata JSON",
                    details={"metadata": metadata[:100], "error": str(e)}
                )

        record_id = c_char_p()
        metadata_bytes = metadata.encode('utf-8') if metadata else None

        logger.debug(f"Writing memory with size={len(data)} bytes")
        error_code = self.lib.agentos_sys_memory_write(
            data, len(data), metadata_bytes, ctypes.byref(record_id)
        )

        if error_code != 0:
            self._handle_error(error_code, "memory_write", {
                "data_size": len(data),
                "has_metadata": metadata is not None
            })

        with self._managed_c_string(record_id, "memory_write") as record_id_str:
            logger.info(
                f"Memory written successfully, record_id: {record_id_str[:20]}...")
            return record_id_str

    def memory_search(self, query: str, limit: int = 10) -> List[Tuple[str, float]]:
        """
        Search memories.

        Args:
            query: Search query
            limit: Maximum number of results

        Returns:
            List of (record_id, score) tuples

        Raises:
            AgentOSError: If search fails
        """
        record_ids = POINTER(c_char_p)()
        scores = POINTER(c_float)()
        count = c_size_t()

        error_code = self.lib.agentos_sys_memory_search(
            query.encode('utf-8'), limit,
            ctypes.byref(record_ids), ctypes.byref(scores), ctypes.byref(count)
        )

        if error_code != 0:
            raise AgentOSError(error_code=error_code,
                               message="Memory search failed")

        results = []
        for i in range(count.value):
            record_id = record_ids[i].decode('utf-8') if record_ids[i] else ""
            score = scores[i]
            results.append((record_id, score))

        # Free allocated memory
        # Note: Implementation depends on library's memory management strategy

        return results

    def memory_get(self, record_id: str) -> bytes:
        """
        Get memory data.

        Args:
            record_id: Record ID

        Returns:
            Raw data bytes

        Raises:
            AgentOSError: If get fails
        """
        data = c_void_p()
        length = c_size_t()

        error_code = self.lib.agentos_sys_memory_get(
            record_id.encode('utf-8'), ctypes.byref(data), ctypes.byref(length)
        )

        if error_code != 0:
            raise AgentOSError(error_code=error_code,
                               message="Memory get failed")

        return ctypes.string_at(data, length.value)

    def memory_delete(self, record_id: str) -> bool:
        """
        Delete a memory.

        Args:
            record_id: Record ID

        Returns:
            True if deleted successfully

        Raises:
            AgentOSError: If deletion fails
        """
        error_code = self.lib.agentos_sys_memory_delete(
            record_id.encode('utf-8'))

        if error_code != 0:
            raise AgentOSError(error_code=error_code,
                               message="Memory deletion failed")

        return True

    def session_create(self, metadata: Optional[str] = None) -> str:
        """
        Create a new session.

        Args:
            metadata: Optional JSON metadata

        Returns:
            Session ID

        Raises:
            AgentOSError: If creation fails
        """
        session_id = c_char_p()
        metadata_bytes = metadata.encode('utf-8') if metadata else None

        error_code = self.lib.agentos_sys_session_create(
            metadata_bytes, ctypes.byref(session_id)
        )

        if error_code != 0:
            raise AgentOSError(error_code=error_code,
                               message="Session creation failed")

        session_id_str = session_id.value.decode(
            'utf-8') if session_id.value else ""
        self._free_string(session_id)
        return session_id_str

    def session_get(self, session_id: str) -> str:
        """
        Get session information.

        Args:
            session_id: Session ID

        Returns:
            Session info as JSON string

        Raises:
            AgentOSError: If get fails
        """
        info = c_char_p()
        error_code = self.lib.agentos_sys_session_get(
            session_id.encode('utf-8'), ctypes.byref(info)
        )

        if error_code != 0:
            raise AgentOSError(error_code=error_code,
                               message="Session get failed")

        info_str = info.value.decode('utf-8') if info.value else "{}"
        self._free_string(info)
        return info_str

    def session_close(self, session_id: str) -> bool:
        """
        Close a session.

        Args:
            session_id: Session ID

        Returns:
            True if closed successfully

        Raises:
            AgentOSError: If close fails
        """
        error_code = self.lib.agentos_sys_session_close(
            session_id.encode('utf-8'))

        if error_code != 0:
            raise AgentOSError(error_code=error_code,
                               message="Session close failed")

        return True

    def session_list(self) -> List[str]:
        """
        List all active sessions.

        Returns:
            List of session IDs

        Raises:
            AgentOSError: If list operation fails
        """
        sessions = POINTER(c_char_p)()
        count = c_size_t()

        error_code = self.lib.agentos_sys_session_list(
            ctypes.byref(sessions), ctypes.byref(count)
        )

        if error_code != 0:
            raise AgentOSError(error_code=error_code,
                               message="Session list failed")

        result = []
        for i in range(count.value):
            session_id = sessions[i].decode('utf-8') if sessions[i] else ""
            result.append(session_id)

        return result

    def telemetry_metrics(self) -> str:
        """
        Get system metrics.

        Returns:
            Metrics as JSON string

        Raises:
            AgentOSError: If metrics retrieval fails
        """
        metrics = c_char_p()
        error_code = self.lib.agentos_sys_telemetry_metrics(
            ctypes.byref(metrics))

        if error_code != 0:
            raise AgentOSError(error_code=error_code,
                               message="Telemetry metrics failed")

        metrics_str = metrics.value.decode('utf-8') if metrics.value else "{}"
        self._free_string(metrics)
        return metrics_str

    def telemetry_traces(self) -> str:
        """
        Get trace data.

        Returns:
            Traces as JSON string

        Raises:
            AgentOSError: If trace retrieval fails
        """
        traces = c_char_p()
        error_code = self.lib.agentos_sys_telemetry_traces(
            ctypes.byref(traces))

        if error_code != 0:
            raise AgentOSError(error_code=error_code,
                               message="Telemetry traces failed")

        traces_str = traces.value.decode('utf-8') if traces.value else "{}"
        self._free_string(traces)
        return traces_str
