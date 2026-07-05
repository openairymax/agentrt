# Protocols — 统一通信协议栈

**模块路径**: `agentrt/protocols/`
**版本**: v0.1.0

## 概述

Protocols 层是 AgentRT 的统一通信协议栈，采用五层架构设计（Common / Core / Standards / Integrations / Frameworks），为系统内部模块间、服务间以及系统与外部平台之间的所有通信提供标准化协议和契约。协议层使用 C 语言实现，通过 CMake 构建系统管理编译选项，编译为 `libagentrt_protocols` 共享库。

核心设计理念：

- **协议无关 API** — 上层业务通过 `unified_message_t` 统一消息模型与 `protocol_adapter_t` 适配器接口通信，无需关心底层协议细节
- **可插拔适配器** — 每种协议实现为独立适配器，支持动态注册/注销/热加载
- **智能路由** — 基于规则引擎的协议路由器，自动完成跨协议消息转换
- **统一注册中心** — 协议发现、能力查询、依赖追踪、生命周期管理

## 目录结构

```
protocols/
├── CMakeLists.txt                          # CMake 构建配置
├── README.md
├── include/                                # 顶层公共头文件
│   ├── agentrt_protocol_interface.h        # 协议系统统一接口定义
│   ├── unified_protocol.h                  # 统一消息模型与协议类型
│   └── protocol_router.h                   # 顶层协议路由器（轻量级）
├── src/                                    # 顶层实现
│   ├── agentrt_protocol_interface.c        # Router/Gateway/Registry 统一接口实现
│   └── protocol_toplevel_impl.c            # 顶层协议路由实现
├── common/                                 # 公共层 — 统一协议接口与公共实现
│   ├── include/
│   │   └── protocols.h                     # 框架主头文件（初始化/管理器/适配器工厂）
│   └── src/
│       ├── unified_protocol.c              # 协议栈核心实现（消息创建/发送/接收/回调）
│       └── protocols_impl.c                # 框架初始化/管理器/默认适配器/错误处理
├── core/                                   # 核心层 — 路由/扩展/转换/注册
│   ├── adapter/                            # 扩展框架
│   ├── registry/                           # 注册中心
│   ├── router/                             # 协议路由引擎
│   └── transformers/                       # 消息转换器
├── standards/                              # 标准协议层 — 行业标准协议适配
│   ├── a2a/                                # A2A v0.3 (Agent-to-Agent)
│   ├── mcp/                                # MCP v1.0 (Model Context Protocol)
│   └── agntcy/                             # AGNTCY ACP
├── integrations/                           # 集成适配层 — 主流 AI 平台集成
│   ├── openai/                             # OpenAI API 企业级适配
│   ├── claude/                             # Anthropic Claude API 适配
│   ├── openjiuwen/                         # OpenJiuwen 二进制协议适配
│   ├── openclaw/                           # OpenClaw (九问) 平台适配
│   └── china_eco/                          # 国内生态（百炼/文心/国密/对象存储）
├── frameworks/                             # 框架适配层 — AI 框架集成
│   ├── langchain/                          # LangChain 框架适配
│   └── autogen/                            # AutoGen 多代理框架适配
└── tests/                                  # 测试套件
    ├── test_openclaw_adapter.c
    ├── test_agntcy_acp.c
    └── test_china_eco_crypto.c
```

## 协议层次

