# Copyright (c) 2026 SPHARX. All Rights Reserved.
"""Tests for ecosystem/hooks/ — BaseHook and all official hooks."""

import json
import os
import tempfile
from datetime import datetime, timezone
from pathlib import Path

import pytest

from ecosystem.hooks import BaseHook, HookContext, HookResult
from ecosystem.hooks.src.audit_hook import AuditHook
from ecosystem.hooks.src.cost_tracker import CostTrackerHook
from ecosystem.hooks.src.security_reminder import SecurityReminderHook
from ecosystem.hooks.src.prompt_injector import PromptInjectorHook


# ============================================================
# BaseHook / HookContext / HookResult
# ============================================================

class TestHookContext:
    """Tests for HookContext dataclass."""

    def test_default_construction(self):
        ctx = HookContext(event="test_event")
        assert ctx.event == "test_event"
        assert ctx.agent_id == ""
        assert ctx.session_id == ""
        assert isinstance(ctx.timestamp, str)
        assert ctx.metadata == {}

    def test_full_construction(self):
        ctx = HookContext(
            event="agent_start",
            agent_id="agent-001",
            session_id="sess-001",
            metadata={"model": "gpt-4o"},
        )
        assert ctx.event == "agent_start"
        assert ctx.agent_id == "agent-001"
        assert ctx.session_id == "sess-001"
        assert ctx.metadata == {"model": "gpt-4o"}

    def test_timestamp_is_iso8601(self):
        ctx = HookContext(event="test")
        # Verify ISO 8601 format with timezone
        parsed = datetime.fromisoformat(ctx.timestamp)
        assert parsed.tzinfo is not None


class TestHookResult:
    """Tests for HookResult dataclass."""

    def test_default_allowed(self):
        result = HookResult()
        assert result.allowed is True
        assert result.modified_data is None
        assert result.messages == []
        assert result.metadata == {}

    def test_blocked_result(self):
        result = HookResult(allowed=False, messages=["Blocked by policy"])
        assert result.allowed is False
        assert result.messages == ["Blocked by policy"]

    def test_modified_data(self):
        result = HookResult(allowed=True, modified_data={"key": "value"})
        assert result.modified_data == {"key": "value"}

    def test_metadata(self):
        result = HookResult(allowed=True, metadata={"cost_usd": 0.01})
        assert result.metadata == {"cost_usd": 0.01}


class TestBaseHook:
    """Tests for BaseHook ABC."""

    def test_default_attributes(self):
        hook = BaseHook()
        assert hook.name == "base_hook"
        assert hook.version == "0.1.0"
        assert hook.priority == 50
        assert hook.enabled is True

    def test_get_hook_info(self):
        hook = BaseHook()
        info = hook.get_hook_info()
        assert info["name"] == "base_hook"
        assert info["version"] == "0.1.0"
        assert info["priority"] == 50
        assert info["enabled"] is True
        assert "on_agent_start" in info["events"]
        assert "on_llm_request" in info["events"]

    def test_default_handlers_return_allowed(self):
        hook = BaseHook()
        ctx = HookContext(event="test")
        assert hook.on_agent_start(ctx).allowed is True
        assert hook.on_agent_end(ctx).allowed is True
        assert hook.on_tool_call(ctx).allowed is True
        assert hook.on_llm_request(ctx).allowed is True
        assert hook.on_llm_response(ctx).allowed is True
        assert hook.on_memory_read(ctx).allowed is True
        assert hook.on_memory_write(ctx).allowed is True


# ============================================================
# AuditHook
# ============================================================

