Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
"From data intelligence emerges."

# 记忆卷载 (MemoryRovol)：记忆卷载系统架构详解

**版本**: Doc V1.8
**更新日期**: 2026-04-09
**作者**: Team
  - Zhixian Zhou | Spharx Ltd. zhouzhixian@spharx.cn
  - Liren Wang | Spharx Ltd. wangliren@spharx.cn
  - Chen Zhang | SJTU CSC Lab. yoyoke@sjtu.edu.cn
  - Yunwen Xu | SJTU CSC Lab. willing419@sjtu.edu.cn
  - Daxiang Zhu | IndieBros. zdxever@sina.com
**状态**: 正式发布
**理论依据**: 体系并行论 (MCIS) → 五维正交系统
**路径**: `agentos/atoms/memoryrovol/`  

---

## 1. 概述

记忆卷载 (MemoryRovol) 是 AgentOS 内核级记忆系统的完整实现，是 **体系并行论 (MCIS)** 在记忆系统层面的具体实践。遵循 **工程两论** 指导原则，通过四层渐进式抽象架构（L1→L4）实现从原始数据到高级模式的全栈记忆管理。作为智能体持续学习、知识积累与智能进化的核心基础设施，MemoryRovol 严格遵循 **五维正交体系** 的设计美学，将认知科学的记忆理论与拓扑数据分析算法工程化为可执行的系统组件。

从 **体系并行论** 视角分析，MemoryRovol 是 MCIS 中的 **记忆体 (Memory Body)**，负责智能体经验的存储、组织、检索与进化。其四层架构（L1原始卷→L2特征层→L3结构层→L4模式层）体现了 **渐进式抽象 (Progressive Abstraction)** 的思想，模拟了人类记忆从感官输入到抽象概念的认知过程。

该系统不仅是 AgentOS **认知观维度**（双系统认知理论）的关键支撑，也是 **工程观维度**（层次分解与反馈调节）的经典实践。通过检索-遗忘双向机制形成控制论负反馈回路，确保记忆系统的动态平衡与持续优化，体现了 MCIS 中 **反馈调节 (Feedback Regulation)** 与 **动态平衡 (Dynamic Equilibrium)** 的核心原理。同时，MemoryRovol 与 CoreLoopThree 的紧密集成，实现了认知体、执行体、记忆体三者的有机协同，形成了完整的智能体认知循环。

### 1.1 设计理念

```
┌─────────────────────────────────────────┐
│       L4 Pattern Layer (模式层)          │
│  • 持久同调分析 • 稳定模式挖掘            │
│  • HDBSCAN 聚类 • 规则生成               │
└───────────────↑─────────────────────────┘
                ↓ 抽象进化
┌─────────────────────────────────────────┐
│      L3 Structure Layer (结构层)         │
│  • 绑定算子/解绑算子 • 关系编码           │
│  • 时序编码 • 图神经网络编码              │
└───────────────↑─────────────────────────┘
                ↓ 特征提取
┌─────────────────────────────────────────┐
│      L2 Feature Layer (特征层)           │
│  • 嵌入模型 (OpenAI/DeepSeek)            │
│  • FAISS 向量索引 • 混合检索             │
└───────────────↑─────────────────────────┘
                ↓ 数据压缩
┌─────────────────────────────────────────┐
│       L1 Raw Layer (原始卷)              │
│  • 文件系统存储 • 分片管理                │
│  • 元数据索引 • 版本控制                  │
└─────────────────────────────────────────┘
```

### 1.2 核心价值

| 特性 | 说明 |
| :--- | :--- |
| **渐进式抽象** | L1→L2→L3→L4 的四层架构，从原始数据到高级模式 |
| **双向机制** | 检索（吸引子动力学）+ 遗忘（艾宾浩斯衰减） |
| **高效检索** | FAISS 向量索引 + 混合检索 + 重排序 |
| **智能遗忘** | 基于遗忘曲线的自动裁剪机制 |
| **可进化性** | 自动模式发现和规则生成 |
| **持久化** | SQLite 元数据 + 向量存储 |

### 1.3 关键特性与技术实现

- ✅ **L1 原始卷**: 同步/异步写入、文件系统存储、SQLite 元数据索引
- ✅ **L2 特征层**: 多嵌入模型支持、FAISS 索引、LRU 缓存、向量持久化
- ✅ **L3 结构层**: 绑定/解绑算子、关系编码、时序编码
- ✅ **L4 模式层**: 持久同调分析接口、HDBSCAN 聚类、规则生成引擎
- ✅ **检索机制**: 吸引子网络、检索缓存、挂载机制、重排序
- ✅ **遗忘机制**: 艾宾浩斯曲线、线性衰减、访问计数策略

