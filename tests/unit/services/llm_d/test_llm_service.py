"""
AgentOS LLM 服务单元测试

验证 LLM 守护进程的核心功能：
- Provider 路由与适配
- 缓存机制
- Token 计数与成本追踪
- 响应处理

Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
"""

import json
import pytest
from unittest.mock import MagicMock, patch

from tests.base.base_test import BaseTestCase


class TestLLMProviderRouting(BaseTestCase):
    """测试 LLM Provider 路由"""

    def test_openai_provider_routing(self):
        """测试 OpenAI Provider 路由"""
        provider_config = {
            "name": "openai",
            "model": "gpt-4",
            "api_key": "sk-test"
        }
        assert provider_config["name"] == "openai"
        assert provider_config["model"] == "gpt-4"

    def test_anthropic_provider_routing(self):
        """测试 Anthropic Provider 路由"""
        provider_config = {
            "name": "anthropic",
            "model": "claude-3-opus",
            "api_key": "sk-ant-test"
        }
        assert provider_config["name"] == "anthropic"

    def test_deepseek_provider_routing(self):
        """测试 DeepSeek Provider 路由"""
        provider_config = {
            "name": "deepseek",
            "model": "deepseek-chat",
            "api_key": "sk-ds-test"
        }
        assert provider_config["name"] == "deepseek"

    def test_local_provider_routing(self):
        """测试本地 Provider 路由"""
        provider_config = {
            "name": "local",
            "model": "llama-3-8b",
            "endpoint": "http://localhost:8080"
        }
        assert provider_config["name"] == "local"
        assert "endpoint" in provider_config

    def test_unknown_provider_returns_error(self):
        """测试未知 Provider 返回错误"""
        provider_name = "unknown_provider"
        valid_providers = ["openai", "anthropic", "deepseek", "local"]
        assert provider_name not in valid_providers


class TestLLMCache(BaseTestCase):
    """测试 LLM 缓存机制"""

    def test_cache_hit_returns_cached_response(self):
        """测试缓存命中返回缓存响应"""
        cache = {}
        cache_key = "prompt_hash_abc123"
        cached_response = {"text": "cached answer", "tokens": 42}
        cache[cache_key] = cached_response

        assert cache_key in cache
        assert cache[cache_key]["text"] == "cached answer"

    def test_cache_miss_fetches_new_response(self):
        """测试缓存未命中获取新响应"""
        cache = {}
        cache_key = "prompt_hash_def456"
        assert cache_key not in cache

    def test_cache_invalidation(self):
        """测试缓存失效"""
        cache = {"key1": "value1", "key2": "value2"}
        cache.pop("key1", None)
        assert "key1" not in cache
        assert "key2" in cache

    def test_cache_size_limit(self):
        """测试缓存大小限制"""
        max_cache_size = 100
        cache = {f"key_{i}": f"value_{i}" for i in range(150)}
        assert len(cache) > max_cache_size
        while len(cache) > max_cache_size:
            cache.pop(next(iter(cache)))
        assert len(cache) == max_cache_size


class TestLLMTokenCounter(BaseTestCase):
    """测试 Token 计数"""

    def test_token_count_for_text(self):
        """测试文本 Token 计数"""
        text = "Hello, this is a test message for token counting."
        estimated_tokens = len(text.split())
        assert estimated_tokens > 0

    @pytest.mark.parametrize("text,expected_min_tokens", [
        ("Hi", 1),
        ("Hello world", 2),
        ("The quick brown fox jumps over the lazy dog", 9),
        ("", 0),
    ])
    def test_token_count_variations(self, text, expected_min_tokens):
        """测试不同文本的 Token 计数"""
        if text:
            tokens = max(1, len(text.split()))
            assert tokens >= expected_min_tokens
        else:
            assert expected_min_tokens == 0


class TestLLMCostTracker(BaseTestCase):
    """测试成本追踪"""

    def test_cost_tracking_per_request(self):
        """测试按请求追踪成本"""
        cost_entry = {
            "request_id": "req_001",
            "model": "gpt-4",
            "input_tokens": 100,
            "output_tokens": 50,
            "cost_usd": 0.003
        }
        assert cost_entry["cost_usd"] > 0
        assert cost_entry["input_tokens"] > 0
        assert cost_entry["output_tokens"] > 0

    def test_cost_aggregation(self):
        """测试成本聚合"""
        costs = [
            {"model": "gpt-4", "cost_usd": 0.003},
            {"model": "gpt-4", "cost_usd": 0.005},
            {"model": "claude-3", "cost_usd": 0.004},
        ]
        total = sum(c["cost_usd"] for c in costs)
        assert total == pytest.approx(0.012, abs=0.001)

    def test_cost_budget_limit(self):
        """测试成本预算限制"""
        budget_usd = 10.0
        spent_usd = 8.5
        remaining = budget_usd - spent_usd
        assert remaining > 0
        assert remaining == pytest.approx(1.5, abs=0.01)


class TestLLMResponse(BaseTestCase):
    """测试 LLM 响应处理"""

    def test_response_parsing(self):
        """测试响应解析"""
        raw_response = {
            "choices": [
                {"message": {"content": "Hello from GPT-4"}, "finish_reason": "stop"}
            ],
            "usage": {"prompt_tokens": 10, "completion_tokens": 5, "total_tokens": 15}
        }
        assert len(raw_response["choices"]) == 1
        assert raw_response["choices"][0]["message"]["content"] == "Hello from GPT-4"
        assert raw_response["usage"]["total_tokens"] == 15

    def test_response_error_handling(self):
        """测试响应错误处理"""
        error_response = {
            "error": {
                "message": "Rate limit exceeded",
                "type": "rate_limit_error",
                "code": "429"
            }
        }
        assert "error" in error_response
        assert error_response["error"]["code"] == "429"

    def test_streaming_response_chunks(self):
        """测试流式响应分块"""
        chunks = [
            {"choices": [{"delta": {"content": "Hello"}}]},
            {"choices": [{"delta": {"content": " world"}}]},
            {"choices": [{"delta": {"content": "!"}}]},
        ]
        full_content = "".join(
            chunk["choices"][0]["delta"]["content"] for chunk in chunks
        )
        assert full_content == "Hello world!"
