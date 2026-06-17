# Integrations — 集成适配层

**模块路径**: `agentos/protocols/integrations/`
**版本**: v0.1.0

## 概述

Integrations 层提供 AgentRT 与主流 AI 平台和生态的集成适配，将各平台特有的 API 和协议映射到 AgentRT 的统一协议体系。当前支持 OpenAI、Claude、OpenJiuwen、OpenClaw（九问）和国内生态五大集成方向。

## 目录结构

```
integrations/
├── README.md
├── openai/                             # OpenAI API 企业级适配
│   ├── include/
│   │   └── openai_enterprise_adapter.h
│   └── src/
│       └── openai_enterprise_adapter.c
├── claude/                             # Anthropic Claude API 适配
│   ├── include/
│   │   └── claude_adapter.h
│   └── src/
│       └── claude_adapter.c
├── openjiuwen/                         # OpenJiuwen 二进制协议适配
│   ├── include/
│   │   └── openjiuwen_adapter.h
│   └── src/
│       └── openjiuwen_adapter.c
├── openclaw/                           # OpenClaw (九问) 平台适配
│   ├── include/
│   │   └── openclaw_adapter.h
│   └── src/
│       └── openclaw_adapter.c
└── china_eco/                          # 国内生态适配
    ├── include/
    │   └── china_eco_adapter.h
    └── src/
        └── china_eco_adapter.c
```

## 核心组件

### OpenAI Enterprise Adapter

**版本**: 1.0.0 | **CMake**: `PROTOCOLS_ENABLE_OPENAI`（默认编译）

OpenAI API 企业级特性适配器，提供完整的 Chat Completions、Embeddings、Function Calling、Streaming、Rate Limiting 等企业级能力。

| 特性 | 说明 |
|------|------|
| Chat Completions | 多轮对话，含 Function Calling / Tool Use |
| Embeddings API | 文本向量化 |
| Streaming SSE | 流式响应 |
| Rate Limiting | RPM/TPM 配额管理 |
| 多模型路由 | 模型注册与回退策略 |
| Token 预算 | 控制最大 Token 消耗 |
| 请求重试 | 指数退避重试（默认 3 次） |
| 审计日志 | 请求/响应/Token 使用记录 |

### Claude Adapter

**版本**: 1.0.0 | **CMake**: `PROTOCOLS_ENABLE_CLAUDE`（默认 ON）

Anthropic Claude API 适配器，实现 Messages API、Tool Use、Extended Thinking、Vision 等完整集成。

| 特性 | 说明 |
|------|------|
| Messages API | 多轮对话、系统提示词 |
| Tool Use | 原生工具调用与函数执行 |
| Extended Thinking | 深度推理模式（Disabled / Enabled / Extended） |
| Vision | 图像理解能力 |
| Streaming | SSE 流式响应 |
| Prompt Caching | 提示缓存优化（None / Ephemeral / Persistent） |
| 安全过滤 | 内容安全策略 |

### OpenJiuwen Adapter

**版本**: 1.0.0 | **CMake**: 默认编译

OpenJiuwen 平台协议适配器，支持自定义二进制协议的消息格式转换和互操作。

| 特性 | 说明 |
|------|------|
| 二进制协议 | Header(24B) + Payload + CRC32(4B) |
| 消息类型 | REQUEST / RESPONSE / NOTIFICATION / HEARTBEAT / ERROR |
| 连接管理 | 自动重连（指数退避，1s-60s） |
| 心跳机制 | 30s 间隔心跳 |
| 压缩/加密 | 可选载荷压缩与加密 |

### OpenClaw Adapter

**版本**: 1.0.0 | **CMake**: `PROTOCOLS_ENABLE_OPENCLAW`（默认 OFF，需 Unix sockets）

OpenClaw（九问）开源 AI Agent 平台适配器，专注于政务和企业应用场景。

| 特性 | 说明 |
|------|------|
| 离线私有化 | 完全本地运行，数据不出域 |
| 安全管控 | 五级安全等级（Public / Internal / Confidential / Secret / TopSecret） |
| 多模态 | 文本/图像/音频/视频/文件/代码统一处理 |
| 多智能体 | Agent 注册/发现/注销，最多 64 个 Agent |
| 工具共享 | 跨 Agent 工具注册与调用 |
| 任务委派 | 任务创建/查询/取消/进度追踪 |
| 会话管理 | 多会话并行，上下文管理 |
| 集群状态 | 节点/Agent/会话/任务统计 |

### China Eco Adapter

**版本**: 0.1.0 | **CMake**: `PROTOCOLS_ENABLE_CHINA_ECO`（默认 ON，Windows 上 OFF）

国内生态协议兼容适配器，提供 LLM Provider Bridge、对象存储 Bridge、国密算法和消息队列四大能力。

| 特性 | 说明 |
|------|------|
| LLM Bridge | 百炼/文心/DashScope/智谱/MiniMax/Moonshot/DeepSeek/Qwen 的 OpenAI 兼容层 |
| Object Storage | 阿里云 OSS / 腾讯云 COS / 百度云 BOS / 华为云 OBS 统一适配 |
| SM Crypto | SM2 非对称加密 / SM3 哈希 / SM4 对称加密 |
| Message Queue | RocketMQ / Pulsar 消息队列协议映射 |

## 构建选项

| CMake 选项 | 默认值 | 说明 |
|------------|--------|------|
| `PROTOCOLS_ENABLE_OPENCLAW` | OFF | OpenClaw 适配器（需 Unix sockets） |
| `PROTOCOLS_ENABLE_CLAUDE` | ON | Claude API 适配器 |
| `PROTOCOLS_ENABLE_CHINA_ECO` | ON（Windows OFF） | 国内生态适配器 |

## 依赖关系

| 依赖 | 来源 | 用途 |
|------|------|------|
| `unified_protocol.h` | `protocols/include/` | 统一消息模型 |
| `agentos_protocol_interface.h` | `protocols/include/` | 适配器虚表与接口定义 |
| `protocol_extension_framework.h` | `core/adapter/include/` | 扩展框架（OpenJiuwen 使用） |
| `cJSON` | 外部 | JSON 解析 |
| `libcurl` | 外部 | HTTP 客户端 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
