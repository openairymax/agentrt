# CoreLoopThree — 核心运行时

`agentos/atoms/coreloopthree/`

**版本**: v0.1.0 | **API 版本**: 1.0.0

---

## 概述

CoreLoopThree 是 AgentOS 的核心运行时引擎，采用创新的**"三循环"架构**实现智能体的感知-思考-行动闭环。该设计受心理学"双系统理论"（System 1 / System 2）启发，将快速直觉思维与慢速理性思维融入同一架构，并通过学习循环实现持续进化。

CoreLoopThree 以 C11 标准实现，提供三个并行运行的引擎——认知引擎（Cognition Engine）、执行引擎（Execution Engine）和记忆引擎（Memory Engine），由主循环（Core Loop）统一协调。支持多种可插拔策略（规划策略、协同策略、调度策略），内置补偿事务机制和检查点恢复能力，确保任务执行的可靠性和持久性。

---

## 设计理念

CoreLoopThree 的核心思想是将智能体的运行过程建模为三个相互耦合的循环：

| 循环 | 对应系统 | 功能 | 频率 |
|------|----------|------|------|
| 认知循环 (Cognition) | System 2 | 环境感知、意图解析、决策规划 | 低频 |
| 执行循环 (Execution) | System 1 | 快速响应、指令执行、状态反馈 | 高频 |
| 学习循环 (Memory/Evolution) | Meta | 经验积累、模式发现、策略优化 | 后台 |

三个循环并行运行、相互协作、持续演进，构成智能体的完整生命活动。

---

## 目录结构