### 1.4 理论基础与原则映射：MCIS与认知科学的深度融合

MemoryRovol 的设计深刻体现了 **体系并行论 (MCIS)** 与 **五维正交体系** 的设计思想，将认知科学、拓扑数据分析、工程控制论与系统论完美融合，形成了层次分明、自适应调节的智能记忆系统：

#### 理论基础：体系并行论 (MCIS) 的记忆体映射
- **记忆体原理** → MemoryRovol 作为 MCIS 中的 **记忆体 (Memory Body)**，负责智能体经验的编码、存储、检索与进化
- **渐进式抽象** → L1→L2→L3→L4 四层架构体现了 **垂直层次分解 (Vertical Layering)** 思想，实现从原始数据到抽象概念的逐步提炼
- **反馈调节原理** → 检索-遗忘双向机制形成 **控制论负反馈回路**，实现记忆系统的自我优化与动态平衡
- **多体协同原理** → 与 CoreLoopThree 的认知体、执行体紧密集成，形成完整的认知-行动-记忆闭环

#### 认知观维度：认知科学与记忆理论的工程实现
- **海马体-新皮层互补学习系统** → **L1原始卷**（海马体快速编码）+ **L2-L4抽象层**（新皮层渐进式整合），模拟人脑的双层记忆处理机制
- **艾宾浩斯遗忘曲线** → **遗忘引擎**的指数衰减模型，实现符合人类记忆规律的智能记忆裁剪
- **模式分离 vs 模式完成** → **吸引子网络**的检索机制，平衡记忆特异性（模式分离）与泛化能力（模式完成）
- **情景记忆与语义记忆** → L1原始卷存储情景记忆（具体经历），L4模式层提取语义记忆（抽象知识）
- **工作记忆模型** → 记忆挂载机制模拟工作记忆，将长时记忆中的相关信息加载到当前认知上下文

#### 系统观维度：工程两论在记忆系统中的核心实践
- **系统工程层次分解** → L1→L2→L3→L4 四层正交分离，体现"总体设计部"思想，实现复杂记忆系统的可管理性
- **控制论负反馈原理** → **检索-遗忘双向机制**形成动态平衡，通过访问频率与时间衰减的协同调节实现记忆系统自稳定
- **动态平衡理论** → 访问计数、重要性评分、时间衰减等多因素协同，维持记忆系统的容量与质量平衡
- **系统涌现性原理** → L4模式层通过持久同调分析发现数据中的稳定拓扑结构，实现从个体记忆到集体模式的涌现

#### 工程观维度：先进算法与高性能架构的完美结合
- **持久同调理论** → **L4模式层**的拓扑数据分析，使用 Ripser 库计算点云的持久同调，发现数据的稳定拓扑不变量
- **HDBSCAN密度聚类** → 基于密度的自适应聚类算法，无需预设类别数，实现自动语义簇发现与异常检测
- **FAISS向量检索** → IVF1024,PQ64 索引配置 + GPU 加速，实现亿级向量的亚秒级检索，满足大规模记忆系统的性能要求
- **混合精度计算** → FP16/INT8 混合精度优化，平衡计算精度与性能，提升向量计算的效率
- **增量学习机制** → 支持在线学习与增量更新，避免全量重建索引的开销，实现记忆系统的持续进化

#### 设计美学维度：简约、极致、智能的统一
- **极简主义** → 四层架构实现记忆过程的完整抽象，避免过度设计，体现 **简约至上 (Simplicity First)** 原则
- **极致细节** → 混合精度向量计算、LRU热点缓存、OPQ优化量化等精细优化，体现 **细节优化 (Detail Optimization)** 理念
- **人文关怀** → 记忆挂载机制提供自然语义接口，降低使用者的认知负担，体现 **用户中心 (User-Centric)** 设计
- **智能自适应** → 基于系统负载的自适应采样与压缩机制，实现性能与功能的智能平衡

