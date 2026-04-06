# AgentOS Python SDK - Checkpoint Manager
# Version: 3.0.0
# Last updated: 2026-04-05
#
# 检查点管理器，支持长时间任务的断点续传和状态恢复
# 遵循 ARCHITECTURAL_PRINCIPLES.md C-2（增量演化）和 E-3（资源确定性）

import os
import json
import hashlib
import time
from datetime import datetime
from typing import Any, Dict, List, Optional
from dataclasses import dataclass, field, asdict
from pathlib import Path


@dataclass
class CheckpointData:
    """
    检查点数据结构
    
    Attributes:
        checkpoint_id: 检查点唯一标识
        task_id: 关联的任务 ID
        timestamp: 创建时间戳
        state: 任务状态快照
        progress: 进度 (0.0-1.0)
        metadata: 元数据
    """
    checkpoint_id: str
    task_id: str
    timestamp: str
    state: Dict[str, Any]
    progress: float
    metadata: Dict[str, str] = field(default_factory=dict)
    
    def to_dict(self) -> dict:
        """转换为字典"""
        return {
            "checkpoint_id": self.checkpoint_id,
            "task_id": self.task_id,
            "timestamp": self.timestamp,
            "state": self.state,
            "progress": self.progress,
            "metadata": self.metadata
        }
    
    @classmethod
    def from_dict(cls, data: dict) -> 'CheckpointData':
        """从字典创建"""
        return cls(
            checkpoint_id=data["checkpoint_id"],
            task_id=data["task_id"],
            timestamp=data["timestamp"],
            state=data["state"],
            progress=float(data["progress"]),
            metadata=data.get("metadata", {})
        )


