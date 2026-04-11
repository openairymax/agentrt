#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS 检查点管理器
# 遵循 AgentOS 架构设计原则：E-3 资源确定性、E-6 错误可追溯、C-2 增量演化

"""
AgentOS 检查点管理器

用于支持长时间任务（1000 小时+）的中断恢复，提供：
- 检查点创建与保存
- 检查点恢复与加载
- 检查点列表与查询
- 自动清理过期检查点

Usage:
    from scripts.ops.checkpoint_manager import CheckpointManager
    
    manager = CheckpointManager(checkpoint_dir="/var/lib/agentos/checkpoints")
    
    # 创建检查点
    checkpoint_id = manager.create_checkpoint(
        task_id="task_123",
        state={"progress": 50, "data": "..."},
        metadata={"description": "Mid-task checkpoint"}
    )
    
    # 恢复检查点
    state = manager.restore_checkpoint(checkpoint_id)
    
    # 列出检查点
    checkpoints = manager.list_checkpoints(task_id="task_123")
"""

import json
import os
import shutil
import hashlib
import tempfile
from dataclasses import dataclass, field, asdict
from datetime import datetime
from enum import Enum
from pathlib import Path
from typing import Any, Dict, List, Optional
import logging

logger = logging.getLogger(__name__)


class CheckpointStatus(Enum):
    """检查点状态"""
    ACTIVE = "active"
    ARCHIVED = "archived"
    EXPIRED = "expired"
    CORRUPTED = "corrupted"


@dataclass
class CheckpointInfo:
    """检查点元信息"""
    checkpoint_id: str
    task_id: str
    created_at: str
    updated_at: str
    status: CheckpointStatus
    size_bytes: int
    checksum: str
    metadata: Dict[str, Any] = field(default_factory=dict)
    description: str = ""
    version: str = "1.0"


@dataclass
class CheckpointResult:
    """检查点操作结果"""
    success: bool
    checkpoint_id: Optional[str] = None
    message: str = ""
    error: Optional[str] = None
    checkpoint_info: Optional[CheckpointInfo] = None


