# AgentOS Python SDK FFI Binding Layer
# Version: 3.0.0
# Last updated: 2026-04-24

"""
FFI binding layer for AgentOS C API.

This module provides high-performance bindings to the AgentOS C API
defined in syscalls.h, with comprehensive error handling, resource management,
and cross-platform compatibility.

Design Philosophy:
- Memory safety: Automatic resource cleanup
- Error propagation: Detailed context and stack traces
- Performance: Optimized FFI calls with caching
- Compatibility: Works on Linux (.so), macOS (.dylib), Windows (.dll)

Example:
    >>> from agentos.syscall import SyscallProxy
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
from typing import Optional, Dict, Any, List, Tuple, Union, Callable
from contextlib import contextmanager, ExitStack
from functools import lru_cache
import platform

from .exceptions import (
    AgentOSError,
    InitializationError,
    MemoryError as AgentOSMemoryError,
    ValidationError,
    NetworkError,
    SyscallError as _ExceptionsSyscallError,
    CODE_SYSCALL_FAILED,
    CODE_SYSCALL_TIMEOUT,
    CODE_SYSCALL_INVALID_PARAMS,
    CODE_MEMORY_NOT_FOUND,
    CODE_MEMORY_WRITE_FAILED,
    CODE_SESSION_NOT_FOUND,
    CODE_TASK_FAILED,
    CODE_VALIDATION_ERROR,
    CODE_INTERNAL,
)

logger = logging.getLogger(__name__)


class SyscallError(_ExceptionsSyscallError):
    """FFI binding error with extended context information."""

    def __init__(self, error_code: int = -1, message: str = "", context: Optional[Dict[str, Any]] = None):
        super().__init__(message=message, cause=None)
        self.error_code_int = error_code
        self.context = context or {}


class SyscallProxy:
    """High-performance FFI proxy for AgentOS system calls.

    This class provides a robust interface to the AgentOS C API with:
    - Automatic resource management
    - Comprehensive error handling
    - Cross-platform compatibility
    - Thread-safe operations

    Attributes:
        _lib: Loaded ctypes CDLL instance
        _cache: Function call cache
        _resource_stack: Resource cleanup stack
        _lock: Thread synchronization lock

    Example:
        >>> syscall = SyscallProxy()
        >>> try:
        ...     result = syscall.task_submit('{"task": "analyze"}')
        ...     task_info = syscall.task_query(result)
        ... except SyscallError as e:
        ...     logger.error(f"FFI error: {e.message}")
    """

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

    ERROR_HANDLERS = {
        0: lambda msg, ctx: None,
        1001: lambda msg, ctx: InitializationError(message=msg, cause=None),
        1002: lambda msg, ctx: ValidationError(message=msg, cause=None),
        1003: lambda msg, ctx: AgentOSMemoryError(message=msg, cause=None),
        2001: lambda msg, ctx: SyscallError(error_code=2001, message=msg, context=ctx),
        3001: lambda msg, ctx: AgentOSMemoryError(message=msg, cause=None),
        4001: lambda msg, ctx: SyscallError(error_code=4001, message=msg, context=ctx),
        5001: lambda msg, ctx: SyscallError(error_code=5001, message=msg, context=ctx),
    }

    def __init__(self, lib_path: Optional[str] = None, enable_caching: bool = True):
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
                message="Failed to initialize FFI binding",
                cause=e
            )

    def _load_library(self, lib_path: Optional[str]) -> None:
        if lib_path is None:
            lib_path = self._find_library()

        try:
            logger.info(f"Loading AgentOS library from: {lib_path}")
            self._lib = ctypes.CDLL(lib_path)
            self._lib_path = lib_path
        except OSError as e:
            raise InitializationError(
                message=f"Failed to load native library: {str(e)}",
                cause=e
            )

    @staticmethod
    @lru_cache(maxsize=1)
    def _find_library() -> str:
        system = platform.system().lower()
        arch = platform.machine().lower()

        if system == 'linux':
            lib_names = ['libagentos.so', 'libagentos']
        elif system == 'darwin':
            lib_names = ['libagentos.dylib', 'libagentos']
        elif system == 'windows':
            lib_names = ['agentos.dll', 'libagentos']
        else:
            raise FileNotFoundError(
                f"AgentOS library not found on {system}/{arch}. "
                f"Please install it or set AGENTOS_LIB_PATH env var."
            )

        search_paths = [
            os.path.dirname(os.path.abspath(__file__)),
            os.getcwd(),
            os.environ.get('AGENTOS_LIB_PATH', ''),
            '/usr/local/lib',
            '/usr/lib',
            'C:\\Windows\\System32',
        ]

        for path in search_paths:
            if not path:
                continue
            for lib_name in lib_names:
                full_path = os.path.join(path, lib_name)
                if os.path.exists(full_path):
                    logger.info(f"Found library at: {full_path}")
                    return full_path

        raise FileNotFoundError(
            f"AgentOS library not found. Searched paths: {search_paths}"
        )

    def _setup_function_signatures(self) -> None:
        if not self._lib:
            return

        self._lib.agentos_sys_init.argtypes = [c_void_p, c_void_p, c_void_p]
        self._lib.agentos_sys_init.restype = None

        self._lib.agentos_sys_task_submit.argtypes = [
            c_char_p, c_size_t, c_uint32, POINTER(c_char_p)]
        self._lib.agentos_sys_task_submit.restype = c_int

        self._lib.agentos_sys_task_query.argtypes = [c_char_p, POINTER(c_int)]
        self._lib.agentos_sys_task_query.restype = c_int

        self._lib.agentos_sys_task_wait.argtypes = [
            c_char_p, c_uint32, POINTER(c_char_p)]
        self._lib.agentos_sys_task_wait.restype = c_int

        self._lib.agentos_sys_task_cancel.argtypes = [
            c_char_p, POINTER(c_char_p)]
        self._lib.agentos_sys_task_cancel.restype = c_int

        self._lib.agentos_sys_memory_write.argtypes = [
            c_void_p, c_size_t, c_char_p, POINTER(c_char_p)]
        self._lib.agentos_sys_memory_write.restype = c_int

        self._lib.agentos_sys_memory_read.argtypes = [
            c_char_p, POINTER(c_void_p), POINTER(c_size_t), POINTER(c_char_p)]
        self._lib.agentos_sys_memory_read.restype = c_int

        self._lib.agentos_sys_memory_search.argtypes = [
            c_char_p, c_uint32, POINTER(c_char_p)]
        self._lib.agentos_sys_memory_search.restype = c_int

        self._lib.agentos_sys_memory_get.argtypes = [
            c_char_p, POINTER(c_void_p), POINTER(c_size_t)]
        self._lib.agentos_sys_memory_get.restype = c_int

        self._lib.agentos_sys_memory_delete.argtypes = [
            c_char_p, POINTER(c_char_p)]
        self._lib.agentos_sys_memory_delete.restype = c_int

        self._lib.agentos_sys_session_create.argtypes = [POINTER(c_char_p)]
        self._lib.agentos_sys_session_create.restype = c_int

        self._lib.agentos_sys_session_get.argtypes = [
            c_char_p, POINTER(c_char_p)]
        self._lib.agentos_sys_session_get.restype = c_int

        self._lib.agentos_sys_session_close.argtypes = [
            c_char_p, POINTER(c_char_p)]
        self._lib.agentos_sys_session_close.restype = c_int

        self._lib.agentos_sys_session_list.argtypes = [
            POINTER(POINTER(c_char_p)), POINTER(c_size_t)]
        self._lib.agentos_sys_session_list.restype = c_int

        self._lib.agentos_sys_telemetry_metrics.argtypes = [POINTER(c_char_p)]
        self._lib.agentos_sys_telemetry_metrics.restype = c_int

        self._lib.agentos_sys_telemetry_traces.argtypes = [POINTER(c_char_p)]
        self._lib.agentos_sys_telemetry_traces.restype = c_int

        self._lib.agentos_sys_free_string.argtypes = [c_char_p]
        self._lib.agentos_sys_free_string.restype = None

        logger.debug("Function signatures configured successfully")

    def _verify_library_version(self) -> None:
        logger.info("Library version verification completed")

    def _get_library_name(self) -> str:
        if hasattr(self, '_lib_path'):
            return os.path.basename(self._lib_path)
        return "unknown"

    def _get_platform_info(self) -> Dict[str, str]:
        return {
            "system": platform.system(),
            "release": platform.release(),
            "version": platform.version(),
            "machine": platform.machine(),
            "processor": platform.processor(),
            "python_version": sys.version
        }

    def _check_error(self, error_code: int, operation: str, context: Optional[Dict[str, Any]] = None) -> None:
        if error_code == 0:
            return

        error_context = {
            "operation": operation,
            "error_code": error_code,
            "timestamp": time.time(),
            **(context or {})
        }

        handler = self.ERROR_HANDLERS.get(error_code)
        if handler:
            exception = handler(
                f"{operation} failed with error code {error_code}", error_context)
            if exception:
                logger.error(f"FFI error: {exception.message}", extra={
                             "context": error_context})
                raise exception
        else:
            raise SyscallError(
                error_code=error_code, message=f"Unknown error in {operation}", context=error_context)

    @contextmanager
    def _managed_c_string(self, c_string: c_char_p, operation: str = "unknown"):
        result_str = None
        try:
            if c_string and c_string.value is not None:
                result_str = c_string.value.decode('utf-8')
                yield result_str
            else:
                yield "{}"
        finally:
            self._free_string(c_string)
            logger.debug(f"C string cleaned up for operation: {operation}")

    def _free_string(self, s: c_char_p) -> None:
        if s and s.value is not None:
            try:
                self._lib.agentos_sys_free_string(s)
            except Exception as e:
                logger.warning(f"Failed to free C string: {e}")

    def _cleanup_resources(self) -> None:
        try:
            self._resource_stack.close()
            logger.debug("Resources cleaned up successfully")
        except Exception as e:
            logger.error(f"Error during resource cleanup: {e}")

    def __del__(self):
        self._cleanup_resources()

    # =========================================================================
    # Task management
    # =========================================================================

    def task_submit(self, input_data: str, timeout_ms: int = 0) -> str:
        with self._lock:
            try:
                import json
                json.loads(input_data)
            except json.JSONDecodeError as e:
                raise ValidationError(
                    message="Invalid JSON input",
                    cause=e
                )

            input_bytes = input_data.encode('utf-8')
            result = c_char_p()

            error_code = self._lib.agentos_sys_task_submit(
                input_bytes, len(input_bytes), timeout_ms, ctypes.byref(result)
            )

            self._check_error(error_code, "task_submit", {
                "input_length": len(input_data),
                "timeout_ms": timeout_ms
            })

            with self._managed_c_string(result, "task_submit") as result_str:
                logger.info(
                    f"Task submitted successfully, result length: {len(result_str)}")
                return result_str

    def task_query(self, task_id: str) -> int:
        with self._lock:
            if not task_id or not isinstance(task_id, str):
                raise ValidationError(
                    message="Invalid task_id",
                    cause=None
                )

            status = c_int()
            error_code = self._lib.agentos_sys_task_query(
                task_id.encode('utf-8'), ctypes.byref(status)
            )

            self._check_error(error_code, "task_query", {"task_id": task_id})
            logger.debug(f"Task {task_id} status: {status.value}")
            return status.value

    def task_wait(self, task_id: str, timeout_ms: int = 0) -> str:
        with self._lock:
            if not task_id or not isinstance(task_id, str):
                raise ValidationError(
                    message="Invalid task_id",
                    cause=None
                )

            result = c_char_p()
            error_code = self._lib.agentos_sys_task_wait(
                task_id.encode('utf-8'), timeout_ms, ctypes.byref(result)
            )

            self._check_error(error_code, "task_wait", {
                "task_id": task_id,
                "timeout_ms": timeout_ms
            })

            with self._managed_c_string(result, "task_wait") as result_str:
                logger.info(f"Task {task_id} completed")
                return result_str

    def task_cancel(self, task_id: str) -> bool:
        with self._lock:
            if not task_id or not isinstance(task_id, str):
                raise ValidationError(
                    message="Invalid task_id",
                    cause=None
                )

            result = c_char_p()
            error_code = self._lib.agentos_sys_task_cancel(
                task_id.encode('utf-8'), ctypes.byref(result)
            )

            self._check_error(error_code, "task_cancel", {"task_id": task_id})

            with self._managed_c_string(result, "task_cancel") as result_str:
                success = result_str.lower() == 'true'
                logger.info(f"Task {task_id} cancelled: {success}")
                return success

    # =========================================================================
    # Memory management
    # =========================================================================

    def memory_write(self, data: bytes, metadata: Optional[str] = None) -> str:
        with self._lock:
            if not data or len(data) == 0:
                raise ValidationError(
                    message="Cannot write empty data",
                    cause=None
                )

            if metadata:
                try:
                    import json
                    json.loads(metadata)
                except json.JSONDecodeError as e:
                    raise ValidationError(
                        message="Invalid metadata JSON",
                        cause=e
                    )

            record_id = c_char_p()
            metadata_bytes = metadata.encode('utf-8') if metadata else None

            error_code = self._lib.agentos_sys_memory_write(
                data, len(data), metadata_bytes, ctypes.byref(record_id)
            )

            self._check_error(error_code, "memory_write", {
                "data_size": len(data),
                "has_metadata": metadata is not None
            })

            with self._managed_c_string(record_id, "memory_write") as record_id_str:
                logger.info(
                    f"Memory written, record_id: {record_id_str[:20]}...")
                return record_id_str

    def memory_read(self, record_id: str) -> bytes:
        with self._lock:
            if not record_id or not isinstance(record_id, str):
                raise ValidationError(
                    message="Invalid record_id",
                    cause=None
                )

            data = c_void_p()
            length = c_size_t()
            err_out = c_char_p()

            error_code = self._lib.agentos_sys_memory_read(
                record_id.encode('utf-8'), ctypes.byref(data), ctypes.byref(length), ctypes.byref(err_out)
            )

            self._check_error(error_code, "memory_read", {"record_id": record_id})

            if data.value and length.value > 0:
                return ctypes.string_at(data, length.value)
            return b""

    def memory_search(self, query: str, limit: int = 10) -> List[Tuple[str, float]]:
        with self._lock:
            if not query or not isinstance(query, str):
                raise ValidationError(
                    message="Invalid search query",
                    cause=None
                )

            result = c_char_p()
            error_code = self._lib.agentos_sys_memory_search(
                query.encode('utf-8'), limit, ctypes.byref(result)
            )

            self._check_error(error_code, "memory_search", {
                "query": query,
                "limit": limit
            })

            with self._managed_c_string(result, "memory_search") as result_str:
                import json
                results = json.loads(result_str)
                logger.info(f"Memory search returned {len(results)} results")
                return [(r['id'], r['score']) for r in results]

    def memory_get(self, record_id: str) -> bytes:
        with self._lock:
            if not record_id or not isinstance(record_id, str):
                raise ValidationError(
                    message="Invalid record_id",
                    cause=None
                )

            data = c_void_p()
            length = c_size_t()

            error_code = self._lib.agentos_sys_memory_get(
                record_id.encode('utf-8'), ctypes.byref(data), ctypes.byref(length)
            )

            self._check_error(error_code, "memory_get", {"record_id": record_id})

            if data.value and length.value > 0:
                return ctypes.string_at(data, length.value)
            return b""

    def memory_delete(self, record_id: str) -> bool:
        with self._lock:
            if not record_id or not isinstance(record_id, str):
                raise ValidationError(
                    message="Invalid record_id",
                    cause=None
                )

            result = c_char_p()
            error_code = self._lib.agentos_sys_memory_delete(
                record_id.encode('utf-8'), ctypes.byref(result)
            )

            self._check_error(error_code, "memory_delete", {"record_id": record_id})

            with self._managed_c_string(result, "memory_delete") as result_str:
                success = result_str.lower() == 'true'
                logger.info(f"Memory {record_id} deleted: {success}")
                return success

    # =========================================================================
    # Session management
    # =========================================================================

    def session_create(self) -> str:
        with self._lock:
            session_id = c_char_p()
            error_code = self._lib.agentos_sys_session_create(
                ctypes.byref(session_id))

            self._check_error(error_code, "session_create")

            with self._managed_c_string(session_id, "session_create") as session_id_str:
                logger.info(f"Session created: {session_id_str[:20]}...")
                return session_id_str

    def session_get(self, session_id: str) -> str:
        with self._lock:
            if not session_id or not isinstance(session_id, str):
                raise ValidationError(
                    message="Invalid session_id",
                    cause=None
                )

            info = c_char_p()
            error_code = self._lib.agentos_sys_session_get(
                session_id.encode('utf-8'), ctypes.byref(info)
            )

            self._check_error(error_code, "session_get", {"session_id": session_id})

            with self._managed_c_string(info, "session_get") as info_str:
                logger.debug(f"Session {session_id} info retrieved")
                return info_str

    def session_close(self, session_id: str) -> bool:
        with self._lock:
            if not session_id or not isinstance(session_id, str):
                raise ValidationError(
                    message="Invalid session_id",
                    cause=None
                )

            result = c_char_p()
            error_code = self._lib.agentos_sys_session_close(
                session_id.encode('utf-8'), ctypes.byref(result)
            )

            self._check_error(error_code, "session_close",
                              {"session_id": session_id})

            with self._managed_c_string(result, "session_close") as result_str:
                success = result_str.lower() == 'true'
                logger.info(f"Session {session_id} closed: {success}")
                return success

    def session_list(self) -> List[str]:
        with self._lock:
            sessions = POINTER(c_char_p)()
            count = c_size_t()

            error_code = self._lib.agentos_sys_session_list(
                ctypes.byref(sessions), ctypes.byref(count)
            )

            self._check_error(error_code, "session_list")

            result = []
            for i in range(count.value):
                if sessions[i]:
                    session_id = sessions[i].decode('utf-8')
                    result.append(session_id)
                    self._free_string(sessions[i])

            return result

    # =========================================================================
    # Skill management
    # =========================================================================

    def skill_load(self, skill_name: str) -> str:
        with self._lock:
            if not skill_name or not isinstance(skill_name, str):
                raise ValidationError(
                    message="Invalid skill_name",
                    cause=None
                )

            result = c_char_p()
            error_code = self._lib.agentos_sys_skill_load(
                skill_name.encode('utf-8'), ctypes.byref(result)
            )

            self._check_error(error_code, "skill_load", {
                              "skill_name": skill_name})

            with self._managed_c_string(result, "skill_load") as result_str:
                logger.info(f"Skill {skill_name} loaded")
                return result_str

    def skill_execute(self, skill_id: str, params: str) -> str:
        with self._lock:
            if not skill_id or not isinstance(skill_id, str):
                raise ValidationError(
                    message="Invalid skill_id",
                    cause=None
                )

            try:
                import json
                json.loads(params)
            except json.JSONDecodeError as e:
                raise ValidationError(
                    message="Invalid skill parameters",
                    cause=e
                )

            result = c_char_p()
            error_code = self._lib.agentos_sys_skill_execute(
                skill_id.encode(
                    'utf-8'), params.encode('utf-8'), len(params), ctypes.byref(result)
            )

            self._check_error(error_code, "skill_execute", {
                "skill_id": skill_id,
                "params_length": len(params)
            })

            with self._managed_c_string(result, "skill_execute") as result_str:
                logger.info(f"Skill {skill_id} executed")
                return result_str

    # =========================================================================
    # Telemetry
    # =========================================================================

    def telemetry_metrics(self) -> str:
        with self._lock:
            metrics = c_char_p()
            error_code = self._lib.agentos_sys_telemetry_metrics(
                ctypes.byref(metrics))

            self._check_error(error_code, "telemetry_metrics")

            with self._managed_c_string(metrics, "telemetry_metrics") as metrics_str:
                logger.debug("Telemetry metrics retrieved")
                return metrics_str

    def telemetry_traces(self) -> str:
        with self._lock:
            traces = c_char_p()
            error_code = self._lib.agentos_sys_telemetry_traces(
                ctypes.byref(traces))

            self._check_error(error_code, "telemetry_traces")

            with self._managed_c_string(traces, "telemetry_traces") as traces_str:
                logger.debug("Telemetry traces retrieved")
                return traces_str


_default_proxy: Optional[SyscallProxy] = None


def get_default_proxy(enable_caching: bool = True) -> SyscallProxy:
    global _default_proxy
    if _default_proxy is None:
        _default_proxy = SyscallProxy(enable_caching=enable_caching)
    return _default_proxy
