# AgentOS 协议兼容性集成指南

**版本**: Doc V1.8 | **最后更新**: 2026-04-13 | **作者**: AgentOS Team

## 概述

AgentOS 网关提供统一的协议兼容层，支持 JSON-RPC 2.0、MCP、A2A、OpenAI API 四种协议的接入、检测与互转。本文档详细说明每种协议的集成方法、消息格式映射和常见集成场景。

---

## 一、协议架构总览

```
                    ┌─────────────────────────────────┐
                    │       AgentOS Gateway            │
                    │                                  │
  JSON-RPC ────────►│  ┌─────────────────────────┐    │
  MCP ─────────────►│  │    Protocol Router       │    │
  A2A ─────────────►│  │  (自动检测 + 路由 + 转换) │    │
  OpenAI API ──────►│  └──────────┬──────────────┘    │
                    │             │                    │
                    │  ┌──────────▼──────────────┐    │
                    │  │   Unified JSON-RPC Core  │    │
                    │  └──────────┬──────────────┘    │
                    │             │                    │
                    │  ┌──────────▼──────────────┐    │
                    │  │    IPC Service Bus       │    │
                    │  └─────────────────────────┘    │
                    └─────────────────────────────────┘
```

**核心组件**:
- **Protocol Router**: 协议路由器，负责自动检测、路由分发和协议转换
- **Gateway Protocol Handler**: 网关协议处理器，处理各协议特有逻辑
- **Unified JSON-RPC Core**: 统一JSON-RPC核心，所有协议最终转为JSON-RPC内部调用

---

## 二、JSON-RPC 2.0 集成

### 2.1 标准请求格式

```json
{
  "jsonrpc": "2.0",
  "method": "agent.list",
  "params": {"filter": "active"},
  "id": 1
}
```

### 2.2 支持的方法

| 方法 | 说明 | 参数 |
|------|------|------|
| `agent.list` | 列出智能体 | `{filter?: string}` |
| `agent.create` | 创建智能体 | `{name: string, config: object}` |
| `agent.delete` | 删除智能体 | `{agent_id: string}` |
| `skill.list` | 列出技能 | `{agent_id?: string}` |
| `skill.execute` | 执行技能 | `{skill_id: string, params: object}` |
| `task.list` | 列出任务 | `{status?: string}` |
| `task.create` | 创建任务 | `{type: string, params: object}` |
| `service.list` | 列出服务 | `{protocol?: string}` |
| `config.get` | 获取配置 | `{key: string}` |
| `config.set` | 设置配置 | `{key: string, value: any}` |

### 2.3 批量请求

```json
[
  {"jsonrpc": "2.0", "method": "agent.list", "id": 1},
  {"jsonrpc": "2.0", "method": "service.list", "id": 2},
  {"jsonrpc": "2.0", "method": "task.list", "id": 3}
]
```

### 2.4 通知（无响应）

```json
{
  "jsonrpc": "2.0",
  "method": "log.record",
  "params": {"level": "info", "message": "Task completed"}
}
```

### 2.5 错误响应

```json
{
  "jsonrpc": "2.0",
  "error": {
    "code": -32601,
    "message": "Method not found",
    "data": {"method": "unknown.method"}
  },
  "id": 1
}
```

**标准错误码**:

| 错误码 | 含义 |
|--------|------|
| -32700 | Parse error（解析错误） |
| -32600 | Invalid Request（无效请求） |
| -32601 | Method not found（方法未找到） |
| -32602 | Invalid params（无效参数） |
| -32603 | Internal error（内部错误） |
| -32001 | Agent not found（智能体未找到） |
| -32002 | Service unavailable（服务不可用） |
| -32003 | Permission denied（权限拒绝） |
| -32004 | Circuit breaker open（熔断器开启） |

---

## 三、MCP (Model Context Protocol) 集成

### 3.1 概述

MCP 是专为 LLM 工具集成设计的协议，AgentOS 网关作为 MCP Server 对外提供服务。

### 3.2 工具列表

```json
// 请求
{
  "jsonrpc": "2.0",
  "method": "tools/list",
  "params": {},
  "id": 1
}

// 响应
{
  "jsonrpc": "2.0",
  "result": {
    "tools": [
      {
        "name": "agent_create",
        "description": "创建新的智能体实例",
        "inputSchema": {
          "type": "object",
          "properties": {
            "name": {"type": "string", "description": "智能体名称"},
            "framework": {"type": "string", "enum": ["agent", "memory", "task", "safety", "tool"]}
          },
          "required": ["name"]
        }
      },
      {
        "name": "task_execute",
        "description": "执行智能体任务",
        "inputSchema": {
          "type": "object",
          "properties": {
            "agent_id": {"type": "string"},
            "task_type": {"type": "string"},
            "params": {"type": "object"}
          },
          "required": ["agent_id", "task_type"]
        }
      }
    ]
  },
  "id": 1
}
```

