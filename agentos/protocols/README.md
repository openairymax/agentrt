# Protocols — 统一通信协议栈

**模块路径**: `agentos/protocols/`
**版本**: v0.0.5

## 概述

Protocols 层是 AgentOS 的通信协议规范集合，定义系统内部模块间、服务间以及系统与外部之间的所有通信协议和契约标准。采用五层架构设计（Common/Core/Standards/Integrations/Frameworks），统一的协议体系确保 AgentOS 各组件之间能够以标准化的方式进行可靠通信。协议层使用 C 语言实现，通过 CMake 构建系统管理编译选项。

## 目录结构

```
protocols/
├── common/                         # 公共层 — 统一协议接口与公共实现
│   ├── include/
│   │   └── protocols.h             # 公共协议头文件
│   └── src/
│       ├── unified_protocol.c      # 统一协议接口实现
│       └── protocols_impl.c        # 公共协议实现
├── core/                           # 核心层 — 协议路由、扩展与转换
│   ├── router/
│   │   ├── include/
│   │   │   └── protocol_router.h   # 路由器头文件
│   │   └── src/
│   │       └── protocol_router.c   # 协议路由实现
│   ├── adapter/
│   │   ├── include/
│   │   │   └── protocol_extension_framework.h  # 扩展框架头文件
│   │   └── src/
│   │       └── protocol_extension_framework.c  # 扩展框架实现
│   ├── transformers/
│   │   ├── include/
│   │   │   └── protocol_transformers.h  # 数据转换头文件
│   │   └── src/
│   │       └── protocol_transformers.c  # 数据转换实现
│   └── registry/
│       ├── include/
│       │   └── protocol_registry.h # 注册中心头文件
│       └── src/
│           └── protocol_registry.c # 注册中心实现
├── standards/                      # 标准协议层 — A2A/MCP/AGNTCY 适配
│   ├── a2a/
│   │   ├── include/
│   │   │   └── a2a_v03_adapter.h   # A2A v0.3 适配器头文件
│   │   └── src/
│   │       └── a2a_v03_adapter.c   # A2A v0.3 适配器实现
│   ├── mcp/
│   │   ├── include/
│   │   │   ├── mcp_v1_adapter.h    # MCP v1 适配器头文件
│   │   │   └── mcp_transport.h     # MCP 传输层头文件
│   │   └── src/
│   │       ├── mcp_v1_adapter.c    # MCP v1 适配器实现
│   │       └── mcp_transport.c     # MCP 传输层实现
│   └── agntcy/
│       ├── include/
│       │   └── agntcy_acp_adapter.h  # AGNTCY ACP 适配器头文件
│       └── src/
│           └── agntcy_acp_adapter.c  # AGNTCY ACP 适配器实现
├── integrations/                   # 集成适配层 — 主流 AI 平台集成
│   ├── openai/
│   │   ├── include/
│   │   │   └── openai_enterprise_adapter.h
│   │   └── src/
│   │       └── openai_enterprise_adapter.c
│   ├── claude/
│   │   ├── include/
│   │   │   └── claude_adapter.h
│   │   └── src/
│   │       └── claude_adapter.c
│   ├── openjiuwen/
│   │   ├── include/
│   │   │   └── openjiuwen_adapter.h
│   │   └── src/
│   │       └── openjiuwen_adapter.c
│   ├── openclaw/
│   │   ├── include/
│   │   │   └── openclaw_adapter.h
│   │   └── src/
│   │       └── openclaw_adapter.c
│   └── china_eco/
│       ├── include/
│       │   └── china_eco_adapter.h
│       └── src/
│           └── china_eco_adapter.c
├── frameworks/                     # 框架适配层 — AI 框架集成
│   ├── langchain/
│   │   ├── include/
│   │   │   └── langchain_adapter.h
│   │   └── src/
│   │       └── langchain_adapter.c
│   └── autogen/
│       ├── include/
│       │   └── autogen_adapter.h
│       └── src/
│           └── autogen_adapter.c
├── include/                        # 顶层公共头文件
│   ├── agentos_protocol_interface.h
│   ├── unified_protocol.h
│   └── protocol_router.h
├── src/                            # 顶层实现
│   ├── agentos_protocol_interface.c
│   └── protocol_toplevel_impl.c
├── tests/                          # 测试套件
│   ├── test_agntcy_acp.c
│   ├── test_openclaw_adapter.c
│   └── test_china_eco_crypto.c
├── CMakeLists.txt                  # CMake 构建配置
└── README.md                       # 本文件
```

