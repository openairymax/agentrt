# Tool Daemon — 工具执行守护进程

> **模块路径**: `agentos/daemon/tool_d/` | **版本**: v0.0.5

## 概述

`daemon/tool_d/` 是 AgentOS 的工具执行守护进程，负责外部工具的注册、发现、安全执行和结果管理。它在沙箱环境中执行工具代码，提供参数校验、结果缓存和执行限流等安全保障，是 Agent 与外部世界交互的关键执行层。通过 daemon_security 集成，所有工具执行都经过输入清洗和权限检查。

### 核心职责

- **工具注册**：动态注册和卸载外部工具，管理工具元数据
- **安全执行**：在沙箱环境中执行工具代码，隔离风险
- **参数校验**：校验工具调用参数的类型和格式（JSON Schema）
- **结果缓存**：缓存相同参数的工具执行结果，提升性能
- **执行限流**：控制工具调用频率，防止滥用
- **流式输出**：支持工具执行的实时流式输出
- **权限控制**：通过 daemon_security 进行工具执行权限检查
- **输入清洗**：通过 daemon_security 防止命令注入

## 目录结构

```
tool_d/
├── CMakeLists.txt                    # 构建配置
├── README.md                         # 本文件
├── include/                          # 公共头文件
│   ├── tool_service.h                # 工具服务对外接口
│   └── tool_svc_adapter.h            # 工具服务适配器接口
├── src/                              # 实现文件
│   ├── main.c                        # 守护进程入口
│   ├── service.c                     # 服务核心实现
│   ├── service.h                     # 服务内部头文件
│   ├── tool_svc_adapter.c            # 请求解析与标准化适配器
│   ├── registry.h                    # 工具注册表头文件
│   ├── registry.c                    # 工具注册与查找
│   ├── executor.h                    # 执行器头文件
│   ├── executor.c                    # 沙箱执行引擎
│   ├── validator.h                   # 校验器头文件
│   ├── validator.c                   # 参数校验实现
│   ├── cache.h                       # 缓存头文件
│   ├── cache.c                       # 结果缓存实现
│   ├── config.h                      # 配置头文件
│   ├── config.c                      # 配置管理
│   └── utils/                        # 工具辅助函数
│       ├── tool_helpers.h            # 辅助函数头文件
│       ├── tool_helpers.c            # 辅助函数实现
│       └── tool_errors.h             # 错误码定义
└── tests/                            # 单元测试
    ├── CMakeLists.txt
    ├── test_tool.c                   # 集成测试
    ├── test_service.c                # 服务测试
    ├── test_registry.c               # 注册表测试
    ├── test_executor.c               # 执行器测试
    ├── test_validator.c              # 校验器测试
    └── test_cache.c                  # 缓存测试
```

## 核心组件说明

### 架构

```
客户端请求 (JSON-RPC 2.0)
       ↓
  tool_svc_adapter  ← 请求解析与标准化
       ↓
  tool_service      ← 核心服务逻辑（路由、限流、缓存）
       ↓
  ┌──────┬──────┬──────┬──────┐
  │registry │executor│validator│ cache│
  └──────┴──────┴──────┴──────┘
       ↓
  工具运行时（Sandbox）
```

### 工具注册表（registry）

管理工具的元数据和生命周期：

- 工具唯一标识（ID）
- 工具名称和描述
- 可执行文件路径或命令
- 参数定义（JSON Schema 格式）
- 执行超时配置
- 缓存策略配置
- 权限规则标识

### 执行器（executor）

沙箱执行引擎，负责工具的安全执行：

- 沙箱环境隔离
- 执行超时控制
- 标准输出/错误捕获
- 进程退出码获取
- 执行耗时统计
- 流式输出支持

### 校验器（validator）

参数校验模块，确保工具调用的参数合法：

- 基于 JSON Schema 的参数类型校验
- 参数格式验证
- 必填参数检查
- 参数范围约束

### 缓存（cache）

结果缓存模块，避免重复执行：

- 基于工具 ID + 参数哈希的缓存键
- 可配置的缓存容量和 TTL
- 支持手动清除缓存
- 仅缓存标记为可缓存的工具结果

## 接口说明

### 工具服务生命周期（tool_service.h）

```c
tool_service_t *tool_service_create(const char *config_path);
void tool_service_destroy(tool_service_t *svc);
```

### 工具管理接口

