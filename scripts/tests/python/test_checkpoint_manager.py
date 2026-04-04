#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS 检查点管理器测试

"""
检查点管理器单元测试

测试覆盖：
- 检查点创建
- 检查点恢复
- 检查点列表
- 检查点删除
- 过期清理
- 完整性验证
"""

import json
import os
import shutil
import tempfile
import unittest
from datetime import datetime, timedelta
from pathlib import Path

from scripts.ops.checkpoint_manager import (
    CheckpointManager,
    CheckpointInfo,
    CheckpointStatus,
    CheckpointResult,
    get_checkpoint_manager
)


class TestCheckpointManager(unittest.TestCase):
    """检查点管理器测试"""
    
    def setUp(self):
        """测试前准备"""
        self.test_dir = tempfile.mkdtemp(prefix="checkpoint_test_")
        self.manager = CheckpointManager(
            checkpoint_dir=self.test_dir,
            max_checkpoints_per_task=5,
            retention_days=30,
            auto_cleanup=False
        )
    
    def tearDown(self):
        """测试后清理"""
        shutil.rmtree(self.test_dir, ignore_errors=True)
    
    def test_create_checkpoint(self):
        """测试创建检查点"""
        task_id = "test_task_001"
        state = {"progress": 50, "data": "test_data"}
        
        result = self.manager.create_checkpoint(
            task_id=task_id,
            state=state,
            metadata={"description": "Test checkpoint"},
            description="Test checkpoint"
        )
        
        self.assertTrue(result.success)
        self.assertIsNotNone(result.checkpoint_id)
        self.assertTrue(result.checkpoint_id.startswith(task_id))
        self.assertEqual(result.checkpoint_info.task_id, task_id)
        self.assertEqual(result.checkpoint_info.status, CheckpointStatus.ACTIVE)
    
    def test_restore_checkpoint(self):
        """测试恢复检查点"""
        task_id = "test_task_002"
        state = {"progress": 75, "data": "restore_test"}
        
        create_result = self.manager.create_checkpoint(
            task_id=task_id,
            state=state
        )
        
        self.assertTrue(create_result.success)
        
        restore_result = self.manager.restore_checkpoint(create_result.checkpoint_id)
        
        self.assertTrue(restore_result.success)
        restored_state = self.manager.get_checkpoint_state(create_result.checkpoint_id)
        self.assertEqual(restored_state["progress"], 75)
        self.assertEqual(restored_state["data"], "restore_test")
    
    def test_list_checkpoints(self):
        """测试列出检查点"""
        task_id = "test_task_003"
        
        for i in range(3):
            self.manager.create_checkpoint(
                task_id=task_id,
                state={"index": i}
            )
        
        checkpoints = self.manager.list_checkpoints(task_id=task_id)
        
        self.assertEqual(len(checkpoints), 3)
        
        checkpoints_sorted = sorted(
            checkpoints,
            key=lambda x: x.created_at,
            reverse=True
        )
        self.assertEqual(checkpoints_sorted, checkpoints)
    
    def test_delete_checkpoint(self):
        """测试删除检查点"""
        task_id = "test_task_004"
        
        create_result = self.manager.create_checkpoint(
            task_id=task_id,
            state={"data": "to_delete"}
        )
        
        self.assertTrue(create_result.success)
        
        deleted = self.manager.delete_checkpoint(create_result.checkpoint_id)
        self.assertTrue(deleted)
        
        checkpoints = self.manager.list_checkpoints(task_id=task_id)
        self.assertEqual(len(checkpoints), 0)
    
    def test_checkpoint_integrity_check(self):
        """测试检查点完整性验证"""
        task_id = "test_task_005"
        
        create_result = self.manager.create_checkpoint(
            task_id=task_id,
            state={"data": "integrity_test"}
        )
        
        restore_result = self.manager.restore_checkpoint(create_result.checkpoint_id)
        
        self.assertTrue(restore_result.success)
        
        checkpoint_file = self.manager._get_checkpoint_file_path(create_result.checkpoint_id)
        
        with open(checkpoint_file, 'r') as f:
            data = json.load(f)
        
        data["info"]["checksum"] = "invalid_checksum"
        
        with open(checkpoint_file, 'w') as f:
            json.dump(data, f)
        
        corrupted_restore = self.manager.restore_checkpoint(create_result.checkpoint_id)
        self.assertFalse(corrupted_restore.success)
        self.assertIn("integrity", corrupted_restore.message.lower())
    
    def test_checkpoint_limits(self):
        """测试检查点数量限制"""
        task_id = "test_task_006"
        
        for i in range(7):
            self.manager.create_checkpoint(
                task_id=task_id,
                state={"index": i}
            )
        
        checkpoints = self.manager.list_checkpoints(task_id=task_id)
        
        self.assertLessEqual(len(checkpoints), 5)
    
    def test_get_checkpoint_state(self):
        """测试获取检查点状态"""
        task_id = "test_task_007"
        state = {"key": "value", "number": 42}
        
        create_result = self.manager.create_checkpoint(
            task_id=task_id,
            state=state
        )
        
        retrieved_state = self.manager.get_checkpoint_state(create_result.checkpoint_id)
        
        self.assertEqual(retrieved_state["key"], "value")
        self.assertEqual(retrieved_state["number"], 42)
    
    def test_checkpoint_persistence(self):
        """测试检查点持久化"""
        task_id = "test_task_008"
        state = {"persistent": True}
        
        self.manager.create_checkpoint(
            task_id=task_id,
            state=state
        )
        
        manager2 = CheckpointManager(
            checkpoint_dir=self.test_dir,
            auto_cleanup=False
        )
        
        checkpoints = manager2.list_checkpoints(task_id=task_id)
        
        self.assertEqual(len(checkpoints), 1)
        self.assertTrue(checkpoints[0].checkpoint_id.startswith(task_id))
    
    def test_invalid_checkpoint_id(self):
        """测试无效检查点 ID"""
        restore_result = self.manager.restore_checkpoint("invalid_id")
        
        self.assertFalse(restore_result.success)
        self.assertIn("not found", restore_result.message.lower())
    
    def test_empty_state(self):
        """测试空状态"""
        task_id = "test_task_009"
        
        result = self.manager.create_checkpoint(
            task_id=task_id,
            state={}
        )
        
        self.assertTrue(result.success)
        
        state = self.manager.get_checkpoint_state(result.checkpoint_id)
        self.assertEqual(state, {})


