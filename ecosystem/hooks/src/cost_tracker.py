# Copyright (c) 2026 SPHARX. All Rights Reserved.
"""
AgentRT Official Hook: Cost Tracker

Hooks into on_llm_response events to track token usage and compute
real-time API costs. Supports multiple provider pricing models.

Tracks per-session:
  - Input tokens consumed
  - Output tokens consumed
  - Estimated cost (USD) based on provider pricing table
  - Cumulative session cost

Pricing table is configurable and covers major providers:
  OpenAI, Anthropic, Google, DeepSeek, local (free).

Hook configuration in agentrt.yaml:
  hooks:
    global_hooks:
      on_llm_response:
        - hook: "cost_tracker"
          priority: 50
          config:
            daily_budget_usd: 10.0
            alert_threshold_percent: 80
"""

from __future__ import annotations

import json
import logging
from collections import defaultdict
from pathlib import Path
from typing import Any, Dict, Optional

from ecosystem.hooks import BaseHook, HookContext, HookResult

logger = logging.getLogger("agentrt.hooks.cost_tracker")

# Provider pricing per 1K tokens (USD)
DEFAULT_PRICING = {
    "openai": {
        "gpt-4o": {"input": 0.005, "output": 0.015},
        "gpt-4o-mini": {"input": 0.00015, "output": 0.0006},
        "gpt-4-turbo": {"input": 0.01, "output": 0.03},
        "gpt-3.5-turbo": {"input": 0.0005, "output": 0.0015},
        "default": {"input": 0.005, "output": 0.015},
    },
    "anthropic": {
        "claude-sonnet-4-20250514": {"input": 0.003, "output": 0.015},
        "claude-3-opus": {"input": 0.015, "output": 0.075},
        "claude-3-haiku": {"input": 0.00025, "output": 0.00125},
        "default": {"input": 0.003, "output": 0.015},
    },
    "google": {
        "gemini-2.5-pro": {"input": 0.0035, "output": 0.0105},
        "gemini-2.5-flash": {"input": 0.00015, "output": 0.0006},
        "default": {"input": 0.0035, "output": 0.0105},
    },
    "deepseek": {
        "deepseek-chat": {"input": 0.00027, "output": 0.0011},
        "deepseek-reasoner": {"input": 0.00055, "output": 0.00219},
        "default": {"input": 0.00027, "output": 0.0011},
    },
    "local": {
        "default": {"input": 0.0, "output": 0.0},
    },
}


class CostTrackerHook(BaseHook):
    """Tracks LLM token usage and cost in real-time.

    Monitors on_llm_response events, extracts token usage from
    provider responses, and computes estimated costs.
    """

    name = "cost_tracker"
    version = "0.1.0"
    priority = 50

    def __init__(self, config: Optional[dict] = None):
        self._config = config or {}
        self._pricing = self._config.get("pricing", DEFAULT_PRICING)
        self._daily_budget_usd = self._config.get("daily_budget_usd", 10.0)
        self._alert_threshold = self._config.get("alert_threshold_percent", 80)

        # Per-session tracking
        self._session_input_tokens: Dict[str, int] = defaultdict(int)
        self._session_output_tokens: Dict[str, int] = defaultdict(int)
        self._session_cost: Dict[str, float] = defaultdict(float)
        self._alerts_sent: Dict[str, bool] = defaultdict(bool)

    def on_llm_response(
        self,
        ctx: HookContext,
        response: Any = None,
        usage: Optional[Dict] = None,
    ) -> HookResult:
        """Extract token usage from LLM response and update cost tracking."""
        session_id = ctx.session_id

        # Extract usage from response
        input_tokens = 0
        output_tokens = 0
        model = ctx.metadata.get("model", "unknown")
        provider = ctx.metadata.get("provider", "unknown")

        if usage:
            input_tokens = usage.get("input_tokens", 0) or usage.get("prompt_tokens", 0)
            output_tokens = usage.get("output_tokens", 0) or usage.get("completion_tokens", 0)
            model = usage.get("model", model)
        elif hasattr(response, "usage"):
            input_tokens = getattr(response.usage, "prompt_tokens", 0)
            output_tokens = getattr(response.usage, "completion_tokens", 0)
            model = getattr(response, "model", model)

        # Accumulate
        self._session_input_tokens[session_id] += input_tokens
        self._session_output_tokens[session_id] += output_tokens

        # Compute cost
        cost = self._compute_cost(provider, model, input_tokens, output_tokens)
        self._session_cost[session_id] += cost

        # Check budget threshold
        total_cost = self._session_cost[session_id]
        if total_cost >= self._daily_budget_usd * (self._alert_threshold / 100.0):
            if not self._alerts_sent[session_id]:
                logger.warning(
                    "Cost alert: session %s reached %.4f USD "
                    "(%d%% of daily budget %.2f USD)",
                    session_id, total_cost, self._alert_threshold,
                    self._daily_budget_usd,
                )
                self._alerts_sent[session_id] = True

        # Attach cost metadata to hook result
        return HookResult(
            allowed=True,
            metadata={
                "cost_usd": round(cost, 6),
                "session_total_cost_usd": round(total_cost, 6),
                "input_tokens": input_tokens,
                "output_tokens": output_tokens,
                "session_total_input_tokens": self._session_input_tokens[session_id],
                "session_total_output_tokens": self._session_output_tokens[session_id],
            },
        )

    def _compute_cost(
        self,
        provider: str,
        model: str,
        input_tokens: int,
        output_tokens: int,
    ) -> float:
        """Compute cost in USD based on provider pricing table."""
        provider_pricing = self._pricing.get(provider, {})
        model_pricing = provider_pricing.get(model, provider_pricing.get("default", {}))

        if not model_pricing:
            # Fallback: try default for provider
            model_pricing = provider_pricing.get("default", {"input": 0.0, "output": 0.0})

        input_cost = (input_tokens / 1000.0) * model_pricing.get("input", 0.0)
        output_cost = (output_tokens / 1000.0) * model_pricing.get("output", 0.0)

        return input_cost + output_cost

    def get_session_stats(self, session_id: str) -> Dict[str, Any]:
        """Get cumulative statistics for a session."""
        return {
            "session_id": session_id,
            "total_input_tokens": self._session_input_tokens.get(session_id, 0),
            "total_output_tokens": self._session_output_tokens.get(session_id, 0),
            "total_cost_usd": round(self._session_cost.get(session_id, 0.0), 6),
            "daily_budget_usd": self._daily_budget_usd,
            "budget_remaining_usd": round(
                self._daily_budget_usd - self._session_cost.get(session_id, 0.0), 6
            ),
        }

    def reset_session(self, session_id: str):
        """Reset tracking for a session."""
        self._session_input_tokens.pop(session_id, None)
        self._session_output_tokens.pop(session_id, None)
        self._session_cost.pop(session_id, None)
        self._alerts_sent.pop(session_id, None)