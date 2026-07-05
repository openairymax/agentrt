# Common — AgentRT 公共服务库

> **模块路径**: `agentrt/daemons/common/` | **版本**: v0.1.0

## 概述

`daemons/common/` 是所有守护进程共享的基础库，提供兼容层、工具函数和通用服务组件。它为上层守护进程屏蔽了操作系统差异，提供统一的服务生命周期管理、IPC 通信、日志、配置、安全、缓存等基础设施，是整个 Daemon 层的基石。

### 设计原则

- **接口契约化**（K-2）：统一的服务接口定义，明确的生命周期管理
- **安全内生**（E-1）：默认安全，所有请求必须验证，零信任架构
- **跨平台一致性**（E-4）：Windows/Linux/macOS 统一实现
- **资源确定性**（E-3）：Fail-closed 模式，外部依赖不可用时安全降级
- **错误可追溯**（E-6）：降级模式下记录明确的降级警告日志

## 目录结构

```
common/
├── CMakeLists.txt                   # 构建配置
├── README.md                        # 本文件
├── include/                         # 公共头文件
│   ├── agentrt_event_loop.h         # 事件循环
│   ├── alert_manager.h              # 智能告警管理器
│   ├── api_recovery.h               # API 恢复策略
│   ├── checkpoint.h                 # 任务检查点/状态持久化
│   ├── circuit_breaker.h            # 熔断器与自愈框架
│   ├── compat.h                     # 兼容性定义
│   ├── config_manager.h             # 配置管理器
│   ├── daemon_defaults.h            # 守护进程默认值
│   ├── daemon_errors.h              # 守护进程错误码
│   ├── daemon_event_driver.h        # 事件驱动框架
│   ├── daemon_security.h            # 安全框架集成（cupolas）
│   ├── error.h                      # 统一错误处理
│   ├── input_validator.h            # 输入校验器
│   ├── ipc_client.h                 # IPC 客户端
│   ├── ipc_service_bus.h            # IPC 服务总线
│   ├── jsonrpc_helpers.h            # JSON-RPC 2.0 辅助函数
│   ├── log_sanitizer.h              # 日志清洗器
│   ├── method_dispatcher.h          # 方法分发器
│   ├── orchestrator.h               # 编排器
│   ├── parallel_dispatcher.h        # 并行分发器
│   ├── param_validator.h            # 参数校验器
│   ├── platform.h                   # 平台兼容层
│   ├── safe_string_utils.h          # 安全字符串操作
│   ├── service_discovery.h          # 跨进程服务发现
│   ├── svc_auth.h                   # 认证中间件（JWT/API Key/速率限制）
│   ├── svc_cache.h                  # 缓存服务兼容层
│   ├── svc_common.h                 # 服务公共定义与生命周期管理
│   ├── svc_config.h                 # 配置服务兼容层
│   ├── svc_logger.h                 # 日志服务兼容层
│   ├── thread_pool.h                # 线程池
│   └── unified_metrics.h            # 统一指标采集
├── src/                             # 实现文件
│   ├── agentrt_event_loop.c
│   ├── alert_manager.c
│   ├── api_recovery.c
│   ├── checkpoint.c
│   ├── circuit_breaker.c
│   ├── config_manager.c
│   ├── daemon_event_driver.c
│   ├── daemon_security.c
│   ├── input_validator.c
│   ├── ipc_client.c
│   ├── ipc_service_bus.c
│   ├── jsonrpc_helpers.c
│   ├── log_sanitizer.c
│   ├── method_dispatcher.c
│   ├── orchestrator.c
│   ├── daemon_task_dispatcher.c
│   ├── param_validator.c
│   ├── platform_compat.c
│   ├── safe_string_utils.c
│   ├── service_discovery.c
│   ├── svc_auth.c
│   ├── svc_common.c
│   ├── thread_pool.c
│   └── unified_metrics.c
└── tests/                           # 单元测试
    ├── CMakeLists.txt
    ├── test_config.c
    ├── test_daemon_common.c
    ├── test_error.c
    ├── test_input_validator.c
    ├── test_ipc_client.c
    ├── test_ipc_service_bus.c
    ├── test_jsonrpc_helpers.c
    ├── test_logger.c
    ├── test_platform.c
    ├── test_safe_string_utils.c
    ├── test_strategies_recovery.c
    ├── test_svc_auth.c
    ├── test_svc_stop.c
    ├── test_daemon_security.c
    ├── test_service_discovery.c
    ├── test_thread_pool.c
    ├── test_log_sanitizer.c
    ├── test_api_recovery.c
    ├── test_agentrt_event_loop.c
    └── test_checkpoint.c
```

