# AgentRT Example Projects

> 10 个示例项目，覆盖从入门到高级的完整学习路径。

## 示例列表

| 示例 | 难度 | 展示能力 |
|------|:---:|------|
| [hello-agent](hello-agent/) | 入门 | 5 分钟 QuickStart：最少代码运行 Agent |
| [weather-agent](weather-agent/) | 初级 | 自定义工具 + MCP 协议集成 |
| [plugin-demo](plugin-demo/) | 初级 | Plugin SDK 四型插件开发教程 |
| [code-review-agent](code-review-agent/) | 中级 | 代码审查 Skill + 安全扫描 |
| [prompt-tuner-demo](prompt-tuner-demo/) | 中级 | Prompt 调优框架使用 |
| [mcp-tool-server](mcp-tool-server/) | 中级 | 自定义 MCP 工具服务器 |
| [research-agent](research-agent/) | 中级 | 多 Agent 协作 + 记忆持久化 |
| [customer-support-agent](customer-support-agent/) | 高级 | 完整 Pipeline + Hook 系统 |
| [multi-agent-debate](multi-agent-debate/) | 高级 | 4 种多 Agent 协作模式 |
| [a2a-chat](a2a-chat/) | 高级 | A2A 协议 Agent 间通信 |

## 学习路径

```
入门 → hello-agent → weather-agent → plugin-demo
       ↓
中级 → code-review-agent → prompt-tuner-demo → mcp-tool-server → research-agent
       ↓
高级 → customer-support-agent → multi-agent-debate → a2a-chat
```

## 运行示例

每个示例目录包含独立的 `README.md` 和 `config.yaml`，可直接运行：

```bash
cd hello-agent
agentrt run "你好，世界！"
```