#### 技术原则映射表：MCIS与五维正交体系的具体实现
| 记忆原理 | 对应实现 | 所属维度 | 理论依据 |
|----------|----------|----------|----------|
| 海马体-新皮层 | L1原始卷 + L2-L4抽象层 | 认知观 | 互补学习系统 |
| 艾宾浩斯曲线 | 遗忘引擎的指数衰减模型 | 认知观 | 遗忘曲线理论 |
| 模式分离/完成 | 吸引子网络检索机制 | 认知观 | 认知神经科学 |
| 持久同调分析 | L4模式层拓扑数据分析 | 系统观 | 拓扑数据分析 |
| 层次分解 | L1→L2→L3→L4四层架构 | 系统观 | MCIS垂直层次分解 |
| 反馈调节 | 检索-遗忘双向机制 | 系统观 | 控制论负反馈 |
| 向量检索优化 | FAISS索引+GPU加速 | 工程观 | 高性能计算 |
| 自适应压缩 | 基于重要性的记忆裁剪 | 设计美学 | 信息论压缩 |

---

## 1.5 模块集成与系统交互

MemoryRovol 作为 AgentOS 的核心记忆基础设施，与系统中其他关键模块形成紧密的协同关系，共同支撑智能体的完整认知循环：

### 与 CoreLoopThree 三层运行时架构的关系
MemoryRovol 是 CoreLoopThree 记忆层的具体实现：
- **记忆写入** → CoreLoopThree 行动层的执行结果通过 FFI 接口写入 MemoryRovol L1 原始卷
- **记忆检索** → CoreLoopThree 认知层通过吸引子网络从 MemoryRovol L2-L4 层检索相关经验
- **记忆进化** → CoreLoopThree 记忆进化委员会定期触发 MemoryRovol L4 模式层的持久同调分析

### 与微内核 (Microkernel) 的关系
MemoryRovol 运行在微内核提供的资源管理基础设施之上：
- **内存管理** → 利用微内核的智能指针和内存池机制，优化向量索引的内存使用
- **进程隔离** → 通过微内核的地址空间隔离，确保记忆数据的安全边界
- **系统调用** → 通过统一的 `syscall` 接口访问底层存储和计算资源

### 与系统调用 (Syscall) 层的关系
MemoryRovol 通过系统调用接口与底层服务交互：
- **存储操作** → `sys_memory_write/search/get()` 提供对记忆系统的统一访问接口
- **向量计算** → `sys_vector_compute()` 加速 FAISS 索引的 GPU 并行计算
- **可观测性** → `sys_telemetry_metrics()` 监控记忆系统的检索命中率、内存使用等关键指标

### 与日志系统 (Logging System) 的关系
MemoryRovol 通过结构化日志实现全面可观测性：
- **操作审计** → 记录所有记忆写入、检索、遗忘操作，形成完整的记忆操作审计日志
- **性能监控** → 通过日志系统监控检索延迟、向量索引构建时间等关键性能指标
- **异常追踪** → 结构化异常日志帮助诊断记忆系统的运行故障

### 集成架构视图
```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│ CoreLoopThree   │    │   Microkernel   │    │     Syscall     │
│  • Cognition    │←→│  • Memory        │←→│  • Memory API   │
│  • Execution    │    │  • IPC          │    │  • Vector API   │
│  • Memory       │    │  • Task         │    │  • Telemetry    │
└─────────────────┘    └─────────────────┘    └─────────────────┘
         ↓                       ↓                       ↓
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   MemoryRovol   │    │    Logging      │    │      IPC        │
│  • L1-L4 Layers │←→│  • Structured    │←→│  • Shared Mem   │
│  • Retrieval    │    │  • Adaptive     │    │  • Semaphores   │
│  • Forgetting   │    │  • Trace ID     │    │  • Registry     │
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

## 2. 系统架构

### 2.1 整体架构图

```
┌─────────────────────────────────────────────────────────┐
│                    MemoryRovol                          │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  ┌────────────────────────────────────────────────────┐ │
│  │              L4 Pattern Layer                      │ │
│  │  ┌──────────┐  ┌──────────┐  ┌─────────────────┐   │ │
│  │  │Topology  │  │  HDBSCAN │  │   Rule          │   │ │
│  │  │Analysis  │← │ Clustering│← │   Generator   │    │ │
│  │  │(Ripser)  │  │          │  │                 │   │ │
│  │  └──────────┘  └──────────┘  └─────────────────┘   │ │
│  └────────────────────────────────────────────────────┘ │
│                           ↑                             │
│  ┌────────────────────────────────────────────────────┐ │
│  │              L3 Structure Layer                    │ │
│  │  ┌──────────┐  ┌──────────┐  ┌─────────────────┐   │ │
│  │  │  Bind    │  │ Relation │  │   Temporal      │   │ │
│  │  │ Operator │  │ Encoder  │  │   Encoder       │   │ │
│  │  └──────────┘  └──────────┘  └─────────────────┘   │ │
│  └────────────────────────────────────────────────────┘ │
│                           ↑                             │
│  ┌────────────────────────────────────────────────────┐ │
│  │              L2 Feature Layer                      │ │
│  │  ┌──────────┐  ┌──────────┐  ┌─────────────────┐   │ │
│  │  │ Embedder │  │   FAISS  │  │    LRU Cache    │   │ │
│  │  │ Models   │→ │  Index   │→ │    + Vector     │   │ │
│  │  │          │  │          │  │    Store        │   │ │
│  │  └──────────┘  └──────────┘  └─────────────────┘   │ │
│  └────────────────────────────────────────────────────┘ │
│                           ↑                             │
│  ┌────────────────────────────────────────────────────┐ │
│  │               L1 Raw Layer                         │ │
│  │  ┌──────────┐  ┌──────────┐  ┌─────────────────┐   │ │
│  │  │  File    │  │ Metadata │  │   Async Write   │   │ │
│  │  │ Storage  │← │  SQLite  │← │   Thread Pool   │   │ │
│  │  └──────────┘  └──────────┘  └─────────────────┘   │ │
│  └────────────────────────────────────────────────────┘ │
│                           ↑                             │
│  ┌────────────────────────────────────────────────────┐ │
│  │           Retrieval & Forgetting                   │ │
│  │  ┌──────────┐  ┌──────────┐  ┌─────────────────┐   │ │
│  │  │Attractor │  │  Mount   │  │   Ebbinghaus    │   │ │
│  │  │ Network  │  │ Context  │  │   Forgetting    │   │ │
│  │  └──────────┘  └──────────┘  └─────────────────┘   │ │
│  └────────────────────────────────────────────────────┘ │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