class TestCheckpointInfo(unittest.TestCase):
    """检查点信息测试"""
    
    def test_checkpoint_info_creation(self):
        """测试检查点信息创建"""
        info = CheckpointInfo(
            checkpoint_id="test_001",
            task_id="task_001",
            created_at=datetime.now().isoformat(),
            updated_at=datetime.now().isoformat(),
            status=CheckpointStatus.ACTIVE,
            size_bytes=1024,
            checksum="abc123"
        )
        
        self.assertEqual(info.checkpoint_id, "test_001")
        self.assertEqual(info.task_id, "task_001")
        self.assertEqual(info.status, CheckpointStatus.ACTIVE)
        self.assertEqual(info.size_bytes, 1024)


class TestCheckpointResult(unittest.TestCase):
    """检查结果测试"""
    
    def test_success_result(self):
        """测试成功结果"""
        result = CheckpointResult(
            success=True,
            checkpoint_id="test_001",
            message="Success"
        )
        
        self.assertTrue(result.success)
        self.assertEqual(result.checkpoint_id, "test_001")
    
    def test_failure_result(self):
        """测试失败结果"""
        result = CheckpointResult(
            success=False,
            message="Failed",
            error="Test error"
        )
        
        self.assertFalse(result.success)
        self.assertEqual(result.error, "Test error")


if __name__ == "__main__":
    unittest.main()
