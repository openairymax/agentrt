Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# CoreLoopThree：三层一体核心运行时架构详解

**版本**: Doc V1.6    
**最新**: 2026-03-25  
**状态**: 优化发布  
**说明**: 根据《AgentOS架构设计原则》深度优化，统一术语定义，强化模块交互关系，完善技术细节描述。  
**路径**: `atoms/coreloopthree/`  

---

## 1. 概述

CoreLoopThree 是 AgentOS 核心创新架构的具象体现，通过将智能体运行时划分为认知、行动、记忆三个正交且协同的层次，实现智能体生命周期管理的完整闭环。该架构严格遵循 **工程两论**（控制论反馈调节 + 系统工程层次分解）指导思想，是 **五维正交体系**（系统观、内核观、认知观、工程观、设计美学）在运行时层面的直接映射。

作为 AgentOS 架构设计原则的工程实践典范，CoreLoopThree 将 **双系统认知理论**（System 1 快思考与 System 2 慢思考）具象化为可执行的运行时层次，通过策略可插拔接口和标准化数据流，实现认知决策、行动执行与记忆积累的有机统一。

### 1.1 设计理念

```
┌─────────────────────────────────────────┐
│         认知层 (Cognition Layer)         │
│  • 意图理解 • 任务规划 • Agent 调度       │
│  • 模型协同 • 策略引擎 • 决策优化         │
└───────────────↓─────────────────────────┘
                ↓ 任务计划（DAG）
┌─────────────────────────────────────────┐
│         行动层 (Execution Layer)         │
│  • 任务执行 • 补偿事务 • 责任链追踪        │
│  • 执行单元 • 状态管理 • 异常处理         │
└───────────────↓─────────────────────────┘
                ↓ 执行记录（Trace）
┌─────────────────────────────────────────┐
│         记忆层 (Memory Layer)            │
│  • 记忆写入 • 查询检索 • 上下文挂载        │
│  • 进化抽象 • 遗忘裁剪 • FFI 接口         │
└─────────────────────────────────────────┘
```

### 1.2 核心价值

| 特性 | 说明 |
| :--- | :--- |
| **正交分离** | 认知、行动、记忆三个层次职责清晰，互不耦合 |
| **策略可插拔** | 每层都提供策略接口，支持动态替换算法 |
| **标准化接口** | C ABI 兼容的 FFI 接口，支持多语言调用 |
| **可追溯性** | 完整的责任链追踪，从意图到执行到记忆的全链路记录 |
| **渐进式抽象** | 原始数据 → 特征向量 → 结构关系 → 抽象模式 |

### 1.3 理论根基与设计演进

CoreLoopThree 架构的诞生源于对智能体运行时本质的深刻思考，其设计演进严格遵循 **工程两论** 指导原则，经历了从理论基础到工程实践的完整闭环：

#### 理论奠基：工程两论的直接映射
- **控制论反馈调节**：认知层的"思考-反思-调整"微循环形成负反馈回路，确保决策质量的持续优化
- **系统工程层次分解**：认知、行动、记忆三层正交分离，体现"总体设计部"思想，各层职责清晰、边界明确

#### 认知科学支撑：双系统理论工程化
- **System 1（快思考）** → 行动层的模式匹配与快速执行
- **System 2（慢思考）** → 认知层的深度分析与策略规划
- **记忆桥梁作用**：连接快慢思考，提供经验支撑与模式积累

#### 设计美学体现：简约至上的层次抽象
- **极简主义**：三层架构实现认知过程的完整抽象，避免冗余设计
- **极致细节**：每层提供策略可插拔接口，支持精细化的算法调优
- **人文关怀**：责任链追踪确保行为可解释，增强人机协作信任

**设计演进的关键洞察**：

- **决策与执行分离**：认知层只负责规划和调度，不执行具体任务，避免"既当裁判又当运动员"的利益冲突
- **记忆卷载机制**：借鉴 CNN 的层级特征提取思想，设计 L1-L4 四层记忆抽象，实现从原始数据到知识模式的渐进式抽象
- **自我纠错能力**：认知层引入"思考-反思-调整"微循环，通过双模型协同解决自我反思的理论极限问题

### 1.4 理论基础与原则映射

CoreLoopThree 架构的理论基础深刻体现了 AgentOS **五维正交体系** 的设计思想，将学术理论与工程实践紧密结合：

