# AgentOS Protocol Quick Start Guide

**Version**: 1.0.0
**Last Updated**: 2026-04-14
**Time to Complete**: 10 minutes
**Difficulty**: Beginner-friendly

Get up and running with AgentOS's multi-protocol communication system in 5 steps.

---

## Prerequisites

- AgentOS Gateway running at `http://localhost:18789` (or your configured URL)
- Python 3.8+ (for examples; Go/Rust/TypeScript also supported)
- Basic understanding of JSON and HTTP

---

## Step 1: Verify Your Setup

Check that the gateway is running and protocols are available:

```bash
agentos status
agentos protocol list
```

Expected output:
```
============================================================
  AgentOS Protocol Adapters
============================================================

Protocol         Version    Status      Endpoint
-----------------------------------------------------
JSON-RPC         2.0        active      /jsonrpc
MCP              1.0        active      /mcp
A2A              0.3        active      /a2a
OpenAI           1.0        active      /v1
OpenJiuwen       1.0        active      /ojiuwen
```

Test connectivity for each protocol:

```bash
agentos protocol test jsonrpc
agentos protocol test mcp
agentos protocol test openai
```

---

## Step 2: Your First Protocol Request (Python)

### Install SDK

```bash
pip install agentos-toolkit
```

### Send a JSON-RPC Request

```python
import asyncio
from agentos.protocol import ProtocolClient, ProtocolConfig

async def main():
    config = ProtocolConfig(
        base_url="http://localhost:18789",
        default_protocol="jsonrpc",
    )

    client = ProtocolClient(config)

    result = await client.send_request(
        method="ping",
        params={},
    )
    print(f"Response: {result}")

asyncio.run(main())
```

Run it:
```bash
python my_first_protocol.py
# Response: {'jsonrpc': '2.0', 'result': {'status': 'ok', 'version': '2.0'}, 'id': '...'}
```

---

## Step 3: Auto-Detect Protocols

The SDK can automatically detect which protocol to use based on content:

```python
from agentos.protocol import ProtocolClient, ProtocolType

client = ProtocolClient.from_env()

result = await client.detect_protocol(
    content='{"model":"gpt-4o","messages":[{"role":"user","content":"Hello"}]}',
)
print(f"Detected: {result.protocol_type}")  # ProtocolType.OPENAI

result = await client.detect_protocol(
    content='{"jsonrpc":"2.0","method":"tools/list","id":1}',
)
print(f"Detected: {result.protocol_type}")  # ProtocolType.MCP
```

Or use the CLI:
```bash
echo '{"model":"gpt-4","messages":[{}]}' | agentos protocol detect --content-type application/json
```

---

## Step 4: Use Different Protocols

### MCP (Model Context Protocol)

```python
from agentos.protocol import create_mcp_client

mcp = create_mcp_client(base_url="http://localhost:18789")

tools = await mcp.list_tools()
print(f"Available tools: {[t['name'] for t in tools]}")

result = await mcp.call_tool("calculator_add", {"a": 10, "b": 20})
print(f"Result: {result}")
```

### OpenAI API Compatible

```python
from agentos.protocol import create_openai_client

openai = create_openai_client(
    base_url="http://localhost:18789",
    model="gpt-4o",
)

response = await openai.chat("What is AgentOS?")
print(response)
```

### A2A (Agent-to-Agent)

```python
from agentos.protocol import ProtocolClient, ProtocolType

client = ProtocolClient.from_env()

agents = await client.send_request(
    "agent.discover",
    {"capability": "data_analysis"},
    protocol=ProtocolType.A2A,
)
print(f"Found agents: {agents}")
```

---

## Step 5: Streaming Responses

All protocols support streaming:

```python
async def streaming_example():
    client = ProtocolClient.from_env()

    print("Streaming response:")
    async for chunk in client.stream_request(
        method="llm.chat",
        params={"prompt": "Tell me about MCIS theory in 3 sentences"},
    ):
        print(chunk, end="", flush=True)
    print()  # newline after stream

asyncio.run(streaming_example())
```

