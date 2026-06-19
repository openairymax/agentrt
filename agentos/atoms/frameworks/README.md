# Frameworks — 框架集成层

`agentos/atoms/frameworks/`

**版本**: v0.1.0

---

## 概述

Frameworks 层是 AgentRT 连接外部 AI 框架的桥梁，采用**五大框架统一抽象**架构，将 Agent（CoreLoopThree）、Memory（MemoryRovol）、Task（CoreKern）、Safety（Cupolas）和 Tool（tool_d）五大核心框架封装为统一的 C API 接口。通过定义标准的"能力模型"和"框架管理器"，实现框架的热插拔、协议感知和可组合编排。

Frameworks 层以 C11 标准实现，通过 `agentos_frameworks.h` 提供统一入口，支持 JSON-RPC、MCP、A2A、OpenAI 等多种协议。框架管理器（Framework Manager）负责框架的完整生命周期管理，包括初始化、启动、停止、健康检查和事件回调。

---

## 设计理念

| 原则 | 说明 |
|------|------|
| **统一入口** | 所有框架通过统一 API 访问，上层应用不感知具体框架实现 |
| **协议感知** | 框架操作支持多协议（JSON-RPC / MCP / A2A / OpenAI） |
| **生产级** | 内置错误处理、重试机制、健康检查和监控 |
| **可组合** | 框架间可自由组合编排，支持跨框架调用 |
| **热插拔** | 框架可在运行时动态初始化和停止，无需重启系统 |

---

## 目录结构

```
frameworks/
├── include/
│   └── agentos_frameworks.h       # 统一框架抽象层头文件
├── src/
│   └── agentos_frameworks.c       # 框架管理器实现
└── CMakeLists.txt                 # 构建配置
```

---

## 五大框架模型

| 框架类型 | 枚举值 | 对应模块 | 核心能力 |
|----------|--------|----------|----------|
| **Agent** | `AGENTOS_FW_AGENT` | CoreLoopThree | 认知-执行-记忆三层循环 |
| **Memory** | `AGENTOS_FW_MEMORY` | MemoryRovol | L1-L4 四层卷载记忆架构 |
| **Task** | `AGENTOS_FW_TASK` | CoreKern | 加权轮询任务调度 |
| **Safety** | `AGENTOS_FW_SAFETY` | Cupolas | 四重安全防护 |
| **Tool** | `AGENTOS_FW_TOOL` | tool_d | 工具服务管理 |

---

## 能力标志

Frameworks 层定义了 13 种标准能力标志（位掩码），每个框架声明其支持的能力子集：

| 能力 | 掩码 | 说明 |
|------|------|------|
| `AGENTOS_CAP_COGNITION` | `1 << 0` | 认知能力 |
| `AGENTOS_CAP_EXECUTION` | `1 << 1` | 执行能力 |
| `AGENTOS_CAP_MEMORY_STORE` | `1 << 2` | 记忆存储 |
| `AGENTOS_CAP_MEMORY_RETRIEVE` | `1 << 3` | 记忆检索 |
| `AGENTOS_CAP_TASK_SCHEDULE` | `1 << 4` | 任务调度 |
| `AGENTOS_CAP_TASK_EXECUTE` | `1 << 5` | 任务执行 |
| `AGENTOS_CAP_SAFETY_CHECK` | `1 << 6` | 安全检查 |
| `AGENTOS_CAP_SANDBOX` | `1 << 7` | 沙箱隔离 |
| `AGENTOS_CAP_TOOL_REGISTER` | `1 << 8` | 工具注册 |
| `AGENTOS_CAP_TOOL_INVOKE` | `1 << 9` | 工具调用 |
| `AGENTOS_CAP_PROTOCOL_MCP` | `1 << 10` | MCP 协议支持 |
| `AGENTOS_CAP_PROTOCOL_A2A` | `1 << 11` | A2A 协议支持 |
| `AGENTOS_CAP_PROTOCOL_OPENAI` | `1 << 12` | OpenAI 协议支持 |
| `AGENTOS_CAP_ALL` | `0x1FFF` | 全部能力 |

---

## 框架状态机

```
UNINITIALIZED → INITIALIZED → RUNNING ⇄ PAUSED
                   │              │
                   └──────────────┴──→ ERROR → SHUTDOWN
```

| 状态 | 枚举值 | 说明 |
|------|--------|------|
| `AGENTOS_FW_STATE_UNINITIALIZED` | 0 | 未初始化 |
| `AGENTOS_FW_STATE_INITIALIZED` | 1 | 已初始化，未启动 |
| `AGENTOS_FW_STATE_RUNNING` | 2 | 运行中 |
| `AGENTOS_FW_STATE_PAUSED` | 3 | 已暂停 |
| `AGENTOS_FW_STATE_ERROR` | 4 | 错误状态 |
| `AGENTOS_FW_STATE_SHUTDOWN` | 5 | 已关闭 |

---

## 接口说明

### 框架管理器生命周期

| 接口 | 功能 |
|------|------|
| `agentos_fw_manager_create()` | 创建框架管理器 |
| `agentos_fw_manager_destroy()` | 销毁框架管理器 |

### 框架初始化与启停

| 接口 | 功能 |
|------|------|
| `agentos_fw_init()` | 初始化指定框架（可传入配置） |
| `agentos_fw_init_all()` | 初始化所有框架 |
| `agentos_fw_start()` | 启动指定框架 |
| `agentos_fw_start_all()` | 启动所有已初始化框架 |
| `agentos_fw_stop()` | 停止指定框架 |
| `agentos_fw_stop_all()` | 停止所有框架 |

### 框架查询