```
coreloopthree/
├── include/                               # 公共头文件
│   ├── loop.h                             # 主循环接口（创建/运行/提交/等待/检查点）
│   ├── cognition.h                        # 认知引擎接口（意图/计划/策略）
│   ├── execution.h                        # 执行引擎接口（任务/执行单元/补偿事务）
│   ├── memory.h                           # 记忆引擎接口（写入/查询/挂载/进化）
│   ├── compensation.h                     # 补偿事务接口（注册/补偿/人工队列）
│   ├── cognitive_evolution.h              # 认知进化系统（经验/策略/模式/知识迁移）
│   ├── multi_agent_collaboration.h        # 多智能体协作框架
│   ├── agent_registry.h                   # Agent 注册中心
│   ├── browser.h                          # 浏览器执行单元（CDP 自动化）
│   ├── config_loader.h                    # 配置加载器
│   ├── id_utils.h                         # ID 生成工具
│   └── error_utils.h                      # 错误处理工具
├── src/                                   # 运行时实现
│   ├── loop.c                             # 主循环实现
│   ├── config_loader.c                    # 配置加载实现
│   ├── cognition/                         # 认知引擎子模块
│   │   ├── engine.c                       # 认知引擎核心
│   │   ├── intent_parser.c                # 意图解析器
│   │   ├── intent_classifier.c            # 意图分类器
│   │   ├── intent_utils.c                 # 意图工具函数
│   │   ├── context_processor.c            # 上下文处理器
│   │   ├── entity_extractor.c             # 实体提取器
│   │   ├── semantic_unit.c                # 语义单元
│   │   ├── thinking_chain.c               # 思维链
│   │   ├── metacognition.c                # 元认知
│   │   ├── confidence_calibrator.c        # 置信度校准器
│   │   ├── stream_critic.c                # 流式评审
│   │   ├── delegate.c                     # 委派调度
│   │   ├── parallel_dispatcher.c          # 并行分发器
│   │   ├── triple_coordinator.c           # 三重协调器
│   │   ├── cognitive_evolution.c          # 认知进化实现
│   │   ├── multi_agent_collaboration.c    # 多智能体协作实现
│   │   ├── planner/                       # 规划策略
│   │   │   ├── reactive.c                 # 反应式规划
│   │   │   ├── reflective.c               # 反思式规划
│   │   │   ├── hierarchical.c             # 层次式规划
│   │   │   └── ml_planner.c               # ML 规划器
│   │   ├── dispatcher/                    # 调度策略
│   │   │   ├── round_robin.c              # 轮询调度
│   │   │   ├── priority.c                 # 优先级调度
│   │   │   ├── weighted.c                 # 加权调度
│   │   │   └── ml_based.c                 # ML 调度
│   │   └── coordinator/                   # 协同策略
│   │       ├── majority.c                 # 多数投票
│   │       ├── weighted.c                 # 加权协同
│   │       ├── arbiter.c                  # 仲裁者
│   │       ├── dual_model.c               # 双模型协同
│   │       └── coordinator_adapter.c      # 协同适配器
│   ├── execution/                         # 执行引擎子模块
│   │   ├── engine.c                       # 执行引擎核心
│   │   ├── registry.c                     # 执行单元注册表
│   │   ├── trace.c                        # 执行追踪
│   │   ├── compensation.c                 # 补偿事务实现
│   │   └── units/                         # 内置执行单元
│   │       ├── tool.c                     # 工具调用单元
│   │       ├── api.c                      # API 调用单元
│   │       ├── code.c                     # 代码执行单元
│   │       ├── file.c                     # 文件操作单元
│   │       ├── shell.c                    # Shell 命令单元
│   │       ├── browser.c                  # 浏览器自动化单元
│   │       ├── db.c                       # 数据库操作单元
│   │       └── weighted.c                 # 加权选择单元
│   ├── memory/                            # 记忆引擎子模块
│   │   ├── engine.c                       # 记忆引擎核心
│   │   └── memory_service.c              # 记忆服务层
│   └── utils/                             # 工具函数
│       ├── id_utils.c                     # ID 生成（UUID 等）
│       └── error_utils.c                  # 错误处理工具
├── tests/                                 # 单元测试
│   ├── unit/
│   │   ├── test_loop.c                    # 主循环测试
│   │   ├── test_cognition.c               # 认知引擎测试
│   │   ├── test_execution.c               # 执行引擎测试
│   │   ├── test_memory.c                  # 记忆引擎测试
│   │   ├── test_intent_parser.c           # 意图解析测试
│   │   ├── test_compensation.c            # 补偿事务测试
│   │   ├── test_triple_coordinator.c      # 三重协调器测试
│   │   ├── test_parallel_dispatcher.c     # 并行分发器测试
│   │   ├── test_context_processor.c       # 上下文处理器测试
│   │   ├── test_cognitive_evolution.c     # 认知进化测试
│   │   ├── test_multi_agent.c             # 多智能体协作测试
│   │   ├── test_browser.c                 # 浏览器单元测试
│   │   ├── test_performance_benchmark.c   # 性能基准测试
│   │   └── test_cl3_stubs.c              # 桩函数测试
│   └── CMakeLists.txt
└── CMakeLists.txt
```

---

## 架构总览

