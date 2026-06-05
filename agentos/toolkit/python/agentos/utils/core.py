# AgentOS Python SDK Utilities - Core Utilities
# Version: 0.1.0

"""
Core utility functions for the AgentOS Python SDK.

This module provides hash generation, environment variable handling,
timing, rate limiting, and input validation.
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
    """Generate a unique identifier using UUID4."""
    unique_id = str(uuid.uuid4())
    return f"{prefix}_{unique_id}" if prefix else unique_id


def generate_timestamp() -> float:
    """Get current Unix timestamp in seconds."""
    return time.time()


@lru_cache(maxsize=2048)
def generate_hash(data: Union[str, bytes]) -> str:
    """Generate SHA256 hash of data with LRU caching."""
    if isinstance(data, str):
        data = data.encode('utf-8')
    return hashlib.sha256(data).hexdigest()


def validate_json(data: Any) -> bool:
    """Validate if data can be serialized as JSON."""
    try:
        json.dumps(data)
        return True
    except (TypeError, ValueError):
        return False


def sanitize_string(s: str, max_length: int = 1000) -> str:
    """Sanitize a string by removing control characters and limiting length."""
    sanitized = ''.join(
        char for char in s
        if ord(char) >= 32 or char in '\n\r\t'
    )
    if len(sanitized) > max_length:
        sanitized = sanitized[:max_length] + "..."
    return sanitized


def retry_with_backoff(
    max_retries: int = 3,
    base_delay: float = 1.0,
    max_delay: float = 60.0,
    exceptions: tuple = (Exception,)
):
    """Decorator for retrying a function with exponential backoff."""
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
    """Get an environment variable with optional validation."""
    value = os.environ.get(name)
    if value is None:
        if required:
            raise ValueError(
                f"Required environment variable '{name}' is not set")
        return default
    return value


def parse_timeout(timeout: Union[int, float, str], default: int = 30) -> int:
    """Parse timeout value from various formats."""
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
    """Recursively merge two dictionaries."""
    result = base.copy()
    for key, value in override.items():
        if key in result and isinstance(result[key], dict) and isinstance(value, dict):
            result[key] = merge_dicts(result[key], value)
        else:
            result[key] = value
    return result


class Timer:
    """High-precision context manager for timing code execution."""

    def __init__(self, name: str = "Operation"):
        self.name = name
        self.start_time: Optional[float] = None
        self.end_time: Optional[float] = None
        self.elapsed: Optional[float] = None
        self.elapsed_ns: Optional[int] = None

    def start(self):
        self.start_time = timeit.default_timer()

    def stop(self):
        if self.start_time is None:
            raise RuntimeError("Timer was not started")
        end_time = timeit.default_timer()
        self.end_time = end_time
        self.elapsed = end_time - self.start_time
        self.elapsed_ns = int(self.elapsed * 1e9)

    def __enter__(self):
        self.start()
        return self

    def __exit__(self, *args):
        self.stop()
        logger.debug(
            f"{self.name} took {self.elapsed:.6f}s ({self.elapsed_ns} ns)")

    def elapsed_ms(self) -> Optional[float]:
        return self.elapsed * 1000 if self.elapsed else None

    def elapsed_us(self) -> Optional[float]:
        return self.elapsed * 1e6 if self.elapsed else None


class RateLimiter:
    """Simple rate limiter using token bucket algorithm."""

    def __init__(self, rate: float, capacity: int):
        self.rate = rate
        self.capacity = capacity
        self.tokens = capacity
        self.last_update = time.time()

    def acquire(self, tokens: int = 1) -> bool:
        now = time.time()
        elapsed = now - self.last_update
        self.last_update = now
        self.tokens = min(self.capacity, self.tokens + elapsed * self.rate)
        if self.tokens >= tokens:
            self.tokens -= tokens
            return True
        return False

    def wait_for_token(self, tokens: int = 1, timeout: Optional[float] = None) -> bool:
        start_time = time.time()
        while not self.acquire(tokens):
            if timeout and (time.time() - start_time) >= timeout:
                return False
            time.sleep(0.1)
        return True