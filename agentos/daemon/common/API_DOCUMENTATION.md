# AgentOS Daemon 公共库 API 文档

**版本**: 2.0.0  
**日期**: 2026-04-04  
**模块**: agentos/daemon/common  
**遵循规范**: ARCHITECTURAL_PRINCIPLES.md V1.8

---

## 目录

1. [概述](#概述)
2. [JSON-RPC 辅助函数库](#json-rpc-辅助函数库)
3. [方法分发器框架](#方法分发器框架)
4. [安全字符串工具](#安全字符串工具)
5. [输入验证器](#输入验证器)
6. [Agent Registry Core](#agent-registry-core)
7. [使用示例](#使用示例)
8. [性能指标](#性能指标)

---

## 概述

本公共库为AgentOS daemon层提供统一的基础设施，包括：

| 组件 | 功能 | 头文件 |
|------|------|--------|
| **jsonrpc_helpers** | JSON-RPC 2.0 协议处理 | jsonrpc_helpers.h |
| **method_dispatcher** | 方法分发与路由 | method_dispatcher.h |
| **safe_string_utils** | 安全字符串操作 | safe_string_utils.h |
| **input_validator** | 输入验证框架 | input_validator.h |

### 设计原则

- ✅ **E-3 资源确定性**: 所有资源成对管理
- ✅ **A-1 简约至上**: API最小化设计
- ✅ **K-2 接口契约化**: 完整Doxygen注释
- ✅ **E-5 命名语义化**: 统一命名规范
- ✅ **E-6 错误可追溯**: 错误码+错误链

---

## JSON-RPC 辅助函数库

### 简介

提供统一的JSON-RPC 2.0请求/响应处理，消除各服务中的重复代码。

### 响应构建

#### `jsonrpc_build_error`

```c
char* jsonrpc_build_error(int code, const char* message, int id);
```

**功能**: 构建JSON-RPC错误响应

**参数**:
- `code`: 错误码（标准或自定义）
- `message`: 错误消息（可为NULL，使用默认消息）
- `id`: 请求ID

**返回**: JSON字符串指针，调用者负责`free()`

**标准错误码**:
```c
#define JSONRPC_PARSE_ERROR      -32700
#define JSONRPC_INVALID_REQUEST  -32600
#define JSONRPC_METHOD_NOT_FOUND -32601
#define JSONRPC_INVALID_PARAMS   -32602
#define JSONRPC_INTERNAL_ERROR   -32000
```

**示例**:
```c
// 基础用法
char* resp = jsonrpc_build_error(JSONRPC_INVALID_PARAMS, 
                                  "Missing required field", 42);
send_response(client_fd, resp);
free(resp);

// 使用默认消息
char* resp = jsonrpc_build_error(-32001, NULL, 1);
```

#### `jsonrpc_build_success`

```c
char* jsonrpc_build_success(cJSON* result, int id);
```

**功能**: 构建JSON-RPC成功响应

**注意**: `result`对象会被消费，无需手动释放

**示例**:
```c
cJSON* result = cJSON_CreateObject();
cJSON_AddStringToObject(result, "status", "ok");
char* resp = jsonrpc_build_success(result, 1); // result被自动释放
send_response(client_fd, resp);
free(resp);
```

### 请求解析

#### `jsonrpc_parse_request`

```c
int jsonrpc_parse_request(const char* raw, 
                          char** out_method, 
                          cJSON** out_params, 
                          int* out_id);
```

**功能**: 解析原始JSON-RPC请求字符串

**返回值**: 0成功，非0失败（标准错误码）

**资源管理**:
- `out_method`: 调用者需`free()`
- `out_params`: 调用者需`cJSON_Delete()`

**示例**:
```c
const char* raw = "{\"method\":\"test\",\"params\":{\"key\":\"val\"},\"id\":1}";
char* method = NULL;
cJSON* params = NULL;
int id = 0;

if (jsonrpc_parse_request(raw, &method, &params, &id) == 0) {
    // 处理请求...
    
    // 清理资源
    free(method);
    if (params) cJSON_Delete(params);
}
```

### 参数提取

#### `jsonrpc_get_string_param`

```c
const char* jsonrpc_get_string_param(cJSON* params, 
                                      const char* key, 
                                      const char* default_value);
```

**功能**: 安全提取字符串参数

**特点**: 返回指向JSON内部的指针，无需释放；缺失时返回默认值

**示例**:
```c
const char* model = jsonrpc_get_string_param(params, "model", "gpt-4");
const char* api_key = jsonrpc_get_string_param(params, "api_key", NULL);
if (!api_key) {
    char* err = jsonrpc_build_error(-1, "API key is required", id);
    send_error(err);
}
```

---

## 方法分发器框架

### 简介

使用函数指针表替代冗长的if-else/switch-case，降低main.c复杂度。

### 核心接口

#### 创建与销毁

```c
method_dispatcher_t* method_dispatcher_create(size_t max_methods);
void method_dispatcher_destroy(method_dispatcher_t* disp);
```

#### 注册处理器

```c
typedef void (*method_fn)(cJSON* params, int id, void* user_data);

int method_dispatcher_register(method_dispatcher_t* disp,
                               const char* method,
                               method_fn handler,
                               void* user_data);
```

#### 分发请求

```c
int method_dispatcher_dispatch(method_dispatcher_t* disp,
                              cJSON* request,
                              char* (*error_response_fn)(int code, const char* msg, int id),
                              void* user_data);
```

### 使用示例

```c
// 1. 定义处理器函数
static void handle_register(cJSON* params, int id, void* user_data) {
    svc_config_t* config = (svc_config_t*)user_data;
    // ... 处理逻辑 ...
}

static void handle_execute(cJSON* params, int id, void* user_data) {
    // ... 处理逻辑 ...
}

// 2. 初始化分发器
method_dispatcher_t* disp = method_dispatcher_create(16);

// 3. 注册处理器
method_dispatcher_register(disp, "register", handle_register, g_config);
method_dispatcher_register(disp, "execute", handle_execute, g_config);

// 4. 在事件循环中分发
while (running) {
    char buffer[MAX_BUFFER];
    ssize_t n = recv(client_fd, buffer, sizeof(buffer), 0);
    if (n > 0) {
        cJSON* req = cJSON_Parse(buffer);
        if (req) {
            method_dispatcher_dispatch(disp, req, 
                                       jsonrpc_build_error, client_fd);
            cJSON_Delete(req);
        }
    }
}

// 5. 清理
method_dispatcher_destroy(disp);
```

### 性能优势

| 指标 | if-else方式 | Method Dispatcher |
|------|------------|-------------------|
| **圈复杂度** | O(n) | O(1) |
| **查找时间** | 线性扫描 | 直接索引 |
| **代码量** | ~200行 | ~50行 |
| **可扩展性** | 需修改核心代码 | 只需注册新处理器 |

---

## 安全字符串工具

### 简介

防止缓冲区溢出的安全字符串操作函数集。

### 核心函数

#### `safe_strcpy`

```c
int safe_strcpy(char* dest, const char* src, size_t dest_size);
```

**安全特性**:
- 自动检查NULL参数
- 自动截断过长字符串
- 保证目标缓冲区以'\0'结尾

**返回值**:
- 0: 成功
- -1: 参数无效
- -2: 截断发生

**示例**:
```c
char buf[64];
int ret = safe_strcpy(buf, user_input, sizeof(buf));
switch (ret) {
    case 0:   break; // 正常
    case -2:  log_warn("Input truncated"); break;
    default:  log_error("Invalid input"); return;
}
```

#### `secure_clear`

```c
void secure_clear(void* buf, size_t size);
```

**功能**: 安全擦除敏感数据（防止编译器优化）

**用途**: 密码、Token、密钥等敏感信息清理

**示例**:
```c
char password[64];
// ... 使用密码 ...
secure_clear(password, sizeof(password)); // 彻底清除
```

---

## 输入验证器

### 简介

统一的输入验证框架，支持多种验证规则和自定义规则。

### 验证规则类型

| 类型 | 说明 | 参数 |
|------|------|------|
| `VALIDATE_REQUIRED` | 必填字段 | 无 |
| `VALIDATE_STRING` | 字符串类型 | 无 |
| `VALIDATE_NUMBER` | 数字类型 | 无 |
| `VALIDATE_MIN_LENGTH` | 最小长度 | `length_value` |
| `VALIDATE_MAX_LENGTH` | 最大长度 | `length_value` |
| `VALIDATE_MIN_VALUE` | 最小值 | `number_value` |
| `VALIDATE_MAX_VALUE` | 最大值 | `number_value` |

### 使用示例

```c
// 1. 创建验证器
validation_result_t* v = validator_create();

// 2. 添加规则
validation_rule_t rules[] = {
    { .type = VALIDATE_REQUIRED, .field_name = "username" },
    { .type = VALIDATE_STRING, .field_name = "username" },
    { .type = VALIDATE_MIN_LENGTH, .field_name = "username", .length_value = 3 },
    { .type = VALIDATE_MAX_LENGTH, .field_name = "username", .length_value = 32 },
    { .type = VALIDATE_REQUIRED, .field_name = "password" },
    { .type = VALIDATE_MIN_LENGTH, .field_name = "password", .length_value = 8 },
};

for (size_t i = 0; i < sizeof(rules)/sizeof(rules[0]); i++) {
    validator_add_rule(v, &rules[i]);
}

// 3. 执行验证
v = validator_validate(v, request_params);

// 4. 检查结果
if (!v->valid) {
    char* error = jsonrpc_build_error(JSONRPC_INVALID_PARAMS, 
                                       v->error_message, request_id);
    send_error(error);
    free(error);
} else {
    // 验证通过，继续处理
}

validator_destroy(v);
```

### 便捷函数

```c
// 快速验证单个字段
char* error = NULL;
if (validate_string_field(params, "email", 5, 100, &error) != 0) {
    // error 包含详细错误信息
    handle_validation_error(error);
    free(error);
}
```

---

## Agent Registry Core

### 简介

重构后的Agent注册表核心模块，圈复杂度从130降至<30。

### 主要接口

```c
// 生命周期
agent_registry_t* agent_registry_create(void);
void agent_registry_destroy(agent_registry_t* reg);
int agent_registry_init(agent_registry_t* reg, const char* db_path);

// CRUD操作
int agent_registry_add(agent_registry_t* reg, const agent_entry_t* entry);
int agent_registry_remove(agent_registry_t* reg, const char* agent_id);
const agent_entry_t* agent_registry_get(agent_registry_t* reg, const char* agent_id);
size_t agent_registry_list(agent_registry_t* reg, ...);

// 搜索
size_t agent_registry_search_by_tag(agent_registry_t* reg, ...);
size_t agent_registry_search(agent_registry_t* reg, ...);

// 版本管理
int agent_registry_add_version(agent_registry_t* reg, ...);
const char* agent_registry_get_latest_version(agent_registry_t* reg, ...);
```

### 模块化结构

```
agent_registry_core (CC < 30)
├── 生命周期管理 (CC: 8)
│   ├── create / destroy
│   ├── init / shutdown
├── 基本操作 (CC: 12)
│   ├── add / remove / get
│   ├── list / count
├── 搜索功能 (CC: 10)
│   ├── search_by_tag
│   └── search (模糊搜索)
└── 版本管理 (CC: 9)
    ├── add_version
    ├── get_latest_version
    └── check_version
```

---

## 使用示例

### 完整的Daemon服务模板

```c
#include "jsonrpc_helpers.h"
#include "method_dispatcher.h"
#include "input_validator.h"
#include "safe_string_utils.h"

// 全局状态
static svc_config_t* g_config = NULL;
static method_dispatcher_t* g_disp = NULL;
static volatile int g_running = 1;

// ==================== 处理器实现 ====================

static void handle_ping(cJSON* params, int id, void* user_data) {
    cJSON* result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "pong", "ok");
    cJSON_AddNumberToObject(result, "timestamp", time(NULL));
    
    char* resp = jsonrpc_build_success(result, id);
    send_response(client_fd, resp);
    free(resp);
}

static void handle_status(cJSON* params, int id, void* user_data) {
    cJSON* result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "service", "my_service");
    cJSON_AddNumberToObject(result, "uptime", get_uptime());
    
    char* resp = jsonrpc_build_success(result, id);
    send_response(client_fd, resp);
    free(resp);
}

// ==================== 初始化 ====================

int service_init(const char* config_path) {
    // 加载配置
    g_config = load_config(config_path);
    if (!g_config) return -1;
    
    // 创建分发器
    g_disp = method_dispatcher_create(32);
    if (!g_disp) {
        free_config(g_config);
        return -1;
    }
    
    // 注册处理器
    method_dispatcher_register(g_disp, "ping", handle_ping, g_config);
    method_dispatcher_register(g_disp, "status", handle_status, g_config);
    
    SVC_LOG_INFO("Service initialized successfully");
    return 0;
}

// ==================== 主循环 ====================

void run_server(void) {
    agentos_socket_t server_fd = create_socket(g_config->socket_path);
    
    while (g_running) {
        agentos_socket_t client_fd = accept_connection(server_fd, 5000);
        if (client_fd == INVALID_SOCKET) continue;
        
        // 读取请求
        char buffer[MAX_BUFFER];
        ssize_t n = recv_safe(client_fd, buffer, sizeof(buffer)-1);
        if (n <= 0) {
            close_socket(client_fd);
            continue;
        }
        buffer[n] = '\0';
        
        // 解析并分发
        cJSON* req = cJSON_Parse(buffer);
        if (req) {
            // 可选：添加输入验证
            validation_result_t* v = validator_create();
            validation_rule_t rule = { .type = VALIDATE_REQUIRED, .field_name = "method" };
            validator_add_rule(v, &rule);
            
            v = validator_validate(v, req);
            if (v->valid) {
                method_dispatcher_dispatch(g_disp, req, 
                                          jsonrpc_build_error, 
                                          (void*)(intptr_t)client_fd);
            } else {
                char* err = jsonrpc_build_error(JSONRPC_INVALID_REQUEST,
                                                v->error_message, 0);
                send_response(client_fd, err);
                free(err);
            }
            
            validator_destroy(v);
            cJSON_Delete(req);
        } else {
            char* err = jsonrpc_build_error(JSONRPC_PARSE_ERROR,
                                           "Invalid JSON", 0);
            send_response(client_fd, err);
            free(err);
        }
        
        close_socket(client_fd);
    }
    
    close_socket(server_fd);
}

// ==================== 清理 ====================

void service_cleanup(void) {
    if (g_disp) method_dispatcher_destroy(g_disp);
    if (g_config) free_config(g_config);
    SVC_LOG_INFO("Service cleanup completed");
}
```

---

## 性能指标

### 时间复杂度

| 操作 | 复杂度 | 说明 |
|------|--------|------|
| `jsonrpc_parse_request` | O(n) | n=JSON大小 |
| `method_dispatcher_dispatch` | O(m) | m=已注册方法数 |
| `validator_validate` | O(r) | r=规则数 |
| `agent_registry_search` | O(n*m) | n=条目数,m=字段数 |

### 内存占用

| 组件 | 基础内存 | 每实例额外 |
|------|---------|-----------|
| method_dispatcher | ~200B | 48B/handler |
| input_validator | ~512B | 24B/rule |
| agent_registry | ~8KB | ~500B/entry |

### 测试覆盖率

| 模块 | 测试用例数 | 行覆盖率 | 目标 |
|------|-----------|---------|------|
| jsonrpc_helpers | 13 | ~85% | >90% |
| agent_registry_core | 10 | ~80% | >90% |
| safe_string_utils | 8 | ~95% | >90% |
| input_validator | 6 | ~85% | >90% |
| **总计** | **37** | **~86%** | **>90%** |

---

## 版本历史

| 版本 | 日期 | 变更 |
|------|------|------|
| 2.0.0 | 2026-04-04 | 新增：safe_string_utils、input_validator、method_dispatcher |
| 1.0.0 | 2026-04-04 | 初始版本：jsonrpc_helpers |

---

## 许可证

© 2026 SPHARX Ltd. All Rights Reserved.

*"From data intelligence emerges."*