---

## CLI Quick Reference

| Command | Description |
|---------|-------------|
| `agentos protocol list` | List all available protocols |
| `agentos protocol info jsonrpc` | Show detailed info about a protocol |
| `agentos protocol test mcp` | Test connection to a protocol |
| `agentos protocol detect -c '{"..."}'` | Auto-detect protocol from content |
| `agentos protocol send jsonrpc task.submit '{"content":"Hi"}'` | Send a message |
| `agentos protocol stats` | View protocol statistics |
| `agentos protocol transform jsonrpc mcp` | Show transformation details |
| `agentos protocol capabilities openai` | Show what a protocol supports |

### Examples

```bash
# Check all protocols
$ agentos protocol list

# Get detailed info about OpenAI protocol
$ agentos protocol info openai

# Test MCP connectivity with verbose output
$ agentos protocol test mcp -v

# Detect protocol from a string
$ agentos protocol detect -c '{"tools/call":{"name":"search"}}'

# Send a JSON-RPC request
$ agentos protocol send jsonrpc ping '{}'

# See statistics
$ agentos protocol stats

# Understand JSON-RPC → MCP transformation
$ agentos protocol transform jsonrpc mcp
```

---

## Multi-Language Quick Start

### Go

```go
package main

import (
    "context"
    "fmt"
    agentos "github.com/spharxworks/agentos-toolkit-go"
    "github.com/spharxworks/agentos-toolkit-go/protocol"
)

func main() {
    ctx := context.Background()
    cfg := protocol.NewConfig(protocol.WithBaseURL("http://localhost:18789"))
    client := protocol.NewProtocolClient(cfg)

    result, err := client.SendRequest(ctx, &protocol.RequestOptions{
        Method: "ping",
    })
    if err != nil {
        panic(err)
    }
    fmt.Printf("Result: %v\n", result)
}
```

### Rust

```rust
use agentos_toolkit::protocol::{ProtocolClient, ProtocolConfig};

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let config = ProtocolConfig::builder()
        .base_url("http://localhost:18789")
        .build()?;
    let client = ProtocolClient::new(config);

    let result = client.send_request("ping", None, None).await?;
    println!("Result: {:?}", result);
    Ok(())
}
```

### TypeScript

```typescript
import { createProtocolClient } from '@agentos/toolkit';

const client = createProtocolClient({ baseURL: 'http://localhost:18789' });

const result = await client.sendRequest({ method: 'ping' });
console.log('Result:', result);
```

---

## Protocol Selection Guide

| Use Case | Recommended Protocol | Why |
|----------|---------------------|-----|
| General AgentOS tasks | **JSON-RPC** | Native, full feature support |
| Tool calling / LLM context | **MCP** | Standardized tool discovery & invocation |
| Multi-agent coordination | **A2A** | Built-in discovery, delegation, consensus |
| LLM chat completions | **OpenAI** | Drop-in compatible with OpenAI ecosystem |
| High-performance binary | **OpenJiuwen** | Lowest latency, CRC32 integrity |

---

## Next Steps

1. **Read the Full Guide**: [PROTOCOL_GUIDE.md](./PROTOCOL_GUIDE.md) — comprehensive documentation
2. **Try OpenLab Integration**: Use `openlab.protocols.ProtocolSessionManager` in your agents
3. **Build Custom Adapters**: Implement `proto_adapter_vtable_t` for new protocols
4. **Explore Transformations**: Use `agentos protocol transform <src> <tgt>` to understand message mapping

---

## Troubleshooting

**Gateway not responding?**
```bash
agentos status
agentos service health
```

**Protocol detection wrong?**
- Add explicit `contentType` hint
- Use `protocol detect -t application/mcp` to force type

**Connection timeout?**
- Check `AGENTOS_GATEWAY_URL` env var
- Verify gateway is running: `agentos status`

---

*For detailed API reference, see [PROTOCOL_GUIDE.md](./PROTOCOL_GUIDE.md)*