```
┌─────────────────────────────────────────────────────┐
│               CoreLoopThree Runtime                   │
│                                                       │
│  ┌─────────────────────────────────────────────┐     │
│  │         Cognition Loop (认知循环)              │     │
│  │  ┌─────────┐  ┌─────────┐  ┌─────────┐     │     │
│  │  │ 感知     │→│ 理解     │→│ 决策     │     │     │
│  │  │ Perceive │ │ Comprehend│ │ Decide   │     │     │
│  │  └─────────┘  └─────────┘  └────┬────┘     │     │
│  └──────────────────────────────────┼──────────┘     │
│                                     │                 │
│  ┌──────────────────────────────────┼──────────┐     │
│  │         Execution Loop (执行循环)  │          │     │
│  │  ┌─────────┐  ┌─────────┐  ┌────▼────┐     │     │
│  │  │ 规划     │  │ 执行     │  │ 反馈     │     │     │
│  │  │ Plan     │→│ Execute  │→│ Feedback │     │     │
│  │  └─────────┘  └─────────┘  └─────────┘     │     │
│  └─────────────────────────────────────────────┘     │
│                                                       │
│  ┌─────────────────────────────────────────────┐     │
│  │         Learning Loop (学习循环)               │     │
│  │  ┌─────────┐  ┌─────────┐  ┌─────────┐     │     │
│  │  │ 收集     │  │ 分析     │  │ 优化     │     │     │
│  │  │ Collect  │→│ Analyze  │→│ Optimize │     │     │
│  │  └─────────┘  └─────────┘  └─────────┘     │     │
│  └─────────────────────────────────────────────┘     │
│                                                       │
│  ┌─────────────────────────────────────────────┐     │
│  │              State Manager                    │     │
│  │  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐       │     │
│  │  │Session│ │ Task  │ │Global│ │Memory│       │     │
│  │  │Ctx   │ │ Ctx   │ │ Ctx  │ │ Ctx  │       │     │
│  │  └──────┘ └──────┘ └──────┘ └──────┘       │     │
│  └─────────────────────────────────────────────┘     │
└─────────────────────────────────────────────────────┘
```

---

## 核心接口说明

### 主循环接口 (loop.h)

| 接口 | 功能 | 线程安全 |
|------|------|----------|
| `agentos_loop_create()` | 创建核心循环（可指定配置） | 否 |
| `agentos_loop_destroy()` | 销毁核心循环及所有内部资源 | 否 |
| `agentos_loop_run()` | 启动核心循环（阻塞，直到停止） | 否 |
| `agentos_loop_stop()` | 停止核心循环 | 是 |
| `agentos_loop_submit()` | 提交用户任务（自然语言输入） | 是 |
| `agentos_loop_wait()` | 等待任务完成并获取结果 | 是 |
| `agentos_loop_get_engines()` | 获取内部引擎指针（用于扩展） | 否 |
| `agentos_loop_submit_persistent()` | 提交持久化长任务（带检查点） | 是 |
| `agentos_loop_restore_task()` | 从检查点恢复长任务 | 是 |
| `agentos_loop_list_checkpoints()` | 列出可恢复的检查点任务 | 是 |

**配置结构** (`agentos_loop_config_t`):

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `loop_config_cognition_threads` | uint32_t | — | 认知层线程数 |
| `loop_config_execution_threads` | uint32_t | — | 执行层线程数 |
| `loop_config_memory_threads` | uint32_t | — | 记忆层线程数 |
| `loop_config_max_queued_tasks` | uint32_t | — | 最大排队任务数 |
| `loop_config_task_timeout_ms` | uint32_t | 30000 | 任务执行超时 |
| `loop_config_memory_importance` | float | 0.7 | 记忆重要性权重 |
| `loop_config_checkpoint_enabled` | uint32_t | 0 | 检查点启用标志 |
| `loop_config_checkpoint_interval_ms` | uint32_t | 30000 | 检查点保存间隔 |

### 认知引擎接口 (cognition.h)

| 接口 | 功能 |
|------|------|
| `agentos_cognition_create()` | 创建认知引擎（指定策略） |
| `agentos_cognition_create_ex()` | 创建认知引擎（带配置） |
| `agentos_cognition_destroy()` | 销毁认知引擎 |
| `agentos_cognition_process()` | 处理用户输入，生成任务计划（DAG） |
| `agentos_cognition_set_fallback_plan()` | 设置回退规划策略 |
| `agentos_cognition_set_context()` | 设置全局上下文 |
| `agentos_cognition_set_memory()` | 绑定记忆引擎 |
| `agentos_cognition_stats()` | 获取统计信息 |
| `agentos_cognition_health_check()` | 健康检查 |

**策略接口**（可插拔）:

