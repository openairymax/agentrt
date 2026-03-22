# AgentOS Python SDK - Session Module
# Version: 2.0.0.0

"""
Session management module.

Provides functionality for:
    - Creating and managing sessions
    - Setting session context
    - Session lifecycle management
"""

try:
    from .manager import SessionManager
    from .models import SessionInfo
    from .errors import SessionError

    __all__ = [
        "SessionManager",
        "SessionInfo",
        "SessionError",
    ]
except ImportError:
    __all__ = []