class TestAuditHook:
    """Tests for AuditHook."""

    def make_ctx(self, **kwargs):
        defaults = {
            "event": "agent_start",
            "agent_id": "agent-001",
            "session_id": "sess-001",
            "metadata": {},
        }
        defaults.update(kwargs)
        return HookContext(**defaults)

    def test_name_and_version(self):
        hook = AuditHook()
        assert hook.name == "audit"
        assert hook.version == "0.1.0"
        assert hook.priority == 100

    def test_on_agent_start_writes_log(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            log_path = Path(tmpdir) / "audit.log"
            hook = AuditHook({"log_path": str(log_path)})
            ctx = self.make_ctx(event="agent_start")
            result = hook.on_agent_start(ctx)
            assert result.allowed is True
            assert log_path.exists()
            lines = log_path.read_text().strip().split("\n")
            assert len(lines) == 1
            entry = json.loads(lines[0])
            assert entry["action"] == "agent_start"
            assert entry["agent_id"] == "agent-001"

    def test_on_agent_end_writes_log(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            log_path = Path(tmpdir) / "audit.log"
            hook = AuditHook({"log_path": str(log_path)})
            ctx = self.make_ctx(event="agent_end")
            result = hook.on_agent_end(ctx)
            assert result.allowed is True
            entry = json.loads(log_path.read_text().strip())
            assert entry["action"] == "agent_end"

    def test_on_tool_call_writes_log(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            log_path = Path(tmpdir) / "audit.log"
            hook = AuditHook({"log_path": str(log_path)})
            ctx = self.make_ctx(event="tool_call")
            result = hook.on_tool_call(ctx, tool_name="read_file", tool_input="test.txt")
            assert result.allowed is True
            entry = json.loads(log_path.read_text().strip())
            assert entry["action"] == "tool_call"
            assert entry["tool_name"] == "read_file"

    def test_on_tool_call_input_logging_disabled(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            log_path = Path(tmpdir) / "audit.log"
            hook = AuditHook({"log_path": str(log_path), "log_tool_inputs": False})
            ctx = self.make_ctx(event="tool_call")
            result = hook.on_tool_call(ctx, tool_name="read_file", tool_input="secret")
            entry = json.loads(log_path.read_text().strip())
            assert "tool_input_summary" not in entry

    def test_on_tool_result_writes_log(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            log_path = Path(tmpdir) / "audit.log"
            hook = AuditHook({"log_path": str(log_path)})
            ctx = self.make_ctx(event="tool_result")
            result = hook.on_tool_result(ctx, tool_name="read_file", result="content")
            assert result.allowed is True
            entry = json.loads(log_path.read_text().strip())
            assert entry["action"] == "tool_result"
            assert entry["success"] is True

    def test_on_tool_result_output_logging_enabled(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            log_path = Path(tmpdir) / "audit.log"
            hook = AuditHook({"log_path": str(log_path), "log_tool_outputs": True})
            ctx = self.make_ctx(event="tool_result")
            result = hook.on_tool_result(ctx, tool_name="read_file", result="output data")
            entry = json.loads(log_path.read_text().strip())
            assert "result_summary" in entry

    def test_summarize_truncates_long_data(self):
        hook = AuditHook()
        summary = hook._summarize("x" * 500)
        assert len(summary) <= 203  # 200 + "..."
        assert summary.endswith("...")

    def test_summarize_short_data(self):
        hook = AuditHook()
        result = hook._summarize("short")
        assert result == "short"

    def test_summarize_unserializable(self):
        hook = AuditHook()
        # Object that raises on str()
        class BadObj:
            def __str__(self):
                raise RuntimeError("nope")
        result = hook._summarize(BadObj())
        assert result == "<unserializable>"

    def test_multiple_entries_appended(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            log_path = Path(tmpdir) / "audit.log"
            hook = AuditHook({"log_path": str(log_path)})
            ctx = self.make_ctx()
            hook.on_agent_start(ctx)
            hook.on_agent_end(ctx)
            lines = log_path.read_text().strip().split("\n")
            assert len(lines) == 2

    def test_creates_parent_directories(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            log_path = Path(tmpdir) / "sub" / "nested" / "audit.log"
            hook = AuditHook({"log_path": str(log_path)})
            ctx = self.make_ctx()
            hook.on_agent_start(ctx)
            assert log_path.exists()


# ============================================================
# CostTrackerHook
# ============================================================

class TestCostTrackerHook:
    """Tests for CostTrackerHook."""

    def make_ctx(self, **kwargs):
        defaults = {
            "event": "llm_response",
            "agent_id": "agent-001",
            "session_id": "sess-001",
            "metadata": {"model": "gpt-4o", "provider": "openai"},
        }
        defaults.update(kwargs)
        return HookContext(**defaults)

    def test_name_and_version(self):
        hook = CostTrackerHook()
        assert hook.name == "cost_tracker"
        assert hook.version == "0.1.0"
        assert hook.priority == 50

    def test_compute_cost_openai_gpt4o(self):
        hook = CostTrackerHook()
        ctx = self.make_ctx()
        usage = {"input_tokens": 1000, "output_tokens": 500}
        result = hook.on_llm_response(ctx, usage=usage)
        assert result.allowed is True
        # gpt-4o: input=$0.005/K, output=$0.015/K
        # 1000 input = $0.005, 500 output = $0.0075 = $0.0125
        assert result.metadata["cost_usd"] == pytest.approx(0.0125)
        assert result.metadata["input_tokens"] == 1000
        assert result.metadata["output_tokens"] == 500

    def test_compute_cost_deepseek(self):
        hook = CostTrackerHook()
        ctx = self.make_ctx(
            metadata={"model": "deepseek-chat", "provider": "deepseek"}
        )
        usage = {"input_tokens": 1000, "output_tokens": 1000}
        result = hook.on_llm_response(ctx, usage=usage)
        # deepseek-chat: input=$0.00027/K, output=$0.0011/K
        assert result.metadata["cost_usd"] == pytest.approx(0.00137)

    def test_compute_cost_local_free(self):
        hook = CostTrackerHook()
        ctx = self.make_ctx(
            metadata={"model": "local-model", "provider": "local"}
        )
        usage = {"input_tokens": 10000, "output_tokens": 5000}
        result = hook.on_llm_response(ctx, usage=usage)
        assert result.metadata["cost_usd"] == 0.0

    def test_session_cumulative_cost(self):
        hook = CostTrackerHook()
        ctx = self.make_ctx()
        usage = {"input_tokens": 1000, "output_tokens": 500}
        hook.on_llm_response(ctx, usage=usage)
        result = hook.on_llm_response(ctx, usage=usage)
        # 2 calls: $0.0125 * 2 = $0.025
        assert result.metadata["session_total_cost_usd"] == pytest.approx(0.025)

    def test_get_session_stats(self):
        hook = CostTrackerHook()
        ctx = self.make_ctx()
        usage = {"input_tokens": 1000, "output_tokens": 500}
        hook.on_llm_response(ctx, usage=usage)
        stats = hook.get_session_stats("sess-001")
        assert stats["session_id"] == "sess-001"
        assert stats["total_input_tokens"] == 1000
        assert stats["total_output_tokens"] == 500
        assert stats["total_cost_usd"] == pytest.approx(0.0125)
        assert stats["daily_budget_usd"] == 10.0

    def test_reset_session(self):
        hook = CostTrackerHook()
        ctx = self.make_ctx()
        usage = {"input_tokens": 1000, "output_tokens": 500}
        hook.on_llm_response(ctx, usage=usage)
        hook.reset_session("sess-001")
        stats = hook.get_session_stats("sess-001")
        assert stats["total_input_tokens"] == 0
        assert stats["total_cost_usd"] == 0.0

    def test_budget_alert_triggered(self, caplog):
        hook = CostTrackerHook(
            {"daily_budget_usd": 0.01, "alert_threshold_percent": 50}
        )
        ctx = self.make_ctx()
        # Each call: $0.0125 > $0.01 budget
        usage = {"input_tokens": 1000, "output_tokens": 500}
        with caplog.at_level("WARNING", logger="agentrt.hooks.cost_tracker"):
            hook.on_llm_response(ctx, usage=usage)
        assert "Cost alert" in caplog.text

    def test_budget_alert_not_triggered_below_threshold(self, caplog):
        hook = CostTrackerHook(
            {"daily_budget_usd": 100.0, "alert_threshold_percent": 80}
        )
        ctx = self.make_ctx()
        usage = {"input_tokens": 100, "output_tokens": 50}
        with caplog.at_level("WARNING", logger="agentrt.hooks.cost_tracker"):
            hook.on_llm_response(ctx, usage=usage)
        assert "Cost alert" not in caplog.text

    def test_handles_response_with_usage_attribute(self):
        """Test extracting usage from response object with .usage attribute."""
        hook = CostTrackerHook()

        class FakeUsage:
            prompt_tokens = 500
            completion_tokens = 300

        class FakeResponse:
            usage = FakeUsage()
            model = "gpt-4o"

        ctx = self.make_ctx()
        result = hook.on_llm_response(ctx, response=FakeResponse())
        assert result.metadata["input_tokens"] == 500
        assert result.metadata["output_tokens"] == 300

    def test_unknown_provider_fallback(self):
        hook = CostTrackerHook()
        ctx = self.make_ctx(
            metadata={"model": "unknown", "provider": "nonexistent"}
        )
        usage = {"input_tokens": 1000, "output_tokens": 1000}
        result = hook.on_llm_response(ctx, usage=usage)
        assert result.metadata["cost_usd"] == 0.0


# ============================================================
# SecurityReminderHook
# ============================================================

class TestSecurityReminderHook:
    """Tests for SecurityReminderHook."""

    def make_ctx(self, **kwargs):
        defaults = {
            "event": "agent_start",
            "agent_id": "agent-001",
            "session_id": "sess-001",
            "metadata": {},
        }
        defaults.update(kwargs)
        return HookContext(**defaults)

    def test_name_and_version(self):
        hook = SecurityReminderHook()
        assert hook.name == "security_reminder"
        assert hook.version == "0.1.0"
        assert hook.priority == 90

    def test_default_level_is_standard(self):
        hook = SecurityReminderHook()
        ctx = self.make_ctx()
        result = hook.on_agent_start(ctx)
        assert result.allowed is True
        assert result.modified_data["security_level"] == "standard"

    def test_strict_level_has_more_reminders(self):
        hook = SecurityReminderHook({"security_level": "strict"})
        ctx = self.make_ctx()
        result = hook.on_agent_start(ctx)
        assert result.modified_data["reminder_count"] == 10

    def test_minimal_level_has_fewer_reminders(self):
        hook = SecurityReminderHook({"security_level": "minimal"})
        ctx = self.make_ctx()
        result = hook.on_agent_start(ctx)
        assert result.modified_data["reminder_count"] == 3

    def test_inject_position_configurable(self):
        hook = SecurityReminderHook({"inject_position": "append"})
        ctx = self.make_ctx()
        result = hook.on_agent_start(ctx)
        assert result.modified_data["inject_position"] == "append"

    def test_custom_reminders_appended(self):
        hook = SecurityReminderHook({
            "security_level": "minimal",
            "custom_reminders": ["Always double-check file paths."],
        })
        ctx = self.make_ctx()
        result = hook.on_agent_start(ctx)
        # 3 minimal + 1 custom = 4
        assert result.modified_data["reminder_count"] == 4

    def test_disabled_categories_filtered(self):
        hook = SecurityReminderHook({
            "security_level": "strict",
            "disabled_categories": ["code_safety", "output_safety"],
        })
        # strict has 10, disable 2 categories (5 code_safety + 3 output_safety = 8)
        # remaining: 2 confidentiality + 2 audit = 4
        active = hook.get_active_categories()
        assert "code_safety" not in active
        assert "output_safety" not in active
        assert "data_confidentiality" in active
        assert "audit_compliance" in active

    def test_format_reminders_produces_preamble(self):
        hook = SecurityReminderHook({"security_level": "minimal"})
        preamble = hook._format_reminders()
        assert "SECURITY POLICY" in preamble
        assert "MINIMAL" in preamble
        assert "CONFIDENTIALITY" in preamble

    def test_no_reminders_when_all_disabled(self):
        hook = SecurityReminderHook({
            "security_level": "strict",
            "disabled_categories": [
                "data_confidentiality", "code_safety",
                "output_safety", "audit_compliance",
            ],
        })
        ctx = self.make_ctx()
        result = hook.on_agent_start(ctx)
        assert result.allowed is True
        assert result.modified_data is None  # no reminders to inject

    def test_unknown_security_level_falls_back_to_standard(self):
        hook = SecurityReminderHook({"security_level": "nonexistent"})
        ctx = self.make_ctx()
        result = hook.on_agent_start(ctx)
        assert result.modified_data["security_level"] == "nonexistent"
        assert result.modified_data["reminder_count"] == 6  # standard


# ============================================================
# PromptInjectorHook
# ============================================================

class TestPromptInjectorHook:
    """Tests for PromptInjectorHook."""

    def make_ctx(self, **kwargs):
        defaults = {
            "event": "llm_request",
            "agent_id": "agent-001",
            "session_id": "sess-001",
            "metadata": {},
        }
        defaults.update(kwargs)
        return HookContext(**defaults)

    def test_name_and_version(self):
        hook = PromptInjectorHook()
        assert hook.name == "prompt_injector"
        assert hook.version == "0.1.0"
        assert hook.priority == 60

    def test_no_fragments_skips_injection(self):
        hook = PromptInjectorHook()
        ctx = self.make_ctx()
        result = hook.on_llm_request(ctx)
        assert result.allowed is True
        assert result.modified_data is None

    def test_single_fragment_injected(self):
        hook = PromptInjectorHook({
            "fragments": [
                {"name": "test", "text": "Hello {agent_name}", "enabled": True},
            ]
        })
        ctx = self.make_ctx()
        result = hook.on_llm_request(ctx, model="gpt-4o")
        assert result.allowed is True
        assert result.modified_data["fragment_count"] == 1
        assert result.modified_data["fragments"][0]["text"] == "Hello agent-001"

    def test_disabled_fragment_not_injected(self):
        hook = PromptInjectorHook({
            "fragments": [
                {"name": "test", "text": "Hello", "enabled": False},
            ]
        })
        ctx = self.make_ctx()
        result = hook.on_llm_request(ctx)
        assert result.modified_data is None

    def test_rule_based_filtering_agent_type(self):
        hook = PromptInjectorHook({
            "fragments": [
                {
                    "name": "code_review_fragment",
                    "text": "Review code",
                    "enabled": True,
                    "rules": {"agent_type": "code_review"},
                },
            ]
        })
        # Wrong agent_type
        ctx = self.make_ctx(metadata={"agent_type": "chat"})
        result = hook.on_llm_request(ctx)
        assert result.modified_data is None

        # Correct agent_type
        ctx = self.make_ctx(metadata={"agent_type": "code_review"})
        result = hook.on_llm_request(ctx)
        assert result.modified_data["fragment_count"] == 1

    def test_rule_based_filtering_task_type(self):
        hook = PromptInjectorHook({
            "fragments": [
                {
                    "name": "task_fragment",
                    "text": "Do task",
                    "enabled": True,
                    "rules": {"task_type": "summarization"},
                },
            ]
        })
        ctx = self.make_ctx(metadata={"task_type": "summarization"})
        result = hook.on_llm_request(ctx)
        assert result.modified_data["fragment_count"] == 1

    def test_rule_based_filtering_metadata_match(self):
        hook = PromptInjectorHook({
            "fragments": [
                {
                    "name": "env_fragment",
                    "text": "Production reminder",
                    "enabled": True,
                    "rules": {"metadata_match": {"env": "production"}},
                },
            ]
        })
        ctx = self.make_ctx(metadata={"env": "production"})
        result = hook.on_llm_request(ctx)
        assert result.modified_data["fragment_count"] == 1

    def test_rule_based_filtering_regex(self):
        hook = PromptInjectorHook({
            "fragments": [
                {
                    "name": "error_fragment",
                    "text": "Error handling reminder",
                    "enabled": True,
                    "rules": {
                        "regex": {"field": "user_input", "pattern": "error|fail"}
                    },
                },
            ]
        })
        # Match
        ctx = self.make_ctx(metadata={"user_input": "something failed"})
        result = hook.on_llm_request(ctx)
        assert result.modified_data["fragment_count"] == 1

        # No match
        ctx = self.make_ctx(metadata={"user_input": "everything is fine"})
        result = hook.on_llm_request(ctx)
        assert result.modified_data is None

    def test_variable_substitution_all_vars(self):
        hook = PromptInjectorHook({
            "fragments": [
                {
                    "name": "all_vars",
                    "text": "Agent: {agent_name}, Session: {session_id}, "
                            "Model: {model}, Event: {event}, "
                            "Date: {date}, Time: {time}",
                    "enabled": True,
                },
            ]
        })
        ctx = self.make_ctx()
        result = hook.on_llm_request(ctx, model="gpt-4o")
        text = result.modified_data["fragments"][0]["text"]
        assert "agent-001" in text
        assert "sess-001" in text
        assert "gpt-4o" in text
        assert "llm_request" in text
        assert "T" in text  # ISO timestamp

    def test_unknown_variable_kept_as_is(self):
        hook = PromptInjectorHook({
            "fragments": [
                {"name": "test", "text": "{nonexistent_var}", "enabled": True},
            ]
        })
        ctx = self.make_ctx()
        result = hook.on_llm_request(ctx)
        assert result.modified_data["fragments"][0]["text"] == "{nonexistent_var}"

    def test_metadata_variables_available(self):
        hook = PromptInjectorHook({
            "fragments": [
                {"name": "test", "text": "Task: {task_type}", "enabled": True},
            ]
        })
        ctx = self.make_ctx(metadata={"task_type": "code_review"})
        result = hook.on_llm_request(ctx)
        assert result.modified_data["fragments"][0]["text"] == "Task: code_review"

    def test_list_fragments(self):
        hook = PromptInjectorHook({
            "fragments": [
                {"name": "f1", "enabled": True, "text": "Hello"},
                {"name": "f2", "enabled": False, "text": "World"},
            ]
        })
        frags = hook.list_fragments()
        assert len(frags) == 2
        assert frags[0]["name"] == "f1"
        assert frags[0]["enabled"] is True
        assert frags[1]["enabled"] is False

    def test_enable_disable_fragment(self):
        hook = PromptInjectorHook({
            "fragments": [
                {"name": "toggle_me", "enabled": False, "text": "Hi"},
            ]
        })
        assert hook.enable_fragment("toggle_me") is True
        frags = hook.list_fragments()
        assert frags[0]["enabled"] is True

        assert hook.disable_fragment("toggle_me") is True
        frags = hook.list_fragments()
        assert frags[0]["enabled"] is False

        assert hook.enable_fragment("nonexistent") is False
        assert hook.disable_fragment("nonexistent") is False

    def test_invalid_regex_returns_false(self):
        hook = PromptInjectorHook({
            "fragments": [
                {
                    "name": "bad_regex",
                    "text": "test",
                    "enabled": True,
                    "rules": {
                        "regex": {"field": "test", "pattern": "["}  # invalid regex
                    },
                },
            ]
        })
        ctx = self.make_ctx(metadata={"test": "anything"})
        result = hook.on_llm_request(ctx)
        assert result.modified_data is None

    def test_text_preview_in_list_truncated(self):
        hook = PromptInjectorHook({
            "fragments": [
                {"name": "long", "text": "x" * 200, "enabled": True},
            ]
        })
        frags = hook.list_fragments()
        assert len(frags[0]["text_preview"]) <= 83  # 80 + "..."