| 策略类型 | 结构体 | 说明 |
|----------|--------|------|
| 规划策略 | `agentos_plan_strategy_t` | 根据意图生成任务计划（reactive/reflective/hierarchical/ml） |
| 协同策略 | `agentos_coordinator_strategy_t` | 协调多个模型输出（majority/weighted/arbiter/dual_model） |
| 调度策略 | `agentos_dispatching_strategy_t` | 从候选 Agent 中选择最合适的（round_robin/priority/weighted/ml） |

**意图解析器**:

| 接口 | 功能 |
|------|------|
| `agentos_intent_parser_create()` | 创建意图解析器 |
| `agentos_intent_parser_parse()` | 解析用户输入，提取意图 |
| `agentos_intent_parser_add_rule()` | 添加自定义意图规则 |
| `agentos_intent_free()` | 释放意图结构 |

### 执行引擎接口 (execution.h)

| 接口 | 功能 | 线程安全 |
|------|------|----------|
| `agentos_execution_create()` | 创建执行引擎（指定最大并发数） | 否 |
| `agentos_execution_destroy()` | 销毁执行引擎 | 否 |
| `agentos_execution_register_unit()` | 注册执行单元 | 否 |
| `agentos_execution_unregister_unit()` | 注销执行单元 | 否 |
| `agentos_execution_submit()` | 提交任务执行 | 是 |
| `agentos_execution_query()` | 查询任务状态 | 是 |
| `agentos_execution_wait()` | 等待任务完成 | 是 |
| `agentos_execution_cancel()` | 取消任务 | 是 |
| `agentos_execution_get_result()` | 获取任务结果 | 是 |
| `agentos_execution_set_feedback_callback()` | 设置反馈回调 | 是 |

**任务状态**: `PENDING → RUNNING → SUCCEEDED / FAILED / CANCELLED / RETRYING`

**内置执行单元**: tool、api、code、file、shell、browser、db、weighted

**补偿事务** (compensation.h):

| 接口 | 功能 |
|------|------|
| `agentos_compensation_create()` | 创建补偿事务管理器 |
| `agentos_compensation_register()` | 注册可补偿操作 |
| `agentos_compensation_compensate()` | 执行补偿（回滚） |
| `agentos_compensation_get_human_queue()` | 获取待人工介入的队列 |

### 记忆引擎接口 (memory.h)

| 接口 | 功能 | 线程安全 |
|------|------|----------|
| `agentos_memory_create()` | 创建记忆引擎 | 否 |
| `agentos_memory_destroy()` | 销毁记忆引擎 | 否 |
| `agentos_memory_write()` | 写入记忆记录 | 是 |
| `agentos_memory_query()` | 查询记忆（语义搜索） | 是 |
| `agentos_memory_get()` | 根据 ID 获取记录 | 是 |
| `agentos_memory_mount()` | 挂载记忆到当前上下文 | 是 |
| `agentos_memory_evolve()` | 触发记忆进化（模式挖掘） | 否 |
| `agentos_memory_health_check()` | 健康检查 | 是 |

### 认知进化系统 (cognitive_evolution.h)

基于 MCIS 理论中的认知观（C 维度）设计，支持智能体认知能力的自适应提升：

| 能力 | 接口 | 说明 |
|------|------|------|
| 经验记忆 | `cog_evolution_record_experience()` | 从历史交互中提取模式 |
| 模式提取 | `cog_evolution_extract_patterns()` | 自动发现行为模式 |
| 策略进化 | `cog_evolution_evolve_strategies()` | 基于反馈优化决策策略 |
| 策略选择 | `cog_evolution_select_strategy()` | 根据上下文选择最优策略 |
| 知识迁移 | `cog_evolution_transfer_knowledge()` | 跨领域知识复用与适配 |
| 认知层级 | `cog_evolution_evaluate_level()` | 从感知到推理的认知跃迁 |

**认知层级**: `PERCEPTION → REACTION → LEARNING → REASONING → CREATION`

### 多智能体协作 (multi_agent_collaboration.h)

