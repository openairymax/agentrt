# AgentRT Python SDK - Session Management Module
# Version: 0.1.0
# Last updated: 2026-03-24

"""
Session management module for AgentRT SDK.

Provides session creation, query, context management, and cleanup operations.

Corresponds to Go SDK: modules/session/manager.go
"""

from .manager import SessionManager

__all__ = ["SessionManager"]
