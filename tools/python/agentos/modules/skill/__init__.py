# AgentOS Python SDK - Skill Management Module
# Version: 3.0.0
# Last updated: 2026-03-24

"""
Skill management module for AgentOS SDK.

Provides skill registration, loading, execution, unloading, and validation operations.

Corresponds to Go SDK: modules/skill/manager.go
"""

from .manager import SkillManager, SkillExecuteRequest

__all__ = ["SkillManager", "SkillExecuteRequest"]
