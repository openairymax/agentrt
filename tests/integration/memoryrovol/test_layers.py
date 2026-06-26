# AgentRT 记忆层集成测试
# Version: 0.1.0
# Last updated: 2026-03-23

"""
AgentRT 记忆层集成测试模块。

测试四层记忆系统（L1原始卷、L2特征层、L3结构层、L4模式层）的功能和集成。
"""

import pytest
import json
import time
import hashlib
import sqlite3
import tempfile
import os
from typing import Dict, Any, List, Optional, Tuple
from unittest.mock import Mock, MagicMock, patch
from dataclasses import dataclass, field
from enum import Enum
from datetime import datetime
from pathlib import Path

import sys
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', 'toolkit', 'python')))


# ============================================================
# 测试标记
# ============================================================

pytestmark = pytest.mark.integration


# ============================================================
# 枚举和数据类定义
# ============================================================

class MemoryLayer(Enum):
    """记忆层级枚举"""
    L1_RAW = "l1_raw"
    L2_FEATURE = "l2_feature"
    L3_STRUCTURE = "l3_structure"
    L4_PATTERN = "l4_pattern"


class MemoryType(Enum):
    """记忆类型枚举"""
    EPISODIC = "episodic"
    SEMANTIC = "semantic"
    PROCEDURAL = "procedural"
    WORKING = "working"


class MemoryPriority(Enum):
    """记忆优先级枚举"""
    LOW = 1
    MEDIUM = 2
    HIGH = 3
    CRITICAL = 4


@dataclass
class MemoryEntry:
    """记忆条目"""
    entry_id: str
    content: str
    memory_type: MemoryType
    layer: MemoryLayer
    timestamp: float
    metadata: Dict[str, Any] = field(default_factory=dict)
    embedding: Optional[List[float]] = None
    importance: float = 0.5
    access_count: int = 0
    last_access: Optional[float] = None


@dataclass
class MemoryQuery:
    """记忆查询"""
    query_text: str
    query_embedding: Optional[List[float]] = None
    layer_filter: Optional[MemoryLayer] = None
    type_filter: Optional[MemoryType] = None
    time_range: Optional[Tuple[float, float]] = None
    limit: int = 10
    min_importance: float = 0.0


@dataclass
class MemoryQueryResult:
    """记忆查询结果"""
    entries: List[MemoryEntry]
    total_count: int
    query_time_ms: float


@dataclass
class BudgetConfig:
    """
    各层内存预算配置。

    预算值以 MB 为单位，用于控制各层的最大内存使用量。
    当超过预算时触发降级策略。
    """
    l1_budget_mb: int = 512      # L1 原始卷：512MB
    l2_budget_mb: int = 1024     # L2 特征层：1024MB
    l3_budget_mb: int = 256      # L3 结构层：256MB
    l4_budget_mb: int = 128      # L4 模式层：128MB
    retrieval_cache_mb: int = 64  # 检索缓存：64MB

    def to_dict(self) -> Dict[str, int]:
        return {
            "l1_budget_mb": self.l1_budget_mb,
            "l2_budget_mb": self.l2_budget_mb,
            "l3_budget_mb": self.l3_budget_mb,
            "l4_budget_mb": self.l4_budget_mb,
            "retrieval_cache_mb": self.retrieval_cache_mb,
        }


class MemoryBudgetManager:
    """
    内存预算管理器。

    管理四层记忆系统的内存预算分配和超限检测。
    当任一层超过预算时触发降级回调。
    """

    def __init__(self, budget: BudgetConfig):
        """
        初始化预算管理器。

        Args:
            budget: 预算配置
        """
        self.budget = budget
        self._current_usage: Dict[str, float] = {
            "l1": 0.0,
            "l2": 0.0,
            "l3": 0.0,
            "l4": 0.0,
            "retrieval_cache": 0.0,
        }
        self._degradation_events: List[Dict[str, Any]] = []
        self._degradation_handlers: Dict[str, callable] = {}

    def register_degradation_handler(self, layer: str, handler: callable) -> None:
        """
        注册降级处理回调。

        Args:
            layer: 层级名称
            handler: 降级回调函数，接收 (layer, current_usage, budget) 参数
        """
        self._degradation_handlers[layer] = handler

    def get_budget_mb(self, layer: str) -> int:
        """
        获取指定层的预算（MB）。

        Args:
            layer: 层级名称

        Returns:
            int: 预算值（MB）
        """
        mapping = {
            "l1": self.budget.l1_budget_mb,
            "l2": self.budget.l2_budget_mb,
            "l3": self.budget.l3_budget_mb,
            "l4": self.budget.l4_budget_mb,
            "retrieval_cache": self.budget.retrieval_cache_mb,
        }
        return mapping.get(layer, 0)

    def update_usage(self, layer: str, usage_mb: float) -> Dict[str, Any]:
        """
        更新某层的当前使用量，检测是否超限。

        Args:
            layer: 层级名称
            usage_mb: 当前使用量（MB）

        Returns:
            Dict: 包含 is_over, over_by_mb, degradation_triggered 等字段
        """
        self._current_usage[layer] = usage_mb
        budget_mb = self.get_budget_mb(layer)
        is_over = usage_mb > budget_mb
        over_by_mb = max(0.0, usage_mb - budget_mb)

        degradation_triggered = False
        if is_over and layer in self._degradation_handlers:
            event = {
                "layer": layer,
                "current_usage_mb": usage_mb,
                "budget_mb": budget_mb,
                "over_by_mb": over_by_mb,
                "timestamp": time.time(),
            }
            self._degradation_events.append(event)
            self._degradation_handlers[layer](layer, usage_mb, budget_mb)
            degradation_triggered = True

        return {
            "layer": layer,
            "current_usage_mb": usage_mb,
            "budget_mb": budget_mb,
            "is_over": is_over,
            "over_by_mb": over_by_mb,
            "degradation_triggered": degradation_triggered,
        }

    def get_current_usage(self, layer: str) -> float:
        """获取当前使用量"""
        return self._current_usage.get(layer, 0.0)

    def get_degradation_events(self) -> List[Dict[str, Any]]:
        """获取所有降级事件"""
        return self._degradation_events

    def is_any_over_budget(self) -> bool:
        """检查是否有任一层超限"""
        for layer in ["l1", "l2", "l3", "l4", "retrieval_cache"]:
            if self._current_usage[layer] > self.get_budget_mb(layer):
                return True
        return False

    def reset(self) -> None:
        """重置预算管理器状态"""
        self._current_usage = {k: 0.0 for k in self._current_usage}
        self._degradation_events = []


# ============================================================
# LRU 缓存实现（用于检索缓存驱逐测试）
# ============================================================