#### 认知观维度：双系统理论的工程实现
- **ACT-R/SOAR 认知架构** → **认知层**的目标栈管理与产生式规则引擎
- **海马体-新皮层记忆理论** → **记忆层**的 L1-L4 渐进式抽象机制
- **双系统认知理论** → **System 1**（行动层快速执行）与 **System 2**（认知层深度分析）的协同工作

#### 系统观维度：工程两论的核心实践
- **控制论负反馈原理** → 认知层"思考-反思-调整"微循环，形成决策质量优化的负反馈回路
- **系统工程层次分解** → 认知、行动、记忆三层正交分离，体现"总体设计部"思想
- **系统论整体性原则** → 三层协同效应大于独立功能之和，实现智能体运行时完整性

#### 工程观维度：设计原则的具象表达
- **正交分离原则** → 认知≠行动≠记忆的清晰职责边界，降低模块耦合度
- **策略可插拔原则** → 每层提供标准化策略接口，支持算法动态替换与升级
- **可观测性原则** → 完整的责任链追踪，实现从意图到执行到记忆的全链路审计

#### 设计美学维度：简约与极致的平衡
- **极简主义** → 三层架构实现认知过程的完整抽象，避免过度设计
- **极致细节** → 策略接口的精细设计，支持算法层面的极致优化
- **人文关怀** → 责任链追踪增强行为可解释性，建立人机协作信任基础

---

## 1.5 模块集成与系统交互

CoreLoopThree 作为 AgentOS 的核心运行时，与系统中其他关键模块形成紧密的协同关系，共同构成完整的智能体生态系统：

### 与微内核 (Microkernel) 的关系
CoreLoopThree 运行在微内核提供的抽象层之上：
- **任务调度** → 通过 `syscall` 层调用微内核的 `Task Scheduler`，实现任务的加权轮询调度
- **内存管理** → 利用微内核 `Memory Manager` 的智能指针和内存池机制，优化运行时内存使用
- **进程通信** → 基于 `IPC Binder` 实现三层间的高效数据传递，支持零拷贝传输

### 与系统调用 (Syscall) 层的关系
CoreLoopThree 通过统一的 `syscall` 接口与底层服务交互：
- **任务提交** → `sys_task_submit()` 将认知层生成的 DAG 任务图提交到系统调度队列
- **记忆操作** → `sys_memory_write/search/get()` 提供对 MemoryRovol 四层记忆的统一访问
- **可观测性** → `sys_telemetry_metrics/traces()` 实现三层运行时的全面监控与追踪

### 与记忆卷载系统 (MemoryRovol) 的关系
CoreLoopThree 的记忆层本质上是 MemoryRovol 的高级封装：
- **记忆写入** → 行动层的执行结果通过 FFI 接口写入 MemoryRovol L1 原始卷
- **记忆检索** → 认知层通过吸引子网络从 MemoryRovol L2-L4 层检索相关经验
- **记忆进化** → 记忆进化委员会定期触发 MemoryRovol L4 模式层的持久同调分析

### 与日志系统 (Logging System) 的关系
三层运行时通过统一的日志接口实现全链路可观测性：
- **结构化日志** → 使用 `AGENTOS_LOG_*` 宏记录各层关键事件，格式统一、内容结构化
- **跨语言追踪** → 通过 `trace_id` 实现 C/Python/Go/Rust 多语言日志的完整关联
- **动态反馈调节** → 日志系统根据运行时负载自动调整采样率，形成控制论负反馈

### 与进程通信 (IPC) 系统的关系
CoreLoopThree 三层间通信基于 IPC Binder 的高效机制：
- **跨层通信** → 认知层→行动层→记忆层的数据流通过共享内存通道传递
- **服务发现** → 利用 IPC 服务注册机制动态发现可用的执行单元和记忆服务
- **流量控制** → 基于信号量的同步机制确保三层间数据流的平衡与稳定

