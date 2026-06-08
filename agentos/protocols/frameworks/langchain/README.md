# LangChain — 框架适配器

**模块路径**: `agentos/protocols/frameworks/langchain/`
**版本**: v0.0.5

## 概述

LangChain 框架适配器实现 AgentOS 与 LangChain 生态的完整集成。将 LangChain 的 Chain、Agent、Tool、Memory、Retriever 等核心概念映射到 AgentOS 的统一协议体系。

支持的 LangChain 组件：

1. **LCEL (LangChain Expression Language)** — 链式执行
2. **AgentExecutor** — 多步推理代理
3. **Tool Calling** — 原生工具调用
4. **RAG** — 检索增强生成
5. **ConversationBufferMemory** — 对话记忆
6. **StreamingIterator** — 流式输出

## 目录结构

```
langchain/
├── README.md
├── include/
│   └── langchain_adapter.h         # LangChain 适配器头文件
└── src/
    └── langchain_adapter.c         # LangChain 适配器实现
```

## 核心组件

### 数据类型

| 类型 | 说明 |
|------|------|
| `langchain_component_type_t` | 组件类型枚举（LLM / CHAT_MODEL / EMBEDDING_MODEL / TOOL / CHAIN / AGENT / MEMORY / RETRIEVER / OUTPUT_PARSER） |
| `langchain_chain_type_t` | Chain 类型枚举（SEQUENTIAL / ROUTER / MAP_REDUCE / PARALLEL / CONDITIONAL / CUSTOM） |
| `langchain_agent_type_t` | Agent 类型枚举（REACT / PLAN_AND_EXECUTE / OPENAI_FUNCTIONS / STRUCTURED_CHAT / XML） |
| `langchain_memory_type_t` | Memory 类型枚举（BUFFER / SUMMARY / WINDOW / TOKEN / ENTITY / KG） |
| `langchain_chain_def_t` | Chain 定义（ID、名称、类型、步骤 ID 列表、编译标志） |
| `langchain_chain_instance_t` | Chain 实例（含编译后的可执行对象、输入/输出 Schema） |
| `langchain_agent_def_t` | Agent 定义（ID、名称、类型、LLM ID、工具列表、记忆 ID、最大迭代次数） |
| `langchain_tool_def_t` | Tool 定义（ID、名称、描述、函数 Schema、类型、异步标志） |
| `langchain_memory_t` | Memory 实例（ID、类型、最大条目数、消息列表、摘要） |
| `langchain_execution_result_t` | 执行结果（Chain ID、输入/输出 JSON、耗时、步骤数、中间结果） |
| `langchain_config_t` | 适配器配置（API URL/Key、超时、流式/追踪/缓存开关、默认模型） |
| `langchain_adapter_context_t` | 适配器上下文（配置、Chain/Tool/Agent/Memory 实例数组、回调函数、统计） |

### 常量限制

| 常量 | 值 | 说明 |
|------|-----|------|
| `LANGCHAIN_MAX_CHAINS` | 64 | 最大 Chain 数量 |
| `LANGCHAIN_MAX_TOOLS` | 128 | 最大 Tool 数量 |
| `LANGCHAIN_MAX_AGENTS` | 32 | 最大 Agent 数量 |
| `LANGCHAIN_MAX_MEMORY_ENTRIES` | 1024 | 最大 Memory 条目数 |
| `LANGCHAIN_DEFAULT_TIMEOUT_MS` | 60000 | 默认超时 60s |

### 核心 API

| 函数 | 说明 |
|------|------|
| `langchain_config_default()` | 获取默认配置 |
| `langchain_adapter_create()` / `langchain_adapter_destroy()` | 创建/销毁适配器上下文 |
| `langchain_register_tool()` | 注册自定义工具（含执行器回调） |
| `langchain_list_tools()` | 列出已注册工具 |
| `langchain_create_chain()` | 创建 Chain 实例 |
| `langchain_execute_chain()` | 执行 Chain（同步） |
| `langchain_execute_chain_streaming()` | 执行 Chain（流式） |
| `langchain_create_agent()` | 创建 Agent |
| `langchain_agent_run()` | 运行 Agent |
| `langchain_create_memory()` | 创建 Memory |
| `langchain_memory_add()` / `langchain_memory_get()` | 添加/获取 Memory 条目 |
| `langchain_set_streaming_handler()` | 设置流式输出回调 |
| `langchain_set_trace_handler()` | 设置追踪回调 |
| `langchain_set_llm_callback()` | 设置 LLM 调用回调 |
| `langchain_get_statistics()` | 获取使用统计 |
| `langchain_get_protocol_adapter()` | 获取协议适配器实例 |

## 依赖关系

| 依赖 | 来源 | 用途 |
|------|------|------|
| `agentos_protocol_interface.h` | `protocols/include/` | 适配器虚表与接口定义 |
| `unified_protocol.h` | `protocols/include/` | 统一消息模型 |

## 使用说明

```c
#include "langchain_adapter.h"

// 创建适配器
langchain_config_t cfg = langchain_config_default();
langchain_adapter_context_t *ctx = langchain_adapter_create(&cfg);

// 注册工具
langchain_tool_def_t tool = {
    .name = "calculator",
    .description = "Performs arithmetic calculations",
    .function_schema_json = "{\"type\":\"object\",...}",
    .tool_type = LC_TYPE_TOOL,
};
langchain_register_tool(ctx, &tool, my_tool_executor, NULL);

// 创建 Chain
langchain_chain_def_t chain_def = {
    .name = "qa-chain",
    .type = LC_CHAIN_SEQUENTIAL,
    .step_ids = (char*[]){"retriever", "llm"},
    .step_count = 2,
};
langchain_chain_instance_t chain;
langchain_create_chain(ctx, &chain_def, &chain);

// 执行 Chain
langchain_execution_result_t result;
langchain_execute_chain(ctx, chain.id, "{\"query\":\"What is AI?\"}", &result);

// 创建 Agent
langchain_agent_def_t agent_def = {
    .name = "research-agent",
    .type = LC_AGENT_REACT,
    .llm_id = "default-llm",
    .max_iterations = 10,
};
char agent_id[64];
langchain_create_agent(ctx, &agent_def, agent_id);

// 运行 Agent
langchain_execution_result_t agent_result;
langchain_agent_run(ctx, agent_id, "Research quantum computing", &agent_result);

// 创建 Memory
langchain_memory_t memory;
langchain_create_memory(ctx, LC_MEM_BUFFER, 100, &memory);
langchain_memory_add(ctx, memory.id, "user", "Hello!");
langchain_memory_add(ctx, memory.id, "assistant", "Hi! How can I help?");

// 流式执行
langchain_set_streaming_handler(ctx, my_stream_handler, NULL);

// 清理
langchain_execution_result_destroy(&result);
langchain_adapter_destroy(ctx);
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