## 核心组件说明

### 1. 服务基础设施

| 组件 | 头文件 | 说明 |
|------|--------|------|
| **svc_common** | `svc_common.h` | 服务公共定义、生命周期管理、服务注册表、服务元数据、跨进程注册中心 |
| **svc_logger** | `svc_logger.h` | 统一日志接口，兼容 commons/logging，支持追踪上下文（TraceID/SpanID/SessionID） |
| **svc_config** | `svc_config.h` | 配置服务兼容层，支持配置加载、监视和热更新 |
| **svc_auth** | `svc_auth.h` | 认证中间件，支持 JWT Token、API Key、速率限制 |
| **svc_cache** | `svc_cache.h` | 缓存服务兼容层，基于 commons/cache，支持 TTL 和容量管理 |

### 2. IPC 通信

| 组件 | 头文件 | 说明 |
|------|--------|------|
| **ipc_service_bus** | `ipc_service_bus.h` | IPC 服务总线，守护进程间统一通信框架，支持多协议（JSON-RPC/MCP/A2A/OpenAI）、服务发现、负载均衡 |
| **ipc_client** | `ipc_client.h` | IPC 客户端，简化版 RPC 调用接口，支持连接池 |
| **service_discovery** | `service_discovery.h` | 跨进程服务发现，基于共享内存，支持心跳、负载均衡（轮询/加权/最少连接）、依赖追踪 |

### 3. JSON-RPC 2.0

| 组件 | 头文件 | 说明 |
|------|--------|------|
| **jsonrpc_helpers** | `jsonrpc_helpers.h` | JSON-RPC 2.0 辅助函数，请求解析、响应构建、参数提取、批量请求支持 |
| **method_dispatcher** | `method_dispatcher.h` | 方法分发器，基于注册表模式的高效路由，O(1) 查找，线程安全 |

### 4. 容错与恢复

| 组件 | 头文件 | 说明 |
|------|--------|------|
| **circuit_breaker** | `circuit_breaker.h` | 熔断器，三态模型（关闭→开启→半开），支持故障转移、慢调用检测、事件回调 |
| **checkpoint** | `checkpoint.h` | 任务检查点，状态持久化与恢复，支持快照、自动检查点钩子 |
| **alert_manager** | `alert_manager.h` | 智能告警管理，多级告警、规则引擎、告警抑制与去重、多通道通知 |
| **api_recovery** | `api_recovery.h` | API 恢复策略，支持重试、降级、回退等恢复模式 |

### 5. 安全

| 组件 | 头文件 | 说明 |
|------|--------|------|
| **daemon_security** | `daemon_security.h` | 安全框架集成，对接 cupolas 模块，提供输入清洗、权限检查、签名验证、安全凭据存储、审计日志 |
| **input_validator** | `input_validator.h` | 输入校验器，防止注入攻击 |
| **param_validator** | `param_validator.h` | 参数校验器，类型和格式验证 |
| **log_sanitizer** | `log_sanitizer.h` | 日志清洗器，防止敏感信息泄露 |
| **safe_string_utils** | `safe_string_utils.h` | 安全字符串操作，防止缓冲区溢出 |

### 6. 并发与调度

| 组件 | 头文件 | 说明 |
|------|--------|------|
| **thread_pool** | `thread_pool.h` | 线程池，支持任务队列和并发控制 |
| **agentrt_event_loop** | `agentrt_event_loop.h` | 事件循环，异步 I/O 处理 |
| **daemon_event_driver** | `daemon_event_driver.h` | 事件驱动框架 |
| **parallel_dispatcher** | `parallel_dispatcher.h` | 并行分发器 |
| **orchestrator** | `orchestrator.h` | 编排器，多服务协调 |

### 7. 平台与工具

| 组件 | 头文件 | 说明 |
|------|--------|------|
| **platform** | `platform.h` | 平台兼容层，操作系统差异屏蔽 |
| **compat** | `compat.h` | 兼容性定义 |
| **daemon_defaults** | `daemon_defaults.h` | 守护进程默认值常量 |
| **daemon_errors** | `daemon_errors.h` | 守护进程错误码定义 |
| **error** | `error.h` | 统一错误处理 |
| **unified_metrics** | `unified_metrics.h` | 统一指标采集 |
| **config_manager** | `config_manager.h` | 配置管理器 |

## 接口说明

### 服务生命周期管理（svc_common.h）