### 2.2 目录结构

```
agentos/atoms/memoryrovol/
├── CMakeLists.txt                 # 顶层构建文件
├── README.md                      # 模块说明
├── include/                       # 公共头文件
│   ├── memoryrovol.h             # 主接口定义
│   ├── layer1_raw.h              # L1 原始卷接口
│   ├── layer2_feature.h          # L2 特征层接口
│   ├── layer3_structure.h        # L3 结构层接口
│   ├── layer4_pattern.h          # L4 模式层接口
│   ├── retrieval.h               # 检索机制接口
│   ├── forgetting.h              # 遗忘机制接口
│   ├── vector_store.h            # 向量持久化接口
│   └── manager.h                  # 配置定义
└── src/                           # 源代码实现
    ├── CMakeLists.txt            # 子模块构建文件
    ├── memoryrovol.c             # 主入口
    ├── layer1_raw/               # L1 实现
    │   ├── storage.c            # 文件存储
    │   ├── metadata.c           # 元数据管理
    │   └── async_write.c        # 异步写入
    ├── layer2_feature/           # L2 实现
    │   ├── index.c              # FAISS 索引
    │   ├── embedder.c           # 嵌入模型
    │   ├── lru_cache.c          # LRU 缓存
    │   └── vector_store.c       # 向量持久化
    ├── layer3_structure/         # L3 实现
    │   ├── bind_operator.c      # 绑定算子
    │   ├── relation_encoder.c   # 关系编码
    │   └── temporal_encoder.c   # 时序编码
    ├── layer4_pattern/           # L4 实现
    │   ├── topology_analysis.c  # 持久同调
    │   ├── hdbscan_cluster.c    # 聚类
    │   └── rule_generator.c     # 规则生成
    ├── retrieval/                # 检索机制
    │   ├── attractor.c          # 吸引子网络
    │   ├── cache.c              # 检索缓存
    │   ├── mount.c              # 挂载机制
    │   └── reranker.c           # 重排序
    └── forgetting/               # 遗忘机制
        ├── ebbinghaus.c         # 艾宾浩斯曲线
        ├── linear_decay.c       # 线性衰减
        └── access_based.c       # 访问计数
```

---

## 3. L1 原始卷 (Raw Layer)

### 3.1 功能定位

L1 原始卷是记忆系统的基础层，负责原始数据的存储和管理，提供:
- 文件系统存储后端
- 异步写入支持
- SQLite 元数据索引
- 版本控制和压缩归档

### 3.2 数据结构

