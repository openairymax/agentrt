# Gateway – AgentOS 网关层

<div align="center">

[![Version](https://img.shields.io/badge/version-1.0.0.7-blue.svg)](https://gitee.com/spharx/agentos)
[![License](https://img.shields.io/badge/license-Apache--2.0-green.svg)](https://gitee.com/spharx/agentos/blob/main/LICENSE)
[![C11](https://img.shields.io/badge/C-11-blue.svg)](https://en.cppreference.com/w/c/11)

**协议转换层：连接内核与外部世界的桥梁**

**版本**: v1.1.0 | **最后更新**: 2026-04-06
**生产级状态**: ✅ 生产就绪 (Production Ready)

</div>

---

## 📐 架构定位

```
┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃                          AgentOS 总体架构                                     ┃
┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
┌──────────────────────────────────────────────────────────────────────────────┐
│                           服务层 (daemon)                                     │
│              llm_d | market_d | monit_d | sched_d | tool_d | gateway_d       │
└──────────────────────────────────────────────────────────────────────────────┘
                                    ↓↑
┌──────────────────────────────────────────────────────────────────────────────┐
│                     ★ 网关层 (gateway) ★ 本模块 ★                            │
│                   HTTP · WebSocket · Stdio                                    │
│                   协议转换：外部请求 → JSON-RPC 2.0                            │
└──────────────────────────────────────────────────────────────────────────────┘
                                    ↓↑
┌──────────────────────────────────────────────────────────────────────────────┐
│                          系统调用层 (agentos/atoms/syscall)                           │
│              task · memory · session · telemetry · agent                      │
└──────────────────────────────────────────────────────────────────────────────┘
```

**核心定位**：gateway 层只做一件事 —— **协议转换**，作为连接内核与外部世界的桥梁。

---

## 🎯 职责边界

### ✅ 职责范围内

| 功能 | 说明 | 实现文件 |
|------|------|----------|
| **HTTP 协议转换** | HTTP 请求 → JSON-RPC 2.0 | `http_gateway.c` |
| **WebSocket 协议转换** | WebSocket 消息 → JSON-RPC 2.0 | `ws_gateway.c` |
| **Stdio 协议转换** | 标准输入输出 → JSON-RPC 2.0 | `stdio_gateway.c` |
| **JSON-RPC 工具** | 请求验证、响应生成 | `jsonrpc.c` |
| **连接生命周期** | 连接建立、维护、关闭 | 各 gateway 实现 |

### ❌ 职责范围外

| 功能 | 正确归属 | 说明 |
|------|----------|------|
| 会话管理 | `agentos/atoms/syscall/session.c` | 通过 `agentos_sys_session_*` 调用 |
| 可观测性 | `agentos/atoms/syscall/telemetry.c` | 通过 `agentos_sys_telemetry_*` 调用 |
| 速率限制 | `agentos/atoms/syscall/rate_limiter.c` | 通过 syscall 接口调用 |
| 认证授权 | `agentos/cupolas/permission/` | 安全穹顶负责 |
| 配置管理 | `agentos/commons/utils/config/` | 统一配置库 |
| 平台适配 | `agentos/commons/platform/` | 跨平台抽象 |
| 网络工具 | `agentos/commons/utils/network/` | 网络连接池等 |
| 健康检查 | `agentos/daemon/monit_d/` | 监控守护进程 |
| 服务管理 | `agentos/daemon/gateway_d/` | 守护进程层 |

---

## 📁 目录结构

```
agentos/gateway/
├── include/
│   └── gateway.h              # 统一公共接口 (12个API)
├── src/
│   ├── gateway/               # 核心网关实现
│   │   ├── gateway.h          # 内部抽象接口 (ops表+inline)
│   │   ├── gateway_api.c      # 公共API包装函数
│   │   ├── http_gateway.c/h   # HTTP 协议转换 (libmicrohttpd)
│   │   ├── http_gateway_routes.c/h  # HTTP 路由处理 (路由表模式)
│   │   ├── ws_gateway.c/h     # WebSocket 协议转换 (libwebsockets)
│   │   └── stdio_gateway.c/h  # Stdio 协议转换 (REPL)
│   └── utils/                 # 共享工具模块
│       ├── jsonrpc.c/h        # JSON-RPC 2.0 工具 (标准+自定义错误码)
│       ├── syscall_router.c/h # 系统调用路由器 (5类×18方法)
│       ├── gateway_utils.h    # 公共工具函数 (time_ns/sleep/elapsed)
│       ├── gateway_rpc_handler.c/h  # 统一RPC处理器 (DRY核心)
│       └── gateway_rate_limiter.c/h # 令牌桶速率限制器 (DoS防护)
├── tests/                     # 单元测试 (4套×50+用例)
│   ├── test_gateway.c         # 网关生命周期测试 (7用例)
│   ├── test_jsonrpc.c         # JSON-RPC协议测试 (17用例)
│   ├── test_syscall_router.c  # 路由器测试 (8用例)
│   └── test_gateway_rpc_handler.c  # RPC处理器测试 (14用例)
├── docker/                    # Docker容器化配置
│   ├── Dockerfile             # 多阶段构建
│   ├── docker-compose.yml     # 编排配置
│   ├── docker-compose.dev.yml # 开发环境
│   ├── docker-compose.prod.yml# 生产环境
│   └── monitoring/            # Prometheus监控栈
├── deploy/                    # K8s部署配置
│   ├── README.md              # 部署指南
│   └── k8s/
│       ├── configmap.yaml     # 配置映射
│       ├── deployment.yaml    # 部署清单
│       ├── namespace.yaml     # 命名空间
│       └── service.yaml       # 服务暴露
├── config/
│   └── cppcheck.cfg           # 静态分析配置
├── CMakeLists.txt             # CMake构建配置
├── .clang-format              # 代码格式规范
├── .clang-tidy                # 静态分析规则
├── README.md                  # 模块文档 (本文件)
└── SECURITY.md                # 安全最佳实践
```

---

## 🔌 API 接口

> **注意**: 以下为当前已实现的公共 API。网关层遵循 K-1 内核极简原则，
> 只提供协议转换所需的**最小接口集**（12 个函数）。
> TLS 配置、路由注册、事件回调等高级功能属于 daemon 层或未来扩展。

### 创建网关

```c
#include <gateway.h>

/* HTTP 网关 - 监听 8080 端口 */
gateway_t* http_gw = gateway_http_create("0.0.0.0", 8080);

/* WebSocket 网关 - 监听 8081 端口 */
gateway_t* ws_gw = gateway_ws_create("0.0.0.0", 8081);

/* Stdio 网关 - 命令行交互 */
gateway_t* stdio_gw = gateway_stdio_create();
```

### 生命周期管理

```c
/* 启动网关 */
agentos_error_t err = gateway_start(gw);
if (err != AGENTOS_SUCCESS) {
    fprintf(stderr, "Failed to start gateway: %d\n", err);
    return;
}

/* 运行中... */

/* 停止网关 */
gateway_stop(gw);

/* 销毁网关 */
gateway_destroy(gw);
```

### 获取统计信息

```c
char* stats_json = NULL;
agentos_error_t err = gateway_get_stats(gw, &stats_json);
if (err == AGENTOS_SUCCESS && stats_json) {
    printf("Gateway stats: %s\n", stats_json);
    free(stats_json);
}
```

### 设置自定义请求处理回调

```c
/**
 * 自定义请求处理器示例
 * @param request_json JSON-RPC 请求字符串（只读）
 * @param response_json 输出响应字符串（由回调分配，调用者释放）
 * @param user_data 用户数据
 * @return 0 成功，非0 失败（返回 GATEWAY_ERROR_* 负值）
 */
int my_handler(const char* request_json, char** response_json, void* user_data) {
    (void)user_data;

    /* 解析并处理请求... */
    cJSON* req = cJSON_Parse(request_json);
    if (!req) return GATEWAY_ERROR_PROTOCOL;

    /* 构造响应 */
    cJSON* resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "result", "custom handler processed");
    *response_json = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    cJSON_Delete(req);

    return (*response_json) ? GATEWAY_SUCCESS : GATEWAY_ERROR_MEMORY;
}

/* 设置回调 */
gateway_set_handler(gw, my_handler, &my_context);

/* 清除回调（恢复默认 syscall 路由） */
gateway_set_handler(gw, NULL, NULL);
```

### 查询操作

```c
/* 获取网关类型 */
gateway_type_t type = gateway_get_type(gw);  // GATEWAY_TYPE_HTTP / WS / STDIO

/* 检查运行状态 */
if (gateway_is_running(gw)) {
    printf("Gateway is running\n");
}

/* 获取网关名称 */
const char* name = gateway_get_name(gw);  // "HTTP Gateway" / "WebSocket Gateway" / "Stdio Gateway"
```

### 完整 API 清单

| 函数 | 说明 | 返回类型 |
|------|------|---------|
| `gateway_http_create(host, port)` | 创建 HTTP 网关 | `gateway_t*` |
| `gateway_ws_create(host, port)` | 创建 WebSocket 网关 | `gateway_t*` |
| `gateway_stdio_create()` | 创建 Stdio 网关 | `gateway_t*` |
| `gateway_start(gw)` | 启动网关 | `agentos_error_t` |
| `gateway_stop(gw)` | 停止网关 | `agentos_error_t` |
| `gateway_destroy(gw)` | 销毁网关 | `void` |
| `gateway_set_handler(gw, fn, data)` | 设置自定义回调 | `agentos_error_t` |
| `gateway_get_type(gw)` | 获取网关类型 | `gateway_type_t` |
| `gateway_is_running(gw)` | 检查是否运行中 | `bool` |
| `gateway_get_stats(gw, &json)` | 获取统计信息 | `agentos_error_t` |
| `gateway_get_name(gw)` | 获取网关名称 | `const char*` |

### HTTP 端点

```
POST /              # JSON-RPC 2.0 请求 → 系统调用
GET  /health        # 健康检查 → {"status":"healthy"}
GET  /metrics       # Prometheus 指标导出
OPTIONS /           # CORS 预检
其他路径             # 404 Not Found
```

---

网关层通过系统调用接口与内核通信，支持以下 syscall：

### 任务管理

| 系统调用 | 说明 |
|----------|------|
| `agentos_sys_task_submit` | 提交任务 |
| `agentos_sys_task_query` | 查询任务状态 |
| `agentos_sys_task_wait` | 等待任务完成 |
| `agentos_sys_task_cancel` | 取消任务 |

### 记忆管理

| 系统调用 | 说明 |
|----------|------|
| `agentos_sys_memory_write` | 写入记忆 |
| `agentos_sys_memory_search` | 搜索记忆 |
| `agentos_sys_memory_get` | 获取记忆 |
| `agentos_sys_memory_delete` | 删除记忆 |

### 会话管理

| 系统调用 | 说明 |
|----------|------|
| `agentos_sys_session_create` | 创建会话 |
| `agentos_sys_session_get` | 获取会话信息 |
| `agentos_sys_session_close` | 关闭会话 |
| `agentos_sys_session_list` | 列出会话 |

### 可观测性

| 系统调用 | 说明 |
|----------|------|
| `agentos_sys_telemetry_metrics` | 获取指标 |
| `agentos_sys_telemetry_traces` | 获取追踪 |

---

## 🌐 WebSocket 消息协议

### 消息类型

```json
{
    "jsonrpc": "2.0",
    "method": "agentos_sys_task_submit",
    "params": {
        "input": "分析这份报告并生成摘要",
        "timeout_ms": 30000
    },
    "id": 1
}
```

### JSON-RPC 响应示例

```json
{
    "jsonrpc": "2.0",
    "result": {
        "result": "任务已提交，ID: task_abc123"
    },
    "id": 1
}
```

---

## 🌐 WebSocket 消息协议

### 消息类型

| 类型 | 说明 |
|------|------|
| `ping` | 心跳请求 |
| `pong` | 心跳响应 |
| `rpc_request` | RPC 请求 |
| `rpc_response` | RPC 响应 |
| `notification` | 服务端通知 |
| `error` | 错误消息 |

### 消息格式

```json
{
    "type": "rpc_request",
    "session_id": "sess_abc123",
    "timestamp": 1711926000.123,
    "payload": {
        "jsonrpc": "2.0",
        "method": "agentos_sys_memory_search",
        "params": {
            "query": "项目进度报告",
            "limit": 10
        },
        "id": 1
    }
}
```

---

## 💻 Stdio 命令行接口

### 可用命令

| 命令 | 说明 |
|------|------|
| `help` | 显示帮助信息 |
| `rpc <json>` | 执行 JSON-RPC 调用 |
| `stats` | 显示网关统计 |
| `exit` | 退出网关 |

### 使用示例

```
AgentOS Stdio Gateway started. Type 'help' for available commands.
> rpc {"jsonrpc":"2.0","method":"agentos_sys_session_create","id":1}
{"jsonrpc":"2.0","result":{"session_id":"sess_xyz789"},"id":1}
> stats
{"commands_total":1,"commands_failed":0,"bytes_received":56,"bytes_sent":45}
> exit
Gateway shutting down...
```

---

## 🏗️ 依赖关系

```
agentos/gateway/
├── 依赖 agentos/atoms/syscall/         # 系统调用接口
│   ├── session.c               # 会话管理
│   ├── telemetry.c             # 可观测性
│   └── rate_limiter.c          # 速率限制
├── 依赖 agentos/commons/               # 基础库
│   ├── agentos/                # 公共类型定义
│   └── utils/                  # 工具函数
├── 依赖外部库
│   ├── libmicrohttpd           # HTTP 服务器
│   ├── libwebsockets           # WebSocket 服务器
│   └── cJSON                   # JSON 解析
└── 被 agentos/daemon/gateway_d/        # 守护进程调用
```

---

## ⚙️ 编译

### 依赖安装

```bash
# Ubuntu/Debian
sudo apt install libmicrohttpd-dev libwebsockets-dev libcjson-dev

# macOS
brew install libmicrohttpd libwebsockets cjson

# Windows (vcpkg)
vcpkg install libmicrohttpd libwebsockets cjson
```

### 构建

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel $(nproc)
```

### 测试

```bash
cmake --build . --target test_gateway
./test_gateway
```

---

## 📊 性能指标

| 指标 | 数值 | 说明 |
|------|------|------|
| HTTP 请求延迟 | < 5ms | 不含 syscall 处理时间 |
| WebSocket 消息延迟 | < 3ms | 单向消息处理 |
| 并发连接数 | 1000+ | HTTP 连接池 |
| 请求解析 | < 1ms | JSON-RPC 验证 |
| 内存占用 | < 10MB | 空闲状态 |

---

## 📜 重构历史

### v2.0.0 (2026-04-01)

**重大重构**：删除所有重复功能，恢复正确的架构定位。

**删除的文件**（16 个，重复/错位实现）：

| 文件 | 原因 | 正确归属 |
|------|------|----------|
| `session.c/h` | 重复实现 | `agentos/atoms/syscall/session.c` |
| `session_stats.c` | 依赖已删除模块 | - |
| `telemetry.c/h` | 重复实现 | `agentos/atoms/syscall/telemetry.c` |
| `ratelimit.c/h` | 重复实现 | `agentos/atoms/syscall/rate_limiter.c` |
| `auth.c/h` | 错位功能 | `agentos/cupolas/permission/` |
| `config.c/h` | 错位功能 | `agentos/commons/utils/config/` |
| `config_runtime.c` | 依赖已删除模块 | - |
| `platform.c/h` | 错位功能 | `agentos/commons/platform/` |
| `connection_pool.c/h` | 错位功能 | `agentos/commons/utils/network/` |
| `health.c/h` | 错位功能 | `agentos/daemon/monit_d/` |
| `server.c/h` | 错位功能 | `agentos/daemon/gateway_d/` |
| `main.c` | 错位功能 | `agentos/daemon/gateway_d/` |
| `agentos.c/h` | 错位功能 | `agentos/commons/agentos/` |
| `logger.h` | 错位功能 | `agentos/commons/utils/logging/` |
| `utils/hash.c/h` | 错位功能 | `agentos/commons/utils/` |
| `utils/secure_mem.c/h` | 错位功能 | `agentos/commons/utils/` |

**保留并持续完善的文件**（核心功能）：

| 文件 | 功能 | 状态 |
|------|------|------|
| `include/gateway.h` | 统一公共接口 (12 API) | ✅ 生产级 |
| `src/agentos/gateway/gateway.h` | 内部抽象接口 (ops表+inline) | ✅ 生产级 |
| `src/agentos/gateway/gateway_api.c` | 公共API包装函数 | ✅ 新增 |
| `src/agentos/gateway/http_gateway.c/h` | HTTP 协议转换 (libmicrohttpd) | ✅ 生产级 |
| `src/agentos/gateway/ws_gateway.c/h` | WebSocket 协议转换 (libwebsockets) | ✅ 生产级 |
| `src/agentos/gateway/stdio_gateway.c/h` | Stdio 协议转换 (REPL) | ✅ 生产级 |
| `src/utils/jsonrpc.c/h` | JSON-RPC 2.0 工具 | ✅ 完整 |
| `src/utils/syscall_router.c/h` | 系统调用路由器 (5类×18方法) | ✅ 新增 |
| `src/utils/gateway_utils.h` | 公共工具函数 | ✅ 新增 |

**新增（本轮修复）**：

- `agentos/daemon/gateway_d/` - 用户态守护进程服务
- `gateway_api.c` - 公共 API 符号导出
- `syscall_router.c/h` - 集中式 syscall 路由
- `gateway_utils.h` - 跨网关公共工具

---

## 🏛️ 架构原则遵循

本模块严格遵循 `ARCHITECTURAL_PRINCIPLES.md` 中定义的五维正交系统原则：

### 系统观 (S-1~S-4)

- **S-2 层次分解**: 只负责协议转换，不越级访问内核
- **S-3 总体设计部**: 只做协议转换，不做业务执行

### 内核观 (K-1~K-4)

- **K-1 微内核化**: 保持单一职责，代码精简
- **K-2 接口约束**: 所有公共 API 都有 Doxygen 注释

### 工程观 (E-1~E-8)

- **E-1 安全内生**: 通过 syscall 接口隔离内核
- **E-2 可观测性**: 支持指标导出和链路追踪
- **E-4 跨平台一致**: 支持 Linux/macOS/Windows

### 设计美学 (A-1~A-4)

- **A-1 简约至上**: 只做协议转换，不做其他
- **A-2 细节极致**: 完整的 Doxygen 注释和错误处理

---

## 📄 许可证

Copyright (C) 2026 SPHARX. All Rights Reserved.

SPDX-License-Identifier: Apache-2.0

---

<div align="center">

*From data intelligence emerges.*

**[返回主目录](../README.md)**

</div>
