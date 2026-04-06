# AgentOS Python SDK - Checkpoint Manager Tests
# Version: 3.0.0
# Last updated: 2026-04-05
#
# 检查点管理器单元测试

import pytest
import os
import json
import tempfile
import shutil
from pathlib import Path

from agentos.modules.task.checkpoint import CheckpointManager, CheckpointData


class TestCheckpointData:
    """CheckpointData 数据类测试"""
    
    def test_create_checkpoint_data(self):
        """测试创建检查点数据"""
        cp = CheckpointData(
            checkpoint_id="test-cp-001",
            task_id="task-123",
            timestamp="2026-04-05T12:00:00",
            state={"step": 100, "data": "value"},
            progress=0.5,
            metadata={"stage": "processing"}
        )
        
        assert cp.checkpoint_id == "test-cp-001"
        assert cp.task_id == "task-123"
        assert cp.progress == 0.5
        assert cp.state["step"] == 100
    
    def test_to_dict(self):
        """测试转换为字典"""
        cp = CheckpointData(
            checkpoint_id="test-cp-002",
            task_id="task-456",
            timestamp="2026-04-05T13:00:00",
            state={"key": "value"},
            progress=0.75
        )
        
        data = cp.to_dict()
        
        assert data["checkpoint_id"] == "test-cp-002"
        assert data["task_id"] == "task-456"
        assert data["progress"] == 0.75
        assert data["state"]["key"] == "value"
    
    def test_from_dict(self):
        """测试从字典创建"""
        data = {
            "checkpoint_id": "test-cp-003",
            "task_id": "task-789",
            "timestamp": "2026-04-05T14:00:00",
            "state": {"processed": 500},
            "progress": 0.25,
            "metadata": {"batch": "1"}
        }
        
        cp = CheckpointData.from_dict(data)
        
        assert cp.checkpoint_id == "test-cp-003"
        assert cp.task_id == "task-789"
        assert cp.progress == 0.25
        assert cp.metadata["batch"] == "1"


