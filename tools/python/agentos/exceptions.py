# AgentOS Python SDK Exceptions
# Version: 1.0.0.5
# Last updated: 2026-03-21

"""
Exception classes for the AgentOS Python SDK.
"""

class AgentOSError(Exception):
    """Base exception class for all AgentOS errors."""
    pass

class TaskError(AgentOSError):
    """Exception raised for task-related errors."""
    pass

class MemoryError(AgentOSError):
    """Exception raised for memory-related errors."""
    pass

class SessionError(AgentOSError):
    """Exception raised for session-related errors."""
    pass

class SkillError(AgentOSError):
    """Exception raised for skill-related errors."""
    pass

class NetworkError(AgentOSError):
    """Exception raised for network-related errors."""
    pass

class AuthenticationError(AgentOSError):
    """Exception raised for authentication errors."""
    pass

class TimeoutError(AgentOSError):
    """Exception raised for timeout errors."""
    pass
