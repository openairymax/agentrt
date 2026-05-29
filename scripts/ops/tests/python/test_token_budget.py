#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS Token 预算管理测试

"""
Token 预算管理单元测试

测试覆盖：
- 预算设置
- 预算检查
- 使用记录
- 预算限制
- 告警机制
- 持久化
"""

import json
import os
import shutil
import tempfile
import unittest
from datetime import datetime

from scripts.toolkit.token_utils import (
    TokenBudget,
    BudgetConfig,
    BudgetUsage,
    BudgetStatus,
    BudgetCheckResult,
    BudgetAlert,
    get_token_budget
)


class TestTokenBudget(unittest.TestCase):
    """Token 预算管理测试"""
    
    def setUp(self):
        """测试前准备"""
        self.test_dir = tempfile.mkdtemp(prefix="budget_test_")
        self.budget = TokenBudget(
            storage_dir=self.test_dir,
            enable_alerts=False
        )
    
    def tearDown(self):
        """测试后清理"""
        shutil.rmtree(self.test_dir, ignore_errors=True)
    
    def test_set_budget(self):
        """测试设置预算"""
        result = self.budget.set_budget(
            task_id="test_task_001",
            max_tokens=10000,
            max_cost_usd=1.0,
            warning_threshold=80.0
        )
        
        self.assertTrue(result)
        
        check_result = self.budget.check_budget("test_task_001")
        self.assertEqual(check_result.max_tokens, 10000)
        self.assertEqual(check_result.max_cost_usd, 1.0)
        self.assertEqual(check_result.status, BudgetStatus.WITHIN_BUDGET)
    
    def test_check_budget_no_config(self):
        """测试未配置预算的检查"""
        result = self.budget.check_budget("nonexistent_task")
        
        self.assertEqual(result.status, BudgetStatus.WITHIN_BUDGET)
        self.assertEqual(result.max_tokens, 0)
        self.assertEqual(result.message, "No budget configured for this task")
    
    def test_record_usage(self):
        """测试记录使用"""
        self.budget.set_budget(
            task_id="test_task_002",
            max_tokens=10000,
            max_cost_usd=1.0
        )
        
        result = self.budget.record_usage(
            task_id="test_task_002",
            tokens=1000,
            cost_usd=0.1
        )
        
        self.assertEqual(result.used_tokens, 1000)
        self.assertEqual(result.used_cost_usd, 0.1)
        self.assertEqual(result.remaining_tokens, 9000)
        self.assertEqual(result.remaining_cost_usd, 0.9)
    
    def test_budget_warning_status(self):
        """测试预算警告状态"""
        self.budget.set_budget(
            task_id="test_task_003",
            max_tokens=1000,
            max_cost_usd=0.1,
            warning_threshold=50.0
        )
        
        result = self.budget.record_usage(
            task_id="test_task_003",
            tokens=600,
            cost_usd=0.06
        )
        
        self.assertEqual(result.status, BudgetStatus.WARNING)
        self.assertTrue(result.is_warning)
        self.assertFalse(result.is_over_budget)
    
    def test_budget_over_budget_status(self):
        """测试预算超限状态"""
        self.budget.set_budget(
            task_id="test_task_004",
            max_tokens=1000,
            max_cost_usd=0.1,
            warning_threshold=80.0
        )
        
        result = self.budget.record_usage(
            task_id="test_task_004",
            tokens=950,
            cost_usd=0.095
        )
        
        self.assertEqual(result.status, BudgetStatus.OVER_BUDGET)
        self.assertTrue(result.is_over_budget)
    
    def test_budget_exhausted_status(self):
        """测试预算耗尽状态"""
        self.budget.set_budget(
            task_id="test_task_005",
            max_tokens=1000,
            max_cost_usd=0.1,
            warning_threshold=80.0
        )
        
        result = self.budget.record_usage(
            task_id="test_task_005",
            tokens=1100,
            cost_usd=0.11
        )
        
        self.assertEqual(result.status, BudgetStatus.EXHAUSTED)
        self.assertTrue(result.is_over_budget)
    
    def test_can_use_tokens_allowed(self):
        """测试允许使用 Token"""
        self.budget.set_budget(
            task_id="test_task_006",
            max_tokens=10000,
            max_cost_usd=1.0
        )
        
        self.budget.record_usage(
            task_id="test_task_006",
            tokens=5000,
            cost_usd=0.5
        )
        
        can_use = self.budget.can_use_tokens(
            task_id="test_task_006",
            tokens=2000,
            cost_usd=0.2
        )
        
        self.assertTrue(can_use)
    
    def test_can_use_tokens_denied(self):
        """测试拒绝使用 Token"""
        self.budget.set_budget(
            task_id="test_task_007",
            max_tokens=10000,
            max_cost_usd=1.0
        )
        
        self.budget.record_usage(
            task_id="test_task_007",
            tokens=9000,
            cost_usd=0.9
        )
        
        can_use = self.budget.can_use_tokens(
            task_id="test_task_007",
            tokens=2000,
            cost_usd=0.2
        )
        
        self.assertFalse(can_use)
    
    def test_reset_budget(self):
        """测试重置预算"""
        self.budget.set_budget(
            task_id="test_task_008",
            max_tokens=10000,
            max_cost_usd=1.0
        )
        
        self.budget.record_usage(
            task_id="test_task_008",
            tokens=5000,
            cost_usd=0.5
        )
        
        result = self.budget.reset_budget("test_task_008")
        
        self.assertTrue(result)
        
        check_result = self.budget.check_budget("test_task_008")
        self.assertEqual(check_result.used_tokens, 0)
        self.assertEqual(check_result.used_cost_usd, 0.0)
    
    def test_delete_budget(self):
        """测试删除预算"""
        self.budget.set_budget(
            task_id="test_task_009",
            max_tokens=10000,
            max_cost_usd=1.0
        )
        
        result = self.budget.delete_budget("test_task_009")
        
        self.assertTrue(result)
        
        check_result = self.budget.check_budget("test_task_009")
        self.assertEqual(check_result.max_tokens, 0)
    
    def test_list_budgets(self):
        """测试列出预算"""
        self.budget.set_budget(
            task_id="test_task_010",
            max_tokens=10000,
            max_cost_usd=1.0
        )
        
        self.budget.set_budget(
            task_id="test_task_011",
            max_tokens=20000,
            max_cost_usd=2.0
        )
        
        budgets = self.budget.list_budgets()
        
        self.assertEqual(len(budgets), 2)
        task_ids = [b.task_id for b in budgets]
        self.assertIn("test_task_010", task_ids)
        self.assertIn("test_task_011", task_ids)
    
    def test_get_usage_summary_single(self):
        """测试获取单个任务使用汇总"""
        self.budget.set_budget(
            task_id="test_task_012",
            max_tokens=10000,
            max_cost_usd=1.0
        )
        
        self.budget.record_usage(
            task_id="test_task_012",
            tokens=3000,
            cost_usd=0.3
        )
        
        summary = self.budget.get_usage_summary(task_id="test_task_012")
        
        self.assertEqual(summary["task_id"], "test_task_012")
        self.assertEqual(summary["used_tokens"], 3000)
        self.assertEqual(summary["used_cost_usd"], 0.3)
    
    def test_get_usage_summary_all(self):
        """测试获取所有任务使用汇总"""
        self.budget.set_budget(
            task_id="test_task_013",
            max_tokens=10000,
            max_cost_usd=1.0
        )
        
        self.budget.set_budget(
            task_id="test_task_014",
            max_tokens=20000,
            max_cost_usd=2.0
        )
        
        self.budget.record_usage(
            task_id="test_task_013",
            tokens=3000,
            cost_usd=0.3
        )
        
        self.budget.record_usage(
            task_id="test_task_014",
            tokens=5000,
            cost_usd=0.5
        )
        
        summary = self.budget.get_usage_summary()
        
        self.assertEqual(summary["total_tasks"], 2)
        self.assertEqual(summary["total_tokens"], 8000)
        self.assertEqual(summary["total_cost_usd"], 0.8)
    
    def test_persistence(self):
        """测试持久化"""
        self.budget.set_budget(
            task_id="test_task_015",
            max_tokens=10000,
            max_cost_usd=1.0
        )
        
        self.budget.record_usage(
            task_id="test_task_015",
            tokens=3000,
            cost_usd=0.3
        )
        
        budget2 = TokenBudget(
            storage_dir=self.test_dir,
            enable_alerts=False
        )
        
        check_result = budget2.check_budget("test_task_015")
        self.assertEqual(check_result.used_tokens, 3000)
        self.assertEqual(check_result.used_cost_usd, 0.3)
    
    def test_multiple_record_usage(self):
        """测试多次记录使用"""
        self.budget.set_budget(
            task_id="test_task_016",
            max_tokens=10000,
            max_cost_usd=1.0
        )
        
        for i in range(5):
            self.budget.record_usage(
                task_id="test_task_016",
                tokens=500,
                cost_usd=0.05
            )
        
        check_result = self.budget.check_budget("test_task_016")
        
        self.assertEqual(check_result.used_tokens, 2500)
        self.assertEqual(check_result.used_cost_usd, 0.25)
        self.assertEqual(check_result.remaining_tokens, 7500)