### 3.3 工具调用

```json
// 请求
{
  "jsonrpc": "2.0",
  "method": "tools/call",
  "params": {
    "name": "agent_create",
    "arguments": {
      "name": "my-agent",
      "framework": "agent"
    }
  },
  "id": 2
}

// 响应
{
  "jsonrpc": "2.0",
  "result": {
    "content": [
      {
        "type": "text",
        "text": "Agent 'my-agent' created successfully with ID: ag_abc123"
      }
    ]
  },
  "id": 2
}
```

### 3.4 资源访问

```json
// 请求
{
  "jsonrpc": "2.0",
  "method": "resources/list",
  "params": {},
  "id": 3
}

// 响应
{
  "jsonrpc": "2.0",
  "result": {
    "resources": [
      {
        "uri": "agentos://agents",
        "name": "Active Agents",
        "description": "当前活跃的智能体列表",
        "mimeType": "application/json"
      },
      {
        "uri": "agentos://tasks",
        "name": "Task Queue",
        "description": "当前任务队列",
        "mimeType": "application/json"
      }
    ]
  },
  "id": 3
}
```

### 3.5 MCP → JSON-RPC 映射

| MCP 方法 | JSON-RPC 方法 | 说明 |
|----------|--------------|------|
| `tools/list` | `skill.list` | 工具映射为技能 |
| `tools/call` | `skill.execute` | 工具调用映射为技能执行 |
| `resources/list` | `service.list` | 资源映射为服务 |
| `resources/read` | `config.get` | 资源读取映射为配置获取 |

### 3.6 Claude Desktop 集成配置

```json
// claude_desktop_config.json
{
  "mcpServers": {
    "agentos": {
      "command": "npx",
      "args": ["-y", "@agentos/mcp-adapter"],
      "env": {
        "AGENTOS_GATEWAY_URL": "http://localhost:18789",
        "AGENTOS_API_KEY": "your-api-key"
      }
    }
  }
}
```

---

## 四、A2A (Agent-to-Agent) 集成

### 4.1 概述

A2A 协议支持智能体之间的发现、协商与任务委派，是构建多智能体协作系统的基础。

### 4.2 智能体发现

```json
// 请求
{
  "jsonrpc": "2.0",
  "method": "agent/discover",
  "params": {
    "capability": "code-generation",
    "framework": "agent",
    "status": "active"
  },
  "id": 1
}

// 响应
{
  "jsonrpc": "2.0",
  "result": {
    "agents": [
      {
        "id": "ag_code_gen_01",
        "name": "Code Generator",
        "capabilities": ["code-generation", "code-review"],
        "framework": "agent",
        "status": "active",
        "endpoint": "ipc://agent.code_gen"
      }
    ]
  },
  "id": 1
}
```

### 4.3 任务委派

```json
// 请求
{
  "jsonrpc": "2.0",
  "method": "task/delegate",
  "params": {
    "target_agent_id": "ag_code_gen_01",
    "task": {
      "type": "code-generation",
      "description": "Generate a Python REST API",
      "params": {
        "language": "python",
        "framework": "fastapi",
        "endpoints": ["/users", "/items"]
      }
    },
    "priority": "normal",
    "timeout_ms": 30000
  },
  "id": 2
}

// 响应
{
  "jsonrpc": "2.0",
  "result": {
    "task_id": "tsk_abc123",
    "status": "accepted",
    "estimated_duration_ms": 15000
  },
  "id": 2
}
```

### 4.4 A2A → JSON-RPC 映射

| A2A 方法 | JSON-RPC 方法 | 说明 |
|----------|--------------|------|
| `agent/discover` | `agent.list` | 发现映射为列表查询 |
| `task/delegate` | `task.create` | 委派映射为任务创建 |
| `task/status` | `task.list` | 状态查询映射为任务列表 |
| `agent/capability` | `service.list` | 能力查询映射为服务列表 |

### 4.5 多智能体协作模式

```
┌──────────┐    A2A discover    ┌──────────┐
│  Agent A │ ──────────────────► │  Agent B │
│ (协调者)  │ ◄────────────────── │ (执行者)  │
└──────────┘    A2A delegate     └──────────┘
     │                                  │
     │  JSON-RPC task.create            │ JSON-RPC task.update
     ▼                                  ▼
┌──────────────────────────────────────────┐
│              IPC Service Bus             │
└──────────────────────────────────────────┘
```

---

## 五、OpenAI API 兼容集成

### 5.1 Chat Completions

```bash
curl -X POST http://localhost:18789/v1/chat/completions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $AGENTOS_API_KEY" \
  -d '{
    "model": "agentos-default",
    "messages": [
      {"role": "system", "content": "You are a helpful assistant."},
      {"role": "user", "content": "Create a Python function to sort a list"}
    ],
    "temperature": 0.7,
    "max_tokens": 1000
  }'
```