## 协议层次

```
┌─────────────────────────────────────────────────────────────────────┐
│                     Frameworks Layer (框架适配层)                     │
│  ┌──────────────────┐  ┌──────────────────┐                        │
│  │ langchain_adapter │  │  autogen_adapter  │                        │
│  └──────────────────┘  └──────────────────┘                        │
├─────────────────────────────────────────────────────────────────────┤
│                  Integrations Layer (集成适配层)                      │
│  ┌───────────────┐ ┌──────────────┐ ┌─────────────┐ ┌───────────┐ │
│  │openai_enterprise│ │openjiuwen    │ │ openclaw    │ │  claude   │ │
│  │   _adapter     │ │  _adapter    │ │  _adapter   │ │ _adapter  │ │
│  └───────────────┘ └──────────────┘ └─────────────┘ └───────────┘ │
│  ┌───────────────┐                                                  │
│  │ china_eco     │                                                  │
│  │  _adapter     │                                                  │
│  └───────────────┘                                                  │
├─────────────────────────────────────────────────────────────────────┤
│                   Standards Layer (标准协议层)                        │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌───────────┐ │
│  │a2a_v03       │ │ mcp_v1       │ │ mcp_transport│ │ agntcy_acp│ │
│  │  _adapter    │ │  _adapter    │ │              │ │  _adapter │ │
│  └──────────────┘ └──────────────┘ └──────────────┘ └───────────┘ │
├─────────────────────────────────────────────────────────────────────┤
│                     Core Layer (核心层)                              │
│  ┌──────────────┐ ┌──────────────────────┐ ┌───────────────────┐  │
│  │protocol_router│ │protocol_extension    │ │protocol_          │  │
│  │              │ │   _framework         │ │  transformers     │  │
│  └──────────────┘ └──────────────────────┘ └───────────────────┘  │
│  ┌──────────────┐                                                   │
│  │protocol_     │                                                   │
│  │  registry    │                                                   │
│  └──────────────┘                                                   │
├─────────────────────────────────────────────────────────────────────┤
│                    Common Layer (公共层)                              │
│  ┌──────────────────────┐  ┌──────────────────┐                    │
│  │ unified_protocol.c   │  │ protocols_impl.c │                    │
│  └──────────────────────┘  └──────────────────┘                    │
└─────────────────────────────────────────────────────────────────────┘
```

## 各层说明

| 层次 | 组件 | 职责 |
|------|------|------|
| **Common Layer** | `unified_protocol.c`, `protocols_impl.c` | 统一协议接口与公共实现，定义跨层共享的数据结构和工具函数 |
| **Core Layer** | `protocol_router.c`, `protocol_extension_framework.c`, `protocol_transformers.c`, `protocol_registry.c` | 协议路由（请求分发）、扩展框架（适配器管理）、数据转换（格式适配）与注册中心（协议发现） |
| **Standards Layer** | `a2a_v03_adapter.c`, `mcp_v1_adapter.c`, `mcp_transport.c`, `agntcy_acp_adapter.c` | A2A（Agent-to-Agent）、MCP（Model Context Protocol）、AGNTCY ACP 等标准协议适配 |
| **Integrations Layer** | `openai_enterprise_adapter.c`, `openjiuwen_adapter.c`, `openclaw_adapter.c`, `claude_adapter.c`, `china_eco_adapter.c` | 主流 AI 平台与生态集成适配（OpenAI/Claude/九文/OpenClaw/中国AI生态） |
| **Frameworks Layer** | `langchain_adapter.c`, `autogen_adapter.c` | LangChain、AutoGen 等 AI 框架适配 |

## 构建选项

协议层支持通过 CMake 选项控制各适配器的编译：

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `PROTOCOLS_ENABLE_OPENCLAW` | OFF | OpenClaw 协议适配器 |
| `PROTOCOLS_ENABLE_CLAUDE` | ON | Claude 协议适配器 |
| `PROTOCOLS_ENABLE_LANGCHAIN` | ON | LangChain 框架适配器 |
| `PROTOCOLS_ENABLE_AUTOGEN` | ON | AutoGen 框架适配器 |
| `PROTOCOLS_ENABLE_AGNTCY` | ON | AGNTCY ACP 协议适配器 |
| `PROTOCOLS_ENABLE_CHINA_ECO` | ON | 中国 AI 生态适配器 |
| `PROTOCOLS_ENABLE_MCP` | ON（Windows 上为 OFF） | MCP 协议适配器 |