| 接口 | 功能 |
|------|------|
| `agentos_fw_get_info()` | 获取单个框架信息 |
| `agentos_fw_get_all_info()` | 获取所有框架信息 |
| `agentos_fw_has_capability()` | 检查框架是否支持指定能力 |
| `agentos_fw_get_state()` | 获取框架状态 |

### 框架事件与监控

| 接口 | 功能 |
|------|------|
| `agentos_fw_register_event_callback()` | 注册框架事件回调 |
| `agentos_fw_health_check()` | 执行单个框架健康检查 |
| `agentos_fw_health_check_all()` | 执行所有框架健康检查 |

### 工具函数

| 接口 | 功能 |
|------|------|
| `agentos_fw_type_to_string()` | 框架类型转字符串 |
| `agentos_fw_state_to_string()` | 框架状态转字符串 |
| `agentos_fw_error_to_string()` | 错误码转字符串 |
| `agentos_fw_create_default_config()` | 创建默认框架配置 |

---

## 核心数据结构

### 框架信息 (`agentos_fw_info_t`)

| 字段 | 类型 | 说明 |
|------|------|------|
| `type` | `agentos_framework_t` | 框架类型 |
| `name` | `char[32]` | 框架名称 |
| `version` | `char[16]` | 框架版本 |
| `state` | `agentos_fw_state_t` | 当前状态 |
| `capabilities` | `uint32_t` | 能力标志位 |
| `init_time_ms` | `uint64_t` | 初始化时间 |
| `last_activity_ms` | `uint64_t` | 最后活动时间 |
| `error_count` | `uint32_t` | 错误计数 |
| `operation_count` | `uint32_t` | 操作计数 |

### 框架配置 (`agentos_fw_config_t`)

| 字段 | 类型 | 说明 |
|------|------|------|
| `type` | `agentos_framework_t` | 框架类型 |
| `max_retries` | `uint32_t` | 最大重试次数 |
| `timeout_ms` | `uint32_t` | 超时时间 |
| `enable_metrics` | `bool` | 是否启用指标收集 |
| `enable_tracing` | `bool` | 是否启用追踪 |
| `enable_protocol_support` | `bool` | 是否启用协议支持 |
| `config_path` | `char[256]` | 配置文件路径 |

### 框架事件 (`agentos_fw_event_t`)

| 字段 | 类型 | 说明 |
|------|------|------|
| `type` | `agentos_fw_event_type_t` | 事件类型（INIT/START/STOP/ERROR/STATE_CHANGE/OPERATION） |
| `framework` | `agentos_framework_t` | 来源框架 |
| `detail` | `const char*` | 事件详情 |
| `timestamp` | `uint64_t` | 事件时间戳 |
| `error_code` | `int32_t` | 错误码 |

### 操作结果 (`agentos_fw_result_t`)

| 字段 | 类型 | 说明 |
|------|------|------|
| `code` | `int32_t` | 结果码 |
| `message` | `char[256]` | 结果消息 |
| `data` | `void*` | 结果数据 |
| `data_size` | `size_t` | 数据大小 |
| `latency_us` | `uint64_t` | 操作延迟（微秒） |

---

## 错误码

| 错误码 | 值 | 说明 |
|--------|-----|------|
| `AGENTOS_FW_OK` | 0 | 成功 |
| `AGENTOS_FW_ERROR` | -1 | 通用错误 |
| `AGENTOS_FW_NOT_INIT` | -2 | 未初始化 |
| `AGENTOS_FW_INVALID_ARG` | -3 | 无效参数 |
| `AGENTOS_FW_TIMEOUT` | -4 | 超时 |
| `AGENTOS_FW_BUSY` | -5 | 忙碌 |
| `AGENTOS_FW_NOT_FOUND` | -6 | 未找到 |
| `AGENTOS_FW_DENIED` | -7 | 拒绝访问 |

---

## 依赖关系

| 依赖项 | 来源 | 用途 |
|--------|------|------|
| CoreLoopThree | atoms/coreloopthree | Agent 框架后端 |
| MemoryRovol | atoms/memoryrovol | Memory 框架后端 |
| CoreKern | atoms/corekern | Task 框架后端 |
| Cupolas | Daemon 层 | Safety 框架后端 |
| tool_d | Daemon 层 | Tool 框架后端 |

---

## 构建说明

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

---

## 使用示例

```c
#include "agentos_frameworks.h"

int main(void) {
    agentos_fw_manager_t mgr = agentos_fw_manager_create();

    agentos_fw_config_t config = agentos_fw_create_default_config(AGENTOS_FW_AGENT);
    config.enable_metrics = true;
    config.enable_tracing = true;
    config.timeout_ms = 30000;

    agentos_fw_init(mgr, AGENTOS_FW_AGENT, &config);
    agentos_fw_init(mgr, AGENTOS_FW_MEMORY, NULL);
    agentos_fw_start_all(mgr);

    bool has_cog = agentos_fw_has_capability(mgr, AGENTOS_FW_AGENT,
                                              AGENTOS_CAP_COGNITION);

    agentos_fw_info_t info;
    agentos_fw_get_info(mgr, AGENTOS_FW_AGENT, &info);

    int32_t health = agentos_fw_health_check_all(mgr);

    agentos_fw_stop_all(mgr);
    agentos_fw_manager_destroy(mgr);
    return 0;
}
```

---

## 与上层模块的关系

- **CoreLoopThree**: 在执行循环中通过 Frameworks 层调用外部 AI 能力
- **Daemon 服务**: LLM 守护进程（LLM_d）使用 Frameworks 层管理与 LLM 的通信
- **Toolkit SDK**: 客户端 SDK 通过 Frameworks 层与外部框架交互

---

© 2026 SPHARX Ltd. All Rights Reserved.
