# Agent OS

<div align="center">

[![Version](https://img.shields.io/badge/version-1.0.0.6-blue.svg)](https://gitee.com/spharx/agentos)
[![License](https://img.shields.io/badge/license-Apache--2.0-green.svg)](https://gitee.com/spharx/agentos/blob/main/LICENSE)
[![Docker](https://img.shields.io/badge/docker-supported-blue.svg?logo=docker&logoColor=white)](https://www.docker.com/)
[![C/C++](https://img.shields.io/badge/C%2FC%2B%2B-11%2F17-blue.svg?logo=c%2B%2B&logoColor=white)](https://isocpp.org/)
[![Python](https://img.shields.io/badge/python-3.9%2B-blue.svg?logo=python&logoColor=white)](https://www.python.org/)
[![Mirror](https://img.shields.io/badge/mirror-GitHub-lightgrey.svg)](https://github.com/SpharxTeam/AgentOS)

---
**智能体超级操作系统**

*"From data intelligence emerges 始于数据，终于智能。"*

--

Language： **简体中文** | [English](partdocs/readme/en/README.md) | [Français](partdocs/readme/fr/README.md) | [Deutsch](partdocs/readme/de/README.md)

</div>

---

## 📄 官方技术白皮书
本项目核心技术理念、架构设计、能力边界与演进规划，均已在官方技术白皮书中进行完整、严谨的系统化阐述。

- 中文正式版：[AgentOS 技术白皮书 V1.0](partdocs/white_paper/zh/AgentOS_技术白皮书_V1.0.pdf)
- English Draft: [AgentOS Technical White Paper V1.0](partdocs/white_paper/en/AgentOS_Technical_White_Paper_V1.0.pdf)


## 🚩 项目简介

AgentOS 是 SpharxWorks 的核心产品，是**面向多智能体协作的操作系统内核**。

它不是又一个 Agent 框架，而是为智能体文明建造**第一块操作系统基石**。

以微内核为骨骼，以认知循环为神经系统，以记忆卷载为经验基底，以安全穹顶为免疫系统，AgentOS让智能体从单打独斗的指令执行器进化为可组建团队、自我进化的智能体文明：

"Intelligence emergence, and nothing less, is the ultimate sublimation of AI."

### 核心优势

- **token 效率领先**：工程级任务比行业主流框架节省约 60% token 使用量
- **架构性能优势**：token 利用效率领先行业主流框架 2-3 倍
- **生产级质量**：微内核架构，服务独立演进，内核稳定如磐石

### 理论基础

AgentOS 的设计根植于三大理论根基：

| 理论 | 来源 | 在 AgentOS 中的体现 |
|------|------|---------------------|
| **工程控制** | Engineering Cybernetics | 三层嵌套负反馈系统（实时/轮次内/跨轮次） |
| **系统工程** | On Systems Engineering | 四维原则体系，层次分解，总体设计部协调 |
| **双系统认知** | Thinking, Fast and Slow | System 1 快速执行，System 2 深度规划 |

详见：[架构设计原则 v1.3](partdocs/architecture/architectural_design_principles.md)

---

## 📋 核心价值

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
│  │  │   corekern  │  │coreloopthree │  │memoryrovol  │   │  │
│  │  │ 微内核基础   │  │三层核心运行时 │  │四层记忆卷载  │   │  │
│  │  │IPC·Mem·Task │  │认知→规划→执行 │  │L1→L2→L3→L4  │   │  │
│  │  └──────────────┘  └──────────────┘  └─────────────┘  │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌─────────────┐  │  │
│  │  │   syscall    │  │    domes     │  │   utils     │  │  │
│  │  │ 系统调用接口  │  │   安全穹顶    │  │  公共工具    │  │  │
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

### 架构设计原则（四维体系）

详见：[架构设计原则 v1.3](partdocs/architecture/architectural_design_principles.md)

```
维度一：系统观 ← 控制论 & 系统工程（S-1~S-4）
维度二：内核观 ← 微内核哲学（K-1~K-4）
维度三：认知观 ← 双系统理论（C-1~C-4）
维度四：工程观 ← 乔布斯美学（E-1~E-7）
```

---

## 📁 项目结构

```
AgentOS/
├── atoms/                      # 内核层（微内核架构）
│   ├── corekern/               # 微内核基础
│   │   ├── include/            # 头文件：ipc.h, mem.h, task.h, time.h, error.h
│   │   └── src/                # IPC/Mem/Task/Time 实现
│   ├── coreloopthree/          # 三层一体核心运行时 ⭐
│   │   ├── cognition/          # 认知层：意图理解、任务规划、Agent 调度
│   │   ├── execution/          # 行动层：执行引擎、补偿事务、责任链追踪
│   │   ├── memory/             # 记忆层：MemoryRovol FFI 封装
│   │   └── planner/            # 规划器：分层/反应式/反思式/ML 规划
│   ├── memoryrovol/            # 记忆卷载系统 ⭐
│   │   ├── layer1_raw/         # L1 原始卷：文件系统存储、分片压缩
│   │   ├── layer2_feature/     # L2 特征层：FAISS 向量索引、混合检索
│   │   ├── layer3_structure/   # L3 结构层：绑定算子、关系编码
│   │   ├── layer4_pattern/     # L4 模式层：持久同调、HDBSCAN 聚类
│   │   ├── retrieval/          # 检索机制：吸引子网络、LRU 缓存、重排序
│   │   └── forgetting/         # 遗忘机制：艾宾浩斯曲线、裁剪、复活
│   ├── syscall/                # 系统调用接口
│   │   └── src/                # task, memory, session, telemetry, agent
│   └── utils/                  # 公共工具库
│       ├── error/              # 错误处理
│       ├── logger/             # 结构化日志
│       ├── metrics/            # 指标收集
│       ├── trace/              # 链路追踪
│       └── cost/               # Token 预算控制
│
├── domes/                      # 安全穹顶 ⭐
│   ├── workbench/              # 虚拟工位（进程/容器/WASM 沙箱）
│   ├── permission/             # 权限裁决（YAML 规则、热更新）
│   ├── sanitizer/              # 输入净化（正则过滤、风险等级）
│   └── audit/                  # 审计追踪（异步写入、日志轮转）
│
├── backs/                      # 用户态守护进程
│   ├── llm_d/                  # LLM 服务（OpenAI/DeepSeek/本地）
│   ├── market_d/               # 市场服务（Agent/技能注册与发现）
│   ├── monit_d/                # 监控服务（OpenTelemetry）
│   ├── sched_d/                # 调度服务（多策略调度器）
│   └── tool_d/                 # 工具服务（工具注册与执行）
│
├── dynamic/                    # 网关层
│   └── src/gateway/            # HTTP/WebSocket/Stdio 网关
│
├── openhub/                    # 开放生态
│   ├── app/                    # 示例应用（docgen/ecommerce/research/videoedit）
│   ├── contrib/                # 社区贡献（Agent/技能/策略）
│   └── markets/                # 市场基础设施
│
├── tools/                      # 多语言 SDK
│   ├── go/                     # Go SDK
│   ├── python/                 # Python SDK
│   ├── rust/                   # Rust SDK
│   └── typescript/             # TypeScript SDK
│
├── config/                     # 配置文件
│   ├── agent/                  # Agent 配置
│   ├── environment/            # 环境配置
│   ├── kernel/                 # 内核配置
│   ├── logging/                # 日志配置
│   ├── model/                  # 模型配置
│   ├── sanitizer/              # 净化规则
│   ├── schema/                 # Schema 定义
│   ├── security/               # 安全策略
│   ├── service/                # 服务配置
│   ├── skill/                  # Skill 配置
│   └── .env.template           # 环境变量模板
│
├── partdata/                   # 运行时数据
│   ├── kernel/                 # 内核数据
│   ├── logs/                   # 日志文件
│   └── services/               # 服务数据
│
├── partdocs/                   # 技术文档
│   ├── architecture/           # 架构文档（设计原则、CoreLoopThree、MemoryRovol 等）
│   ├── philosophy/             # 设计哲学（认知理论、记忆理论）
│   ├── specifications/         # 技术规范（编码标准、API 规范）
│   ├── api/                    # API 文档
│   ├── guides/                 # 开发指南
│   ├── readme/                 # 多语言 README
│   └── white_paper/            # 技术白皮书（zh/en）
│
├── scripts/                    # 运维脚本
│   ├── build/                  # 构建脚本
│   ├── deploy/                 # 部署脚本
│   ├── dev/                    # 开发辅助脚本
│   ├── init/                   # 初始化脚本
│   ├── lib/                    # 脚本库
│   └── ops/                    # 运维脚本
│
└── tests/                      # 测试套件
    ├── unit/                   # 单元测试
    ├── integration/            # 集成测试
    ├── contract/               # 契约测试
    ├── security/               # 安全测试
    ├── benchmarks/             # 性能基准测试
    └── fixtures/               # 测试夹具
```

**关键模块说明**：
- **⭐ 核心创新**: CoreLoopThree（三层认知循环）、MemoryRovol（四层记忆卷载）、Domes（安全穹顶）
- **微内核**: 仅 ~9,000 行代码，提供 IPC、内存、任务、时间四大原子机制
- **守护进程**: 所有服务运行在用户态，独立演进，故障隔离
- **系统调用**: 用户态与内核通信的唯一通道，严格的接口契约

---

## 🛡️ Domes：安全穹顶

`domes/` 是 AgentOS 的安全免疫系统，为每个 Agent 提供隔离的沙箱环境，让智能体既能自由工作，又无法超越边界一步。

### 四重防护机制

| 防护层 | 组件 | 职责 | 核心接口 |
|--------|------|------|---------|
| **虚拟工位** | `workbench/` | 进程/容器/WASM 沙箱隔离，资源限额，网络可选 | `domes_workbench_create/exec/destroy` |
| **权限裁决** | `permission/` | YAML 规则引擎，通配符匹配，缓存加速，热更新 | `domes_permission_check/reload` |
| **输入净化** | `sanitizer/` | 正则规则过滤，风险等级标注（0-3），可替换或删除 | `domes_sanitize` |
| **审计追踪** | `audit/` | 异步写入，日志轮转，结构化查询，全链路记录 | `domes_audit_record/query` |

### 设计原则

- **最小权限**：默认拒绝一切未明确授权的操作
- **防御深度**：单点失效不导致系统崩溃
- **热更新**：安全规则变更零停机
- **可观测**：所有敏感操作全链路审计

详见：[domes 设计文档](domes/README.md)

---

## 🧠 CoreLoopThree：三层认知循环

CoreLoopThree 是 AgentOS 的核心创新，将智能体运行时划分为三个正交且协同的层次，实现**决策与执行分离、认知与记忆融合**。

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

#### 认知层（Cognition）

| 组件 | 功能 | 可插拔策略 |
|------|------|-----------|
| 意图理解引擎 | 解析用户输入，识别核心目标 | - |
| 任务规划器 | DAG 任务图生成与动态扩展 | 分层/反应式/反思式/ML |
| Agent 调度器 | 多目标优化选择最优 Agent | 加权/轮询/优先级/ML |
| 模型协同器 | 主辅模型交叉验证与仲裁 | 双模型/多数投票/加权融合 |

#### 行动层（Execution）

| 组件 | 功能 | 特性 |
|------|------|------|
| 执行引擎 | 任务状态机管理 | Pending→Running→Succeeded/Failed/Cancelled/Retrying |
| 补偿事务 | 失败自动回滚 | Saga 模式，人工介入队列 |
| 责任链追踪 | 全链路执行记录 | TraceID 关联，OpenTelemetry 集成 |
| 执行单元注册表 | 原子执行器动态加载 | API/浏览器/代码/数据库/文件/Shell/工具 |

### 调度官评分函数

```
Score(agent) = w₁ · (1/cost) + w₂ · success_rate + w₃ · trust_score
```

详见：[CoreLoopThree 架构文档](partdocs/architecture/coreloopthree.md)

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

| 层级 | 脑区 | 功能 |
|------|------|------|
| L1 原始卷 | 海马体 CA3 | 原始情景痕迹 |
| L2 特征层 | 内嗅皮层 | 特征提取与索引 |
| L3 结构层 | 海马-新皮层通路 | 关系绑定与巩固 |
| L4 模式层 | 前额叶皮层 | 抽象规则与图式 |

### 遗忘机制

基于艾宾浩斯遗忘曲线（$R = e^{-\lambda t}$）：

- **遗忘策略**：指数衰减 / 线性衰减 / 基于访问次数
- **裁剪**：低权重记忆被裁剪或归档
- **复活**：被归档的记忆在相关查询时可重新激活
- **固化**：高频使用或高价值记忆被提取为 L4 模式，豁免遗忘

### 检索动力学

基于现代 Hopfield 网络的吸引子动力学：

```
z(t+1) = σ(∑ᵤ mᵘ (mᵘ · z(t)))
```

部分线索经过迭代收敛到完整记忆，实现**模式补全**。

### 性能指标

| 指标 | 数值 |
|------|------|
| L1 写入吞吐 | 10,000+ 条/秒 |
| L2 向量检索延迟 | < 10ms (k=10) |
| 混合检索延迟 | < 50ms (top-100) |
| L2→L3 抽象速度 | 100 条/秒 |
| L4 模式挖掘速度 | 10 万条/分钟 |

详见：[MemoryRovol 架构文档](partdocs/architecture/memoryrovol.md)

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

```c
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

详见：[系统调用规范](partdocs/architecture/syscall.md)

---

## 🛠️ 开发指南

### 环境要求

| 类别 | 要求 |
|------|------|
| **操作系统** | Linux (Ubuntu 22.04+), macOS 13+, Windows 11 (WSL2) |
| **编译器** | GCC 11+ / Clang 14+ / MSVC 2022+ |
| **构建工具** | CMake 3.20+, Ninja 或 Make |
| **核心依赖** | OpenSSL, libevent, pthread, SQLite3, libcurl, cJSON |
| **向量检索** | FAISS >= 1.7.0 |
| **可选依赖** | Ripser (持久同调), HDBSCAN (聚类分析) |

### 快速开始

```bash
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

### 资源利用率

| 场景 | CPU 占用 | 内存占用 | 磁盘 IO |
|------|---------|---------|---------|
| 空闲状态 | < 5% | 200MB | < 1MB/s |
| 中等负载 | 30-50% | 1-2GB | 10-50MB/s |
| 高负载 | 80-100% | 4-8GB | 100-500MB/s |

详见：[benchmark.py](scripts/benchmark.py)

---

## 📚 文档资源

### 核心文档

| 文档 | 说明 |
|------|------|
| [架构设计原则 v3.0](partdocs/architecture/architectural_design_principles.md) | **四维原则体系**：系统观/内核观/认知观/工程观 |
| [CoreLoopThree 架构](partdocs/architecture/coreloopthree.md) | 三层认知循环：认知→规划→执行 |
| [MemoryRovol 架构](partdocs/architecture/memoryrovol.md) | 四层记忆系统：L1→L2→L3→L4 |
| [微内核设计](partdocs/architecture/microkernel.md) | corekern 原子内核：IPC/Mem/Task/Time |
| [系统调用规范](partdocs/architecture/syscall.md) | syscall 接口契约 |
| [Domes 安全穹顶](domes/README.md) | 安全四重防护机制 |

### 哲学基础

| 文档 | 说明 |
|------|------|
| [认知层理论](partdocs/philosophy/Cognition_Theory.md) | 双系统认知模型在 AgentOS 中的应用 |
| [记忆层理论](partdocs/philosophy/Memory_Theory.md) | 四层记忆卷载的神经科学与数学基础 |
| [设计原则](partdocs/philosophy/Design_Principles.md) | 从"工程两论"到架构设计 |

### 开发指南

| 文档 | 说明 |
|------|------|
| [快速入门](partdocs/guides/getting_started.md) | 环境搭建与第一个 Agent |
| [创建 Agent](partdocs/guides/create_agent.md) | Agent 开发教程 |
| [创建技能](partdocs/guides/create_skill.md) | 技能开发教程 |
| [部署指南](partdocs/guides/deployment.md) | 生产环境部署 |

---

## 🔄 版本路线图

### 当前版本 (v1.0.0.6) - 生产就绪

**核心模块完成度**: 100%

| 模块 | 完成度 | 状态 |
|------|--------|------|
| corekern 微内核 | 100% | ✅ IPC Binder、内存管理、任务调度、高精度时间 |
| coreloopthree 三层循环 | 100% | ✅ 认知层、行动层、记忆层 FFI |
| memoryrovol 四层记忆 | 100% | ✅ L1-L4 架构、FAISS 集成、遗忘曲线 |
| syscall 系统调用 | 100% | ✅ 任务/记忆/会话/可观测性/Agent |
| domes 安全穹顶 | 100% | ✅ 虚拟工位、权限裁决、净化、审计 |
| backs 守护进程 | 100% | ✅ llm_d/market_d/monit_d/sched_d/tool_d |
| tools 多语言 SDK | 90% | 🔄 Go/Python/Rust/TS |

### 短期目标 (2026 Q2-Q3)

- **v1.0.1.0**: 性能优化（FAISS 参数调优、LRU 缓存命中率提升）
- **v1.0.2.0**: 开发者工具（记忆可视化、执行追踪器）

### 中期规划 (2026 Q4-2027)

- **v1.0.3.0**: 生产就绪（端到端测试、安全审计）
- **v1.0.4.0**: 分布式支持（多节点集群、分布式记忆）
- **v1.0.5.0**: 智能化升级（自适应记忆、强化学习优化）

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
- **策略贡献**: 新的规划/协同/调度策略（通过 `tests/contract/` 验证）
- **文档完善**: 使用指南和技术文档
- **测试验证**: 功能测试和性能评估

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