#### 元数据
```c
typedef struct agentos_raw_metadata {
    char* metadata_record_id;       // 记录 ID（系统生成）
    uint64_t metadata_timestamp;      // 时间戳（纳秒）
    char* metadata_source;           // 来源标识
    char* metadata_trace_id;         // 追踪 ID
    size_t metadata_data_len;        // 原始数据长度
    uint32_t metadata_access_count;  // 访问次数
    uint64_t metadata_last_access;   // 最后访问时间
    char* metadata_tags_json;        // 扩展标签（JSON）
} agentos_raw_metadata_t;
```

#### L1 层实例
```c
typedef struct agentos_layer1_raw {
    char* base_path;               // 存储根路径
    agentos_mutex_t* lock;         // 线程锁
    uint64_t next_id;              // 下一个可用 ID
    write_queue_t* queue;          // 异步写入队列
    uint32_t num_workers;          // 工作线程数
    // ...
} agentos_layer1_raw_t;
```

### 3.3 核心功能

#### 3.3.1 同步写入
```c
agentos_error_t agentos_layer1_raw_write(
    agentos_layer1_raw_t* layer,
    const void* data,
    size_t len,
    const char* metadata,
    char** out_record_id);
```

**特点**:
- 阻塞式写入，确保数据持久化
- 立即返回记录 ID
- 适合低延迟场景

#### 3.3.2 异步写入
```c
agentos_error_t agentos_layer1_raw_write_async(
    agentos_layer1_raw_t* layer,
    const void* data,
    size_t len,
    const char* metadata,
    void (*callback)(agentos_error_t, const char*, void*),
    void* userdata);
```

**特点**:
- 后台线程池执行
- 回调通知机制
- 高吞吐量（10,000+ 条/秒）
- 可配置队列大小和线程数

### 3.4 API 接口

#### 创建 L1 层
```c
// 同步模式
agentos_error_t agentos_layer1_raw_create(
    const char* base_path,
    agentos_layer1_raw_t** out_layer);

// 异步模式
agentos_error_t agentos_layer1_raw_create_async(
    const char* base_path,
    size_t queue_size,
    uint32_t num_workers,
    agentos_layer1_raw_t** out_layer);
```

#### 等待异步写入完成
```c
agentos_error_t agentos_layer1_raw_wait_complete(
    agentos_layer1_raw_t* layer,
    uint32_t timeout_ms);
```

---

## 4. L2 特征层 (Feature Layer)

### 4.1 功能定位

L2 特征层负责将原始数据转换为向量表示，并提供高效的相似度检索能力:
- 多嵌入模型集成
- FAISS 向量索引
- LRU 高速缓存
- 向量持久化存储

### 4.2 数据结构

#### 特征向量
```c
typedef struct agentos_feature_vector {
    float* vector_data;         // 向量数据
    size_t vector_dim;          // 维度
    int vector_ref_count;       // 引用计数
} agentos_feature_vector_t;
```

#### L2 层配置
```c
typedef struct agentos_layer2_feature_config {
    const char* config_index_path;          // 索引持久化路径
    const char* config_embedding_model;     // 嵌入模型名称
    const char* config_api_key;             // API 密钥
    const char* config_api_base;            // API 基础 URL
    size_t config_dimension;                // 向量维度（0 表示自动）
    uint32_t config_index_type;             // 索引类型（0=flat,1=ivf,2=hnsw）
    uint32_t config_ivf_nlist;              // IVF 聚类中心数
    uint32_t config_hnsw_m;                 // HNSW M 参数
    uint32_t config_cache_size;             // LRU 缓存大小（0 表示无限）
    const char* config_vector_store_path;   // 向量持久化路径
    uint32_t config_rebuild_interval_sec;   // 重建索引间隔（秒）
} agentos_layer2_feature_config_t;
```

### 4.3 核心组件

#### 4.3.1 嵌入模型

**支持的模型**:
- OpenAI embeddings (text-embedding-3-small/large)
- DeepSeek embeddings
- Sentence Transformers (all-MiniLM-L6-v2 等)

**接口**:
```c
agentos_error_t agentos_embedder_encode(
    embedder_handle_t* h,
    const char* text,
    float** out_vec,
    size_t* out_dim);
```

#### 4.3.2 FAISS 索引

**索引类型**:
- **Flat (0)**: 精确搜索，适合小规模数据
- **IVF (1)**: 倒排索引，适合中等规模
  - 参数：`nlist` (聚类中心数)
- **HNSW (2)**: 图索引，适合大规模
  - 参数：`M` (连接数)

#### 4.3.3 LRU 缓存

**功能**:
- 热点向量高速缓存
- 可配置缓存大小
- 自动淘汰策略
- 命中/缺失统计