class LRUCache:
    """
    LRU（最近最少使用）缓存。

    用于检索缓存管理，当缓存超过预算时驱逐最久未使用的条目。
    """

    def __init__(self, max_size_mb: int):
        """
        初始化 LRU 缓存。

        Args:
            max_size_mb: 最大缓存大小（MB）
        """
        self.max_size_mb = max_size_mb
        self._cache: Dict[str, Any] = {}
        self._access_order: List[str] = []
        self._current_size_mb = 0.0
        self._eviction_count = 0

    def _estimate_entry_size_mb(self, entry: Any) -> float:
        """
        估算条目大小（MB）。

        Args:
            entry: 缓存条目

        Returns:
            float: 估算大小（MB）
        """
        if isinstance(entry, MemoryEntry):
            size = len(entry.content.encode("utf-8")) if entry.content else 0
            size += len(json.dumps(entry.metadata)) if entry.metadata else 0
            if entry.embedding:
                size += len(entry.embedding) * 8
            return size / (1024 * 1024)
        elif isinstance(entry, list):
            return sum(len(json.dumps(e)) for e in entry) / (1024 * 1024)
        return 0.01

    def put(self, key: str, value: Any) -> List[str]:
        """
        放入缓存条目，如果超限则驱逐旧条目。

        Args:
            key: 键
            value: 值

        Returns:
            List[str]: 被驱逐的键列表
        """
        entry_size = self._estimate_entry_size_mb(value)

        # 如果 key 已存在，先移除旧的
        if key in self._cache:
            old_size = self._estimate_entry_size_mb(self._cache[key])
            self._current_size_mb -= old_size
            self._access_order.remove(key)
            del self._cache[key]

        evicted = []
        while self._current_size_mb + entry_size > self.max_size_mb and self._access_order:
            # 驱逐最久未使用的条目
            evicted_key = self._access_order.pop(0)
            evicted_size = self._estimate_entry_size_mb(self._cache[evicted_key])
            self._current_size_mb -= evicted_size
            del self._cache[evicted_key]
            self._eviction_count += 1
            evicted.append(evicted_key)

        self._cache[key] = value
        self._access_order.append(key)
        self._current_size_mb += entry_size

        return evicted

    def get(self, key: str) -> Optional[Any]:
        """
        获取缓存条目，更新访问时间。

        Args:
            key: 键

        Returns:
            Optional[Any]: 缓存值
        """
        if key in self._cache:
            # 更新访问顺序
            self._access_order.remove(key)
            self._access_order.append(key)
            return self._cache[key]
        return None

    def remove(self, key: str) -> bool:
        """移除缓存条目"""
        if key in self._cache:
            entry_size = self._estimate_entry_size_mb(self._cache[key])
            self._current_size_mb -= entry_size
            self._access_order.remove(key)
            del self._cache[key]
            return True
        return False

    @property
    def current_size_mb(self) -> float:
        return self._current_size_mb

    @property
    def size(self) -> int:
        return len(self._cache)

    @property
    def eviction_count(self) -> int:
        return self._eviction_count


# ============================================================
# 记忆层实现
# ============================================================