| 接口 | 功能 |
|------|------|
| `mac_framework_create()` | 创建协作框架（指定默认模式） |
| `mac_framework_register_agent()` | 注册智能体 |
| `mac_framework_create_group()` | 创建协作组 |
| `mac_framework_delegate_task()` | 委派任务 |
| `mac_framework_collect_results()` | 收集结果 |
| `mac_framework_start_consensus()` | 启动共识投票 |
| `mac_framework_resolve_consensus()` | 解决共识 |

**协作模式**: `INDEPENDENT / COLLABORATIVE / CONSENSUS / DELEGATED`

**共识策略**: `MAJORITY / UNANIMOUS / WEIGHTED / LEADER`

---

## 事件驱动模型

CoreLoopThree 采用事件驱动架构，三循环之间通过事件进行通信：

```
┌────────────┐     ┌────────────┐     ┌────────────┐
│ Cognition  │────→│ Execution  │────→│  Learning   │
│ Loop       │ 事件│ Loop       │ 事件│  Loop       │
│            │←────│            │←────│             │
└────────────┘     └────────────┘     └────────────┘
```

- **认知 → 执行**: 行动计划事件、决策指令
- **执行 → 认知**: 执行结果、状态反馈
- **执行 → 学习**: 执行轨迹、性能数据
- **学习 → 认知**: 优化建议、策略更新

---

## 状态管理

CoreLoopThree 维护四种类型的上下文状态：

| 上下文类型 | 作用域 | 生命周期 | 用途 |
|-----------|--------|----------|------|
| Session Ctx | 会话 | 一次对话 | 当前对话的连续性和上下文 |
| Task Ctx | 任务 | 任务期间 | 当前执行任务的中间状态 |
| Global Ctx | 系统 | 持久化 | 全局配置和共享状态 |
| Memory Ctx | 记忆 | 持久化 | 跨会话的知识和记忆 |

---

## 依赖关系

| 依赖项 | 来源 | 用途 |
|--------|------|------|
| CoreKern | atoms/corekern | 微内核调度和 IPC 能力 |
| Syscall | atoms/syscall | 系统调用接口 |
| Memory | atoms/memory | 记忆存储和检索 |
| TaskFlow | atoms/taskflow | DAG 任务编排 |
| commons | agentos/commons | 统一类型和平台抽象 |

---

## 构建说明

```bash
# 构建
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 运行单元测试
cmake -B build -DBUILD_TESTS=ON
ctest --test-dir build -R coreloopthree
```

---

## 使用示例

```c
#include "loop.h"
#include "cognition.h"
#include "execution.h"
#include "memory.h"

int main(void) {
    agentos_core_init();

    agentos_core_loop_t *loop;
    agentos_loop_config_t config = {0};
    config.loop_config_cognition_threads = 2;
    config.loop_config_execution_threads = 4;
    config.loop_config_memory_threads = 1;
    config.loop_config_max_queued_tasks = 100;
    config.loop_config_task_timeout_ms = 30000;
    config.loop_config_checkpoint_enabled = 1;

    agentos_loop_create(&config, &loop);

    char *task_id;
    agentos_loop_submit(loop, "分析这份文档并生成摘要",
                        strlen("分析这份文档并生成摘要"), &task_id);

    char *result;
    size_t result_len;
    agentos_loop_wait(loop, task_id, 60000, &result, &result_len);

    agentos_mem_free(result);
    agentos_mem_free(task_id);
    agentos_loop_destroy(loop);
    agentos_core_shutdown();
    return 0;
}
```

---

## 与相关模块的关系

- **CoreKern**: 依赖微内核的调度和服务能力
- **Syscall**: 通过标准化接口调用系统能力
- **Memory/MemoryRovol**: 管理记忆上下文的持久化和检索
- **TaskFlow**: 在执行循环中驱动 DAG 任务流
- **Frameworks**: 在执行循环中调用外部 AI 框架能力

---

© 2026 SPHARX Ltd. All Rights Reserved.
