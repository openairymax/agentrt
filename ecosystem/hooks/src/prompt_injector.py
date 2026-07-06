# Copyright (c) 2026 SPHARX. All Rights Reserved.
"""
AgentRT Official Hook: Prompt Injector

Hooks into on_llm_request events to inject custom prompt fragments into
the LLM request message list. Enables dynamic prompt augmentation based
on agent context, session state, and task requirements.

Injection capabilities:
  - Named fragments with enable/disable toggles
  - Rule-based conditional injection (agent type, task type, regex match)
  - Variable substitution ({agent_name}, {session_id}, {timestamp}, etc.)
  - Positional injection: before/after system message, before user message
  - Priority-based fragment ordering for deterministic output

Hook configuration in agentrt.yaml:
  hooks:
    global_hooks:
      on_llm_request:
        - hook: "prompt_injector"
          priority: 60
          config:
            fragments:
              - name: "context_reminder"
                enabled: true
                position: "after_system"   # before_system | after_system | before_user
                priority: 10
                text: |
                  Current context:
                  - Agent: {agent_name}
                  - Session: {session_id}
                  - Time: {timestamp}
              - name: "output_format"
                enabled: true
                position: "after_system"
                priority: 20
                text: |
                  Always format your responses in valid Markdown.
                  Use code blocks with language identifiers for code.
              - name: "task_reminder"
                enabled: false
                position: "before_user"
                rules:
                  agent_type: "code_review"
                text: |
                  Remember to check for:
                  1. Security vulnerabilities
                  2. Performance issues
                  3. Code style violations
            default_position: "after_system"
"""

from __future__ import annotations

import logging
import re
from datetime import datetime, timezone
from typing import Any, Dict, List, Optional

from ecosystem.hooks import BaseHook, HookContext, HookResult

logger = logging.getLogger("agentrt.hooks.prompt_injector")

DEFAULT_FRAGMENT_POSITION = "after_system"

# Supported variable substitutions in fragment text
VARIABLE_PATTERN = re.compile(r"\{(\w+)\}")


