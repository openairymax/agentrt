#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS 记忆管理工具
# 遵循 AgentOS 架构设计原则：C-3 记忆卷载、E-2 可观测性、A-1 简约至上

"""
AgentOS 记忆管理工具

用于管理和优化 AgentOS 记忆系统，支持：
- 记忆查询和检索
- 记忆清理和归档
- 记忆使用统计
- 遗忘策略配置

Usage:
    from scripts.ops.memory_manager import MemoryManager, get_memory_manager
    
    manager = get_memory_manager()
    
    # 查询记忆
    memories = manager.search_memories(query="task completion", limit=10)
    
    # 获取记忆统计
    stats = manager.get_memory_stats()
    print(f"Total memories: {stats.total_count}")
    
    # 清理过期记忆
    cleaned = manager.cleanup_expired_memories(days=30)
"""

import json
import os
import logging
from dataclasses import dataclass, field, asdict
from datetime import datetime, timedelta
from enum import Enum
from pathlib import Path
from typing import Any, Dict, List, Optional
import threading

logger = logging.getLogger(__name__)


class MemoryLevel(Enum):
    """记忆层级（对应 MemoryRovol 四层记忆系统）"""
    L1_RAW = "l1_raw"  # 原始记忆
    L2_INDEXED = "l2_indexed"  # 索引记忆
    L3_ABSTRACTED = "l3_abstracted"  # 抽象记忆
    L4_PATTERN = "l4_pattern"  # 模式记忆


class ForgettingStrategy(Enum):
    """遗忘策略"""
    EBBINGHAUS = "ebbinghaus"  # 艾宾浩斯遗忘曲线
    LINEAR = "linear"  # 线性遗忘
    ACCESS_BASED = "access_based"  # 基于访问次数
    CUSTOM = "custom"  # 自定义策略


@dataclass
class MemoryStats:
    """记忆统计信息"""
    total_count: int = 0
    l1_count: int = 0
    l2_count: int = 0
    l3_count: int = 0
    l4_count: int = 0
    total_size_bytes: int = 0
    oldest_memory: str = ""
    newest_memory: str = ""
    average_access_count: float = 0.0


@dataclass
class MemoryInfo:
    """记忆元信息"""
    memory_id: str
    level: MemoryLevel
    created_at: str
    last_accessed_at: str
    access_count: int
    size_bytes: int
    metadata: Dict[str, Any] = field(default_factory=dict)