class TestBudgetConfig(unittest.TestCase):
    """预算配置测试"""
    
    def test_budget_config_creation(self):
        """测试预算配置创建"""
        config = BudgetConfig(
            task_id="test_001",
            max_tokens=10000,
            max_cost_usd=1.0,
            warning_threshold_percentage=80.0
        )
        
        self.assertEqual(config.task_id, "test_001")
        self.assertEqual(config.max_tokens, 10000)
        self.assertEqual(config.max_cost_usd, 1.0)
        self.assertEqual(config.warning_threshold_percentage, 80.0)


class TestBudgetUsage(unittest.TestCase):
    """预算使用测试"""
    
    def test_budget_usage_creation(self):
        """测试预算使用创建"""
        usage = BudgetUsage(
            task_id="test_001",
            used_tokens=5000,
            used_cost_usd=0.5,
            request_count=10,
            average_tokens_per_request=500.0
        )
        
        self.assertEqual(usage.task_id, "test_001")
        self.assertEqual(usage.used_tokens, 5000)
        self.assertEqual(usage.average_tokens_per_request, 500.0)


class TestBudgetCheckResult(unittest.TestCase):
    """预算检查结果测试"""
    
    def test_budget_check_result_within_budget(self):
        """测试预算内状态"""
        result = BudgetCheckResult(
            task_id="test_001",
            status=BudgetStatus.WITHIN_BUDGET,
            used_tokens=5000,
            max_tokens=10000,
            used_cost_usd=0.5,
            max_cost_usd=1.0,
            remaining_tokens=5000,
            remaining_cost_usd=0.5,
            remaining_percentage=50.0,
            is_over_budget=False,
            is_warning=False
        )
        
        self.assertEqual(result.status, BudgetStatus.WITHIN_BUDGET)
        self.assertFalse(result.is_over_budget)
        self.assertFalse(result.is_warning)


class TestBudgetAlert(unittest.TestCase):
    """预算告警测试"""
    
    def test_budget_alert_creation(self):
        """测试预算告警创建"""
        alert = BudgetAlert(
            alert_id="alert_001",
            task_id="task_001",
            alert_type="warning",
            threshold_percentage=80.0,
            current_percentage=85.0,
            timestamp=datetime.now().isoformat(),
            message="Budget warning"
        )
        
        self.assertEqual(alert.alert_id, "alert_001")
        self.assertEqual(alert.alert_type, "warning")
        self.assertEqual(alert.current_percentage, 85.0)


if __name__ == "__main__":
    unittest.main()