class L1RawLayer:
    """
    L1 原始卷层。

    负责原始记忆的存储和检索，使用 SQLite 作为存储后端。
    类比：海马体 CA3 区
    """

    def __init__(self, storage_path: str):
        """
        初始化 L1 原始卷层。

        Args:
            storage_path: 存储路径
        """
        self.storage_path = storage_path
        self._init_storage()

    def _init_storage(self) -> None:
        """初始化存储"""
        os.makedirs(os.path.dirname(self.storage_path), exist_ok=True)
        self._conn = sqlite3.connect(self.storage_path)
        self._create_tables()

    def _create_tables(self) -> None:
        """创建数据库表"""
        cursor = self._conn.cursor()
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS memories (
                entry_id TEXT PRIMARY KEY,
                content TEXT NOT NULL,
                memory_type TEXT NOT NULL,
                layer TEXT NOT NULL,
                timestamp REAL NOT NULL,
                metadata TEXT,
                importance REAL DEFAULT 0.5,
                access_count INTEGER DEFAULT 0,
                last_access REAL
            )
        ''')
        cursor.execute('CREATE INDEX IF NOT EXISTS idx_timestamp ON memories(timestamp)')
        cursor.execute('CREATE INDEX IF NOT EXISTS idx_type ON memories(memory_type)')
        self._conn.commit()

    def store(self, entry: MemoryEntry) -> bool:
        """
        存储记忆条目。

        Args:
            entry: 记忆条目

        Returns:
            bool: 是否成功存储
        """
        try:
            cursor = self._conn.cursor()
            cursor.execute('''
                INSERT OR REPLACE INTO memories
                (entry_id, content, memory_type, layer, timestamp, metadata, importance, access_count, last_access)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
            ''', (
                entry.entry_id,
                entry.content,
                entry.memory_type.value,
                entry.layer.value,
                entry.timestamp,
                json.dumps(entry.metadata),
                entry.importance,
                entry.access_count,
                entry.last_access
            ))
            self._conn.commit()

            return True
        except Exception:
            return False

    def retrieve(self, entry_id: str) -> Optional[MemoryEntry]:
        """
        检索记忆条目。

        Args:
            entry_id: 条目 ID

        Returns:
            Optional[MemoryEntry]: 记忆条目
        """
        cursor = self._conn.cursor()
        cursor.execute('SELECT * FROM memories WHERE entry_id = ?', (entry_id,))
        row = cursor.fetchone()

        if row:
            return self._row_to_entry(row)

        return None

    def query(self, query: MemoryQuery) -> List[MemoryEntry]:
        """
        查询记忆条目。

        Args:
            query: 查询条件

        Returns:
            List[MemoryEntry]: 查询结果
        """
        cursor = self._conn.cursor()
        sql = 'SELECT * FROM memories WHERE 1=1'
        params = []

        if query.type_filter:
            sql += ' AND memory_type = ?'
            params.append(query.type_filter.value)

        if query.time_range:
            sql += ' AND timestamp BETWEEN ? AND ?'
            params.extend(query.time_range)

        if query.min_importance > 0:
            sql += ' AND importance >= ?'
            params.append(query.min_importance)

        sql += f' ORDER BY timestamp DESC LIMIT {query.limit}'

        cursor.execute(sql, params)
        rows = cursor.fetchall()

        return [self._row_to_entry(row) for row in rows]

    def delete(self, entry_id: str) -> bool:
        """
        删除记忆条目。

        Args:
            entry_id: 条目 ID

        Returns:
            bool: 是否成功删除
        """
        cursor = self._conn.cursor()
        cursor.execute('DELETE FROM memories WHERE entry_id = ?', (entry_id,))
        self._conn.commit()

        return cursor.rowcount > 0

    def count(self) -> int:
        """
        获取记忆条目数量。

        Returns:
            int: 条目数量
        """
        cursor = self._conn.cursor()
        cursor.execute('SELECT COUNT(*) FROM memories')

        return cursor.fetchone()[0]

    def _row_to_entry(self, row: tuple) -> MemoryEntry:
        """将数据库行转换为记忆条目"""
        return MemoryEntry(
            entry_id=row[0],
            content=row[1],
            memory_type=MemoryType(row[2]),
            layer=MemoryLayer(row[3]),
            timestamp=row[4],
            metadata=json.loads(row[5]) if row[5] else {},
            importance=row[6],
            access_count=row[7],
            last_access=row[8]
        )

    def estimated_size_mb(self) -> float:
        """
        估算当前存储占用（MB）。

        Returns:
            float: 估算大小（MB）
        """
        cursor = self._conn.cursor()
        cursor.execute('SELECT SUM(LENGTH(content) + LENGTH(COALESCE(metadata, ""))) FROM memories')
        result = cursor.fetchone()[0]
        return (result or 0) / (1024 * 1024)

    def compress_low_importance(self, threshold: float = 0.3) -> int:
        """
        压缩低重要性记忆：将重要性低于阈值的条目内容截断。
        这是 L1 超限时的降级策略之一。

        Args:
            threshold: 重要性阈值

        Returns:
            int: 被压缩的条目数
        """
        cursor = self._conn.cursor()
        cursor.execute(
            'UPDATE memories SET content = SUBSTR(content, 1, 100) || "...[compressed]" '
            'WHERE importance < ? AND LENGTH(content) > 100',
            (threshold,)
        )
        self._conn.commit()
        return cursor.rowcount

    def prune_by_importance(self, threshold: float = 0.1) -> int:
        """
        按重要性裁剪：删除重要性低于阈值的条目。
        这是 L1 超限时配合遗忘模块的降级策略。

        Args:
            threshold: 重要性阈值

        Returns:
            int: 被裁剪的条目数
        """
        cursor = self._conn.cursor()
        cursor.execute('DELETE FROM memories WHERE importance < ?', (threshold,))
        self._conn.commit()
        return cursor.rowcount

    def close(self) -> None:
        """关闭存储"""
        self._conn.close()


class L2FeatureLayer:
    """
    L2 特征层。

    负责记忆的特征提取和向量索引。
    类比：内嗅皮层
    """

    def __init__(self, embedding_dim: int = 128):
        """
        初始化 L2 特征层。

        Args:
            embedding_dim: 嵌入向量维度
        """
        self.embedding_dim = embedding_dim
        self._embeddings: Dict[str, List[float]] = {}
        self._index: Dict[str, List[str]] = {}

    def add_embedding(self, entry_id: str, embedding: List[float]) -> bool:
        """
        添加嵌入向量。

        Args:
            entry_id: 条目 ID
            embedding: 嵌入向量

        Returns:
            bool: 是否成功添加
        """
        if len(embedding) != self.embedding_dim:
            return False

        self._embeddings[entry_id] = embedding

        return True

    def search_similar(self, query_embedding: List[float], top_k: int = 10) -> List[Tuple[str, float]]:
        """
        搜索相似记忆。

        Args:
            query_embedding: 查询嵌入向量
            top_k: 返回数量

        Returns:
            List[Tuple[str, float]]: (条目 ID, 相似度) 列表
        """
        if len(query_embedding) != self.embedding_dim:
            return []

        similarities = []

        for entry_id, embedding in self._embeddings.items():
            similarity = self._cosine_similarity(query_embedding, embedding)
            similarities.append((entry_id, similarity))

        similarities.sort(key=lambda x: x[1], reverse=True)

        return similarities[:top_k]

    def _cosine_similarity(self, a: List[float], b: List[float]) -> float:
        """计算余弦相似度"""
        dot_product = sum(x * y for x, y in zip(a, b))
        norm_a = sum(x * x for x in a) ** 0.5
        norm_b = sum(x * x for x in b) ** 0.5

        if norm_a == 0 or norm_b == 0:
            return 0.0

        return dot_product / (norm_a * norm_b)

    def get_embedding(self, entry_id: str) -> Optional[List[float]]:
        """
        获取嵌入向量。

        Args:
            entry_id: 条目 ID

        Returns:
            Optional[List[float]]: 嵌入向量
        """
        return self._embeddings.get(entry_id)

    def remove_embedding(self, entry_id: str) -> bool:
        """
        移除嵌入向量。

        Args:
            entry_id: 条目 ID

        Returns:
            bool: 是否成功移除
        """
        if entry_id in self._embeddings:
            del self._embeddings[entry_id]

            return True

        return False

    def count(self) -> int:
        """
        获取嵌入向量数量。

        Returns:
            int: 向量数量
        """
        return len(self._embeddings)

    def estimated_size_mb(self) -> float:
        """
        估算当前嵌入向量占用（MB）。

        Returns:
            float: 估算大小（MB）
        """
        return self.count() * self.embedding_dim * 4 / (1024 * 1024)

    def reduce_dimension(self, target_dim: int) -> int:
        """
        降维：将所有嵌入向量降维到目标维度。
        这是 L2 超限时的降级策略，通过 PCA 简化（此处用均值池化模拟）。

        Args:
            target_dim: 目标维度

        Returns:
            int: 降维后的向量数量
        """
        if target_dim >= self.embedding_dim or target_dim < 1:
            return self.count()

        reduced_count = 0
        for entry_id, embedding in list(self._embeddings.items()):
            reduced = []
            step = len(embedding) // target_dim
            if step < 1:
                step = 1
            for i in range(target_dim):
                start = i * step
                end = min(start + step, len(embedding))
                reduced.append(sum(embedding[start:end]) / (end - start))
            self._embeddings[entry_id] = reduced
            reduced_count += 1

        self.embedding_dim = target_dim
        return reduced_count


class L3StructureLayer:
    """
    L3 结构层。

    负责记忆的结构化组织和关系编码。
    类比：海马-新皮层通路
    """

    def __init__(self):
        """初始化 L3 结构层"""
        self._relations: Dict[str, List[Tuple[str, str]]] = {}
        self._clusters: Dict[str, List[str]] = {}

    def add_relation(self, source_id: str, target_id: str, relation_type: str) -> bool:
        """
        添加关系。

        Args:
            source_id: 源条目 ID
            target_id: 目标条目 ID
            relation_type: 关系类型

        Returns:
            bool: 是否成功添加
        """
        if source_id not in self._relations:
            self._relations[source_id] = []

        self._relations[source_id].append((target_id, relation_type))

        return True

    def get_relations(self, entry_id: str) -> List[Tuple[str, str]]:
        """
        获取条目的所有关系。

        Args:
            entry_id: 条目 ID

        Returns:
            List[Tuple[str, str]]: (目标 ID, 关系类型) 列表
        """
        return self._relations.get(entry_id, [])

    def create_cluster(self, cluster_id: str, entry_ids: List[str]) -> bool:
        """
        创建记忆簇。

        Args:
            cluster_id: 簇 ID
            entry_ids: 条目 ID 列表

        Returns:
            bool: 是否成功创建
        """
        self._clusters[cluster_id] = entry_ids

        return True

    def get_cluster(self, cluster_id: str) -> List[str]:
        """
        获取记忆簇。

        Args:
            cluster_id: 簇 ID

        Returns:
            List[str]: 条目 ID 列表
        """
        return self._clusters.get(cluster_id, [])

    def add_to_cluster(self, cluster_id: str, entry_id: str) -> bool:
        """
        将条目添加到簇。

        Args:
            cluster_id: 簇 ID
            entry_id: 条目 ID

        Returns:
            bool: 是否成功添加
        """
        if cluster_id not in self._clusters:
            self._clusters[cluster_id] = []

        if entry_id not in self._clusters[cluster_id]:
            self._clusters[cluster_id].append(entry_id)

        return True

    def find_related(self, entry_id: str, max_depth: int = 2) -> List[str]:
        """
        查找相关条目。

        Args:
            entry_id: 起始条目 ID
            max_depth: 最大搜索深度

        Returns:
            List[str]: 相关条目 ID 列表
        """
        visited = set()
        result = []
        queue = [(entry_id, 0)]

        while queue:
            current_id, depth = queue.pop(0)

            if current_id in visited or depth > max_depth:
                continue

            visited.add(current_id)

            if current_id != entry_id:
                result.append(current_id)

            for target_id, _ in self.get_relations(current_id):
                if target_id not in visited:
                    queue.append((target_id, depth + 1))

        return result


class L4PatternLayer:
    """
    L4 模式层。

    负责记忆模式的抽象和规则生成。
    类比：前额叶皮层
    """

    def __init__(self):
        """初始化 L4 模式层"""
        self._patterns: Dict[str, Dict[str, Any]] = {}
        self._rules: Dict[str, Dict[str, Any]] = {}

    def extract_pattern(self, pattern_id: str, entries: List[MemoryEntry]) -> Dict[str, Any]:
        """
        提取模式。

        Args:
            pattern_id: 模式 ID
            entries: 记忆条目列表

        Returns:
            Dict[str, Any]: 提取的模式
        """
        pattern = {
            'id': pattern_id,
            'entry_count': len(entries),
            'types': {},
            'avg_importance': 0.0,
            'time_span': None,
            'keywords': []
        }

        if not entries:
            return pattern

        type_counts: Dict[str, int] = {}
        total_importance = 0.0
        timestamps = []

        for entry in entries:
            type_key = entry.memory_type.value
            type_counts[type_key] = type_counts.get(type_key, 0) + 1
            total_importance += entry.importance
            timestamps.append(entry.timestamp)

        pattern['types'] = type_counts
        pattern['avg_importance'] = total_importance / len(entries)
        pattern['time_span'] = (min(timestamps), max(timestamps))

        self._patterns[pattern_id] = pattern

        return pattern

    def get_pattern(self, pattern_id: str) -> Optional[Dict[str, Any]]:
        """
        获取模式。

        Args:
            pattern_id: 模式 ID

        Returns:
            Optional[Dict[str, Any]]: 模式数据
        """
        return self._patterns.get(pattern_id)

    def create_rule(self, rule_id: str, condition: Dict[str, Any], action: Dict[str, Any]) -> bool:
        """
        创建规则。

        Args:
            rule_id: 规则 ID
            condition: 条件
            action: 动作

        Returns:
            bool: 是否成功创建
        """
        self._rules[rule_id] = {
            'id': rule_id,
            'condition': condition,
            'action': action,
            'created_at': time.time()
        }

        return True

    def get_rule(self, rule_id: str) -> Optional[Dict[str, Any]]:
        """
        获取规则。

        Args:
            rule_id: 规则 ID

        Returns:
            Optional[Dict[str, Any]]: 规则数据
        """
        return self._rules.get(rule_id)

    def apply_rules(self, context: Dict[str, Any]) -> List[Dict[str, Any]]:
        """
        应用规则。

        Args:
            context: 上下文

        Returns:
            List[Dict[str, Any]]: 匹配的规则动作列表
        """
        actions = []

        for rule in self._rules.values():
            if self._match_condition(rule['condition'], context):
                actions.append(rule['action'])

        return actions

    def _match_condition(self, condition: Dict[str, Any], context: Dict[str, Any]) -> bool:
        """匹配条件"""
        for key, value in condition.items():
            if key not in context:
                return False

            if context[key] != value:
                return False

        return True


# ============================================================
# 测试用例
# ============================================================

class TestL1RawLayer:
    """L1 原始卷层测试"""

    @pytest.fixture
    def temp_storage(self, tmp_path) -> str:
        """
        提供临时存储路径。

        Returns:
            str: 存储路径
        """
        return str(tmp_path / "test_memories.db")

    @pytest.fixture
    def l1_layer(self, temp_storage) -> L1RawLayer:
        """
        提供 L1 层实例。

        Returns:
            L1RawLayer: L1 层实例
        """
        return L1RawLayer(temp_storage)

    @pytest.fixture
    def sample_entry(self) -> MemoryEntry:
        """
        提供示例记忆条目。

        Returns:
            MemoryEntry: 记忆条目
        """
        return MemoryEntry(
            entry_id="test_entry_001",
            content="这是一条测试记忆内容",
            memory_type=MemoryType.EPISODIC,
            layer=MemoryLayer.L1_RAW,
            timestamp=time.time(),
            metadata={"source": "test", "tags": ["test", "sample"]},
            importance=0.8
        )

    def test_store_and_retrieve(self, l1_layer, sample_entry):
        """
        测试存储和检索。

        验证:
            - 记忆条目被正确存储
            - 记忆条目被正确检索
        """
        l1_layer.store(sample_entry)
        retrieved = l1_layer.retrieve(sample_entry.entry_id)

        assert retrieved is not None
        assert retrieved.entry_id == sample_entry.entry_id
        assert retrieved.content == sample_entry.content
        assert retrieved.memory_type == sample_entry.memory_type

    def test_delete_entry(self, l1_layer, sample_entry):
        """
        测试删除条目。

        验证:
            - 记忆条目被正确删除
        """
        l1_layer.store(sample_entry)
        result = l1_layer.delete(sample_entry.entry_id)

        assert result is True
        assert l1_layer.retrieve(sample_entry.entry_id) is None

    def test_query_by_type(self, l1_layer):
        """
        测试按类型查询。

        验证:
            - 按类型查询返回正确结果
        """
        entries = [
            MemoryEntry(
                entry_id=f"entry_{i}",
                content=f"内容 {i}",
                memory_type=MemoryType.EPISODIC if i % 2 == 0 else MemoryType.SEMANTIC,
                layer=MemoryLayer.L1_RAW,
                timestamp=time.time() + i
            )
            for i in range(10)
        ]

        for entry in entries:
            l1_layer.store(entry)

        query = MemoryQuery(query_text="", type_filter=MemoryType.EPISODIC, limit=100)
        results = l1_layer.query(query)

        assert len(results) == 5
        for entry in results:
            assert entry.memory_type == MemoryType.EPISODIC

    def test_query_by_time_range(self, l1_layer):
        """
        测试按时间范围查询。

        验证:
            - 按时间范围查询返回正确结果
        """
        base_time = time.time()

        for i in range(5):
            entry = MemoryEntry(
                entry_id=f"time_entry_{i}",
                content=f"时间内容 {i}",
                memory_type=MemoryType.EPISODIC,
                layer=MemoryLayer.L1_RAW,
                timestamp=base_time + i * 100
            )
            l1_layer.store(entry)

        query = MemoryQuery(
            query_text="",
            time_range=(base_time + 50, base_time + 250),
            limit=100
        )
        results = l1_layer.query(query)

        assert len(results) == 2

    def test_count(self, l1_layer):
        """
        测试计数。

        验证:
            - 计数返回正确数量
        """
        assert l1_layer.count() == 0

        for i in range(5):
            entry = MemoryEntry(
                entry_id=f"count_entry_{i}",
                content=f"计数内容 {i}",
                memory_type=MemoryType.EPISODIC,
                layer=MemoryLayer.L1_RAW,
                timestamp=time.time()
            )
            l1_layer.store(entry)

        assert l1_layer.count() == 5


class TestL2FeatureLayer:
    """L2 特征层测试"""

    @pytest.fixture
    def l2_layer(self) -> L2FeatureLayer:
        """
        提供 L2 层实例。

        Returns:
            L2FeatureLayer: L2 层实例
        """
        return L2FeatureLayer(embedding_dim=64)

    def test_add_embedding(self, l2_layer):
        """
        测试添加嵌入向量。

        验证:
            - 嵌入向量被正确添加
        """
        embedding = [0.1] * 64
        result = l2_layer.add_embedding("entry_001", embedding)

        assert result is True
        assert l2_layer.get_embedding("entry_001") == embedding

    def test_add_invalid_embedding(self, l2_layer):
        """
        测试添加无效嵌入向量。

        验证:
            - 维度不匹配的嵌入向量被拒绝
        """
        embedding = [0.1] * 32
        result = l2_layer.add_embedding("entry_001", embedding)

        assert result is False

    def test_search_similar(self, l2_layer):
        """
        测试相似搜索。

        验证:
            - 相似搜索返回正确结果
        """
        for i in range(10):
            # Use orthogonal-like embeddings to distinguish them
            embedding = [0.0] * 64
            embedding[i % 64] = 1.0 if i == 5 else 0.1 * (i + 1)
            l2_layer.add_embedding(f"entry_{i}", embedding)

        query_embedding = [0.0] * 64
        query_embedding[5] = 1.0  # Perfect match with entry_5
        results = l2_layer.search_similar(query_embedding, top_k=3)

        assert len(results) == 3
        assert results[0][0] == "entry_5"

    def test_remove_embedding(self, l2_layer):
        """
        测试移除嵌入向量。

        验证:
            - 嵌入向量被正确移除
        """
        embedding = [0.1] * 64
        l2_layer.add_embedding("entry_001", embedding)

        result = l2_layer.remove_embedding("entry_001")

        assert result is True
        assert l2_layer.get_embedding("entry_001") is None

    def test_count(self, l2_layer):
        """
        测试计数。

        验证:
            - 计数返回正确数量
        """
        assert l2_layer.count() == 0

        for i in range(5):
            embedding = [float(i)] * 64
            l2_layer.add_embedding(f"entry_{i}", embedding)

        assert l2_layer.count() == 5


class TestL3StructureLayer:
    """L3 结构层测试"""

    @pytest.fixture
    def l3_layer(self) -> L3StructureLayer:
        """
        提供 L3 层实例。

        Returns:
            L3StructureLayer: L3 层实例
        """
        return L3StructureLayer()

    def test_add_relation(self, l3_layer):
        """
        测试添加关系。

        验证:
            - 关系被正确添加
        """
        result = l3_layer.add_relation("entry_001", "entry_002", "related_to")

        assert result is True

        relations = l3_layer.get_relations("entry_001")
        assert len(relations) == 1
        assert relations[0] == ("entry_002", "related_to")

    def test_create_cluster(self, l3_layer):
        """
        测试创建记忆簇。

        验证:
            - 记忆簇被正确创建
        """
        entry_ids = ["entry_001", "entry_002", "entry_003"]
        result = l3_layer.create_cluster("cluster_001", entry_ids)

        assert result is True

        cluster = l3_layer.get_cluster("cluster_001")
        assert cluster == entry_ids

    def test_add_to_cluster(self, l3_layer):
        """
        测试添加到簇。

        验证:
            - 条目被正确添加到簇
        """
        l3_layer.create_cluster("cluster_001", ["entry_001"])
        l3_layer.add_to_cluster("cluster_001", "entry_002")

        cluster = l3_layer.get_cluster("cluster_001")
        assert "entry_002" in cluster

    def test_find_related(self, l3_layer):
        """
        测试查找相关条目。

        验证:
            - 相关条目被正确查找
        """
        l3_layer.add_relation("entry_001", "entry_002", "related_to")
        l3_layer.add_relation("entry_002", "entry_003", "related_to")
        l3_layer.add_relation("entry_003", "entry_004", "related_to")

        related = l3_layer.find_related("entry_001", max_depth=2)

        assert "entry_002" in related
        assert "entry_003" in related
        assert "entry_004" not in related


class TestL4PatternLayer:
    """L4 模式层测试"""

    @pytest.fixture
    def l4_layer(self) -> L4PatternLayer:
        """
        提供 L4 层实例。

        Returns:
            L4PatternLayer: L4 层实例
        """
        return L4PatternLayer()

    @pytest.fixture
    def sample_entries(self) -> List[MemoryEntry]:
        """
        提供示例记忆条目列表。

        Returns:
            List[MemoryEntry]: 记忆条目列表
        """
        base_time = time.time()

        return [
            MemoryEntry(
                entry_id=f"pattern_entry_{i}",
                content=f"模式内容 {i}",
                memory_type=MemoryType.EPISODIC if i < 3 else MemoryType.SEMANTIC,
                layer=MemoryLayer.L1_RAW,
                timestamp=base_time + i * 100,
                importance=0.5 + i * 0.1
            )
            for i in range(5)
        ]

    def test_extract_pattern(self, l4_layer, sample_entries):
        """
        测试提取模式。

        验证:
            - 模式被正确提取
        """
        pattern = l4_layer.extract_pattern("pattern_001", sample_entries)

        assert pattern['entry_count'] == 5
        assert pattern['types']['episodic'] == 3
        assert pattern['types']['semantic'] == 2
        assert pattern['avg_importance'] > 0

    def test_get_pattern(self, l4_layer, sample_entries):
        """
        测试获取模式。

        验证:
            - 模式被正确获取
        """
        l4_layer.extract_pattern("pattern_001", sample_entries)
        pattern = l4_layer.get_pattern("pattern_001")

        assert pattern is not None
        assert pattern['id'] == "pattern_001"

    def test_create_rule(self, l4_layer):
        """
        测试创建规则。

        验证:
            - 规则被正确创建
        """
        result = l4_layer.create_rule(
            "rule_001",
            {"memory_type": "episodic"},
            {"action": "boost_importance"}
        )

        assert result is True

        rule = l4_layer.get_rule("rule_001")
        assert rule is not None
        assert rule['condition'] == {"memory_type": "episodic"}

    def test_apply_rules(self, l4_layer):
        """
        测试应用规则。

        验证:
            - 规则被正确应用
        """
        l4_layer.create_rule(
            "rule_001",
            {"memory_type": "episodic"},
            {"action": "boost_importance"}
        )
        l4_layer.create_rule(
            "rule_002",
            {"memory_type": "semantic"},
            {"action": "archive"}
        )

        context = {"memory_type": "episodic"}
        actions = l4_layer.apply_rules(context)

        assert len(actions) == 1
        assert actions[0]['action'] == "boost_importance"


class TestMemoryLayerIntegration:
    """记忆层集成测试"""

    @pytest.fixture
    def temp_storage(self, tmp_path) -> str:
        """提供临时存储路径"""
        return str(tmp_path / "integration_test.db")

    @pytest.fixture
    def layers(self, temp_storage) -> Tuple[L1RawLayer, L2FeatureLayer, L3StructureLayer, L4PatternLayer]:
        """
        提供所有层实例。

        Returns:
            Tuple: 四层实例
        """
        return (
            L1RawLayer(temp_storage),
            L2FeatureLayer(embedding_dim=64),
            L3StructureLayer(),
            L4PatternLayer()
        )

    def test_full_memory_lifecycle(self, layers):
        """
        测试完整记忆生命周期。

        验证:
            - 记忆在各层之间正确流转
        """
        l1, l2, l3, l4 = layers

        entry = MemoryEntry(
            entry_id="integration_entry_001",
            content="集成测试记忆内容",
            memory_type=MemoryType.EPISODIC,
            layer=MemoryLayer.L1_RAW,
            timestamp=time.time(),
            importance=0.7
        )

        l1.store(entry)

        embedding = [0.5] * 64
        l2.add_embedding(entry.entry_id, embedding)

        l3.create_cluster("test_cluster", [entry.entry_id])

        pattern = l4.extract_pattern("integration_pattern", [entry])

        assert l1.retrieve(entry.entry_id) is not None
        assert l2.get_embedding(entry.entry_id) == embedding
        assert entry.entry_id in l3.get_cluster("test_cluster")
        assert l4.get_pattern("integration_pattern") is not None

    def test_memory_query_flow(self, layers):
        """
        测试记忆查询流程。

        验证:
            - 查询在各层之间正确协作
        """
        l1, l2, l3, l4 = layers

        for i in range(5):
            entry = MemoryEntry(
                entry_id=f"query_entry_{i}",
                content=f"查询测试内容 {i}",
                memory_type=MemoryType.EPISODIC,
                layer=MemoryLayer.L1_RAW,
                timestamp=time.time() + i,
                importance=0.5 + i * 0.1
            )
            l1.store(entry)

            embedding = [float(i) / 10] * 64
            l2.add_embedding(entry.entry_id, embedding)

        query_embedding = [0.3] * 64
        similar = l2.search_similar(query_embedding, top_k=3)
        similar_ids = [entry_id for entry_id, _ in similar]

        query = MemoryQuery(query_text="", limit=10)
        results = l1.query(query)

        for entry_id in similar_ids:
            entry = l1.retrieve(entry_id)
            assert entry is not None

    def test_memory_forgetting_simulation(self, layers):
        """
        测试记忆遗忘模拟。

        验证:
            - 低重要性记忆被正确识别
        """
        l1, l2, l3, l4 = layers

        for i in range(10):
            entry = MemoryEntry(
                entry_id=f"forget_entry_{i}",
                content=f"遗忘测试内容 {i}",
                memory_type=MemoryType.EPISODIC,
                layer=MemoryLayer.L1_RAW,
                timestamp=time.time() - i * 86400,
                importance=0.1 * (10 - i),
                access_count=i
            )
            l1.store(entry)

        query = MemoryQuery(query_text="", min_importance=0.5, limit=10)
        high_importance = l1.query(query)

        assert len(high_importance) > 0
        for entry in high_importance:
            assert entry.importance >= 0.5


# ============================================================
# P3.19: MemoryRovol 内存预算验证（L1~L4 超限降级）
# ============================================================

class TestMemoryBudgetValidation:
    """P3.19: MemoryRovol 内存预算验证测试"""

    # ============================================================
    # P3.19.1: 配置 MemoryRovol 各层内存预算
    # ============================================================

    def test_p3_19_1_budget_configuration(self):
        """
        P3.19.1: 配置 MemoryRovol 各层内存预算。

        验证:
            - 各层预算正确配置：L1: 512MB, L2: 1024MB, L3: 256MB, L4: 128MB
            - 检索缓存预算：64MB
            - BudgetManager 正确读取预算值
            - 各层初始使用量为 0
            - 预算配置可序列化
        """
        budget = BudgetConfig(
            l1_budget_mb=512,
            l2_budget_mb=1024,
            l3_budget_mb=256,
            l4_budget_mb=128,
            retrieval_cache_mb=64,
        )

        manager = MemoryBudgetManager(budget)

        # 验证预算值
        assert manager.get_budget_mb("l1") == 512, "L1 budget should be 512MB"
        assert manager.get_budget_mb("l2") == 1024, "L2 budget should be 1024MB"
        assert manager.get_budget_mb("l3") == 256, "L3 budget should be 256MB"
        assert manager.get_budget_mb("l4") == 128, "L4 budget should be 128MB"
        assert manager.get_budget_mb("retrieval_cache") == 64, "Retrieval cache budget should be 64MB"

        # 验证初始使用量
        assert manager.get_current_usage("l1") == 0.0
        assert manager.get_current_usage("l2") == 0.0
        assert manager.get_current_usage("l3") == 0.0
        assert manager.get_current_usage("l4") == 0.0

        # 验证无超限
        assert not manager.is_any_over_budget(), "No layer should be over budget initially"

        # 验证序列化
        config_dict = budget.to_dict()
        assert config_dict["l1_budget_mb"] == 512
        assert config_dict["l2_budget_mb"] == 1024
        assert config_dict["l3_budget_mb"] == 256
        assert config_dict["l4_budget_mb"] == 128
        assert config_dict["retrieval_cache_mb"] == 64

    def test_p3_19_1_budget_default_config(self):
        """
        P3.19.1: 默认预算配置验证。

        验证:
            - 默认 BudgetConfig 使用正确的默认值
            - 默认配置下各层关系合理（L2 > L1 > L3 > L4）
        """
        budget = BudgetConfig()

        # 验证默认值
        assert budget.l1_budget_mb == 512
        assert budget.l2_budget_mb == 1024
        assert budget.l3_budget_mb == 256
        assert budget.l4_budget_mb == 128
        assert budget.retrieval_cache_mb == 64

        # 验证层次关系：L2 特征层需要最大内存（嵌入向量）
        assert budget.l2_budget_mb > budget.l1_budget_mb, "L2 should have largest budget for embeddings"
        assert budget.l1_budget_mb > budget.l3_budget_mb, "L1 should be larger than L3"
        assert budget.l3_budget_mb > budget.l4_budget_mb, "L3 should be larger than L4"
        assert budget.l4_budget_mb > budget.retrieval_cache_mb, "L4 should be larger than retrieval cache"

    # ============================================================
    # P3.19.2: L2 嵌入计算内存不足 → 降维
    # ============================================================

    def test_p3_19_2_l2_embedding_dimension_reduction(self):
        """
        P3.19.2: L2 嵌入计算内存不足 → 降维。

        验证:
            - L2 层使用量超过预算时触发降维
            - 降维后嵌入向量维度减少
            - 降维后内存占用降低
            - 降维不影响已存储的向量数量
            - 降级事件被正确记录
        """
        budget = BudgetConfig(l2_budget_mb=5)  # 设置很小的 L2 预算以触发降级
        manager = MemoryBudgetManager(budget)

        l2 = L2FeatureLayer(embedding_dim=768)  # 使用 768 维（BERT 风格）

        # 注册 L2 降级处理：降维到 128
        degradation_log = []

        def l2_degradation_handler(layer, current_usage, budget_mb):
            degradation_log.append({
                "layer": layer,
                "before_dim": l2.embedding_dim,
                "usage_mb": current_usage,
                "budget_mb": budget_mb,
            })
            # 降维策略：将维度减半
            target_dim = max(64, l2.embedding_dim // 2)
            l2.reduce_dimension(target_dim)
            degradation_log[-1]["after_dim"] = l2.embedding_dim

        manager.register_degradation_handler("l2", l2_degradation_handler)

        # 添加大量嵌入向量，使 L2 超过预算
        import random
        random.seed(42)
        num_vectors = 10000
        for i in range(num_vectors):
            embedding = [random.random() for _ in range(768)]
            l2.add_embedding(f"vec_{i}", embedding)

        # 估算当前使用量
        current_usage = l2.estimated_size_mb()
        assert current_usage > 0, "L2 should have some usage"

        # 触发预算检查
        result = manager.update_usage("l2", current_usage)

        # 验证超限
        assert result["is_over"], (
            f"L2 should be over budget: usage={current_usage:.2f}MB > budget={budget.l2_budget_mb}MB"
        )

        # 验证降级触发
        assert result["degradation_triggered"], "L2 degradation should be triggered"
        assert len(degradation_log) > 0, "Degradation log should have entries"

        # 验证降维效果
        before_dim = degradation_log[0]["before_dim"]
        after_dim = degradation_log[0]["after_dim"]
        assert after_dim < before_dim, (
            f"Dimension should be reduced: {before_dim} -> {after_dim}"
        )
        assert after_dim == 384, f"Expected 384 after halving, got {after_dim}"

        # 验证降维后向量数量不变
        assert l2.count() == num_vectors, "Vector count should remain unchanged after reduction"

        # 验证降维后内存占用降低
        new_usage = l2.estimated_size_mb()
        assert new_usage < current_usage, (
            f"Memory usage should decrease after reduction: {current_usage:.2f} -> {new_usage:.2f}"
        )

        # 验证降维后仍可检索
        for i in range(10):
            embedding = l2.get_embedding(f"vec_{i}")
            assert embedding is not None, f"vec_{i} should still exist after reduction"
            assert len(embedding) == after_dim, f"vec_{i} should have reduced dimension"

    def test_p3_19_2_l2_no_reduction_when_under_budget(self):
        """
        P3.19.2: L2 在预算内时不触发降维。

        验证:
            - 使用量低于预算时不触发降级
            - 嵌入维度保持不变
        """
        budget = BudgetConfig(l2_budget_mb=1024)
        manager = MemoryBudgetManager(budget)

        l2 = L2FeatureLayer(embedding_dim=128)
        original_dim = l2.embedding_dim

        degradation_triggered = [False]

        def l2_handler(layer, usage, budget_mb):
            degradation_triggered[0] = True

        manager.register_degradation_handler("l2", l2_handler)

        # 添加少量向量
        import random
        for i in range(100):
            embedding = [random.random() for _ in range(128)]
            l2.add_embedding(f"vec_{i}", embedding)

        current_usage = l2.estimated_size_mb()
        result = manager.update_usage("l2", current_usage)

        assert not result["is_over"], "L2 should be under budget"
        assert not result["degradation_triggered"], "No degradation should be triggered"
        assert not degradation_triggered[0], "Handler should not be called"
        assert l2.embedding_dim == original_dim, "Dimension should remain unchanged"

    # ============================================================
    # P3.19.3: 检索缓存超限 → LRU 驱逐
    # ============================================================

    def test_p3_19_3_retrieval_cache_lru_eviction(self):
        """
        P3.19.3: 检索缓存超限 → LRU 驱逐。

        验证:
            - 缓存满载时新条目触发 LRU 驱逐
            - 最久未使用的条目被优先驱逐
            - 最近访问的条目被保留
            - 驱逐计数正确
            - 缓存大小不超过预算
        """
        cache = LRUCache(max_size_mb=0.5)  # 0.5MB 缓存预算

        # 创建模拟检索结果条目
        entries_put = []
        for i in range(200):
            entry = MemoryEntry(
                entry_id=f"cache_entry_{i:04d}",
                content=f"检索结果缓存内容 {i} " + "x" * 8000,  # ~8KB per entry
                memory_type=MemoryType.EPISODIC,
                layer=MemoryLayer.L1_RAW,
                timestamp=time.time(),
                importance=0.5 + (i % 10) * 0.05,
            )
            entries_put.append(entry)
            evicted = cache.put(entry.entry_id, entry)

            if evicted:
                # 驱逐的是最久未使用的
                for evicted_key in evicted:
                    assert evicted_key not in [e.entry_id for e in entries_put[-10:]], (
                        "Recently added entries should not be evicted"
                    )

        # 验证缓存大小不超过预算
        assert cache.current_size_mb <= cache.max_size_mb * 1.1, (
            f"Cache size {cache.current_size_mb:.2f}MB should be near budget {cache.max_size_mb}MB"
        )

        # 验证有驱逐发生
        assert cache.eviction_count > 0, "Some entries should be evicted"

        # 验证最近添加的条目仍在缓存中
        recent_entry = entries_put[-1]
        cached = cache.get(recent_entry.entry_id)
        assert cached is not None, "Most recent entry should still be in cache"

        # 验证最早添加的条目已被驱逐
        first_entry = entries_put[0]
        cached_first = cache.get(first_entry.entry_id)
        assert cached_first is None, "Oldest entry should have been evicted"

    def test_p3_19_3_lru_access_order_updates(self):
        """
        P3.19.3: LRU 访问顺序更新。

        验证:
            - 访问已缓存条目更新其位置
            - 经常访问的条目不被驱逐
            - 未被访问的旧条目被驱逐
        """
        cache = LRUCache(max_size_mb=1)

        # 创建条目
        frequently_accessed = MemoryEntry(
            entry_id="frequent_entry",
            content="经常访问的条目 " + "x" * 4000,  # ~4KB
            memory_type=MemoryType.SEMANTIC,
            layer=MemoryLayer.L1_RAW,
            timestamp=time.time(),
            importance=0.9,
        )

        cache.put(frequently_accessed.entry_id, frequently_accessed)

        # 频繁访问这个条目
        for _ in range(50):
            cache.get(frequently_accessed.entry_id)

        # 放入大量其他条目
        for i in range(200):
            entry = MemoryEntry(
                entry_id=f"bulk_entry_{i:04d}",
                content=f"批量条目 {i} " + "x" * 5000,
                memory_type=MemoryType.EPISODIC,
                layer=MemoryLayer.L1_RAW,
                timestamp=time.time(),
                importance=0.3,
            )
            evicted = cache.put(entry.entry_id, entry)
            # 频繁访问的条目不应被驱逐
            for evicted_key in evicted:
                assert evicted_key != frequently_accessed.entry_id, (
                    "Frequently accessed entry should never be evicted"
                )

        # 确认频繁访问的条目仍在缓存中
        assert cache.get(frequently_accessed.entry_id) is not None, (
            "Frequently accessed entry should remain in cache"
        )

    def test_p3_19_3_retrieval_cache_budget_integration(self):
        """
        P3.19.3: 检索缓存预算集成测试。

        验证:
            - BudgetManager 与 LRUCache 集成
            - 缓存超限触发降级事件
            - 降级事件记录正确
        """
        budget = BudgetConfig(retrieval_cache_mb=1)
        manager = MemoryBudgetManager(budget)
        cache = LRUCache(max_size_mb=1)

        degradation_events = []

        def cache_degradation_handler(layer, usage, budget_mb):
            degradation_events.append({
                "layer": layer,
                "usage_mb": usage,
                "budget_mb": budget_mb,
                "cache_size": cache.size,
                "eviction_count": cache.eviction_count,
            })

        manager.register_degradation_handler("retrieval_cache", cache_degradation_handler)

        # 填充缓存直到超限
        for i in range(300):
            entry = MemoryEntry(
                entry_id=f"integ_entry_{i:04d}",
                content=f"集成测试条目 {i} " + "x" * 5000,
                memory_type=MemoryType.EPISODIC,
                layer=MemoryLayer.L1_RAW,
                timestamp=time.time(),
                importance=0.5,
            )
            cache.put(entry.entry_id, entry)

        # 检查预算
        result = manager.update_usage("retrieval_cache", cache.current_size_mb)

        if result["is_over"]:
            assert result["degradation_triggered"], "Degradation should be triggered when over budget"
            assert len(degradation_events) > 0, "Should have degradation events"

        # 验证缓存状态
        assert cache.size > 0, "Cache should have entries"
        assert cache.current_size_mb <= cache.max_size_mb * 1.1, "Cache should be near budget limit"

    # ============================================================
    # P3.19.4: L1 存储超限 → 压缩 + 遗忘 prune
    # ============================================================

    @pytest.fixture
    def temp_storage_p3_19(self, tmp_path) -> str:
        """提供临时存储路径"""
        return str(tmp_path / "test_budget.db")

    def test_p3_19_4_l1_storage_overflow_compression(self, temp_storage_p3_19):
        """
        P3.19.4: L1 存储超限 → 压缩 + 遗忘 prune。

        验证:
            - L1 使用量超过预算时触发压缩
            - 低重要性条目被压缩（内容截断）
            - 压缩后内存占用降低
            - 高重要性条目不被压缩
            - 降级后触发遗忘裁剪
        """
        budget = BudgetConfig(l1_budget_mb=1)  # 1MB L1 预算
        manager = MemoryBudgetManager(budget)
        l1 = L1RawLayer(temp_storage_p3_19)

        degradation_log = []

        def l1_degradation_handler(layer, usage, budget_mb):
            # 第一步：压缩低重要性记忆
            compressed = l1.compress_low_importance(threshold=0.3)
            # 第二步：如果仍超限，裁剪最低重要性条目
            pruned = 0
            new_usage = l1.estimated_size_mb()
            if new_usage > budget_mb:
                pruned = l1.prune_by_importance(threshold=0.15)

            degradation_log.append({
                "layer": layer,
                "usage_before_mb": usage,
                "budget_mb": budget_mb,
                "compressed_count": compressed,
                "pruned_count": pruned,
                "usage_after_mb": l1.estimated_size_mb(),
            })

        manager.register_degradation_handler("l1", l1_degradation_handler)

        # 填充 L1 存储：写入大量数据
        content_template = "这是 L1 存储超限测试数据条目 {}，包含大量文本内容以模拟实际使用场景。" + "x" * 4000
        for i in range(500):
            entry = MemoryEntry(
                entry_id=f"l1_entry_{i:04d}",
                content=content_template.format(i),
                memory_type=MemoryType.EPISODIC,
                layer=MemoryLayer.L1_RAW,
                timestamp=time.time(),
                importance=0.2 + (i % 10) * 0.08,  # 0.2 ~ 0.92 的重要性分布
                metadata={"index": i, "batch": "overflow_test"},
            )
            l1.store(entry)

        # 估算当前使用量并检查预算
        current_usage = l1.estimated_size_mb()
        result = manager.update_usage("l1", current_usage)

        assert result["is_over"], (
            f"L1 should be over budget: {current_usage:.2f}MB > {budget.l1_budget_mb}MB"
        )
        assert result["degradation_triggered"], "L1 degradation should be triggered"

        # 验证降级日志
        assert len(degradation_log) > 0, "Should have degradation log entries"
        log = degradation_log[0]

        # 验证压缩执行
        assert log["compressed_count"] > 0, "Some entries should be compressed"
        assert log["usage_after_mb"] < log["usage_before_mb"], (
            f"Usage should decrease: {log['usage_before_mb']:.2f} -> {log['usage_after_mb']:.2f}"
        )

        # 验证降级后内存降低
        assert l1.estimated_size_mb() < current_usage, (
            "Memory usage should decrease after degradation"
        )

        # 验证高重要性条目未被压缩
        high_importance_entry = l1.retrieve("l1_entry_0099")  # i=99: importance = 0.2 + 9*0.08 = 0.92
        if high_importance_entry:
            assert "[compressed]" not in high_importance_entry.content, (
                "High importance entries should not be compressed"
            )

        # 验证低重要性条目被压缩
        low_importance_entry = l1.retrieve("l1_entry_0000")  # importance ~0.2
        if low_importance_entry:
            assert "[compressed]" in low_importance_entry.content, (
                "Low importance entries should be compressed"
            )

    def test_p3_19_4_l1_overflow_prune_only(self, temp_storage_p3_19):
        """
        P3.19.4: L1 超限时纯裁剪（无低重要性条目可压缩的场景）。

        验证:
            - 所有条目重要性均高时，压缩无效
            - 触发裁剪机制删除条目
            - 裁剪后条目数减少
        """
        budget = BudgetConfig(l1_budget_mb=1)
        manager = MemoryBudgetManager(budget)
        l1 = L1RawLayer(temp_storage_p3_19)

        prune_log = []

        def l1_handler(layer, usage, budget_mb):
            # 所有条目重要性都较高，压缩不会命中（threshold=0.1，所有条目 > 0.1）
            compressed = l1.compress_low_importance(threshold=0.1)
            # 强制裁剪 importance < 0.5 的条目
            pruned = l1.prune_by_importance(threshold=0.5)
            prune_log.append({
                "compressed": compressed,
                "pruned": pruned,
                "remaining": l1.count(),
            })

        manager.register_degradation_handler("l1", l1_handler)

        # 全部写入高重要性条目
        content_template = "高重要性条目 {} 不会被压缩" + "x" * 4000
        for i in range(300):
            entry = MemoryEntry(
                entry_id=f"high_imp_{i:04d}",
                content=content_template.format(i),
                memory_type=MemoryType.EPISODIC,
                layer=MemoryLayer.L1_RAW,
                timestamp=time.time(),
                importance=0.3 + (i % 5) * 0.1,  # 0.3 ~ 0.7，部分低于 0.5 会被裁剪
            )
            l1.store(entry)

        initial_count = l1.count()
        current_usage = l1.estimated_size_mb()
        result = manager.update_usage("l1", current_usage)

        assert result["is_over"], "L1 should be over budget"
        assert result["degradation_triggered"], "Degradation should be triggered"

        # 验证裁剪
        assert len(prune_log) > 0
        assert prune_log[0]["compressed"] == 0, "No entries should be compressed (all high importance)"
        assert prune_log[0]["pruned"] > 0, "Some entries should be pruned"
        assert prune_log[0]["remaining"] < initial_count, "Entry count should decrease"

    def test_p3_19_4_l1_budget_compression_then_prune(self, temp_storage_p3_19):
        """
        P3.19.4: L1 超限完整降级流程：压缩 → 裁剪。

        验证:
            - 完整的降级流程：先压缩低重要性，后裁剪
            - 两种策略互补
            - 降级后内存低于预算
        """
        budget = BudgetConfig(l1_budget_mb=1)
        manager = MemoryBudgetManager(budget)
        l1 = L1RawLayer(temp_storage_p3_19)

        final_state = {}

        def l1_full_handler(layer, usage, budget_mb):
            # 阶段1：压缩低重要性
            compressed = l1.compress_low_importance(threshold=0.3)
            after_compress_mb = l1.estimated_size_mb()

            # 阶段2：如果仍超限，裁剪
            pruned = 0
            if after_compress_mb > budget_mb:
                pruned = l1.prune_by_importance(threshold=0.2)
            after_prune_mb = l1.estimated_size_mb()

            final_state["compressed"] = compressed
            final_state["pruned"] = pruned
            final_state["after_compress_mb"] = after_compress_mb
            final_state["after_prune_mb"] = after_prune_mb
            final_state["remaining"] = l1.count()

        manager.register_degradation_handler("l1", l1_full_handler)

        # 混合重要性条目
        content = "混合重要性条目 {} 测试压缩与裁剪" + "x" * 4000
        for i in range(400):
            entry = MemoryEntry(
                entry_id=f"mixed_{i:04d}",
                content=content.format(i),
                memory_type=MemoryType.EPISODIC,
                layer=MemoryLayer.L1_RAW,
                timestamp=time.time(),
                importance=0.1 + (i % 10) * 0.09,  # 0.1 ~ 0.91
            )
            l1.store(entry)

        initial_count = l1.count()
        usage_before = l1.estimated_size_mb()
        result = manager.update_usage("l1", usage_before)

        assert result["is_over"], "L1 should be over budget"
        assert result["degradation_triggered"], "Degradation should be triggered"

        # 验证两种策略互补
        if final_state.get("compressed", 0) > 0:
            assert final_state["after_compress_mb"] < usage_before, "Compression should reduce usage"
        if final_state.get("pruned", 0) > 0:
            assert final_state["after_prune_mb"] <= final_state.get("after_compress_mb", usage_before), (
                "Pruning should further reduce usage"
            )

        assert final_state["remaining"] < initial_count, "Total entries should decrease"

        # 验证降级事件记录
        events = manager.get_degradation_events()
        assert len(events) == 1, "Should have one degradation event"
        assert events[0]["layer"] == "l1"
        assert events[0]["over_by_mb"] > 0, "Should be over budget by some amount"

    # ============================================================
    # P3.19: 综合预算验证
    # ============================================================

    def test_p3_19_multi_layer_budget_coordination(self):
        """
        P3.19: 多层预算协调验证。

        验证:
            - 多层同时超限时各自触发降级
            - 降级事件互不干扰
            - 各层预算独立管理
        """
        budget = BudgetConfig(
            l1_budget_mb=1,
            l2_budget_mb=1,
            l3_budget_mb=1,
            l4_budget_mb=1,
        )
        manager = MemoryBudgetManager(budget)

        degradation_counts = {"l1": 0, "l2": 0, "l3": 0, "l4": 0}

        def make_handler(layer_name):
            def handler(layer, usage, budget_mb):
                degradation_counts[layer_name] += 1
            return handler

        for layer in ["l1", "l2", "l3", "l4"]:
            manager.register_degradation_handler(layer, make_handler(layer))

        # 模拟各层超限
        manager.update_usage("l1", 1500.0)  # 超过 512MB
        manager.update_usage("l2", 2000.0)  # 超过 1024MB
        manager.update_usage("l3", 500.0)   # 超过 256MB
        manager.update_usage("l4", 300.0)   # 超过 128MB

        assert manager.is_any_over_budget(), "Should detect over budget"

        # 各层独立触发
        assert degradation_counts["l1"] == 1, "L1 degradation should fire once"
        assert degradation_counts["l2"] == 1, "L2 degradation should fire once"
        assert degradation_counts["l3"] == 1, "L3 degradation should fire once"
        assert degradation_counts["l4"] == 1, "L4 degradation should fire once"

        # 验证降级事件
        events = manager.get_degradation_events()
        assert len(events) == 4, "Should have 4 degradation events"

        layers_seen = {e["layer"] for e in events}
        assert layers_seen == {"l1", "l2", "l3", "l4"}, "All layers should have degradation events"

    def test_p3_19_budget_reset_and_recovery(self):
        """
        P3.19: 预算重置和恢复验证。

        验证:
            - 预算管理器可重置
            - 重置后使用量归零
            - 降级事件清空
            - 重置后可重新检测
        """
        budget = BudgetConfig(l1_budget_mb=512)
        manager = MemoryBudgetManager(budget)

        handler_called = [0]

        def l1_handler(layer, usage, budget_mb):
            handler_called[0] += 1

        manager.register_degradation_handler("l1", l1_handler)

        # 触发超限
        manager.update_usage("l1", 600.0)
        assert handler_called[0] == 1
        assert len(manager.get_degradation_events()) == 1

        # 重置
        manager.reset()
        assert manager.get_current_usage("l1") == 0.0
        assert len(manager.get_degradation_events()) == 0
        assert not manager.is_any_over_budget()

        # 再次超限
        manager.update_usage("l1", 700.0)
        assert handler_called[0] == 2, "Handler should be called again after reset"
        assert len(manager.get_degradation_events()) == 1

    def test_p3_19_budget_boundary_conditions(self):
        """
        P3.19: 预算边界条件验证。

        验证:
            - 恰好等于预算时不触发降级
            - 略超预算时触发降级
            - 大幅超预算时正确计算超限量
            - 零使用量时正常
        """
        budget = BudgetConfig(l1_budget_mb=512)
        manager = MemoryBudgetManager(budget)

        triggered = []

        def handler(layer, usage, budget_mb):
            triggered.append({"usage": usage, "budget": budget_mb})

        manager.register_degradation_handler("l1", handler)

        # 恰好等于预算：不触发
        result = manager.update_usage("l1", 512.0)
        assert not result["is_over"], "Exactly at budget should not trigger"
        assert result["over_by_mb"] == 0.0
        assert len(triggered) == 0

        # 略超预算：触发
        result = manager.update_usage("l1", 512.1)
        assert result["is_over"], "Slightly over budget should trigger"
        assert result["over_by_mb"] == pytest.approx(0.1, rel=0.1)
        assert len(triggered) == 1

        # 大幅超预算
        result = manager.update_usage("l1", 1024.0)
        assert result["is_over"]
        assert result["over_by_mb"] == pytest.approx(512.0, rel=0.1)
        assert len(triggered) == 2

        # 零使用量
        result = manager.update_usage("l1", 0.0)
        assert not result["is_over"]
        assert result["over_by_mb"] == 0.0


# ============================================================
# 运行测试
# ============================================================

if __name__ == "__main__":
    pytest.main([__file__, "-v", "--tb=short", "-m", "integration"])
