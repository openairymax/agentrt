"""support_hooks.py — 客服 Agent Hook 集合

包含三个生产级 Hook：
1. SecurityReminderHook — 安全提醒，防止敏感信息泄露
2. AuditHook — 审计记录，满足合规要求
3. CostTrackerHook — 成本追踪，防止超支
"""

from __future__ import annotations

import json
import logging
import time
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List, Optional

import sys as _sys
from pathlib import Path as _Path

# 确保 agentos 包可导入（开发模式）
_sdk_python = _Path(__file__).resolve().parents[3] / "sdk" / "python"
if str(_sdk_python) not in _sys.path:
    _sys.path.insert(0, str(_sdk_python))

from agentos.plugin_types import HookPlugin

logger = logging.getLogger(__name__)

# 敏感字段列表
_SENSITIVE_FIELDS = {"password", "token", "api_key", "secret", "credit_card", "ssn"}
# 敏感信息正则模式
_SENSITIVE_PATTERNS = [
    r'\b\d{16}\b',          # 信用卡号
    r'\b\d{3}-\d{2}-\d{4}\b',  # SSN
    r'Bearer\s+[A-Za-z0-9\-._~+/]+=*',  # Bearer Token
]


class SecurityReminderHook(HookPlugin):
    """安全提醒 Hook。

    在工具调用前检查安全性，阻止敏感信息泄露。
    优先级最高（100），确保在所有其他 Hook 之前执行。
    """

    PLUGIN_TYPE = "hook"

    def on_tool_call(self, ctx: Any, tool_name: str = "", tool_input: Any = None) -> Dict[str, Any]:
        """工具调用前的安全检查。"""
        # 检查工具输入是否包含敏感信息
        if tool_input and self._contains_sensitive(tool_input):
            logger.warning("SecurityReminder: 阻止包含敏感信息的工具调用 tool=%s", tool_name)
            return {
                "allowed": False,
                "reason": f"工具 {tool_name} 的输入包含敏感信息，已被安全 Hook 拦截",
            }

        # 检查是否为高风险工具
        high_risk_tools = {"shell_skill", "python_skill"}
        if tool_name in high_risk_tools:
            logger.warning("SecurityReminder: 高风险工具调用 tool=%s", tool_name)
            # 生产环境中可加入审批流程

        return {"allowed": True}

    def on_llm_response(self, ctx: Any, response: Any = None, usage: Optional[Dict] = None) -> Dict[str, Any]:
        """LLM 响应后的敏感信息脱敏。"""
        if response and isinstance(response, str):
            redacted = self._redact_sensitive(response)
            return {"allowed": True, "modified_data": redacted}
        return {"allowed": True}

    def on_memory_write(self, ctx: Any, key: str = "", data: Any = None) -> Dict[str, Any]:
        """记忆写入前的安全检查。"""
        if data and self._contains_sensitive(data):
            logger.warning("SecurityReminder: 阻止敏感信息写入记忆 key=%s", key)
            return {"allowed": False, "reason": "包含敏感信息，不允许写入记忆"}
        return {"allowed": True}

    def _contains_sensitive(self, data: Any) -> bool:
        """检查数据是否包含敏感信息。"""
        text = json.dumps(data, ensure_ascii=False) if not isinstance(data, str) else data
        text_lower = text.lower()
        for field in _SENSITIVE_FIELDS:
            if field in text_lower:
                return True
        import re
        for pattern in _SENSITIVE_PATTERNS:
            if re.search(pattern, text):
                return True
        return False

    def _redact_sensitive(self, text: str) -> str:
        """对文本中的敏感信息进行脱敏。"""
        import re
        for pattern in _SENSITIVE_PATTERNS:
            text = re.sub(pattern, "[REDACTED]", text)
        return text


class AuditHook(HookPlugin):
    """审计 Hook。

    记录所有关键操作到审计日志，满足合规要求。
    """

    PLUGIN_TYPE = "hook"

    def __init__(self):
        self._log_path = Path("./logs/audit.log")
        self._log_path.parent.mkdir(parents=True, exist_ok=True)

    def _write_audit(self, event: str, data: Dict[str, Any]) -> None:
        """写入审计日志。"""
        entry = {
            "timestamp": datetime.now().isoformat(),
            "event": event,
            **data,
        }
        try:
            with open(self._log_path, "a", encoding="utf-8") as f:
                f.write(json.dumps(entry, ensure_ascii=False) + "\n")
        except Exception as e:
            logger.error("AuditHook: 写入审计日志失败: %s", e)

    def on_agent_start(self, ctx: Any, data: Any = None) -> Dict[str, Any]:
        """记录 Agent 启动。"""
        self._write_audit("agent_start", {"context": str(ctx)[:200]})
        return {"allowed": True}

    def on_agent_end(self, ctx: Any, data: Any = None) -> Dict[str, Any]:
        """记录 Agent 结束。"""
        self._write_audit("agent_end", {"context": str(ctx)[:200]})
        return {"allowed": True}

    def on_tool_call(self, ctx: Any, tool_name: str = "", tool_input: Any = None) -> Dict[str, Any]:
        """记录工具调用。"""
        self._write_audit("tool_call", {
            "tool": tool_name,
            "input_size": len(str(tool_input)) if tool_input else 0,
        })
        return {"allowed": True}

    def on_tool_result(self, ctx: Any, tool_name: str = "", result: Any = None) -> Dict[str, Any]:
        """记录工具结果。"""
        self._write_audit("tool_result", {
            "tool": tool_name,
            "result_size": len(str(result)) if result else 0,
        })
        return {"allowed": True}

    def on_llm_response(self, ctx: Any, response: Any = None, usage: Optional[Dict] = None) -> Dict[str, Any]:
        """记录 LLM 使用情况。"""
        if usage:
            self._write_audit("llm_usage", {
                "prompt_tokens": usage.get("prompt_tokens", 0),
                "completion_tokens": usage.get("completion_tokens", 0),
                "total_tokens": usage.get("total_tokens", 0),
            })
        return {"allowed": True}


