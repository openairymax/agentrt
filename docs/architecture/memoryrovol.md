Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# AgentOS 四层记忆系统 (MemoryRovol)

**版本**: 1.0.0
**最后更新**: 2026-04-06
**状态**: 生产就绪
**相关原则**: C-3 (记忆卷载), C-4 (遗忘机制)
**核心文档**: [MemoryRovol架构](../../agentos/manuals/architecture/memoryrovol.md)

---

## 🧠 设计理念

MemoryRovol 实现了**从原始数据到高级模式的完整记忆管理**，是 AgentOS 的"经验基底"。基于神经科学的记忆分层理论：

> **存用分离原则**: L1 永久保存原始数据（仅追加），L2-L4 仅存储索引和特征

### 四层架构概览

```
┌─────────────────────────────────────────────────────────────┐
│                    L4: 模式层 (Pattern Layer)                 │
│   持久同调 · HDBSCAN · 抽象模式挖掘                          │
│   用途: 长期策略、行为模式、知识抽象                           │
│   延迟: 秒级 | 吞吐: 10万条/分钟                              │
├─────────────────────────────────────────────────────────────┤
│                    L3: 结构层 (Structure Layer)               │
│   绑定算子 · 图神经网络 · 关系编码                            │
│   用途: 实体关系、因果链、知识图谱                            │
│   延迟: 10-100ms | 吞吐: 100条/秒                             │
├─────────────────────────────────────────────────────────────┤
│                    L2: 特征层 (Feature Layer)                 │
│   FAISS · Embedding模型 · 向量相似度                         │
│   用途: 语义检索、快速匹配、相似性搜索                        │
│   延迟: <10ms | 吞吐: 千级QPS                                 │
├─────────────────────────────────────────────────────────────┤
│                    L1: 原始卷 (Raw Volume)                    │
│   文件系统 · SQLite索引 · 仅追加写入                         │
│   用途: 原始事件存储、完整审计追踪、不可变归档                │
│   延迟: <1ms | 吞吐: 10000+条/秒                              │
└─────────────────────────────────────────────────────────────┘
```

---

## 📊 各层详细设计

### L1: 原始卷 (Raw Volume)

**职责**: 原始事件的永久、不可变存储

#### 数据结构

```python
@dataclass
class RawRecord:
    """L1 原始记录"""
    id: str                              # UUID v4
    timestamp: datetime                  # 事件时间戳（微秒精度）
    source: str                          # 数据来源（agent/session/system）
    event_type: str                      # 事件类型（observation/action/reflection）
    data: Dict[str, Any]                 # 原始数据（JSON序列化）
    metadata: Dict[str, Any]             # 元数据（标签、版本等）
    checksum: str                        # SHA256 校验和（完整性验证）
    size_bytes: int                      # 记录大小
```

#### 存储实现

