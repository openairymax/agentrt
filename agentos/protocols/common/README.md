# Common — 公共协议层

**模块路径**: `agentos/protocols/common/`
**版本**: v0.1.0

## 概述

Common 层是 Protocols 协议栈的基础层，定义了所有上层组件共享的统一消息模型、协议类型枚举、协议栈生命周期管理以及框架初始化/管理器接口。所有其他层（Core / Standards / Integrations / Frameworks）均依赖本层提供的数据结构和 API。

本层包含两个核心实现文件：

- **`unified_protocol.c`** — 统一协议栈核心实现，包括协议栈创建/销毁、适配器注册、消息发送/接收、回调设置与统计信息
- **`protocols_impl.c`** — 框架级初始化/清理、协议栈管理器、默认适配器工厂（HTTP/WebSocket/gRPC/MQTT）与错误处理

## 目录结构

```
common/
├── README.md
├── include/
│   └── protocols.h                 # 框架主头文件
└── src/
    ├── unified_protocol.c          # 统一协议栈核心实现
    └── protocols_impl.c            # 框架初始化/管理器/默认适配器
```

## 核心组件

### protocols.h — 框架主头文件

框架的统一入口头文件，包含以下 API 分组：

| API 分组 | 函数 | 说明 |
|----------|------|------|
| **初始化与清理** | `protocols_framework_init()`, `protocols_framework_cleanup()`, `protocols_framework_version()` | 框架生命周期管理 |
| **协议栈管理器** | `protocol_manager_create()`, `protocol_manager_destroy()`, `protocol_manager_create_stack()`, `protocol_manager_destroy_stack()`, `protocol_manager_get_stacks()` | 多协议栈实例管理，每个管理器最多持有 32 个协议栈 |
| **默认适配器** | `protocol_adapter_http()`, `protocol_adapter_websocket()`, `protocol_adapter_grpc()`, `protocol_adapter_mqtt()` | 内置协议适配器工厂函数 |
| **错误处理** | `protocol_error_to_string()`, `protocol_get_last_error()` | 错误码描述与最近错误查询 |
| **配置辅助** | `protocol_stack_config_default()`, `protocol_stack_config_destroy()` | 默认配置生成与资源释放 |

### unified_protocol.h — 统一消息模型（位于 `protocols/include/`）

定义了协议栈的核心数据类型：

| 类型 | 说明 |
|------|------|
| `agentos_protocol_type_t` | 支持的协议类型枚举（JSON-RPC / MCP / A2A / OpenAI / OpenJiuwen / Claude / ChinaEco / AGNTCY / OpenClaw） |
| `unified_message_t` | 统一消息结构体，包含协议类型、方向、端点、方法、载荷、错误信息、追踪元数据等 |
| `protocol_adapter_t` | 协议适配器结构体，定义 init/destroy/encode/decode/connect/disconnect/send/receive 等接口 |
| `protocol_stack_config_t` | 协议栈配置，包含名称、最大适配器数、超时、压缩/加密开关等 |
| `protocol_stack_handle_t` | 协议栈实例句柄 |
| `message_direction_t` | 消息方向枚举（REQUEST / RESPONSE / NOTIFICATION / ERROR） |

### unified_protocol.c — 协议栈核心实现

| 函数 | 说明 |
|------|------|
| `protocol_stack_create()` | 创建协议栈实例，初始化适配器链表与统计计数器 |
| `protocol_stack_destroy()` | 销毁协议栈，释放所有已注册适配器 |
| `protocol_stack_register_adapter()` | 注册协议适配器（支持同类型替换） |
| `protocol_stack_send()` | 通过匹配适配器编码并发送消息 |
| `protocol_stack_receive()` | 遍历适配器接收并解码消息 |
| `protocol_stack_set_callback()` | 设置消息到达回调 |
| `protocol_stack_get_stats()` | 获取发送/接收/字节统计 |
| `unified_message_create()` | 创建统一消息（自动分配消息 ID 和时间戳） |
| `unified_message_destroy()` | 销毁统一消息（不释放 payload，由调用者管理） |
| `protocol_type_to_string()` / `protocol_type_from_string()` | 协议类型与字符串互转 |

### protocols_impl.c — 框架实现

| 函数 | 说明 |
|------|------|
| `protocols_framework_init()` / `protocols_framework_cleanup()` | 框架全局初始化/清理 |
| `protocols_framework_version()` | 返回框架版本号 |
| `protocol_manager_create()` / `protocol_manager_destroy()` | 创建/销毁管理器（最多管理 32 个协议栈） |
| `protocol_manager_create_stack()` / `protocol_manager_destroy_stack()` | 在管理器中创建/销毁协议栈 |
| `protocol_adapter_http()` / `protocol_adapter_websocket()` / `protocol_adapter_grpc()` / `protocol_adapter_mqtt()` | 内置默认适配器实例 |
| `protocol_error_to_string()` | 错误码到描述字符串转换 |
| `protocol_stack_config_default()` | 生成默认协议栈配置（HTTP 协议、64KB 消息上限、30s 超时） |

## 错误码

| 枚举值 | 说明 |
|--------|------|
| `PROTOCOL_SUCCESS` | 成功 |
| `PROTOCOL_ERROR_INVALID_ARG` | 无效参数 |
| `PROTOCOL_ERROR_MEMORY` | 内存不足 |
| `PROTOCOL_ERROR_NOT_INITIALIZED` | 未初始化 |
| `PROTOCOL_ERROR_NOT_CONNECTED` | 未连接 |
| `PROTOCOL_ERROR_TIMEOUT` | 超时 |
| `PROTOCOL_ERROR_ENCODE` | 编码失败 |
| `PROTOCOL_ERROR_DECODE` | 解码失败 |
| `PROTOCOL_ERROR_NETWORK` | 网络错误 |
| `PROTOCOL_ERROR_PROTOCOL` | 协议错误 |
| `PROTOCOL_ERROR_INTERNAL` | 内部错误 |

## 依赖关系

| 依赖 | 来源 | 用途 |
|------|------|------|
| `unified_protocol.h` | `protocols/include/` | 统一消息模型与协议类型定义 |
| `memory_compat.h` | `commons/utils/compat/` | `AGENTOS_CALLOC/MALLOC/FREE` 内存管理宏 |
| `platform.h` | `commons/platform/` | 平台抽象 |
| `safe_string_utils.c` | `daemon/common/` | 安全字符串操作 |
| `error.h` | `commons/utils/` | 错误码定义与错误栈 |
| `types.h` | `commons/utils/` | 基础类型定义 |

## 使用说明

```c
#include "protocols.h"

// 1. 初始化框架
protocols_framework_init();

// 2. 创建管理器与协议栈
protocol_manager_handle_t mgr = protocol_manager_create();
protocol_stack_config_t cfg = protocol_stack_config_default("my-stack");
protocol_stack_handle_t stack = protocol_manager_create_stack(mgr, &cfg);

// 3. 注册适配器
protocol_adapter_t http = *protocol_adapter_http();
protocol_stack_register_adapter(stack, http);

// 4. 创建并发送消息
unified_message_t msg = unified_message_create(
    PROTOCOL_HTTP, DIRECTION_REQUEST, "/api/v1/chat",
    payload, payload_size);
protocol_stack_send(stack, &msg);

// 5. 接收消息
unified_message_t recv_msg;
if (protocol_stack_receive(stack, &recv_msg, 5000) == 0) {
    // 处理接收到的消息
}

// 6. 清理
unified_message_destroy(&msg);
protocol_manager_destroy_stack(mgr, stack);
protocol_manager_destroy(mgr);
protocols_framework_cleanup();
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
