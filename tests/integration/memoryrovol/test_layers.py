# AgentOS 记忆层集成测试
# Version: 1.0.0.6
# Last updated: 2026-03-23

"""
AgentOS 记忆层集成测试模块。

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
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', 'tools', 'python')))


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
        
        query = MemoryQuery(type_filter=MemoryType.EPISODIC, limit=100)
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
            embedding = [float(i) / 10] * 64
            l2_layer.add_embedding(f"entry_{i}", embedding)
        
        query_embedding = [0.5] * 64
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
        
        query = MemoryQuery(limit=10)
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
        
        query = MemoryQuery(min_importance=0.5, limit=10)
        high_importance = l1.query(query)
        
        assert len(high_importance) > 0
        for entry in high_importance:
            assert entry.importance >= 0.5


# ============================================================
# 运行测试
# ============================================================

if __name__ == "__main__":
    pytest.main([__file__, "-v", "--tb=short", "-m", "integration"])
