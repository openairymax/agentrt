Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# AgentOS Agent 创建指南

**版本**: Doc V1.7  
**最后更新**: 2026-03-31  
**难度**: ⭐⭐ 中级  
**预计时间**: 30 分钟  
**作者**: LirenWang

---

## 1. 概述

Agent 是 AgentOS 的核心执行实体。每一个 Agent 都是一个自主运行的单元，拥有独立的生命周期、技能集合和记忆空间。本文档将引导你从零开始创建一个符合 AgentOS 契约规范的生产级 Agent。

### 1.1 Agent 在架构中的位置

```
┌─────────────────────────────────────────────┐
│              daemon/ 用户态服务                │
│          ┌─────────────────────┐            │
│          │   your_agent_d      │ ← 你要创建的 Agent
│          └─────────┬───────────┘            │
├────────────────────┼────────────────────────┤
│            syscall/ 系统调用接口              │
│     task · memory · session · telemetry     │
├────────────────────┼────────────────────────┤
│            cupolas/ 安全穹顶                   │
│     workbench · permission · sanitizer      │
├────────────────────┴────────────────────────┤
│         corekern/ 微内核基础                 │
│     IPC · Mem · Task · Time                 │
└─────────────────────────────────────────────┘
```

Agent 以守护进程 (`_d`) 的形式运行在用户态，通过系统调用与内核交互，受到安全穹顶的保护。

### 1.2 设计原则

- **契约驱动**: Agent 必须实现 Agent Contract 定义的标准接口
- **安全内生**: Agent 在虚拟工位中运行，遵循最小权限原则
- **可观测**: Agent 自动集成结构化日志、指标收集和链路追踪
- **双系统协同**: 简单任务走 System 1 快速路径，复杂任务走 System 2 深度路径

---

## 2. 前置条件

### 2.1 环境要求

| 依赖 | 最低版本 | 推荐版本 |
|------|---------|---------|
| CMake | 3.20+ | 3.28+ |
| C 编译器 (GCC/Clang/MSVC) | C11 | C17 |
| AgentOS 内核 | v1.0.0 | v1.0.0.6 |
| AgentOS CLI | v1.0.0 | v1.0.0.6 |

### 2.2 知识储备

- 熟悉 C11 语言标准
- 了解 AgentOS 微内核架构（参见 [ARCHITECTURAL_PRINCIPLES.md](../architecture/ARCHITECTURAL_PRINCIPLES.md)）
- 理解 Agent Contract 契约（参见 [agent_contract.md](../specifications/agentos_contract/agent/agent_contract.md)）

---

## 3. Agent 生命周期

理解 Agent 的生命周期是创建 Agent 的基础。AgentOS 中 Agent 的状态转换如下：

```
                 ┌──────────┐
         ┌──────►│  CREATED  │
         │       └────┬─────┘
         │            │ agent_start()
         │       ┌────▼─────┐
         │       │ INITING  │
         │       └────┬─────┘
         │            │ on_initialize()
         │       ┌────▼─────┐
         │       │  READY   │◄─────────────────┐
         │       └────┬─────┘                   │
         │            │ receive_task()          │
         │       ┌────▼─────┐                   │
         │       │ RUNNING  │                   │
         │       └────┬─────┘                   │
         │            │ task_complete()         │
         │       ┌────▼─────┐   on_error()      │
         │       │ PAUSED   ├──────────────────►│
         │       └────┬─────┘                   │
         │            │ resume() / on_shutdown()│
         │       ┌────▼─────┐                   │
    reset()      │ STOPPING │                   │
         │       └────┬─────┘                   │
         │            │ cleanup()               │
         │       ┌────▼─────┐                   │
         └───────│  STOPPED │                   │
                 └──────────┘                   │
                       │ destroy()              │
                       ▼                       │
                 ┌──────────┐                   │
                 │ DESTROYED│───────────────────┘
                 └──────────┘
```

---

## 4. 创建 Agent — 从模板开始

### 4.1 使用脚手架生成

