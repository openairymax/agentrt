# AgentOS Python SDK - Task Management Module
# Version: 3.0.0
# Last updated: 2026-03-24

"""
Task management module for AgentOS SDK.

Provides task submission, query, wait, cancel, and list operations.

Corresponds to Go SDK: modules/task/manager.go
"""

from .manager import TaskManager

__all__ = ["TaskManager"]