```python
class L1RawVolume:
    def __init__(self, storage_path: str):
        self.storage_path = Path(storage_path)
        self.db = sqlite3.connect(storage_path / "l1_index.db")
        self._init_schema()

    def _init_schema(self):
        """初始化 SQLite 索引表"""
        self.db.execute("""
            CREATE TABLE IF NOT EXISTS records (
                id TEXT PRIMARY KEY,
                timestamp REAL NOT NULL,
                source TEXT NOT NULL,
                event_type TEXT NOT NULL,
                file_path TEXT NOT NULL,
                metadata_json TEXT,
                checksum TEXT NOT NULL,
                size_bytes INTEGER NOT NULL
            )
        """)
        # 创建复合索引加速查询
        self.db.execute("""
            CREATE INDEX IF NOT EXISTS idx_timestamp_source
            ON records(timestamp, source)
        """)
        self.db.execute("""
            CREATE INDEX IF NOT EXISTS idx_event_type
            ON records(event_type)
        """)

    def append(self, record: RawRecord) -> str:
        """追加写入原始记录（仅追加，不修改）"""
        # 序列化数据
        data_json = json.dumps(record.data, ensure_ascii=False)
        metadata_json = json.dumps(record.metadata, ensure_ascii=False)

        # 写入文件系统（按日期分片）
        date_path = self.storage_path / record.timestamp.strftime("%Y/%m/%d")
        date_path.mkdir(parents=True, exist_ok=True)

        file_path = date_path / f"{record.id}.json"
        with open(file_path, 'w') as f:
            f.write(data_json)

        # 写入 SQLite 索引
        self.db.execute(
            "INSERT INTO records VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
            (
                record.id,
                record.timestamp.timestamp(),
                record.source,
                record.event_type,
                str(file_path),
                metadata_json,
                record.checksum,
                record.size_bytes
            )
        )
        self.db.commit()

        return record.id

    def query(self, start_time: datetime, end_time: datetime,
              source: Optional[str] = None,
              event_type: Optional[str] = None,
              limit: int = 1000) -> List[RawRecord]:
        """查询原始记录"""
        query = "SELECT * FROM records WHERE timestamp BETWEEN ? AND ?"
        params = [start_time.timestamp(), end_time.timestamp()]

        if source:
            query += " AND source = ?"
            params.append(source)

        if event_type:
            query += " AND event_type = ?"
            params.append(event_type)

        query += " ORDER BY timestamp ASC LIMIT ?"
        params.append(limit)

        cursor = self.db.execute(query, params)
        rows = cursor.fetchall()

        results = []
        for row in rows:
            # 从文件读取完整数据
            with open(row[4], 'r') as f:
                data = json.load(f)
            results.append(RawRecord(
                id=row[0],
                timestamp=datetime.fromtimestamp(row[1]),
                source=row[2],
                event_type=row[3],
                data=data,
                metadata=json.loads(row[5]) if row[5] else {},
                checksum=row[6],
                size_bytes=row[7]
            ))

        return results
```

#### 性能指标

| 指标 | 数值 | 测试条件 |
|------|------|---------|
| **写入吞吐量** | > 10,000 条/秒 | 批量写入 |
| **单次写入延迟** | < 1ms | 包含索引更新 |
| **查询延迟** | < 10ms | 100万条记录 |
| **存储效率** | 压缩后 ~200 bytes/条 | 典型记录 |
| **数据完整性** | SHA256 校验 | 每条记录 |

---

### L2: 特征层 (Feature Layer)

**职责**: 向量嵌入检索与语义相似度匹配

#### 向量嵌入生成

```python
class L2FeatureLayer:
    def __init__(self, embedding_model: str, index_type: str = "faiss_ivf"):
        self.embedding_model = self._load_model(embedding_model)
        self.index = self._create_index(index_type)

    def _load_model(self, model_name: str):
        """加载 Embedding 模型"""
        # 支持多种模型：sentence-transformers, OpenAI embeddings 等
        if model_name.startswith("sentence-transformers/"):
            from sentence_transformers import SentenceTransformer
            return SentenceTransformer(model_name)
        elif model_name == "openai":
            return OpenAIEmbeddings()
        else:
            raise ValueError(f"Unsupported embedding model: {model_name}")

    def encode(self, text: str) -> np.ndarray:
        """文本向量化"""
        embedding = self.embedding_model.encode(text)
        # L2 归一化（用于余弦相似度）
        norm = np.linalg.norm(embedding)
        if norm > 0:
            embedding = embedding / norm
        return embedding

    def add(self, record_id: str, embedding: np.ndarray, metadata: dict):
        """添加向量到索引"""
        self.index.add_with_ids(embedding.reshape(1, -1), record_id)
        # 存储元数据映射
        self.metadata_store[record_id] = metadata
```

#### FAISS 索引配置

```python
def create_faiss_index(dimension: int, index_type: str, nlist: int = 1024):
    """创建 FAISS 索引"""

    if index_type == "faiss_ivf":
        # IVF (Inverted File) - 平衡速度与精度
        quantizer = faiss.IndexFlatIP(dimension)  # 内积量化器
        index = faiss.IndexIVFFlat(quantizer, dimension, nlist, faiss.METRIC_INNER_PRODUCT)

    elif index_type == "faiss_hnsw":
        # HNSW (Hierarchical Navigable Small World) - 高召回率
        index = faiss.IndexHNSWFlat(dimension, M=32)
        index.hnsw.efConstruction = 200
        index.hnsw.efSearch = 50

    elif index_type == "faiss_ivfpq":
        # IVF+PQ (Product Quantization) - 内存优化
        quantizer = fa.IndexFlatIP(dimension)
        nsubquantizers = min(dimension // 2, 32)
        index = faiss.IVFPQ(quantizer, dimension, nlist, nsubquantizers, 8)

    else:
        raise ValueError(f"Unsupported index type: {index_type}")

    return index
```