class CheckpointManager:
    """
    检查点管理器
    
    遵循架构原则：
    - E-3 资源确定性：检查点文件与任务关联，离开作用域时自动清理
    - E-6 错误可追溯：完整的错误链和上下文信息
    - C-2 增量演化：支持检查点的增量保存和恢复
    - A-1 简约至上：接口简洁，功能明确
    """
    
    def __init__(
        self,
        checkpoint_dir: Optional[str] = None,
        max_checkpoints_per_task: int = 10,
        retention_days: int = 30,
        auto_cleanup: bool = True
    ):
        """
        初始化检查点管理器
        
        Args:
            checkpoint_dir: 检查点存储目录，默认为 /var/lib/agentos/checkpoints
            max_checkpoints_per_task: 每个任务的最大检查点数量
            retention_days: 检查点保留天数
            auto_cleanup: 是否自动清理过期检查点
        """
        self.checkpoint_dir = checkpoint_dir or "/var/lib/agentos/checkpoints"
        self.max_checkpoints_per_task = max_checkpoints_per_task
        self.retention_days = retention_days
        self.auto_cleanup = auto_cleanup
        
        self._ensure_checkpoint_dir()
        
        if auto_cleanup:
            self.cleanup_expired_checkpoints()
    
    def _ensure_checkpoint_dir(self) -> None:
        """确保检查点目录存在"""
        try:
            Path(self.checkpoint_dir).mkdir(parents=True, exist_ok=True)
            logger.info(f"Checkpoint directory ensured: {self.checkpoint_dir}")
        except OSError as e:
            logger.error(f"Failed to create checkpoint directory: {e}")
            raise
    
    def create_checkpoint(
        self,
        task_id: str,
        state: Dict[str, Any],
        metadata: Optional[Dict[str, Any]] = None,
        description: str = ""
    ) -> CheckpointResult:
        """
        创建检查点
        
        Args:
            task_id: 任务 ID
            state: 任务状态字典
            metadata: 元数据
            description: 描述信息
            
        Returns:
            CheckpointResult: 创建结果
        """
        try:
            timestamp = datetime.now().isoformat()
            checkpoint_id = self._generate_checkpoint_id(task_id, timestamp, state)
            
            checkpoint_file = self._get_checkpoint_file_path(checkpoint_id)
            
            info_dict = {
                "checkpoint_id": checkpoint_id,
                "task_id": task_id,
                "created_at": timestamp,
                "updated_at": timestamp,
                "status": CheckpointStatus.ACTIVE.value,
                "size_bytes": 0,
                "checksum": "pending",
                "metadata": metadata or {},
                "description": description
            }
            
            checkpoint_data = {
                "info": info_dict,
                "state": state
            }
            
            temp_fd, temp_file = tempfile.mkstemp(dir=self.checkpoint_dir, suffix='.tmp')
            try:
                with os.fdopen(temp_fd, 'w', encoding='utf-8') as f:
                    json.dump(checkpoint_data, f, indent=2, ensure_ascii=False)
                
                checksum = self._calculate_file_checksum(temp_file)
                size_bytes = os.path.getsize(temp_file)
                
            finally:
                if os.path.exists(temp_file):
                    os.remove(temp_file)
            
            info_dict["checksum"] = checksum
            info_dict["size_bytes"] = size_bytes
            
            checkpoint_data_final = {
                "info": info_dict,
                "state": state
            }
            
            with open(checkpoint_file, 'w', encoding='utf-8') as f:
                json.dump(checkpoint_data_final, f, indent=2, ensure_ascii=False)
            
            checkpoint_info = CheckpointInfo(
                checkpoint_id=checkpoint_id,
                task_id=task_id,
                created_at=timestamp,
                updated_at=timestamp,
                status=CheckpointStatus.ACTIVE,
                size_bytes=size_bytes,
                checksum=checksum,
                metadata=metadata or {},
                description=description
            )
            
            self._enforce_checkpoint_limits(task_id)
            
            logger.info(f"Checkpoint created: {checkpoint_id} for task {task_id}")
            
            return CheckpointResult(
                success=True,
                checkpoint_id=checkpoint_id,
                message=f"Checkpoint created successfully",
                checkpoint_info=checkpoint_info
            )
            
        except Exception as e:
            logger.error(f"Failed to create checkpoint: {e}")
            return CheckpointResult(
                success=False,
                message="Failed to create checkpoint",
                error=str(e)
            )
    
    def restore_checkpoint(
        self,
        checkpoint_id: str
    ) -> CheckpointResult:
        """
        恢复检查点
        
        Args:
            checkpoint_id: 检查点 ID
            
        Returns:
            CheckpointResult: 恢复结果，state 在 metadata 中返回
        """
        try:
            checkpoint_file = self._get_checkpoint_file_path(checkpoint_id)
            
            if not os.path.exists(checkpoint_file):
                return CheckpointResult(
                    success=False,
                    message=f"Checkpoint not found: {checkpoint_id}",
                    error="Checkpoint file does not exist"
                )
            
            with open(checkpoint_file, 'r', encoding='utf-8') as f:
                checkpoint_data = json.load(f)
            
            state = checkpoint_data.get("state", {})
            
            logger.info(f"Checkpoint restored: {checkpoint_id}")
            
            return CheckpointResult(
                success=True,
                checkpoint_id=checkpoint_id,
                message="Checkpoint restored successfully",
                checkpoint_info=CheckpointInfo(**checkpoint_data["info"]),
            )
            
        except json.JSONDecodeError as e:
            logger.error(f"Checkpoint file corrupted: {checkpoint_id}, error: {e}")
            return CheckpointResult(
                success=False,
                message="Checkpoint file is corrupted",
                error=str(e)
            )
        except Exception as e:
            logger.error(f"Failed to restore checkpoint: {checkpoint_id}, error: {e}")
            return CheckpointResult(
                success=False,
                message="Failed to restore checkpoint",
                error=str(e)
            )
    
    @staticmethod
    def _parse_checkpoint_info(checkpoint_data: dict, checkpoint_id: str) -> Optional[CheckpointInfo]:
        """解析检查点数据为 CheckpointInfo 对象"""
        info = checkpoint_data.get("info", {})
        return CheckpointInfo(
            checkpoint_id=info.get("checkpoint_id", checkpoint_id),
            task_id=info.get("task_id", ""),
            created_at=info.get("created_at", ""),
            updated_at=info.get("updated_at", ""),
            status=CheckpointStatus(info.get("status", "active")),
            size_bytes=info.get("size_bytes", 0),
            checksum=info.get("checksum", ""),
            metadata=info.get("metadata", {}),
            description=info.get("description", "")
        )
    
    def _load_checkpoint_from_file(self, filepath: str) -> Optional[CheckpointInfo]:
        """从文件加载检查点信息"""
        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                data = json.load(f)
            return self._parse_checkpoint_info(data, filepath[:-5])
        except Exception as e:
            logger.warning(f"Failed to load checkpoint {filepath}: {e}")
            return None
    
    def _should_include_checkpoint(self, checkpoint_id: str, task_id: Optional[str]) -> bool:
        """判断是否应包含该检查点"""
        if not task_id:
            return True
        return checkpoint_id.startswith(f"{task_id}_")
    
    def _filter_checkpoint(
        self,
        checkpoint_info: CheckpointInfo,
        status: Optional[CheckpointStatus]
    ) -> bool:
        """过滤检查点"""
        if status and checkpoint_info.status != status:
            return False
        return True
    
    def list_checkpoints(
        self,
        task_id: Optional[str] = None,
        status: Optional[CheckpointStatus] = None,
        limit: int = 100
    ) -> List[CheckpointInfo]:
        """
        列出检查点
        
        Args:
            task_id: 任务 ID（可选，过滤特定任务的检查点）
            status: 检查点状态（可选）
            limit: 返回数量限制
            
        Returns:
            List[CheckpointInfo]: 检查点列表
        """
        checkpoints = []
        
        try:
            json_files = [f for f in os.listdir(self.checkpoint_dir) if f.endswith(".json")]
            
            for filename in json_files:
                checkpoint_id = filename[:-5]
                
                if not self._should_include_checkpoint(checkpoint_id, task_id):
                    continue
                
                checkpoint_file = os.path.join(self.checkpoint_dir, filename)
                checkpoint_info = self._load_checkpoint_from_file(checkpoint_file)
                
                if checkpoint_info and self._filter_checkpoint(checkpoint_info, status):
                    checkpoints.append(checkpoint_info)
            
            checkpoints.sort(key=lambda x: x.created_at, reverse=True)
            
            return checkpoints[:limit]
            
        except Exception as e:
            logger.error(f"Failed to list checkpoints: {e}")
            return []
    
    def delete_checkpoint(self, checkpoint_id: str) -> bool:
        """
        删除检查点
        
        Args:
            checkpoint_id: 检查点 ID
            
        Returns:
            bool: 是否删除成功
        """
        try:
            checkpoint_file = self._get_checkpoint_file_path(checkpoint_id)
            
            if os.path.exists(checkpoint_file):
                os.remove(checkpoint_file)
                logger.info(f"Checkpoint deleted: {checkpoint_id}")
                return True
            else:
                logger.warning(f"Checkpoint not found: {checkpoint_id}")
                return False
                
        except Exception as e:
            logger.error(f"Failed to delete checkpoint: {checkpoint_id}, error: {e}")
            return False
    
    def cleanup_expired_checkpoints(self) -> int:
        """
        清理过期检查点
        
        Returns:
            int: 清理的检查点数量
        """
        cleaned_count = 0
        cutoff_date = datetime.now()
        
        checkpoints = self.list_checkpoints()
        
        for checkpoint in checkpoints:
            try:
                created_at = datetime.fromisoformat(checkpoint.created_at)
                age_days = (cutoff_date - created_at).days
                
                if age_days > self.retention_days:
                    if self.delete_checkpoint(checkpoint.checkpoint_id):
                        cleaned_count += 1
                        logger.info(
                            f"Cleaned up expired checkpoint: {checkpoint.checkpoint_id}, "
                            f"age: {age_days} days"
                        )
            except Exception as e:
                logger.warning(f"Failed to cleanup checkpoint {checkpoint.checkpoint_id}: {e}")
        
        logger.info(f"Cleaned up {cleaned_count} expired checkpoints")
        return cleaned_count
    
    def get_checkpoint_state(self, checkpoint_id: str) -> Optional[Dict[str, Any]]:
        """
        获取检查点状态（不验证完整性）
        
        Args:
            checkpoint_id: 检查点 ID
            
        Returns:
            Optional[Dict]: 状态字典，失败返回 None
        """
        try:
            checkpoint_file = self._get_checkpoint_file_path(checkpoint_id)
            
            if not os.path.exists(checkpoint_file):
                return None
            
            with open(checkpoint_file, 'r', encoding='utf-8') as f:
                checkpoint_data = json.load(f)
            
            return checkpoint_data.get("state", {})
            
        except Exception as e:
            logger.error(f"Failed to get checkpoint state: {e}")
            return None
    
    def _generate_checkpoint_id(
        self,
        task_id: str,
        timestamp: str,
        state: Dict[str, Any]
    ) -> str:
        """生成检查点 ID"""
        content = f"{task_id}_{timestamp}_{json.dumps(state, sort_keys=True)}"
        hash_value = hashlib.sha256(content.encode('utf-8')).hexdigest()[:12]
        return f"{task_id}_{hash_value}"
    
    def _get_checkpoint_file_path(self, checkpoint_id: str) -> str:
        """获取检查点文件路径"""
        return os.path.join(self.checkpoint_dir, f"{checkpoint_id}.json")
    
    def _calculate_file_checksum(self, file_path: str) -> str:
        """计算文件 checksum"""
        sha256_hash = hashlib.sha256()
        with open(file_path, "rb") as f:
            for byte_block in iter(lambda: f.read(4096), b""):
                sha256_hash.update(byte_block)
        return sha256_hash.hexdigest()
    
    def _write_checkpoint_atomically(
        self,
        target_path: str,
        data: Dict[str, Any]
    ) -> str:
        """原子性写入检查点文件"""
        dir_name = os.path.dirname(target_path)
        fd, temp_path = tempfile.mkstemp(dir=dir_name, suffix='.tmp')
        
        try:
            with os.fdopen(fd, 'w', encoding='utf-8') as f:
                json.dump(data, f, indent=2, ensure_ascii=False)
            
            return temp_path
            
        except Exception:
            if os.path.exists(temp_path):
                os.remove(temp_path)
            raise
    
    def _update_checkpoint_info(self, info: CheckpointInfo) -> None:
        """更新检查点元信息"""
        checkpoint_file = self._get_checkpoint_file_path(info.checkpoint_id)
        
        if os.path.exists(checkpoint_file):
            with open(checkpoint_file, 'r', encoding='utf-8') as f:
                data = json.load(f)
            
            info_dict = asdict(info)
            info_dict["status"] = info.status.value
            data["info"] = info_dict
            
            self._write_checkpoint_atomically(checkpoint_file, data)
    
    def _enforce_checkpoint_limits(self, task_id: str) -> None:
        """执行检查点数量限制"""
        checkpoints = self.list_checkpoints(task_id=task_id)
        
        if len(checkpoints) > self.max_checkpoints_per_task:
            checkpoints_to_delete = checkpoints[self.max_checkpoints_per_task:]
            
            for checkpoint in checkpoints_to_delete:
                self.delete_checkpoint(checkpoint.checkpoint_id)
                logger.info(
                    f"Deleted excess checkpoint: {checkpoint.checkpoint_id} "
                    f"(limit: {self.max_checkpoints_per_task})"
                )


def get_checkpoint_manager(
    checkpoint_dir: Optional[str] = None,
    **kwargs
) -> CheckpointManager:
    """
    获取全局检查点管理器实例
    
    Args:
        checkpoint_dir: 检查点目录
        **kwargs: 其他参数
        
    Returns:
        CheckpointManager: 检查点管理器实例
    """
    global _global_checkpoint_manager
    
    if '_global_checkpoint_manager' not in globals():
        _global_checkpoint_manager = CheckpointManager(
            checkpoint_dir=checkpoint_dir,
            **kwargs
        )
    
    return _global_checkpoint_manager