```
┌─────────────────────────────────────────────────────────────────────┐
│                     Frameworks Layer (框架适配层)                     │
│  ┌──────────────────┐  ┌──────────────────┐                        │
│  │ langchain_adapter │  │  autogen_adapter  │                        │
│  └──────────────────┘  └──────────────────┘                        │
├─────────────────────────────────────────────────────────────────────┤
│                  Integrations Layer (集成适配层)                      │
│  ┌───────────────┐ ┌──────────────┐ ┌─────────────┐ ┌───────────┐ │
│  │openai_enterprise│ │openjiuwen    │ │ openclaw    │ │  claude   │ │
│  │   _adapter     │ │  _adapter    │ │  _adapter   │ │ _adapter  │ │
│  └───────────────┘ └──────────────┘ └─────────────┘ └───────────┘ │
│  ┌───────────────┐                                                  │
│  │ china_eco     │                                                  │
│  │  _adapter     │                                                  │
│  └───────────────┘                                                  │
├─────────────────────────────────────────────────────────────────────┤
│                   Standards Layer (标准协议层)                        │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐               │
│  │a2a_v03       │ │ mcp_v1       │ │ agntcy_acp   │               │
│  │  _adapter    │ │  _adapter    │ │  _adapter    │               │
│  └──────────────┘ └──────────────┘ └──────────────┘               │
├─────────────────────────────────────────────────────────────────────┤
│                     Core Layer (核心层)                              │
│  ┌──────────────┐ ┌──────────────────────┐ ┌───────────────────┐  │
│  │protocol_router│ │protocol_extension    │ │protocol_          │  │
│  │              │ │   _framework         │ │  transformers     │  │
│  └──────────────┘ └──────────────────────┘ └───────────────────┘  │
│  ┌──────────────┐                                                   │
│  │protocol_     │                                                   │
│  │  registry    │                                                   │
│  └──────────────┘                                                   │
├─────────────────────────────────────────────────────────────────────┤
│                    Common Layer (公共层)                              │
│  ┌──────────────────────┐  ┌──────────────────┐                    │
│  │ unified_protocol.c   │  │ protocols_impl.c │                    │
│  └──────────────────────┘  └──────────────────┘                    │
└─────────────────────────────────────────────────────────────────────┘
```

## 各层说明

| 层次 | 组件 | 职责 |
|------|------|------|
| **Common Layer** | `unified_protocol.c`, `protocols_impl.c` | 统一消息模型 (`unified_message_t`)、协议栈生命周期 (`protocol_stack_*`)、适配器注册与路由、框架初始化/管理器/默认适配器工厂 |
| **Core Layer** | `protocol_router.c`, `protocol_extension_framework.c`, `protocol_transformers.c`, `protocol_registry.c` | 协议路由引擎（规则匹配/消息转换）、扩展框架（插件式适配器管理/中间件管道）、消息转换器（跨协议格式适配）、注册中心（协议发现/能力查询/依赖追踪） |
| **Standards Layer** | `a2a_v03_adapter.c`, `mcp_v1_adapter.c`, `mcp_transport.c`, `agntcy_acp_adapter.c` | A2A (Agent-to-Agent)、MCP (Model Context Protocol)、AGNTCY ACP 等行业标准协议适配 |
| **Integrations Layer** | `openai_enterprise_adapter.c`, `claude_adapter.c`, `openjiuwen_adapter.c`, `openclaw_adapter.c`, `china_eco_adapter.c` | OpenAI API (Chat/Embeddings/Function Calling/Streaming)、Claude (Messages/Tool Use/Extended Thinking/Vision)、OpenJiuwen (自定义二进制协议)、OpenClaw (九问/多智能体/安全管控)、国内生态 (百炼/文心/国密SM2-4/对象存储) |
| **Frameworks Layer** | `langchain_adapter.c`, `autogen_adapter.c` | LangChain (Chain/Agent/Tool/Memory/RAG/Streaming)、AutoGen (多代理对话/群聊/代码执行/人机协作) |

## 核心接口

### I-L1: 协议适配器接口 (`proto_adapter_vtable_t`)

所有协议适配器的统一虚表接口，定义 `init/destroy/encode/decode/connect/disconnect/send/receive/get_stats` 等方法，以及能力标志位 (`proto_capability_flags_t`)。

### I-L2: 协议路由接口 (`proto_router_iface_t`)

基于规则引擎的协议路由器，支持路由添加/删除、消息路由/批量路由、协议转换、默认协议设置、路由统计。

### I-L3: 协议网关接口 (`proto_gateway_iface_t`)

协议网关集成接口，提供协议注册/注销、请求处理、协议自动检测、事件回调、统计查询。

### I-L4: 协议扩展接口 (`proto_extension_mgr_iface_t`)

扩展管理器接口，支持扩展注册/注销、加载/卸载、自动检测、能力查询。

## 构建选项

