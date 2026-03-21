# CoreLoopThree：三层一体核心运行时架构详解

**版本**: v1.0.0.5  
**最后更新**: 2026-03-21  
**状态**: 🟢 生产就绪  
**路径**: `atoms/coreloopthree/`

---

## 1. 概述

CoreLoopThree 是 AgentOS 的核心创新架构，通过将智能体运行时划分为三个正交且协同的层次，实现认知、行动和记忆的有机统一。

这三层通过可插拔的策略接口和标准化的数据流协作，形成完整的智能体生命周期管理闭环。

### 1.1 设计理念

```
┌─────────────────────────────────────────┐
│         认知层 (Cognition Layer)         │
│  • 意图理解 • 任务规划 • Agent 调度       │
│  • 模型协同 • 策略引擎 • 决策优化         │
└───────────────↓─────────────────────────┘
                ↓ 任务计划（DAG）
┌─────────────────────────────────────────┐
<!-- From data intelligence emerges. by spharx -->
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

- **正交分离**: 认知、行动、记忆三个层次职责清晰，互不耦合
- **策略可插拔**: 每层都提供策略接口，支持动态替换算法
- **标准化接口**: C ABI 兼容的 FFI 接口，支持多语言调用
- **可追溯性**: 完整的责任链追踪，从意图到执行到记忆的全链路记录
- **渐进式抽象**: 原始数据 → 特征向量 → 结构关系 → 抽象模式

### 1.3 学术基础

**认知科学理论支撑**:

1. **ACT-R 认知架构** [Anderson2007]:
   - 模块划分：陈述性记忆 vs 程序性记忆
   - 产生式系统：条件 - 动作规则
   - 目标栈管理：递归子目标处理
   
   AgentOS 对应实现:
   - 认知层 → 目标栈管理 + 产生式规则引擎
   - 记忆层 → 陈述性记忆（向量检索）+ 程序性记忆（技能库）
   - 行动层 → 产生式执行引擎

2. **SOAR 认知架构** [Newell1990]:
   - 问题空间假设：搜索 - 评估 - 选择循环
   - 组块机制：从实例中学习通用规则
   - 决策循环：提议 - 比较 - 选择
   
   AgentOS 对应实现:
   - DAG 任务规划 → 问题空间搜索
   - 记忆进化 → 组块学习机制
   - 策略可插拔 → 决策规则替换

3. **海马体记忆巩固理论** [McClelland1995]:
   - 快速编码（海马体）→ 慢速整合（新皮层）
   - 系统级巩固：记忆重组和抽象化
   - 睡眠中的重放机制
   
   AgentOS 对应实现:
   - L1 Raw Layer → 快速编码（原始经历）
   - L2/L3/L4 → 渐进式抽象和整合
   - 记忆进化委员会 → 类似睡眠重放的抽象机制

**系统论与控制论原理**:

1. **系统论第一性原理** [von Bertalanffy1968]:
   - 整体性：三层协同大于部分之和
   - 层次性：清晰的职责边界
   - 动态平衡：反馈调节维持稳态

2. **控制论负反馈原理** [Wiener1948]:
   - 误差检测：实际输出 vs 期望目标
   - 反馈调节：动态调整策略参数
   - 稳定性保证：Lyapunov 稳定性分析

**工程实践指导**:
- 正交分解降低耦合度（认知≠行动≠记忆）
- 策略接口提升可维护性（算法可替换）
- 责任链追踪增强可观测性（完整审计日志）

---

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
│  │  │ Registry │ │ Machine  │ │     the-loop   │      │ │
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
    char* raw_text;                 // 原始输入文本
    size_t raw_len;                 // 原始文本长度
    char* goal;                     // 提取的核心目标
    size_t goal_len;                // 目标长度
    uint32_t flags;                 // 标志位（紧急、复杂等）
    void* context;                  // 附加上下文
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
    char* task_id;                   // 任务 ID
    size_t id_len;                   // ID 长度
    char* agent_role;                // 需要的 Agent 角色
    size_t role_len;                 // 角色长度
    char** depends_on;               // 依赖的任务 ID 数组
    size_t depends_count;            // 依赖数量
    uint32_t timeout_ms;             // 超时时间
    uint8_t priority;                // 优先级
    void* input;                     // 输入数据
    void* output;                    // 输出数据
} agentos_task_node_t;

typedef struct agentos_task_plan {
    char* plan_id;                   // 计划 ID
    size_t id_len;                   // ID 长度
    agentos_task_node_t** nodes;     // 节点数组
    size_t node_count;               // 节点数量
    char** entry_points;             // 入口点节点 ID 数组
    size_t entry_count;              // 入口点数量
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
    char* record_id;                     // 记录唯一 ID
    size_t id_len;                        // ID 长度
    agentos_memory_type_t type;           // 类型（RAW/FEATURE/STRUCTURE/PATTERN）
    uint64_t timestamp_ns;                // 时间戳
    char* source_agent;                   // 来源 Agent ID
    size_t source_len;                    // 来源长度
    char* trace_id;                       // 关联追踪 ID
    size_t trace_len;                     // 追踪 ID 长度
    void* data;                           // 记忆数据
    size_t data_len;                      // 数据长度
    float importance;                     // 重要性（0-1）
    uint32_t access_count;                // 访问次数
} agentos_memory_record_t;
```

#### 5.1.2 记忆查询

```c
typedef struct agentos_memory_query {
    char* text;                            // 查询文本
    size_t text_len;                        // 文本长度
    uint64_t start_time;                     // 起始时间
    uint64_t end_time;                       // 结束时间
    char* source_agent;                      // 来源 Agent
    char* trace_id;                          // 关联追踪 ID
    uint32_t limit;                          // 返回数量上限
    uint32_t offset;                         // 偏移量
    uint8_t include_raw;                     // 是否包含原始数据
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
    uint32_t cognition_threads;         // 认知层线程数
    uint32_t execution_threads;         // 行动层线程数
    uint32_t memory_threads;            // 记忆层线程数
    uint32_t max_queued_tasks;          // 最大排队任务数
    uint32_t stats_interval_ms;         // 统计输出间隔
    agentos_plan_strategy_t* plan_strategy;      // 规划策略
    agentos_coordinator_strategy_t* coord_strategy; // 协同策略
    agentos_dispatching_strategy_t* disp_strategy; // 调度策略
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

## 7. 与其他模块的交互

### 7.1 与 core 的关系

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

### 7.2 与 memoryrovol 的关系

记忆层通过 FFI 接口调用 MemoryRovol:

```c
// memory/rov_ffi.h
#include "memoryrovol.h"

// 直接调用 MemoryRovol 各层接口
agentos_layer1_raw_write();
agentos_layer2_feature_add();
agentos_retrieval_attractor_network_retrieve();
```

### 7.3 与 syscall 的关系

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

---

## 8. 实现状态 (v1.0.0.5)

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