### 集成架构视图
```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   CoreLoopThree  │    │    Microkernel   │    │  MemoryRovol    │
│  • Cognition    │←→│  • Task Scheduler │←→│  • L1-L4 Layers │
│  • Execution    │    │  • Memory Manager│    │  • Retrieval    │
│  • Memory       │    │  • IPC Binder    │    │  • Forgetting   │
└─────────────────┘    └─────────────────┘    └─────────────────┘
         ↓                       ↓                       ↓
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│    Syscall      │    │   Logging       │    │      IPC        │
│  • Task API     │    │  • Structured   │    │  • Channels     │
│  • Memory API   │    │  • Cross-Lang   │    │  • Semaphores   │
│  • Telemetry    │    │  • Adaptive     │    │  • Registry     │
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

## 2. 系统架构

### 2.1 整体架构图

```
┌─────────────────────────────────────────────────────────┐
│                    CoreLoopThree                        │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  ┌────────────────────────────────────────────────────┐ │
│  │              认知层 (Cognition)                     │ │
│  │  ┌──────────┐  ┌──────────┐  ┌─────────────────┐   │ │
│  │  │  Intent  │  │   Plan   │  │  Coordinator    │   │ │
│  │  │  Parser  │→ │ Generator│→ │  & Dispatcher   │   │ │
│  │  └──────────┘  └──────────┘  └─────────────────┘   │ │
│  │       ↑             ↑               ↓              │ │
│  │       │             │               │              │ │
│  │  ┌────┴─────┐ ┌────┴─────┐ ┌───────┴────────┐      │ │
│  │  │ Strategy │ │ Strategy │ │   Strategy     │      │ │
│  │  │  Plan    │ │ Coord    │ │   Dispatch     │      │ │
│  │  └──────────┘ └──────────┘ └────────────────┘      │ │
│  └────────────────────────────────────────────────────┘ │
│                           ↓ 任务计划（DAG）              │
│  ┌────────────────────────────────────────────────────┐ │
│  │              行动层 (Execution)                     │ │
│  │  ┌──────────┐  ┌──────────┐  ┌─────────────────┐   │ │
│  │  │   Task   │  │ Execution│  │  Compensation   │   │ │
│  │  │  Manager │→ │  Engine  │→ │    Manager      │   │ │
│  │  └──────────┘  └──────────┘  └─────────────────┘   │ │
│  │       ↑             ↑               ↓              │ │
│  │       │             │               │              │ │
│  │  ┌────┴─────┐ ┌────┴─────┐ ┌───────┴────────┐      │ │
│  │  │  Unit    │ │  State   │ │    Human-in-   │      │ │
│  │  │ Registry │ │  Machine  │ │     the-loop   │      │ │
│  │  └──────────┘ └──────────┘ └────────────────┘      │ │
│  └────────────────────────────────────────────────────┘ │
│                           ↓ 执行记录（Trace）            │
│  ┌────────────────────────────────────────────────────┐ │
│  │               记忆层 (Memory)                       │ │
│  │  ┌──────────┐  ┌──────────┐  ┌─────────────────┐   │ │
│  │  │  Memory  │  │ Retrieval│  │   Forgetting    │   │ │
│  │  │  Engine  │← │ Attractor│← │     Engine      │   │ │
│  │  └──────────┘  └──────────┘  └─────────────────┘   │ │
│  │       ↓             ↓               ↓              │ │
│  │  ┌────┴─────┐ ┌────┴─────┐ ┌───────┴────────┐      │ │
│  │  │  L1-L4   │ │  Mount   │ │    Evolve      │      │ │
│  │  │  Layers  │ │ Context  │ │    Committee   │      │ │
│  │  └──────────┘ └──────────┘ └────────────────┘      │ │
│  └────────────────────────────────────────────────────┘ │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

### 2.2 目录结构

```
atoms/coreloopthree/
├── CMakeLists.txt                 # 顶层构建文件
├── README.md                      # 模块说明
├── include/                       # 公共头文件
│   ├── cognition.h               # 认知层接口定义
│   ├── execution.h               # 行动层接口定义
│   ├── memory.h                  # 记忆层接口定义
│   └── loop.h                    # 三层闭环主接口
└── src/                           # 源代码实现
    ├── cognition/                # 认知层实现
    │   ├── engine.c             # 认知引擎主逻辑
    │   ├── intent_parser.c      # 意图解析器
    │   ├── plan_generator.c     # 规划生成器
    │   ├── coordinator.c        # 协同器实现
    │   ├── dispatcher.c         # 调度器实现
    │   └── strategies/          # 策略实现
    │       ├── default_plan.c   # 默认规划策略
    │       └── weighted_dispatch.c # 加权调度策略
    ├── execution/               # 行动层实现
    │   ├── engine.c            # 执行引擎主逻辑
    │   ├── task_manager.c      # 任务管理器
    │   ├── compensation.c      # 补偿事务
    │   └── units/              # 执行单元
    │       └── weighted.c      # 加权调度单元
    └── memory/                  # 记忆层实现
        ├── engine.c            # 记忆引擎封装
        └── rov_ffi.h           # MemoryRovol FFI 接口
```

