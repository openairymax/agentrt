# Protocols — 通信协议定义

**路径**: `agentos/protocols/`

Protocols 层是 AgentOS 的**通信协议规范集合**，定义系统内部模块间、服务间以及系统与外部之间的所有通信协议和契约标准。统一的协议体系确保 AgentOS 各组件之间能够以标准化的方式进行可靠通信。

---

## 协议层次

AgentOS 的协议体系分为三个层次：

```
┌─────────────────────────────────────────────────────┐
│           External Protocols (外部协议)               │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌────────┐ │
│  │ HTTP    │  │WebSocket│  │   MQTT  │  │ gRPC   │ │
│  │ RESTful │  │         │  │         │  │        │ │
│  └─────────┘  └─────────┘  └─────────┘  └────────┘ │
├─────────────────────────────────────────────────────┤
│          Inter-Service Protocols (服务间协议)         │
│  ┌───────────────────────────────────────────────┐  │
│  │           JSON-RPC 2.0 over IPC                │  │
│  │  服务注册 · 服务发现 · 远程调用 · 事件广播      │  │
│  └───────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────┤
│            Internal IPC (内部进程间通信)              │
│  ┌───────────────────────────────────────────────┐  │
│  │              Binder Protocol                    │  │
│  │  消息头 + 负载 · 同步/异步 · 零拷贝传输        │  │
│  └───────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────┘
```

---

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

---

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
    "messages": [
      {"role": "user", "content": "Hello"}
    ]
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
    "usage": {
      "prompt_tokens": 10,
      "completion_tokens": 8
    }
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
    "data": {
      "detail": "Model 'gpt-4' is currently overloaded"
    }
  }
}
```

---

## 外部协议

AgentOS 通过 Gateway 层对外暴露多种标准协议：

| 协议 | 用途 | 端口 | 传输 |
|------|------|------|------|
| HTTP RESTful | API 管理接口 | 8080 | TCP |
| WebSocket | 实时消息推送 | 8081 | TCP |
| MQTT | IoT 设备通信 | 1883 | TCP |
| gRPC | 高性能服务调用 | 50051 | HTTP/2 |
| SSE | 服务端推送事件 | 8080 | HTTP |

---

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

---

## 协议契约

协议契约定义了每个服务的接口规范，包括方法签名、参数类型和返回格式。契约文件采用 Protocol Buffers 或 OpenAPI 3.0 格式定义：

```protobuf
// 示例: LLM 服务契约
service LLMService {
    rpc Generate(GenerateRequest) returns (GenerateResponse);
    rpc StreamGenerate(GenerateRequest) returns (stream GenerateResponse);
    rpc Embed(EmbedRequest) returns (EmbedResponse);
}
```

---

## 与相关模块的关系

- **Gateway**: 对外协议的转换和路由
- **CoreKern**: Binder IPC 是 CoreKern 的核心通信机制
- **Daemon 服务**: 各守护进程通过 JSON-RPC 2.0 进行服务间通信
- **Toolkit**: SDK 实现中包含协议客户端库

---

© 2026 SPHARX Ltd. All Rights Reserved.
