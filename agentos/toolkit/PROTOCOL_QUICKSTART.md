# AgentOS 协议快速入门指南

**版本**: 1.0.0
**最后更新**: 2026-04-14
**预计完成时间**: 10 分钟
**难度**: 入门级

通过 5 个步骤快速上手 AgentOS 多协议通信系统。

---

## 前置条件

- AgentOS 网关运行在 `http://localhost:18789`（或您配置的 URL）
- Python 3.8+（示例使用 Python；也支持 Go/Rust/TypeScript）
- 具备 JSON 和 HTTP 基础知识

---

## 步骤 1: 验证环境

检查网关是否运行以及协议是否可用：

```bash
agentos status
agentos protocol list
```

预期输出：
```
============================================================
  AgentOS 协议适配器
============================================================

协议          版本     状态      端点
-----------------------------------------------------
JSON-RPC      2.0      active    /jsonrpc
MCP           1.0      active    /mcp
A2A           0.3      active    /a2a
OpenAI        1.0      active    /v1
OpenJiuwen    1.0      active    /ojiuwen
```

测试各协议的连接：

```bash
agentos protocol test jsonrpc
agentos protocol test mcp
agentos protocol test openai
```

---

## 步骤 2: 您的第一个协议请求（Python）

### 安装 SDK

```bash
pip install agentos-toolkit
```

### 发送 JSON-RPC 请求

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
    print(f"响应: {result}")

asyncio.run(main())
```

运行：
```bash
python my_first_protocol.py
# 响应: {'jsonrpc': '2.0', 'result': {'status': 'ok', 'version': '2.0'}, 'id': '...'}
```

---

## 步骤 3: 自动检测协议

SDK 可以根据内容自动检测应使用哪种协议：

```python
from agentos.protocol import ProtocolClient, ProtocolType

client = ProtocolClient.from_env()

result = await client.detect_protocol(
    content='{"model":"gpt-4o","messages":[{"role":"user","content":"Hello"}]}',
)
print(f"检测到: {result.protocol_type}")  # ProtocolType.OPENAI

result = await client.detect_protocol(
    content='{"jsonrpc":"2.0","method":"tools/list","id":1}',
)
print(f"检测到: {result.protocol_type}")  # ProtocolType.MCP
```

或使用 CLI：
```bash
echo '{"model":"gpt-4","messages":[{}]}' | agentos protocol detect --content-type application/json
```

---

## 步骤 4: 使用不同协议

### MCP (Model Context Protocol)

```python
from agentos.protocol import create_mcp_client

mcp = create_mcp_client(base_url="http://localhost:18789")

tools = await mcp.list_tools()
print(f"可用工具: {[t['name'] for t in tools]}")

result = await mcp.call_tool("calculator_add", {"a": 10, "b": 20})
print(f"结果: {result}")
```

### OpenAI API 兼容

```python
from agentos.protocol import create_openai_client

openai = create_openai_client(
    base_url="http://localhost:18789",
    model="gpt-4o",
)

response = await openai.chat("什么是 AgentOS？")
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
print(f"发现的代理: {agents}")
```

---

## 步骤 5: 流式响应

所有协议都支持流式响应：

```python
async def streaming_example():
    client = ProtocolClient.from_env()

    print("流式响应:")
    async for chunk in client.stream_request(
        method="llm.chat",
        params={"prompt": "用三句话介绍 MCIS 理论"},
    ):
        print(chunk, end="", flush=True)
    print()  # 流结束后换行

asyncio.run(streaming_example())
```

---

## CLI 快速参考

| 命令 | 说明 |
|---------|-------------|
| `agentos protocol list` | 列出所有可用协议 |
| `agentos protocol info jsonrpc` | 显示协议的详细信息 |
| `agentos protocol test mcp` | 测试协议连接 |
| `agentos protocol detect -c '{"..."}'` | 从内容自动检测协议 |
| `agentos protocol send jsonrpc task.submit '{"content":"Hi"}'` | 发送协议消息 |
| `agentos protocol stats` | 查看协议统计信息 |
| `agentos protocol transform jsonrpc mcp` | 显示转换详情 |
| `agentos protocol capabilities openai` | 显示协议支持的能力 |

### 使用示例

```bash
# 查看所有协议
$ agentos protocol list

# 获取 OpenAI 协议的详细信息
$ agentos protocol info openai

# 测试 MCP 连接（含详细输出）
$ agentos protocol test mcp -v

# 从字符串检测协议
$ agentos protocol detect -c '{"tools/call":{"name":"search"}}'

# 发送 JSON-RPC 请求
$ agentos protocol send jsonrpc ping '{}'

# 查看统计信息
$ agentos protocol stats

# 了解 JSON-RPC → MCP 转换
$ agentos protocol transform jsonrpc mcp
```

---

## 多语言快速开始

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
    fmt.Printf("结果: %v\n", result)
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
    println!("结果: {:?}", result);
    Ok(())
}
```

### TypeScript

```typescript
import { createProtocolClient } from '@agentos/toolkit';

const client = createProtocolClient({ baseURL: 'http://localhost:18789' });

const result = await client.sendRequest({ method: 'ping' });
console.log('结果:', result);
```

---

## 协议选择指南

| 使用场景 | 推荐协议 | 原因 |
|----------|---------|------|
| AgentOS 通用任务 | **JSON-RPC** | 原生支持，功能完整 |
| 工具调用 / LLM 上下文 | **MCP** | 标准化的工具发现与调用 |
| 多代理协调 | **A2A** | 内置发现、委托、共识机制 |
| LLM 聊天补全 | **OpenAI** | 与 OpenAI 生态即插即用兼容 |
| 高性能二进制 | **OpenJiuwen** | 最低延迟，CRC32 完整性保证 |

---

## 下一步

1. **阅读完整指南**: [PROTOCOL_GUIDE.md](./PROTOCOL_GUIDE.md) — 全面的协议系统文档
2. **尝试 OpenLab 集成**: 在您的代理中使用 `openlab.protocols.ProtocolSessionManager`
3. **构建自定义适配器**: 为新协议实现 `proto_adapter_vtable_t`
4. **探索协议转换**: 使用 `agentos protocol transform <src> <tgt>` 了解消息映射

---

## 故障排除

**网关无响应？**
```bash
agentos status
agentos service health
```

**协议检测错误？**
- 添加显式的 `contentType` 提示
- 使用 `protocol detect -t application/mcp` 强制指定类型

**连接超时？**
- 检查 `AGENTOS_GATEWAY_URL` 环境变量
- 确认网关正在运行：`agentos status`

---

*详细 API 参考请参阅 [PROTOCOL_GUIDE.md](./PROTOCOL_GUIDE.md)*