---

## 3. 认知层 (Cognition Layer)

### 3.1 核心组件

#### 3.1.1 意图理解引擎

**数据结构**:
```c
typedef struct agentos_intent {
    char* intent_raw_text;          // 原始输入文本
    size_t intent_raw_len;          // 原始文本长度
    char* intent_goal;              // 提取的核心目标
    size_t intent_goal_len;         // 目标长度
    uint32_t intent_flags;          // 标志位（紧急、复杂等）
    void* intent_context;           // 附加上下文
} agentos_intent_t;
```

**功能**:
- 解析用户自然语言输入
- 识别真实意图和目标
- 标记意图特征（紧急度、复杂度等）
- 提取上下文信息

#### 3.1.2 任务规划器

**数据结构**:
```c
typedef struct agentos_task_node {
    char* task_node_id;              // 任务 ID
    size_t task_node_id_len;         // ID 长度
    char* task_node_agent_role;      // 需要的 Agent 角色
    size_t task_node_role_len;       // 角色长度
    char** task_node_depends_on;     // 依赖的任务 ID 数组
    size_t task_node_depends_count;  // 依赖数量
    uint32_t task_node_timeout_ms;   // 超时时间
    uint8_t task_node_priority;      // 优先级
    void* task_node_input;           // 输入数据
    void* task_node_output;          // 输出数据
} agentos_task_node_t;

typedef struct agentos_task_plan {
    char* task_plan_id;              // 计划 ID
    size_t task_plan_id_len;         // ID 长度
    agentos_task_node_t** task_plan_nodes;  // 节点数组
    size_t task_plan_node_count;     // 节点数量
    char** task_plan_entry_points;   // 入口点节点 ID 数组
    size_t task_plan_entry_count;    // 入口点数量
} agentos_task_plan_t;
```

**功能**:
- 基于意图生成 DAG 任务图
- 管理任务依赖关系
- 识别入口点任务
- 分配 Agent 角色和资源

#### 3.1.3 策略接口

**规划策略**:
```c
typedef agentos_error_t (*agentos_plan_func_t)(
    const agentos_intent_t* intent,
    void* context,
    agentos_task_plan_t** out_plan);
```

**协同策略**:
```c
typedef agentos_error_t (*agentos_coordinate_func_t)(
    const char** prompts,
    size_t count,
    void* context,
    char** out_result);
```

**调度策略**:
```c
typedef agentos_error_t (*agentos_dispatch_func_t)(
    const agentos_task_node_t* task,
    const void** candidates,
    size_t count,
    void* context,
    char** out_agent_id);
```

### 3.2 工作流程

```
用户输入
   ↓
[意图解析] → agentos_intent_t
   ↓
[规划策略] → agentos_task_plan_t (DAG)
   ↓
[协同策略] → 多模型协调
   ↓
[调度策略] → 选择最佳 Agent
   ↓
行动层
```

### 3.3 API 接口

#### 创建认知引擎
```c
agentos_error_t agentos_cognition_create(
    agentos_plan_strategy_t* plan_strategy,
    agentos_coordinator_strategy_t* coord_strategy,
    agentos_dispatching_strategy_t* disp_strategy,
    agentos_cognition_engine_t** out_engine);
```

#### 处理用户输入
```c
agentos_error_t agentos_cognition_process(
    agentos_cognition_engine_t* engine,
    const char* input,
    size_t input_len,
    agentos_task_plan_t** out_plan);
```

---

## 4. 行动层 (Execution Layer)

### 4.1 核心组件

#### 4.1.1 任务状态机

```c
typedef enum {
    TASK_STATUS_PENDING = 0,
    TASK_STATUS_RUNNING,
    TASK_STATUS_SUCCEEDED,
    TASK_STATUS_FAILED,
    TASK_STATUS_CANCELLED,
    TASK_STATUS_RETRYING
} agentos_task_status_t;
```