**响应格式**:

```json
{
  "id": "chatcmpl-abc123",
  "object": "chat.completion",
  "created": 1713000000,
  "model": "agentos-default",
  "choices": [
    {
      "index": 0,
      "message": {
        "role": "assistant",
        "content": "```python\ndef sort_list(lst):\n    return sorted(lst)\n```"
      },
      "finish_reason": "stop"
    }
  ],
  "usage": {
    "prompt_tokens": 25,
    "completion_tokens": 15,
    "total_tokens": 40
  }
}
```

### 5.2 Models 列表

```bash
curl http://localhost:18789/v1/models \
  -H "Authorization: Bearer $AGENTOS_API_KEY"
```

```json
{
  "object": "list",
  "data": [
    {"id": "agentos-default", "object": "model", "owned_by": "agentos"},
    {"id": "agentos-code-gen", "object": "model", "owned_by": "agentos"},
    {"id": "agentos-reasoning", "object": "model", "owned_by": "agentos"}
  ]
}
```

### 5.3 OpenAI → JSON-RPC 映射

| OpenAI 端点 | JSON-RPC 方法 | 说明 |
|-------------|--------------|------|
| `POST /v1/chat/completions` | `llm.chat` | 聊天补全映射为LLM调用 |
| `GET /v1/models` | `service.list` | 模型列表映射为服务列表 |
| `POST /v1/embeddings` | `llm.embed` | 嵌入映射为LLM嵌入 |

### 5.4 流式响应 (SSE)

```bash
curl -X POST http://localhost:18789/v1/chat/completions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $AGENTOS_API_KEY" \
  -d '{
    "model": "agentos-default",
    "messages": [{"role": "user", "content": "Hello"}],
    "stream": true
  }'
```

---

## 六、协议自动检测

### 6.1 检测规则

网关按以下优先级自动检测入站请求的协议类型：

| 优先级 | 检测条件 | 协议 |
|--------|---------|------|
| 1 | URL路径以 `/v1/` 开头 + Authorization头 | OpenAI API |
| 2 | URL路径包含 `/mcp` | MCP |
| 3 | URL路径包含 `/a2a` | A2A |
| 4 | 请求体包含 `jsonrpc: "2.0"` | JSON-RPC |
| 5 | 请求体包含 `method: "tools/*"` | MCP (回退) |
| 6 | 请求体包含 `method: "agent/*"` | A2A (回退) |

### 6.2 统一接入点

```bash
# 所有协议均可通过统一端点接入，网关自动检测
curl -X POST http://localhost:18789/api/v1/invoke \
  -H "Content-Type: application/json" \
  -d '<任意协议格式的请求>'
```

---

## 七、协议转换深度参考

### 7.1 转换矩阵

```
           → JSON-RPC    → MCP         → A2A         → OpenAI
JSON-RPC   Identity      tools/list    agent/list    N/A
MCP        skill.list    Identity      agent/list    N/A
A2A        agent.list    tools/list    Identity      N/A
OpenAI     llm.chat      tools/call    task/delegate Identity
```

### 7.2 转换中的数据保留

- **请求ID**: 始终保留原始ID用于追踪
- **时间戳**: 添加转换时间戳到元数据
- **协议标记**: 在响应中标记原始协议类型
- **错误传播**: 转换错误保留原始错误信息

---

## 八、集成测试验证

### 8.1 运行兼容性测试

```bash
# 完整测试套件
python3 tests/integration/test_protocol_compatibility.py \
  --gateway http://localhost:18789 --verbose

# 仅测试特定协议
python3 tests/integration/test_protocol_compatibility.py \
  --gateway http://localhost:18789 --category jsonrpc

# CI环境运行
# 由 .gitcode/workflows/protocol-compatibility.yml 自动执行
```

### 8.2 测试覆盖矩阵

| 测试类别 | JSON-RPC | MCP | A2A | OpenAI |
|----------|----------|-----|-----|--------|
| 基本请求 | ✅ | ✅ | ✅ | ✅ |
| 批量请求 | ✅ | - | - | - |
| 错误处理 | ✅ | ✅ | ✅ | ✅ |
| 协议转换 | ✅→MCP | ✅→RPC | ✅→RPC | ✅→RPC |
| 自动检测 | ✅ | ✅ | ✅ | ✅ |
| 性能基准 | ✅ | ✅ | ✅ | ✅ |

---

## 参考文档

| 文档 | 路径 |
|------|------|
| 快速入门 | [quickstart_protocol_gateway.md](quickstart_protocol_gateway.md) |
| 最佳实践 | [best_practices.md](best_practices.md) |
| 故障排查 | [troubleshooting_faq.md](troubleshooting_faq.md) |
| API文档 | [Doxygen生成](../../docs/api/) |