#### 4.3.4 向量持久化

**SQLite 存储**:
```c
// 表结构
CREATE TABLE vectors (
    record_id TEXT PRIMARY KEY,
    vector BLOB NOT NULL,
    dimension INTEGER NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

// 存储向量
agentos_error_t agentos_vector_store_put(
    agentos_vector_store_t* store,
    const char* record_id,
    const float* vector,
    size_t dim);
```

### 4.4 API 接口

#### 创建 L2 层
```c
agentos_error_t agentos_layer2_feature_create(
    const agentos_layer2_feature_config_t* manager,
    agentos_layer2_feature_t** out_layer);
```

#### 添加向量
```c
agentos_error_t agentos_layer2_feature_add(
    agentos_layer2_feature_t* layer,
    const char* record_id,
    const char* text);
```

**流程**:
1. 使用嵌入模型生成向量
2. 检查是否已存在
3. 加入 LRU 缓存或直接写入存储
4. 添加到 FAISS 索引

#### 批量添加
```c
agentos_error_t agentos_layer2_feature_add_batch(
    agentos_layer2_feature_t* layer,
    const char** record_ids,
    const char** texts,
    size_t count);
```

---

## 5. L3 结构层 (Structure Layer)

### 5.1 功能定位

L3 结构层负责将多个记忆单元绑定为复合结构，并编码语义关系和时序信息:
- 绑定/解绑算子
- 关系编码器
- 时序编码
- 图神经网络编码（实验性）

### 5.2 核心组件

#### 5.2.1 绑定算子

**功能**:
- 将多个记忆单元绑定为复合结构
- 创建非交换的结合关系
- 支持嵌套绑定

**接口**:
```c
agentos_error_t agentos_layer3_bind(
    agentos_layer3_structure_t* layer,
    const char** member_ids,
    size_t count,
    const char* relation_type,
    char** out_bound_id);
```

#### 5.2.2 关系编码

**功能**:
- 显式编码记忆间的语义关系
- 支持多种关系类型（因果、包含、引用等）
- 关系图构建

**示例**:
```c
// 编码因果关系
agentos_layer3_add_relation(layer, cause_id, effect_id, "CAUSES");

// 编码包含关系
agentos_layer3_add_relation(layer, whole_id, part_id, "CONTAINS");
```

#### 5.2.3 时序编码

**功能**:
- 记录记忆的时间顺序
- 识别因果关系
- 时间窗口聚合

**实现**:
```c
typedef struct temporal_encoding {
    uint64_t start_time;
    uint64_t end_time;
    char* sequence_json;  // 时间序列
} temporal_encoding_t;
```

---

## 6. L4 模式层 (Pattern Layer)

### 6.1 功能定位

L4 模式层负责从大量记忆中挖掘高级模式和规则:
- 持久同调分析（拓扑数据分析）
- HDBSCAN 密度聚类
- 稳定模式识别
- 规则生成引擎

### 6.2 核心组件

#### 6.2.1 持久同调分析

**技术**: 基于 Ripser 库的拓扑数据分析

**功能**:
- 计算点云的持久同调
- 识别拓扑特征（连通分量、洞、空洞等）
- 生成持久图

**接口**:
```c
agentos_error_t agentos_layer4_persistence_analyze(
    agentos_layer4_pattern_t* layer,
    const float* point_cloud,
    size_t point_count,
    size_t dim,
    char** out_persistence_diagram);
```

#### 6.2.2 HDBSCAN 聚类

**功能**:
- 基于密度的聚类
- 自动发现簇数量
- 识别噪声点

**实现**:
```c
// 使用 HDBSCAN 库
hdbscan_cluster(
    points, n_points, dim,
    min_cluster_size,
    &labels, &n_clusters);
```

#### 6.2.3 规则生成

**功能**:
- 从模式中提炼可复用规则
- 规则评估和验证
- 规则库管理

**示例输出**:
```json
{
  "rule_id": "rule_001",
  "pattern": "IF condition_A AND condition_B THEN action_C",
  "confidence": 0.95,
  "support": 120
}
```

---

## 7. 检索机制 (Retrieval)

### 7.1 吸引子网络

#### 功能定位

吸引子网络是一种基于动力学的检索机制，通过迭代演化找到与查询最匹配的记忆:

```
初始状态（查询向量）
   ↓
[能量函数最小化]
   ↓
[状态演化]
   ↓
收敛到吸引子 basin
   ↓
输出最佳匹配
```