协议层支持通过 CMake 选项控制各适配器的编译：

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `PROTOCOLS_ENABLE_OPENCLAW` | OFF | OpenClaw (九问) 平台适配器 |
| `PROTOCOLS_ENABLE_CLAUDE` | ON | Claude API 适配器 |
| `PROTOCOLS_ENABLE_LANGCHAIN` | ON | LangChain 框架适配器 |
| `PROTOCOLS_ENABLE_AUTOGEN` | ON | AutoGen 框架适配器 |
| `PROTOCOLS_ENABLE_AGNTCY` | ON | AGNTCY ACP 协议适配器 |
| `PROTOCOLS_ENABLE_CHINA_ECO` | ON | 国内生态适配器 |
| `PROTOCOLS_ENABLE_MCP` | ON（Windows 上为 OFF） | MCP 协议适配器 |

## 依赖关系

| 依赖 | 来源 | 用途 |
|------|------|------|
| `agentrt_common` | `commons/` | 平台抽象、内存管理、字符串工具 |
| `svc_common` | `daemons/common/` | 安全字符串工具 (`safe_string_utils.c`) |
| `agentrt_compile_defs` | 根 CMake | 编译定义 |
| `cJSON` | 外部 | JSON 解析（MCP 等适配器） |
| `libcurl` | 外部 | HTTP 客户端（部分集成适配器） |
| `corekern` | `atoms/corekern/` | 内核类型定义 |

## 使用说明

### 基本初始化

```c
#include "protocols.h"

// 初始化框架
protocols_framework_init();

// 创建协议栈管理器
protocol_manager_handle_t mgr = protocol_manager_create();

// 创建协议栈
protocol_stack_config_t cfg = protocol_stack_config_default("main-stack");
protocol_stack_handle_t stack = protocol_manager_create_stack(mgr, &cfg);

// 注册适配器
protocol_adapter_t http_adapter = *protocol_adapter_http();
protocol_stack_register_adapter(stack, http_adapter);

// 发送消息
unified_message_t msg = unified_message_create(
    PROTOCOL_HTTP, DIRECTION_REQUEST, "/api/v1/chat", payload, payload_size);
protocol_stack_send(stack, &msg);

// 清理
protocol_manager_destroy_stack(mgr, stack);
protocol_manager_destroy(mgr);
protocols_framework_cleanup();
```

### 协议路由

```c
#include "agentrt_protocol_interface.h"

// 创建标准路由器
proto_router_iface_t *router = proto_router_standard_create();

// 添加路由规则
protocol_rule_t rule = {
    .source_protocol = PROTO_JSONRPC,
    .target_protocol = PROTO_MCP,
    .source_endpoint = "/mcp/*",
    .target_endpoint = "/mcp/v1",
    .priority = 10
};
router->add_route(router, "/mcp/*", PROTO_JSONRPC, "/mcp/v1", PROTO_MCP, 10);

// 路由消息
route_decision_t decision;
router->route(router, &message, &decision);

// 销毁
proto_router_standard_destroy(router);
```

### 协议网关

```c
#include "agentrt_protocol_interface.h"

// 创建网关
proto_gateway_iface_t *gw = proto_gateway_standard_create();

// 注册协议
gw->register_protocol(gw, "jsonrpc", &jsonrpc_vtable);
gw->register_protocol(gw, "mcp", &mcp_vtable);

// 处理请求（自动检测协议）
char *response = NULL;
size_t resp_size = 0;
char *content_type = NULL;
gw->handle_request(gw, raw_data, data_size, "application/json",
                   &response, &resp_size, &content_type);

// 销毁
proto_gateway_standard_destroy(gw);
```

## 与相关模块的关系

- **Gateway**: 对外协议的转换和路由
- **CoreKern**: Binder IPC 是 CoreKern 的核心通信机制
- **Daemon 服务**: 各守护进程通过 JSON-RPC 2.0 进行服务间通信
- **Toolkit**: SDK 实现中包含协议客户端库
- **OpenLab**: 所有 OpenLab 模块通过 JSON-RPC 2.0 与核心运行时通信

---

© 2026 SPHARX Ltd. All Rights Reserved.
