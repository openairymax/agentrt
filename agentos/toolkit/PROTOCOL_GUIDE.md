# AgentOS Protocol System Guide

**Version**: 1.0.0
**Last Updated**: 2026-04-14
**Status**: Stable
**License**: MIT

AgentOS Protocol System provides unified multi-protocol communication capabilities supporting **JSON-RPC 2.0**, **MCP v1.0**, **A2A v0.3**, **OpenAI API**, and **OpenJiuwen (custom binary)** protocols with automatic detection, routing, and bidirectional transformation.

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Supported Protocols](#2-supported-protocols)
3. [Protocol Detection](#3-protocol-detection)
4. [SDK Usage Guides](#4-sdk-usage-guides)
   - [Python SDK](#python-sdk)
   - [Go SDK](#go-sdk)
   - [Rust SDK](#rust-sdk)
   - [TypeScript SDK](#typescript-sdk)
5. [Protocol Transformation](#5-protocol-transformation)
6. [OpenLab Integration](#6-openlab-integration)
7. [Gateway Bridge](#7-gateway-bridge)
8. [CLI Tool Reference](#8-cli-tool-reference)
9. [Advanced Topics](#9-advanced-topics)
10. [Troubleshooting](#10-troubleshooting)

---

## 1. Architecture Overview

### Layered Protocol Architecture

```
┌─────────────────────────────────────────────────────┐
│              Application Layer (OpenLab)             │
│         ProtocolSessionManager / ProtocolHandler     │
├─────────────────────────────────────────────────────┤
│              SDK Client Layer (Toolkit)              │
│    ProtocolClient (Go/Python/Rust/TypeScript)        │
├─────────────────────────────────────────────────────┤
│              Gateway Bridge Layer                    │
│      gateway_protocol_bridge (auto-detect + route)   │
├─────────────────────────────────────────────────────┤
│              Core Protocol Layer                     │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌────────┐ │
│  │ JSON-RPC │ │   MCP    │ │   A2A    │ │ OpenAI │ │
│  │ Adapter  │ │ Adapter  │ │ Adapter  │ │Adapter │ │
│  └──────────┘ └──────────┘ └──────────┘ └────────┘ │
│  ┌────────────────────────────────────────────────┐ │
│  │          Protocol Transformers                  │ │
│  │  Bidirectional: JSON-RPC <-> MCP/A2A/OpenAI/... │ │
│  └────────────────────────────────────────────────┘ │
│  ┌────────────────┐ ┌─────────────────────────────┐ │
│  │ Protocol Router │ │ Protocol Extension Manager  │ │
│  └────────────────┘ └─────────────────────────────┘ │
├─────────────────────────────────────────────────────┤
│            Interface Layer (I-L1 ~ I-L4)             │
│  proto_adapter_vtable / proto_router_iface / ...     │
└─────────────────────────────────────────────────────┘
```

### Interface Layers (I-L1 ~ I-L4)

| Layer | Interface | Responsibility |
|-------|-----------|----------------|
| **I-L1** | `proto_adapter_vtable_t` | Single protocol adapter lifecycle (init/destroy/handle/version) |
| **I-L2** | `proto_router_iface_t` | Message routing and transformation between protocols |
| **I-L3** | `proto_gateway_iface_t` | Protocol gateway registration, request handling, auto-detection |
| **I-L4** | `proto_extension_mgr_iface_s` | Dynamic extension loading/unloading/auto-discovery |

### Key Components

| Component | Location | Description |
|-----------|----------|-------------|
| Unified Interface | `interfaces/include/agentos_protocol_interface.h` | I-L1~I-L4 interface definitions |
| Protocol Router | `protocols/core/router/` | Message routing and dispatch |
| Protocol Transformers | `protocols/core/transformers/` | Bidirectional format conversion |
| MCP Adapter | `protocols/adapters/mcp/` | MCP v1.0 protocol implementation |
| A2A Adapter | `protocols/standards/a2a/` | A2A v0.3 agent-to-agent protocol |
| OpenAI Adapter | `protocols/integrations/openai/` | OpenAI Enterprise API compatibility |
| OpenJiuwen Adapter | `protocols/integrations/openjiuwen/` | Custom binary protocol with CRC32 |
| Extension Framework | `protocols/core/adapter/` | Dynamic adapter loading system |
| Gateway Bridge | `gateway/src/gateway/gateway_protocol_bridge.c` | Gateway ↔ Protocols integration |

---

## 2. Supported Protocols

### 2.1 JSON-RPC 2.0

The default and primary protocol for AgentOS internal communication.

**Endpoint**: `/jsonrpc`
**Version**: 2.0
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

Standardized protocol for AI model context management.

**Endpoint**: `/mcp`
**Version**: 1.0
**Content-Type**: `application/json`

```json
{
  "jsonrpc": "2.0",
  "method": "tools/list",
  "id": 1
}
```

Key MCP methods supported:
- `tools/list` — List available tools
- `tools/call` — Execute a tool by name
- `initialize` — Session initialization
- `ping` — Health check

### 2.3 A2A (Agent-to-Agent) v0.3

Protocol for multi-agent coordination and task delegation.

**Endpoint**: `/a2a`
**Version**: 0.3
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

Key A2A operations:
- Agent discovery and registration
- Task delegation with negotiation
- Consensus-based decision making
- Streaming progress updates

### 2.4 OpenAI API Compatible

OpenAI-compatible API for LLM interactions.

**Endpoint**: `/v1/chat/completions`, `/v1/embeddings`
**Version**: 1.0 (compatible)
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

Built-in models:
- `gpt-4o`, `gpt-4o-mini`, `gpt-4-turbo` (chat completions)
- `text-embedding-ada-002`, `text-embedding-3-small`, `text-embedding-3-large` (embeddings)

### 2.5 OpenJiuwen (Custom Binary)

High-performance custom binary protocol with CRC32 integrity check.

**Magic Number**: `0x4F4A574D` ("OJWM")
**Header Size**: 24 bytes
**Features**: Binary payload, CRC32 checksum, versioned format

Binary header structure:
```
Offset  Size  Field
0       4     Magic (0x4F4A574D)
4       2     Version (uint16)
6       2     Message Type (uint16)
8       4     Payload Length (uint32)
12      4     Sequence Number (uint32)
16      4     Timestamp (uint32)
20      4     CRC32 Checksum (uint32)
```

---

## 3. Protocol Detection

The system supports automatic protocol detection based on content analysis:

### Detection Heuristics

| Signal | Confidence | Detected Protocol |
|--------|-----------|-------------------|
| Content-Type: `application/json-rpc` or body has `jsonrpc` field | 95% | JSON-RPC |
| Content-Type: `application/mcp` or MCP-specific method names | 90% | MCP |
| Content-Type: `application/a2a` or `agent_id` + `task` fields | 85% | A2A |
| URL path starts with `/v1/` or has `model`+`messages` fields | 88% | OpenAI |
| Binary data with magic `0x4F4A574D` | 100% | OpenJiuwen |
| Default fallback | — | JSON-RPC |

### SDK Auto-Detection Example

```python
from agentos.protocol import ProtocolClient, ProtocolType

client = ProtocolClient.from_env()
result = await client.detect_protocol(
    content='{"model":"gpt-4","messages":[{"role":"user","content":"Hi"}]}',
    content_type="application/json"
)
print(f"Detected: {result.protocol_type}")  # ProtocolType.OPENAI
print(f"Confidence: {result.confidence:.1%}")
```

---

## 4. SDK Usage Guides

### Python SDK

#### Installation

```bash
pip install agentos-toolkit
```

#### Quick Start

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
        params={"content": "Analyze sales data", "priority": "high"},
    )
    print(f"Response: {result}")

    streaming = await client.stream_request(
        method="llm.chat",
        params={"prompt": "Tell me about AgentOS"},
    )
    async for chunk in streaming:
        print(chunk, end="", flush=True)

asyncio.run(main())
```

#### Protocol-Specific Clients (Factory Functions)

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
response = await openai_client.chat("Explain MCIS theory")
```

#### Full API Reference

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

#### Installation

```bash
go get github.com/spharxworks/agentos-toolkit-go
```

#### Quick Start

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
    fmt.Printf("Detected: %s (confidence: %.1f%%)\n",
        result.ProtocolType, result.Confidence*100)

    resp, err := client.SendRequest(ctx, &protocol.RequestOptions{
        Method:  "task.submit",
        Params:  map[string]interface{}{"content": "Hello from Go"},
    })
    if err != nil {
        log.Fatal(err)
    }
    fmt.Printf("Response: %v\n", resp)
}
```

#### Streaming Example

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

#### Full API Reference

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

#### Installation

Add to `Cargo.toml`:

```toml
[dependencies]
agentos-toolkit = "3.0.0"
tokio = { version = "1", features = ["full"] }
```

#### Quick Start

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

    println!("Detected: {} ({:.1}%)", detection.protocol_type, detection.confidence * 100.0);

    let response = client.send_request(
        "task.submit",
        Some(serde_json::json!({"content": "Hello from Rust"})),
        None,
    ).await?;

    println!("Response: {:?}", response);

    Ok(())
}
```

#### Streaming with Error Handling

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
            Err(e) => eprintln!("Stream error: {}", e),
        }
    }

    Ok(())
}
```

#### Full API Reference

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

#### Installation

```bash
npm install @agentos/toolkit
# or
yarn add @agentos/toolkit
```

#### Quick Start

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
  console.log(`Detected: ${detection.protocolType} (${(detection.confidence * 100).toFixed(1)}%)`);

  const response = await client.sendRequest({
    method: 'task.submit',
    params: { content: 'Hello from TypeScript' },
  });
  console.log('Response:', response);
}

main();
```

#### Factory Functions

```typescript
const mcpClient = createMCPClient({ baseURL: 'http://localhost:18789' });
const tools = await mcpClient.listTools();
const result = await mcpClient.callTool('search', { query: 'AgentOS' });

const openaiClient = createOpenAIClient({
  baseURL: 'http://localhost:18789',
  apiKey: 'your-key',
  model: 'gpt-4o',
});
const chat = await openaiClient.chat('What is MCIS theory?');
```

#### Full API Reference

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

## 5. Protocol Transformation

The protocol transformer module enables bidirectional message conversion between any two supported protocols.

### Supported Transformations

| Source → Target | Function | Status |
|-----------------|----------|--------|
| JSON-RPC → MCP | `transformer_jsonrpc_to_mcp_request` | ✅ |
| MCP → JSON-RPC | `transformer_mcp_to_jsonrpc_request` | ✅ |
| JSON-RPC → A2A | `transformer_jsonrpc_to_a2a_request` | ✅ |
| A2A → JSON-RPC | `transformer_a2a_to_jsonrpc_request` | ✅ |
| JSON-RPC → OpenAI | `transformer_jsonrpc_to_openai_request` | ✅ |
| OpenAI → JSON-RPC | `transformer_openai_to_jsonrpc_request` | ✅ |
| JSON-RPC → OpenJiuwen | `transformer_jsonrpc_to_openjiuwen_request` | ✅ |
| OpenJiuwen → JSON-RPC | `transformer_openjiuwen_to_jsonrpc_request` | ✅ |

### Using Auto-Transform

The router's `protocol_auto_transform()` function automatically selects the correct transformation:

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

### Transformation Examples

#### JSON-RPC → MCP (tools/call mapping)

```json
// Input (JSON-RPC):
{"jsonrpc":"2.0","method":"tool.execute","params":{"name":"web_search","query":"AgentOS"},"id":1}

// Output (MCP):
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"web_search","arguments":{"query":"AgentOS"}},"id":1}
```

#### JSON-RPC → OpenAI (chat/completions mapping)

```json
// Input (JSON-RPC):
{"jsonrpc":"2.0","method":"llm.chat","params":{"prompt":"Hello","max_tokens":100},"id":1}

// Output (OpenAI):
{"model":"gpt-4o","messages":[{"role":"user","content":"Hello"}],"max_tokens":100,"temperature":0.7}
```

#### JSON-RPC → OpenJiuwen (binary encoding)

```c
// Input: JSON-RPC text message
// Output: 24-byte header + binary payload + CRC32
// Magic: 0x4F4A574D, Type: MSG_TYPE_REQUEST
```

---

## 6. OpenLab Integration

The `openlab.protocols` module provides Python bindings for integrating the protocol system into OpenLab applications.

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
print(f"Result: {result.output}")

await manager.close()
```

### Custom Protocol Handler

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
        return ProtocolResponse.error("unknown_method", f"Unknown method: {ctx.method}")

manager.register_handler(MyCustomHandler())
```

### Tool Binding Modes

| Mode | Behavior |
|------|----------|
| `AUTO` | Automatically bind discovered tools |
| `MANUAL` | Explicit registration required |
| `LAZY` | Bind on first use |

---

## 7. Gateway Bridge

The `gateway_protocol_bridge` module integrates the protocol system into the HTTP/WebSocket gateway layer.

### Configuration

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

### Registration Flow

```c
gateway_bridge_handle_t* bridge = gateway_bridge_create(&config);

gateway_bridge_register_handler(bridge, PROTO_JSONRPC, jsonrpc_handler_fn);
gateway_bridge_register_handler(bridge, PROTO_MCP, mcp_handler_fn);
gateway_bridge_register_handler(bridge, PROTO_A2A, a2a_handler_fn);
gateway_bridge_register_handler(bridge, PROTO_OPENAI, openai_handler_fn);
```

### Statistics & Diagnostics

```c
char* diagnostics = gateway_bridge_get_diagnostics(bridge, true);
printf("%s\n", diagnostics);
free(diagnostics);

gateway_bridge_stats_t stats;
gateway_bridge_get_statistics(bridge, &stats);
printf("Total requests: %llu\n", stats.total_requests);
printf("Transform count: %llu\n", stats.transform_count);
```

---

## 8. CLI Tool Reference

The `agentos` CLI tool includes protocol management commands:

```bash
agentos protocol list              # List all available protocol adapters
agentos protocol detect           # Detect protocol from input
agentos protocol test <proto>     # Test connection for specific protocol
agentos protocol send <proto> <method> [params]  # Send a protocol message
agentos protocol stats            # Show protocol statistics
agentos protocol transform <src> <tgt> <data>    # Transform between protocols
```

### Examples

```bash
$ agentos protocol list
Protocol         Version    Status      Endpoint
─────────────    ───────    ──────      ────────────
JSON-RPC         2.0        active      /jsonrpc
MCP              1.0        active      /mcp
A2A              0.3        active      /a2a
OpenAI           1.0        active      /v1
OpenJiuwen       1.0        active      /ojiuwen

$ agentos protocol test jsonrpc
Testing JSON-RPC connection to http://localhost:18789/jsonrpc...
✓ Connected (latency: 12ms, version: 2.0)

$ agentos protocol send jsonrpc task.submit '{"content":"Hello CLI"}'
{
  "jsonrpc": "2.0",
  "result": { "task_id": "task-abc123", "status": "pending" },
  "id": "cli-001"
}

$ agentos protocol stats
Protocol Statistics:
  Total Requests:    1,234
  By Protocol:
    JSON-RPC:  856 (69.4%)
    MCP:       234 (19.0%)
    A2A:       98  (7.9%)
    OpenAI:    46  (3.7%)
  Transforms:  45
  Errors:      3
```

---

## 9. Advanced Topics

### 9.1 Custom Protocol Adapter Development

To add a new protocol adapter:

1. Implement `proto_adapter_vtable_t` interface
2. Register via `proto_extension_mgr_iface_s.load()`
3. Add transformer entries to the dispatch table

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

### 9.2 Streaming Protocol Patterns

All protocols support streaming responses through the unified streaming API:

- **JSON-RPC**: Server-Sent Events (SSE) over `/jsonrpc/stream`
- **MCP**: `notifications/message` with incremental content
- **A2A**: Phase-based progress callbacks
- **OpenAI**: Native SSE streaming (`stream: true`)
- **OpenJiuwen**: Chunked binary frames with sequence numbers

### 9.3 Security Considerations

- All adapters validate input size against `max_payload_size`
- OpenJiuwen uses CRC32 for integrity verification
- API key authentication via `Authorization: Bearer <token>` header
- Rate limiting configurable per protocol endpoint
- Input sanitization through Cupolas security dome

---

## 10. Troubleshooting

### Common Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| `Protocol not detected` | Missing content-type headers | Set explicit `contentType` in requests |
| `Transformation failed` | Incompatible field types | Check field compatibility matrix |
| `Connection timeout` | Gateway not running | Run `agentos service health` first |
| `CRC32 mismatch` | Corrupted OpenJiuwen payload | Re-send the message |
| `Unknown method` | Protocol handler not registered | Check `protocol list` output |

### Debug Mode

Enable verbose logging:

```bash
export AGENTOS_DEBUG=true
export AGENTOS_LOG_LEVEL=DEBUG
agentos protocol test jsonrpc --verbose
```

### Health Check Sequence

```bash
agentos status                    # Overall system status
agentos service health            # Gateway health
agentos protocol list             # Available protocols
agentos protocol test jsonrpc     # Per-protocol connectivity
agentos protocol test mcp
agentos protocol test openai
```

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-04-14 | Initial release — comprehensive protocol system documentation |

## Related Documents

- [Unified Interface Specification](../interfaces/include/agentos_protocol_interface.h)
- [Protocol Router Design](../protocols/core/router/include/protocol_router.h)
- [Transformer Implementation](../protocols/core/transformers/include/protocol_transformers.h)
- [Gateway Bridge API](../gateway/include/gateway_protocol_bridge.h)
- [OpenLab Protocol Bindings](../openlab/openlab/protocols/__init__.py)
- [AgentOS Architecture](../../docs/Capital_Architecture/)
- [Getting Started Guide](../../docs/Capital_Guides/getting_started.md)
