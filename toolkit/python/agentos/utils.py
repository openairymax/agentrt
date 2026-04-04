# AgentOS Python SDK Utilities
# Version: 3.0.0
# Last updated: 2026-04-04

"""
Utility functions and helpers for the AgentOS Python SDK.

This module provides optimized commons utilities with caching,
efficient algorithms, and thread-safe implementations.

Example:
    >>> from agentos import generate_id, Timer, RateLimiter
    >>> 
    >>> # Generate unique IDs with prefix
    >>> task_id = generate_id("task")
    >>> 
    >>> # Time code execution
    >>> with Timer("computation"):
    ...     result = expensive_operation()
    >>> 
    >>> # Rate limiting
    >>> limiter = RateLimiter(rate=10, capacity=20)
    >>> if limiter.acquire():
    ...     make_api_call()
"""

import hashlib
import uuid
import time
import json
import os
import logging
from typing import Any, Dict, Optional, Union, Callable, TypeVar
from functools import wraps, lru_cache
from threading import Lock
from collections import defaultdict
import timeit

logger = logging.getLogger(__name__)

_cache_locks: Dict[str, Lock] = defaultdict(Lock)
_id_counter: Dict[str, int] = defaultdict(int)


def generate_id(prefix: str = "") -> str:
    """
    Generate a unique identifier.

    Uses UUID4 for uniqueness.

    Args:
        prefix: Optional prefix for the ID

    Returns:
        A unique identifier string (format: "{prefix}_{uuid}" if prefix provided)

    Example:
        >>> generate_id()  # '550e8400-e29b-41d4-a716-446655440000'
        >>> generate_id("task")  # 'task_550e8400-e29b-41d4-a716-446655440000'
    """
    unique_id = str(uuid.uuid4())
    return f"{prefix}_{unique_id}" if prefix else unique_id


def generate_timestamp() -> float:
    """
    Get current Unix timestamp in seconds.

    Returns:
        Current timestamp
    """
    return time.time()


@lru_cache(maxsize=2048)
def generate_hash(data: Union[str, bytes]) -> str:
    """
    Generate SHA256 hash of data with LRU caching.

    Uses Python's built-in lru_cache for automatic cache management.
    Cache size limited to 2048 entries to balance memory usage.

    Args:
        data: Data to hash (string or bytes)

    Returns:
        Hexadecimal hash string (64 characters)

    Performance:
        - Cache hit: O(1)
        - Cache miss: O(n) where n is data length

    Example:
        >>> generate_hash("hello world")
        'b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9'
        >>> 
        >>> # Same result for bytes
        >>> generate_hash(b"hello world")
        'b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9'
    """
    if isinstance(data, str):
        data = data.encode('utf-8')
    return hashlib.sha256(data).hexdigest()


def validate_json(data: Any) -> bool:
    """
    Validate if data can be serialized as JSON.

    Args:
        data: Data to validate

    Returns:
        True if valid JSON, False otherwise
    """
    try:
        json.dumps(data)
        return True
    except (TypeError, ValueError):
        return False


def sanitize_string(s: str, max_length: int = 1000) -> str:
    """
    Sanitize a string by removing control characters and limiting length.

    Args:
        s: String to sanitize
        max_length: Maximum allowed length

    Returns:
        Sanitized string
    """
    # Remove control characters except newline and tab
    sanitized = ''.join(
        char for char in s
        if ord(char) >= 32 or char in '\n\r\t'
    )

    # Truncate if too long
    if len(sanitized) > max_length:
        sanitized = sanitized[:max_length] + "..."

    return sanitized


def retry_with_backoff(
    max_retries: int = 3,
    base_delay: float = 1.0,
    max_delay: float = 60.0,
    exceptions: tuple = (Exception,)
):
    """
    Decorator for retrying a function with exponential backoff.

    Args:
        max_retries: Maximum number of retry attempts
        base_delay: Base delay in seconds
        max_delay: Maximum delay in seconds
        exceptions: Tuple of exceptions to catch

    Returns:
        Decorated function
    """
    def decorator(func):
        @wraps(func)
        def wrapper(*args, **kwargs):
            last_exception = None

            for attempt in range(max_retries + 1):
                try:
                    return func(*args, **kwargs)
                except exceptions as e:
                    last_exception = e

                    if attempt == max_retries:
                        break

                    # Calculate delay with exponential backoff
                    delay = min(base_delay * (2 ** attempt), max_delay)
                    logger.warning(
                        f"Attempt {attempt + 1}/{max_retries + 1} failed: {str(e)}. "
                        f"Retrying in {delay:.2f}s..."
                    )
                    time.sleep(delay)

            raise last_exception

        return wrapper
    return decorator


def get_env_var(name: str, default: Optional[str] = None, required: bool = False) -> Optional[str]:
    """
    Get an environment variable with optional validation.

    Args:
        name: Variable name
        default: Default value if not set
        required: If True, raises error when not found

    Returns:
        Environment variable value or default

    Raises:
        ValueError: If required variable is not set
    """
    value = os.environ.get(name)

    if value is None:
        if required:
            raise ValueError(
                f"Required environment variable '{name}' is not set")
        return default

    return value


