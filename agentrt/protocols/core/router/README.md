# Router — 协议路由引擎

**模块路径**: `agentrt/protocols/core/router/`
**版本**: v0.1.0

## 概述

协议路由引擎（Protocol Router）是 Core 层的消息路由核心，支持 MCP/A2A/OpenAI API 等协议的自适应路由和转换。基于可配置的规则引擎，路由器根据消息的源协议类型和端点模式匹配转换规则，自动将消息路由到目标协议并执行格式转换。

## 目录结构

```
router/
├── README.md
├── include/
│   └── protocol_router.h          # 路由引擎头文件
└── src/
    └── protocol_router.c          # 路由引擎实现
```

## 核心组件

### 数据类型

| 类型 | 说明 |
|------|------|
| `protocol_rule_t` | 路由规则，定义源/目标协议类型、源/目标端点模式（支持通配符）、优先级、转换器上下文 |
| `message_transformer_t` | 消息转换函数类型，接收源消息和上下文，输出目标消息 |
| `route_decision_func_t` | 路由决策函数类型，根据消息和规则列表选择匹配规则 |
| `protocol_router_handle_t` | 路由引擎实例句柄 |

### 核心 API

| 函数 | 说明 |
|------|------|
| `protocol_router_create()` | 创建路由引擎实例，指定默认协议（无匹配规则时使用） |
| `protocol_router_destroy()` | 销毁路由引擎实例 |
| `protocol_router_add_rule()` | 添加协议转换规则，可选自定义转换函数（NULL 则使用默认转换） |
| `protocol_router_route()` | 路由单条消息：匹配规则 → 执行转换 → 输出目标消息 |
| `protocol_router_route_batch()` | 批量路由消息，返回成功路由的消息数量 |
| `protocol_router_set_decision_func()` | 设置自定义路由决策函数 |
| `protocol_router_get_stats()` | 获取路由统计信息（JSON 格式） |

### 预定义转换器

| 转换器 | 说明 |
|--------|------|
| `protocol_transformer_jsonrpc_to_mcp()` | JSON-RPC → MCP 协议转换 |
| `protocol_transformer_mcp_to_jsonrpc()` | MCP → JSON-RPC 协议转换 |
| `protocol_transformer_openai_to_jsonrpc()` | OpenAI API → JSON-RPC 协议转换 |
| `protocol_transformer_a2a_to_jsonrpc()` | A2A → JSON-RPC 协议转换 |
| `protocol_transformer_default()` | 默认转换器（直接复制，无格式转换） |

### 路由流程

```
输入消息 → 规则匹配 → [匹配?]
                        ├─ 是 → 执行转换器 → 输出目标消息
                        └─ 否 → 使用默认协议直接传递（不转换）
```

## 依赖关系

| 依赖 | 来源 | 用途 |
|------|------|------|
| `unified_protocol.h` | `protocols/include/` | 统一消息模型与协议类型 |
| `memory_compat.h` | `commons/utils/compat/` | 内存管理宏 |
| `error.h` | `commons/utils/` | 错误码定义 |

## 使用说明

```c
#include "protocol_router.h"

// 创建路由引擎（默认协议为 HTTP/JSON-RPC）
protocol_router_handle_t router = protocol_router_create(PROTOCOL_HTTP);

// 添加路由规则
protocol_rule_t rule = {
    .source_protocol = PROTO_JSONRPC,
    .target_protocol = PROTO_MCP,
    .source_endpoint = "/mcp/*",
    .target_endpoint = "/mcp/v1",
    .priority = 10,
    .transformer_context = NULL,
};
protocol_router_add_rule(router, &rule, protocol_transformer_jsonrpc_to_mcp);

// 路由消息
unified_message_t transformed;
int result = protocol_router_route(router, &input_msg, &transformed);

// 批量路由
unified_message_t outputs[10];
int routed = protocol_router_route_batch(router, messages, 10, outputs);

// 获取统计
char *stats_json = NULL;
protocol_router_get_stats(router, &stats_json);

// 清理
protocol_router_destroy(router);
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
