# Copyright (c) 2026 SPHARX. All Rights Reserved.
"""
AgentRT OpenLab: Market Error Mapping

Maps market_d daemon error codes (from market_service.h / main.c) to
Python exception classes for clean error handling in OpenLab Markets.

Error code reference from agentrt/daemons/market_d/src/main.c:
  - MARKET_ERR_INVALID_PARAM   = AGENTRT_ERR_INVALID_PARAM
  - MARKET_ERR_OUT_OF_MEMORY   = AGENTRT_ERR_OUT_OF_MEMORY
  - MARKET_ERR_NOT_FOUND       = AGENTRT_ERR_NOT_FOUND
  - MARKET_ERR_ALREADY_EXISTS  = AGENTRT_ERR_DAEMON_BASE + 0x20
  - MARKET_ERR_INSTALL_FAIL    = AGENTRT_ERR_DAEMON_BASE + 0x21
"""

from __future__ import annotations


# JSON-RPC standard error codes
JSONRPC_PARSE_ERROR = -32700
JSONRPC_INVALID_REQUEST = -32600
JSONRPC_METHOD_NOT_FOUND = -32601
JSONRPC_INVALID_PARAMS = -32602
JSONRPC_INTERNAL_ERROR = -32603

# Market daemon specific error codes
MARKET_ERR_INVALID_PARAM = -1000
MARKET_ERR_OUT_OF_MEMORY = -1001
MARKET_ERR_NOT_FOUND = -1002
MARKET_ERR_ALREADY_EXISTS = -1003
MARKET_ERR_INSTALL_FAIL = -1004


class MarketError(Exception):
    """Base exception for all market_d communication errors."""

    def __init__(self, message: str = "", code: int = 0, details: str = ""):
        super().__init__(message)
        self.message = message
        self.code = code
        self.details = details

    def __str__(self):
        if self.code:
            return f"MarketError[{self.code}]: {self.message}"
        return f"MarketError: {self.message}"


class MarketConnectionError(MarketError):
    """Raised when unable to connect to market_d daemon."""
    pass


class MarketNotFoundError(MarketError):
    """Raised when an Agent or Skill is not found in the market."""
    pass


class MarketAlreadyExistsError(MarketError):
    """Raised when registering an Agent/Skill that already exists."""
    pass


class MarketInstallError(MarketError):
    """Raised when an installation operation fails."""
    pass


class MarketValidationError(MarketError):
    """Raised when request parameters fail validation."""
    pass


# Error code → exception class mapping
_ERROR_CODE_MAP = {
    MARKET_ERR_INVALID_PARAM: MarketValidationError,
    MARKET_ERR_OUT_OF_MEMORY: MarketError,
    MARKET_ERR_NOT_FOUND: MarketNotFoundError,
    MARKET_ERR_ALREADY_EXISTS: MarketAlreadyExistsError,
    MARKET_ERR_INSTALL_FAIL: MarketInstallError,
    JSONRPC_PARSE_ERROR: MarketValidationError,
    JSONRPC_INVALID_REQUEST: MarketValidationError,
    JSONRPC_METHOD_NOT_FOUND: MarketError,
    JSONRPC_INVALID_PARAMS: MarketValidationError,
    JSONRPC_INTERNAL_ERROR: MarketError,
}


def market_error_from_code(code: int, message: str = "", details: str = "") -> MarketError:
    """Create the appropriate MarketError subclass from a market_d error code.

    Args:
        code: Error code from market_d (can be JSON-RPC or daemon-specific).
        message: Human-readable error message.
        details: Additional error details.

    Returns:
        Appropriate MarketError subclass instance.
    """
    exception_cls = _ERROR_CODE_MAP.get(code, MarketError)
    return exception_cls(message=message, code=code, details=details)