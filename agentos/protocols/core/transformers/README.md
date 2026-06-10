# Transformers — 协议消息转换器

**模块路径**: `agentos/protocols/core/transformers/`
**版本**: v0.1.0

## 概述

协议消息转换器（Protocol Transformers）实现了 AgentOS 支持的所有协议之间的双向消息格式转换。转换器以 JSON-RPC 2.0 作为内部统一中间格式，实现与 MCP、A2A、OpenAI API、OpenJiuwen 等协议的互操作。转换规则遵循 `agentos_contract/protocol_contract.md` 中定义的协议契约。

## 目录结构

```
transformers/
├── README.md
├── include/
│   └── protocol_transformers.h     # 转换器头文件
└── src/
    └── protocol_transformers.c     # 转换器实现
```

## 核心组件

### 转换上下文

| 类型 | 说明 |
|------|------|
| `transform_context_t` | 转换上下文，携带源/目标协议名称、Agent ID、Session ID、Trace ID、JSON-RPC ID 计数器等元数据 |

### JSON-RPC ↔ MCP 转换器

| 函数 | 说明 |
|------|------|
| `transformer_jsonrpc_to_mcp_request()` | JSON-RPC `skill.execute` → MCP `tools/call` |
| `transformer_mcp_to_jsonrpc_response()` | MCP `tools/call` 响应 → JSON-RPC 结果 |
| `transformer_mcp_tools_list_to_jsonrpc()` | MCP `tools/list` 响应 → JSON-RPC `skill.list` |

映射规则：
- `jsonrpc.method = "skill.execute"` → `mcp.method = "tools/call"`
- `jsonrpc.params.name` → `mcp.params.name`
- `jsonrpc.params.arguments` → `mcp.params.arguments`
- `mcp.result.content[]` → `jsonrpc.result.output`

### JSON-RPC ↔ A2A 转换器

| 函数 | 说明 |
|------|------|
| `transformer_jsonrpc_to_a2a_task()` | JSON-RPC `task.submit` → A2A `task/delegate` |
| `transformer_a2a_to_jsonrpc_response()` | A2A task 响应 → JSON-RPC |
| `transformer_jsonrpc_to_a2a_discover()` | JSON-RPC `agent.discover` → A2A `agent/discover` |
| `transformer_a2a_agents_to_jsonrpc()` | A2A agent card 列表 → JSON-RPC `agent.list` |

### JSON-RPC ↔ OpenAI API 转换器

| 函数 | 说明 |
|------|------|
| `transformer_jsonrpc_to_openai_chat()` | JSON-RPC `llm.complete` → OpenAI `/v1/chat/completions` |
| `transformer_openai_chat_to_jsonrpc()` | OpenAI chat completions 响应 → JSON-RPC |
| `transformer_openai_stream_chunk_to_jsonrpc()` | OpenAI streaming chunk → JSON-RPC notification |
| `transformer_jsonrpc_to_openai_embedding()` | JSON-RPC embedding 请求 → OpenAI `/v1/embeddings` |

映射规则：
- `jsonrpc.params.messages` → `openai.messages` (role/content)
- `jsonrpc.params.model` → `openai.model`
- `jsonrpc.params.temperature` → `openai.temperature`
- `jsonrpc.params.tools` → `openai.tools/functions[]`
- `openai.choices[0].message.content` → `jsonrpc.result.content`
- `openai.usage` → `jsonrpc.result.usage`

### JSON-RPC ↔ OpenJiuwen 转换器

| 函数 | 说明 |
|------|------|
| `transformer_jsonrpc_to_openjiuwen()` | JSON-RPC → OpenJiuwen 自定义二进制格式 |
| `transformer_openjiuwen_to_jsonrpc()` | OpenJiuwen 响应 → JSON-RPC |

OpenJiuwen 二进制协议格式：`Header(24B) + Payload(variable) + CRC32(4B)`

### 通用工具函数

| 函数 | 说明 |
|------|------|
| `protocol_auto_transform()` | 根据源/目标协议自动选择转换器 |
| `protocol_validate_transformed()` | 验证转换后消息的完整性 |
| `protocol_list_transformers()` | 获取所有已注册转换器名称列表 |

### 自动转换端点映射

| 端点模式 | 目标协议 |
|----------|----------|
| `/mcp/(*)` | MCP |
| `/a2a/(*)` | A2A |
| `/v1/chat/(*)` | OpenAI |
| `/ojw/(*)` | OpenJiuwen |

## 依赖关系

| 依赖 | 来源 | 用途 |
|------|------|------|
| `unified_protocol.h` | `protocols/include/` | 统一消息模型与协议类型 |
| `memory_compat.h` | `commons/utils/compat/` | 内存管理宏 |

## 使用说明

```c
#include "protocol_transformers.h"

// 创建转换上下文
transform_context_t *ctx = transform_context_create("jsonrpc", "mcp");

// JSON-RPC → MCP 转换
unified_message_t mcp_msg;
transformer_jsonrpc_to_mcp_request(&jsonrpc_msg, &mcp_msg, ctx);

// MCP → JSON-RPC 转换
unified_message_t jsonrpc_resp;
transformer_mcp_to_jsonrpc_response(&mcp_resp, &jsonrpc_resp, ctx);

// 自动转换（根据端点自动选择转换器）
unified_message_t target;
protocol_auto_transform(&source, &target, "mcp");

// 验证转换结果
if (protocol_validate_transformed(&target) == 0) {
    // 转换成功，使用 target 消息
}

// 列出所有转换器
const char **names = NULL;
size_t count = 0;
names = protocol_list_transformers(&count);

// 清理
transform_context_destroy(ctx);
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
