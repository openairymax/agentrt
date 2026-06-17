# MCP Tool Server - 自定义 MCP 工具服务器

> 难度：中级 | 主题：MCP 协议与自定义工具服务器

## 项目说明

本项目演示如何基于 MCP（Model Context Protocol）协议开发自定义工具服务器。包含一个 MCP 工具服务器实现和两个示例工具（计算器和文件读取器），展示 MCP 协议的核心概念和开发流程。

### 什么是 MCP 协议？

MCP（Model Context Protocol）是一种开放协议，标准化了 AI 模型与外部工具和数据源的交互方式。它定义了：

- **工具发现**：Agent 如何发现可用的工具
- **工具调用**：Agent 如何调用工具并获取结果
- **资源访问**：Agent 如何访问外部数据源
- **提示模板**：Agent 如何使用预定义的提示模板

### MCP 核心概念

| 概念 | 说明 |
|------|------|
| **Server** | MCP 服务器，提供工具和资源 |
| **Client** | MCP 客户端（即 Agent），调用工具和访问资源 |
| **Tool** | 可被调用的工具，有名称、描述和参数 Schema |
| **Resource** | 可被访问的数据源，如文件、数据库 |
| **Prompt** | 预定义的提示模板，可接受参数 |

## 目录结构

```
mcp-tool-server/
├── README.md                    # 本文件
├── config.yaml                  # AgentRT 配置，连接 MCP 服务器
├── server/
│   └── tool_server.py           # MCP 工具服务器实现
└── tools/
    ├── calculator.py            # 计算器工具
    └── file_reader.py           # 文件读取工具
```

## 运行方式

```bash
# 1. 确保 AgentRT 和 MCP SDK 已安装
pip install agentrt mcp

# 2. 启动 MCP 工具服务器
python server/tool_server.py

# 3. 在另一个终端，通过 AgentRT 连接 MCP 服务器
agentrt run --config config.yaml

# 4. 或使用 MCP CLI 测试工具
mcp inspect python server/tool_server.py
mcp call python server/tool_server.py calculator --args '{"expression": "2 + 3 * 4"}'
```

## 关键概念

### 1. MCP 服务器实现

MCP 服务器需要实现以下核心方法：

```python
from mcp.server import MCPServer

class MyServer(MCPServer):
    async def list_tools(self) -> list[Tool]:
        """返回服务器提供的所有工具列表。"""
        ...

    async def call_tool(self, name: str, arguments: dict) -> str:
        """调用指定工具并返回结果。"""
        ...
```

### 2. 工具定义格式

每个工具需要声明名称、描述和参数 Schema：

```python
Tool(
    name="calculator",
    description="数学表达式计算器",
    parameters={
        "type": "object",
        "properties": {
            "expression": {
                "type": "string",
                "description": "数学表达式，如 '2 + 3 * 4'"
            }
        },
        "required": ["expression"]
    }
)
```

### 3. 传输方式

MCP 支持多种传输方式：

| 传输方式 | 说明 | 适用场景 |
|---------|------|---------|
| **stdio** | 标准输入输出 | 本地开发、CLI 集成 |
| **SSE** | Server-Sent Events | Web 应用、远程服务 |
| **WebSocket** | 双向持久连接 | 实时交互、流式传输 |

### 4. AgentRT 集成

在 `config.yaml` 中配置 MCP 服务器连接：

```yaml
mcp_servers:
  - name: my-tools
    transport: stdio
    command: python server/tool_server.py
```

AgentRT 会自动发现 MCP 服务器提供的工具，并将其注册为 Agent 可用的工具。

## 扩展建议

- **数据库工具**：添加 SQL 查询工具，支持安全的数据访问
- **API 网关工具**：封装常用 API 为 MCP 工具，统一调用方式
- **工具组合**：在 MCP 服务器中实现工具链，一个工具调用另一个工具
- **权限控制**：为工具添加权限检查，限制敏感操作
- **缓存层**：对频繁调用的工具结果添加缓存，提升响应速度
- **监控指标**：记录工具调用次数、耗时等指标，便于性能优化