#### 4.1.2 执行单元

```c
struct agentos_execution_unit {
    void* data;
    agentos_error_t (*execute)(
        agentos_execution_unit_t* unit,
        const void* input,
        void** out_output);
    void (*destroy)(agentos_execution_unit_t* unit);
    const char* (*get_metadata)(agentos_execution_unit_t* unit);
};
```

#### 4.1.3 补偿事务

**功能**:
- 注册可补偿操作
- 自动回滚机制
- 人工介入队列
- 分级异常处理

### 4.2 工作流程

```
接收任务计划（DAG）
   ↓
[任务提交] → agentos_task_t
   ↓
[并发控制] → 线程池调度
   ↓
[执行单元] → execute()
   ↓
[状态追踪] → Pending→Running→Succeeded/Failed
   ↓
[补偿事务] → 失败时回滚
   ↓
输出结果 + Trace ID
```

### 4.3 API 接口

#### 注册执行单元
```c
agentos_error_t agentos_execution_register_unit(
    agentos_execution_engine_t* engine,
    const char* unit_id,
    agentos_execution_unit_t* unit);
```

#### 提交任务
```c
agentos_error_t agentos_execution_submit(
    agentos_execution_engine_t* engine,
    const agentos_task_t* task,
    char** out_task_id);
```

#### 等待完成
```c
agentos_error_t agentos_execution_wait(
    agentos_execution_engine_t* engine,
    const char* task_id,
    uint32_t timeout_ms,
    agentos_task_t** out_result);
```

---

## 5. 记忆层 (Memory Layer)

### 5.1 核心组件

#### 5.1.1 记忆记录

```c
typedef struct agentos_memory_record {
    char* memory_record_id;                     // 记录唯一 ID
    size_t memory_record_id_len;                // ID 长度
    agentos_memory_type_t memory_record_type;   // 类型（RAW/FEATURE/STRUCTURE/PATTERN）
    uint64_t memory_record_timestamp_ns;        // 时间戳
    char* memory_record_source_agent;           // 来源 Agent ID
    size_t memory_record_source_len;            // 来源长度
    char* memory_record_trace_id;               // 关联追踪 ID
    size_t memory_record_trace_len;             // 追踪 ID 长度
    void* memory_record_data;                   // 记忆数据
    size_t memory_record_data_len;              // 数据长度
    float memory_record_importance;             // 重要性（0-1）
    uint32_t memory_record_access_count;        // 访问次数
} agentos_memory_record_t;
```

#### 5.1.2 记忆查询

```c
typedef struct agentos_memory_query {
    char* memory_query_text;                 // 查询文本
    size_t memory_query_text_len;            // 文本长度
    uint64_t memory_query_start_time;        // 起始时间
    uint64_t memory_query_end_time;          // 结束时间
    char* memory_query_source_agent;         // 来源 Agent
    char* memory_query_trace_id;             // 关联追踪 ID
    uint32_t memory_query_limit;             // 返回数量上限
    uint32_t memory_query_offset;            // 偏移量
    uint8_t memory_query_include_raw;        // 是否包含原始数据
} agentos_memory_query_t;
```

### 5.2 与 MemoryRovol 的关系

记忆层通过 FFI 接口 (`rov_ffi.h`) 封装 MemoryRovol 四层架构:

```
记忆层 (Memory Layer)
   ↓ FFI 调用
MemoryRovol
   ├─ L1 Raw: 文件系统存储
   ├─ L2 Feature: FAISS 向量索引
   ├─ L3 Structure: 绑定算子
   └─ L4 Pattern: 持久同调分析
```

### 5.3 API 接口

#### 写入记忆
```c
agentos_error_t agentos_memory_write(
    agentos_memory_engine_t* engine,
    const agentos_memory_record_t* record,
    char** out_record_id);
```

#### 查询记忆
```c
agentos_error_t agentos_memory_query(
    agentos_memory_engine_t* engine,
    const agentos_memory_query_t* query,
    agentos_memory_result_t** out_result);
```

#### 挂载上下文
```c
agentos_error_t agentos_memory_mount(
    agentos_memory_engine_t* engine,
    const char* record_id,
    const char* context);
```

---

## 6. 三层闭环 (Core Loop)

### 6.1 主循环接口