class MemoryManager:
    """
    记忆管理器
    
    遵循架构原则：
    - C-3 记忆卷载：支持四层记忆系统管理
    - E-2 可观测性：记忆使用统计和监控
    - A-1 简约至上：简洁的接口设计
    """
    
    def __init__(
        self,
        stats_dir: Optional[str] = None,
        auto_cleanup: bool = False,
        default_retention_days: int = 90
    ):
        """
        初始化记忆管理器
        
        Args:
            stats_dir: 统计数据存储目录
            auto_cleanup: 是否自动清理
            default_retention_days: 默认保留天数
        """
        self.stats_dir = stats_dir or "/var/lib/agentos/memory_stats"
        self.auto_cleanup = auto_cleanup
        self.default_retention_days = default_retention_days
        
        self._stats = MemoryStats()
        self._lock = threading.Lock()
        
        self._ensure_stats_dir()
        self._load_stats()
        
        if auto_cleanup:
            self.cleanup_expired_memories()
    
    def _ensure_stats_dir(self) -> None:
        """确保统计目录存在"""
        try:
            Path(self.stats_dir).mkdir(parents=True, exist_ok=True)
            logger.info(f"Memory stats directory ensured: {self.stats_dir}")
        except OSError as e:
            logger.error(f"Failed to create stats directory: {e}")
            raise
    
    def search_memories(
        self,
        query: Optional[str] = None,
        level: Optional[MemoryLevel] = None,
        limit: int = 100,
        min_access_count: int = 0
    ) -> List[MemoryInfo]:
        """
        搜索记忆（模拟实现，实际应连接 MemoryRovol）
        
        Args:
            query: 搜索查询
            level: 记忆层级过滤
            limit: 返回数量限制
            min_access_count: 最小访问次数
            
        Returns:
            List[MemoryInfo]: 记忆列表
        """
        logger.info(f"Searching memories: query={query}, level={level}, limit={limit}")
        
        memories = []
        
        try:
            memory_api = self._get_memory_api()
            
            if memory_api:
                search_params = {
                    "limit": limit,
                    "min_access_count": min_access_count
                }
                
                if query:
                    search_params["query"] = query
                
                if level:
                    search_params["level"] = level.value
                
                result = memory_api.search(**search_params)
                
                for item in result.get("memories", []):
                    memory = MemoryInfo(
                        memory_id=item.get("id", ""),
                        level=MemoryLevel(item.get("level", "l1_raw")),
                        created_at=item.get("created_at", ""),
                        last_accessed_at=item.get("last_accessed", ""),
                        access_count=item.get("access_count", 0),
                        size_bytes=item.get("size", 0),
                        metadata=item.get("metadata", {})
                    )
                    memories.append(memory)
            
        except Exception as e:
            logger.error(f"Failed to search memories: {e}")
        
        return memories
    
    def get_memory_stats(self) -> MemoryStats:
        """
        获取记忆统计信息
        
        Returns:
            MemoryStats: 统计信息
        """
        with self._lock:
            try:
                memory_api = self._get_memory_api()
                
                if memory_api:
                    stats = memory_api.get_stats()
                    
                    self._stats.total_count = stats.get("total_count", 0)
                    self._stats.l1_count = stats.get("l1_count", 0)
                    self._stats.l2_count = stats.get("l2_count", 0)
                    self._stats.l3_count = stats.get("l3_count", 0)
                    self._stats.l4_count = stats.get("l4_count", 0)
                    self._stats.total_size_bytes = stats.get("total_size", 0)
                    self._stats.oldest_memory = stats.get("oldest", "")
                    self._stats.newest_memory = stats.get("newest", "")
                    
            except Exception as e:
                logger.error(f"Failed to get memory stats: {e}")
            
            return self._stats
    
    def cleanup_expired_memories(
        self,
        days: Optional[int] = None,
        level: Optional[MemoryLevel] = None,
        dry_run: bool = False
    ) -> int:
        """
        清理过期记忆
        
        Args:
            days: 保留天数，默认 90 天
            level: 记忆层级
            dry_run: 是否仅模拟清理
            
        Returns:
            int: 清理的记忆数量
        """
        retention_days = days or self.default_retention_days
        cutoff_date = datetime.now() - timedelta(days=retention_days)
        
        logger.info(f"Cleaning up memories older than {retention_days} days")
        
        cleaned_count = 0
        
        try:
            memory_api = self._get_memory_api()
            
            if memory_api:
                memories = self.search_memories(limit=1000)
                
                for memory in memories:
                    try:
                        created_at = datetime.fromisoformat(memory.created_at)
                        
                        if created_at < cutoff_date:
                            if level and memory.level != level:
                                continue
                            
                            if not dry_run:
                                memory_api.delete(memory.memory_id)
                            
                            cleaned_count += 1
                            logger.debug(
                                f"Cleaned up memory: {memory.memory_id}, "
                                f"age: {(datetime.now() - created_at).days} days"
                            )
                            
                    except Exception as e:
                        logger.warning(f"Failed to process memory {memory.memory_id}: {e}")
                
                logger.info(f"Cleaned up {cleaned_count} expired memories")
                
        except Exception as e:
            logger.error(f"Failed to cleanup memories: {e}")
        
        return cleaned_count
    
    def configure_forgetting_strategy(
        self,
        strategy: ForgettingStrategy,
        parameters: Optional[Dict[str, Any]] = None
    ) -> bool:
        """
        配置遗忘策略
        
        Args:
            strategy: 遗忘策略类型
            parameters: 策略参数
            
        Returns:
            bool: 是否配置成功
        """
        try:
            config = {
                "strategy": strategy.value,
                "parameters": parameters or {},
                "updated_at": datetime.now().isoformat()
            }
            
            config_file = os.path.join(self.stats_dir, "forgetting_strategy.json")
            
            with open(config_file, 'w', encoding='utf-8') as f:
                json.dump(config, f, indent=2, ensure_ascii=False)
            
            logger.info(f"Forgetting strategy configured: {strategy.value}")
            
            return True
            
        except Exception as e:
            logger.error(f"Failed to configure forgetting strategy: {e}")
            return False
    
    def export_memory_report(
        self,
        output_path: str,
        format: str = "json"
    ) -> bool:
        """
        导出记忆报告
        
        Args:
            output_path: 输出文件路径
            format: 导出格式
            
        Returns:
            bool: 是否成功
        """
        try:
            stats = self.get_memory_stats()
            
            if format == "json":
                data = {
                    "timestamp": datetime.now().isoformat(),
                    "stats": asdict(stats),
                    "summary": {
                        "total_memories": stats.total_count,
                        "total_size_mb": stats.total_size_bytes / (1024 * 1024),
                        "l1_percentage": (
                            stats.l1_count / stats.total_count * 100
                            if stats.total_count > 0 else 0
                        ),
                        "l2_percentage": (
                            stats.l2_count / stats.total_count * 100
                            if stats.total_count > 0 else 0
                        ),
                        "l3_percentage": (
                            stats.l3_count / stats.total_count * 100
                            if stats.total_count > 0 else 0
                        ),
                        "l4_percentage": (
                            stats.l4_count / stats.total_count * 100
                            if stats.total_count > 0 else 0
                        )
                    }
                }
                
                with open(output_path, 'w', encoding='utf-8') as f:
                    json.dump(data, f, indent=2, ensure_ascii=False)
                
                logger.info(f"Exported memory report to: {output_path}")
                return True
            else:
                logger.error(f"Unsupported export format: {format}")
                return False
                
        except Exception as e:
            logger.error(f"Failed to export memory report: {e}")
            return False
    
    def _get_memory_api(self) -> Optional[Any]:
        """
        获取 MemoryRovol API（模拟实现）
        
        实际项目中应集成真实的 MemoryRovol 接口
        """
        try:
            from agentos.syscalls import memory as memory_api
            return memory_api
        except ImportError:
            logger.warning("MemoryRovol API not available, using mock implementation")
            return None
    
    def _load_stats(self) -> None:
        """加载统计信息"""
        try:
            stats_file = os.path.join(self.stats_dir, "memory_stats.json")
            
            if os.path.exists(stats_file):
                with open(stats_file, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                
                self._stats = MemoryStats(**data)
                logger.info("Loaded memory stats")
            
        except Exception as e:
            logger.error(f"Failed to load memory stats: {e}")


def get_memory_manager(
    stats_dir: Optional[str] = None,
    **kwargs
) -> MemoryManager:
    """
    获取全局记忆管理器实例
    
    Args:
        stats_dir: 统计目录
        **kwargs: 其他参数
        
    Returns:
        MemoryManager: 记忆管理器实例
    """
    global _global_memory_manager
    
    if '_global_memory_manager' not in globals():
        _global_memory_manager = MemoryManager(
            stats_dir=stats_dir,
            **kwargs
        )
    
    return _global_memory_manager
