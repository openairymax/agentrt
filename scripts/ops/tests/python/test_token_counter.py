#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS Token 计数器测试

"""
Token 计数器单元测试

测试覆盖：
- Token 计数（快速估算和精确计数）
- 使用记录
- 统计查询
- 成本估算
- 多模型支持
- 持久化
"""

import json
import os
import shutil
import tempfile
import unittest
from datetime import datetime, timedelta
from pathlib import Path

from scripts.toolkit.token_utils import (
    TokenCounter,
    TokenCountResult,
    TokenUsageStats,
    get_token_counter
)


class TestTokenCounter(unittest.TestCase):
    """Token 计数器测试"""
    
    def setUp(self):
        """测试前准备"""
        self.test_dir = tempfile.mkdtemp(prefix="token_test_")
        self.counter = TokenCounter(
            storage_dir=self.test_dir,
            auto_save=False
        )
    
    def tearDown(self):
        """测试后清理"""
        shutil.rmtree(self.test_dir, ignore_errors=True)
    
    def test_fast_token_estimate(self):
        """测试快速 Token 估算"""
        text = "Hello, world!"
        tokens = self.counter.count_tokens(text, use_fast_estimator=True)
        
        self.assertGreater(tokens, 0)
        self.assertEqual(tokens, len(text) // 4)
    
    def test_count_tokens_with_model(self):
        """测试不同模型的 Token 计数"""
        text = "This is a test message for token counting"
        
        tokens_gpt4 = self.counter.count_tokens(text, model="gpt-4")
        tokens_claude = self.counter.count_tokens(text, model="claude-3-opus")
        
        self.assertGreater(tokens_gpt4, 0)
        self.assertGreater(tokens_claude, 0)
        self.assertEqual(tokens_gpt4, tokens_claude)
    
    def test_record_usage(self):
        """测试记录 Token 使用"""
        record_id = self.counter.record_usage(
            task_id="test_task_001",
            input_tokens=100,
            output_tokens=50,
            model="gpt-4"
        )
        
        self.assertIsNotNone(record_id)
        self.assertTrue(len(record_id) > 0)
        
        stats = self.counter.get_usage_stats(task_id="test_task_001")
        self.assertEqual(stats.total_tokens, 150)
        self.assertEqual(stats.input_tokens, 100)
        self.assertEqual(stats.output_tokens, 50)
    
    def test_record_usage_with_custom_cost(self):
        """测试自定义成本记录"""
        record_id = self.counter.record_usage(
            task_id="test_task_002",
            input_tokens=100,
            output_tokens=50,
            model="gpt-4",
            cost_override=0.05
        )
        
        self.assertIsNotNone(record_id)
        
        stats = self.counter.get_usage_stats(task_id="test_task_002")
        self.assertEqual(stats.total_cost_usd, 0.05)
    
    def test_get_usage_stats_by_task(self):
        """测试按任务获取统计"""
        self.counter.record_usage(
            task_id="test_task_003",
            input_tokens=200,
            output_tokens=100,
            model="gpt-4"
        )
        
        self.counter.record_usage(
            task_id="test_task_003",
            input_tokens=150,
            output_tokens=75,
            model="claude-3-opus"
        )
        
        stats = self.counter.get_usage_stats(task_id="test_task_003")
        
        self.assertEqual(stats.total_tokens, 525)
        self.assertEqual(stats.request_count, 2)
        self.assertIn("gpt-4", stats.model_breakdown)
        self.assertIn("claude-3-opus", stats.model_breakdown)
    
    def test_get_usage_stats_by_time_range(self):
        """测试按时间范围获取统计"""
        self.counter.record_usage(
            task_id="test_task_004",
            input_tokens=100,
            output_tokens=50,
            model="gpt-4"
        )
        
        start_time = datetime.now() - timedelta(hours=1)
        end_time = datetime.now() + timedelta(hours=1)
        
        stats = self.counter.get_usage_stats(
            task_id="test_task_004",
            start_time=start_time,
            end_time=end_time
        )
        
        self.assertEqual(stats.total_tokens, 150)
    
    def test_get_model_breakdown(self):
        """测试模型使用分布"""
        self.counter.record_usage(
            task_id="test_task_005",
            input_tokens=100,
            output_tokens=50,
            model="gpt-4"
        )
        
        self.counter.record_usage(
            task_id="test_task_005",
            input_tokens=80,
            output_tokens=40,
            model="claude-3-opus"
        )
        
        breakdown = self.counter.get_model_breakdown(task_id="test_task_005")
        
        self.assertEqual(breakdown.get("gpt-4"), 150)
        self.assertEqual(breakdown.get("claude-3-opus"), 120)
    
    def test_get_cost_estimate(self):
        """测试成本估算"""
        self.counter.record_usage(
            task_id="test_task_006",
            input_tokens=1000,
            output_tokens=500,
            model="gpt-4"
        )
        
        cost = self.counter.get_cost_estimate(
            task_id="test_task_006",
            period_days=30
        )
        
        self.assertGreater(cost, 0)
    
    def test_reset_stats(self):
        """测试重置统计"""
        self.counter.record_usage(
            task_id="test_task_007",
            input_tokens=100,
            output_tokens=50,
            model="gpt-4"
        )
        
        self.counter.reset_stats(task_id="test_task_007")
        
        stats = self.counter.get_usage_stats(task_id="test_task_007")
        self.assertEqual(stats.total_tokens, 0)
    
    def test_reset_all_stats(self):
        """测试重置所有统计"""
        self.counter.record_usage(
            task_id="test_task_008",
            input_tokens=100,
            output_tokens=50,
            model="gpt-4"
        )
        
        self.counter.record_usage(
            task_id="test_task_009",
            input_tokens=80,
            output_tokens=40,
            model="claude-3-opus"
        )
        
        self.counter.reset_stats()
        
        stats = self.counter.get_usage_stats()
        self.assertEqual(stats.total_tokens, 0)
    
    def test_export_report_json(self):
        """测试导出 JSON 报告"""
        self.counter.record_usage(
            task_id="test_task_010",
            input_tokens=100,
            output_tokens=50,
            model="gpt-4"
        )
        
        output_path = os.path.join(self.test_dir, "report.json")
        result = self.counter.export_report(output_path, format="json")
        
        self.assertTrue(result)
        self.assertTrue(os.path.exists(output_path))
        
        with open(output_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
        
        self.assertIsInstance(data, list)
        self.assertEqual(len(data), 1)
    
    def test_persistence(self):
        """测试持久化"""
        counter1 = TokenCounter(
            storage_dir=self.test_dir,
            auto_save=True
        )
        
        counter1.record_usage(
            task_id="test_task_011",
            input_tokens=100,
            output_tokens=50,
            model="gpt-4"
        )
        
        counter2 = TokenCounter(
            storage_dir=self.test_dir,
            auto_save=True
        )
        
        stats = counter2.get_usage_stats(task_id="test_task_011")
        self.assertEqual(stats.total_tokens, 150)
    
    def test_detect_provider(self):
        """测试模型提供商检测"""
        provider = self.counter._detect_provider("gpt-4")
        self.assertEqual(provider, ModelProvider.OPENAI)
        
        provider = self.counter._detect_provider("claude-3-opus")
        self.assertEqual(provider, ModelProvider.ANTHROPIC)
        
        provider = self.counter._detect_provider("deepseek-chat")
        self.assertEqual(provider, ModelProvider.DEEPSEEK)
        
        provider = self.counter._detect_provider("unknown-model")
        self.assertEqual(provider, ModelProvider.OTHER)
    
    def test_calculate_cost(self):
        """测试成本计算"""
        cost = self.counter._calculate_cost(1000, 500, "gpt-4")
        
        expected_input_cost = (1000 / 1000) * 0.03
        expected_output_cost = (500 / 1000) * 0.06
        expected_total = expected_input_cost + expected_output_cost
        
        self.assertAlmostEqual(cost, expected_total, places=6)
    
    def test_calculate_cost_custom_model(self):
        """测试自定义模型成本计算"""
        custom_pricing = {
            "custom-model": ModelPricing(input_price_usd=0.01, output_price_usd=0.02)
        }
        
        counter = TokenCounter(
            storage_dir=self.test_dir,
            custom_pricing=custom_pricing
        )
        
        cost = counter._calculate_cost(1000, 500, "custom-model")
        
        expected = (1000 / 1000) * 0.01 + (500 / 1000) * 0.02
        self.assertAlmostEqual(cost, expected, places=6)
    
    def test_calculate_cost_unknown_model(self):
        """测试未知模型成本计算（使用默认值）"""
        cost = self.counter._calculate_cost(1000, 500, "unknown-model")
        
        expected = (1000 / 1000) * 0.001 + (500 / 1000) * 0.002
        self.assertAlmostEqual(cost, expected, places=6)


class TestModelPricing(unittest.TestCase):
    """模型定价测试"""
    
    def test_model_pricing_creation(self):
        """测试模型定价创建"""
        pricing = ModelPricing(
            input_price_usd=0.03,
            output_price_usd=0.06
        )
        
        self.assertEqual(pricing.input_price_usd, 0.03)
        self.assertEqual(pricing.output_price_usd, 0.06)
        self.assertEqual(pricing.currency, "USD")


class TestTokenUsageStats(unittest.TestCase):
    """Token 使用统计测试"""
    
    def test_token_usage_stats_creation(self):
        """测试 Token 使用统计创建"""
        stats = TokenUsageStats(
            total_tokens=1000,
            input_tokens=700,
            output_tokens=300,
            total_cost_usd=0.05,
            request_count=5,
            average_tokens_per_request=200.0
        )
        
        self.assertEqual(stats.total_tokens, 1000)
        self.assertEqual(stats.input_tokens, 700)
        self.assertEqual(stats.output_tokens, 300)
        self.assertEqual(stats.request_count, 5)
        self.assertEqual(stats.average_tokens_per_request, 200.0)


class TestTokenUsageRecord(unittest.TestCase):
    """Token 使用记录测试"""
    
    def test_token_usage_record_creation(self):
        """测试 Token 使用记录创建"""
        record = TokenUsageRecord(
            record_id="test_001",
            task_id="task_001",
            timestamp=datetime.now().isoformat(),
            model="gpt-4",
            provider="openai",
            input_tokens=100,
            output_tokens=50,
            total_tokens=150,
            cost_usd=0.005
        )
        
        self.assertEqual(record.record_id, "test_001")
        self.assertEqual(record.task_id, "task_001")
        self.assertEqual(record.total_tokens, 150)
        self.assertEqual(record.cost_usd, 0.005)


if __name__ == "__main__":
    unittest.main()
