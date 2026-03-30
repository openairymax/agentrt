# AgentOS

<div align="center">

[![Version](https://img.shields.io/badge/version-1.0.0.6-blue.svg)](https://gitee.com/spharx/agentos)
[![License](https://img.shields.io/badge/license-Apache--2.0-green.svg)](https://gitee.com/spharx/agentos/blob/main/LICENSE)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/SpharxTeam/AgentOS/actions)
[![Documentation Status](https://img.shields.io/badge/docs-Doc%20V1.7-blue)](manuals/README.md)
[![Docker](https://img.shields.io/badge/docker-supported-blue.svg?logo=docker&logoColor=white)](https://www.docker.com/)
[![C/C++](https://img.shields.io/badge/C%2FC%2B%2B-11%2F17-blue.svg?logo=c%2B%2B&logoColor=white)](https://isocpp.org/)
[![Python](https://img.shields.io/badge/python-3.10+-blue.svg?logo=python&logoColor=white)](https://www.python.org/)
[![Mirror](https://img.shields.io/badge/mirror-GitHub-lightgrey.svg)](https://github.com/SpharxTeam/AgentOS)

---

## 🎯 智能体超级操作系统

*"From data intelligence emerges 始于数据，终于智能。"*

---

**🌐 Language / 语言**:

[简体中文 (当前)](README.md) | [English](manuals/readme/en/README.md) | [Français](manuals/readme/fr/README.md) | [Deutsch](manuals/readme/de/README.md)

</div>

---

## 📜 官方技术白皮书

项目完整系统阐述请参阅官方技术白皮书。

| 版本 | 文档 |
|------|------|
| 🇨🇳 中文 V1.0 | [AgentOS 技术白皮书](manuals/white_paper/zh/AgentOS_技术白皮书_V1.0.pdf) |
| 🇬🇧 English V1.0 | [AgentOS Technical White Paper](manuals/white_paper/en/AgentOS_Technical_White_Paper_V1.0.pdf) |

---

## 🚀 项目简介

AgentOS 是 SpharxWorks 的核心产品之一，是一个**生产就绪的智能体操作系统**。它不仅是一个智能体框架，而是为驱动智能体提供完整的操作系统级支持，包括微内核架构、安全隔离、记忆系统和认知运行时。

> "Intelligence emergence, and nothing less, is the ultimate sublimation of AI."

### 核心优势

- **架构简洁清晰**：基于五维正交原则体系，模块化设计，接口稳定
- **Token 效率提升**：相比传统方案平均节省约 200% token 使用量
- **微内核架构**：内核仅提供 IPC、内存、任务、时间四大原子机制
- **安全可靠**：四重安全防护（沙箱隔离、权限裁决、输入净化、审计追踪）
- **认知科学融合**：双系统认知理论完整映射到计算机系统架构

### 理论基础

AgentOS 的设计融合多种理论成果，形成完整的科学支撑体系。

| 理论 | 来源 | 核心思想 | 在 AgentOS 中的体现 |
|------|------|----------|---------------------|
| **工程控制论** | Engineering Cybernetics | 反馈调节、动态平衡 | 三层闭环控制系统，实时感知决策执行分离，确保每个环节可控 |
| **系统工程论** | On Systems Engineering | 层次分解、综合集成、整体优化 | 五维正交原则体系，七层架构分层，CoreLoopThree 实现 |
| **双系统理论** | Thinking, Fast and Slow | System 1 快思考，System 2 慢思考 | 认知层 (System 2) 深谋远虑 vs 行动层 (System 1) 快速执行 |
| **微内核理论** | Liedtke 微内核原理 + seL4 形式化验证 | 极小化内核、模块化、最小特权 | corekern 仅 4 个原子机制，所有服务运行在用户态 |
| **设计美学** | 乔布斯设计哲学 | 简约至上、极致细节、人文关怀、完美主义 | 工程哲学提升到架构原则高度，强调开发者体验 |

**理论创新点**:
- **控制论与系统论融合**：将控制论的动态调节（反馈机制）与系统论的结构设计（层次分解）相结合
- **认知科学与计算机科学的交叉**：首次将双系统理论完整映射到计算机系统架构
- **微内核范式转移**：从传统 OS 的微内核扩展为"操作系统即内核"的新范式
- **工程哲学的引入**：将设计美学提升到架构原则高度，强调人文关怀和美学追求

详见 [架构设计原则 Doc V1.7](manuals/ARCHITECTURAL_PRINCIPLES.md)

---

## 💎 核心价值

| 价值维度 | 说明 | 对应原则 |
|---------|------|---------|
| **微内核设计** | 内核仅提供 IPC、内存、任务、时间四个原子机制，所有服务运行在用户态 | K-1 微内核化原则 |
| **三层认知循环** | 认知→规划→行动，知行分离，不断迭代 | S-3 三层控制原则 |
| **四层记忆系统** | L1 原始层→L2 特征层→L3 结构层→L4 模式层，涌现智慧 | C-3 四层抽象原则 |
| **安全穹顶** | 沙箱隔离、权限裁决、输入净化、审计追踪，全面防护 | E-1 安全第一原则 |
| **可插拔性** | 规划/协同/学习/执行/记忆算法都可运行时替换 | K-4 可插拔原则 |
| **实时响应** | 事件驱动优先级调度，可中断低优先级任务，保障关键业务 | S-1 实时优先原则 |
| **接口约束** | 所有模块交互通过 Doxygen 约束，线程安全，所有权明确 | K-2 接口约束原则 |
| **多语言 SDK** | Go、Python、Rust、TypeScript 原生支持，FFI 接口高效安全 | A-3 人文关怀原则 |

---

## 🏗️ 系统架构

```
┌────────────────────────────────────────────────────────────────────────────┐
│                   AgentOS 总体架构                                          │
└────────────────────────────────────────────────────────────────────────────┘
│                                                                            │
│ ┌──────────────────────────────────────────────────────────────────────┐ │
│ │             应用层 (openlab)                                          │ │
│ │ docgen | ecommerce | research | videoedit | ...                      │ │
│ └──────────────────────────────────────────────────────────────────────┘ │
│                          ↓↑                                              │
│ ┌──────────────────────────────────────────────────────────────────────┐ │
│ │            服务层 (daemon)                                            │ │
│ │ llm_d | market_d | monit_d | sched_d | tool_d                        │ │
│ └──────────────────────────────────────────────────────────────────────┘ │
│                          ↓↑                                              │
│ ┌──────────────────────────────────────────────────────────────────────┐ │
│ │             内核层 (atoms)                                            │ │
│ │ ┌─────────────────┐  ┌─────────────────┐  ┌──────────────────┐      │ │
│ │ │  corekern       │  │ coreloopthree   │  │ memoryrovol      │      │ │
│ │ │ 微内核基础       │  │ 三层认知运行时   │  │ 四层记忆系统     │      │ │
│ │ │ IPC,Mem,Task   │  │ 认知/行动/记忆   │  │ L1/L2/L3/L4      │      │ │
│ │ └─────────────────┘  └─────────────────┘  └──────────────────┘      │ │
│ │ ┌─────────────────┐  ┌─────────────────┐  ┌──────────────────┐      │ │
│ │ │  syscall        │  │    cupolas        │  │  utils           │      │ │
│ │ │ 系统调用接口     │  │  安全穹顶        │  │ 公共工具库        │      │ │
│ │ └─────────────────┘  └─────────────────┘  └──────────────────┘      │ │
│ └──────────────────────────────────────────────────────────────────────┘ │
│                          ↓↑                                              │
│ ┌──────────────────────────────────────────────────────────────────────┐ │
│ │            SDK 层 (toolkit)                                             │ │
│ │ Go | Python | Rust | TypeScript                                      │ │
│ └──────────────────────────────────────────────────────────────────────┘ │
│                                                                            │
└────────────────────────────────────────────────────────────────────────────┘
```

### 架构设计原则（五维正交体系）

详见 [架构设计原则 Doc V1.7](manuals/ARCHITECTURAL_PRINCIPLES.md)

```
维度一：系统观 (S-1~S-4) → 工程控制论 & 系统工程
  反馈闭环 → 层次分解 → 总体设计部 → 涌现性管理
  
维度二：内核观 (K-1~K-4) → 微内核理论
  内核极简 → 接口契约 → 服务隔离 → 可插拔策略
  
维度三：认知观 (C-1~C-4) → 双系统认知理论
  双系统协同 → 增量演化 → 记忆卷载 → 遗忘机制
  
维度四：工程观 (E-1~E-8) → 工程实践准则
  安全内生 → 可观测性 → 资源确定性 → 跨平台一致
  命名语义化 → 错误可追溯 → 文档即代码 → 可测试性
  
维度五：设计美学 (A-1~A-4) → 工程哲学
  简约至上 → 极致细节 → 人文关怀 → 完美主义
```

---

## 📁 项目结构

```
AgentOS/
├── atoms/                      # 内核层（微内核架构）v1.0.0.6
│   ├── corekern/               # 微内核基础，IPC/Mem/Task/Time
│   ├── coreloopthree/          # 三层认知运行时
│   ├── memoryrovol/            # 四层记忆系统
│   ├── syscall/                # 系统调用接口
│   └── utils/                  # 公共工具
│
├── atomsmini/                  # 轻量内核（嵌入式场景）
│   └── corekernlite/           # 精简微内核
│
├── cupolas/                    # 安全穹顶 v1.0.0.6
│   ├── workbench/              # 虚拟工位（进程/容器隔离）
│   ├── permission/             # 权限裁决（RBAC/YAML）
│   ├── sanitizer/              # 输入净化（风险评级）
│   └── audit/                  # 审计追踪（异步日志）
│
├── daemon/                     # 用户态服务（Daemon）v1.0.0.6
│   ├── llm_d/                  # LLM 服务（OpenAI/DeepSeek）
│   ├── market_d/               # 市场服务（Agent/技能注册）
│   ├── monit_d/                # 监控服务（OpenTelemetry）
│   ├── sched_d/                # 调度服务（多策略）
│   └── tool_d/                 # 工具服务（工具执行）
│
├── commons/                    # 基础支撑层
│   ├── platform/               # 平台抽象层
│   └── utils/                  # 通用工具库
│
├── gateway/                    # 网关层（HTTP/WebSocket/Stdio）v1.1.0
├── heapstore/                  # 运行时数据存储（内核/日志/服务）
├── manager/                    # 配置文件（Agent/环境/安全等）
├── openlab/                    # 开放生态（应用/社区贡献）
├── toolkit/                    # 多语言 SDK（Go/Python/Rust/TS）v3.0.0
├── manuals/                    # 文档体系（架构/指南/白皮书）Doc V1.7
├── scripts/                    # 脚本（构建/部署/运维）
├── tests/                      # 测试（单元/集成/端到端/安全）
└── reports/                    # 报告（质量/性能/评估）
```

**关键模块说明**:
- **三大核心创新**: CoreLoopThree（三层认知循环）、MemoryRovol（四层记忆系统）、cupolas（安全穹顶）
- **微内核设计**: 约 9,000 行代码，提供 IPC Binder、内存管理（RAII）、任务调度（加权轮询）、高精度时间四大原子机制
- **内核隔离**: 所有服务运行在用户态，通过 syscall 与内核通信，严格接口约束
- **系统调用**: 用户态与内核通信的唯一通道，严格的接口约束（Doxygen 注释）
- **统一日志**: 跨语言日志接口（C/Python/Go/Rust/TS），trace_id 全链路追踪，OpenTelemetry 集成

---

## 🛡️ cupolas 安全穹顶

`cupolas/` 是 AgentOS 的安全防护系统，为每个智能体提供独立沙箱环境，确保任何智能体都无法突破边界。

### 四大组件

| 组件 | 路径 | 职责 | 核心接口 |
|------|------|------|----------|
| **虚拟工位** | `workbench/` | 进程级隔离（namespaces+cgroups），类似 runc 容器化，资源配额，网络隔离 | `cupolas_workbench_create/exec/destroy` |
| **权限裁决** | `permission/` | YAML 规则引擎，RBAC 模型，动态匹配，缓存加速，优先级仲裁 | `cupolas_permission_check/reload` |
| **输入净化** | `sanitizer/` | 多级过滤器，风险等级分级（0-3），自动修正或删除 | `cupolas_sanitize` |
| **审计追踪** | `audit/` | 异步双缓冲写入，JSON 格式，日志轮转，结构化查询，全链路追踪 | `cupolas_audit_record/query` |

### 设计原则

- **最小权限**: 默认拒绝一切未明确授权的操作
- **故障安全**: 组件失效时不影响系统运行
- **优先级仲裁**: 安全优先级最高
- **可追溯**: 所有操作留痕并全链路追踪

详见 [cupolas 架构文档](cupolas/README.md)

---

## 🔄 CoreLoopThree 三层认知循环

CoreLoopThree 是 AgentOS 的核心执行架构，将智能体运行时分为三个相互协同的层级，实现**"认知与执行分离、感知与行动解耦"**。这种设计来源于控制理论和认知神经科学的双重启发。

**理论依据**:
- **双系统认知理论**: 认知层=System 2（慢思考，深谋远虑）vs 行动层 (System 1，快思考，模式执行)
- **ACT-R/SOAR 认知架构**: 目标栈、产生式规则、工作记忆、长期记忆
- **海马体 - 新皮层理论**: 快速编码（海马体 L1）→ 缓慢巩固（新皮层 L2-L4）

### 架构总览

```
┌────────────────────────────────────────────────────────────────────────────┐
│                   认知层 (Cognition)                                        │
│ 意图识别 → 任务规划 (DAG) → Agent 调度 → 模型协同                            │
│    ↓↑                                                                      │
│    └─────────────────────────────────────────────────────────────────────┘
│                           ↓↑
┌────────────────────────────────────────────────────────────────────────────┐
│                   行动层 (Execution)                                        │
│ 状态机管理 → 补偿事务 → 可追溯执行 → 元动作注册                              │
│    ↓↑                                                                      │
│    └─────────────────────────────────────────────────────────────────────┘
│                           ↓↑
┌────────────────────────────────────────────────────────────────────────────┐
│                   记忆层 (Memory)                                           │
│ MemoryRovol FFI → 多层记忆管理 → LRU 缓存 → 遗忘曲线                         │
└────────────────────────────────────────────────────────────────────────────┘
```

### 层级详解

#### 认知层（Cognition Layer）- System 2 慢思考

**核心职责**: 理解用户意图，制定战略规划，分发任务

| 功能 | 说明 | 可插拔算法 | 理论映射 |
|------|------|-----------|---------|
| 意图识别 | 解析用户输入，识别核心目标，Intent 结构化 | - | 图灵测试 |
| 任务规划 | DAG 有向图分解，动态生成子任务，依赖关系处理 | 固定式/自适应式/ML | 问题空间搜索 (SOAR) |
| Agent 调度 | 多目标优化选择最优 Agent 组合，负载均衡 | 贪心/遗传/强化学习/ML | 效用最大化 |
| 模型协同 | 多模型结果交叉验证，少数服从多数，加权融合 | 双模型/多模型投票/加权融合 | 群体智能 |
| 协同 | 多 Agent 协作分工，任务分解和合并 | - | 分布式人工智能 |

**关键特性**:
- **认知与执行分离**: 认知层只负责规划和调度，具体执行交给行动层，保证专注性和单一职责
- **动态规划**: 支持 DAG 动态生成，根据执行反馈实时调整计划
- **多模型协同**: 通过"思考 - 行动 - 反思"微循环和多模型交叉验证提升决策质量
- **完全可追溯**: 所有决策记录可实现全链路复盘和行为可解释

#### 行动层（Execution Layer）- System 1 快思考

**核心职责**: 模式匹配、自动化执行、异常恢复

| 功能 | 说明 | 状态 | 理论映射 |
|------|------|------|---------|
| 状态机管理 | 基于状态机的任务生命周期管理 | Pending、Running、Succeeded/Failed/Cancelled/Retrying | 状态自动化 |
| 补偿事务 | 失败自动回滚，Saga 模式 | 正向操作 + 逆向补偿 | 逆操作理论 |
| 可追溯执行 | 全链路执行记录，TraceID 关联 | OpenTelemetry 集成 | 分布式追踪 |
| 元动作注册 | 原子执行单元，动态注册，热插拔 | API/数据库操作/文件操作/Shell/网络请求 | 插件架构 |

**关键特性**:
- **自动化模式执行**: 对重复性任务自动匹配历史成功模式，减少 90% 以上的不必要思考
- **异常自动恢复**: 失败时自动触发回滚机制，保证数据一致性
- **执行单元热插拔**: 支持在运行时动态注册新的执行器而无需重启系统
- **人机协同回路**: 支持人工介入确认，暂停等待人工审批，实现 Human-in-the-loop

### 效用函数计算

```
Score(agent) = w₁ * (1/cost) + w₂ * success_rate + w₃ * trust_score
```

其中:
- `cost`: Agent 使用成本（token 消耗或时间开销）
- `success_rate`: 历史成功率（0-1 之间）
- `trust_score`: 信任评分（基于用户评价和推荐记录）
- `w₁, w₂, w₃`: 权重系数，可动态调整

详见 [CoreLoopThree 架构文档](manuals/architecture/coreloopthree.md)

---

## 🧠 MemoryRovol 四层记忆系统

MemoryRovol 是 AgentOS 的核心记忆系统，实现从原始数据到高级模式的全栈记忆处理能力。其灵感来源于人脑的海马体和新皮层理论：短期记忆像海马体一样快速编码但易失，长期记忆则如新皮层一般缓慢巩固但持久。

### 四层架构

```
┌────────────────────────────────────────────────────────────────────────────┐
│             L4 Pattern Layer (模式层)                                       │
│  持久同调分析 (Ripser) + HDBSCAN 聚类 → 发现行为模式                         │
│  高频重复的行为被提炼为模式，指导未来决策                                     │
└────────────────────────────────────────────────────────────────────────────┘
                          ↓记忆巩固
┌────────────────────────────────────────────────────────────────────────────┐
│            L3 Structure Layer (结构层)                                      │
│  知识图谱 + 关系抽取 + 时空索引 + 因果推理                                   │
│  将碎片化的记忆组织成结构化的知识网络                                         │
└────────────────────────────────────────────────────────────────────────────┘
                          ↓记忆提取
┌────────────────────────────────────────────────────────────────────────────┐
│            L2 Feature Layer (特征层)                                        │
│  嵌入模型 + FAISS 向量检索 + 混合检索（稀疏 +BM25）                          │
│  从原始数据中提取语义特征，支持相似度检索                                       │
└────────────────────────────────────────────────────────────────────────────┘
                          ↓记忆压缩
┌────────────────────────────────────────────────────────────────────────────┐
│              L1 Raw Layer (原始层)                                          │
│  文件系统存储 + 分片压缩 + SQLite 元数据索引 + 版本控制                        │
│  直接记录感官输入的原始数据，支持追加不可篡改                                  │
└────────────────────────────────────────────────────────────────────────────┘
```

### 神经科学映射

| 层级 | 大脑区域 | 功能 | 实现技术 |
|------|---------|------|---------|
| L1 原始层 | 海马体 CA3 区 | 快速场景编码，短期记忆 | 文件系统 + SQLite 索引 |
| L2 特征层 | 内嗅皮层 | 特征提取，语义检索 | FAISS + Embedding 模型 |
| L3 结构层 | 海马体 - 新皮层通路 | 关系建模，时空索引 | 知识图谱 + 图数据库 |
| L4 模式层 | 前额叶皮层 | 高级抽象和模式识别，元认知 | 持久同调 + HDBSCAN 聚类 |

### 遗忘曲线

采用艾宾浩斯遗忘曲线模型 R = e^{-λt}，其中 λ 为遗忘速率常数，受访问频率和情感权重影响：

**数学模型**:
- **指数衰减**: R(t) = e^{-λt}（默认）
- **线性衰减**: R(t) = 1 - αt
- **访问频率修正**: R(t) = e^{-λt / (1 + β · access_count)}
- **情感权重修正**: λ' = λ · e^{-γ · emotional_weight}

**记忆管理策略**:
- **归档**: 低权重记忆被归档到低温存储（L1 Cold Archive）
- **激活**: 当归档记忆被重新查询时重新激活（基于多模态 Pattern Completion）
- **巩固**: 频繁使用或高价值记忆会被提升到 L4 模式层并长期保存（类似海马体到新皮层的记忆巩固）
- **睡眠回放**: 在空闲时段进行记忆回放强化重要连接（借鉴神经科学睡眠巩固理论）

### 多模态检索

基于现代 Hopfield 网络和多模态融合技术实现**模式补全**（Pattern Completion）。

**能量函数**:
```
E(z) = -Σᵢ (mᵢ·z)² + λ·||z||²
```

**动力学方程**:
```
z(t+1) = σ(Σᵢ mᵢ(mᵢ·z(t)))
```

即使输入不完整的线索也能回忆起完整记忆。

**多路召回 Reranker 融合公式**:
```python
# 多路召回的最终得分
final_scores = α * vector_similarity + β * bm25_score + γ * recency_boost + δ * emotional_weight
```

其中:
- `vector_similarity`: 向量相似度（FAISS 计算）
- `bm25_score`: BM25 文本相关性分数
- `recency_boost`: 近期效应增强因子（随时间衰减）
- `emotional_weight`: 情感权重（基于情绪价值评估）

### 性能指标

| 指标 | 数值 |
|------|------|
| L1 写入吞吐 | 10,000+ 条/秒 |
| L2 检索延迟 | < 10ms (k=10) |
| 混合检索召回率 | < 50ms (top-100) |
| L2→L3 迁移速度 | 100 条/秒 |
| L4 模式挖掘速度 | 10 万条/分钟 |

详见 [MemoryRovol 架构文档](manuals/architecture/memoryrovol.md)

---

## 🎨 设计美学在工程中的体现

AgentOS 不仅是一个工程产品，更是一个精心雕琢的艺术品。我们将**设计美学**提升到架构原则的高度，强调人文关怀和美学追求。

### 四大美学原则

| 原则 | 内涵 | 在 AgentOS 中的体现 |
|------|------|---------------------|
| **简约至上 (A-1)** | 少即是多，去除一切不必要的复杂度 | • 微内核仅提供 4 个原子机制<br>• 系统调用接口精简到仅 50 个核心函数<br>• 内核代码严格控制在可理解范围内（corekern ~9K LOC） |
| **细节极致 (A-2)** | 魔鬼在细节中，追求完美 | • 所有公共 API 都有 Doxygen 详细注释，包含参数说明、返回值、线程安全性、示例<br>• 错误码统一分类 ERROR/WARNING/INFO<br>• 日志格式标准化（时间戳 +trace_id+ 结构化数据） |
| **人文关怀 (A-3)** | 以开发者为本，强化用户体验 | • 全链路追踪让行为可解释<br>• Human-in-the-loop 支持人工干预关键决策<br>• 多语言 SDK 降低开发门槛<br>• 详尽的文档体系（架构/指南/教程） |
| **完美主义 (A-4)** | 持续改进，追求卓越 | • 核心模块 100% 单元测试覆盖<br>• 性能指标对标业界领先方案<br>• 内存管理和并发架构高效稳定<br>• 变更日志细致遵循 Keep a Changelog 规范 |

### 工程美学示例

**细节极致 - RAII 内存管理**:
```cpp
// 智能指针自动释放，无需手动 free
core_mem_ptr_t ptr = core_mem_alloc(size);
if (!ptr) return AGENTOS_ERROR_NO_MEMORY;
// 离开作用域时自动释放，无泄漏风险
```

**简约至上 - Doxygen 注释**:
```cpp
/**
 * @brief 写入原始记忆
 * @param data [in] 数据缓冲区，不能为 NULL
 * @param len [in] 数据长度，必须>0
 * @param metadata [in,opt] JSON 元数据，可为 NULL
 * @param out_record_id [out] 输出记录 ID，调用者负责释放
 * @return agentos_error_t
 * @threadsafe 否，内部使用全局锁
 * @see agentos_sys_memory_search(), agentos_sys_memory_delete()
 */
AGENTOS_API agentos_error_t agentos_sys_memory_write(...);
```

**人文关怀 - 统一错误码**:
```cpp
switch (error) {
    case AGENTOS_SUCCESS:
        // 成功路径，直接返回
        break;
    case AGENTOS_ERROR_INVALID_PARAM:
        // 参数错误，记录 WARNING 级别日志
        AGENTOS_LOG_WARNING("Invalid parameter: %s", param_name);
        break;
    case AGENTOS_ERROR_NO_MEMORY:
        // 内存错误，尝试恢复并记录 ERROR
        AGENTOS_LOG_ERROR("Out of memory, attempting recovery");
        attempt_memory_recovery();
        break;
}
```

详见 [架构设计原则 Doc V1.7](manuals/ARCHITECTURAL_PRINCIPLES.md)

---

## 🔌 Syscall 系统调用接口

系统调用是用户态程序与内核之间的唯一通信通道。所有 Daemon 服务（`daemon/`）都通过 `syscalls.h` 与内核交互，禁止直接调用内核内部函数。

### 核心接口

| 类别 | 接口 | 说明 |
|------|------|------|
| **任务管理** | `agentos_sys_task_submit/query/wait/cancel` | 提交和管理任务生命周期 |
| **记忆管理** | `agentos_sys_memory_write/search/get/delete` | 记忆 CRUD 操作 |
| **会话管理** | `agentos_sys_session_create/get/close/list` | 多轮对话上下文管理 |
| **可观测性** | `agentos_sys_telemetry_metrics/traces` | 指标采集和链路追踪 |
| **Agent 管理** | `agentos_sys_agent_register/invoke/terminate` | Agent 注册和调用 |

### 接口约束示例

```cpp
/**
 * @brief 写入原始记忆
 * @param data 数据缓冲区
 * @param len 数据长度
 * @param metadata JSON 元数据，可为 NULL
 * @param out_record_id 输出记录 ID，调用者负责释放
 * @return agentos_error_t
 * @threadsafe 否
 * @see agentos_sys_memory_search(), agentos_sys_memory_delete()
 */
AGENTOS_API agentos_error_t agentos_sys_memory_write(
    const void* data, size_t len,
    const char* metadata, char** out_record_id);
```

详见 [系统调用规范](manuals/architecture/syscall.md)

---

## ⚙️ 编译指南

### 环境要求

| 类别 | 要求 |
|------|------|
| **操作系统** | Linux (Ubuntu 22.04+), macOS 13+, Windows 11 (WSL2) |
| **编译器** | GCC 11+ / Clang 14+ / MSVC 2022+ |
| **构建工具** | CMake 3.20+, Ninja 或 Make |
| **系统依赖** | OpenSSL, libevent, pthread, SQLite3, libcurl, cJSON |
| **可选依赖** | FAISS >= 1.7.0（IVF/HNSW 索引） |
| **可选依赖** | Ripser (持久同调), HDBSCAN (聚类分析), libyaml (配置解析), libcjson (JSON 处理) |

### 快速开始

```bash
# 克隆项目
git clone https://gitee.com/spharx/agentos.git
cd agentos

# 初始化配置
cp .env.example .env
python scripts/init_config.py

# 构建内核
mkdir build && cd build
cmake ../atoms -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build . --parallel $(nproc)

# 运行测试
ctest --output-on-failure
```

### CMake 配置选项

| 选项 | 说明 | 默认值 |
|------|------|--------|
| `CMAKE_BUILD_TYPE` | Debug/Release/RelWithDebInfo | `Release` |
| `BUILD_TESTS` | 构建单元测试 | `OFF` |
| `ENABLE_TRACING` | 启用 OpenTelemetry 追踪 | `OFF` |
| `ENABLE_ASAN` | 启用 AddressSanitizer | `OFF` |
| `ENABLE_LOGGING` | 启用统一日志系统 | `ON` |

详见 [BUILD.md](atoms/BUILD.md)

---

## 📊 性能基准

基于标准测试环境 (Intel i7-12700K, 32GB RAM, NVMe SSD):

### 核心性能

| 指标 | 数值 | 测试条件 |
|------|------|---------|
| 记忆写入吞吐 | 10,000+ 条/秒 | L1 层，异步批量写入 |
| 记忆检索延迟 | < 10ms | FAISS IVF1024,PQ64, k=10 |
| 混合检索召回 | < 50ms | 向量+BM25, top-100 结果 |
| IPC 吞吐量 | 1024 | Binder IPC 消息/秒 |
| 权限检查映射 | < 1ms | 带缓存查询 |
| Agent 调用映射 | < 5ms | 带缓存查询 |
| L2→L3 迁移速度 | 100 条/秒 | 批量后台迁移 |
| L4 模式挖掘速度 | 10 万条/分钟 | 持久同调分析 |

### 资源占用

| 场景 | CPU 占用 | 内存占用 | 磁盘 IO |
|------|---------|---------|---------|
| 空闲状态 | < 5% | 200MB | < 1MB/s |
| 中等负载 | 30-50% | 1-2GB | 10-50MB/s |
| 高负载 | 80-100% | 4-8GB | 100-500MB/s |

详见 [benchmark.py](scripts/ops/benchmark.py) | [性能指标文档](manuals/specifications/README.md)

---

## 📚 文档索引

### 架构文档

| 文档 | 说明 |
|------|------|
| [架构设计原则 Doc V1.7](manuals/ARCHITECTURAL_PRINCIPLES.md) | **五维正交原则体系**: 系统观/内核观/认知观/工程观/设计美学 |
| [CoreLoopThree 架构](manuals/architecture/coreloopthree.md) | 三层认知循环：认知→规划→行动 |
| [MemoryRovol 架构](manuals/architecture/memoryrovol.md) | 四层记忆系统：L1→L2→L3→L4 |
| [微内核架构](manuals/architecture/microkernel.md) | corekern 原子内核：IPC/Mem/Task/Time |
| [系统调用规范](manuals/architecture/syscall.md) | syscall 接口约束 |
| [IPC 通信机制](manuals/architecture/ipc.md) | Binder 跨进程通信 |
| [统一日志系统](manuals/architecture/logging_system.md) | trace_id 全链路追踪 |
| [cupolas 安全穹顶](cupolas/README.md) | 安全四大组件详解 |

### 哲学文档

| 文档 | 说明 |
|------|------|
| [认知理论](manuals/philosophy/Cognition_Theory.md) | 双系统认知模型在 AgentOS 中的应用 |
| [记忆理论](manuals/philosophy/Memory_Theory.md) | 四层记忆系统的神经科学和心理学基础 |
| [设计原则](manuals/philosophy/Design_Principles.md) | "少即是多"的架构哲学 |

### 使用指南

| 文档 | 说明 |
|------|------|
| [快速开始](manuals/guides/getting_started.md) | 环境搭建和第一个 Agent |
| [创建 Agent](manuals/guides/create_agent.md) | Agent 开发教程 |
| [创建技能](manuals/guides/create_skill.md) | 技能开发指南 |
| [部署指南](manuals/guides/deployment.md) | 生产环境部署 |
| [内核调优](manuals/guides/kernel_tuning.md) | 性能优化和参数配置 |
| [故障排查](manuals/guides/troubleshooting.md) | 常见问题和解决方案 |
| [模块功能](manuals/guides/module_features.md) | 各模块功能详解 |

---

## 🚀 版本路线图

### 当前版本 (v1.0.0.6) - 生产就绪 (Production Ready) ✅

**发布日期**: 2026-03-25  
**核心模块完成度**: 100%

以下是当前的代码实现情况和各模块的详细完成度：

| 模块 | 完成度 | 状态 | 关键功能 | 代码规模 |
|------|--------|------|---------|---------|
| **corekern 微内核** | 100% | ✅ | IPC Binder、内存管理（RAII）、任务调度（加权轮询）、高精度时间 | ~9,000 LOC |
| **coreloopthree 三层循环** | 100% | ✅ | 认知层（意图识别/任务规划/Agent 调度）、行动层（状态机/补偿事务）、记忆层 FFI | ~15,000 LOC |
| **memoryrovol 四层记忆** | 100% | ✅ | L1-L4 全栈实现、FAISS 集成（IVF/HNSW）、遗忘曲线、多模态检索 | ~20,000 LOC |
| **syscall 系统调用** | 100% | ✅ | 任务/记忆/会话/可观测性/Agent 全接口、Doxygen 约束 | ~3,000 LOC |
| **cupolas 安全穹顶** | 100% | ✅ | 虚拟工位（进程/容器隔离）、权限裁决（RBAC/YAML）、输入净化、审计追踪 | ~12,000 LOC |
| **daemon 服务层** | 100% | ✅ | llm_d/market_d/monit_d/sched_d/tool_d 全部实现 | ~25,000 LOC |
| **toolkit 多语言 SDK** | 90% | 🚧 | Go/Python/Rust/TypeScript，异步支持，错误处理 | ~8,000 LOC |
| **统一日志系统** | 100% | ✅ | 跨语言接口、trace_id 追踪、OpenTelemetry 集成 | ~5,000 LOC |

**关键里程碑**:
- ✅ **基础架构搭建完成**（v1.0.0.0-v1.0.0.3）：微内核 + 三层循环 + 四层记忆理论设计
- ✅ **MemoryRovol 完整系统实现**（v1.0.0.4-v1.0.0.5）：L1-L4 全栈功能
- ✅ **CoreLoopThree 完整运行时实现**（v1.0.0.5-v1.0.0.6）：认知层 + 行动层 + 记忆层
- ✅ **系统调用 100% 覆盖**（v1.0.0.6）：所有公共 API 都有 Doxygen 约束
- ✅ **统一日志系统完成**（v1.0.0.6）：跨语言追踪 + OpenTelemetry 集成
- ✅ **cupolas 安全穹顶完成**（v1.0.0.6）：四大组件全部实现
- ✅ **文档体系完善**（v1.0.0.6）：架构原则 Doc V1.7、CoreLoopThree、MemoryRovol

**v1.0.0.6 新增内容**（相比 v1.0.0.5）:
- ✅ 新增设计美学维度（A-1~A-4），将工程哲学提升到架构原则高度
- ✅ 新增全链路追踪机制，实现行为可解释和可追溯
- ✅ 新增 Human-in-the-loop 支持，支持人工干预和确认
- ✅ 新增睡眠回放机制，在空闲时段进行记忆回放强化重要连接
- ✅ 优化情感权重计算，基于情绪价值评估调整记忆强度
- ✅ 优化多路召回融合，引入更多权重因子提升召回质量

### 短期目标 (2026 Q2-Q3)

- **v1.0.1.0**: 性能优化专项
  - FAISS 参数调优（IVF 数量优化，PQ 码长优化）
  - LRU 缓存命中率提升至 95%+
  - 混合检索延迟优化至 <30ms
  - 记忆写入吞吐提升至 20,000 条/秒
  
- **v1.0.2.0**: 可视化工具集
  - 记忆可视化界面（图形化查看 L1-L4 记忆状态）
  - 执行追踪视图实时查看任务执行流程
  - 性能分析工具（CPU/Memory/IO 等瓶颈定位）
  - IDE 插件（VSCode/IntelliJ 代码补全和调试支持）

### 中期规划 (2026 Q4-2027)

- **v1.0.3.0**: 功能增强（边缘计算支持、安全加固、性能基准）
- **v1.0.4.0**: 分布式支持（节点集群、分布式记忆、共识算法）
- **v1.0.5.0**: 智能化增强（自适应记忆、强化学习优化、元学习）

### 长期愿景 (2027+)

- 成为全球主流 AgentOS 实现标准
- 构建全球化开源社区
- 支持量子计算和脑机接口级别应用

---

## 🌟 生态合作

### 招募合作伙伴

- **AI 实验室**: 大模型、认知架构、记忆研究专家
- **硬件厂商**: GPU、NPU、存储设备提供商
- **应用企业**: 机器人、自动驾驶、智能家居、教育

### 参与贡献

AgentOS 欢迎各种形式的贡献!

- **代码贡献**: 新功能开发、性能优化、Bug 修复（符合 K-4 可插拔原则）
- **测试贡献**: 新的规划/协同/学习/执行/记忆测试，通过 `tests/contract/` 契约验证
- **文档贡献**: 使用指南和教程文档，中英文翻译
- **性能测试**: 性能测试、性能基准对比、安全渗透测试
- **生态贡献**: Agent 应用开发、技能市场交易、开发者工具

详见 [CONTRIBUTING.md](CONTRIBUTING.md)

---

## 📜 许可证

AgentOS 采用**商业友好、生态友好的分层开源协议**。

| 模块目录 | 使用协议 | 说明 |
|----------|----------|------|
| `atoms/`（内核） | Apache License 2.0 | 核心代码，可商用修改 |
| `cupolas/`（安全） | Apache License 2.0 | 安全组件，与内核协议一致 |
| `openlab/`（生态） | MIT License | 应用生态模块，更加宽松 |
| 其他模块 | 遵循原协议 | 模块依赖，协议传染 |

### 您自由享有

- ✅ **商用**: 允许用于商业目的
- ✅ **修改**: 允许修改源码和二次开发
- ✅ **分发**: 允许分发源码和二进制
- ✅ **专利使用**: 核心代码包含专利授权

### 唯一义务

- 保留原始的版权声明和许可声明
- 如果修改后开源发布，需在文件中注明修改记录

详见 [LICENSE](LICENSE) | [NOTICE](NOTICE)

---

## 🙏 致谢

感谢所有为开源社区做出贡献的开发者，以及为 AgentOS 项目提供支持的合作伙伴。

特别感谢:
- FAISS 团队 (Facebook AI Research)
- Sentence Transformers 团队
- Rust 和 Go 语言社区
- 所有贡献者和用户

详见 [ACKNOWLEDGMENTS.md](ACKNOWLEDGMENTS.md) | [AUTHORS.md](AUTHORS.md)

---

<div align="center">

### "From data intelligence emerges"

### "始于数据，终于智能"

---

#### 📧 联系方式

**技术支持**: lidecheng@spharx.cn  
**安全报告**: wangliren@spharx.cn  
**商业咨询**: zhouzhixian@spharx.cn  
**官方网站**: https://spharx.cn

<p>
  <a href="https://gitee.com/spharx/agentos">Gitee（官方主仓库）</a> ·
  <a href="https://github.com/SpharxTeam/AgentOS">GitHub（镜像仓库）</a> ·
  <a href="https://spharx.cn">官方网站</a>
</p>

© 2026 SPHARX Ltd. All Rights Reserved.

</div>