#### 相似度搜索

```python
def search(self, query_embedding: np.ndarray, top_k: int = 10,
           filters: Optional[Dict] = None) -> List[SearchResult]:
    """向量相似度搜索"""

    # Step 1: FAISS 近似搜索（返回 top_k * 3 候选）
    candidates_count = top_k * 3
    distances, indices = self.index.search(query_embedding.reshape(1, -1), candidates_count)

    # Step 2: 应用过滤器（如果有）
    filtered_results = []
    for i, (dist, idx) in enumerate(zip(distances[0], indices[0])):
        if idx == -1:  # 无效索引
            continue

        record_id = self.id_map[idx]
        metadata = self.metadata_store.get(record_id, {})

        # 应用过滤条件
        if filters and not self._match_filters(metadata, filters):
            continue

        filtered_results.append(SearchResult(
            id=record_id,
            score=float(dist),  # 余弦相似度（因为已归一化）
            metadata=metadata
        ))

        if len(filtered_results) >= top_k:
            break

    # Step 3: 按分数排序
    filtered_results.sort(key=lambda x: x.score, reverse=True)

    return filtered_results[:top_k]
```

#### 性能指标

| 指标 | 数值 | 测试条件 |
|------|------|---------|
| **查询延迟** | < 10ms | k=10, 百万级向量 |
| **召回率** | > 95% | FAISS IVF |
| **索引构建速度** | ~10k vectors/s | CPU 单线程 |
| **内存占用** | ~1GB | 百万维768向量 |
| **吞吐量** | > 1000 QPS | 并发查询 |

---

### L3: 结构层 (Structure Layer)

**职责**: 关系绑定编码与知识图谱构建

#### 关系类型定义

```python
@dataclass
class Relation:
    """实体关系定义"""
    source_entity: str          # 源实体 ID
    target_entity: str          # 目标实体 ID
    relation_type: RelationType # 关系类型
    weight: float               # 关系强度 [0, 1]
    confidence: float           # 置信度 [0, 1]
    timestamp: datetime         # 关系建立时间
    evidence: List[str]         # 支持证据（L1 记录 ID 列表）

class RelationType(Enum):
    """预定义关系类型"""
    CAUSALITY = "causality"           # 因果关系
    TEMPORAL = "temporal"             # 时序关系
    SPATIAL = "spatial"               # 空间关系
    SEMANTIC = "semantic"             # 语义关联
    HIERARCHICAL = "hierarchical"     # 层次关系
    FUNCTIONAL = "functional"         # 功能依赖
    CORRELATION = "correlation"       # 统计相关
```

#### 绑定算子 (Binding Operators)

```python
class BindingOperators:
    """关系绑定操作符集合"""

    def temporal_bind(self, entity_a: str, entity_b: str,
                     time_window_sec: float = 3600) -> Optional[Relation]:
        """
        时间绑定：如果两个实体在时间窗口内频繁共现，建立时序关系
        """
        cooccurrence_count = self._count_cooccurrences(
            entity_a, entity_b, time_window_sec
        )

        if cooccurrence_count >= self.threshold:
            confidence = min(cooccurrence_count / self.max_expected, 1.0)
            return Relation(
                source_entity=entity_a,
                target_entity=entity_b,
                relation_type=RelationType.TEMPORAL,
                weight=confidence,
                confidence=confidence,
                timestamp=datetime.now(),
                evidence=self._get_evidence(entity_a, entity_b)
            )
        return None

    def causal_bind(self, cause_entity: str, effect_entity: str,
                   min_lag_sec: float = 1.0,
                   max_lag_sec: float = 3600.0) -> Optional[Relation]:
        """
        因果绑定：基于格兰杰因果关系检验
        """
        # 提取两个实体的事件序列
        cause_series = self._extract_time_series(cause_entity)
        effect_series = self._extract_time_series(effect_entity)

        # 格兰杰因果关系检验
        causality_score = grangercausalitytests(
            [cause_series, effect_series],
            maxlag=int(max_lag_sec / self.sampling_interval)
        )[1][0]['ssr_ftest'][1]  # p-value

        if causality_score < 0.05:  # 显著性水平 5%
            return Relation(
                source_entity=cause_entity,
                target_entity=effect_entity,
                relation_type=RelationType.CAUSALITY,
                weight=1.0 - causality_score,
                confidence=1.0 - causality_score,
                timestamp=datetime.now(),
                evidence=self._get_evidence(cause_entity, effect_entity)
            )
        return None

    def semantic_bind(self, entity_a: str, entity_b: str,
                     threshold: float = 0.8) -> Optional[Relation]:
        """
        语义绑定：基于向量相似度和上下文共现
        """
        # 获取实体的上下文向量
        context_a = self._get_context_vector(entity_a)
        context_b = self._get_context_vector(entity_b)

        # 计算语义相似度
        similarity = cosine_similarity(context_a, context_b)

        if similarity >= threshold:
            return Relation(
                source_entity=entity_a,
                target_entity=entity_b,
                relation_type=RelationType.SEMANTIC,
                weight=similarity,
                confidence=similarity,
                timestamp=datetime.now(),
                evidence=[]
            )
        return None
```

