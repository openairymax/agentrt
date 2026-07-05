# Copyright (c) 2026 SPHARX. All Rights Reserved.
"""
AgentRT Official Hook Base

Defines the common hook interface and lifecycle events that all
official hooks must implement.

Hook lifecycle events (from agentrt.yaml hooks section):
  - on_agent_start    — Fired when an agent starts execution
  - on_agent_end      — Fired when an agent finishes execution
  - on_tool_call      — Fired before/after a tool is invoked
  - on_llm_request    — Fired before sending a request to LLM
  - on_llm_response   — Fired after receiving a response from LLM
  - on_memory_read    — Fired when reading from memory layers
  - on_memory_write   — Fired when writing to memory layers
"""

from __future__ import annotations

from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from datetime import datetime, timezone
from typing import Any, Dict, Optional


@dataclass
class HookContext:
    """Context passed to hook callbacks with event metadata."""
    event: str
    agent_id: str = ""
    session_id: str = ""
    timestamp: str = field(default_factory=lambda: datetime.now(timezone.utc).isoformat())
    metadata: Dict[str, Any] = field(default_factory=dict)


@dataclass
class HookResult:
    """Result returned by a hook. Can modify the pipeline."""
    allowed: bool = True
    modified_data: Optional[Any] = None
    messages: list = field(default_factory=list)
    metadata: Dict[str, Any] = field(default_factory=dict)


class BaseHook(ABC):
    """Abstract base class for all AgentRT hooks.

    Subclasses override only the event handlers they need.
    Returning HookResult(allowed=False) will abort the pipeline.
    """

    name: str = "base_hook"
    version: str = "0.1.0"
    priority: int = 50  # 0=lowest, 100=highest
    enabled: bool = True

    def on_agent_start(self, ctx: HookContext, data: Any = None) -> HookResult:
        """Called when an agent starts execution."""
        return HookResult(allowed=True)

    def on_agent_end(self, ctx: HookContext, data: Any = None) -> HookResult:
        """Called when an agent finishes execution."""
        return HookResult(allowed=True)

    def on_tool_call(self, ctx: HookContext, tool_name: str = "", tool_input: Any = None) -> HookResult:
        """Called before a tool is invoked."""
        return HookResult(allowed=True)

    def on_tool_result(self, ctx: HookContext, tool_name: str = "", result: Any = None) -> HookResult:
        """Called after a tool returns a result."""
        return HookResult(allowed=True)

    def on_llm_request(self, ctx: HookContext, messages: Any = None, model: str = "") -> HookResult:
        """Called before sending a request to LLM."""
        return HookResult(allowed=True)

    def on_llm_response(self, ctx: HookContext, response: Any = None, usage: Optional[Dict] = None) -> HookResult:
        """Called after receiving a response from LLM."""
        return HookResult(allowed=True)

    def on_memory_read(self, ctx: HookContext, key: str = "", layer: str = "") -> HookResult:
        """Called when reading from memory."""
        return HookResult(allowed=True)

    def on_memory_write(self, ctx: HookContext, key: str = "", data: Any = None) -> HookResult:
        """Called when writing to memory."""
        return HookResult(allowed=True)

    def get_hook_info(self) -> Dict[str, Any]:
        """Return hook metadata for registration."""
        return {
            "name": self.name,
            "version": self.version,
            "priority": self.priority,
            "enabled": self.enabled,
            "events": [
                name for name in dir(self)
                if name.startswith("on_") and callable(getattr(self, name))
            ],
        }