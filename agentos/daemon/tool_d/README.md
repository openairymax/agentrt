# Tool Daemon — 工具执行守护进程

`daemon/tool_d/` 是 AgentOS 的工具执行守护进程，负责外部工具的注册、发现、安全执行和结果管理。

## 核心职责

- **工具注册**：动态注册和卸载外部工具
- **安全执行**：在沙箱环境中执行工具代码
- **结果缓存**：缓存相同参数的工具执行结果
- **参数校验**：校验工具调用参数的类型和格式
- **执行限流**：控制工具调用频率，防止滥用

## 架构

```
客户端请求 (JSON-RPC 2.0)
       ↓
  tool_svc_adapter  ← 请求解析与标准化
       ↓
  tool_service      ← 核心服务逻辑（路由、限流、缓存）
       ↓
  ┌──────┬──────┬──────┬──────┐
  │registry │executor│validator│ cache│
  └──────┴──────┴──────┴──────┘
       ↓
  工具运行时（Sandbox）
```

## 核心能力

| 能力 | 说明 |
|------|------|
| `tool.register` | 注册新工具 |
| `tool.execute` | 执行工具调用 |
| `tool.list` | 列出可用工具 |
| `tool.info` | 查询工具详细信息 |
| `tool.unregister` | 卸载工具 |
| `tool.cache.clear` | 清除工具缓存 |

## 使用方式

```bash
# 启动工具守护进程
./tool_d --config tool_config.json

# 指定工具目录
./tool_d --tool-dir /opt/agentos/tools

# 启用缓存
./tool_d --cache-size 1000
```

---

*AgentOS Daemon — Tool Daemon*
