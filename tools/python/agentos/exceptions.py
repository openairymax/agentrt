# AgentOS Python SDK Exceptions
# Version: 1.0.0.6
# Last updated: 2026-03-21

"""
Exception classes for the AgentOS Python SDK.

This module defines a comprehensive exception hierarchy for all AgentOS operations.
All exceptions inherit from AgentOSError and include rich error information.
"""

from typing import Optional, Any, Dict


class AgentOSError(Exception):
    """
    Base exception class for all AgentOS errors.
    
    Attributes:
        error_code (int): The numeric error code from the C API
        message (str): Human-readable error description
        details (Optional[Dict]): Additional error context
    """
    
    def __init__(self, error_code: int = -1, message: str = "", details: Optional[Dict[str, Any]] = None):
        self.error_code = error_code
        self.message = message
        self.details = details or {}
        super().__init__(self._format_message())
    
    def _format_message(self) -> str:
        """Format the error message with error code and details."""
        base_msg = f"[Error {self.error_code}] {self.message}" if self.error_code >= 0 else self.message
        
        if self.details:
            details_str = ", ".join(f"{k}={v}" for k, v in self.details.items())
            return f"{base_msg} ({details_str})"
        
        return base_msg
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert exception to dictionary for serialization."""
        return {
            "error_code": self.error_code,
            "message": self.message,
            "details": self.details,
            "type": self.__class__.__name__
        }


class TaskError(AgentOSError):
    """Exception raised for task-related errors."""
    pass


class MemoryError(AgentOSError):
    """Exception raised for memory operation errors."""
    pass


class SessionError(AgentOSError):
    """Exception raised for session management errors."""
    pass


class SkillError(AgentOSError):
    """Exception raised for skill loading and execution errors."""
    pass


class TelemetryError(AgentOSError):
    """Exception raised for telemetry and observability errors."""
    pass


class InitializationError(AgentOSError):
    """Exception raised when SDK initialization fails."""
    pass


class TimeoutError(AgentOSError):
    """
    Exception raised when an operation times out.
    
    Attributes:
        timeout_ms (int): The timeout value that was exceeded
        operation (str): The operation that timed out
    """
    
    def __init__(self, timeout_ms: int, operation: str = "operation", **kwargs):
        super().__init__(message=f"{operation.capitalize()} timed out after {timeout_ms}ms", **kwargs)
        self.timeout_ms = timeout_ms
        self.operation = operation


class ValidationError(AgentOSError):
    """Exception raised when input validation fails."""
    pass


class NetworkError(AgentOSError):
    """Exception raised for network-related errors."""
    pass


class AuthenticationError(AgentOSError):
    """Exception raised for authentication errors."""
    pass
