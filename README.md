# Agent OS

<div align="center">

[![Version](https://img.shields.io/badge/version-1.0.0.6-blue.svg)](https://gitee.com/spharx/agentos)
[![License](https://img.shields.io/badge/license-Apache--2.0-green.svg)](https://gitee.com/spharx/agentos/blob/main/LICENSE)
[![Docker](https://img.shields.io/badge/docker-supported-blue.svg?logo=docker&logoColor=white)](https://www.docker.com/)
[![C/C++](https://img.shields.io/badge/C%2FC%2B%2B-11%2F17-blue.svg?logo=c%2B%2B&logoColor=white)](https://isocpp.org/)
[![Python](https://img.shields.io/badge/python-3.9%2B-blue.svg?logo=python&logoColor=white)](https://www.python.org/)
[![Mirror](https://img.shields.io/badge/mirror-GitHub-lightgrey.svg)](https://github.com/SpharxTeam/AgentOS)
[![Status](https://img.shields.io/badge/status-production%20ready-success.svg)](https://gitee.com/spharx/agentos)

---

<div align="center">

## 🤖 智能体超级驱动


*"From data intelligence emerges · 始于数据，终于智能"*

---

**🌍 Language / 语言**:

[🇨🇳 简体中文](README.md) | [🇬🇧 English](partdocs/readme/en/README.md) | [🇫🇷 Français](partdocs/readme/fr/README.md) | [🇩🇪 Deutsch](partdocs/readme/de/README.md)

</div>

---
</div>

## 📚 官方技术白皮书

项目核心理念已在官方技术白皮书中进行了系统化阐述

| 版本 | 链接 |
|------|------|
| 📘 中文 V1.0 | [AgentOS 技术白皮书](partdocs/white_paper/zh/AgentOS_技术白皮书_V1.0.pdf) |
| 📗 English V1.0 | [AgentOS Technical White Paper](partdocs/white_paper/en/AgentOS_Technical_White_Paper_V1.0.pdf) |

---

</div>

## 🚀 项目简介

AgentOS 是 SpharxWorks 生产线的核心功能之一，是**面向多智能体协作的操作系统内核**。它不是又一个 Agent 框架，而是为智能体文明建造**第一块基石**。可组建团队、自我进化的智能体文明：

"Intelligence emergence, and nothing less, is the ultimate sublimation of AI."

### 核心优势

- **token 效率领先**：工程级任务比行业主流框架节省约 200% token 使用量
- **架构性能优势**：token 利用效率领先行业主流框架 2-3 倍
- **生产级质量**：微内核架构，服务独立演进，内核稳定如磐石
- **安全内生**：四层防护机制，虚拟工位隔离，权限热更新

### 理论基础

AgentOS 的设计根植于五大理论根基，形成完整的学术支撑体系：

| 理论 | 来源 | 核心思想 | 在 AgentOS 中的体现 |
|------|------|----------|---------------------|
| **工程控制论** | Engineering Cybernetics | 反馈调节、动态平衡 | 三层嵌套负反馈系统（实时/轮次内/跨轮次），每层独立闭环 |  
| **系统工程** | On Systems Engineering | 层次分解、综合集成、总体设计部 | 五维正交原则体系，七层架构分层，CoreLoopThree 三层运行时 |
| **双系统认知** | Thinking, Fast and Slow | System 1 快思考，System 2 慢思考 | 认知层 (System 2) 深度规划 vs 行动层 (System 1) 快速执行 |
| **微内核哲学** | Liedtke 微内核原则 + seL4 形式化验证 | 机制与策略分离、最小特权、可验证性 | corekern 仅 4 个原子机制，所有服务运行在用户态 |
| **设计美学** | 乔布斯设计理念 + 人文主义 | 简约至上、极致细节、人文关怀、完美主义 | 代码极简主义、接口精细化设计、责任链可解释性 |

**理论创新点**：
- **工程两论深度融合**：控制论解决动态调节问题（反馈闭环），系统工程解决静态结构问题（层次分解），两者结合形成 AgentOS 架构的双重支柱
- **认知科学工程化**：首次将双系统认知理论完整编码进操作系统架构，实现快慢思考的协同工作
- **微内核范式转移**：从传统 OS 的微内核扩展到智能体操作系统，重新定义"机制 - 策略"边界
- **美学驱动工程**：将设计美学提升至原则高度，以人文关怀引导技术决策

详见：[架构设计原则 v1.6 · 第 1 章 引言]

---

## 💎 核心价值

| 价值维度 | 说明 | 对应原则 |
|---------|------|---------|
| **微内核极简** | 内核仅保留 IPC、内存、任务、时间四个原子机制，所有服务运行在用户态 | K-1 内核极简原则 |
| **三层认知循环** | 认知→规划→调度→执行，决策层与执行层严格分离 | S-3 总体设计部原则 |
| **四层记忆卷载** | L1 原始卷 → L2 特征层 → L3 结构层 → L4 模式层，逐层提炼智慧 | C-3 记忆升华原则 |
| **安全穹顶** | 虚拟工位隔离、权限裁决、输入净化、审计追踪，安全内生 | E-1 安全内生原则 |
| **可插拔策略** | 规划/协同/调度/遗忘/检索算法均可运行时替换 | K-4 可插拔策略原则 |
| **反馈闭环** | 实时反馈修正当前任务，跨轮次反馈驱动长期进化 | S-1 反馈闭环原则 |
| **接口契约化** | 所有跨模块交互通过 Doxygen 契约声明（所有权、线程安全、错误处理） | K-2 接口契约化原则 |
| **多语言 SDK** | Go、Python、Rust、TypeScript 原生支持，FFI 接口高效安全 | - |

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
│  │  llm_d | market_d | monit_d | sched_d | tool_d        │  │
│  └───────────────────────────────────────────────────────┘  │
│                           ↕                                 │
│  ┌───────────────────────────────────────────────────────┐  │
│  │            内核层 (atoms)                              │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌─────────────┐  │  │
│  │  │   corekern   │  │coreloopthree │  │memoryrovol  │  │  │
│  │  │ 微内核基础    │  │三层核心运行时 │  │四层记忆卷载  │  │  │
│  │  │IPC·Mem·Task  │  │认知→规划→执行 │  │L1→L2→L3→L4  │  │  │
│  │  └──────────────┘  └──────────────┘  └─────────────┘  │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌─────────────┐  │  │
│  │  │   syscall    │  │    domes     │  │   utils     │  │  │
│  │  │ 系统调用接口  │  │   安全穹顶    │  │  公共工具   │  │  │
│  │  └──────────────┘  └──────────────┘  └─────────────┘  │  │
│  └───────────────────────────────────────────────────────┘  │
│                           ↕                                 │
│  ┌───────────────────────────────────────────────────────┐  │
│  │           SDK 层 (tools)                              │  │
│  │  Go | Python | Rust | TypeScript                      │  │
│  └───────────────────────────────────────────────────────┘  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 架构设计原则（五维正交体系）

详见：[架构设计原则 v1.6](partdocs/architecture/folder/architectural_design_principles.md)

```
维度一：系统观 (S-1~S-4) ← 工程控制论 & 系统工程
  • 反馈闭环 • 层次分解 • 总体设计部 • 协调性
  
维度二：内核观 (K-1~K-4) ← 微内核哲学
  • 极简主义 • 接口契约 • 最小特权 • 可插拔策略
  
维度三：认知观 (C-1~C-4) ← 双系统认知理论
  • 快慢协同 • 渐进规划 • 自我纠错 • 持续学习
  
维度四：工程观 (E-1~E-7) ← 工程最佳实践
  • 安全内生 • 模块化 • 可测试 • 可观测 • 文档化
  
维度五：设计美学 (A-1~A-4) ← 乔布斯美学 + 人文关怀
  • 简约至上 • 极致细节 • 人文关怀 • 完美主义
```

---

## 📁 项目结构

```
AgentOS/
├── atoms/                      # 内核层（微内核架构）
│   ├── corekern/               # 微内核基础：IPC/Mem/Task/Time
│   ├── coreloopthree/          # 三层一体运行时 ⭐
│   ├── memoryrovol/            # 记忆卷载系统 ⭐
│   ├── syscall/                # 系统调用接口
│   └── utils/                  # 公共工具库
│
├── atomslite/                  # 轻量级内核（嵌入式场景）
│   └── corekernlite/           # 精简版微内核
│
├── domes/                      # 安全穹顶 ⭐
│   ├── workbench/              # 虚拟工位（进程/容器沙箱）
│   ├── permission/             # 权限裁决（RBAC/YAML）
│   ├── sanitizer/              # 输入净化（正则过滤）
│   └── audit/                  # 审计追踪（异步日志）
│
├── backs/                      # 用户态守护进程
│   ├── llm_d/                  # LLM 服务（OpenAI/DeepSeek）
│   ├── market_d/               # 市场服务（Agent/技能注册）
│   ├── monit_d/                # 监控服务（OpenTelemetry）
│   ├── sched_d/                # 调度服务（多策略）
│   └── tool_d/                 # 工具服务（工具执行）
│
├── common/                     # 公共组件库
│   ├── platform/               # 平台抽象层
│   └── utils/                  # 通用工具集
│
├── dynamic/                    # 网关层（HTTP/WebSocket/Stdio）
├── openhub/                    # 开放生态（应用/社区贡献）
├── tools/                      # 多语言 SDK（Go/Python/Rust/TS）
├── config/                     # 配置文件（Agent/环境/安全等）
├── partdata/                   # 运行时数据（内核/日志/服务）
├── partdocs/                   # 技术文档（架构/指南/规范）
├── scripts/                    # 运维脚本（构建/部署/开发）
└── tests/                      # 测试套件（单元/集成/契约/基准）
```

**关键模块说明**：
- **⭐ 核心创新**: CoreLoopThree（三层认知循环）、MemoryRovol（四层记忆卷载）、Domes（安全穹顶）
- **微内核**: 仅 ~9,000 行代码，提供 IPC Binder、内存管理（RAII）、任务调度（加权轮询）、时间服务四大原子机制
- **守护进程**: 所有服务运行在用户态，独立演进，故障隔离
- **系统调用**: 用户态与内核通信的唯一通道，严格的接口契约（Doxygen 注释）
- **统一日志**: 跨语言日志接口（C/Python/Go/Rust/TS），trace_id 全链路追踪，OpenTelemetry 集成

---

## 🛡️ Domes：安全穹顶

`domes/` 是 AgentOS 的安全免疫系统，为每个 Agent 提供隔离的沙箱环境，让智能体既能自由工作，又无法超越边界一步。

### 四重防护机制

| 防护层 | 组件 | 职责 | 核心接口 |
|--------|------|------|---------|
| **虚拟工位** | `workbench/` | 进程（namespaces+cgroups）/容器（runc）沙箱隔离，资源限额，网络可选 | `domes_workbench_create/exec/destroy` |
| **权限裁决** | `permission/` | YAML 规则引擎，RBAC 模型，通配符匹配，缓存加速，热更新 | `domes_permission_check/reload` |
| **输入净化** | `sanitizer/` | 正则规则过滤，风险等级标注（0-3），可替换或删除 | `domes_sanitize` |
| **审计追踪** | `audit/` | 异步队列写入，JSON 格式，日志轮转，结构化查询，全链路记录 | `domes_audit_record/query` |

### 设计原则

- **最小权限**：默认拒绝一切未明确授权的操作
- **防御深度**：单点失效不导致系统崩溃
- **热更新**：安全规则变更零停机
- **可观测**：所有敏感操作全链路审计

详见：[Domes 设计文档](domes/README.md)

---

## 🧠 CoreLoopThree：三层认知循环

CoreLoopThree 是 AgentOS 的核心创新架构，将智能体运行时划分为三个正交且协同的层次，实现**决策与执行分离、认知与记忆融合**。其设计深受**工程两论**指导：控制论的反馈调节（每层独立闭环）+ 系统工程的层次分解（三层正交分离）。

**理论根基**：
- **双系统认知理论**：认知层=System 2（慢思考·深度规划），行动层=System 1（快思考·模式执行）
- **ACT-R/SOAR 认知架构**：目标栈管理、产生式规则引擎、问题空间假设
- **海马体 - 新皮层记忆理论**：快速编码（海马体/L1）→ 慢速整合（新皮层/L2-L4）

### 架构图

```
┌─────────────────────────────────────────────────────────┐
│                    认知层 (Cognition)                    │
│  意图理解 → 任务规划(DAG) → Agent调度 → 模型协同           │
│     ↑                                                   │
│     └────────────── 跨轮次反馈 ──────────────────────┐   │
└─────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────┐
│                    行动层 (Execution)                    │
│  执行引擎 → 补偿事务 → 责任链追踪 → 执行单元注册表          │
│     ↑                                                   │
│     └────────────── 实时反馈 ────────────────────────┐   │
└─────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────┐
│                    记忆层 (Memory)                       │
│  MemoryRovol FFI → 上下文挂载 → LRU缓存 → 进化触发        │
└─────────────────────────────────────────────────────────┘
```

### 核心组件

#### 认知层（Cognition）- System 2 慢思考

**设计理念**：深度分析、战略规划、自我反思

| 组件 | 功能 | 可插拔策略 | 理论映射 |
|------|------|-----------|----------|
| 意图理解引擎 | 解析用户输入，识别核心目标，Intent 结构体 | - | 语言理解模块 |
| 任务规划器 | DAG 任务图生成与动态扩展，依赖关系管理 | 分层/反应式/反思式/ML | 问题空间假设 (SOAR) |
| Agent 调度器 | 多目标优化选择最优 Agent，评分函数驱动 | 加权/轮询/优先级/ML | 效用理论 |
| 模型协同器 | 主辅模型交叉验证与仲裁，加权融合 | 双模型/多数投票/加权融合 | 群体智慧 |
| 协同器 | 多 Agent 协作编排，任务分解与分配 | - | 分布式人工智能 |

**关键特性**：
- **决策与执行分离**：认知层只负责规划和调度，不执行具体任务，避免"既当裁判又当运动员"
- **增量规划器**：支持 DAG 动态扩展，根据执行反馈实时调整任务图
- **自我纠错机制**：引入"思考 - 反思 - 调整"微循环，通过双模型协同解决自我反思的理论极限问题
- **责任链追踪**：完整的决策记录，实现全链路审计和行为可解释性

#### 行动层（Execution）- System 1 快思考

**设计理念**：模式匹配、快速执行、自动补偿

| 组件 | 功能 | 特性 | 理论映射 |
|------|------|------|----------|
| 执行引擎 | 任务状态机管理，生命周期控制 | Pending→Running→Succeeded/Failed/Cancelled/Retrying | 状态自动机 |
| 补偿事务 | 失败自动回滚，Saga 模式 | 补偿链、人工介入队列 | 事务处理理论 |
| 责任链追踪 | 全链路执行记录，TraceID 关联 | OpenTelemetry 集成 | 分布式追踪 |
| 执行单元注册表 | 原子执行器动态加载，热插拔 | API/浏览器/代码/数据库/文件/Shell/工具 | 插件架构 |

**关键特性**：
- **快速模式执行**：基于历史经验的模式匹配，80% 常规任务无需深度思考
- **补偿事务框架**：失败时自动触发回滚链，保证最终一致性
- **执行单元热插拔**：支持运行时动态加载新的执行器，无需重启系统
- **人机协同回路**：复杂任务可暂停等待人工确认，实现 Human-in-the-loop

### 调度官评分函数

```
Score(agent) = w₁ · (1/cost) + w₂ · success_rate + w₃ · trust_score
```

其中：
- `cost`: Agent 调用成本（token 消耗或时间开销）
- `success_rate`: 历史成功率（0-1 区间）
- `trust_score`: 信任评分（基于用户评价和审计记录）
- `w₁, w₂, w₃`: 权重系数，可动态调整

详见：[CoreLoopThree 架构文档](partdocs/architecture/folder/coreloopthree.md)

---

## 💾 MemoryRovol：四层记忆卷载

MemoryRovol 是 AgentOS 的内核级记忆系统，实现从原始数据到高级模式的**全栈记忆管理能力**。它的名字源自"卷"——像胶卷一样，记忆被逐层显影，从朦胧的原始影像逐步凝结为清晰的模式。

### 四层架构

```
┌─────────────────────────────────────────────────────────┐
│              L4 Pattern Layer (模式层)                   │
│   持久同调分析(Ripser) · HDBSCAN聚类 · 规则生成           │
│   输出：可复用的行为模式，豁免遗忘                         │
└─────────────────────────↑───────────────────────────────┘
                          ↓ 抽象进化
┌─────────────────────────────────────────────────────────┐
│             L3 Structure Layer (结构层)                  │
│   绑定/解绑算子 · 关系编码 · 时序编码 · 图编码             │
│   输出：记忆间的结构关系                                  │
└─────────────────────────↑───────────────────────────────┘
                          ↓ 特征提取
┌─────────────────────────────────────────────────────────┐
│             L2 Feature Layer (特征层)                   │
│   嵌入模型 · FAISS向量索引 · 混合检索(向量+BM25)          │
│   输出：语义向量，支撑相似度搜索                          │
└─────────────────────────↑───────────────────────────────┘
                          ↓ 数据压缩
┌─────────────────────────────────────────────────────────┐
│               L1 Raw Layer (原始卷)                      │
│   文件系统存储 · 分片压缩 · SQLite元数据 · 版本控制        │
│   输出：带时间戳的原始记忆，仅追加，永不修改                │
└─────────────────────────────────────────────────────────┘
```

### 神经科学类比

| 层级 | 脑区类比 | 功能 | 实现技术 |
|------|---------|------|----------|
| L1 原始卷 | 海马体 CA3 区 | 原始情景痕迹，快速编码 | 文件系统 + SQLite 索引 |
| L2 特征层 | 内嗅皮层 | 特征提取与向量索引 | FAISS + Embedding 模型 |
| L3 结构层 | 海马 - 新皮层通路 | 关系绑定与时序巩固 | 绑定算子 + 图神经网络 |
| L4 模式层 | 前额叶皮层 | 抽象规则与图式，慢速整合 | 持久同调 + HDBSCAN 聚类 |

### 遗忘机制

基于艾宾浩斯遗忘曲线（$R = e^{-\lambda t}$），其中 $\lambda$ 为遗忘速率常数，受访问频率和情感权重调节：

**数学模型**：
- **指数衰减**：$R(t) = e^{-\lambda t}$（默认）
- **线性衰减**：$R(t) = 1 - \alpha t$
- **基于访问次数**：$R(t) = e^{-\lambda t / (1 + \beta \cdot access_count)}$
- **情感权重调节**：$\lambda' = \lambda \cdot e^{-\gamma \cdot emotional_weight}$

**遗忘策略**：
- **裁剪**：低权重记忆被裁剪或归档至冷存储（L1 Cold Archive）
- **复活**：被归档的记忆在相关查询时可重新激活（吸引子动力学·Pattern Completion）
- **固化**：高频使用或高价值记忆被提取为 L4 模式，豁免遗忘（类似海马体→新皮层巩固）
- **睡眠重放**：空闲时段触发记忆回放，强化重要连接（借鉴神经科学睡眠理论）

### 检索动力学

基于现代 Hopfield 网络的吸引子动力学，实现**模式补全**（Pattern Completion）：

**能量函数**：
```
E(z) = -∑ᵤ (mᵘ · z)² + λ·||z||²
```

**迭代收敛**：
```
z(t+1) = σ(∑ᵤ mᵘ (mᵘ · z(t)))
```

部分线索经过迭代收敛到完整记忆，即使输入不完整也能检索到相关记忆。

**重排序（Reranker）机制**：
```python
# 混合检索后的精排阶段
final_scores = α * vector_similarity + β * bm25_score + γ * recency_boost + δ * emotional_weight
```

其中：
- `vector_similarity`: 余弦相似度（FAISS 计算）
- `bm25_score`: BM25 文本相关性分数
- `recency_boost`: 近因效应增强（基于时间衰减）
- `emotional_weight`: 情感权重（高情感价值记忆优先）

### 性能指标

| 指标 | 数值 |
|------|------|
| L1 写入吞吐 | 10,000+ 条/秒 |
| L2 向量检索延迟 | < 10ms (k=10) |
| 混合检索延迟 | < 50ms (top-100) |
| L2→L3 抽象速度 | 100 条/秒 |
| L4 模式挖掘速度 | 10 万条/分钟 |

详见：[MemoryRovol 架构文档](partdocs/architecture/folder/memoryrovol.md)

---

## 🎨 设计美学：代码中的艺术

AgentOS 不仅是一个工程产品，更是一件精心雕琢的艺术品。我们将**设计美学**提升至架构原则的高度，以人文关怀引导技术决策。

### 四大美学原则

| 原则 | 内涵 | 在 AgentOS 中的体现 |
|------|------|---------------------|
| **简约至上 (A-1)** | 少即是多，去除一切不必要的复杂性 | • 微内核仅保留 4 个原子机制<br>• 系统调用接口精简（<50 个核心函数）<br>• 代码行数控制在可理解范围内（corekern ~9K LOC） |
| **极致细节 (A-2)** | 魔鬼藏在细节中，追求完美 | • 所有公共 API 定义 Doxygen 契约（参数方向、所有权、线程安全）<br>• 错误码统一分级处理（ERROR/WARNING/INFO）<br>• 日志格式标准化（时间戳 +trace_id+ 结构化数据） |
| **人文关怀 (A-3)** | 技术服务于人，增强而非替代 | • 责任链追踪实现行为可解释性<br>• Human-in-the-loop 支持人工介入关键决策<br>• 多语言 SDK 降低开发者门槛<br>• 完善的文档体系（架构/指南/哲学） |
| **完美主义 (A-4)** | 不妥协，持续改进，追求卓越 | • 核心模块 100% 单元测试覆盖<br>• 性能指标量化并持续优化<br>• 定期代码审查和架构评审<br>• 变更日志严格遵循 Keep a Changelog 规范 |

### 代码美学示例

**极简主义 - RAII 内存管理**：
```
// 智能指针自动释放，无需手动 free
core_mem_ptr_t ptr = core_mem_alloc(size);
if (!ptr) return AGENTOS_ERROR_NO_MEMORY;
// 离开作用域时自动释放，零泄漏风险
```

**契约化接口 - Doxygen 注释**：
```
/**
 * @brief 写入原始记忆
 * @param data [in] 数据缓冲区（不可为 NULL）
 * @param len [in] 数据长度（必须>0）
 * @param metadata [in,opt] JSON 元数据（可为 NULL）
 * @param out_record_id [out] 输出记录 ID（需调用者释放）
 * @return agentos_error_t
 * @threadsafe 是（内部使用互斥锁保护）
 * @see agentos_sys_memory_search(), agentos_sys_memory_delete()
 */
AGENTOS_API agentos_error_t agentos_sys_memory_write(...);
```

**错误处理 - 统一分级**：
```
switch (error) {
    case AGENTOS_SUCCESS:
        // 成功路径，快速返回
        break;
    case AGENTOS_ERROR_INVALID_PARAM:
        // 参数错误，记录 WARNING 级别日志
        AGENTOS_LOG_WARNING("Invalid parameter: %s", param_name);
        break;
    case AGENTOS_ERROR_NO_MEMORY:
        // 严重错误，触发告警并尝试恢复
        AGENTOS_LOG_ERROR("Out of memory, attempting recovery");
        attempt_memory_recovery();
        break;
}
```

详见：[架构设计原则 v1.6 · 第 6 章 维度五：设计美学](partdocs/architecture/folder/architectural_design_principles.md#第 -6-章 -维度五设计美学)

---

## 📞 Syscall：系统调用接口

系统调用是用户态服务与内核之间的唯一通信通道。所有守护进程（`backs/`）必须通过 `syscalls.h` 与内核交互，禁止直接调用内核内部函数。

### 核心接口

| 类别 | 接口 | 说明 |
|------|------|------|
| **任务管理** | `agentos_sys_task_submit/query/wait/cancel` | 任务全生命周期管理 |
| **记忆管理** | `agentos_sys_memory_write/search/get/delete` | 记忆的 CRUD 操作 |
| **会话管理** | `agentos_sys_session_create/get/close/list` | 多轮对话上下文管理 |
| **可观测性** | `agentos_sys_telemetry_metrics/traces` | 指标采集与链路追踪 |
| **Agent 管理** | `agentos_sys_agent_register/invoke/terminate` | Agent 创建与调用 |

### 接口契约示例

```
/**
 * @brief 写入原始记忆
 * @param data 数据
 * @param len 数据长度
 * @param metadata JSON元数据（可为NULL）
 * @param out_record_id 输出记录ID（需调用者释放）
 * @return agentos_error_t
 * @threadsafe 是
 * @see agentos_sys_memory_search(), agentos_sys_memory_delete()
 */
AGENTOS_API agentos_error_t agentos_sys_memory_write(
    const void* data, size_t len,
    const char* metadata, char** out_record_id);
```

详见：[系统调用规范](partdocs/architecture/folder/syscall.md)

---

## 🛠️ 开发指南

### 环境要求

| 类别 | 要求 |
|------|------|
| **操作系统** | Linux (Ubuntu 22.04+), macOS 13+, Windows 11 (WSL2) |
| **编译器** | GCC 11+ / Clang 14+ / MSVC 2022+ |
| **构建工具** | CMake 3.20+, Ninja 或 Make |
| **核心依赖** | OpenSSL, libevent, pthread, SQLite3, libcurl, cJSON |
| **向量检索** | FAISS >= 1.7.0（IVF/HNSW 索引） |
| **可选依赖** | Ripser (持久同调), HDBSCAN (聚类分析), libyaml (配置解析), libcjson (JSON 序列化) |

### 快速开始

```
# 克隆项目
git clone https://gitee.com/spharx/agentos.git
cd agentos

# 初始化配置
cp .env.example .env
python scripts/init_config.py

# 构建项目
mkdir build && cd build
cmake ../atoms -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build . --parallel $(nproc)

# 运行测试
ctest --output-on-failure
```

### CMake 配置选项

| 变量 | 说明 | 默认值 |
|------|------|--------|
| `CMAKE_BUILD_TYPE` | Debug/Release/RelWithDebInfo | `Release` |
| `BUILD_TESTS` | 构建单元测试 | `OFF` |
| `ENABLE_TRACING` | 启用 OpenTelemetry 追踪 | `OFF` |
| `ENABLE_ASAN` | 启用 AddressSanitizer | `OFF` |
| `ENABLE_LOGGING` | 启用统一日志系统 | `ON` |

详见：[BUILD.md](atoms/BUILD.md)

---

## 📊 性能指标

基于标准测试环境 (Intel i7-12700K, 32GB RAM, NVMe SSD):

### 处理能力

| 指标 | 数值 | 测试条件 |
|------|------|---------|
| 记忆写入吞吐 | 10,000+ 条/秒 | L1 层，异步批量写入 |
| 向量检索延迟 | < 10ms | FAISS IVF1024,PQ64, k=10 |
| 混合检索延迟 | < 50ms | 向量+BM25, top-100 重排序 |
| 并发连接数 | 1024 | Binder IPC 最大连接 |
| 任务调度延迟 | < 1ms | 加权轮询策略 |
| Agent 调度延迟 | < 5ms | 加权轮询 |
| L2→L3 抽象速度 | 100 条/秒 | 批量处理模式 |
| L4 模式挖掘速度 | 10 万条/分钟 | 持久同调分析 |

### 资源利用率

| 场景 | CPU 占用 | 内存占用 | 磁盘 IO |
|------|---------|---------|---------|
| 空闲状态 | < 5% | 200MB | < 1MB/s |
| 中等负载 | 30-50% | 1-2GB | 10-50MB/s |
| 高负载 | 80-100% | 4-8GB | 100-500MB/s |

详见：[benchmark.py](scripts/benchmark.py) | [性能指标文档](partdocs/specifications/README.md)

---

## 📚 文档资源

### 核心文档

| 文档 | 说明 |
|------|------|
| [架构设计原则 v3.0](partdocs/architecture/folder/architectural_design_principles.md) | **四维原则体系**：系统观/内核观/认知观/工程观 |
| [CoreLoopThree 架构](partdocs/architecture/folder/coreloopthree.md) | 三层认知循环：认知→规划→执行 |
| [MemoryRovol 架构](partdocs/architecture/folder/memoryrovol.md) | 四层记忆系统：L1→L2→L3→L4 |
| [微内核设计](partdocs/architecture/folder/microkernel.md) | corekern 原子内核：IPC/Mem/Task/Time |
| [系统调用规范](partdocs/architecture/folder/syscall.md) | syscall 接口契约 |
| [IPC 通信机制](partdocs/architecture/folder/ipc.md) | Binder 驱动的进程间通信 |
| [统一日志系统](partdocs/architecture/folder/logging_system.md) | trace_id 全链路追踪 |
| [Domes 安全穹顶](domes/README.md) | 安全四重防护机制 |

### 哲学基础

| 文档 | 说明 |
|------|------|
| [认知层理论](partdocs/philosophy/Cognition_Theory.md) | 双系统认知模型在 AgentOS 中的应用 |
| [记忆层理论](partdocs/philosophy/Memory_Theory.md) | 四层记忆卷载的神经科学与数学基础 |
| [设计原则](partdocs/philosophy/Design_Principles.md) | 从"工程两论"到架构设计 |
| [架构师手册](partdocs/architecture/folder/architect_handbook.md) | 架构决策记录与设计模式 |

### 开发指南

| 文档 | 说明 |
|------|------|
| [快速入门](partdocs/guides/folder/getting_started.md) | 环境搭建与第一个 Agent |
| [创建 Agent](partdocs/guides/folder/create_agent.md) | Agent 开发教程 |
| [创建技能](partdocs/guides/folder/create_skill.md) | 技能开发教程 |
| [部署指南](partdocs/guides/folder/deployment.md) | 生产环境部署 |
| [内核调优](partdocs/guides/folder/kernel_tuning.md) | 性能优化与参数配置 |
| [故障排查](partdocs/guides/folder/troubleshooting.md) | 常见问题诊断与解决 |
| [模块特性](partdocs/guides/folder/module_features.md) | 核心模块功能详解 |

---

## 🗺️ 版本路线图

### 当前版本 (v1.0.0.6) - 生产就绪 (Production Ready) 🎉

**发布日期**: 2026-03-25  
**核心模块完成度**: 100%

基于最新的代码实现分析，以下是各模块的详细完成度评估：

| 模块 | 完成度 | 状态 | 关键特性 | 代码行数 |
|------|--------|------|----------|----------|
| **corekern 微内核** | 100% | ✅ | IPC Binder、内存管理（RAII）、任务调度（加权轮询）、高精度时间 | ~9,000 LOC |
| **coreloopthree 三层循环** | 100% | ✅ | 认知层（意图理解/任务规划/Agent 调度）、行动层（执行引擎/补偿事务）、记忆层 FFI | ~15,000 LOC |
| **memoryrovol 四层记忆** | 100% | ✅ | L1-L4 全栈实现、FAISS 集成（IVF/HNSW）、遗忘曲线、吸引子检索 | ~20,000 LOC |
| **syscall 系统调用** | 100% | ✅ | 任务/记忆/会话/可观测性/Agent 全接口、Doxygen 契约 | ~3,000 LOC |
| **domes 安全穹顶** | 100% | ✅ | 虚拟工位（进程/容器）、权限裁决（RBAC/YAML）、净化、审计 | ~12,000 LOC |
| **backs 守护进程** | 100% | ✅ | llm_d/market_d/monit_d/sched_d/tool_d 全部实现 | ~25,000 LOC |
| **tools 多语言 SDK** | 90% | 🔄 | Go/Python/Rust/TypeScript，异步支持，类型注解 | ~8,000 LOC |
| **统一日志系统** | 100% | ✅ | 跨语言接口、trace_id 追踪、OpenTelemetry 集成 | ~5,000 LOC |

**关键里程碑**：
- ✅ **核心架构设计完成**（v1.0.0.0-v1.0.0.3）：微内核 + 三层运行时 + 四层记忆理论奠基
- ✅ **MemoryRovol 记忆系统实现**（v1.0.0.4-v1.0.0.5）：L1-L4 全栈开发完成
- ✅ **CoreLoopThree 运行时框架完成**（v1.0.0.5-v1.0.0.6）：认知层 + 行动层 + 记忆层闭环
- ✅ **系统调用层 100% 完成**（v1.0.0.6）：所有公共 API 定义 Doxygen 契约
- ✅ **统一日志系统集成**（v1.0.0.6）：跨语言追踪 + OpenTelemetry 集成
- ✅ **Domes 安全穹顶发布**（v1.0.0.6）：四重防护机制全部实现
- ✅ **文档体系完善**（v1.0.0.6）：架构原则 v1.6、CoreLoopThree v1.6、MemoryRovol v1.6

**v1.0.0.6 新增特性**（相比 v1.0.0.5）：
- ✨ 新增：设计美学维度（A-1~A-4），将美学原则提升至架构高度
- ✨ 新增：责任链追踪机制，实现全链路审计和行为可解释性
- ✨ 新增：Human-in-the-loop 支持，复杂任务可暂停等待人工确认
- ✨ 新增：睡眠重放机制，空闲时段触发记忆回放强化重要连接
- 🚀 优化：情感权重调节遗忘速率，高情感价值记忆豁免遗忘
- 🚀 优化：重排序机制增加情感权重因子，提升检索相关性
- 📝 完善：架构设计原则 v1.6，深度整合工程两论和双系统认知理论

### 短期目标 (2026 Q2-Q3)

- **v1.0.1.0**: 性能优化专项
  - FAISS 参数调优（IVF 分区数、PQ 量化参数）
  - LRU 缓存命中率提升至 95%+
  - 混合检索延迟优化至 <30ms
  - 批量写入吞吐提升至 20,000 条/秒
  
- **v1.0.2.0**: 开发者工具集
  - 记忆可视化调试器（查看 L1-L4 各层记忆状态）
  - 执行追踪器（实时查看责任链执行情况）
  - 性能分析工具（CPU/Memory/IO 热点定位）
  - IDE 插件（VSCode/IntelliJ 代码补全和调试支持）

### 中期规划 (2026 Q4-2027)

- **v1.0.3.0**: 生产增强（端到端测试、安全审计、性能基准）
- **v1.0.4.0**: 分布式支持（多节点集群、分布式记忆、共识算法）
- **v1.0.5.0**: 智能化升级（自适应记忆、强化学习优化、元学习）

### 长期愿景 (2027+)

- 成为智能体操作系统的事实标准
- 构建全球化开源社区生态
- 支持万亿级记忆容量和毫秒级检索

---

## 🤝 生态合作

### 技术合作伙伴

- **AI 实验室**: 大模型、记忆系统、认知架构等领域专家
- **硬件厂商**: GPU、NPU、存储设备提供商
- **应用企业**: 机器人、智能助理、自动化等落地场景

### 社区贡献

AgentOS 欢迎所有形式的贡献：

- **代码贡献**: 核心功能开发和优化（遵循 K-4 可插拔策略原则）
- **策略贡献**: 新的规划/协同/调度/遗忘/检索策略（通过 `tests/contract/` 契约验证）
- **文档完善**: 使用指南和技术文档（多语言翻译）
- **测试验证**: 功能测试、性能基准测试、安全渗透测试
- **生态建设**: Agent 应用开发、技能市场贡献、开发者工具

详见：[CONTRIBUTING.md](CONTRIBUTING.md)

---

## 📄 许可证

AgentOS 采用**商业友好、生态开放的分层开源协议架构**：

| 模块目录 | 适用协议 | 说明 |
|----------|----------|------|
| `atoms/`（内核） | Apache License 2.0 | 核心代码，不可变基础 |
| `domes/`（扩展） | Apache License 2.0 | 安全穹顶，与内核协议一致 |
| `openhub/`（生态） | MIT License | 社区贡献模块，降低参与门槛 |
| 第三方依赖 | 遵循原组件协议 | 模块隔离，无协议传染 |

### 您可以自由地

- ✅ **商用**：免费用于闭源商业产品
- ✅ **修改**：无需开源修改后的业务代码
- ✅ **分发**：自由分发源代码或二进制
- ✅ **专利使用**：获得核心代码的永久专利授权

### 唯一核心义务

- 保留原项目的版权声明和许可证文本
- 若修改核心源代码，需在文件中保留修改记录

详见：[LICENSE](LICENSE) | [NOTICE](NOTICE)

---

## 🙏 致谢

感谢所有为开源社区做出贡献的开发者们，以及为 AgentOS 项目提供支持的合作伙伴。

特别感谢：
- FAISS 团队 (Facebook AI Research)
- Sentence Transformers 团队
- Rust 和 Go 语言社区
- 所有贡献者和用户

详见：[ACKNOWLEDGMENTS.md](ACKNOWLEDGMENTS.md) | [AUTHORS.md](AUTHORS.md)

---

<div align="center">

### "From data intelligence emerges"

### "始于数据，终于智能"

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