```bash
# 创建新 Agent 项目
agentos-cli agent create --name my_agent --template daemon

# 生成的目录结构
my_agent/
├── CMakeLists.txt
├── agent_contract.json
├── include/
│   └── my_agent.h
├── src/
│   ├── main.c
│   ├── my_agent.c
│   └── handlers.c
└── manager/
    └── my_agent.yaml
```

### 4.2 Agent Contract 配置

Agent Contract 是 Agent 的身份声明和能力描述。创建 `agent_contract.json`：

```json
{
  "$schema": "https://agentos.io/schemas/agent_contract_v1.2.0.json",
  "name": "my_agent",
  "version": "1.0.0",
  "description": "示例 Agent — 数据分析",
  "author": "Your Name",
  "license": "MIT",
  "capabilities": [
    {
      "name": "data_analysis",
      "description": "结构化数据分析",
      "input_schema": { "type": "object", "properties": { "data_path": { "type": "string" } } },
      "output_schema": { "type": "object", "properties": { "result": { "type": "string" } } }
    }
  ],
  "memory": {
    "type": "rovol",
    "layers": ["L1", "L2"],
    "persistence": true
  },
  "security": {
    "sandbox": "workbench",
    "permissions": ["fs.read", "net.http", "process.spawn"]
  },
  "observability": {
    "log_level": "INFO",
    "metrics_enabled": true,
    "tracing_enabled": true
  }
}
```

### 4.3 CMakeLists.txt 配置

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_agent VERSION 1.0.0 LANGUAGES C)

set(C_STANDARD 11)
set(C_STANDARD_REQUIRED ON)

# 依赖 AgentOS 内核头文件
find_package(AgentOS REQUIRED COMPONENTS corekern syscall)

add_executable(my_agent_d
    src/main.c
    src/my_agent.c
    src/handlers.c
)

target_include_directories(my_agent_d PRIVATE include)
target_link_libraries(my_agent_d PRIVATE AgentOS::syscall)

# 安装为守护进程
install(TARGETS my_agent_d DESTINATION lib/agentos/daemon)
```

---

## 5. 实现 Agent 核心

### 5.1 头文件定义

```c
#ifndef MY_AGENT_H
#define MY_AGENT_H

#include <agentos/agent.h>
#include <agentos/syscall.h>
#include <agentos/memory.h>

#define MY_AGENT_VERSION "1.0.0"
#define MY_AGENT_MAX_TASKS 128

typedef struct {
    agentos_agent_t base;
    char*            data_dir;
    int              max_concurrent;
    atomic_int       active_tasks;
} my_agent_t;

/**
 * @brief 初始化 Agent 实例
 * 
 * 分配资源、注册技能、配置记忆层。
 * 此函数在 Agent 进入 READY 状态前调用。
 * 
 * @param agent Agent 实例指针
 * @param manager 配置参数
 * @return 0 成功；负值 错误码
 */
int my_agent_init(my_agent_t* agent, const agentos_config_t* manager);

/**
 * @brief 处理接收到的任务
 * 
 * 解析任务描述，选择执行路径（System 1 快速路径 / System 2 深度路径）。
 * 
 * @param agent Agent 实例指针
 * @param task 任务描述
 * @param result 输出执行结果
 * @return 0 成功；负值 错误码
 */
int my_agent_handle_task(my_agent_t* agent, const agentos_task_t* task, agentos_result_t* result);

/**
 * @brief 关闭 Agent，释放资源
 * 
 * 等待所有活跃任务完成，持久化记忆，释放内存。
 * 
 * @param agent Agent 实例指针
 * @return 0 成功；负值 错误码
 */
int my_agent_shutdown(my_agent_t* agent);

#endif
```

### 5.2 核心实现

```c
#include "my_agent.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief 初始化 Agent
 */