#### 实现细节

```c
agentos_error_t agentos_attractor_network_retrieve(
    agentos_attractor_network_t* net,
    const float* query_vector,
    const char** candidate_ids,
    size_t candidate_count,
    char** out_best_id,
    float* out_confidence) {
    
    // 1. 初始化状态
    float* state = copy_vector(query_vector);
    
    // 2. 迭代演化
    for (size_t iter = 0; iter < max_iterations; iter++) {
        // 计算能量梯度
        float* gradient = compute_energy_gradient(state, candidates);
        
        // 更新状态
        float* new_state = evolve_state(state, gradient, beta);
        
        // 检查收敛
        if (convergence_check(state, new_state, tolerance)) {
            break;
        }
        
        state = new_state;
    }
    
    // 3. 找到最接近的候选
    find_best_candidate(state, candidates, out_best_id, out_confidence);
}
```

#### 参数配置

```c
typedef struct agentos_retrieval_config {
    uint32_t max_iterations;       // 最大迭代次数
    float tolerance;               // 收敛容差
    float beta;                    // 非线性参数
} agentos_retrieval_config_t;
```

### 7.2 检索缓存

#### 功能

- 缓存历史查询结果
- LRU 淘汰策略
- 提升重复查询速度

#### 接口

```c
// 创建缓存
agentos_error_t agentos_retrieval_cache_create(
    size_t max_size,
    agentos_retrieval_cache_t** out_cache);

// 获取缓存
agentos_error_t agentos_retrieval_cache_get(
    agentos_retrieval_cache_t* cache,
    const char* query,
    char*** out_result_ids,
    size_t* out_count);
```

### 7.3 挂载机制

#### 功能

- 将记忆挂载到当前上下文
- 更新访问计数
- 感知记忆使用情况

#### 接口

```c
agentos_error_t agentos_mounter_mount(
    agentos_mounter_t* mounter,
    const char* record_id,
    const char* context_id);
```

### 7.4 重排序

#### 功能

- 使用交叉编码器对初筛结果精排
- 提升结果相关性
- 支持自定义重排序模型

#### 性能

- 延迟：< 50ms (top-100)
- 精度提升：10-20%

---

## 8. 遗忘机制 (Forgetting)

### 8.1 功能定位

遗忘机制模拟人类记忆的遗忘过程，自动裁剪低价值记忆，保持记忆系统的精炼:
- 艾宾浩斯曲线衰减
- 线性衰减
- 基于访问次数的策略

### 8.2 遗忘策略

#### 8.2.1 艾宾浩斯曲线

**公式**:
```
R(t) = exp(-t / λ)
```

其中:
- `R(t)`: 记忆保留率
- `t`: 时间（天）
- `λ`: 衰减率（可配置）

**实现**:
```c
double ebbinghaus_forgetting(double days, double lambda) {
    return exp(-days / lambda);
}

// 检查是否应该遗忘
bool should_forget(uint64_t created_time, double importance, 
                   uint32_t access_count, double threshold) {
    double days = (now() - created_time) / 86400000000000.0; // 纳秒→天
    double retention = ebbinghaus_forgetting(days, lambda);
    double score = retention * importance * log(access_count + 1);
    return score < threshold;
}
```

#### 8.2.2 线性衰减

**公式**:
```
weight(t) = max(0, initial_weight - decay_rate * t)
```

#### 8.2.3 基于访问次数

**策略**:
- 记录每次访问时间和计数
- LRU (Least Recently Used)
- LFU (Least Frequently Used)

### 8.3 自动遗忘任务

#### 配置

```c
typedef struct agentos_forgetting_config {
    agentos_forget_strategy_t strategy;   // 策略类型
    double lambda;                         // 衰减率（Ebbinghaus）
    double threshold;                      // 裁剪阈值
    uint32_t min_access;                   // 最小访问次数
    uint32_t check_interval_sec;           // 检查间隔（秒）
    const char* archive_path;              // 归档路径
} agentos_forgetting_config_t;
```

#### 工作流程

```c
// 后台线程
void* forgetting_thread(void* arg) {
    while (!engine->shutdown) {
        sleep(engine->check_interval_sec);
        
        // 执行一次修剪
        uint32_t pruned_count;
        agentos_forgetting_prune(engine, &pruned_count);
        
        AGENTOS_LOG_INFO("Pruned %u memories", pruned_count);
    }
}
```

---

## 9. 性能指标

### 9.1 基准测试

