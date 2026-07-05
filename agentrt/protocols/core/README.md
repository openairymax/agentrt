# Core — 核心协议层

**模块路径**: `agentrt/protocols/core/`
**版本**: v0.1.0

## 概述

Core 层是 Protocols 协议栈的核心引擎层，提供协议路由、扩展框架、消息转换和注册中心四大核心能力。这些组件协同工作，实现跨协议消息的智能路由、格式转换、适配器生命周期管理以及协议发现与能力查询。

## 目录结构

```
core/
├── README.md
├── adapter/                            # 扩展框架
│   ├── include/
│   │   └── protocol_extension_framework.h
│   └── src/
│       └── protocol_extension_framework.c
├── registry/                           # 注册中心
│   ├── include/
│   │   └── protocol_registry.h
│   └── src/
│       └── protocol_registry.c
├── router/                             # 协议路由引擎
│   ├── include/
│   │   └── protocol_router.h
│   └── src/
│       └── protocol_router.c
└── transformers/                       # 消息转换器
    ├── include/
    │   └── protocol_transformers.h
    └── src/
        └── protocol_transformers.c
```

## 核心组件

| 组件 | 头文件 | 说明 |
|------|--------|------|
| **Router** | `protocol_router.h` | 协议路由引擎，基于规则匹配实现跨协议消息路由与转换 |
| **Extension Framework** | `protocol_extension_framework.h` | 插件式协议扩展框架，支持适配器热加载/卸载、中间件管道 |
| **Transformers** | `protocol_transformers.h` | 消息格式转换器，实现 JSON-RPC ↔ MCP/A2A/OpenAI/OpenJiuwen 双向转换 |
| **Registry** | `protocol_registry.h` | 协议注册中心，提供适配器注册/发现/能力查询/依赖追踪 |

## 组件协作关系

```
                    ┌──────────────────┐
                    │   Registry       │ ← 协议注册/发现/能力查询
                    └────────┬─────────┘
                             │
                    ┌────────▼─────────┐
                    │   Router         │ ← 规则匹配/消息路由
                    └────────┬─────────┘
                             │
              ┌──────────────┼──────────────┐
              │              │              │
     ┌────────▼──────┐ ┌────▼──────┐ ┌─────▼───────┐
     │ Transformers  │ │ Extension │ │  Gateway    │
     │ (格式转换)    │ │ Framework │ │ (协议检测)  │
     └───────────────┘ │ (插件管理) │ └─────────────┘
                       └───────────┘
```

## 依赖关系

| 依赖 | 来源 | 用途 |
|------|------|------|
| `unified_protocol.h` | `protocols/include/` | 统一消息模型 |
| `agentrt_protocol_interface.h` | `protocols/include/` | 适配器虚表与接口定义 |
| `memory_compat.h` | `commons/utils/compat/` | 内存管理宏 |
| `error.h` | `commons/utils/` | 错误码定义 |
| `logging.h` | `commons/utils/logging/` | 日志输出 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
