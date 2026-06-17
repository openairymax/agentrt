# hello-agent — 5分钟 QuickStart

> 难度：入门级 | 预计时间：5分钟 | 主题：Agent 基础概念

## 项目说明

这是 AgentRT 最简单的示例项目。一个名为 `hello-agent` 的 Agent 会用友好的方式问候用户，并回答基本问题。

通过本项目你将了解：
- AgentRT 的最小可运行配置是什么
- `agent.yaml` 如何定义一个 Agent
- `config.yaml` 如何配置运行时参数
- 如何用 `agentrt init` 和 `agentrt run` 启动 Agent

## 目录结构

```
hello-agent/
├── README.md                 # 本文件
├── config.yaml               # 运行时配置（网关、模型、记忆等）
└── agents/
    └── main.agent.yaml       # Agent 定义（名称、模型、系统提示词）
```

## 运行方式

```bash
# 1. 初始化项目（可选，已提供完整配置）
agentrt init --path ./hello-agent

# 2. 启动 Agent
agentrt run --config config.yaml

# 3. 在另一个终端与 Agent 对话
# Agent 会在网关端口（默认 8080）提供 HTTP/WebSocket 接口
curl -X POST http://localhost:8080/api/v1/chat \
  -H "Content-Type: application/json" \
  -d '{"message": "你好，请介绍一下你自己"}'
```

## 关键概念

### 1. agent.yaml — Agent 定义文件

`agent.yaml` 是 AgentRT 中定义 Agent 的核心文件，声明了 Agent 的身份和行为：

| 字段 | 说明 |
|------|------|
| `name` | Agent 唯一标识名称 |
| `version` | Agent 版本号 |
| `model` | 默认使用的 LLM 模型 |
| `system_prompt` | 系统提示词，定义 Agent 的角色和行为准则 |

### 2. config.yaml — 运行时配置

`config.yaml` 控制 AgentRT 运行时的全局行为：

| 字段 | 说明 |
|------|------|
| `gateway_url` | Agent 网关地址 |
| `default_model` | 全局默认模型（可被 agent.yaml 覆盖） |
| `memory` | 记忆系统配置 |
| `security` | 安全策略配置 |

### 3. CoreLoopThree — Agent 执行循环

AgentRT 的 Agent 遵循 **CoreLoopThree** 执行循环：

```
认知(Cognition) → 规划(Planning) → 执行(Execution) → 反思(Reflection)
```

即使是简单的 `hello-agent`，也在内部经历这个循环来处理用户输入。

## 扩展建议

- **添加工具**：在 `agent.yaml` 的 `tools` 节注册自定义工具，让 Agent 能执行实际操作
- **切换模型**：修改 `model` 字段为 `gpt-4o-mini` 或 `deepseek-chat` 以降低成本
- **丰富提示词**：在 `system_prompt` 中加入更多角色描述，让 Agent 更专业
- **启用记忆**：将 `memory.enabled` 设为 `true`，Agent 将记住跨会话的对话历史