```c
typedef struct agentos_loop_config {
    uint32_t loop_config_cognition_threads;   // 认知层线程数
    uint32_t loop_config_execution_threads;   // 行动层线程数
    uint32_t loop_config_memory_threads;      // 记忆层线程数
    uint32_t loop_config_max_queued_tasks;    // 最大排队任务数
    uint32_t loop_config_stats_interval_ms;   // 统计输出间隔
    agentos_plan_strategy_t* loop_config_plan_strategy;      // 规划策略
    agentos_coordinator_strategy_t* loop_config_coord_strategy; // 协同策略
    agentos_dispatching_strategy_t* loop_config_disp_strategy; // 调度策略
} agentos_loop_config_t;
```

### 6.2 完整流程

```
1. 用户提交自然语言任务
   ↓
2. 认知层处理
   - 解析意图
   - 生成 DAG 计划
   - 调度 Agent
   ↓
3. 行动层执行
   - 提交任务到线程池
   - 执行单元处理
   - 追踪责任链
   ↓
4. 记忆层记录
   - 写入执行 Trace
   - 更新访问计数
   - 触发进化（可选）
   ↓
5. 返回结果给用户
```

### 6.3 API 接口

#### 创建核心循环
```c
agentos_error_t agentos_loop_create(
    const agentos_loop_config_t* config,
    agentos_core_loop_t** out_loop);
```

#### 提交任务
```c
agentos_error_t agentos_loop_submit(
    agentos_core_loop_t* loop,
    const char* input,
    size_t input_len,
    char** out_task_id);
```

#### 等待结果
```c
agentos_error_t agentos_loop_wait(
    agentos_core_loop_t* loop,
    const char* task_id,
    uint32_t timeout_ms,
    char** out_result,
    size_t* out_result_len);
```

---

## 7. 案例分析

### 7.1 案例：用户任务的完整生命周期

**场景**：用户输入"帮我分析这份财报并生成摘要"

**执行流程**：

```
[用户输入] "帮我分析这份财报并生成摘要"
    ↓
[认知层]
    ├── 意图解析：识别为"文档分析+摘要生成"任务
    ├── 任务规划：生成 DAG（读取→分析→生成摘要）
    │   ┌─────────────┐
    │   │ 读取财报文件 │ (入口点)
    │   └──────┬──────┘
    │          ↓
    │   ┌─────────────┐
    │   │ 分析财务数据 │
    │   └──────┬──────┘
    │          ↓
    │   ┌─────────────┐
    │   │ 生成摘要报告 │
    │   └─────────────┘
    ├── Agent 调度：选择"文档分析 Agent"和"摘要生成 Agent"
    ↓
[行动层]
    ├── 任务提交：创建 3 个任务实例
    ├── 并发控制：线程池调度（最大并发数=4）
    ├── 执行单元：
    │   ├── Task-1: 文件读取（耗时 50ms）
    │   ├── Task-2: LLM 分析（耗时 2000ms）
    │   └── Task-3: 摘要生成（耗时 800ms）
    ├── 状态追踪：Pending → Running → Succeeded
    ├── 补偿事务：如果 Task-2 失败，回退到简化分析
    ↓
[记忆层]
    ├── 记忆写入：将本次交互记录到 L1 原始卷
    │   {
    │     "timestamp": "2026-03-24T10:30:00Z",
    │     "event_type": "task_execution",
    │     "task": "财报分析+摘要生成",
    │     "duration_ms": 2850,
    │     "success": true
    │   }
    ├── 上下文挂载：检索相关的历史记忆（如用户偏好）
    ├── 进化触发：如果发现新模式，触发 L4 模式挖掘
    ↓
[返回结果]
    {
      "summary": "营收增长15%，净利润增长20%，现金流健康...",
      "key_metrics": {...},
      "trace_id": "trace_20260324_001"
    }
```

**性能分析**：

| 指标 | 传统单智能体 | CoreLoopThree | 改进 |
|------|-------------|---------------|------|
| Token 消耗 | 约 15,000 | 约 6,000 | 节省 60% |
| 执行时间 | 约 5s | 约 2.85s | 提升 43% |
| 可追溯性 | 低 | 高（完整 Trace） | 质的飞跃 |
| 容错能力 | 低 | 高（补偿事务） | 显著提升 |

### 7.2 案例：复杂任务的多 Agent 协作