## 内部协议: Binder IPC

CoreKern 的核心通信协议，用于进程间通信。

### 消息格式

```
┌─────────────────────────────────────────────┐
│              Binder Message Header            │
├──────────┬──────────┬──────────┬────────────┤
│  Magic   │ Version  │ Msg Type │   Flags    │
│ (4 bytes)│ (1 byte) │ (1 byte) │ (2 bytes)  │
├──────────┴──────────┴──────────┴────────────┤
│              Sender ID (8 bytes)             │
├─────────────────────────────────────────────┤
│              Target ID (8 bytes)             │
├─────────────────────────────────────────────┤
│              Sequence No (8 bytes)           │
├─────────────────────────────────────────────┤
│              Payload Length (8 bytes)        │
├─────────────────────────────────────────────┤
│              Payload (variable)              │
└─────────────────────────────────────────────┘
```

### 消息类型

| 类型 | 值 | 说明 |
|------|-----|------|
| REQUEST | 0x01 | 请求消息 |
| RESPONSE | 0x02 | 响应消息 |
| NOTIFICATION | 0x03 | 通知消息（无需响应） |
| ERROR | 0x04 | 错误消息 |
| EVENT | 0x05 | 事件广播 |

## 服务间协议: JSON-RPC 2.0

分布式服务间通信采用 JSON-RPC 2.0 协议，支持方法调用、事件通知和批量请求。

### 请求格式

```json
{
  "jsonrpc": "2.0",
  "id": "req-001",
  "method": "llm.generate",
  "params": {
    "model": "gpt-4",
    "messages": [{"role": "user", "content": "Hello"}]
  }
}
```

### 响应格式

```json
{
  "jsonrpc": "2.0",
  "id": "req-001",
  "result": {
    "content": "Hello! How can I help you?",
    "usage": {"prompt_tokens": 10, "completion_tokens": 8}
  }
}
```

### 错误格式

```json
{
  "jsonrpc": "2.0",
  "id": "req-001",
  "error": {
    "code": -32603,
    "message": "Internal error",
    "data": {"detail": "Model 'gpt-4' is currently overloaded"}
  }
}
```

## 外部协议

AgentOS 通过 Gateway 层对外暴露多种标准协议：

| 协议 | 用途 | 端口 | 传输 |
|------|------|------|------|
| HTTP RESTful | API 管理接口 | 8080 | TCP |
| WebSocket | 实时消息推送 | 8081 | TCP |
| MQTT | IoT 设备通信 | 1883 | TCP |
| gRPC | 高性能服务调用 | 50051 | HTTP/2 |
| SSE | 服务端推送事件 | 8080 | HTTP |

## 错误码体系

统一的错误码规范，确保跨模块的错误传递一致性：

| 范围 | 类别 | 说明 |
|------|------|------|
| 0xxx | 成功 | 操作成功完成 |
| 1xxx | 通用错误 | 参数错误、权限不足等 |
| 2xxx | 内核错误 | 资源不足、调度失败等 |
| 3xxx | 协议错误 | 格式错误、版本不匹配等 |
| 4xxx | 服务错误 | 服务不可用、超时等 |
| 5xxx | 应用错误 | 业务逻辑错误 |
| 9xxx | 内部错误 | 系统内部异常 |

## 协议契约

协议契约定义了每个服务的接口规范，采用 Protocol Buffers 或 OpenAPI 3.0 格式：

```protobuf
service LLMService {
    rpc Generate(GenerateRequest) returns (GenerateResponse);
    rpc StreamGenerate(GenerateRequest) returns (stream GenerateResponse);
    rpc Embed(EmbedRequest) returns (EmbedResponse);
}
```

## 与相关模块的关系

- **Gateway**: 对外协议的转换和路由
- **CoreKern**: Binder IPC 是 CoreKern 的核心通信机制
- **Daemon 服务**: 各守护进程通过 JSON-RPC 2.0 进行服务间通信
- **Toolkit**: SDK 实现中包含协议客户端库
- **OpenLab**: 所有 OpenLab 模块通过 JSON-RPC 2.0 与核心运行时通信

---

© 2026 SPHARX Ltd. All Rights Reserved.
