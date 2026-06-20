# Copyright (c) 2026 SPHARX. All Rights Reserved.
"""
AgentRT Official Hook: Audit Hook

Hooks into on_tool_call and on_agent_end events to write structured
audit logs for compliance and debugging.

Audit log entries include:
  - Timestamp (UTC ISO 8601)
  - Agent ID and session ID
  - Event type (tool_call, tool_result, agent_start, agent_end)
  - Operation details (tool name, input/output summary)
  - Result status (success/error)

Output: JSON lines to configured audit log file.
"""

from __future__ import annotations

import json
import logging
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Optional

from ecosystem.hooks import BaseHook, HookContext, HookResult

logger = logging.getLogger("agentrt.hooks.audit")

DEFAULT_AUDIT_LOG_PATH = "/var/log/agentos/audit.log"


class AuditHook(BaseHook):
    """Listens to tool call events and writes structured audit logs.

    Hook configuration in agentos.yaml:
      hooks:
        global_hooks:
          on_tool_call:
            - hook: "audit"
              priority: 100
              config:
                log_path: "/var/log/agentos/audit.log"
                log_tool_inputs: true
                log_tool_outputs: false
    """

    name = "audit"
    version = "0.1.0"
    priority = 100  # Run first to capture all actions

    def __init__(self, config: Optional[dict] = None):
        self._config = config or {}
        self._log_path = Path(
            self._config.get("log_path", DEFAULT_AUDIT_LOG_PATH)
        )
        self._log_tool_inputs = self._config.get("log_tool_inputs", True)
        self._log_tool_outputs = self._config.get("log_tool_outputs", False)

    def on_agent_start(self, ctx: HookContext, data: Any = None) -> HookResult:
        self._write_audit_entry(ctx, {
            "action": "agent_start",
            "agent_id": ctx.agent_id,
            "session_id": ctx.session_id,
        })
        return HookResult(allowed=True)

    def on_agent_end(self, ctx: HookContext, data: Any = None) -> HookResult:
        self._write_audit_entry(ctx, {
            "action": "agent_end",
            "agent_id": ctx.agent_id,
            "session_id": ctx.session_id,
        })
        return HookResult(allowed=True)

    def on_tool_call(self, ctx: HookContext, tool_name: str = "", tool_input: Any = None) -> HookResult:
        entry = {
            "action": "tool_call",
            "agent_id": ctx.agent_id,
            "tool_name": tool_name,
        }
        if self._log_tool_inputs and tool_input is not None:
            entry["tool_input_summary"] = self._summarize(tool_input)
        self._write_audit_entry(ctx, entry)
        return HookResult(allowed=True)

    def on_tool_result(self, ctx: HookContext, tool_name: str = "", result: Any = None) -> HookResult:
        entry = {
            "action": "tool_result",
            "agent_id": ctx.agent_id,
            "tool_name": tool_name,
            "success": True,
        }
        if self._log_tool_outputs and result is not None:
            entry["result_summary"] = self._summarize(result)
        self._write_audit_entry(ctx, entry)
        return HookResult(allowed=True)

    def _write_audit_entry(self, ctx: HookContext, entry: dict):
        """Write a single audit log entry as JSON line."""
        entry.update({
            "timestamp": ctx.timestamp,
            "event": ctx.event,
        })
        try:
            # Ensure parent directory exists
            self._log_path.parent.mkdir(parents=True, exist_ok=True)
            with open(self._log_path, "a", encoding="utf-8") as f:
                f.write(json.dumps(entry, ensure_ascii=False) + "\n")
        except OSError as e:
            logger.error("Failed to write audit log: %s", e)

    @staticmethod
    def _summarize(data: Any, max_length: int = 200) -> str:
        """Create a safe truncated summary of data for audit log."""
        try:
            text = str(data)
            if len(text) <= max_length:
                return text
            return text[:max_length] + "..."
        except Exception:
            return "<unserializable>"