**测试环境**: Intel i7-12700K, 32GB RAM, NVMe SSD, Linux 6.5

#### 处理能力

| 指标 | 数值 | 测试条件 |
| :--- | :--- | :--- |
| **记忆写入吞吐** | 10,000+ 条/秒 | L1 异步批量写入 |
| **向量检索延迟** | < 10ms | FAISS IVF1024,PQ64, k=10 |
| **混合检索延迟** | < 50ms | 向量+BM25, top-100 重排序 |
| **记忆抽象速度** | 100 条/秒 | L2→L3 渐进式抽象 |
| **模式挖掘速度** | 10 万条/分钟 | L4 持久同调分析 |

#### 资源利用率

| 场景 | CPU 占用 | 内存占用 | 磁盘 IO |
| :--- | :--- | :--- | :--- |
| **空闲状态** | < 5% | 200MB | < 1MB/s |
| **中等负载** | 30-50% | 1-2GB | 10-50MB/s |
| **高负载** | 80-100% | 4-8GB | 100-500MB/s |

### 9.2 扩展性

- **水平扩展**: 支持多节点分布式部署（规划中）
- **垂直扩展**: 可配置资源限制和分配
- **弹性伸缩**: 根据负载自动调整（规划中）

---

## 10. 开发指南

### 10.1 快速开始

#### 创建 MemoryRovol 实例

```c
#include "memoryrovol.h"

agentos_memoryrov_config_t manager = {0};
manager.raw_storage_path = "/path/to/memory/raw";
manager.index_path = "/path/to/memory/index";
manager.index_type = 2;  // HNSW
manager.hnsw_m = 32;
manager.forget_strategy = AGENTOS_FORGET_EBBINGHAUS;
manager.forget_lambda = 0.5;

agentos_memoryrov_handle_t* handle;
agentos_error_t err = agentos_memoryrov_init(&manager, &handle);
if (err != AGENTOS_SUCCESS) {
    fprintf(stderr, "Error: %s\n", agentos_strerror(err));
    return err;
}
```

#### 写入记忆

```c
// L1 写入
char* record_id;
const char* data = "这是一条测试记忆";
err = agentos_layer1_raw_write(layer1, data, strlen(data), NULL, &record_id);

// L2 添加向量
err = agentos_layer2_feature_add(layer2, record_id, data);

free(record_id);
```

#### 检索记忆

```c
// 使用吸引子网络
char* best_id;
float confidence;
err = agentos_attractor_network_retrieve(net, query_vector, 
                                          candidate_ids, count,
                                          &best_id, &confidence);
```

### 10.2 配置示例

#### 高性能配置

```c
agentos_layer2_feature_config_t manager = {
    .index_type = 2,              // HNSW
    .hnsw_m = 32,                 // 更大的 M
    .cache_size = 100000,         // 大缓存
    .rebuild_interval_sec = 1800, // 30 分钟重建
};
```

#### 低内存配置

```c
agentos_layer2_feature_config_t manager = {
    .index_type = 1,              // IVF
    .ivf_nlist = 100,             // 少量聚类中心
    .cache_size = 1000,           // 小缓存
    .vector_store_path = "/path/to/db", // 启用持久化
};
```

---

## 11. 故障排查

### 11.1 常见问题

#### 问题：向量检索结果为空

**症状**: `agentos_layer2_feature_search()` 返回 0 结果  
**排查**:
1. 确认 FAISS 索引已创建
2. 检查是否有向量数据
3. 验证查询向量维度是否正确

#### 问题：异步写入队列满

**症状**: `agentos_layer1_raw_write_async()` 返回队列满错误  
**排查**:
1. 增加队列大小 (`queue_size`)
2. 增加工作线程数 (`num_workers`)
3. 检查写入速度是否过慢

### 11.2 调试技巧

- 启用 Debug 日志级别
- 使用 `agentos_memoryrov_stats()` 查看统计信息
- 监控 LRU 缓存命中率

---

## 12. 参考资料

- [README.md](../../README.md) - 项目总览
- [coreloopthree.md](coreloopthree.md) - CoreLoopThree 架构详解
- [layer1_raw.h](../include/layer1_raw.h) - L1 头文件
- [layer2_feature.h](../include/layer2_feature.h) - L2 头文件
- [retrieval.h](../include/retrieval.h) - 检索机制头文件
- [forgetting.h](../include/forgetting.h) - 遗忘机制头文件

---

<div align="center">

**© 2026 SPHARX Ltd. All Rights Reserved.**

*From data intelligence emerges*

</div>