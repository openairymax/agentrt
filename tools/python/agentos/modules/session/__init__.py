# AgentOS Python SDK - Session Management Module
# Version: 3.0.0
# Last updated: 2026-03-24

"""
Session management module for AgentOS SDK.

Provides session creation, query, context management, and cleanup operations.

Corresponds to Go SDK: modules/session/manager.go
"""

from .manager import SessionManager

__all__ = ["SessionManager"]