```c
agentrt_error_t agentrt_service_create(agentrt_service_t *service, const char *name,
                                       const agentrt_svc_interface_t *iface,
                                       const agentrt_svc_config_t *config);
agentrt_error_t agentrt_service_init(agentrt_service_t service);
agentrt_error_t agentrt_service_start(agentrt_service_t service);
agentrt_error_t agentrt_service_stop(agentrt_service_t service, bool force);
void agentrt_service_destroy(agentrt_service_t service);
agentrt_svc_state_t agentrt_service_get_state(agentrt_service_t service);
bool agentrt_service_is_ready(agentrt_service_t service);
bool agentrt_service_is_running(agentrt_service_t service);
agentrt_error_t agentrt_service_healthcheck(agentrt_service_t service);
agentrt_error_t agentrt_service_get_stats(agentrt_service_t service,
                                          agentrt_svc_stats_t *stats);
```

### IPC 服务总线（ipc_service_bus.h）

```c
ipc_service_bus_t ipc_service_bus_create(const char *bus_name,
                                         const ipc_bus_channel_config_t *config);
agentrt_error_t ipc_service_bus_start(ipc_service_bus_t bus);
agentrt_error_t ipc_service_bus_stop(ipc_service_bus_t bus);
agentrt_error_t ipc_service_bus_send(ipc_service_bus_t bus, const char *target_service,
                                     const ipc_bus_message_t *message);
agentrt_error_t ipc_service_bus_request(ipc_service_bus_t bus, const char *target_service,
                                        const ipc_bus_message_t *request,
                                        ipc_bus_message_t *response, uint32_t timeout_ms);
agentrt_error_t ipc_service_bus_broadcast(ipc_service_bus_t bus,
                                          const ipc_bus_message_t *message);
agentrt_error_t ipc_service_bus_register_handler(ipc_service_bus_t bus,
                                                 ipc_bus_message_handler_t handler,
                                                 void *user_data);
```

### JSON-RPC 辅助（jsonrpc_helpers.h）

```c
char *jsonrpc_build_error(int code, const char *message, int id);
char *jsonrpc_build_success(cJSON *result, int id);
int jsonrpc_parse_request(const char *raw, char **out_method, cJSON **out_params, int *out_id);
int jsonrpc_validate_request(cJSON *req);
const char *jsonrpc_get_string_param(cJSON *params, const char *key, const char *default_value);
int jsonrpc_get_int_param(cJSON *params, const char *key, int default_value);
```

### 方法分发器（method_dispatcher.h）

```c
method_dispatcher_t *method_dispatcher_create(size_t max_methods);
void method_dispatcher_destroy(method_dispatcher_t *disp);
int method_dispatcher_register(method_dispatcher_t *disp, const char *method,
                               method_fn handler, void *user_data);
int method_dispatcher_dispatch(method_dispatcher_t *disp, cJSON *request,
                               char *(*error_response_fn)(int, const char *, int),
                               void *user_data);
```

### 熔断器（circuit_breaker.h）

```c
cb_manager_t cb_manager_create(void);
circuit_breaker_t cb_create(cb_manager_t manager, const char *name, const cb_config_t *config);
bool cb_allow_request(circuit_breaker_t breaker);
void cb_record_success(circuit_breaker_t breaker, uint32_t duration_ms);
void cb_record_failure(circuit_breaker_t breaker, int32_t error_code);
cb_state_t cb_get_state(circuit_breaker_t breaker);
agentrt_error_t cb_execute_failover(circuit_breaker_t breaker, int32_t original_error,
                                    char *fallback_result, size_t result_size);
```

### 认证中间件（svc_auth.h）

```c
int auth_init(const auth_config_t *config);
int auth_authenticate(const char *auth_header, const char *client_id, auth_result_t *result);
int auth_jwt_generate_token(const char *subject, const char *role, char **out_token);
int auth_jwt_verify_token(const char *token, auth_result_t *result);
int auth_apikey_verify(const char *api_key, auth_result_t *result);
int auth_ratelimit_check(const char *client_id);
void auth_cleanup(void);
```

### 安全框架（daemon_security.h）

```c
int daemon_security_init(const daemon_security_config_t *config, agentrt_error_t *error);
int daemon_sanitize_llm_input(const char *input, char *output, size_t output_size);
int daemon_sanitize_tool_params(const char *tool_name, const char *params,
                                char *sanitized_tool, size_t tool_buf_size,
                                char *sanitized_params, size_t param_buf_size);
int daemon_check_tool_permission(const char *agent_id, const char *tool_name,
                                 const char *action);
int daemon_verify_package_signature(const char *package_path, bool *is_valid,
                                    cupolas_signer_info_t *signer_info);
int daemon_store_credential(const char *cred_id, cupolas_vault_cred_type_t cred_type,
                            const uint8_t *data, size_t data_len, const char *agent_id);
int daemon_audit_log_event(const char *service_name, const char *operation,
                           const char *resource, int result, const char *agent_id);
```

