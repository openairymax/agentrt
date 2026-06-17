# AutoGen — 多代理框架适配器

**模块路径**: `agentos/protocols/frameworks/autogen/`
**版本**: v0.1.0

## 概述

AutoGen 框架适配器实现 AgentRT 与 Microsoft AutoGen 多代理对话框架的完整集成。将 AutoGen 的 ConversableAgent、GroupChat、UserProxyAgent、CodeExecutor 等核心概念映射到 AgentRT 的统一协议体系。

支持的 AutoGen 特性：

1. **多代理对话编排** — 多个 Agent 之间的有序/自由对话
2. **角色定义** — UserProxy / Assistant / Coder / Planner / Researcher / Reviewer
3. **群聊管理** — Round-robin / Speaker selection / Sequential / Parallel 模式
4. **代码执行沙箱** — 安全的代码执行环境
5. **人机协作** — Human-in-the-loop 三种模式（Never / Terminate / Always / CodeExecution）
6. **对话历史持久化** — 完整的对话记录与摘要
7. **工具调用与函数执行** — 自定义工具注册与执行
8. **流式对话输出** — 实时消息流

## 目录结构

```
autogen/
├── README.md
├── include/
│   └── autogen_adapter.h           # AutoGen 适配器头文件
└── src/
    └── autogen_adapter.c           # AutoGen 适配器实现
```

## 核心组件

### 数据类型

| 类型 | 说明 |
|------|------|
| `autogen_agent_role_t` | Agent 角色枚举（USER_PROXY / ASSISTANT / CODER / PLANNER / RESEARCHER / REVIEWER / CUSTOM） |
| `autogen_chat_mode_t` | 群聊模式枚举（ROUND_ROBIN / SPEAKER_SELECTION / RAG / SEQUENTIAL / PARALLEL / CUSTOM） |
| `autogen_message_type_t` | 消息类型枚举（TEXT / TOOL_CALL / TOOL_RESULT / CODE_BLOCK / TERMINATION / HANDOFF / SYSTEM） |
| `autogen_human_mode_t` | 人机协作模式（NEVER / TERMINATE / ALWAYS / CODE_EXECUTION） |
| `autogen_agent_def_t` | Agent 定义（ID、名称、角色、系统消息、LLM 配置、终止条件、代码执行开关、人机协作模式） |
| `autogen_group_chat_def_t` | 群聊定义（ID、名称、模式、参与者、Speaker 选择提示词、最大轮次） |
| `autogen_message_t` | 消息结构（发送者、接收者、类型、内容、工具调用、元数据） |
| `autogen_conversation_t` | 对话结构（消息列表、摘要、创建时间、终止原因） |
| `autogen_group_chat_result_t` | 群聊执行结果（发起者、对话、耗时、轮次、最终摘要） |
| `autogen_config_t` | 适配器配置（API URL/Key、超时、流式开关、代码执行开关、人机协作开关、默认模型） |

### 常量限制

| 常量 | 值 | 说明 |
|------|-----|------|
| `AUTOGEN_MAX_AGENTS` | 32 | 最大 Agent 数量 |
| `AUTOGEN_MAX_GROUP_CHATS` | 16 | 最大群聊数量 |
| `AUTOGEN_MAX_MESSAGES` | 512 | 最大消息数量 |
| `AUTOGEN_MAX_TOOLS` | 64 | 最大工具数量 |
| `AUTOGEN_DEFAULT_TIMEOUT_MS` | 120000 | 默认超时 120s |

### 核心 API

| 函数 | 说明 |
|------|------|
| `autogen_config_default()` | 获取默认配置 |
| `autogen_adapter_create()` / `autogen_adapter_destroy()` | 创建/销毁适配器上下文 |
| `autogen_create_agent()` / `autogen_destroy_agent()` | 创建/销毁 Agent |
| `autogen_list_agents()` | 列出所有 Agent 实例 |
| `autogen_create_group_chat()` | 创建群聊 |
| `autogen_initiate_chat()` | 在群聊中发起对话 |
| `autogen_send_message()` | Agent 间发送消息 |
| `autogen_register_tool()` | 注册自定义工具 |
| `autogen_get_conversation()` | 获取对话记录 |
| `autogen_set_code_executor()` | 设置代码执行器 |
| `autogen_set_human_callback()` | 设置人机协作回调 |
| `autogen_set_message_hook()` | 设置消息钩子 |
| `autogen_set_llm_callback()` | 设置 LLM 调用回调 |
| `autogen_get_statistics()` | 获取使用统计 |
| `autogen_get_protocol_adapter()` | 获取协议适配器实例 |

## 依赖关系

| 依赖 | 来源 | 用途 |
|------|------|------|
| `agentos_protocol_interface.h` | `protocols/include/` | 适配器虚表与接口定义 |
| `unified_protocol.h` | `protocols/include/` | 统一消息模型 |

## 使用说明

```c
#include "autogen_adapter.h"

// 创建适配器
autogen_config_t cfg = autogen_config_default();
autogen_adapter_context_t *ctx = autogen_adapter_create(&cfg);

// 创建 Agent
autogen_agent_def_t assistant = {
    .name = "assistant",
    .role = AGENT_ROLE_ASSISTANT,
    .system_message = "You are a helpful assistant.",
    .max_consecutive_auto_reply = 10,
};
char agent_id[64];
autogen_create_agent(ctx, &assistant, agent_id);

// 创建群聊
autogen_group_chat_def_t chat = {
    .name = "team-chat",
    .mode = GROUP_CHAT_ROUND_ROBIN,
    .max_rounds = 5,
};
char group_id[64];
autogen_create_group_chat(ctx, &chat, group_id);

// 发起对话
autogen_group_chat_result_t result;
autogen_initiate_chat(ctx, group_id, agent_id, "Hello!", &result);

// 注册工具
autogen_register_tool(ctx, "search", "Search the web",
    "{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\"}}}",
    my_tool_executor, NULL);

// 设置 LLM 回调
autogen_set_llm_callback(ctx, my_llm_callback, NULL);

// 清理
autogen_group_chat_result_destroy(&result);
autogen_adapter_destroy(ctx);
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
