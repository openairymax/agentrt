# A2A Chat - A2A 协议 Agent 间通信

> 难度：高级 | 主题：A2A 协议与跨 Agent 通信

## 项目说明

本项目演示 AgentRT 中基于 A2A（Agent-to-Agent）协议的 Agent 间通信。两个 Agent 通过 A2A 协议进行对话，展示如何让不同 Agent 之间发现、连接和交互。

### 什么是 A2A 协议？

A2A（Agent-to-Agent）协议是一种开放标准，定义了 Agent 之间如何互相发现、通信和协作。它解决了以下问题：

- **发现**：Agent 如何找到其他 Agent
- **认证**：Agent 如何验证对方身份
- **通信**：Agent 之间如何交换消息
- **协作**：Agent 如何委派任务和共享结果

### A2A 协议核心概念

| 概念 | 说明 |
|------|------|
| **Agent Card** | Agent 的身份卡片，声明能力、端点和认证方式 |
| **Message** | Agent 间传递的消息，包含内容和元数据 |
| **Task** | 一个 Agent 向另一个 Agent 委派的任务 |
| **Artifact** | 任务执行产生的结果产物 |

## 目录结构

```
a2a-chat/
├── README.md                        # 本文件
├── config.yaml                      # A2A 通信配置
└── agents/
    ├── chat_agent_a.agent.yaml      # Agent A 配置
    └── chat_agent_b.agent.yaml      # Agent B 配置
```

## 运行方式

```bash
# 1. 确保 AgentRT 已安装
pip install agentrt

# 2. 启动 Agent A 的服务端
agentrt serve --agent agents/chat_agent_a.agent.yaml --port 8001

# 3. 启动 Agent B 的服务端（另一个终端）
agentrt serve --agent agents/chat_agent_b.agent.yaml --port 8002

# 4. 发起 A2A 对话
agentrt run --config config.yaml

# 5. 或使用 CLI 直接测试
agentrt a2a chat --from agents/chat_agent_a.agent.yaml --to http://localhost:8002
```

## 关键概念

### 1. Agent Card — 身份与能力声明

每个 Agent 启动时会发布自己的 Agent Card，其他 Agent 据此发现和连接：

```yaml
# Agent Card 包含的信息
a2a:
  name: chat-agent-a
  description: "擅长技术讨论的 Agent"
  endpoint: http://localhost:8001
  capabilities:
    - chat
    - code_review
  authentication:
    type: bearer
```

### 2. 消息传递流程

```
Agent A                          Agent B
  │                                │
  │─── 发现 Agent Card ──────────→│
  │←── 返回 Agent Card ───────────│
  │                                │
  │─── 发送 Task ────────────────→│
  │←── 返回 Task Accepted ────────│
  │                                │
  │←── 推送 Artifact (结果) ──────│
  │─── 确认接收 ────────────────→│
```

### 3. 通信模式

| 模式 | 说明 | 适用场景 |
|------|------|---------|
| **同步请求** | 发送消息后等待对方响应 | 简单问答、即时查询 |
| **异步任务** | 委派任务后通过回调获取结果 | 耗时计算、批量处理 |
| **流式传输** | 持续接收对方的部分输出 | 实时翻译、协作编辑 |
| **发布订阅** | 订阅对方的事件通知 | 状态监控、事件驱动 |

### 4. 安全与认证

A2A 协议支持多种认证方式：

- **Bearer Token**：简单的令牌认证
- **mTLS**：双向 TLS 证书认证
- **OAuth2**：基于 OAuth2 的授权认证

## 扩展建议

- **多 Agent 网络**：扩展到3个以上 Agent，构建 Agent 通信网络
- **服务发现**：集成服务注册中心，实现 Agent 动态发现
- **消息加密**：对敏感通信内容进行端到端加密
- **协议网关**：实现 A2A 与其他协议（如 MCP）的互转网关
- **监控面板**：可视化展示 Agent 间的通信拓扑和消息流