class PromptInjectorHook(BaseHook):
    """Injects custom prompt fragments into LLM request messages.

    Supports named fragments with conditional injection rules,
    positional control, and variable substitution. Fragments are
    injected in priority order for deterministic behavior.
    """

    name = "prompt_injector"
    version = "0.1.0"
    priority = 60  # Run after security reminder but before formatting hooks

    def __init__(self, config: Optional[dict] = None):
        self._config = config or {}
        self._default_position = self._config.get(
            "default_position", DEFAULT_FRAGMENT_POSITION
        )
        self._fragments: List[dict] = self._config.get("fragments", [])

    def on_llm_request(
        self,
        ctx: HookContext,
        messages: Any = None,
        model: str = "",
    ) -> HookResult:
        """Inject configured prompt fragments into the LLM request.

        Evaluates all enabled fragments against their injection rules,
        performs variable substitution, and returns the ordered list
        of fragments to inject.

        Args:
            ctx: Hook context with agent and session metadata.
            messages: The current list of messages to be sent to the LLM.
            model: The target LLM model name.

        Returns:
            HookResult with modified_data containing injection instructions.
        """
        if not self._fragments:
            logger.debug("No prompt fragments configured, skipping injection")
            return HookResult(allowed=True)

        # Collect active fragments (enabled + rules pass)
        active = self._collect_active_fragments(ctx)

        if not active:
            logger.debug("No active fragments after rule evaluation")
            return HookResult(allowed=True)

        # Sort by priority (lowest first for insertion order)
        active.sort(key=lambda f: f.get("priority", 50))

        # Perform variable substitution on each fragment
        resolved = []
        for frag in active:
            text = self._substitute_vars(frag.get("text", ""), ctx, model)
            resolved.append({
                "name": frag.get("name", "unnamed"),
                "text": text,
                "position": frag.get("position", self._default_position),
                "priority": frag.get("priority", 50),
            })

        logger.info(
            "Injecting %d prompt fragments for agent %s (model=%s)",
            len(resolved),
            ctx.agent_id,
            model,
        )

        return HookResult(
            allowed=True,
            modified_data={
                "fragments": resolved,
                "fragment_count": len(resolved),
            },
            metadata={
                "injected_fragments": [f["name"] for f in resolved],
                "total_configured": len(self._fragments),
                "model": model,
            },
        )

    def _collect_active_fragments(self, ctx: HookContext) -> List[dict]:
        """Collect and filter fragments that are enabled and pass rules."""
        active = []
        for frag in self._fragments:
            if not frag.get("enabled", True):
                continue

            if not self._evaluate_rules(frag.get("rules", {}), ctx):
                continue

            active.append(frag)

        return active

    def _evaluate_rules(
        self, rules: Dict[str, Any], ctx: HookContext
    ) -> bool:
        """Evaluate injection rules against the current context.

        Supported rule types:
          - agent_type: Exact match against ctx.metadata["agent_type"]
          - task_type: Exact match against ctx.metadata["task_type"]
          - metadata_match: Dict of key-value pairs to match in ctx.metadata
          - regex: Compiled regex pattern to match against a metadata field
        """
        if not rules:
            return True

        metadata = ctx.metadata or {}

        # agent_type rule
        if "agent_type" in rules:
            expected = rules["agent_type"]
            actual = metadata.get("agent_type", "")
            if actual != expected:
                logger.debug(
                    "Fragment skipped: agent_type mismatch "
                    "(expected=%s, actual=%s)",
                    expected, actual,
                )
                return False

        # task_type rule
        if "task_type" in rules:
            expected = rules["task_type"]
            actual = metadata.get("task_type", "")
            if actual != expected:
                logger.debug(
                    "Fragment skipped: task_type mismatch "
                    "(expected=%s, actual=%s)",
                    expected, actual,
                )
                return False

        # metadata_match rule (dict of key-value pairs)
        if "metadata_match" in rules:
            for key, expected in rules["metadata_match"].items():
                actual = metadata.get(key)
                if actual != expected:
                    logger.debug(
                        "Fragment skipped: metadata.%s mismatch "
                        "(expected=%s, actual=%s)",
                        key, expected, actual,
                    )
                    return False

        # regex rule: {"field": "user_input", "pattern": ".*error.*"}
        if "regex" in rules:
            regex_rule = rules["regex"]
            field = regex_rule.get("field", "")
            pattern = regex_rule.get("pattern", "")
            value = str(metadata.get(field, ""))
            try:
                if not re.search(pattern, value):
                    logger.debug(
                        "Fragment skipped: regex mismatch "
                        "(field=%s, pattern=%s)",
                        field, pattern,
                    )
                    return False
            except re.error as e:
                logger.warning(
                    "Invalid regex pattern in fragment rule: %s", e
                )
                return False

        return True

    def _substitute_vars(
        self,
        text: str,
        ctx: HookContext,
        model: str = "",
    ) -> str:
        """Substitute {variable} placeholders with actual values.

        Supported variables:
          - {agent_name}: ctx.agent_id
          - {agent_id}: ctx.agent_id
          - {session_id}: ctx.session_id
          - {timestamp}: Current UTC ISO 8601 timestamp
          - {date}: Current date (YYYY-MM-DD)
          - {time}: Current time (HH:MM:SS)
          - {model}: The target LLM model name
          - {event}: The event name from ctx
          - Any key from ctx.metadata (e.g., {task_type})
        """
        now = datetime.now(timezone.utc)

        # Build variable lookup table
        variables: Dict[str, str] = {
            "agent_name": ctx.agent_id,
            "agent_id": ctx.agent_id,
            "session_id": ctx.session_id,
            "timestamp": now.isoformat(),
            "date": now.strftime("%Y-%m-%d"),
            "time": now.strftime("%H:%M:%S"),
            "model": model,
            "event": ctx.event,
        }

        # Merge metadata variables (override defaults with caution)
        for key, value in (ctx.metadata or {}).items():
            if key not in variables and isinstance(value, (str, int, float, bool)):
                variables[key] = str(value)

        def replacer(match: re.Match) -> str:
            var_name = match.group(1)
            if var_name in variables:
                return variables[var_name]
            logger.debug("Unknown variable {%s} in fragment text", var_name)
            return match.group(0)  # Keep original placeholder

        return VARIABLE_PATTERN.sub(replacer, text)

    def list_fragments(self) -> List[Dict[str, Any]]:
        """List all configured fragments with their status."""
        return [
            {
                "name": f.get("name", "unnamed"),
                "enabled": f.get("enabled", True),
                "position": f.get("position", self._default_position),
                "priority": f.get("priority", 50),
                "has_rules": bool(f.get("rules")),
                "text_preview": (
                    f.get("text", "")[:80] + "..."
                    if len(f.get("text", "")) > 80
                    else f.get("text", "")
                ),
            }
            for f in self._fragments
        ]

    def enable_fragment(self, name: str) -> bool:
        """Enable a fragment by name. Returns True if found."""
        for frag in self._fragments:
            if frag.get("name") == name:
                frag["enabled"] = True
                return True
        return False

    def disable_fragment(self, name: str) -> bool:
        """Disable a fragment by name. Returns True if found."""
        for frag in self._fragments:
            if frag.get("name") == name:
                frag["enabled"] = False
                return True
        return False