#### 图神经网络推理

```python
class GraphNeuralNetwork:
    """基于 GNN 的关系推理引擎"""

    def __init__(self, hidden_dim: int = 128, num_layers: int = 2):
        self.gnn = self._build_gnn(hidden_dim, num_layers)

    def infer_relations(self, entity: str, depth: int = 2,
                       relation_types: List[RelationType] = None) -> Dict[str, float]:
        """
        推理实体的潜在关系（多跳推理）
        """
        # 提取子图（BFS 遍历到指定深度）
        subgraph = self._extract_subgraph(entity, depth, relation_types)

        # 节点特征初始化
        node_features = self._initialize_node_features(subgraph.nodes)

        # GNN 前向传播
        hidden_states = self.gnn(node_features, subgraph.edge_index)

        # 解码潜在关系
        potential_relations = {}
        for target_node in subgraph.nodes:
            if target_node != entity:
                score = self._decode_relation_score(
                    hidden_states[entity],
                    hidden_states[target_node]
                )
                if score > 0.5:  # 阈值过滤
                    potential_relations[target_node] = score

        return potential_relations
```

#### 性能指标

| 指标 | 数值 | 测试条件 |
|------|------|---------|
| **关系绑定延迟** | 10-100ms | 单对实体 |
| **图谱查询延迟** | < 50ms | 2跳查询 |
| **GNN 推理延迟** | < 500ms | 子图100节点 |
| **关系存储** | ~100 bytes/条 | 典型关系 |
| **吞吐量** | ~100 条/秒 | 批量处理 |

---

### L4: 模式层 (Pattern Layer)

**职责**: 高级模式挖掘与知识抽象

#### 持久同调 (Persistent Homology)

```python
class PersistentHomologyMiner:
    """基于持久同调的拓扑模式挖掘"""

    def mine_patterns(self, data_points: np.ndarray,
                     max_dimension: int = 2) -> List[Pattern]:
        """
        挖掘数据的拓扑特征（连通分量、环、空洞）
        """
        # 构建 Vietoris-Rips 复形
        vr_complex = RipsComplex(data_points, max_edge_length=self.max_radius)

        # 计算持久同调
        persistence = vr_complex.compute_persistence(
            homology_dimensions=range(max_dimension + 1)
        )

        # 提取显著模式（长生命周期特征）
        patterns = []
        for dim, (birth, death) in enumerate(persistence):
            persistence_length = death - birth

            if persistence_length > self.min_persistence[dim]:
                pattern = Pattern(
                    id=generate_uuid(),
                    type=f"topological_dim{dim}",
                    birth=birth,
                    death=death,
                    persistence=persistence_length,
                    significance=self._calculate_significance(persistence_length, dim),
                    members=self._identify_pattern_members(data_points, birth, death),
                    timestamp=datetime.now()
                )
                patterns.append(pattern)

        return patterns
```

#### HDBSCAN 聚类