class CheckpointManager:
    """
    检查点管理器，管理长时间任务的检查点生命周期
    
    功能:
        - 创建检查点（保存中间状态）
        - 加载检查点（恢复任务进度）
        - 列出检查点（查看所有历史）
        - 删除检查点（清理过期数据）
    
    使用场景:
        - 长时间运行任务（>1 小时）
        - 需要断点续传的任务
        - 容易失败且恢复成本高的任务
    
    Example:
        >>> mgr = CheckpointManager("/tmp/checkpoints")
        >>> 
        >>> # 创建检查点
        >>> checkpoint = mgr.create_checkpoint(
        ...     task_id="task-123",
        ...     state={"step": 100, "data": {...}},
        ...     progress=0.5
        ... )
        >>> 
        >>> # 恢复检查点
        >>> last_checkpoint = mgr.load_checkpoint("task-123")
        >>> if last_checkpoint:
        ...     start_step = last_checkpoint.state["step"]
        >>>     print(f"从步骤 {start_step} 继续执行")
    """
    
    def __init__(self, storage_path: str = "/tmp/agentos_checkpoints"):
        """
        初始化检查点管理器
        
        Args:
            storage_path: 检查点存储路径
        
        Example:
            >>> mgr = CheckpointManager("/var/lib/agentos/checkpoints")
        """
        self.storage_path = Path(storage_path)
        self.storage_path.mkdir(parents=True, exist_ok=True)
        
        # 检查点索引（内存缓存）
        self._index: Dict[str, List[CheckpointData]] = {}
        self._load_index()
    
    def create_checkpoint(
        self,
        task_id: str,
        state: Dict[str, Any],
        progress: float = 0.0,
        metadata: Optional[Dict[str, str]] = None
    ) -> CheckpointData:
        """
        创建检查点
        
        Args:
            task_id: 任务 ID
            state: 任务状态快照（可序列化为 JSON）
            progress: 进度 (0.0-1.0)
            metadata: 元数据（可选）
        
        Returns:
            CheckpointData: 创建的检查点对象
        
        Raises:
            ValueError: 进度超出范围
            IOError: 保存失败
        
        Example:
            >>> mgr = CheckpointManager()
            >>> checkpoint = mgr.create_checkpoint(
            ...     task_id="task-123",
            ...     state={"processed_items": 1000, "last_id": "item-999"},
            ...     progress=0.5,
            ...     metadata={"stage": "processing"}
            ... )
            >>> print(f"检查点已创建：{checkpoint.checkpoint_id}")
        """
        # 验证进度范围
        if not 0.0 <= progress <= 1.0:
            raise ValueError(f"进度必须在 0.0-1.0 之间，实际：{progress}")
        
        # 生成检查点 ID
        checkpoint_id = self._generate_checkpoint_id(task_id)
        timestamp = datetime.utcnow().isoformat()
        
        # 创建检查点对象
        checkpoint = CheckpointData(
            checkpoint_id=checkpoint_id,
            task_id=task_id,
            timestamp=timestamp,
            state=state,
            progress=progress,
            metadata=metadata or {}
        )
        
        # 持久化到磁盘
        self._save_checkpoint(checkpoint)
        
        # 更新索引
        self._add_to_index(checkpoint)
        
        return checkpoint
    
    def load_checkpoint(
        self,
        task_id: str,
        checkpoint_id: Optional[str] = None
    ) -> Optional[CheckpointData]:
        """
        加载检查点
        
        Args:
            task_id: 任务 ID
            checkpoint_id: 检查点 ID（可选，默认加载最新）
        
        Returns:
            CheckpointData/None: 检查点对象或 None
        
        Example:
            >>> mgr = CheckpointManager()
            >>> 
            >>> # 加载最新检查点
            >>> checkpoint = mgr.load_checkpoint("task-123")
            >>> if checkpoint:
            ...     print(f"恢复到进度：{checkpoint.progress:.1%}")
            ...     state = checkpoint.state
            >>> 
            >>> # 加载指定检查点
            >>> checkpoint = mgr.load_checkpoint(
            ...     "task-123",
            ...     checkpoint_id="abc123"
            ... )
        """
        if checkpoint_id:
            return self._load_by_id(task_id, checkpoint_id)
        else:
            return self._load_latest(task_id)
    
    def list_checkpoints(self, task_id: str) -> List[CheckpointData]:
        """
        列出任务的所有检查点
        
        Args:
            task_id: 任务 ID
        
        Returns:
            List[CheckpointData]: 检查点列表（按时间倒序）
        
        Example:
            >>> mgr = CheckpointManager()
            >>> checkpoints = mgr.list_checkpoints("task-123")
            >>> for cp in checkpoints:
            ...     print(f"{cp.timestamp}: 进度{cp.progress:.1%}")
        """
        # 从索引获取
        if task_id in self._index:
            return sorted(
                self._index[task_id],
                key=lambda x: x.timestamp,
                reverse=True
            )
        
        # 从磁盘加载
        checkpoints = []
        pattern = f"{task_id}_*.json"
        
        for filepath in self.storage_path.glob(pattern):
            try:
                with open(filepath, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                    checkpoint = CheckpointData.from_dict(data)
                    checkpoints.append(checkpoint)
            except (json.JSONDecodeError, KeyError) as e:
                # 忽略损坏的检查点文件
                print(f"警告：检查点文件 {filepath} 损坏：{e}")
        
        # 更新索引
        if checkpoints:
            self._index[task_id] = checkpoints
        
        return sorted(checkpoints, key=lambda x: x.timestamp, reverse=True)
    
    def delete_checkpoint(self, checkpoint_id: str) -> bool:
        """
        删除检查点
        
        Args:
            checkpoint_id: 检查点 ID
        
        Returns:
            bool: 是否删除成功
        
        Example:
            >>> mgr = CheckpointManager()
            >>> success = mgr.delete_checkpoint("abc123")
            >>> if success:
            ...     print("检查点已删除")
        """
        filepath = self.storage_path / f"{checkpoint_id}.json"
        
        if filepath.exists():
            try:
                filepath.unlink()
                # 从索引中移除
                self._remove_from_index(checkpoint_id)
                return True
            except OSError as e:
                print(f"删除检查点失败：{e}")
                return False
        
        return False
    
    def cleanup_old_checkpoints(
        self,
        task_id: str,
        keep_latest: int = 5,
        max_age_hours: int = 24
    ) -> int:
        """
        清理旧检查点
        
        Args:
            task_id: 任务 ID
            keep_latest: 保留最近 N 个
            max_age_hours: 最大保留小时数
        
        Returns:
            int: 删除的检查点数量
        
        Example:
            >>> mgr = CheckpointManager()
            >>> deleted = mgr.cleanup_old_checkpoints(
            ...     "task-123",
            ...     keep_latest=3,
            ...     max_age_hours=12
            ... )
            >>> print(f"删除了 {deleted} 个旧检查点")
        """
        checkpoints = self.list_checkpoints(task_id)
        deleted_count = 0
        
        now = datetime.utcnow()
        
        for i, checkpoint in enumerate(checkpoints):
            # 保留最近的 N 个
            if i < keep_latest:
                continue
            
            # 检查年龄
            try:
                cp_time = datetime.fromisoformat(checkpoint.timestamp)
                age_hours = (now - cp_time).total_seconds() / 3600
                
                if age_hours > max_age_hours:
                    if self.delete_checkpoint(checkpoint.checkpoint_id):
                        deleted_count += 1
            except (ValueError, KeyError):
                # 时间格式错误，删除
                if self.delete_checkpoint(checkpoint.checkpoint_id):
                    deleted_count += 1
        
        return deleted_count
    
    def get_checkpoint_stats(self, task_id: str) -> Dict[str, Any]:
        """
        获取检查点统计信息
        
        Args:
            task_id: 任务 ID
        
        Returns:
            dict: 统计信息
        
        Example:
            >>> mgr = CheckpointManager()
            >>> stats = mgr.get_checkpoint_stats("task-123")
            >>> print(f"总检查点数：{stats['total']}")
            >>> print(f"最新进度：{stats['latest_progress']:.1%}")
        """
        checkpoints = self.list_checkpoints(task_id)
        
        if not checkpoints:
            return {
                "total": 0,
                "latest_progress": 0.0,
                "oldest_timestamp": None,
                "latest_timestamp": None,
                "total_size_bytes": 0
            }
        
        # 计算总大小
        total_size = 0
        for cp in checkpoints:
            filepath = self.storage_path / f"{cp.checkpoint_id}.json"
            if filepath.exists():
                total_size += filepath.stat().st_size
        
        return {
            "total": len(checkpoints),
            "latest_progress": checkpoints[0].progress,
            "oldest_timestamp": checkpoints[-1].timestamp,
            "latest_timestamp": checkpoints[0].timestamp,
            "total_size_bytes": total_size
        }
    
    def _generate_checkpoint_id(self, task_id: str) -> str:
        """生成唯一的检查点 ID"""
        raw = f"{task_id}:{time.time()}:{os.getpid()}"
        return hashlib.sha256(raw.encode()).hexdigest()[:16]
    
    def _save_checkpoint(self, checkpoint: CheckpointData):
        """保存检查点到磁盘"""
        filepath = self.storage_path / f"{checkpoint.checkpoint_id}.json"
        
        with open(filepath, 'w', encoding='utf-8') as f:
            json.dump(checkpoint.to_dict(), f, indent=2, default=str)
    
    def _load_by_id(self, task_id: str, checkpoint_id: str) -> Optional[CheckpointData]:
        """按 ID 加载检查点"""
        filepath = self.storage_path / f"{checkpoint_id}.json"
        
        if not filepath.exists():
            return None
        
        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                data = json.load(f)
                checkpoint = CheckpointData.from_dict(data)
                
                # 验证 task_id 匹配
                if checkpoint.task_id == task_id:
                    return checkpoint
                else:
                    print(f"警告：检查点 {checkpoint_id} 不属于任务 {task_id}")
                    return None
        except (json.JSONDecodeError, KeyError) as e:
            print(f"加载检查点失败：{e}")
            return None
    
    def _load_latest(self, task_id: str) -> Optional[CheckpointData]:
        """加载最新的检查点"""
        checkpoints = self.list_checkpoints(task_id)
        return checkpoints[0] if checkpoints else None
    
    def _load_index(self):
        """从磁盘加载索引"""
        for filepath in self.storage_path.glob("*.json"):
            try:
                with open(filepath, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                    checkpoint = CheckpointData.from_dict(data)
                    self._add_to_index(checkpoint)
            except (json.JSONDecodeError, KeyError):
                pass
    
    def _add_to_index(self, checkpoint: CheckpointData):
        """添加到索引"""
        task_id = checkpoint.task_id
        if task_id not in self._index:
            self._index[task_id] = []
        
        # 避免重复
        existing = [
            cp for cp in self._index[task_id]
            if cp.checkpoint_id == checkpoint.checkpoint_id
        ]
        if not existing:
            self._index[task_id].append(checkpoint)
    
    def _remove_from_index(self, checkpoint_id: str):
        """从索引中移除"""
        for task_id, checkpoints in self._index.items():
            self._index[task_id] = [
                cp for cp in checkpoints
                if cp.checkpoint_id != checkpoint_id
            ]
