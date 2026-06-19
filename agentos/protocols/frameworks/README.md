# Frameworks — 框架适配层

**模块路径**: `agentos/protocols/frameworks/`
**版本**: v0.1.0

## 概述

Frameworks 层提供主流 AI 框架与 AgentRT 的集成适配，将框架特有的概念和 API 映射到 AgentRT 的统一协议体系。当前支持 LangChain 和 AutoGen 两大框架，分别覆盖链式执行/工具调用/记忆管理和多代理对话/群聊编排等场景。

## 目录结构

```
frameworks/
├── README.md
├── langchain/                          # LangChain 框架适配
│   ├── include/
│   │   └── langchain_adapter.h
│   └── src/
│       └── langchain_adapter.c
└── autogen/                            # AutoGen 框架适配
    ├── include/
    │   └── autogen_adapter.h
    └── src/
        └── autogen_adapter.c
```

## 核心组件

| 适配器 | 版本 | 说明 |
|--------|------|------|
| **LangChain** | 1.0.0 | LangChain 框架适配器，支持 Chain/Agent/Tool/Memory/RAG/Streaming |
| **AutoGen** | 1.0.0 | AutoGen 多代理框架适配器，支持多代理对话/群聊/代码执行/人机协作 |

## 框架概念映射

### LangChain → AgentRT

| LangChain 概念 | AgentRT 映射 |
|----------------|-------------|
| Chain | AgentRT Task Pipeline |
| Agent | AgentRT Agent + Protocol Session |
| Tool | AgentRT MCP/OpenAI tool interface |
| LLM | AgentRT LLM Daemon via protocol |
| Memory | AgentRT MemoryRovol (L1-L4) |
| Retriever | AgentRT memory.search protocol |

### AutoGen → AgentRT

| AutoGen 概念 | AgentRT 映射 |
|-------------|-------------|
| ConversableAgent | AgentRT Agent + Protocol Session |
| GroupChat | AgentRT A2A multi-agent coordination |
| UserProxyAgent | AgentRT human-in-the-loop interface |
| CodeExecutor | AgentRT tool execution sandbox |
| AssistantAgent | LLM-backed agent via protocol |
| ChatCompletionClient | Protocol-based LLM client |

## 构建选项

| CMake 选项 | 默认值 | 说明 |
|------------|--------|------|
| `PROTOCOLS_ENABLE_LANGCHAIN` | ON | 启用 LangChain 框架适配器 |
| `PROTOCOLS_ENABLE_AUTOGEN` | ON | 启用 AutoGen 框架适配器 |

## 依赖关系

| 依赖 | 来源 | 用途 |
|------|------|------|
| `agentos_protocol_interface.h` | `protocols/include/` | 适配器虚表与接口定义 |
| `unified_protocol.h` | `protocols/include/` | 统一消息模型 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