int my_agent_init(my_agent_t* agent, const agentos_config_t* manager)
{
    int ret = 0;

    // 初始化基类
    ret = agentos_agent_init(&agent->base, "my_agent", MY_AGENT_VERSION);
    if (ret != 0) return ret;

    // 解析配置
    agent->data_dir = config_get_string(manager, "data_dir", "/var/lib/agentos/my_agent");
    agent->max_concurrent = config_get_int(manager, "max_concurrent", 8);
    atomic_store(&agent->active_tasks, 0);

    // 注册记忆层 — 启用 L1 原始卷和 L2 特征层
    ret = agentos_memory_init(&agent->base.memory, MEMORY_ROVOL, agent->data_dir);
    if (ret != 0) {
        agentos_log_error("my_agent", "记忆初始化失败: %s", agentos_strerror(ret));
        return ret;
    }

    agentos_memory_enable_layer(&agent->base.memory, MEMORY_LAYER_L1);
    agentos_memory_enable_layer(&agent->base.memory, MEMORY_LAYER_L2);

    agentos_log_info("my_agent", "Agent 初始化完成, version=%s", MY_AGENT_VERSION);
    return 0;
}

/**
 * @brief 双系统路径选择器
 */
static int classify_task_complexity(const agentos_task_t* task)
{
    size_t desc_len = strlen(task->description);

    if (desc_len < 64 && task->priority < TASK_PRIORITY_NORMAL) {
        return SYSTEM_1_FAST_PATH;
    }
    return SYSTEM_2_DEEP_PATH;
}

/**
 * @brief 处理任务 — System 1 快速路径
 */
static int handle_fast_path(my_agent_t* agent, const agentos_task_t* task, agentos_result_t* result)
{
    agentos_trace_push("my_agent", "fast_path_start");

    // 快速匹配已知模式
    agentos_memory_query_t query = {
        .layer = MEMORY_LAYER_L2,
        .pattern = task->description,
        .max_results = 3,
        .threshold = 0.85f
    };

    agentos_memory_result_t memories;
    int ret = agentos_memory_search(&agent->base.memory, &query, &memories);

    if (ret == 0 && memories.count > 0) {
        // 命中已知模式 — 直接复用
        result->success = true;
        result->confidence = memories.results[0].relevance;
        snprintf(result->output, sizeof(result->output),
                 "模式匹配成功，复用方案 #%llu", memories.results[0].id);
    } else {
        // 未命中 — 降级到 System 2
        ret = handle_deep_path(agent, task, result);
    }

    agentos_trace_pop("my_agent");
    return ret;
}

/**
 * @brief 处理任务 — System 2 深度路径
 */
static int handle_deep_path(my_agent_t* agent, const agentos_task_t* task, agentos_result_t* result)
{
    agentos_trace_push("my_agent", "deep_path_start");

    // 1. 将任务描述转换为任务图 (DAG)
    agentos_task_graph_t graph;
    int ret = agentos_task_decompose(task, &graph);
    if (ret != 0) {
        agentos_log_error("my_agent", "任务分解失败: %s", agentos_strerror(ret));
        agentos_trace_pop("my_agent");
        return ret;
    }

    // 2. 增量规划 — 按阶段执行
    for (size_t i = 0; i < graph.stage_count; i++) {
        ret = agentos_task_execute_stage(&graph.stages[i], result);
        if (ret != 0) {
            agentos_log_warn("my_agent", "阶段 %zu 执行失败，触发补偿", i);
            ret = agentos_task_compensate(&graph.stages[i]);
            if (ret != 0) break;
        }

        // 轮次内反馈 — 根据执行结果调整后续规划
        if (i + 1 < graph.stage_count) {
            agentos_task_replan(&graph, i, result);
        }
    }

    // 3. 记录到记忆层
    agentos_memory_record_t record = {
        .layer = MEMORY_LAYER_L1,
        .content = task->description,
        .outcome = result->success ? "success" : "failure",
        .metadata = { .duration_ms = result->elapsed_ms }
    };
    agentos_memory_store(&agent->base.memory, &record);

    agentos_task_graph_destroy(&graph);
    agentos_trace_pop("my_agent");
    return ret;
}

/**
 * @brief 任务处理入口 — 双系统调度
 */
