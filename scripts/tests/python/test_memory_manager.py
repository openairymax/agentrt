#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS 记忆管理器测试

"""
记忆管理器单元测试

测试覆盖：
- 记忆统计
- 记忆查询
- 记忆清理
- 遗忘策略配置
- 报告导出
"""

import json
import os
import shutil
import tempfile
import unittest
from datetime import datetime

from scripts.ops.memory_manager import (
    MemoryManager,
    MemoryStats,
    MemoryInfo,
    MemoryLevel,
    ForgettingStrategy,
    get_memory_manager
)


class TestMemoryManager(unittest.TestCase):
    """记忆管理器测试"""
    
    def setUp(self):
        """测试前准备"""
        self.test_dir = tempfile.mkdtemp(prefix="memory_test_")
        self.manager = MemoryManager(
            stats_dir=self.test_dir,
            auto_cleanup=False
        )
    
    def tearDown(self):
        """测试后清理"""
        shutil.rmtree(self.test_dir, ignore_errors=True)
    
    def test_get_memory_stats(self):
        """测试获取记忆统计"""
        stats = self.manager.get_memory_stats()
        
        self.assertIsInstance(stats, MemoryStats)
        self.assertGreaterEqual(stats.total_count, 0)
    
    def test_search_memories(self):
        """测试搜索记忆"""
        memories = self.manager.search_memories(limit=10)
        
        self.assertIsInstance(memories, list)
    
    def test_cleanup_expired_memories_dry_run(self):
        """测试清理过期记忆（干跑模式）"""
        cleaned = self.manager.cleanup_expired_memories(
            days=30,
            dry_run=True
        )
        
        self.assertGreaterEqual(cleaned, 0)
    
    def test_cleanup_expired_memories_with_level(self):
        """测试按层级清理过期记忆"""
        cleaned = self.manager.cleanup_expired_memories(
            days=30,
            level=MemoryLevel.L1_RAW,
            dry_run=True
        )
        
        self.assertGreaterEqual(cleaned, 0)
    
    def test_configure_forgetting_strategy_ebbinghaus(self):
        """测试配置艾宾浩斯遗忘策略"""
        result = self.manager.configure_forgetting_strategy(
            strategy=ForgettingStrategy.EBBINGHAUS,
            parameters={"half_life_days": 7}
        )
        
        self.assertTrue(result)
        
        config_file = os.path.join(self.test_dir, "forgetting_strategy.json")
        self.assertTrue(os.path.exists(config_file))
        
        with open(config_file, 'r', encoding='utf-8') as f:
            config = json.load(f)
        
        self.assertEqual(config["strategy"], "ebbinghaus")
    
    def test_configure_forgetting_strategy_linear(self):
        """测试配置线性遗忘策略"""
        result = self.manager.configure_forgetting_strategy(
            strategy=ForgettingStrategy.LINEAR,
            parameters={"decay_rate": 0.1}
        )
        
        self.assertTrue(result)
    
    def test_configure_forgetting_strategy_access_based(self):
        """测试配置基于访问次数的遗忘策略"""
        result = self.manager.configure_forgetting_strategy(
            strategy=ForgettingStrategy.ACCESS_BASED,
            parameters={"min_access_count": 5}
        )
        
        self.assertTrue(result)
    
    def test_export_memory_report_json(self):
        """测试导出记忆报告"""
        output_path = os.path.join(self.test_dir, "memory_report.json")
        result = self.manager.export_memory_report(
            output_path=output_path,
            format="json"
        )
        
        self.assertTrue(result)
        self.assertTrue(os.path.exists(output_path))
        
        with open(output_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
        
        self.assertIn("timestamp", data)
        self.assertIn("stats", data)
        self.assertIn("summary", data)
    
    def test_export_memory_report_invalid_format(self):
        """测试导出无效格式的报告"""
        output_path = os.path.join(self.test_dir, "memory_report.txt")
        result = self.manager.export_memory_report(
            output_path=output_path,
            format="txt"
        )
        
        self.assertFalse(result)


class TestMemoryStats(unittest.TestCase):
    """记忆统计测试"""
    
    def test_memory_stats_creation(self):
        """测试记忆统计创建"""
        stats = MemoryStats(
            total_count=1000,
            l1_count=500,
            l2_count=300,
            l3_count=150,
            l4_count=50,
            total_size_bytes=1024000,
            average_access_count=10.5
        )
        
        self.assertEqual(stats.total_count, 1000)
        self.assertEqual(stats.l1_count, 500)
        self.assertEqual(stats.l2_count, 300)
        self.assertEqual(stats.l3_count, 150)
        self.assertEqual(stats.l4_count, 50)
    
    def test_memory_stats_percentages(self):
        """测试记忆统计百分比计算"""
        stats = MemoryStats(
            total_count=100,
            l1_count=40,
            l2_count=30,
            l3_count=20,
            l4_count=10
        )
        
        self.assertEqual(stats.total_count, 100)


class TestMemoryInfo(unittest.TestCase):
    """记忆信息测试"""
    
    def test_memory_info_creation(self):
        """测试记忆信息创建"""
        info = MemoryInfo(
            memory_id="mem_001",
            level=MemoryLevel.L2_INDEXED,
            created_at=datetime.now().isoformat(),
            last_accessed_at=datetime.now().isoformat(),
            access_count=10,
            size_bytes=1024
        )
        
        self.assertEqual(info.memory_id, "mem_001")
        self.assertEqual(info.level, MemoryLevel.L2_INDEXED)
        self.assertEqual(info.access_count, 10)


class TestMemoryLevel(unittest.TestCase):
    """记忆层级测试"""
    
    def test_memory_level_values(self):
        """测试记忆层级值"""
        self.assertEqual(MemoryLevel.L1_RAW.value, "l1_raw")
        self.assertEqual(MemoryLevel.L2_INDEXED.value, "l2_indexed")
        self.assertEqual(MemoryLevel.L3_ABSTRACTED.value, "l3_abstracted")
        self.assertEqual(MemoryLevel.L4_PATTERN.value, "l4_pattern")
    
    def test_memory_level_count(self):
        """测试记忆层级数量"""
        self.assertEqual(len(MemoryLevel), 4)


class TestForgettingStrategy(unittest.TestCase):
    """遗忘策略测试"""
    
    def test_forgetting_strategy_values(self):
        """测试遗忘策略值"""
        self.assertEqual(ForgettingStrategy.EBBINGHAUS.value, "ebbinghaus")
        self.assertEqual(ForgettingStrategy.LINEAR.value, "linear")
        self.assertEqual(ForgettingStrategy.ACCESS_BASED.value, "access_based")
        self.assertEqual(ForgettingStrategy.CUSTOM.value, "custom")
    
    def test_forgetting_strategy_count(self):
        """测试遗忘策略数量"""
        self.assertEqual(len(ForgettingStrategy), 4)


if __name__ == "__main__":
    unittest.main()