class CostTrackerHook(HookPlugin):
    """成本追踪 Hook。

    追踪 LLM 调用成本，当累计成本超过预算时发出警告。
    """

    PLUGIN_TYPE = "hook"

    # 每千 Token 成本（美元）
    COST_PER_1K_TOKENS = {
        "gpt-4": {"input": 0.03, "output": 0.06},
        "gpt-4o": {"input": 0.005, "output": 0.015},
        "gpt-4o-mini": {"input": 0.00015, "output": 0.0006},
    }

    def __init__(self):
        self._total_cost_usd: float = 0.0
        self._daily_budget_usd: float = 10.0
        self._call_count: int = 0
        self._start_time: float = time.time()

    def on_llm_response(self, ctx: Any, response: Any = None, usage: Optional[Dict] = None) -> Dict[str, Any]:
        """追踪 LLM 调用成本。"""
        if not usage:
            return {"allowed": True}

        model = usage.get("model", "gpt-4")
        prompt_tokens = usage.get("prompt_tokens", 0)
        completion_tokens = usage.get("completion_tokens", 0)

        # 计算成本
        cost_rates = self.COST_PER_1K_TOKENS.get(model, self.COST_PER_1K_TOKENS["gpt-4"])
        cost = (prompt_tokens / 1000 * cost_rates["input"]) + \
               (completion_tokens / 1000 * cost_rates["output"])

        self._total_cost_usd += cost
        self._call_count += 1

        # 成本超预算警告
        if self._total_cost_usd > self._daily_budget_usd:
            logger.warning(
                "CostTracker: 成本超预算! 当前 $%.4f / 预算 $%.2f (调用 %d 次)",
                self._total_cost_usd, self._daily_budget_usd, self._call_count,
            )
            return {
                "allowed": True,
                "warning": f"成本已超日预算 (${self._daily_budget_usd})",
            }

        # 定期报告
        if self._call_count % 10 == 0:
            logger.info(
                "CostTracker: 累计成本 $%.4f, 调用 %d 次",
                self._total_cost_usd, self._call_count,
            )

        return {"allowed": True}

    def on_agent_end(self, ctx: Any, data: Any = None) -> Dict[str, Any]:
        """Agent 结束时输出成本摘要。"""
        elapsed = time.time() - self._start_time
        logger.info(
            "CostTracker: 会话结束 — 成本 $%.4f, 调用 %d 次, 耗时 %.1fs",
            self._total_cost_usd, self._call_count, elapsed,
        )
        return {"allowed": True}

    def get_stats(self) -> Dict[str, Any]:
        """获取成本统计。"""
        return {
            "total_cost_usd": round(self._total_cost_usd, 4),
            "call_count": self._call_count,
            "daily_budget_usd": self._daily_budget_usd,
            "budget_remaining_usd": round(self._daily_budget_usd - self._total_cost_usd, 4),
            "budget_exceeded": self._total_cost_usd > self._daily_budget_usd,
        }


if __name__ == "__main__":
    # 本地测试
    print("=== SecurityReminderHook 测试 ===")
    sec_hook = SecurityReminderHook()
    result = sec_hook.on_tool_call(None, "http_skill", {"url": "https://api.example.com", "password": "secret123"})
    print("包含敏感信息的工具调用:", result)
    result2 = sec_hook.on_tool_call(None, "http_skill", {"url": "https://api.example.com"})
    print("正常工具调用:", result2)

    print("\n=== AuditHook 测试 ===")
    audit_hook = AuditHook()
    audit_hook.on_agent_start({"user": "test"})
    audit_hook.on_tool_call(None, "http_skill", {"url": "https://api.example.com"})
    print("审计日志已写入 ./logs/audit.log")

    print("\n=== CostTrackerHook 测试 ===")
    cost_hook = CostTrackerHook()
    cost_hook.on_llm_response(None, usage={"model": "gpt-4", "prompt_tokens": 1000, "completion_tokens": 500})
    cost_hook.on_llm_response(None, usage={"model": "gpt-4", "prompt_tokens": 2000, "completion_tokens": 1000})
    print("成本统计:", cost_hook.get_stats())