int my_agent_handle_task(my_agent_t* agent, const agentos_task_t* task, agentos_result_t* result)
{
    atomic_fetch_add(&agent->active_tasks, 1);

    int path = classify_task_complexity(task);
    agentos_log_info("my_agent", "收到任务[%llu], 路径=%s",
                     task->id, path == SYSTEM_1_FAST_PATH ? "S1-快速" : "S2-深度");

    int ret;
    if (path == SYSTEM_1_FAST_PATH) {
        ret = handle_fast_path(agent, task, result);
    } else {
        ret = handle_deep_path(agent, task, result);
    }

    // 记录指标
    agentos_metrics_counter("my_agent.tasks.total", 1);
    if (ret == 0) {
        agentos_metrics_counter("my_agent.tasks.success", 1);
    } else {
        agentos_metrics_counter("my_agent.tasks.failure", 1);
    }
    agentos_metrics_histogram("my_agent.tasks.duration_ms", result->elapsed_ms);

    atomic_fetch_sub(&agent->active_tasks, 1);
    return ret;
}

/**
 * @brief 关闭 Agent
 */
int my_agent_shutdown(my_agent_t* agent)
{
    agentos_log_info("my_agent", "Agent 关闭中, 活跃任务: %d", atomic_load(&agent->active_tasks));

    // 等待活跃任务完成（最多 30 秒）
    int wait_count = 0;
    while (atomic_load(&agent->active_tasks) > 0 && wait_count < 300) {
        agentos_time_msleep(100);
        wait_count++;
    }

    if (atomic_load(&agent->active_tasks) > 0) {
        agentos_log_warn("my_agent", "强制关闭，%d 个任务未完成", atomic_load(&agent->active_tasks));
    }

    // 持久化记忆
    agentos_memory_flush(&agent->base.memory);

    // 释放资源
    free(agent->data_dir);
    agentos_agent_destroy(&agent->base);

    agentos_log_info("my_agent", "Agent 已关闭");
    return 0;
}
```

### 5.3 主入口

```c
#include "my_agent.h"
#include <signal.h>

static volatile int g_running = 1;

static void signal_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

int main(int argc, char* argv[])
{
    // 注册信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // 加载配置
    agentos_config_t manager;
    int ret = agentos_config_load("manager/my_agent.yaml", &manager);
    if (ret != 0) {
        fprintf(stderr, "配置加载失败: %s\n", agentos_strerror(ret));
        return 1;
    }

    // 初始化 Agent
    my_agent_t agent = { 0 };
    ret = my_agent_init(&agent, &manager);
    if (ret != 0) {
        fprintf(stderr, "Agent 初始化失败: %s\n", agentos_strerror(ret));
        return 1;
    }

    // 通过系统调用注册到内核
    ret = agentos_syscall_agent_register(&agent.base);
    if (ret != 0) {
        fprintf(stderr, "Agent 注册失败: %s\n", agentos_strerror(ret));
        return 1;
    }

    agentos_log_info("my_agent", "Agent 已启动，等待任务...");

    // 主事件循环 — 通过 IPC 接收任务
    while (g_running) {
        agentos_task_t task;
        ret = agentos_syscall_task_recv(&task, 1000);

        if (ret == AGENTOS_ETIMEDOUT) continue;
        if (ret != 0) {
            agentos_log_error("my_agent", "任务接收失败: %s", agentos_strerror(ret));
            continue;
        }

        agentos_result_t result = { 0 };
        ret = my_agent_handle_task(&agent, &task, &result);
        agentos_syscall_task_complete(task.id, ret, &result);
    }

    // 优雅关闭
    my_agent_shutdown(&agent);
    agentos_config_free(&manager);

    return 0;
}
```

---

## 6. 注册与部署

### 6.1 编译安装

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/usr/local
cmake --build . --parallel
sudo cmake --install .
```

### 6.2 系统注册