```python
class HDBSCANClusterer:
    """基于 HDBSCAN 的密度聚类"""

    def cluster(self, embeddings: np.ndarray,
               min_cluster_size: int = 5,
               min_samples: int = 3) -> ClusterResult:
        """
        层次密度聚类，自动确定聚类数量
        """
        import hdbscan

        clusterer = hdbscan.HDBSCAN(
            min_cluster_size=min_cluster_size,
            min_samples=min_samples,
            metric='euclidean',
            cluster_selection_method='eom'  # Excess of Mass
        )

        labels = clusterer.fit_predict(embeddings)
        probabilities = clusterer.probabilities_

        # 提取聚类结果
        clusters = {}
        for label in set(labels):
            if label == -1:  # 噪声点
                continue

            mask = labels == label
            cluster_members = np.where(mask)[0]
            cluster_centroid = embeddings[mask].mean(axis=0)

            clusters[label] = Cluster(
                id=str(label),
                centroid=cluster_centroid,
                members=cluster_members,
                size=len(cluster_members),
                stability=clusterer.cluster_persistence_[label],
                probability=probabilities[mask].mean()
            )

        return ClusterResult(
            clusters=clusters,
            noise_points=np.where(labels == -1)[0],
            labels=labels,
            probabilities=probabilities
        )
```

#### 模式抽象示例

```python
class PatternAbstraction:
    """从具体实例中抽象出通用模式"""

    def abstract_behavior_pattern(self, episodes: List[Episode]) -> BehaviorPattern:
        """
        从多个行为片段中抽象出通用行为模式
        """
        # Step 1: 序列对齐（DTW 动态时间规整）
        aligned_sequences = self._align_sequences(episodes)

        # Step 2: 提取共同子序列（最长公共子序列变体）
        common_subsequence = self._lcs_variant(aligned_sequences)

        # Step 3: 参数化抽象（识别可变部分）
        parameterized_pattern = self._parameterize(common_subsequence)

        # Step 4: 计算模式统计特性
        statistics = self._compute_statistics(episodes, parameterized_pattern)

        return BehaviorPattern(
            id=generate_uuid(),
            name=self._generate_pattern_name(parameterized_pattern),
            template=parameterized_pattern,
            frequency=len(episodes),
            success_rate=sum(1 for e in episodes if e.success) / len(episodes),
            avg_duration=np.mean([e.duration for e in episodes]),
            typical_context=self._extract_typical_context(episodes),
            variations=self._identify_variations(episodes),
            statistics=statistics,
            created_at=datetime.now()
        )
```

#### 性能指标

| 指标 | 数值 | 测试条件 |
|------|------|---------|
| **持久同调计算** | ~10s | 10万数据点，dim=2 |
| **HDBSCAN 聚类** | ~5s | 10万向量 |
| **模式抽象** | ~30s | 100个episodes |
| **存储效率** | ~1KB/模式 | 典型模式 |
| **吞吐量** | 10万条/分钟 | 批量处理 |

---

## 🔗 层间交互协议

### 数据流协议

```
L1 (写入触发)
    ↓ on_append(record)
L2 (异步提取特征)
    ↓ on_feature_ready(record_id, embedding)
L3 (异步绑定关系)
    ↓ on_relation_bound(relations)
L4 (定期批量挖掘)
    ↓ on_patterns_mined(patterns)
反馈给认知层
```

### 一致性保证

| 保证级别 | 时间窗口 | 机制 |
|----------|----------|------|
| **L1 → L2** | < 1秒 | 事件驱动 + 异步队列 |
| **L2 → L3** | < 10秒 | 批量处理 + 定时触发 |
| **L3 → L4** | < 1小时 | 定时任务 + 增量更新 |
| **全局一致性** | < 5秒 | 版本向量 + 最终一致性 |

---

## ⚙️ 配置示例