**场景**：用户输入"帮我完成市场调研报告，包括数据收集、分析和可视化"

**执行流程**：

```
[用户输入]
    ↓
[认知层]
    ├── 意图解析：识别为"市场调研"复杂任务
    ├── 任务规划：生成复杂 DAG（10+ 节点）
    │   ┌──────────────┐
    │   │ 收集市场数据  │ ←─┐
    │   └──────┬───────┘   │
    │          ↓           │ 并行
    │   ┌──────────────┐   │
    │   │ 收集竞品信息  │ ──┘
    │   └──────┬───────┘
    │          ↓
    │   ┌──────────────┐
    │   │ 数据清洗处理  │
    │   └──────┬───────┘
    │          ↓
    │   ┌──────┴───────┐
    │   ↓              ↓
    │ ┌────────┐  ┌────────┐
    │ │定量分析│  │定性分析│
    │ └───┬────┘  └───┬────┘
    │     └─────┬─────┘
    │           ↓
    │   ┌──────────────┐
    │   │ 生成可视化图表│
    │   └──────┬───────┘
    │          ↓
    │   ┌──────────────┐
    │   │ 撰写调研报告  │
    │   └──────────────┘
    ├── Agent 调度：选择 5 个专业 Agent
    │   ├── 数据收集 Agent
    │   ├── 数据分析 Agent
    │   ├── 可视化 Agent
    │   ├── 报告撰写 Agent
    │   └── 质量审核 Agent
    ↓
[行动层]
    ├── 并发执行：同时运行多个独立任务
    ├── 依赖管理：确保任务按 DAG 顺序执行
    ├── 异常处理：某个 Agent 失败时的降级策略
    ↓
[记忆层]
    ├── 记忆写入：记录完整的执行轨迹
    ├── 模式识别：识别"市场调研"任务的稳定模式
    ├── 规则生成：生成可复用的"市场调研流程"规则
```

**关键指标**：

- **任务节点数**：12 个
- **Agent 数量**：5 个
- **总执行时间**：约 45 秒
- **Token 消耗**：约 25,000（相比单智能体节省 50%）
- **成功率**：95%（有补偿事务保障）

---

## 8. 与其他模块的交互

### 8.1 与 core 的关系

CoreLoopThree 通过 syscall 层调用 core 提供的基础服务:

```
CoreLoopThree
   ↓
Syscall 层
   ↓
Core (微内核)
   ├─ IPC Binder: 进程间通信
   ├─ Memory: 内存管理
   ├─ Task: 任务调度
   └─ Time: 时间服务
```

**交互示例**：

```c
// 认知层提交任务
agentos_error_t err = sys_task_submit(plan, &task_id);

// 行动层等待任务完成
agentos_task_t* result;
err = sys_task_wait(task_id, 5000, &result);

// 记忆层写入记忆
err = sys_memory_write(record, &record_id);
```

### 8.2 与 memoryrovol 的关系

记忆层通过 FFI 接口调用 MemoryRovol:

```c
// memory/rov_ffi.h
#include "memoryrovol.h"

// 直接调用 MemoryRovol 各层接口
agentos_layer1_raw_write();
agentos_layer2_feature_add();
agentos_retrieval_attractor_network_retrieve();
```

**四层记忆调用**：

```
记忆层 API
   ↓ FFI 封装
MemoryRovol 四层架构
   ├─ L1 Raw: agentos_layer1_raw_write()
   ├─ L2 Feature: agentos_layer2_feature_add()
   ├─ L3 Structure: agentos_layer3_structure_encode()
   └─ L4 Pattern: agentos_layer4_pattern_mine()
```

### 8.3 与 syscall 的关系

认知层和行动层通过系统调用接口与内核交互:

```c
// syscall/task.c
extern agentos_cognition_engine_t* g_cognition;
extern agentos_execution_engine_t* g_execution;

void agentos_sys_init(void* cognition, void* execution, void* memory) {
    g_cognition = (agentos_cognition_engine_t*)cognition;
    g_execution = (agentos_execution_engine_t*)execution;
}
```

**系统调用映射**：

| CoreLoopThree API | Syscall | 内核服务 |
|-------------------|---------|---------|
| agentos_cognition_process() | sys_task_submit() | Task Scheduler |
| agentos_execution_submit() | sys_task_submit() | Task Scheduler |
| agentos_memory_write() | sys_memory_write() | Memory Manager |
| agentos_memory_query() | sys_memory_query() | Memory Manager |

