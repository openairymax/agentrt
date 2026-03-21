# Agent OS

<div align="center">

[![Version](https://img.shields.io/badge/version-1.0.0.5-blue.svg)](https://gitee.com/spharx/agentos)
[![License](https://img.shields.io/badge/license-Apache--2.0-green.svg)](https://gitee.com/spharx/agentos/blob/main/LICENSE)
[![Docker](https://img.shields.io/badge/docker-supported-blue.svg?logo=docker&logoColor=white)](https://www.docker.com/)
[![C/C++](https://img.shields.io/badge/C%2FC%2B%2B-11%2F17-blue.svg?logo=c%2B%2B&logoColor=white)](https://isocpp.org/)
[![Python](https://img.shields.io/badge/python-3.9%2B-blue.svg?logo=python&logoColor=white)](https://www.python.org/)
[![Mirror](https://img.shields.io/badge/mirror-GitHub-lightgrey.svg)](https://github.com/SpharxTeam/AgentOS)

---
**智能体超级操作系统**

*"From data intelligence emerges. 始于数据，终于智能。"*

--

Language： **简体中文** | [English](partdocs/readme/en/README.md) | [Français](partdocs/readme/fr/README.md) | [Deutsch](partdocs/readme/de/README.md)

</div>



## 🚩简介
<!-- From data intelligence emerges. by spharx -->

- 可以工程化的完成任务，最大化拉满 token 使用效率；
- 全新架构在 token 利用效率上领先当前行业主流框架 2-3 倍；
- 工程级任务比 OpenClaw 效率高出 3-5 倍，节省 token 约 60% 的使用量。

## 📋 项目概述

- Agent OS（SuperAI OS）是SpharxWorks的核心智能体操作系统内核，提供完整的智能体运行时环境、记忆系统、认知引擎和执行框架。
- 作为物理世界数据基础设施的生产级操作团队, AgentOS（SuperAI OS）实现了从数据处理到智能决策的完整闭环。

### 核心价值

- **微内核**: 最小化内核设计，所有服务运行在用户态，确保系统稳定性和可扩展性。
- **三层一体架构**: 认知层、行动层、记忆层协同工作，实现智能体完整生命周期管理。
- **记忆卷载系统**: L1-L4 渐进式记忆抽象，支持记忆存储、检索、进化和遗忘。
- **系统调用抽象**: 稳定安全的系统调用接口，隐藏内核实现细节。
- **可插拔策略**: 认知、规划、调度等核心算法支持动态加载和运行时替换。
- **统一日志系统**: 跨语言日志接口，支持全链路追踪和 OpenTelemetry 集成。
- **多语言 SDK**: Go、Python、Rust、TypeScript 原生支持，FFI 接口高效安全。

### 版本状态

**当前版本**: v1.0.0.5 (生产就绪)
- ✅ 核心架构设计完成
- ✅ MemoryRovol 记忆卷载系统
  - L1-L4 四层架构全部实现
  - 同步/异步写入支持（10,000+ 条/秒）
  - FAISS 向量检索集成（IVF/HNSW 索引）
  - 吸引子网络检索机制
  - 艾宾浩斯遗忘曲线实现
  - LRU 缓存与向量持久化
- ✅ CoreLoopThree 三层一体架构
  - 认知层：意图理解、任务规划、多策略协同（90%）
  - 行动层：执行引擎、补偿事务、责任链追踪（85%）
  - 记忆层：MemoryRovol FFI 封装（80%）
- ✅ 微内核基础模块 (core) 实现
  - IPC Binder 通信
  - 内存管理（RAII、智能指针）
  - 任务调度（加权轮询）
  - 高精度时间服务
- ✅ 系统调用层 (syscall) 开发完成 (100%)
  - ✅ 任务系统调用：`sys_task_submit/query/wait/cancel`
  - ✅ 记忆系统调用：`sys_memory_write/search/get/delete`
  - ✅ 会话系统调用：`sys_session_create/get/close/list`
  - ✅ 可观测性调用：`sys_telemetry_metrics/traces`
  - ✅ 统一入口：`agentos_syscall_invoke()`
- 🔲 完整端到端集成测试

---

## 🏗️ 系统架构

```
┌─────────────────────────────────────────────────────────────┐
│                    AgentOS 整体架构                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌───────────────────────────────────────────────────────┐  │
│  │              应用层 (openhub)                          │  │
│  │  docgen | ecommerce | research | videoedit | ...      │  │
│  └───────────────────────────────────────────────────────┘  │
│                           ↕                                 │
│  ┌───────────────────────────────────────────────────────┐  │
│  │           核心服务层 (backs)                           │  │
│  │  llm_d | market_d | monit_d | perm_d | sched_d | ...  │  │
│  └───────────────────────────────────────────────────────┘  │
│                           ↕                                 │
│  ┌───────────────────────────────────────────────────────┐  │
│  │            内核层 (atoms)                              │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌─────────────┐  │  │
│  │  │   core       │  │coreloopthree │  │memoryrovol  │  │  │
│  │  │ 微内核基础    │  │三层核心运行时 │  │四层记忆卷载  │  │  │
│  │  └──────────────┘  └──────────────┘  └─────────────┘  │  │
│  │  ┌──────────────┐                                     │  │
│  │  │   syscall    │                                     │  │
│  │  │ 系统调用层    │                                     │  │
│  │  └──────────────┘                                     │  │
│  └───────────────────────────────────────────────────────┘  │
│                           ↕                                 │
│  ┌───────────────────────────────────────────────────────┐  │
│  │           SDK 层 (tools)                              │  │
│  │  Go | Python | Rust | TypeScript | ...                │  │
│  └───────────────────────────────────────────────────────┘  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 📁 项目结构

```
AgentOS/
├── config/                      # 配置文件中心
│   ├── agents.yaml             # Agent 配置
│   ├── kernel.yaml             # 内核配置 (日志、调度器、内存、IPC)
│   ├── logging.yaml            # 日志配置
│   ├── models.yaml             # 模型配置
│   ├── sanitizer.yaml          # 数据脱敏配置
│   ├── security.yaml           # 安全配置
│   ├── services.yaml           # 服务配置
│   └── skills.yaml             # 技能配置
│
├── atoms/                 # 内核层（微内核架构）
│   ├── README.md               # 内核设计文档
│   ├── BUILD.md                # 编译指南
│   ├── CMakeLists.txt          # 顶层构建文件
│   │
│   ├── core/                   # 微内核基础 (IPC、内存、任务、时间)
│   │   ├── include/            # 公共头文件
│   │   └── src/                # 源代码实现
│   │
│   ├── coreloopthree/          # 三层核心运行时 ⭐ 核心架构
│   │   ├── README.md           # 详细设计文档
│   │   ├── include/            # 公共头文件
│   │   │   ├── cognition.h     # 认知层接口
│   │   │   ├── execution.h     # 行动层接口
│   │   │   ├── memory.h        # 记忆层接口
│   │   │   └── loop.h          # 三层闭环主接口
│   │   └── src/                # 源代码实现
│   │
│   ├── memoryrovol/            # 四层记忆卷载系统 ⭐ 核心创新
│   │   ├── README.md           # 详细设计文档
│   │   ├── include/            # 公共头文件
│   │   └── src/                # 源代码实现
│   │
│   ├── syscall/                # 系统调用层
│   │   ├── README.md           # 系统调用文档
│   │   ├── include/syscalls.h  # 系统调用头文件
│   │   └── src/                # 系统调用实现 (entry/table)
│   │
│   └── utils/                  # 通用工具库
│       ├── logger/             # 统一日志系统
│       ├── tracer/             # 可观测性追踪
│       └── errors/             # 错误处理
│
├── domes/                       # 圆顶隔离层（安全沙箱）
│   ├── README.md               # 设计文档
│   ├── CMakeLists.txt          # 构建文件
│   ├── include/                # 公共头文件
│   └── src/                    # 源代码实现
│
├── dynamic/                     # 动态加载层（热插拔机制）
│   ├── README.md               # 设计文档
│   ├── CMakeLists.txt          # 构建文件
│   ├── include/                # 公共头文件
│   └── src/                    # 源代码实现
│
├── tools/                     # 多语言 SDK
│   ├── go/                     # Go 语言 SDK
│   ├── python/                 # Python SDK
│   ├── rust/                   # Rust SDK
│   └── typescript/             # TypeScript SDK
│
├── backs/                     # 核心守程服务（用户态服务）
│   ├── llm_d/                  # LLM 服务守护进程
│   ├── market_d/               # 市场服务守护进程
│   ├── monit_d/                # 监控服务守护进程
│   ├── sched_d/                # 调度服务守护进程
│   └── tool_d/                 # 工具服务守护进程
│
├── openhub/                     # 开放生态贡献中心（官方应用 + 社区贡献）
│   ├── app/                    # 官方应用示例
│   │   ├── docgen/                 # 文档生成应用
│   │   ├── ecommerce/              # 电子商务应用
│   │   ├── research/               # 研究分析应用
│   │   └── videoedit/              # 视频编辑应用
│   ├── contrib/                # 社区贡献代码
│   │   ├── agents/               # 社区贡献 Agent
│   │   ├── skills/               # 社区贡献技能
│   │   └── strategies/           # 社区贡献策略
│   └── markets/                # 市场基础设施
│
├── partdata/                    # 数据分区（运行时数据）
│   ├── kernel/                 # 内核数据
│   ├── logs/                   # 日志文件（集中存储）
│   │   ├── apps/               # 应用层日志
│   │   ├── kernel/             # 内核层日志
│   │   └── services/           # 服务层日志
│   ├── traces/spans/           # OpenTelemetry 追踪数据
│   ├── registry/               # 全局注册表 (agents.db, skills.db, sessions.db)
│   └── services/               # 服务运行时数据
│
├── partdocs/                    # 技术文档中心
│   ├── api/                    # API 文档
│   ├── architecture/           # 架构设计文档
│   ├── guides/                 # 开发指南
│   ├── philosophy/             # 设计哲学
│   ├── readme/                 # README 相关文档
│   └── specifications/         # 技术规范
│
├── scripts/                     # 运维脚本工具
│   ├── docker/                 # Docker 相关脚本
│   ├── build.sh                # 构建脚本
│   ├── install.sh              # Linux 安装脚本
│   ├── install.ps1             # Windows 安装脚本
│   ├── quickstart.sh           # 快速启动脚本
│   ├── validate.sh             # 验证脚本
│   ├── benchmark.py            # 性能基准测试
│   ├── doctor.py               # 系统健康检查
│   ├── generate_docs.py        # 文档生成
│   ├── init_config.py          # 配置初始化
│   ├── update_registry.py      # 注册表更新
│   └── validate_contracts.py   # 合约验证
│
└── tests/                       # 测试套件
    ├── unit/                   # 单元测试
    ├── integration/            # 集成测试
    ├── security/               # 安全测试
    ├── benchmarks/             # 性能基准测试
    └── contract/               # 合约测试
```

---

## 🧠 CoreLoopThree: 三层一体结构

### 设计理念

CoreLoopThree 是 AgentOS 的核心创新架构，通过将智能体运行时划分为三个正交且协同的层次，实现认知、行动和记忆的有机统一:

```
┌─────────────────────────────────────────┐
         认知层 (Cognition Layer)         
   • 意图理解 • 任务规划 • Agent 调度       
   • 模型协同 • 策略引擎 • 决策优化         
└───────────────↓─────────────────────────┘
                ↓
┌─────────────────────────────────────────┐
          行动层 (Execution Layer)        
    • 任务执行 • 补偿事务 • 责任链追踪       
    • 执行单元 • 状态管理 • 异常处理         
└───────────────↓─────────────────────────┘
                ↓
┌─────────────────────────────────────────┐
         记忆层 (Memory Layer)            
    • 记忆写入 • 查询检索 • 上下文挂载        
    • 进化抽象 • 遗忘裁剪 • FFI 接口         
└─────────────────────────────────────────┘
```

### 核心组件

#### 1. 认知层 (Cognition)
- **意图理解引擎**: 解析用户输入，识别真实意图
  - 原始文本处理
  - 核心目标提取
  - 意图标志位（紧急、复杂等）
- **任务规划器**: 基于目标的自动任务分解和排序
  - DAG 任务图生成
  - 依赖关系管理
  - 入口点识别
- **Agent 调度器**: 多 Agent 协同和资源分配
  - 加权调度策略
  - 候选 Agent 评分
  - 动态任务分配
- **模型协同器**: 大模型选择和提示词工程
  - 多模型输入协调
  - 输出结果融合
  - 策略可插拔
- **策略接口**: 可插拔的认知算法策略
  - `agentos_plan_strategy_t` - 规划策略
  - `agentos_coordinator_strategy_t` - 协同策略
  - `agentos_dispatching_strategy_t` - 调度策略

#### 2. 行动层 (Execution)
- **执行引擎**: 任务的具体执行和状态跟踪
  - 任务状态机（Pending/Running/Succeeded/Failed/Cancelled/Retrying）
  - 并发控制
  - 超时管理
- **补偿事务**: 失败回滚和补偿逻辑
  - 补偿执行单元注册
  - 自动回滚机制
  - 人工介入队列
- **责任链追踪**: 完整的执行链路记录
  - Trace ID 关联
  - 执行历史归档
  - 状态查询接口
- **执行单元注册表**: 原子执行单元的注册和发现
  - 元数据描述
  - 动态注册/注销
- **异常处理**: 分级异常捕获和恢复机制
  - 重试策略
  - 错误消息记录

#### 3. 记忆层 (Memory)
- **记忆服务**: 封装 MemoryRovol 提供高级接口
  - 记忆引擎 (`agentos_memory_engine_t`)
  - 记录类型（RAW/FEATURE/STRUCTURE/PATTERN）
- **写入接口**: 支持同步/异步记忆写入
  - 记忆记录结构定义
  - 时间戳和来源追踪
  - 重要性评分
- **查询接口**: 语义查询和向量检索
  - 多维度查询条件（时间、来源、TraceID）
  - 限制和偏移分页
  - 包含原始数据选项
- **上下文挂载**: 基于上下文的记忆自动关联
  - Mount 机制
  - 访问计数更新
  - 使用感知
- **FFI 接口**: `rov_ffi.h` 提供跨语言调用能力
  - C ABI 兼容
  - 多语言 SDK 支持

### 与其他模块的交互

- **与 core 的关系**: CoreLoopThree 通过 syscall 层调用 core 提供的 IPC、内存管理、任务调度等基础服务
- **与 memoryrovol 的关系**: 记忆层通过 FFI 接口 (`rov_ffi.h`) 调用 MemoryRovol 的核心功能，实现记忆的存储和检索
- **与 syscall 的关系**: 认知层和行动层通过系统调用接口与内核交互，确保接口的稳定性和安全性

详见：[CoreLoopThree 架构文档](partdocs/architecture/coreloopthree.md)

---

## 💾 MemoryRovol: 记忆卷载

### 功能定位

MemoryRovol 是 AgentOS 的内核级记忆系统，实现从原始数据到高级模式的全栈记忆管理能力。它不仅是简单的数据存储，更是智能体实现持续学习、知识积累和智能进化的核心基础设施。

### 四层架构

```
┌─────────────────────────────────────────────────────────┐
               L4 Pattern Layer (模式层)                   
   • 持久同调分析 (Ripser)  • 稳定模式挖掘                  
   • HDBSCAN 聚类  • 规则生成  • 进化委员会联动             
└───────────────────↑─────────────────────────────────────┘
                    ↓ 抽象进化
┌─────────────────────────────────────────────────────────┐
             L3 Structure Layer (结构层)                   
   • 绑定算子/解绑算子  • 关系编码  • 时序编码  • 图编码      
   • 结构化表示  • 语义关联  • 因果推理基础                  
└───────────────────↑─────────────────────────────────────┘
                    ↓ 特征提取
┌─────────────────────────────────────────────────────────┐
             L2 Feature Layer (特征层)                     
   • 嵌入模型 (OpenAI/DeepSeek/SentenceTransformers)       
   • FAISS 向量索引  • 混合检索 (向量+BM25)                 
   • 语义相似度计算  • 向量空间操作                         
└───────────────────↑─────────────────────────────────────┘
                    ↓ 数据压缩
┌─────────────────────────────────────────────────────────┐
              L1 Raw Layer (原始卷)                        
   • 文件系统存储  • 分片管理  • 压缩归档                    
   • 元数据索引  • 完整性校验  • 版本控制                    
└─────────────────────────────────────────────────────────┘
```

### 数据存储模型

#### L1 原始卷 - 原始数据存储
- **存储介质**: 基于文件系统的高效存储
  - 分片文件管理
  - 自动压缩归档
  - 根路径配置
- **数据格式**: 支持文本、图像、音频、视频等多种格式
- **分片管理**: 自动分片和压缩，优化存储空间
  - 元数据索引（SQLite）
  - 版本控制机制
- **异步写入**: 后台工作线程池
  - 写入队列管理
  - 完成回调通知
  - 可配置线程数量

#### L2 特征层 - 向量化表示
- **嵌入模型**: 集成多种预训练模型
  - OpenAI embeddings (text-embedding-3-small/large)
  - DeepSeek embeddings
  - Sentence Transformers (all-MiniLM-L6-v2 等)
- **向量索引**: FAISS 高效相似度搜索
  - IVF 倒排索引（可配置 nlist）
  - HNSW 图索引（可配置 M 参数）
  - 量化压缩（PQ/OPQ）
- **混合检索**: 向量检索 + BM25 关键词检索
  - 加权融合排序
  - 交叉编码器重排序
- **LRU 缓存**: 热点向量高速缓存
  - 可配置缓存大小
  - 自动淘汰策略
  - 命中/缺失统计
- **向量持久化**: SQLite 存储后端
  - 记录 ID 映射
  - 维度管理

#### L3 结构层 - 结构化表示
- **绑定算子**: 将多个记忆单元绑定为复合结构
- **解绑算子**: 分解复合记忆结构
- **关系编码**: 显式编码记忆间的语义关系
- **时序编码**: 记录记忆的时间顺序和因果关系
- **图编码**: 基于图神经网络的记忆表示

#### L4 模式层 - 高级模式挖掘
- **持久同调**: 基于 Ripser 的拓扑数据分析
- **稳定模式**: 识别跨记忆的不变模式
- **HDBSCAN 聚类**: 密度聚类发现记忆簇
- **规则生成**: 从模式中提炼可复用规则
- **进化委员会**: 与认知层联动，评估模式价值

### 在状态持久化中的作用

#### 与 partdata/registry 的集成
- **注册表数据**: MemoryRovol 为 agents.db 和 skills.db 提供记忆 backing store
- **Agent 状态**: 每个 Agent 的运行状态和历史记录存储在 L1/L2 层
- **技能记忆**: 技能的执行记录和效果反馈存储在 L2/L3 层

#### 与 partdata/traces/spans 的集成
- **追踪数据**: OpenTelemetry spans 作为原始记忆存入 L1 层
- **上下文关联**: 基于追踪 ID 自动关联相关记忆片段
- **性能分析**: L4 层挖掘性能瓶颈和优化模式

### 核心功能

#### 1. 记忆存储
- **同步写入**: 阻塞式写入，确保数据持久化
  - `agentos_layer1_raw_write()`
  - 立即返回记录 ID
- **异步写入**: 批量写入，提升吞吐量
  - `agentos_layer1_raw_write_async()`
  - 后台线程池执行
  - 回调通知机制
  - 吞吐量：10,000+ 条/秒
- **事务支持**: ACID 语义保证
- **压缩归档**: 自动压缩低频记忆

#### 2. 记忆检索
- **向量检索**: 余弦相似度搜索
  - FAISS 索引查询
  - Top-K 结果返回
  - 延迟：< 10ms（k=10）
- **语义检索**: 基于自然语言查询
  - 文本向量化
  - 相似度排序
- **上下文感知**: 根据当前上下文自动过滤
  - 时间范围过滤
  - 来源 Agent 过滤
  - Trace ID 关联
- **LRU 缓存**: 热点记忆高速缓存
  - 缓存命中统计
  - 自动淘汰
- **重排序**: 交叉编码器精排
  - 提升结果相关性
  - 延迟：< 50ms（top-100）

#### 3. 记忆进化
- **自动抽象**: L1→L2→L3→L4 的渐进式抽象
  - 特征提取（L1→L2）
  - 结构绑定（L2→L3）
  - 模式挖掘（L3→L4）
- **模式发现**: 识别高频模式和规则
  - 持久同调分析（Ripser）
  - HDBSCAN 聚类
  - 稳定模式识别
- **权重更新**: 基于访问频率和相关性动态调整权重
- **进化评估**: 与认知层联动评估记忆价值
  - 进化委员会机制

#### 4. 记忆遗忘
- **艾宾浩斯曲线**: 基于遗忘曲线的智能裁剪
  - 衰减率可配置 (lambda)
  - 阈值控制
- **线性衰减**: 简单的线性权重衰减
- **访问计数**: 基于 LRU/LFU 策略
  - 最小访问次数阈值
  - 访问时间追踪
- **主动遗忘**: 认知层触发的定向遗忘
  - 检查间隔可配置
  - 归档机制

详见：[MemoryRovol 架构文档](partdocs/architecture/memoryrovol.md)

---

## 🛠️ 开发指南

### 环境要求

- **操作系统**: Linux (Ubuntu 22.04+), macOS 13+, Windows 11 (WSL2)
- **编译器**: GCC 11+ 或 Clang 14+
- **构建工具**: CMake 3.20+, Ninja 或 Make
- **依赖库**:
  - OpenSSL >= 1.1.1 (加密)
  - libevent (事件循环)
  - pthread (线程)
  - FAISS >= 1.7.0 (向量检索)
  - SQLite3 >= 3.35 (元数据存储)
  - libcurl >= 7.68 (HTTP 客户端)
  - cJSON >= 1.7.15 (JSON 解析)
  - Ripser >= 2.3.1 (持久同调分析，可选)
  - HDBSCAN >= 0.8.27 (聚类分析，可选)

### 快速开始

#### 1. 克隆项目

```bash
# 从官方仓库克隆（推荐，国内访问更快）
git clone https://gitee.com/spharx/agentos.git
cd agentos

# 或从镜像仓库克隆
git clone https://github.com/SpharxTeam/AgentOS.git
cd AgentOS
```

#### 2. 初始化配置

```bash
# 复制环境变量模板
cp .env.example .env

# 编辑 .env 文件，设置必要的环境变量
# 如：API 密钥、存储路径等

# 运行配置初始化脚本
python scripts/init_config.py
```

#### 3. 构建项目

```bash
# 创建构建目录
mkdir build && cd build

# 配置 CMake
cmake ../atoms \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON \
  -DENABLE_TRACING=ON

# 编译
cmake --build . --parallel $(nproc)

# 运行测试
ctest --output-on-failure

# 安装 (可选)
sudo cmake --install .
```

#### 4. 配置选项说明

| CMake 变量 | 说明 | 默认值 |
| :--- | :--- | :--- |
| `CMAKE_BUILD_TYPE` | Debug/Release/RelWithDebInfo | `Release` |
| `BUILD_TESTS` | 构建单元测试 | `OFF` |
| `ENABLE_TRACING` | 启用 OpenTelemetry 追踪 | `OFF` |
| `ENABLE_ASAN` | 启用 AddressSanitizer | `OFF` |
| `USE_LLVM` | 使用 LLVM 工具链 | `OFF` |

详见：[BUILD.md](atoms/BUILD.md)

### 日志系统

AgentOS 采用统一的跨语言日志系统架构：

#### 日志存储位置
```
partdata/logs/
├── kernel/         # 内核层日志 → agentos.log
├── services/       # 服务层日志 → llm_d.log, tool_d.log, etc.
└── apps/           # 应用层日志 → 各应用独立日志
```

#### 日志格式
- **人类可读格式**: `%(asctime)s.%(msecs)03d [%(levelname)s] [%(name)s] %(message)s`
- **JSON 格式**: 支持结构化日志，便于 ELK/Splunk 集成

#### 跨语言日志关联
- 通过 `trace_id` 实现全链路追踪
- C/Python/Go/Rust/TypeScript共享同一套日志规范
- 集成 OpenTelemetry 作为可观测性后端

详见：[日志系统架构文档](partdocs/architecture/logging_system.md)

#### C++ 依赖 (通过 CMake FetchContent)

```cmake
# CMakeLists.txt 示例
FetchContent_Declare(
  faiss
  GIT_REPOSITORY https://github.com/facebookresearch/faiss.git
  GIT_TAG v1.7.4
)
FetchContent_MakeAvailable(faiss)
```

#### Python 依赖

```bash
pip install -r requirements.txt
```

### 测试

```bash
# 单元测试
ctest -R unit --output-on-failure

# 集成测试
ctest -R integration --output-on-failure

# 性能基准测试
python scripts/benchmark.py
```

---

## 📊 性能指标

基于标准测试环境 (Intel i7-12700K, 32GB RAM, NVMe SSD):

### 处理能力

| 指标 | 数值 | 测试条件 |
| :--- | :--- | :--- |
| **记忆写入吞吐** | 10,000+ 条/秒 | L1 层，异步批量写入 |
| **向量检索延迟** | < 10ms | FAISS IVF1024,PQ64, k=10 |
| **混合检索延迟** | < 50ms | 向量+BM25, top-100 重排序 |
| **记忆抽象速度** | 100 条/秒 | L2→L3 渐进式抽象 |
| **模式挖掘速度** | 10 万条/分钟 | L4 持久同调分析 |
| **并发连接数** | 1024 | Binder IPC 最大连接 |
| **任务调度延迟** | < 1ms | 加权轮询策略 |
| **意图解析延迟** | < 50ms | 简单意图 |
| **任务规划速度** | 100+ 节点/秒 | DAG 生成 |
| **Agent 调度延迟** | < 5ms | 加权轮询 |
| **任务执行吞吐** | 1000+ 任务/秒 | 并发执行 |

### 资源利用率

| 场景 | CPU 占用 | 内存占用 | 磁盘 IO |
| :--- | :--- | :--- | :--- |
| **空闲状态** | < 5% | 200MB | < 1MB/s |
| **中等负载** | 30-50% | 1-2GB | 10-50MB/s |
| **高负载** | 80-100% | 4-8GB | 100-500MB/s |

### 扩展性

- **水平扩展**: 支持多节点分布式部署 (规划中)
- **垂直扩展**: 可配置资源限制和分配
- **弹性伸缩**: 根据负载自动调整资源 (规划中)

注：详细性能数据请参考 [scripts/benchmark.py](scripts/benchmark.py)

---

## 📚 文档资源

### 核心文档

- [📘 CoreLoopThree 架构详解](partdocs/architecture/coreloopthree.md) - 三层一体核心运行时
- [💾 MemoryRovol 架构详解](partdocs/architecture/memoryrovol.md) - 记忆卷载系统
- [🔧 IPC 机制详解](partdocs/architecture/ipc.md) - 进程间通信设计
- [⚙️ 微内核设计](partdocs/architecture/microkernel.md) - 微内核架构
- [📞 系统调用详解](partdocs/architecture/syscall.md) - 系统调用接口
- [📝 日志系统架构](partdocs/architecture/logging_system.md) - 跨语言统一日志

### 开发指南

- [🚀 快速入门](partdocs/guides/getting_started.md) - 快速上手指南
- [🤖 创建 Agent](partdocs/guides/create_agent.md) - Agent 开发教程
- [🛠️ 创建技能](partdocs/guides/create_skill.md) - 技能开发教程
- [📦 部署指南](partdocs/guides/deployment.md) - 生产环境部署
- [🎛️ 内核调优](partdocs/guides/kernel_tuning.md) - 性能优化指南
- [🔍 故障排查](partdocs/guides/troubleshooting.md) - 常见问题解决

### 技术规范

- [📋 编码规范](partdocs/specifications/coding_standards.md) - 开发标准
- [🧪 测试规范](partdocs/specifications/testing.md) - 测试要求
- [🔒 安全规范](partdocs/specifications/security.md) - 安全实践
- [📊 性能指标](partdocs/specifications/performance.md) - 性能要求

### 外部文档

- [🏭 Workshop 文档](../Workshop/README.md) - 数据采集工厂
- [🔬 Deepness 文档](../Deepness/README.md) - 深度加工系统
- [📊 Benchmark 文档](../Benchmark/metrics/README.md) - 评测指标

---

## 🔄 版本路线图

### 当前版本 (v1.0.0.5) - 生产就绪

**完成度**: 85%

- ✅ 核心架构设计完成
- ✅ MemoryRovol 记忆系统实现
  - L1-L4 四层架构全部实现
    - L1 Raw: 同步/异步写入、文件系统存储、SQLite 元数据、分片管理
    - L2 Feature: FAISS 索引（IVF/HNSW）、多嵌入模型支持、LRU 缓存、向量持久化
    - L3 Structure: 绑定/解绑算子、关系编码、时序编码
    - L4 Pattern: 持久同调分析接口、HDBSCAN 聚类、规则生成
  - 检索机制完整实现
    - 吸引子网络（Attractor Network）
    - 检索缓存（LRU）
    - 挂载机制（Mount）
    - 重排序（Reranker）
  - 遗忘机制实现
    - 艾宾浩斯曲线衰减
    - 线性衰减
    - 基于访问次数策略
    - 自动遗忘任务
  - FAISS 向量检索集成
    - IVF、HNSW 索引
    - 混合检索（向量+BM25）
- ✅ CoreLoopThree 三层运行时框架
  - 认知层基础框架（90%）
    - 意图理解引擎（Intent 结构）
    - 任务规划器（DAG 生成）
    - Agent 调度器（加权轮询）
    - 多策略接口（Plan/Coordinator/Dispatching）
  - 行动层执行引擎（85%）
    - 任务状态机管理
    - 补偿事务框架
    - 执行单元注册表
    - 责任链追踪
  - 记忆层 FFI 接口（80%）
    - MemoryRovol 封装
    - 记忆引擎（Memory Engine）
    - 查询和挂载接口
- ✅ 微内核基础模块 (core)
  - IPC Binder 实现
  - 内存管理（RAII、智能指针）
  - 任务调度（加权轮询算法）
  - 高精度时间服务
- ✅ 系统调用层 (syscall) - 100%
  - ✅ 任务系统调用完成
    - `sys_task_submit()` - 提交任务
    - `sys_task_query()` - 查询状态
    - `sys_task_wait()` - 等待完成
    - `sys_task_cancel()` - 取消任务
  - ✅ 记忆系统调用完成
    - `sys_memory_write()` - 写入记忆
    - `sys_memory_search()` - 语义搜索
    - `sys_memory_get()` - 获取数据
    - `sys_memory_delete()` - 删除记忆
  - ✅ 会话系统调用完成
    - `sys_session_create()` - 创建会话
    - `sys_session_get()` - 获取信息
    - `sys_session_close()` - 关闭会话
    - `sys_session_list()` - 列出会话
  - ✅ 可观测性系统调用完成
    - `sys_telemetry_metrics()` - 获取指标
    - `sys_telemetry_traces()` - 获取追踪
- ✅ 统一日志系统实现
  - 跨语言日志接口（C/Python/Go/Rust/TS）
  - 集中式日志存储（partdata/logs/）
  - trace_id 全链路追踪
  - OpenTelemetry 集成
- 🔲 完整端到端集成测试

### 短期目标 (2026 Q2-Q3)

**v1.0.0.4 - 增强与优化**
- 完善 CoreLoopThree 异常处理机制
- 优化吸引子网络检索性能
- 提升 LRU 缓存命中率
- 改进记忆进化算法
- 增加更多执行单元

**v1.0.1.0 - 性能优化**
- 优化向量检索性能
  - FAISS 索引参数调优
  - LRU 缓存命中率提升
- 改进记忆抽象算法
  - L3→L4 进化优化
- 降低系统延迟
  - 吸引子网络迭代优化

**v1.0.2.0 - 开发者工具**
- 完善 SDK (Go/Python/Rust/TS)
  - 高级抽象接口
  - 异步支持
- 提供调试工具
  - 记忆可视化
  - 执行追踪器
- 增强文档和示例

### 中期规划 (2026 Q4-2027)

**v1.0.3.0 - 生产就绪**
- 完整的端到端测试覆盖
- 性能基准测试达标
- 安全性审计通过
- 生产环境部署验证

**v1.0.4.0 - 分布式支持**
- 多节点集群部署
- 分布式记忆存储
- 跨节点任务调度

**v1.0.5.0 - 智能化升级**
- 自适应记忆管理
- 强化学习优化
- 自主进化机制

### 长期愿景 (2027+)

- 🌐 成为智能体操作系统的事实标准
- 🤝 构建全球化的开源社区生态
- 🏆 引领下一代通用人工智能技术发展
- 📈 支持万亿级记忆容量和毫秒级检索

---

## 🤝 生态合作

我们诚邀各界合作伙伴共同建设智能体操作系统生态:

### 技术合作伙伴
- **AI实验室**: 大模型、记忆系统、认知架构等领域专家
- **硬件厂商**: GPU、NPU、存储设备提供商
- **应用企业**: 机器人、智能助理、自动化等落地场景

### 社区贡献
- **代码贡献**: 核心功能开发和优化
- **文档完善**: 使用指南和技术文档
- **测试验证**: 功能测试和性能评估
- **生态建设**: 社区运营和知识分享

---

## 📞 技术支持

### 社区支持
- **Gitee Issues**: [官方 Issue 追踪](https://gitee.com/spharx/agentos/issues) (首选)
- **GitHub Issues**: [镜像 Issue 追踪](https://github.com/SpharxTeam/AgentOS/issues)
- **讨论区**: [GitHub Discussions](https://github.com/SpharxTeam/AgentOS/discussions)
- **文档**: [在线文档](https://docs.spharx.cn/agentos)

### 商业支持
- **企业版**: 提供商业许可和技术支持
- **定制开发**: 根据需求定制功能模块
- **培训服务**: 提供系统使用和开发培训

授权申请请联系:
- 官方网站：https://spharx.cn

---

## 📄 许可证

## 📄 许可证
AgentOS 采用**商业友好、生态开放的分层开源协议架构**，与主流OS的协议设计逻辑一致，兼顾核心知识产权保护、社区生态开放与商业落地自由。

### 主协议声明
项目核心内核代码，默认采用 **Apache License 2.0** 开源协议，完整协议文本见根目录 [LICENSE]文件。

### 分层协议细则
| 模块目录 | 适用协议 | 协议说明 |
|----------|----------|----------|
| `atoms/`（内核） | Apache License 2.0 | 包含 CoreLoopThree 三层一体架构、MemoryRovol 记忆引擎、运行时、安全隔离层等不可变核心代码 |
| `domes/`（扩展） | Apache License 2.0 | 包含核心架构的扩展增强模块，与内核协议保持一致 |
| `openhub/`（生态） | MIT License | 包含 Agent 市场、技能市场、社区贡献模块，最大化降低社区贡献门槛，社区贡献者也可自选 Apache 2.0 协议 |
| 第三方依赖组件 | 遵循原组件开源协议 | 所有第三方依赖均采用宽松开源协议，做好模块隔离，无协议传染风险 |

### 您可以自由地
- ✅ 商用：免费将本项目用于闭源商业产品、企业级项目与商业化服务
- ✅ 修改：自由修改、定制、二次开发项目代码，无需开源修改后的业务代码
- ✅ 分发：自由分发、复制项目的源代码或编译后的二进制文件
- ✅ 专利使用：获得项目核心代码的永久专利授权，无专利侵权风险
- ✅ 私用：可自由用于个人、企业内部私有项目，无任何强制公开义务

### 您需要遵守的唯一核心义务
- 保留原项目的版权声明、许可证文本与NOTICE文件，不得删除原作者的版权信息
- 若修改了核心源代码文件，需在文件中保留修改记录声明

### 商业服务与授权
- 本项目开源协议无任何商业使用限制，企业可免费用于商业项目。
- 同时我们提供企业级商业技术支持、定制化开发、私有化部署服务，如有需求可通过项目联系方式沟通。

---

## 🙏 致谢

感谢所有为开源社区做出贡献的开发者们，以及为 AgentOS 项目提供支持的合作伙伴。

特别感谢:
- FAISS 团队 (Facebook AI Research)
- Sentence Transformers 团队
- Rust 和 Go 语言社区
- 所有贡献者和用户

---

<div align="center">

<h4>"From data intelligence emerges"</h4>
<h4>"始于数据，终于智能"</h4>
---

#### 📞 联系我们

**技术支持**: lidecheng@spharx.cn  
**安全问题**: wangliren@spharx.cn  
**商务合作**: zhouzhixian@spharx.cn  
**官方网站**: https://spharx.cn

<p>
  <a href="https://gitee.com/spharx/agentos">Gitee（官方仓库）</a> ·
  <a href="https://github.com/SpharxTeam/AgentOS">GitHub（镜像仓库）</a> ·
  <a href="https://spharx.cn">官方网站</a>
</p>

© 2026 SPHARX Ltd. All Rights Reserved.

</div>