```bash
# 验证 Agent 契约
agentos-cli contract validate agent_contract.json

# 注册 Agent 到系统
agentos-cli agent register \
    --name my_agent \
    --contract agent_contract.json \
    --binary /usr/local/lib/agentos/daemon/my_agent_d \
    --manager manager/my_agent.yaml

# 启动 Agent
agentos-cli agent start my_agent

# 验证运行状态
agentos-cli agent status my_agent
```

### 6.3 安全配置

在安全穹顶中配置 Agent 权限：

```yaml
# manager/security/policy.yaml
agents:
  my_agent:
    sandbox: workbench
    permissions:
      - action: "fs.read"
        resource: "/var/lib/agentos/my_agent/**"
      - action: "fs.write"
        resource: "/var/lib/agentos/my_agent/**"
      - action: "net.http"
        resource: "api.example.com"
      - action: "memory.read"
        resource: "own"
      - action: "memory.write"
        resource: "own"
    resource_limits:
      max_memory: "512MB"
      max_cpu: "80%"
      max_tasks: 128
      timeout: 300
```

---

## 7. 测试

### 7.1 单元测试

```c
#include "my_agent.h"
#include <ctest/ctest.h>

CTEST(my_agent, init_success)
{
    my_agent_t agent = { 0 };
    agentos_config_t manager = { .data_dir = "/tmp/test_agent" };

    int ret = my_agent_init(&agent, &manager);
    ASSERT_EQUAL(0, ret);
    ASSERT_STR_EQUAL("my_agent", agent.base.name);

    my_agent_shutdown(&agent);
}

CTEST(my_agent, fast_path_simple_task)
{
    my_agent_t agent = { 0 };
    agentos_config_t manager = { .data_dir = "/tmp/test_agent" };
    my_agent_init(&agent, &manager);

    agentos_task_t task = {
        .id = 1,
        .description = "简单的问候任务",
        .priority = TASK_PRIORITY_LOW
    };
    agentos_result_t result = { 0 };

    int ret = my_agent_handle_task(&agent, &task, &result);
    ASSERT_EQUAL(0, ret);

    my_agent_shutdown(&agent);
}
```

### 7.2 契约测试

```bash
# 运行契约合规测试
agentos-cli test contract --agent my_agent

# 运行安全合规测试
agentos-cli test security --agent my_agent
```

---

## 8. 最佳实践

### 8.1 双系统路径设计

| 场景 | 推荐路径 | 原因 |
|------|---------|------|
| 简单查询、模式匹配 | System 1 | 延迟敏感，模式已知 |
| 数据分析、报告生成 | System 2 | 需要深度推理和规划 |
| 紧急告警响应 | System 1 → System 2 | 先快速响应，再深度分析 |
| 长周期任务 | System 2 + 增量规划 | 需要轮次内反馈 |

### 8.2 错误处理

```c
// 所有系统调用必须检查返回值
int ret = agentos_syscall_memory_store(&record);
if (ret != 0) {
    agentos_log_error("my_agent", "记忆存储失败: %s (errno=%d)", 
                      agentos_strerror(ret), ret);
    
    // 区分可恢复和不可恢复错误
    if (ret == AGENTOS_EAGAIN) {
        // 资源暂时不可用 — 等待重试
        agentos_time_msleep(100);
    } else {
        // 不可恢复 — 向上传播
        return ret;
    }
}
```

### 8.3 资源管理

- 所有 `malloc` 必须有对应的 `free`
- 所有打开的文件描述符必须在关闭时检查
- 使用 RAII 模式管理资源（C11 的 `_Generic` + cleanup 属性）

---

## 9. 相关文档

- [Agent 契约规范](../specifications/agentos_contract/agent/agent_contract.md)
- [架构设计原则](../architecture/ARCHITECTURAL_PRINCIPLES.md)
- [Skill 创建指南](create_skill.md)
- [编码规范](../specifications/coding_standard/C_coding_style_guide.md)
- [系统调用 API](../architecture/folder/syscall.md)

---

**最后更新**: 2026-03-23  
**维护者**: AgentOS 核心团队

---

© 2026 SPHARX Ltd. All Rights Reserved.  
*"From data intelligence emerges."*