```yaml
# memoryrovol.yaml
memoryrovol:

  l1_raw_volume:
    storage_path: /app/data/memory/l1
    shard_strategy: daily  # hourly | daily | weekly
    compression: gzip
    retention_days: 3650  # 10年

  l2_feature_layer:
    embedding_model: "sentence-transformers/all-MiniLM-L6-v2"
    dimension: 384
    index_type: faiss_ivf
    faiss_nlist: 1024
    faiss_nprobe: 32
    cache_size_mb: 1024
    async_workers: 4

  l3_structure_layer:
    binding_operators:
      temporal:
        enabled: true
        time_window_sec: 3600
        threshold: 5
      causal:
        enabled: true
        min_lag_sec: 1.0
        max_lag_sec: 3600.0
      semantic:
        enabled: true
        threshold: 0.8
    gnn:
      hidden_dim: 128
      num_layers: 2
    graph_max_nodes: 100000

  l4_pattern_layer:
    persistent_homology:
      enabled: true
      max_dimension: 2
      min_persistence:
        0: 0.1  # 连通分量
        1: 0.05  # 环
        2: 0.01  # 空洞
    hdbscan:
      min_cluster_size: 5
      min_samples: 3
    mining_interval_min: 60
    batch_size: 10000
```

---

## 📈 监控指标

### Prometheus 指标

```yaml
# memoryrovol metrics
- name: agentos_memory_l1_records_total
  type: Gauge
  help: "L1 原始卷总记录数"

- name: agentos_memory_l1_write_latency_seconds
  type: Histogram
  help: "L1 写入延迟分布"

- name: agentos_memory_l2_query_duration_seconds
  type: Histogram
  help: "L2 查询延迟分布"

- name: agentos_memory_l2_index_size
  type: Gauge
  help: "L2 FAISS 索引向量数"

- name: agentos_memory_l3_relations_total
  type: Gauge
  help: "L3 关系总数"

- name: agentos_memory_l4_patterns_total
  type: Gauge
  help: "L4 已挖掘模式数"

- name: agentos_memory_l4_mining_duration_seconds
  type: Histogram
  help: "L4 模式挖掘耗时"
```

---

## 🔧 运维指南

### 数据备份

```bash
# 备份 L1 原始数据
tar czf l1_backup_$(date +%Y%m%d).tar.gz /app/data/memory/l1/

# 备份 L2 FAISS 索引
cp /app/data/memory/l2/faiss_index.index /backup/

# 备份 L3 图数据库
pg_dump -U agentos -d agentos_graph > l3_backup.sql

# 备份 L4 模式库
mongodump --db agentos_patterns --out /backup/l4/
```

### 性能调优

```bash
# L2: 调整 FAISS 参数提高查询速度
# 增加 nprobe 牺牲一点速度换取更高召回率
export AGENTOS_FAISS_NPROBE=64

# L3: 增加 GNN 批处理大小
export AGENTOS_GNN_BATCH_SIZE=256

# L4: 减少挖掘频率降低资源消耗
export AGENTOS_L4_MINING_INTERVAL_MIN=120
```

### 故障排查

**问题 1: L2 查询延迟突然升高**

```bash
# 检查 FAISS 索引状态
python -c "
import faiss
index = fa.read_index('/app/data/memory/l2/faiss_index.index')
print(f'Index size: {index.ntotal}')
print(f'Is trained: {index.is_trained}')
"

# 解决方案：重建索引或增加 nprobe
```

**问题 2: L3 关系绑定积压**

```bash
# 检查绑定队列长度
curl http://localhost:9090/metrics | grep agentos_memory_l3_binding_queue

# 解决方案：增加 worker 数量
export AGENTOS_L3_WORKERS=8
```

---

## 📚 相关文档

- **[记忆理论](../../agentos/manuals/philosophy/Memory_Theory.md)** — 记忆分层的神经科学基础
- **[三层认知运行时](architecture/coreloopthree.md)** — 记忆层在认知循环中的作用
- **[系统调用 API - Memory](../../agentos/manuals/api/syscalls/memory.md)** — Memory Syscall 接口
- **[架构设计原则](../../agentos/manuals/ARCHITECTURAL_PRINCIPLES.md)** — C-3/C-4 原则详解

---

## ⚠️ 注意事项

1. **L1 不可变性**: 原始记录一旦写入永不修改或删除（仅追加）
2. **L2 索引重建**: 当向量数量增长 10 倍时应考虑重建 FAISS 索引
3. **L3 图规模控制**: 单个实体关联数应限制在 < 1000 以防止爆炸
4. **L4 挖掘成本**: 持久同调和 HDBSCAN 是计算密集型任务，应在低峰期运行
5. **跨层数据一致性**: 采用最终一致性模型，通常 5 秒内完成同步

---

**© 2026 SPHARX Ltd. All Rights Reserved.**

*"From data intelligence emerges."*