```c
int tool_service_register(tool_service_t *svc, const tool_metadata_t *meta);
int tool_service_unregister(tool_service_t *svc, const char *tool_id);
tool_metadata_t *tool_service_get(tool_service_t *svc, const char *tool_id);
void tool_metadata_free(tool_metadata_t *meta);
char *tool_service_list(tool_service_t *svc);
```

### 工具执行接口

```c
int tool_service_execute(tool_service_t *svc, const tool_execute_request_t *req,
                         tool_result_t **out_result);
int tool_service_execute_stream(tool_service_t *svc, const tool_execute_request_t *req,
                                tool_stream_callback_t callback, void *callback_data,
                                tool_result_t **out_result);
void tool_result_free(tool_result_t *res);
```

### 核心数据结构

```c
typedef struct {
    const char *name;
    const char *schema;
} tool_param_t;

typedef struct {
    char *id;
    char *name;
    char *description;
    char *executable;
    tool_param_t *params;
    size_t param_count;
    int timeout_sec;
    int cacheable;
    char *permission_rule;
} tool_metadata_t;

typedef struct {
    const char *tool_id;
    const char *params_json;
    int stream;
    void *user_data;
} tool_execute_request_t;

typedef struct {
    int success;
    char *output;
    char *error;
    int exit_code;
    uint64_t duration_ms;
} tool_result_t;

typedef void (*tool_stream_callback_t)(const char *chunk, int is_stderr, void *user_data);
```

### JSON-RPC 2.0 方法

| 方法 | 说明 |
|------|------|
| `tool.register` | 注册新工具 |
| `tool.execute` | 执行工具调用 |
| `tool.list` | 列出可用工具 |
| `tool.info` | 查询工具详细信息 |
| `tool.unregister` | 卸载工具 |
| `tool.cache.clear` | 清除工具缓存 |

## 通信方式

| 方向 | 协议 | 说明 |
|------|------|------|
| 入站 | JSON-RPC 2.0 | 通过 IPC Service Bus 接收请求 |
| 出站 | 进程调用 | 在沙箱中执行工具可执行文件 |

## 依赖关系

```
tool_d
├── common (svc_common, svc_logger, svc_config, svc_cache, ipc_service_bus,
│           method_dispatcher, jsonrpc_helpers, daemon_security, circuit_breaker,
│           input_validator, param_validator)
├── cupolas (daemon_security)  # 输入清洗、权限检查、审计日志
└── llm_d  # 工具调用可能触发 LLM 推理
```

## 构建说明

```bash
# 构建工具守护进程
cmake -B build -DBUILD_TESTS=ON
cmake --build build --target agentos-tool-d

# 运行工具测试
ctest --test-dir build -R "test_tool|test_service|test_registry|test_executor|test_validator|test_cache" -V
```

## 使用示例

### 启动工具守护进程

```bash
# 启动工具守护进程
./tool_d --config tool_config.json

# 指定工具目录
./tool_d --tool-dir /opt/agentos/tools

# 启用缓存
./tool_d --cache-size 1000
```

### 代码调用示例 — 注册工具

```c
#include "daemon/tool_d/include/tool_service.h"

tool_service_t *svc = tool_service_create("tool_config.json");

tool_param_t params[] = {
    {.name = "query", .schema = "{\"type\":\"string\"}"},
    {.name = "limit", .schema = "{\"type\":\"integer\",\"minimum\":1,\"maximum\":100}"}
};

tool_metadata_t meta = {
    .id = "web-search",
    .name = "Web Search",
    .description = "Search the web for information",
    .executable = "/opt/agentos/tools/web-search",
    .params = params,
    .param_count = 2,
    .timeout_sec = 30,
    .cacheable = 1,
    .permission_rule = "web_search:execute"
};

tool_service_register(svc, &meta);
```

### 代码调用示例 — 执行工具

```c
tool_execute_request_t req = {
    .tool_id = "web-search",
    .params_json = "{\"query\":\"AgentOS architecture\",\"limit\":10}",
    .stream = 0
};

tool_result_t *result = NULL;
tool_service_execute(svc, &req, &result);

if (result->success == 0) {
    printf("Output: %s\n", result->output);
    printf("Duration: %lu ms\n", result->duration_ms);
} else {
    printf("Error: %s (exit code: %d)\n", result->error, result->exit_code);
}

tool_result_free(result);
tool_service_destroy(svc);
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