class TestCheckpointManager:
    """CheckpointManager 测试"""
    
    @pytest.fixture
    def temp_dir(self):
        """创建临时目录"""
        temp = tempfile.mkdtemp()
        yield temp
        shutil.rmtree(temp)
    
    @pytest.fixture
    def checkpoint_mgr(self, temp_dir):
        """创建检查点管理器"""
        return CheckpointManager(temp_dir)
    
    def test_init(self, temp_dir):
        """测试初始化"""
        mgr = CheckpointManager(temp_dir)
        
        assert mgr.storage_path == Path(temp_dir)
        assert mgr.storage_path.exists()
    
    def test_create_checkpoint(self, checkpoint_mgr):
        """测试创建检查点"""
        cp = checkpoint_mgr.create_checkpoint(
            task_id="task-123",
            state={"step": 100, "data": "test"},
            progress=0.5,
            metadata={"stage": "processing"}
        )
        
        assert cp.checkpoint_id is not None
        assert cp.task_id == "task-123"
        assert cp.progress == 0.5
        assert cp.state["step"] == 100
        assert len(cp.checkpoint_id) == 16  # SHA256 前 16 位
    
    def test_create_checkpoint_invalid_progress(self, checkpoint_mgr):
        """测试创建检查点 - 无效进度"""
        with pytest.raises(ValueError) as exc_info:
            checkpoint_mgr.create_checkpoint(
                task_id="task-123",
                state={},
                progress=1.5  # 超出范围
            )
        
        assert "进度必须在 0.0-1.0 之间" in str(exc_info.value)
    
    def test_load_checkpoint_latest(self, checkpoint_mgr):
        """测试加载最新检查点"""
        # 创建多个检查点
        checkpoint_mgr.create_checkpoint(
            task_id="task-123",
            state={"step": 100},
            progress=0.25
        )
        
        cp2 = checkpoint_mgr.create_checkpoint(
            task_id="task-123",
            state={"step": 200},
            progress=0.5
        )
        
        # 加载最新
        loaded = checkpoint_mgr.load_checkpoint("task-123")
        
        assert loaded is not None
        assert loaded.checkpoint_id == cp2.checkpoint_id
        assert loaded.state["step"] == 200
        assert loaded.progress == 0.5
    
    def test_load_checkpoint_by_id(self, checkpoint_mgr):
        """测试按 ID 加载检查点"""
        cp1 = checkpoint_mgr.create_checkpoint(
            task_id="task-123",
            state={"step": 100},
            progress=0.25
        )
        
        checkpoint_mgr.create_checkpoint(
            task_id="task-123",
            state={"step": 200},
            progress=0.5
        )
        
        # 按 ID 加载
        loaded = checkpoint_mgr.load_checkpoint("task-123", checkpoint_id=cp1.checkpoint_id)
        
        assert loaded is not None
        assert loaded.checkpoint_id == cp1.checkpoint_id
        assert loaded.state["step"] == 100
    
    def test_load_nonexistent_checkpoint(self, checkpoint_mgr):
        """测试加载不存在的检查点"""
        loaded = checkpoint_mgr.load_checkpoint("nonexistent-task")
        assert loaded is None
    
    def test_list_checkpoints(self, checkpoint_mgr):
        """测试列出检查点"""
        # 创建 5 个检查点
        for i in range(5):
            checkpoint_mgr.create_checkpoint(
                task_id="task-123",
                state={"step": i * 100},
                progress=(i + 1) / 10
            )
        
        checkpoints = checkpoint_mgr.list_checkpoints("task-123")
        
        assert len(checkpoints) == 5
        # 按时间倒序
        assert checkpoints[0].progress == 0.5
        assert checkpoints[-1].progress == 0.1
    
    def test_delete_checkpoint(self, checkpoint_mgr):
        """测试删除检查点"""
        cp = checkpoint_mgr.create_checkpoint(
            task_id="task-123",
            state={},
            progress=0.5
        )
        
        # 删除
        success = checkpoint_mgr.delete_checkpoint(cp.checkpoint_id)
        assert success
        
        # 验证已删除
        loaded = checkpoint_mgr.load_checkpoint("task-123")
        assert loaded is None
    
    def test_delete_nonexistent_checkpoint(self, checkpoint_mgr):
        """测试删除不存在的检查点"""
        success = checkpoint_mgr.delete_checkpoint("nonexistent-id")
        assert not success
    
    def test_cleanup_old_checkpoints(self, checkpoint_mgr):
        """测试清理旧检查点"""
        # 创建 10 个检查点
        for i in range(10):
            checkpoint_mgr.create_checkpoint(
                task_id="task-123",
                state={"step": i},
                progress=i / 10
            )
        
        # 清理，保留最近 3 个
        deleted = checkpoint_mgr.cleanup_old_checkpoints(
            task_id="task-123",
            keep_latest=3,
            max_age_hours=0  # 立即过期
        )
        
        assert deleted == 7  # 删除 7 个
        
        # 验证剩余 3 个
        checkpoints = checkpoint_mgr.list_checkpoints("task-123")
        assert len(checkpoints) == 3
    
    def test_get_checkpoint_stats(self, checkpoint_mgr):
        """测试获取统计信息"""
        for i in range(5):
            checkpoint_mgr.create_checkpoint(
                task_id="task-123",
                state={"step": i * 100},
                progress=(i + 1) / 10
            )
        
        stats = checkpoint_mgr.get_checkpoint_stats("task-123")
        
        assert stats["total"] == 5
        assert stats["latest_progress"] == 0.5
        assert stats["oldest_timestamp"] is not None
        assert stats["latest_timestamp"] is not None
        assert stats["total_size_bytes"] > 0
    
    def test_persistence(self, temp_dir):
        """测试持久化（重启后恢复）"""
        # 创建管理器并添加检查点
        mgr1 = CheckpointManager(temp_dir)
        cp = mgr1.create_checkpoint(
            task_id="task-123",
            state={"data": "persistent"},
            progress=0.75
        )
        
        # 创建新实例（模拟重启）
        mgr2 = CheckpointManager(temp_dir)
        
        # 验证可以加载
        loaded = mgr2.load_checkpoint("task-123")
        assert loaded is not None
        assert loaded.checkpoint_id == cp.checkpoint_id
        assert loaded.state["data"] == "persistent"
        assert loaded.progress == 0.75
    
    def test_concurrent_checkpoints(self, checkpoint_mgr):
        """测试并发创建检查点"""
        import threading
        
        errors = []
        
        def create_checkpoint(task_id, progress):
            try:
                checkpoint_mgr.create_checkpoint(
                    task_id=task_id,
                    state={"task": task_id},
                    progress=progress
                )
            except Exception as e:
                errors.append(e)
        
        # 并发创建 10 个检查点
        threads = []
        for i in range(10):
            t = threading.Thread(
                target=create_checkpoint,
                args=(f"task-{i}", i / 10)
            )
            threads.append(t)
            t.start()
        
        for t in threads:
            t.join()
        
        # 验证没有错误
        assert len(errors) == 0
        
        # 验证所有检查点都已创建
        for i in range(10):
            checkpoints = checkpoint_mgr.list_checkpoints(f"task-{i}")
            assert len(checkpoints) == 1
    
    def test_large_state(self, checkpoint_mgr):
        """测试大状态保存"""
        # 创建大状态（1MB）
        large_data = {"data": "x" * (1024 * 1024)}
        
        cp = checkpoint_mgr.create_checkpoint(
            task_id="task-large",
            state=large_data,
            progress=0.5
        )
        
        # 验证可以加载
        loaded = checkpoint_mgr.load_checkpoint("task-large")
        assert loaded is not None
        assert loaded.checkpoint_id == cp.checkpoint_id
        assert len(loaded.state["data"]) == 1024 * 1024
    
    def test_metadata_handling(self, checkpoint_mgr):
        """测试元数据处理"""
        cp = checkpoint_mgr.create_checkpoint(
            task_id="task-123",
            state={},
            progress=0.5,
            metadata={
                "stage": "processing",
                "worker": "worker-1",
                "items_processed": "1000"
            }
        )
        
        assert cp.metadata["stage"] == "processing"
        assert cp.metadata["worker"] == "worker-1"
        assert cp.metadata["items_processed"] == "1000"
    
    def test_empty_metadata(self, checkpoint_mgr):
        """测试空元数据"""
        cp = checkpoint_mgr.create_checkpoint(
            task_id="task-123",
            state={},
            progress=0.5
        )
        
        assert cp.metadata == {}


class TestCheckpointIntegration:
    """检查点集成测试"""
    
    @pytest.fixture
    def temp_dir(self):
        temp = tempfile.mkdtemp()
        yield temp
        shutil.rmtree(temp)
    
    def test_checkpoint_restore_workflow(self, temp_dir):
        """测试检查点恢复工作流"""
        mgr = CheckpointManager(temp_dir)
        
        # 阶段 1: 创建检查点
        for i in range(5):
            mgr.create_checkpoint(
                task_id="task-workflow",
                state={"step": i, "data": f"step_{i}"},
                progress=(i + 1) / 10
            )
        
        # 阶段 2: 恢复
        last_cp = mgr.load_checkpoint("task-workflow")
        assert last_cp is not None
        assert last_cp.state["step"] == 4
        
        # 阶段 3: 继续创建
        mgr.create_checkpoint(
            task_id="task-workflow",
            state={"step": 5, "data": "step_5"},
            progress=0.6
        )
        
        # 验证
        checkpoints = mgr.list_checkpoints("task-workflow")
        assert len(checkpoints) == 6
        assert checkpoints[0].state["step"] == 5


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