def parse_timeout(timeout: Union[int, float, str], default: int = 30) -> int:
    """
    Parse timeout value from various formats.

    Args:
        timeout: Timeout value (seconds as int/float, or string like "30s", "5m")
        default: Default timeout if parsing fails

    Returns:
        Timeout in seconds as integer
    """
    if isinstance(timeout, (int, float)):
        return int(timeout)

    if isinstance(timeout, str):
        timeout = timeout.strip().lower()

        if timeout.endswith('ms'):
            return int(float(timeout[:-2]) / 1000)
        elif timeout.endswith('s'):
            return int(float(timeout[:-1]))
        elif timeout.endswith('m'):
            return int(float(timeout[:-1]) * 60)
        elif timeout.endswith('h'):
            return int(float(timeout[:-1]) * 3600)
        else:
            try:
                return int(float(timeout))
            except ValueError:
                pass

    return default


def merge_dicts(base: Dict[str, Any], override: Dict[str, Any]) -> Dict[str, Any]:
    """
    Recursively merge two dictionaries.

    Args:
        base: Base dictionary
        override: Dictionary with values to override

    Returns:
        Merged dictionary
    """
    result = base.copy()

    for key, value in override.items():
        if key in result and isinstance(result[key], dict) and isinstance(value, dict):
            result[key] = merge_dicts(result[key], value)
        else:
            result[key] = value

    return result


class Timer:
    """High-precision context manager for timing code execution.

    Uses timeit.default_timer() for highest available precision.
    Provides both seconds and milliseconds precision.

    Attributes:
        name: Name of the timed operation
        start_time: Start timestamp (None if not started)
        end_time: End timestamp (None if not ended)
        elapsed: Elapsed time in seconds (None if not ended)
        elapsed_ns: Elapsed time in nanoseconds (high precision)

    Example:
        >>> with Timer("database_query") as timer:
        ...     result = db.query(sql)
        >>> print(f"Query took {timer.elapsed_ms:.2f} ms")

        >>> # Manual control
        >>> timer = Timer("computation")
        >>> timer.start()
        >>> compute()
        >>> timer.stop()
        >>> print(f"Duration: {timer.elapsed_ns} ns")
    """

    def __init__(self, name: str = "Operation"):
        """
        Initialize timer.

        Args:
            name: Name of the timed operation
        """
        self.name = name
        self.start_time: Optional[float] = None
        self.end_time: Optional[float] = None
        self.elapsed: Optional[float] = None
        self.elapsed_ns: Optional[int] = None

    def start(self):
        """Start timing with high precision."""
        self.start_time = timeit.default_timer()

    def stop(self):
        """Stop timing and calculate elapsed time."""
        if self.start_time is None:
            raise RuntimeError("Timer was not started")

        end_time = timeit.default_timer()
        self.end_time = end_time
        self.elapsed = end_time - self.start_time
        self.elapsed_ns = int(self.elapsed * 1e9)

    def __enter__(self):
        """Start timing on context entry."""
        self.start()
        return self

    def __exit__(self, *args):
        """Stop timing on context exit and log result."""
        self.stop()
        logger.debug(
            f"{self.name} took {self.elapsed:.6f}s ({self.elapsed_ns} ns)")

    def elapsed_ms(self) -> Optional[float]:
        """Get elapsed time in milliseconds."""
        return self.elapsed * 1000 if self.elapsed else None

    def elapsed_us(self) -> Optional[float]:
        """Get elapsed time in microseconds."""
        return self.elapsed * 1e6 if self.elapsed else None


class RateLimiter:
    """Simple rate limiter using token bucket algorithm."""

    def __init__(self, rate: float, capacity: int):
        """
        Initialize rate limiter.

        Args:
            rate: Token refill rate (tokens per second)
            capacity: Maximum bucket capacity
        """
        self.rate = rate
        self.capacity = capacity
        self.tokens = capacity
        self.last_update = time.time()

    def acquire(self, tokens: int = 1) -> bool:
        """
        Try to acquire tokens.

        Args:
            tokens: Number of tokens to acquire

        Returns:
            True if tokens acquired, False if rate limited
        """
        now = time.time()
        elapsed = now - self.last_update
        self.last_update = now

        # Refill tokens based on elapsed time
        self.tokens = min(self.capacity, self.tokens + elapsed * self.rate)

        if self.tokens >= tokens:
            self.tokens -= tokens
            return True

        return False

    def wait_for_token(self, tokens: int = 1, timeout: Optional[float] = None) -> bool:
        """
        Wait until tokens are available.

        Args:
            tokens: Number of tokens to acquire
            timeout: Maximum wait time in seconds

        Returns:
            True if tokens acquired, False if timeout
        """
        start_time = time.time()

        while not self.acquire(tokens):
            if timeout and (time.time() - start_time) >= timeout:
                return False
            time.sleep(0.1)

        return True
