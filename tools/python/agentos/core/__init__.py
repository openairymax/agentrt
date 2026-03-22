# AgentOS Python SDK - Core Layer
# Version: 2.0.0.0

"""
Core layer providing low-level FFI bindings and syscall proxies.

This package contains:
    - SyscallBinding: Direct ctypes bindings to C API
    - SyscallProxy: High-level proxy with error handling and caching
"""

try:
    from .syscall import SyscallBinding
    from .proxy import SyscallProxy, get_default_proxy

    __all__ = [
        "SyscallBinding",
        "SyscallProxy",
        "get_default_proxy",
    ]
except ImportError:
    # 允许延迟导入
    __all__ = []