---

## 8. 实现状态与性能基准

### 8.1 已完成功能

#### 认知层 (90%)
- ✅ 意图理解引擎基础框架
- ✅ 任务规划器（DAG 生成）
- ✅ Agent 调度器（加权轮询）
- ✅ 规划策略接口
- ✅ 协同策略接口
- ✅ 调度策略接口
- 🔲 强化学习决策优化（规划中）

#### 行动层 (85%)
- ✅ 执行引擎基础框架
- ✅ 任务状态机管理
- ✅ 补偿事务框架
- ✅ 执行单元注册表
- ✅ 责任链追踪
- 🔲 完整异常处理（开发中）

#### 记忆层 (80%)
- ✅ MemoryRovol FFI 封装
- ✅ 记忆写入接口
- ✅ 记忆查询接口
- ✅ 上下文挂载机制
- 🔲 记忆进化触发（部分实现）

### 8.2 性能指标

**测试环境**: Intel i7-12700K, 32GB RAM, Linux 6.5

| 指标 | 数值 | 测试条件 |
| :--- | :--- | :--- |
| **意图解析延迟** | < 50ms | 简单意图 |
| **任务规划速度** | 100+ 节点/秒 | DAG 生成 |
| **Agent 调度延迟** | < 5ms | 加权轮询 |
| **任务执行吞吐** | 1000+ 任务/秒 | 并发执行 |
| **记忆写入延迟** | < 10ms | 同步写入 |
| **记忆检索延迟** | < 50ms | Top-100 重排序 |

---

## 9. 开发指南

### 9.1 自定义规划策略

```c
typedef struct my_plan_data {
    // 自定义数据
} my_plan_data_t;

agentos_error_t my_plan_strategy(
    const agentos_intent_t* intent,
    void* context,
    agentos_task_plan_t** out_plan) {
    
    my_plan_data_t* data = (my_plan_data_t*)context;
    // 实现规划逻辑
    
    return AGENTOS_SUCCESS;
}

void my_plan_destroy(agentos_plan_strategy_t* strategy) {
    if (strategy->data) {
        free(strategy->data);
    }
}

// 创建策略
agentos_plan_strategy_t* strategy = malloc(sizeof(agentos_plan_strategy_t));
strategy->plan = my_plan_strategy;
strategy->destroy = my_plan_destroy;
strategy->data = NULL;
```

### 9.2 注册执行单元

```c
typedef struct my_unit {
    agentos_execution_unit_t base;
    // 私有数据
} my_unit_t;

agentos_error_t my_execute(
    agentos_execution_unit_t* unit,
    const void* input,
    void** out_output) {
    
    my_unit_t* my_unit = (my_unit_t*)unit;
    // 实现执行逻辑
    
    return AGENTOS_SUCCESS;
}

// 注册
agentos_execution_register_unit(engine, "my_unit", &my_unit->base);
```

---

## 10. 故障排查

### 10.1 常见问题

#### 问题：任务规划失败
**症状**: `agentos_cognition_process()` 返回错误  
**排查**:
1. 检查意图解析是否正确
2. 验证规划策略是否注册
3. 查看日志中的详细错误信息

#### 问题：执行单元未调用
**症状**: 任务一直处于 PENDING 状态  
**排查**:
1. 确认执行单元已注册
2. 检查任务依赖关系
3. 验证并发线程数配置

### 10.2 调试技巧

- 启用 Debug 日志级别
- 使用 `agentos_cognition_stats()` 查看统计信息
- 使用 `agentos_execution_health_check()` 检查健康状态

---

## 11. 参考资料

- [README.md](../../README.md) - 项目总览
- [memoryrovol.md](memoryrovol.md) - MemoryRovol 架构详解
- [syscall.md](syscall.md) - 系统调用接口文档
- [cognition.h](../include/cognition.h) - 认知层头文件
- [execution.h](../include/execution.h) - 行动层头文件
- [memory.h](../include/memory.h) - 记忆层头文件
- [loop.h](../include/loop.h) - 核心循环接口

---

<div align="center">

**© 2026 SPHARX Ltd. All Rights Reserved.**

*From data intelligence emerges*

</div>