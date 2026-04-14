# AgentOS 协议系统使用指南

**版本**: 1.0.0
**最后更新**: 2026-04-14
**状态**: 稳定
**许可证**: MIT

AgentOS 协议系统提供统一的 多协议通信能力，支持 **JSON-RPC 2.0**、**MCP v1.0**、**A2A v0.3**、**OpenAI API**、**OpenJiuwen（自定义二进制）**、**OpenClaw（离线AI Agent平台）** 和 **Claude API（Anthropic）** 共 **7种协议**，以及 **LangChain** 和 **AutoGen** 框架适配，具备自动检测、路由和双向转换功能。

## 目录

1. [架构概述](#1-架构概述)
2. [支持的协议](#2-支持的协议)
3. [协议检测](#3-协议检测)
4. [SDK 使用指南](#4-sdk-使用指南)
   - [Python SDK](#python-sdk)
   - [Go SDK](#go-sdk)
   - [Rust SDK](#rust-sdk)
   - [TypeScript SDK](#typescript-sdk)
5. [协议转换](#5-协议转换)
6. [框架适配器](#6-框架适配器)
   - [LangChain 适配器](#langchain-适配器)
   - [AutoGen 适配器](#autogen-适配器)
7. [OpenLab 集成](#7-openlab-集成)
8. [网关桥接](#8-网关桥接)
9. [CLI 工具参考](#9-cli-工具参考)
10. [高级主题](#10-高级主题)
11. [故障排除](#11-故障排除)

---

## 1. 架构概述

### 分层协议架构

```
┌─────────────────────────────────────────────────────┐
│              应用层 (OpenLab)                         │
│         ProtocolSessionManager / ProtocolHandler     │
├─────────────────────────────────────────────────────┤
│              SDK 客户端层 (Toolkit)                  │
│    ProtocolClient (Go/Python/Rust/TypeScript)       │
├─────────────────────────────────────────────────────┤
│         框架适配层 (LangChain / AutoGen)             │
│  LCEL Chain · Agent · Memory · GroupChat            │
├─────────────────────────────────────────────────────┤
│              网关桥接层                               │
│      gateway_protocol_bridge (自动检测 + 路由)       │
├─────────────────────────────────────────────────────┤
│              核心协议层                              │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌────────┐ │
│  │ JSON-RPC │ │   MCP    │ │   A2A    │ │ OpenAI │ │
│  │ Adapter  │ │ Adapter  │ │ Adapter  │ │Adapter │ │
│  └──────────┘ └──────────┘ └──────────┘ └────────┘ │
│  ┌──────────┐ ┌──────────┐                         │
│  │OpenJiuwen│ │OpenClaw  │  Claude API Adapter     │
│  │ Adapter  │ │ Adapter  │                         │
│  └──────────┘ └──────────┘                         │
│  ┌────────────────────────────────────────────────┐ │
│  │          协议转换器 (Protocol Transformers)      │ │
│  │  双向转换: JSON-RPC <-> MCP/A2A/OpenAI/...     │ │
│  └────────────────────────────────────────────────┘ │
│  ┌────────────────┐ ┌─────────────────────────────┐ │
│  │ Protocol Router │ │ Protocol Extension Manager   │ │
│  └────────────────┘ └─────────────────────────────┘ │
│  ┌────────────────────────────────────────────────┐ │
│  │        Protocol Registry (注册中心)             │ │
│  │  7内置协议 / 5分类 / 8态状态机 / 依赖追踪       │ │
│  └────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────┤
│            接口层 (I-L1 ~ I-L4)                     │
│  proto_adapter_vtable / proto_router_iface / ...    │
└─────────────────────────────────────────────────────┘
```

### 接口层说明 (I-L1 ~ I-L4)

| 层级 | 接口 | 职责 |
|------|------|------|
| **I-L1** | `proto_adapter_vtable_t` | 单个协议适配器的生命周期管理 (init/destroy/handle/version) |
| **I-L2** | `proto_router_iface_t` | 协议间消息路由和转换 |
| **I-L3** | `proto_gateway_iface_t` | 协议网关注册、请求处理、自动检测 |
| **I-L4** | `proto_extension_mgr_iface_s` | 动态扩展加载/卸载/自动发现 |

### 核心组件

| 组件 | 位置 | 说明 |
|------|------|------|
| 统一接口 | `interfaces/include/agentos_protocol_interface.h` | I-L1~I-L4 接口定义 |
| 协议路由器 | `protocols/core/router/` | 消息路由与分发 |
| 协议转换器 | `protocols/core/transformers/` | 双向格式转换 |
| 协议注册中心 | `protocols/core/registry/` | 7内置协议注册/状态机/依赖追踪 |
| MCP 适配器 | `protocols/adapters/mcp/` | MCP v1.0 协议实现 |
| A2A 适配器 | `protocols/standards/a2a/` | A2A v0.3 代理间协议 |
| OpenAI 适配器 | `protocols/integrations/openai/` | OpenAI 企业 API 兼容 |
| OpenJiuwen 适配器 | `protocols/integrations/openjiuwen/` | 自定义二进制协议，含 CRC32 |
| **OpenClaw 适配器** | `protocols/integrations/openclaw/` | **离线私有化 AI Agent 平台集成** |
| **Claude API 适配器** | `protocols/integrations/claude/` | **Anthropic Claude API 集成** |
| **LangChain 框架适配** | `protocols/frameworks/langchain/` | **LCEL Chain / Agent / Memory / Tool** |
| **AutoGen 框架适配** | `protocols/frameworks/autogen/` | **多 Agent 对话 / Group Chat / Code Executor** |
| 扩展框架 | `protocols/core/adapter/` | 动态适配器加载系统 (I-L4) |
| 网关桥接 | `gateway/src/gateway/gateway_protocol_bridge.c` | 网关 ↔ 协议集成 (7协议检测+registry集成) |

---

## 2. 支持的协议

### 2.1 JSON-RPC 2.0

AgentOS 内部通信的默认和主要协议。

**端点**: `/jsonrpc`
**版本**: 2.0
**Content-Type**: `application/json`

```json
{
  "jsonrpc": "2.0",
  "method": "task.submit",
  "params": { "content": "Hello World" },
  "id": "req-001"
}
```

### 2.2 MCP (Model Context Protocol) v1.0

AI 模型上下文管理的标准化协议。

**端点**: `/mcp`
**版本**: 1.0
**Content-Type**: `application/json`

```json
{
  "jsonrpc": "2.0",
  "method": "tools/list",
  "id": 1
}
```

支持的 MCP 核心方法：
- `tools/list` — 列出可用工具
- `tools/call` — 按名称执行工具
- `initialize` — 会话初始化
- `ping` — 健康检查

### 2.3 A2A (Agent-to-Agent) v0.3

多代理协调与任务委托协议。

**端点**: `/a2a`
**版本**: 0.3
**Content-Type**: `application/json`

```json
{
  "type": "task_delegation",
  "agent_id": "agent-001",
  "task": {
    "description": "Analyze data",
    "input_data": { "source": "database" }
  },
  "priority": "high"
}
```

核心 A2A 操作：
- 代理发现与注册
- 带协商的任务委托
- 基于共识的决策
- 流式进度更新

### 2.4 OpenAI API 兼容

用于 LLM 交互的 OpenAI 兼容 API。

**端点**: `/v1/chat/completions`, `/v1/embeddings`
**版本**: 1.0 (兼容)
**Content-Type**: `application/json`

```json
{
  "model": "gpt-4o",
  "messages": [
    { "role": "user", "content": "Hello" }
  ],
  "temperature": 0.7,
  "max_tokens": 1024
}
```

内置模型：
- `gpt-4o`, `gpt-4o-mini`, `gpt-4-turbo` (聊天补全)
- `text-embedding-ada-002`, `text-embedding-3-small`, `text-embedding-3-large` (向量嵌入)

### 2.5 OpenJiuwen (自定义二进制)

高性能自定义二进制协议，带 CRC32 完整性校验。

**魔数**: `0x4F4A574D` ("OJWM")
**头部大小**: 24 字节
**特性**: 二进制载荷、CRC32 校验和、带版本格式

二进制头部结构：
```
偏移量  大小  字段
0       4     魔数 (0x4F4A574D)
4       2     版本 (uint16)
6       2     消息类型 (uint16)
8       4     载荷长度 (uint32)
12      4     序列号 (uint32)
16      4     时间戳 (uint32)
20      4     CRC32 校验和 (uint32)
```

### 2.6 OpenClaw (离线 AI Agent 平台)

OpenClaw 是离线私有化部署的 AI Agent 平台，支持多模态交互与集群协作。

**端点**: `/openclaw`
**版本**: 1.0
**特性**: 4种运行模式 / 5级安全等级 / 6种模态

```json
{
  "agent_id": "agent-001",
  "action": "send_message",
  "session_id": "sess-abc123",
  "payload": {
    "content": "分析数据趋势",
    "modality": "text",
    "priority": "normal"
  },
  "security_level": 3,
  "cluster_mode": "standalone"
}
```

核心 OpenClaw 能力：
- **Agent 生命周期**: 注册 → 发现 → 注销 + 心跳保活
- **会话管理**: 创建/关闭会话，消息收发
- **任务委派**: 带超时和优先级的任务分发
- **集群模式**: standalone / clustered / hybrid / embedded
- **安全分级**: L0(开放) → L4(最高机密)，5级权限控制
- **多模态**: 文本 / 图像 / 音频 / 视频 / 文件 / 代码

### 2.7 Claude API (Anthropic)

Anthropic Claude 大语言模型 API 集成，支持 Extended Thinking 和 Tool Use。

**端点**: `/v1/messages`
**版本**: 1.0
**Content-Type**: `application/json`

```json
{
  "model": "claude-sonnet-4-20250514",
  "max_tokens": 4096,
  "messages": [
    { "role": "user", "content": "解释 MCIS 理论" }
  ],
  "tools": [
    {
      "name": "web_search",
      "description": "搜索互联网信息",
      "input_schema": {
        "type": "object",
        "properties": { "query": { "type": "string" } }
      }
    }
  ]
}
```

内置模型支持：

| 模型 | 类型 | 最大上下文 | 特性 |
|------|------|-----------|------|
| `claude-opus-4` | 超旗舰 | 200K | 最强推理能力 |
| `claude-sonnet-4` | 旗舰 | 200K | 高性能平衡 |
| `claude-haiku-4` | 轻量 | 200K | 低延迟快速响应 |
| `claude-3.5-sonnet` | 前代旗舰 | 200K | Extended Thinking |
| `claude-3.5-haiku` | 前代轻量 | 200K | 快速工具调用 |
| `claude-3-opus` | 经典旗舰 | 200K | 复杂推理 |

核心 Claude 能力：
- **Messages API**: 完整的对话接口（同步+流式）
- **Tool Use**: 结构化工具调用与结果回传
- **Extended Thinking**: 深度思考模式（思维链输出）
- **Vision**: 图像理解与分析
- **Token 计数**: 精确的 token 用量统计
- **Prompt Caching**: 提示词缓存优化成本

---

## 3. 协议检测

系统支持基于内容分析的自动协议检测：

### 检测启发式规则

| 信号 | 置信度 | 检测到的协议 |
|------|--------|-------------|
| Content-Type: `application/json-rpc` 或报文体中有 `jsonrpc` 字段 | 95% | JSON-RPC |
| Content-Type: `application/mcp` 或 MCP 特定方法名 | 90% | MCP |
| Content-Type: `application/a2a` 或 `agent_id` + `task` 字段 | 85% | A2A |
| URL 路径以 `/v1/` 开头或包含 `model`+`messages` 字段 | 88% | OpenAI |
| 带魔数 `0x4F4A574D` 的二进制数据 | 100% | OpenJiuwen |
| 包含 `openclaw`/`cluster_mode`/`security_level` 字段 | 88% | OpenClaw |
| 包含 `claude-`/`anthropic-version`/`max_tokens`+Anthropic特征 | 90% | Claude |
| 默认回退 | — | JSON-RPC |

### SDK 自动检测示例

```python
from agentos.protocol import ProtocolClient, ProtocolType

client = ProtocolClient.from_env()
result = await client.detect_protocol(
    content='{"model":"gpt-4","messages":[{"role":"user","content":"Hi"}]}',
    content_type="application/json"
)
print(f"检测到: {result.protocol_type}")  # ProtocolType.OPENAI
print(f"置信度: {result.confidence:.1%}")
```

---

## 4. SDK 使用指南

### Python SDK

#### 安装

```bash
pip install agentos-toolkit
```

#### 快速开始

```python
import asyncio
from agentos.protocol import (
    ProtocolClient,
    ProtocolType,
    ProtocolConfig,
)

async def main():
    config = ProtocolConfig(
        base_url="http://localhost:18789",
        api_key="your-api-key",
        default_protocol=ProtocolType.JSONRPC,
    )

    client = ProtocolClient(config)

    result = await client.send_request(
        method="task.submit",
        params={"content": "分析销售数据", "priority": "high"},
    )
    print(f"响应: {result}")

    streaming = await client.stream_request(
        method="llm.chat",
        params={"prompt": "介绍 AgentOS"},
    )
    async for chunk in streaming:
        print(chunk, end="", flush=True)

asyncio.run(main())
```

#### 协议专用客户端（工厂函数）

```python
from agentos.protocol import create_mcp_client, create_openai_client

mcp_client = create_mcp_client(base_url="http://localhost:18789")
tools = await mcp_client.list_tools()
result = await mcp_client.call_tool("browser_navigate", {"url": "https://example.com"})

openai_client = create_openai_client(
    base_url="http://localhost:18789",
    api_key="your-key",
    model="gpt-4o",
)
response = await openai_client.chat("解释 MCIS 理论")
```

#### 完整 API 参考

```python
class ProtocolClient:
    async def detect_protocol(self, content, content_type=None) -> ProtocolDetectionResult
    async def send_request(self, method, params=None, protocol=None, **kwargs) -> dict
    async def stream_request(self, method, params=None, protocol=None) -> AsyncGenerator[str, None]
    async def list_protocols(self) -> list[dict]
    async def test_connection(self, protocol=None) -> ConnectionTestResult
    async def get_capabilities(self, protocol=None) -> ProtocolCapabilities

class ProtocolConfig:
    base_url: str
    api_key: str
    timeout: int
    max_retries: int
    default_protocol: ProtocolType

    @classmethod
    def from_env(cls) -> ProtocolConfig

class ProtocolType(IntEnum):
    JSONRPC = 0
    MCP = 1
    A2A = 2
    OPENAI = 3
    OPENJIUWEN = 4
```

---

### Go SDK

#### 安装

```bash
go get github.com/spharxworks/agentos-toolkit-go
```

#### 快速开始

```go
package main

import (
    "context"
    "fmt"
    "log"

    agentos "github.com/spharxworks/agentos-toolkit-go"
    "github.com/spharxworks/agentos-toolkit-go/protocol"
)

func main() {
    ctx := context.Background()

    cfg := protocol.NewConfig(
        protocol.WithBaseURL("http://localhost:18789"),
        protocol.WithAPIKey("your-api-key"),
    )

    client := protocol.NewProtocolClient(cfg)

    result, err := client.DetectProtocol(ctx, &protocol.DetectInput{
        Content:      `{"model":"gpt-4","messages":[{"role":"user","content":"hi"}]}`,
        ContentType:  "application/json",
    })
    if err != nil {
        log.Fatal(err)
    }
    fmt.Printf("检测到: %s (置信度: %.1f%%)\n",
        result.ProtocolType, result.Confidence*100)

    resp, err := client.SendRequest(ctx, &protocol.RequestOptions{
        Method:  "task.submit",
        Params:  map[string]interface{}{"content": "Hello from Go"},
    })
    if err != nil {
        log.Fatal(err)
    }
    fmt.Printf("响应: %v\n", resp)
}
```

#### 流式响应示例

```go
stream, err := client.StreamRequest(ctx, &protocol.RequestOptions{
    Method: "llm.chat",
    Params: map[string]interface{}{"prompt": "Tell me about AgentOS"},
})
if err != nil {
    log.Fatal(err)
}

for chunk := range stream.Ch() {
    fmt.Print(chunk.Data)
}
```

#### 完整 API 参考

```go
type ProtocolClient struct { ... }

func NewProtocolClient(cfg *ProtocolConfig) *ProtocolClient
func (c *ProtocolClient) DetectProtocol(ctx context.Context, input *DetectInput) (*DetectionResult, error)
func (c *ProtocolClient) SendRequest(ctx context.Context, opts *RequestOptions) (*Response, error)
func (c *ProtocolClient) StreamRequest(ctx context.Context, opts *RequestOptions) (*StreamResponse, error)
func (c *ProtocolClient) ListProtocols(ctx context.Context) ([]ProtocolInfo, error)
func (c *ProtocolClient) TestConnection(ctx context.Context, proto ProtocolType) (*ConnectionTestResult, error)
func (c *ProtocolClient) GetCapabilities(ctx context.Context, proto ProtocolType) (*ProtocolCapabilities, error)

type ProtocolConfig struct { ... }
func NewConfig(opts ...ConfigOption) *ProtocolConfig
func WithBaseURL(url string) ConfigOption
func WithAPIKey(key string) ConfigOption
func WithTimeout(d time.Duration) ConfigOption

type ProtocolType int
const (
    ProtocolJSONRPC  ProtocolType = iota
    ProtocolMCP
    ProtocolA2A
    ProtocolOpenAI
    ProtocolOpenJiuwen
)
```

---

### Rust SDK

#### 安装

在 `Cargo.toml` 中添加：

```toml
[dependencies]
agentos-toolkit = "3.0.0"
tokio = { version = "1", features = ["full"] }
```

#### 快速开始

```rust
use agentos_toolkit::protocol::{
    ProtocolClient, ProtocolConfig, ProtocolType, DetectionResult,
};
use tokio;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let config = ProtocolConfig::builder()
        .base_url("http://localhost:18789")
        .api_key("your-api-key")
        .build()?;

    let client = ProtocolClient::new(config);

    let detection = client.detect_protocol(
        r#"{"jsonrpc":"2.0","method":"ping","params":{}}"#,
        Some("application/json"),
    ).await?;

    println!("检测到: {} ({:.1}%)", detection.protocol_type, detection.confidence * 100.0);

    let response = client.send_request(
        "task.submit",
        Some(serde_json::json!({"content": "Hello from Rust"})),
        None,
    ).await?;

    println!("响应: {:?}", response);

    Ok(())
}
```

#### 流式响应与错误处理

```rust
use agentos_toolkit::protocol::{ProtocolClient, ProtocolConfig};

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let config = ProtocolConfig::builder()
        .base_url("http://localhost:18789")
        .build()?;
    let client = ProtocolClient::new(config);

    let mut stream = client.stream_request(
        "llm.chat",
        Some(serde_json::json!({"prompt": "Explain MCIS"})),
        None,
    ).await?;

    while let Some(chunk) = stream.next().await {
        match chunk {
            Ok(data) => print!("{}", data),
            Err(e) => eprintln!("流式错误: {}", e),
        }
    }

    Ok(())
}
```

#### 完整 API 参考

```rust
pub struct ProtocolClient { /* ... */ }

impl ProtocolClient {
    pub fn new(config: ProtocolConfig) -> Self;
    pub async fn detect_protocol(&self, content: &str, content_type: Option<&str>) -> Result<DetectionResult>;
    pub async fn send_request(&self, method: &str, params: Option<Value>, protocol: Option<ProtocolType>) -> Result<Value>;
    pub async fn stream_request(&self, method: &str, params: Option<Value>, protocol: Option<ProtocolType>) -> Result<Pin<Box<dyn Stream<Item = Result<String>>>>>;
    pub async fn list_protocols(&self) -> Result<Vec<ProtocolInfo>>;
    pub async fn test_connection(&self, protocol: Option<ProtocolType>) -> Result<ConnectionTestResult>;
    pub async fn get_capabilities(&self, protocol: Option<ProtocolType>) -> Result<ProtocolCapabilities>;
}

pub struct ProtocolConfig { /* ... */ }
impl ProtocolConfig {
    pub fn builder() -> ProtocolConfigBuilder;
}

pub enum ProtocolType {
    JsonRpc,
    Mcp,
    A2a,
    OpenAi,
    OpenJiuwen,
}
impl Display for ProtocolType { /* ... */ }

pub struct DetectionResult {
    pub protocol_type: ProtocolType,
    pub confidence: f64,
    pub details: Value,
}
```

---

### TypeScript SDK

#### 安装

```bash
npm install @agentos/toolkit
# 或
yarn add @agentos/toolkit
```

#### 快速开始

```typescript
import {
  ProtocolClient,
  ProtocolType,
  createProtocolClient,
  createMCPClient,
  createOpenAIClient,
} from '@agentos/toolkit';

async function main() {
  const client = createProtocolClient({
    baseURL: 'http://localhost:18789',
    apiKey: 'your-api-key',
  });

  const detection = await client.detectProtocol({
    content: '{"model":"gpt-4","messages":[{"role":"user","content":"hi"}]}',
    contentType: 'application/json',
  });
  console.log(`检测到: ${detection.protocolType} (${(detection.confidence * 100).toFixed(1)}%)`);

  const response = await client.sendRequest({
    method: 'task.submit',
    params: { content: 'Hello from TypeScript' },
  });
  console.log('响应:', response);
}

main();
```

#### 工厂函数

```typescript
const mcpClient = createMCPClient({ baseURL: 'http://localhost:18789' });
const tools = await mcpClient.listTools();
const result = await mcpClient.callTool('search', { query: 'AgentOS' });

const openaiClient = createOpenAIClient({
  baseURL: 'http://localhost:18789',
  apiKey: 'your-key',
  model: 'gpt-4o',
});
const chat = await openaiClient.chat('什么是 MCIS 理论?');
```

#### 完整 API 参考

```typescript
enum ProtocolType {
  JSONRPC = 0,
  MCP = 1,
  A2A = 2,
  OPENAI = 3,
  OPENJIUWEN = 4,
}

interface ProtocolConfigOptions {
  baseURL?: string;
  apiKey?: string;
  timeout?: number;
  defaultProtocol?: ProtocolType;
  maxRetries?: number;
}

interface ProtocolDetectionResult {
  protocolType: ProtocolType;
  confidence: number;
  details: Record<string, any>;
}

interface ConnectionTestResult {
  connected: boolean;
  latencyMs: number;
  protocolVersion: string;
  error?: string;
}

interface ProtocolCapabilities {
  supportedMethods: string[];
  features: string[];
  maxPayloadSize: number;
  streamingSupported: boolean;
}

class ProtocolClient {
  constructor(options?: ProtocolConfigOptions);
  detectProtocol(input: { content: string; contentType?: string }): Promise<ProtocolDetectionResult>;
  sendRequest(options: { method: string; params?: any; protocol?: ProtocolType }): Promise<any>;
  streamRequest(options: { method: string; params?: any; protocol?: ProtocolType }): AsyncIterable<string>;
  listProtocols(): Promise<any[]>;
  testConnection(protocol?: ProtocolType): Promise<ConnectionTestResult>;
  getCapabilities(protocol?: ProtocolType): Promise<ProtocolCapabilities>;
}

function createProtocolClient(options?: ProtocolConfigOptions): ProtocolClient;
function createMCPClient(options?: ProtocolConfigOptions): ProtocolClient;
function createOpenAIClient(options?: ProtocolConfigOptions): ProtocolClient;
```

---

## 5. 协议转换

协议转换器模块支持任意两种支持的协议之间的双向消息转换。

### 支持的转换路径

| 源 → 目标 | 函数 | 状态 |
|-----------|------|------|
| JSON-RPC → MCP | `transformer_jsonrpc_to_mcp_request` | ✅ |
| MCP → JSON-RPC | `transformer_mcp_to_jsonrpc_request` | ✅ |
| JSON-RPC → A2A | `transformer_jsonrpc_to_a2a_request` | ✅ |
| A2A → JSON-RPC | `transformer_a2a_to_jsonrpc_request` | ✅ |
| JSON-RPC → OpenAI | `transformer_jsonrpc_to_openai_request` | ✅ |
| OpenAI → JSON-RPC | `transformer_openai_to_jsonrpc_request` | ✅ |
| JSON-RPC → OpenJiuwen | `transformer_jsonrpc_to_openjiuwen_request` | ✅ |
| OpenJiuwen → JSON-RPC | `transformer_openjiuwen_to_jsonrpc_request` | ✅ |

### 使用自动转换

路由器的 `protocol_auto_transform()` 函数自动选择正确的转换路径：

```c
#include "protocol_transformers.h"

int result = protocol_auto_transform(
    source_message,
    target_buffer,
    &target_size,
    source_type,    // PROTO_JSONRPC
    target_type,    // PROTO_MCP
    TRANSFORM_REQUEST,
    NULL            // context
);
```

### 转换示例

#### JSON-RPC → MCP (tools/call 映射)

```json
// 输入 (JSON-RPC):
{"jsonrpc":"2.0","method":"tool.execute","params":{"name":"web_search","query":"AgentOS"},"id":1}

// 输出 (MCP):
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"web_search","arguments":{"query":"AgentOS"}},"id":1}
```

#### JSON-RPC → OpenAI (chat/completions 映射)

```json
// 输入 (JSON-RPC):
{"jsonrpc":"2.0","method":"llm.chat","params":{"prompt":"Hello","max_tokens":100},"id":1}

// 输出 (OpenAI):
{"model":"gpt-4o","messages":[{"role":"user","content":"Hello"}],"max_tokens":100,"temperature":0.7}
```

#### JSON-RPC → OpenJiuwen (二进制编码)

```c
// 输入: JSON-RPC 文本消息
// 输出: 24字节头部 + 二进制载荷 + CRC32
// 魔数: 0x4F4A574D, 类型: MSG_TYPE_REQUEST
```

---

## 6. 框架适配器

AgentOS 协议系统提供对主流 AI 开发框架的适配支持，使 AgentOS 能与 LangChain 和 AutoGen 等框架无缝协作。

### 6.1 LangChain 适配器

将 LangChain 的核心概念映射到 AgentOS 协议层。

**位置**: `protocols/frameworks/langchain/`

#### 概念映射

| LangChain 概念 | AgentOS 映射 | 说明 |
|----------------|-------------|------|
| LCEL Chain | Pipeline (协议会话) | 编译后的执行链 |
| Agent | Protocol Session + Handler | 带工具调用的智能体 |
| Tool | MCP Tool | 通过 executor 回调执行 |
| LLM / ChatModel | LLM Daemon | 大语言模型接口 |
| Memory | MemoryRovol (L1-L4) | 四层滚动记忆 |
| Retriever | memory.search() | 记忆检索 |
| OutputParser | Transformer | 输出格式转换 |

#### 支持的 Chain 类型

| Chain 类型 | 说明 |
|-----------|------|
| Sequential | 线性步骤链（按序执行） |
| Router | 条件路由链（按条件分支） |
| MapReduce | 映射归约链（并行处理+聚合） |
| Parallel | 并行链（多路同时执行） |
| Conditional | 条件判断链 |

#### 支持的 Agent 类型

| Agent 类型 | 说明 |
|-----------|------|
| ReAct | 推理-行动循环 |
| PlanAndExecute | 先规划后执行 |
| OpenAIFunctions | OpenAI 函数调用风格 |
| StructuredChat | 结构化对话 |
| XML | XML 格式推理 |

#### 使用示例

```c
#include "langchain_adapter.h"

langchain_adapter_t* lc = langchain_adapter_create();

langchain_tool_def_t tool = {
    .name = "calculator",
    .description = "数学计算",
    .executor = calculator_execute,
};
langchain_register_tool(lc, &tool);

langchain_chain_def_t chain = {
    .type = LANGCHAIN_CHAIN_SEQUENTIAL,
    .steps = { "input", "llm", "output" },
    .step_count = 3,
};
langchain_create_chain(lc, &chain, "my_chain");

char* result = NULL;
langchain_execute_chain(lc, "my_chain", "{\"query\":\"1+1\"}", &result);
printf("结果: %s\n", result);  // {"result": 2}
free(result);

// 流式输出
langchain_stream_handler_t stream_handler = {
    .on_chunk = my_chunk_callback,
    .user_data = NULL,
};
langchain_execute_chain_streaming(lc, "my_chain", "{\"query\":\"分析\"}", &stream_handler);

langchain_adapter_destroy(lc);
```

### 6.2 AutoGen 适配器

将 Microsoft AutoGen 多代理框架集成到 AgentOS。

**位置**: `protocols/frameworks/autogen/`

#### 概念映射

| AutoGen 概念 | AgentOS 映射 | 说明 |
|-------------|-------------|------|
| ConversableAgent | Agent + Session | 可对话代理 |
| GroupChat | A2A 协议协调 | 多代理群组对话 |
| UserProxyAgent | Human-in-Loop | 人工介入回调 |
| CodeExecutor | Tool Sandbox | 代码执行沙箱 |

#### 支持的角色类型

| 角色 | 说明 |
|------|------|
| UserProxy | 用户代理（人工交互） |
| Assistant | 助手代理（通用响应） |
| Coder | 编程代理（代码生成） |
| Planner | 规划代理（任务分解） |
| Researcher | 研究代理（信息检索） |
| Reviewer | 审查代理（质量检查） |
| Custom | 自定义角色 |

#### Group Chat 模式

| 模式 | 说明 |
|------|------|
| RoundRobin | 轮流发言 |
| SpeakerSelection | 选择下一个发言者 |
| RAG | 检索增强生成模式 |
| Sequential | 顺序执行模式 |
| Parallel | 并行处理模式 |
| Custom | 自定义调度逻辑 |

#### 使用示例

```c
#include "autogen_adapter.h"

autogen_adapter_t* ag = autogen_adapter_create();

autogen_agent_config_t planner_cfg = {
    .name = "planner",
    .role = AUTOGEN_ROLE_PLANNER,
    .system_message = "你是一个任务规划专家，负责将复杂问题分解为子任务。",
};
autogen_create_agent(ag, &planner_cfg);

autogen_agent_config_t coder_cfg = {
    .name = "coder",
    .role = AUTOGEN_ROLE_CODER,
    .system_message = "你是一个编程专家，负责编写高质量代码。",
};
autogen_create_agent(ag, &coder_cfg);

autogen_group_chat_config_t gc_cfg = {
    .mode = AUTOGEN_CHAT_ROUND_ROBIN,
    .max_rounds = 10,
    .participants = { "planner", "coder" },
    .participant_count = 2,
};
autogen_create_group_chat(ag, &gc_cfg, "dev_team");

autogen_chat_result_t result;
autogen_initiate_chat(ag, "dev_team", "设计一个REST API服务", &result);
printf("完成轮次: %d\n", result.rounds_completed);
printf("消息历史: %zu 条\n", result.message_count);

autogen_adapter_destroy(ag);
```

---

## 6. OpenLab 集成

`openlab.protocols` 模块提供 Python 绑定，用于将协议系统集成到 OpenLab 应用中。

### ProtocolSessionManager

```python
from openlab.protocols import (
    ProtocolSessionManager,
    ProtocolSessionConfig,
    JSONRPCHandler,
    MCPHandler,
)

config = ProtocolSessionConfig(
    gateway_url="http://localhost:18789",
    default_protocol="jsonrpc",
    auto_detect=True,
)

manager = ProtocolSessionManager(config)
await manager.initialize()

await manager.discover_tools()

result = await manager.execute_tool("calculator_add", {"a": 10, "b": 20})
print(f"结果: {result.output}")

await manager.close()
```

### 自定义协议处理器

```python
from openlab.protocols import ProtocolHandler, ProtocolRequestContext, ProtocolResponse

class MyCustomHandler(ProtocolHandler):
    @property
    def name(self) -> str:
        return "my_custom"

    @property
    def supported_methods(self) -> list[str]:
        return ["custom.ping", "custom.echo"]

    async def handle(self, ctx: ProtocolRequestContext) -> ProtocolResponse:
        if ctx.method == "custom.ping":
            return ProtocolResponse.success({"pong": True, "timestamp": time.time()})
        elif ctx.method == "custom.echo":
            return ProtocolResponse.success({"echo": ctx.params.get("data")})
        return ProtocolResponse.error("unknown_method", f"未知方法: {ctx.method}")

manager.register_handler(MyCustomHandler())
```

### 工具绑定模式

| 模式 | 行为 |
|------|------|
| `AUTO` | 自动绑定发现到的工具 |
| `MANUAL` | 需要显式注册 |
| `LAZY` | 首次使用时绑定 |

---

## 7. 网关桥接

`gateway_protocol_bridge` 模块将协议系统集成到 HTTP/WebSocket 网关层。

### 配置

```c
#include "gateway_protocol_bridge.h"

gateway_bridge_config_t config = {
    .gateway_url = "http://localhost:18789",
    .auto_detect = true,
    .default_protocol = PROTO_JSONRPC,
    .max_payload_size = 10 * 1024 * 1024,
    .enable_stats = true,
};
```

### 注册流程

```c
gateway_bridge_handle_t* bridge = gateway_bridge_create(&config);

gateway_bridge_register_handler(bridge, PROTO_JSONRPC, jsonrpc_handler_fn);
gateway_bridge_register_handler(bridge, PROTO_MCP, mcp_handler_fn);
gateway_bridge_register_handler(bridge, PROTO_A2A, a2a_handler_fn);
gateway_bridge_register_handler(bridge, PROTO_OPENAI, openai_handler_fn);
```

### 统计与诊断

```c
char* diagnostics = gateway_bridge_get_diagnostics(bridge, true);
printf("%s\n", diagnostics);
free(diagnostics);

gateway_bridge_stats_t stats;
gateway_bridge_get_statistics(bridge, &stats);
printf("总请求数: %llu\n", stats.total_requests);
printf("转换次数: %llu\n", stats.transform_count);
```

---

## 8. CLI 工具参考

`agentos` CLI 工具包含协议管理命令：

```bash
agentos protocol list              # 列出所有可用协议适配器
agentos protocol detect           # 从输入检测协议
agentos protocol test <proto>     # 测试特定协议的连接
agentos protocol send <proto> <method> [params]  # 发送协议消息
agentos protocol stats            # 显示协议统计信息
agentos protocol transform <src> <tgt> [data]    # 协议间转换
```

### 使用示例

```bash
$ agentos protocol list
协议          版本     状态      端点           类型
────────────  ──────   ──────    ───────────    ──────────
JSON-RPC      2.0      active    /jsonrpc        标准
MCP           1.0      active    /mcp            标准
A2A           0.3      active    /a2a            标准
OpenAI        1.0      active    /v1            集成
OpenJiuwen    1.0      active    /ojiuwen       集成
OpenClaw      1.0      active    /openclaw      集成
Claude API    1.0      active    /v1/messages   集成

$ agentos protocol stats
协议统计:
  总请求数:     1,234
  按协议分布:
    JSON-RPC:  856 (69.4%)
    MCP:       234 (19.0%)
    A2A:       98  (7.9%)
    OpenAI:    46  (3.7%)
    OpenClaw:  12  (1.0%)
    Claude:     8  (0.6%)
    OpenJiuwen: 4  (0.3%)
  转换次数:    45
  错误次数:    3
  注册中心:    7 协议已注册
  扩展适配器:  2 (LangChain + AutoGen)
```

---

## 9. 高级主题

### 9.1 自定义协议适配器开发

添加新协议适配器的步骤：

1. 实现 `proto_adapter_vtable_t` 接口
2. 通过 `proto_extension_mgr_iface_s.load()` 注册
3. 在分发表中添转换器条目

```c
#include "agentos_protocol_interface.h"

static int my_adapter_init(proto_adapter_t* adapter, void* config) { return 0; }
static int my_adapter_destroy(proto_adapter_t* adapter) { return 0; }
static int my_adapter_handle(proto_adapter_t* adapter, proto_message_t* msg) { return 0; }
static const char* my_adapter_version(const proto_adapter_t* adapter) { return "1.0"; }

static proto_adapter_vtable_t my_vtable = {
    .init = my_adapter_init,
    .destroy = my_adapter_destroy,
    .handle = my_adapter_handle,
    .version = my_adapter_version,
};
```

### 9.2 流式协议模式

所有协议通过统一流式 API 支持流式响应：

- **JSON-RPC**: 通过 `/jsonrpc/stream` 的 Server-Sent Events (SSE)
- **MCP**: 带增量内容的 `notifications/message`
- **A2A**: 基于阶段的进度回调
- **OpenAI**: 原生 SSE 流式响应 (`stream: true`)
- **OpenJiuwen**: 带序列号的分块二进制帧

### 9.3 安全注意事项

- 所有适配器根据 `max_payload_size` 验证输入大小
- OpenJiuwen 使用 CRC32 进行完整性校验
- API 密钥认证通过 `Authorization: Bearer <token>` 头
- 每个协议端点均可配置速率限制
- 通过 Cupolas 安全穹顶进行输入清理

---

## 10. 故障排除

### 常见问题

| 问题 | 原因 | 解决方案 |
|------|------|---------|
| `协议未检测到` | 缺少 content-type 头 | 在请求中显式设置 `contentType` |
| `转换失败` | 不兼容的字段类型 | 检查字段兼容性矩阵 |
| `连接超时` | 网关未运行 | 先运行 `agentos service health` |
| `CRC32 不匹配` | OpenJiuwen 载荷损坏 | 重新发送消息 |
| `未知方法` | 协议处理器未注册 | 检查 `protocol list` 输出 |

### 调试模式

启用详细日志：

```bash
export AGENTOS_DEBUG=true
export AGENTOS_LOG_LEVEL=DEBUG
agentos protocol test jsonrpc --verbose
```

### 健康检查序列

```bash
agentos status                    # 整体系统状态
agentos service health            # 网关健康状态
agentos protocol list             # 可用协议列表
agentos protocol test jsonrpc     # 各协议连接测试
agentos protocol test mcp
agentos protocol test openai
```

---

## 版本历史

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.1.0 | 2026-04-14 | 新增 OpenClaw/Claude 协议文档 + LangChain/AutoGen 框架适配器文档 + 注册中心说明 + 网关7协议检测 |
| 1.0.0 | 2026-04-14 | 初始版本 — 完整的协议系统文档 |

## 相关文档

- [统一接口规范](../interfaces/include/agentos_protocol_interface.h)
- [协议路由器设计](../protocols/core/router/include/protocol_router.h)
- [转换器实现](../protocols/core/transformers/include/protocol_transformers.h)
- [网关桥接 API](../gateway/include/gateway_protocol_bridge.h)
- [OpenLab 协议绑定](../openlab/openlab/protocols/__init__.py)
- [AgentOS 架构](../../docs/Capital_Architecture/)
- [快速入门指南](../../docs/Capital_Guides/getting_started.md)
