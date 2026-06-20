# Registry — 协议注册中心

**模块路径**: `agentos/protocols/core/registry/`
**版本**: v0.1.0

## 概述

协议注册中心（Protocol Registry）是 AgentRT 协议系统的核心组件，提供协议适配器的注册/注销/发现、能力查询与匹配、版本管理、依赖关系追踪、运行时统计与监控以及热加载/卸载支持。所有协议适配器（内置和自定义）均通过注册中心进行统一管理。

## 目录结构

```
registry/
├── README.md
├── include/
│   └── protocol_registry.h        # 注册中心头文件
└── src/
    └── protocol_registry.c        # 注册中心实现
```

## 核心组件

### 数据类型

| 类型 | 说明 |
|------|------|
| `proto_registry_entry_t` | 注册条目，包含名称、版本、描述、分类、协议类型、能力标志、状态、适配器指针、依赖列表、统计信息 |
| `proto_category_t` | 协议分类枚举（CORE / STANDARD / INTEGRATION / FRAMEWORK / CUSTOM） |
| `proto_state_t` | 协议状态枚举（UNREGISTERED / REGISTERED / INITIALIZING / READY / ACTIVE / DEGRADED / ERROR / SHUTDOWN） |
| `proto_dependency_t` | 依赖声明，包含名称与状态 |
| `proto_registry_stats_t` | 注册中心全局统计（总条目数/活跃数/内置数/自定义数/总请求/总错误/平均延迟） |
| `proto_registry_event_fn` | 状态变更事件回调函数类型 |

### 核心 API

| 函数 | 说明 |
|------|------|
| `proto_registry_create()` / `proto_registry_destroy()` | 创建/销毁注册中心实例 |
| `proto_registry_version()` | 返回注册中心版本（当前 "2.1.0"） |
| `proto_registry_register()` | 注册协议适配器（指定名称、版本、描述、分类、类型、能力、适配器指针） |
| `proto_registry_unregister()` | 注销协议适配器 |
| `proto_registry_find()` | 按名称查找协议 |
| `proto_registry_find_by_type()` | 按协议类型查找 |
| `proto_registry_find_by_capability()` | 按能力标志查找 |
| `proto_registry_list_all()` | 列出所有已注册协议 |
| `proto_registry_list_by_category()` | 按分类列出协议 |
| `proto_registry_list_active()` | 列出所有活跃协议 |
| `proto_registry_set_state()` | 设置协议状态 |
| `proto_registry_add_dependency()` | 添加协议依赖关系 |
| `proto_registry_check_dependencies()` | 检查协议依赖是否满足 |
| `proto_registry_activate()` / `proto_registry_deactivate()` | 激活/停用协议 |
| `proto_registry_heartbeat()` | 发送心跳（更新最后活跃时间） |
| `proto_registry_record_request()` | 记录请求（成功/失败、延迟） |
| `proto_registry_get_statistics()` | 获取全局统计信息 |
| `proto_registry_export_json()` | 导出注册信息为 JSON |
| `proto_registry_set_event_callback()` | 设置状态变更事件回调 |
| `proto_registry_initialize_builtins()` | 初始化内置协议（JSON-RPC / MCP / A2A / OpenAI / OpenJiuwen / OpenClaw / Claude / AGNTCY / ChinaEco） |
| `proto_category_to_string()` / `proto_state_to_string()` | 枚举值转字符串 |

### 协议状态机

```
UNREGISTERED → REGISTERED → INITIALIZING → READY → ACTIVE
                  ↑              ↓            ↓       ↓  ↓
                  └──── SHUTDOWN ← ERROR ← DEGRADED ←┘  │
                          ↑                              │
                          └──────────────────────────────┘
```

### 常量限制

| 常量 | 值 | 说明 |
|------|-----|------|
| `PROTO_REGISTRY_MAX_ADAPTERS` | 32 | 最大适配器数量 |
| `PROTO_REGISTRY_MAX_DEPS` | 16 | 最大依赖数量 |
| `PROTO_REGISTRY_NAME_MAX_LEN` | 64 | 名称最大长度 |
| `PROTO_REGISTRY_DESC_MAX_LEN` | 256 | 描述最大长度 |

## 依赖关系

| 依赖 | 来源 | 用途 |
|------|------|------|
| `agentos_protocol_interface.h` | `protocols/include/` | 适配器虚表与接口定义 |
| `unified_protocol.h` | `protocols/include/` | 统一消息模型与协议类型 |

## 使用说明

```c
#include "protocol_registry.h"

// 创建注册中心
protocol_registry_t *registry = proto_registry_create();

// 初始化内置协议
int count = proto_registry_initialize_builtins(registry);

// 注册自定义协议
proto_registry_register(registry, "my-proto", "1.0.0",
    "My custom protocol", PROTO_CAT_CUSTOM, PROTOCOL_CUSTOM,
    PROTO_CAP_STREAMING, &my_adapter, NULL);

// 查找协议
proto_registry_entry_t *entry = proto_registry_find(registry, "my-proto");

// 按能力查找
proto_registry_entry_t *results = NULL;
size_t n = proto_registry_find_by_capability(registry,
    PROTO_CAP_STREAMING, &results);

// 激活协议
proto_registry_activate(registry, "my-proto");

// 记录请求
proto_registry_record_request(registry, "my-proto", true, 12.5);

// 获取统计
proto_registry_stats_t stats;
proto_registry_get_statistics(registry, &stats);

// 清理
proto_registry_destroy(registry);
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