### 服务发现（service_discovery.h）

```c
service_discovery_t sd_create(const sd_config_t *config);
agentrt_error_t sd_register(service_discovery_t sd, const char *service_name,
                            const char *service_type, const sd_instance_t *instance,
                            const char *tags, const char *dependencies);
agentrt_error_t sd_discover(service_discovery_t sd, const char *service_name,
                            sd_instance_t *instances, uint32_t max_count,
                            uint32_t *found_count);
agentrt_error_t sd_select_instance(service_discovery_t sd, const char *service_name,
                                   sd_lb_strategy_t strategy, sd_instance_t *instance);
agentrt_error_t sd_heartbeat(service_discovery_t sd, const char *service_name,
                             const char *instance_id);
```

### 告警管理器（alert_manager.h）

```c
int am_init(const am_config_t *config);
int am_add_rule(const am_rule_t *rule);
int am_fire(const char *name, am_level_t level, const char *message,
            const char *source, const char *labels);
int am_resolve(const char *name);
int am_acknowledge(const char *name);
int am_evaluate(const char *metric_name, double value);
int am_get_active_alerts(am_alert_t *alerts, uint32_t max_count, uint32_t *found_count);
```

## 通信方式

common 模块本身不直接参与守护进程间通信，但提供了 IPC 通信的底层基础设施：

| 组件 | 通信方式 | 适用场景 |
|------|----------|----------|
| `ipc_service_bus` | Unix Socket / TCP | 守护进程间请求/响应通信 |
| `ipc_client` | Unix Socket / TCP | 简化版 RPC 客户端调用 |
| `service_discovery` | 共享内存 | 跨进程服务注册与发现 |

## 依赖关系

```
common
├── commons/utils/logging     # 统一日志系统
├── commons/utils/cache       # 统一缓存库
├── cupolas                   # 安全框架（可选，Fail-closed 模式）
├── cjson                     # JSON 解析库
└── platform                  # 操作系统抽象层
```

## 构建说明

common 模块作为静态库 `svc_common` 构建，被所有守护进程链接：

```bash
# 构建 common 模块（包含在 daemon 顶层构建中）
cmake -B build -DBUILD_TESTS=ON
cmake --build build

# 仅运行 common 测试
ctest --test-dir build -R "test_daemon_common|test_ipc|test_jsonrpc|test_logger|test_config|test_auth|test_safe_string|test_input_validator|test_platform|test_error|test_svc_stop|test_strategies_recovery"
```

## 使用示例

### 初始化守护进程服务

```c
#include "daemons/common/svc_common.h"
#include "daemons/common/svc_logger.h"
#include "daemons/common/ipc_service_bus.h"

agentrt_svc_config_t config = {
    .name = "my_daemon",
    .version = "1.0.0",
    .capabilities = AGENTRT_SVC_CAP_ASYNC | AGENTRT_SVC_CAP_CANCELABLE,
    .max_concurrent = 16,
    .timeout_ms = 5000,
    .auto_start = true,
    .enable_metrics = true,
    .enable_tracing = true
};

agentrt_service_t service;
agentrt_service_create(&service, "my_daemon", &iface, &config);
agentrt_service_init(service);
agentrt_service_start(service);
```

### 使用 IPC 服务总线

```c
ipc_service_bus_t bus = ipc_service_bus_create("main_bus", NULL);
ipc_service_bus_start(bus);

ipc_bus_message_t *msg = ipc_bus_message_create(
    IPC_BUS_MSG_REQUEST, IPC_BUS_PROTO_JSON_RPC, payload, payload_size);

ipc_bus_message_t response;
ipc_service_bus_request(bus, "llm_d", msg, &response, 5000);
```

### 使用方法分发器

```c
method_dispatcher_t *disp = method_dispatcher_create(32);
method_dispatcher_register(disp, "llm.generate", on_llm_generate, NULL);
method_dispatcher_register(disp, "llm.chat", on_llm_chat, NULL);
method_dispatcher_dispatch(disp, request, jsonrpc_build_error, &client_fd);
```

### 使用熔断器

```c
cb_manager_t cb_mgr = cb_manager_create();
circuit_breaker_t cb = cb_create(cb_mgr, "llm_service", NULL);

if (cb_allow_request(cb)) {
    agentrt_error_t err = call_llm_service();
    if (err == AGENTRT_OK) {
        cb_record_success(cb, duration_ms);
    } else {
        cb_record_failure(cb, err);
    }
} else {
    cb_execute_failover(cb, err, fallback_result, sizeof(fallback_result));
}
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
