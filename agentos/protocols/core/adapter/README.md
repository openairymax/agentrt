# Adapter — 协议扩展框架

**模块路径**: `agentos/protocols/core/adapter/`
**版本**: v0.1.0

## 概述

协议扩展框架（Protocol Extension Framework）是 Core 层的插件式适配器管理组件，支持第三方开发者创建和注册自定义协议适配器。提供协议生命周期管理、消息转换管道、能力声明与发现、协议协商和热加载等扩展机制。

核心设计：

1. **插件式协议注册** — 动态加载/卸载协议适配器，最多支持 64 个适配器
2. **消息转换管道** — 可组合的消息处理中间件链，最多 32 个中间件
3. **能力声明与发现** — 协议能力自动注册与查询，最多 128 种能力
4. **协议协商** — 客户端-服务端协议版本协商
5. **热加载** — 运行时动态添加协议支持

## 目录结构

```
adapter/
├── README.md
├── include/
│   └── protocol_extension_framework.h     # 扩展框架头文件
└── src/
    └── protocol_extension_framework.c     # 扩展框架实现
```

## 核心组件

### 数据类型

| 类型 | 说明 |
|------|------|
| `proto_ext_descriptor_t` | 扩展描述符，包含名称、版本、描述、作者、协议类型、能力标志、优先级、热加载标志 |
| `proto_ext_callbacks_t` | 扩展回调函数集，定义 `on_load/on_init/on_start/on_stop/on_unload` 生命周期回调，以及 `encode_message/decode_message/handle_request/negotiate_version/get_capabilities/on_error` |
| `proto_middleware_t` | 中间件定义，包含名称、处理函数、优先级、用户数据、启用标志 |
| `proto_ext_state_t` | 适配器状态枚举（UNLOADED / LOADED / INITIALIZED / RUNNING / ERROR / DISABLED） |
| `proto_ext_priority_t` | 优先级枚举（LOWEST=0 / LOW=25 / NORMAL=50 / HIGH=75 / HIGHEST=100） |
| `proto_ext_stats_t` | 适配器运行统计（状态、错误数、最后活动时间、已处理消息数） |

### 核心 API

| 函数 | 说明 |
|------|------|
| `proto_ext_framework_create()` / `proto_ext_framework_destroy()` | 创建/销毁扩展框架实例 |
| `proto_ext_register()` / `proto_ext_unregister()` | 注册/注销扩展适配器 |
| `proto_ext_load()` / `proto_ext_unload()` | 加载/卸载扩展（传入配置 JSON） |
| `proto_ext_start()` / `proto_ext_stop()` | 启动/停止扩展 |
| `proto_ext_send_message()` | 通过指定适配器发送消息 |
| `proto_ext_handle_request()` | 通过指定适配器处理请求 |
| `proto_ext_auto_route()` | 自动路由消息到匹配的适配器 |
| `proto_ext_negotiate()` | 协议版本协商 |
| `proto_ext_add_middleware()` / `proto_ext_remove_middleware()` | 添加/移除中间件 |
| `proto_ext_enable_middleware()` / `proto_ext_disable_middleware()` | 启用/禁用中间件 |
| `proto_ext_process_middleware_chain()` | 执行中间件链处理 |
| `proto_ext_list_adapters()` / `proto_ext_list_capabilities()` | 列出已注册适配器/能力 |
| `proto_ext_find_by_capability()` | 按能力标志查找适配器 |
| `proto_ext_get_state()` | 获取适配器状态 |
| `proto_ext_load_from_config()` | 从 JSON 配置批量加载扩展 |
| `proto_ext_get_global_instance()` | 获取全局框架实例 |

### 适配器生命周期

```
UNLOADED → LOADED → INITIALIZED → RUNNING
   ↑          ↓          ↓           ↓
   └──── DISABLED ← ERROR ←────────┘
```

## 依赖关系

| 依赖 | 来源 | 用途 |
|------|------|------|
| `agentos_protocol_interface.h` | `protocols/include/` | 适配器虚表与接口定义 |
| `unified_protocol.h` | `protocols/include/` | 统一消息模型 |

## 使用说明

```c
#include "protocol_extension_framework.h"

// 创建框架
proto_ext_framework_t *fw = proto_ext_framework_create();

// 定义扩展
proto_ext_descriptor_t desc = {
    .name = "my-protocol",
    .version = "1.0.0",
    .description = "My custom protocol adapter",
    .protocol_type = PROTOCOL_CUSTOM,
    .capabilities = PROTO_CAP_STREAMING | PROTO_CAP_FUNCTION_CALLING,
    .priority = PROTO_EXT_PRIORITY_NORMAL,
    .hot_loadable = true,
    .requires_auth = false,
};

// 注册并加载
proto_ext_register(fw, &desc, &callbacks);
proto_ext_load(fw, "my-protocol", "{\"endpoint\":\"localhost:8080\"}");
proto_ext_start(fw, "my-protocol");

// 添加中间件
proto_ext_add_middleware(fw, "logging", my_logging_middleware,
                         PROTO_EXT_PRIORITY_LOW, NULL);

// 通过扩展发送消息
proto_ext_send_message(fw, "my-protocol", &message);

// 清理
proto_ext_stop(fw, "my-protocol");
proto_ext_unload(fw, "my-protocol");
proto_ext_unregister(fw, "my-protocol");
proto_ext_framework_destroy(fw);
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
