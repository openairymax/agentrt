# AgentOS 公共基础库 (commons)

**版本**: v1.0.0.6  
**最后更新**: 2026-03-29  
**许可证**: Apache License 2.0

---

## 🎯 模块定位

`commons/` 是 AgentOS 的**统一公共基础库**，提供跨模块共享的通用工具和基础设施能力。作为整个系统的底层依赖，commons 为 atoms（内核层）、daemon（服务层）、cupolas（安全层）等所有上层模块提供标准化的基础服务。

**核心职责**:
- **平台抽象**: 屏蔽 Linux、macOS、Windows 三平台差异，提供统一的系统 API
- **通用工具集**: 15+ 功能模块覆盖日志、配置、错误处理、内存管理等常用需求
- **基础设施**: 提供线程同步、网络通信、文件操作等底层能力
- **零循环依赖**: 不依赖任何上层模块，仅使用标准库和第三方库

---

## 📁 目录结构

```
commons/
├── README.md                 # 本文档
├── CMakeLists.txt            # 构建配置
├── test_unified_modules.c    # 统一模块集成测试
│
├── platform/                 # 平台抽象层
│   ├── include/
│   │   └── platform.h        # 跨平台 API 统一接口
│   └── platform.c            # 平台适配实现
│
└── utils/                    # 通用工具集 (15 个子模块)
    │
    ├── cache/                # 缓存工具
    │   ├── include/cache_common.h
    │   └── cache_common.c
    │
    ├── cognition/            # 认知工具
    │   ├── include/cognition_common.h
    │   └── cognition_common.c
    │
    ├── config/               # 简单配置管理
    │   ├── include/config.h
    │   └── config.c
    │
    ├── config_unified/       # 统一配置框架
    │   ├── include/
    │   │   ├── config_unified.h
    │   │   ├── core_config.h
    │   │   ├── config_source.h
    │   │   ├── config_service.h
    │   │   └── config_compat.h
    │   ├── src/
    │   │   ├── core_config.c
    │   │   ├── config_source.c
    │   │   ├── config_service.c
    │   │   └── config_compat.c
    │   ├── test/
    │   │   └── test_config_unified.c
    │   └── README.md
    │
    ├── cost/                 # 成本估算与控制
    │   ├── include/cost.h
    │   ├── estimator.c       # 成本估算器
    │   └── controller.c      # 成本控制器
    │
    ├── error/                # 错误处理框架
    │   ├── include/
    │   │   ├── error.h           # 统一错误码定义
    │   │   └── error_compat.h    # 兼容接口
    │   └── handler.c         # 错误处理器
    │
    ├── execution/            # 执行工具
    │   ├── include/execution_common.h
    │   └── execution_common.c
    │
    ├── io/                   # IO 操作工具
    │   ├── include/io.h
    │   └── file_utils.c      # 文件工具函数
    │
    ├── logging/              # 分层日志系统
    │   ├── include/
    │   │   ├── logging.h         # 核心层 API
    │   │   ├── logging_common.h  # 通用定义
    │   │   ├── logging_compat.h  # 兼容接口
    │   │   ├── atomic_logging.h  # 原子层 API
    │   │   └── service_logging.h # 服务层 API
    │   ├── src/
    │   │   ├── logging.c         # 核心层实现
    │   │   ├── logging_common.c  # 通用实现
    │   │   ├── logging_compat.c  # 兼容实现
    │   │   ├── atomic_logging.c  # 原子层实现
    │   │   └── service_logging.c # 服务层实现
    │   ├── test/
    │   │   └── test_logging_unified.c
    │   ├── bench_atomic_logging.c  # 性能基准测试
    │   └── README.md
    │
    ├── memory/               # 内存管理
    │   ├── include/
    │   │   ├── memory.h          # 内存分配接口
    │   │   ├── memory_common.h   # 通用定义
    │   │   ├── memory_compat.h   # 兼容接口
    │   │   ├── memory_pool.h     # 内存池接口
    │   │   └── memory_debug.h    # 调试工具
    │   ├── src/
    │   │   ├── memory.c          # 内存分配实现
    │   │   ├── memory_common.c   # 通用实现
    │   │   ├── memory_pool.c     # 内存池实现
    │   │   └── memory_debug.c    # 调试实现
    │   ├── test/
    │   │   └── test_memory.c
    │   └── memory_common.c
    │
    ├── network/              # 网络工具
    │   ├── include/network_common.h
    │   └── network_common.c
    │
    ├── observability/        # 可观测性
    │   ├── include/
    │   │   ├── observability.h   # 统一可观测性接口
    │   │   ├── metrics.h         # 指标采集
    │   │   ├── trace.h           # 链路追踪
    │   │   └── logger.h          # 日志桥接
    │   ├── logger.c
    │   ├── metrics.c
    │   └── trace.c
    │
    ├── platform/             # 平台适配器
    │   ├── include/platform_adapter.h
    │   └── platform_adapter.c
    │
    ├── resource/             # 资源管理
    │   ├── resource_guard.h
    │   └── resource_guard.c
    │
    ├── security/             # 安全工具
    │   ├── input_validator.h   # 输入验证器
    │   └── input_validator.c
    │
    ├── strategy/             # 策略接口
    │   ├── include/strategy_common.h
    │   └── strategy_common.c
    │
    ├── string/               # 字符串处理
    │   ├── include/
    │   │   ├── string.h
    │   │   ├── string_common.h
    │   │   └── string_compat.h
    │   ├── src/
    │   │   └── string.c
    │   ├── string_common.c
    │   └── test/
    │       └── test_string.c
    │
    ├── sync/                 # 同步原语
    │   ├── include/
    │   │   ├── sync.h
    │   │   ├── sync_common.h
    │   │   └── sync_compat.h
    │   ├── src/
    │   │   └── sync.c
    │   ├── sync_common.c
    │   └── test/
    │       └── test_sync.c
    │
    ├── token/                # Token 管理
    │   ├── include/token.h
    │   ├── counter.c         # Token 计数器
    │   └── budget.c          # Token 预算管理
    │
    └── types/                # 类型定义
        └── include/
            └── types.h       # 通用类型定义
```

---

## 🔧 核心功能详解

### 1. 平台抽象层 (platform/)

**功能**: 屏蔽 Linux、macOS、Windows 三平台差异，提供统一的系统 API

**核心接口**:
```c
#include <platform.h>

// 线程管理
agentos_thread_t thread;
agentos_thread_create(&thread, my_function, arg);
agentos_thread_join(thread, NULL);

// 同步原语
agentos_mutex_t mutex;
agentos_mutex_init(&mutex);
agentos_mutex_lock(&mutex);
agentos_mutex_unlock(&mutex);
agentos_mutex_destroy(&mutex);

// 网络通信
agentos_socket_t sock = agentos_socket_tcp();
agentos_socket_set_nonblock(sock, 1);

// 进程管理
agentos_process_info_t proc;
agentos_process_start("/bin/myapp", argv, envp, &proc);
agentos_process_wait(&proc, 5000, &exit_code);

// 高精度时间
uint64_t ns = agentos_time_ns();  // 纳秒级时间戳
uint64_t ms = agentos_time_ms();  // 毫秒级时间戳
```

**支持平台**:
| 平台 | 架构 | 状态 |
|------|------|------|
| Windows | x64/x86 | ✅ 完整支持 |
| Linux | x64/ARM64 | ✅ 完整支持 |
| macOS | x64/ARM64 | ✅ 完整支持 |

### 2. 错误处理框架 (error/)

**功能**: 提供统一的错误码定义、错误链追踪、多语言支持

**核心特性**:
- 分段错误码管理（避免模块冲突）
- 错误链追踪（支持 16 层深度）
- 线程安全的错误上下文存储
- 多语言本地化支持（8 种语言）
- JSON 格式错误报告

**使用示例**:
```c
#include <error.h>

agentos_error_t my_function(int param) {
    if (param < 0) {
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "Parameter must be positive");
    }
    
    void* ptr = malloc(size);
    if (!ptr) {
        AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY, "Allocation failed");
    }
    
    // 正常逻辑
    return AGENTOS_OK;
}

// 调用处
agentos_error_t err = my_function(42);
if (err != AGENTOS_OK) {
    agentos_error_chain_t* chain = agentos_error_get_chain();
    char* json = agentos_error_chain_to_json(chain);
    LOG_ERROR("Error: %s", json);
    free(json);
}
```

**错误码分段**:
| 范围 | 模块 | 示例 |
|------|------|------|
| -1 ~ -99 | 通用基础错误 | `AGENTOS_ERR_INVALID_PARAM` |
| -100 ~ -199 | 系统与平台错误 | `AGENTOS_ERR_SYS_NOT_INIT` |
| -200 ~ -299 | 内核层错误 | `AGENTOS_ERR_KERN_IPC` |
| -300 ~ -399 | 服务层错误 | `AGENTOS_ERR_SVC_CONFIG` |
| -400 ~ -499 | LLM/AI 服务错误 | `AGENTOS_ERR_LLM_RATE_LIMIT` |
| -500 ~ -599 | 执行/工具错误 | `AGENTOS_ERR_EXEC_SANDBOX` |
| -600 ~ -699 | 记忆/存储错误 | `AGENTOS_ERR_MEM_WRITE` |
| -700 ~ -799 | 安全/沙箱错误 | `AGENTOS_ERR_SEC_VIOLATION` |
| -800 ~ -899 | 协调/规划错误 | `AGENTOS_ERR_COORD_PLAN_FAIL` |

### 3. 分层日志系统 (logging/)

**功能**: 提供高性能、结构化、分层设计的日志系统

**架构层次**:
```
┌─────────────────────────────────────┐
│   服务层 (service_logging.h)        │  ← 高级功能：轮转、过滤、传输
├─────────────────────────────────────┤
│   原子层 (atomic_logging.h)         │  ← 高性能：无锁队列、批量写入
├─────────────────────────────────────┤
│   核心层 (logging.h)                │  ← 基础接口：日志写入、配置
└─────────────────────────────────────┘
```

**快速开始**:
```c
#include <logging.h>

// 初始化
log_config_t config = {
    .level = LOG_LEVEL_INFO,
    .outputs = LOG_OUTPUT_CONSOLE | LOG_OUTPUT_FILE,
    .format = LOG_FORMAT_JSON,
    .file_path = "/var/log/agentos/app.log",
    .max_file_size = 100 * 1024 * 1024,  // 100MB
    .max_backup_count = 5
};
log_init(&config);

// 记录日志
LOG_INFO("系统启动成功");
LOG_ERROR("连接失败，错误码：%d", errno);

// 设置追踪 ID（用于分布式追踪）
log_set_trace_id("req-123456");
LOG_DEBUG("处理请求");

// 清理
log_cleanup();
```

**日志级别**:
| 级别 | 宏 | 使用场景 |
|------|-----|---------|
| DEBUG | `LOG_DEBUG()` | 开发调试信息 |
| INFO | `LOG_INFO()` | 正常运行状态 |
| WARN | `LOG_WARN()` | 警告但不影响运行 |
| ERROR | `LOG_ERROR()` | 功能错误 |
| FATAL | `LOG_FATAL()` | 致命错误（自动终止） |

**输出格式**:
- **TEXT**: 人类可读的文本格式
- **JSON**: 结构化 JSON 格式（推荐生产环境）
- **STRUCTURED**: 键值对格式
- **BINARY**: 二进制格式（高性能场景）

### 4. 统一配置框架 (config_unified/)

**功能**: 提供多层级、多源、热重载的配置管理能力

**架构设计**:
```
┌──────────────────────────────────────┐
│   config_service.h                   │  ← 服务层：远程配置、配置中心
├──────────────────────────────────────┤
│   config_source.h                    │  ← 源抽象：文件、环境变量、远程
├──────────────────────────────────────┤
│   core_config.h                      │  ← 核心层：配置解析、验证
├──────────────────────────────────────┤
│   config_compat.h                    │  ← 兼容层：旧接口兼容
└──────────────────────────────────────┘
```

**使用示例**:
```c
#include <config_unified.h>

// 加载配置
core_config_t* config = core_config_load("/etc/agentos/config.yaml");

// 读取基本类型
const char* host = core_config_get_string(config, "server.host", "localhost");
int port = core_config_get_int(config, "server.port", 8080);
bool enabled = core_config_get_bool(config, "feature.enabled", true);

// 读取嵌套配置
const char* db_host = core_config_get_string(config, "database.connection.host", "127.0.0.1");

// 热重载
core_config_reload(config);

// 释放
core_config_free(config);
```

**支持的配置源**:
- YAML 文件
- JSON 文件
- 环境变量
- 命令行参数
- 远程配置中心（HTTP/gRPC）

### 5. 内存管理 (memory/)

**功能**: 提供多种内存分配策略和调试工具

**分配器类型**:
```c
#include <memory.h>

// 1. 标准分配器（带调试信息）
void* ptr = memory_alloc(1024);
memory_free(ptr);

// 2. 内存池（高性能场景）
memory_pool_t* pool = memory_pool_create(1024, 100, GFP_ZERO);
void* obj = memory_pool_alloc(pool, sizeof(my_object_t));
memory_pool_free(pool, obj);
memory_pool_destroy(pool);

// 3. 竞技场分配器（批量分配场景）
arena_t* arena = arena_create(1024 * 1024);  // 1MB
void* buf1 = arena_alloc(arena, 1024);
void* buf2 = arena_alloc(arena, 2048);
arena_reset(arena);   // 重置（不释放）
arena_destroy(arena); // 释放所有
```

**调试功能**:
- 内存泄漏检测
- 越界访问检测
- 使用已释放内存检测
- 内存使用统计

### 6. 可观测性 (observability/)

**功能**: 集成 OpenTelemetry 标准的指标、追踪、日志三支柱

**指标采集**:
```c
#include <observability/metrics.h>

// 注册指标
metric_t* requests = metric_register(
    "requests_total",
    "Total number of requests",
    METRIC_TYPE_COUNTER
);

metric_t* duration = metric_register(
    "request_duration_seconds",
    "Request duration in seconds",
    METRIC_TYPE_HISTOGRAM
);

// 记录数据
metric_inc(requests, NULL);
metric_observe(duration, 0.234, NULL);

// 导出到 Prometheus
prometheus_exporter_start(9090);
```

**链路追踪**:
```c
#include <observability/trace.h>

// 创建 Span
span_t* span = tracer_start_span("process_request");
tracer_set_attribute(span, "user_id", "12345");
tracer_set_attribute(span, "method", "POST");

// 业务逻辑
process_request();

// 结束 Span
tracer_end_span(span);
```

### 7. Token 管理 (token/)

**功能**: LLM Token 计数、预算管理、成本控制

**使用示例**:
```c
#include <token.h>

// Token 计数
size_t count = token_counter_count("Hello, world!", "cl100k_base");

// 预算管理
token_budget_t* budget = token_budget_create(100000);  // 10 万 Token
token_budget_consume(budget, count);
if (token_budget_exceeded(budget)) {
    LOG_WARN("Token budget exceeded");
}

// 成本估算
double cost = cost_estimator_estimate(count, "gpt-4");
```

### 8. 同步原语 (sync/)

**功能**: 跨平台线程同步工具

**支持的同步原语**:
```c
#include <sync.h>

// 互斥锁
sync_mutex_t mutex;
sync_mutex_init(&mutex);
sync_mutex_lock(&mutex);
sync_mutex_unlock(&mutex);
sync_mutex_destroy(&mutex);

// 读写锁
sync_rwlock_t rwlock;
sync_rwlock_init(&rwlock);
sync_rwlock_rdlock(&rwlock);
sync_rwlock_wrlock(&rwlock);
sync_rwlock_unlock(&rwlock);
sync_rwlock_destroy(&rwlock);

// 条件变量
sync_cond_t cond;
sync_cond_init(&cond);
sync_cond_wait(&cond, &mutex);
sync_cond_signal(&cond);
sync_cond_destroy(&cond);

// 信号量
sync_sem_t sem;
sync_sem_init(&sem, 10);  // 初始值 10
sync_sem_wait(&sem);
sync_sem_post(&sem);
sync_sem_destroy(&sem);
```

---

## 🚀 构建指南

### 环境要求

| 依赖 | 最低版本 | 推荐版本 | 必需 |
|------|---------|---------|------|
| **CMake** | 3.20 | 3.25+ | ✅ |
| **GCC/Clang** | GCC 11 / Clang 14 | GCC 12 / Clang 15 | ✅ |
| **libyaml** | 0.2.5 | 0.2.5+ | ✅ |
| **tiktoken** | 0.4.0 | 0.5.0+ | ✅ |
| **libcjson** | 1.7.15 | 1.7.16+ | ✅ |
| **OpenSSL** | 1.1.1 | 3.0+ | ❌ 可选 |

### 编译步骤

```bash
# 1. 进入项目目录
cd commons

# 2. 创建构建目录
mkdir build && cd build

# 3. 配置 CMake
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON \
  -DCMAKE_INSTALL_PREFIX=/opt/agentos

# 4. 编译
cmake --build . --parallel $(nproc)

# 5. 产物
# - libagentos_common.a  (静态库)
# - include/agentos/     (头文件)

# 6. 安装（可选）
sudo cmake --install .
```

### CMake 选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `CMAKE_BUILD_TYPE` | Release | Debug/Release/RelWithDebInfo |
| `BUILD_SHARED_LIBS` | OFF | 构建动态库 |
| `BUILD_TESTS` | OFF | 构建测试套件 |
| `ENABLE_CRYPTO` | ON | 启用加密功能 |
| `CMAKE_INSTALL_PREFIX` | /usr/local | 安装路径 |

---

## 💻 使用示例

### 完整应用示例

```c
#include <agentos/commons.h>

int main(int argc, char* argv[]) {
    // 1. 初始化平台层
    agentos_network_init();
    
    // 2. 初始化日志系统
    log_config_t log_config = {
        .level = LOG_LEVEL_INFO,
        .format = LOG_FORMAT_JSON,
        .output = LOG_OUTPUT_CONSOLE
    };
    log_init(&log_config);
    
    LOG_INFO("AgentOS 应用启动...");
    
    // 3. 加载配置
    core_config_t* config = core_config_load("/etc/agentos/config.yaml");
    if (!config) {
        LOG_ERROR("配置加载失败");
        return 1;
    }
    
    const char* server_host = core_config_get_string(config, "server.host", "0.0.0.0");
    int server_port = core_config_get_int(config, "server.port", 8080);
    
    LOG_INFO("服务器监听：%s:%d", server_host, server_port);
    
    // 4. 初始化错误处理
    agentos_error_set_language(AGENTOS_LANG_ZH_CN);
    
    // 5. 创建内存池
    memory_pool_t* pool = memory_pool_create(1024, 100, GFP_ZERO);
    
    // 6. 分配对象
    my_object_t* obj = memory_pool_alloc(pool, sizeof(my_object_t));
    obj->id = 123;
    obj->name = memory_pool_alloc(pool, strlen("Test Object") + 1);
    strcpy(obj->name, "Test Object");
    
    // 7. 设置追踪 ID
    log_set_trace_id("req-123456");
    LOG_INFO("开始处理请求");
    
    // 8. 业务逻辑（带错误处理）
    agentos_error_t err = process_data(obj);
    if (err != AGENTOS_OK) {
        agentos_error_chain_t* chain = agentos_error_get_chain();
        char* json = agentos_error_chain_to_json(chain);
        LOG_ERROR("处理失败：%s", json);
        free(json);
    }
    
    // 9. 清理资源
    core_config_free(config);
    memory_pool_destroy(pool);
    log_cleanup();
    agentos_network_cleanup();
    
    return 0;
}
```

### 编译链接

```bash
# 编译
gcc -I/opt/agentos/include \
    -o myapp myapp.c \
    -L/opt/agentos/lib -lagentos_common \
    -lyaml -lcjson -ltiktoken -lpthread -lm

# 运行
./myapp
```

### CMake 集成

```cmake
find_package(agentos_common REQUIRED)

target_link_libraries(myapp
    agentos_common
    ${AGENTOS_COMMON_LIBRARIES}
)

target_include_directories(myapp
    ${AGENTOS_COMMON_INCLUDE_DIRS}
)
```

---

## 📊 API 参考

### 核心函数速查

| 模块 | 函数 | 说明 | 示例 |
|------|------|------|------|
| **平台** | `agentos_thread_create()` | 创建线程 | `agentos_thread_create(&t, fn, arg)` |
| **平台** | `agentos_mutex_lock()` | 互斥锁加锁 | `agentos_mutex_lock(&mutex)` |
| **错误** | `AGENTOS_ERROR()` | 设置错误并返回 | `AGENTOS_ERROR(code, msg)` |
| **错误** | `agentos_error_str()` | 获取错误描述 | `agentos_error_str(err)` |
| **日志** | `LOG_INFO()` | 记录 INFO 日志 | `LOG_INFO("Message: %s", msg)` |
| **日志** | `log_set_trace_id()` | 设置追踪 ID | `log_set_trace_id("req-123")` |
| **配置** | `core_config_load()` | 加载配置 | `config = core_config_load(path)` |
| **配置** | `core_config_get_string()` | 获取字符串配置 | `host = core_config_get_string(cfg, "host")` |
| **内存** | `memory_pool_create()` | 创建内存池 | `pool = memory_pool_create(size, count, flags)` |
| **内存** | `memory_pool_alloc()` | 从池分配 | `obj = memory_pool_alloc(pool, size)` |
| **Token** | `token_counter_count()` | 计算 Token 数 | `count = token_counter_count(text, model)` |
| **同步** | `sync_cond_wait()` | 条件变量等待 | `sync_cond_wait(&cond, &mutex)` |

---

## 🧪 测试

### 运行测试

```bash
# 进入构建目录
cd commons/build

# 运行所有测试
ctest --output-on-failure

# 运行特定模块测试
ctest -R test_logging --verbose
ctest -R test_memory --verbose
ctest -R test_config --verbose
ctest -R test_error --verbose
ctest -R test_sync --verbose

# 运行集成测试
ctest -R test_common_integration --verbose
```

### 代码覆盖率

```bash
# 启用覆盖率编译
cmake .. -DCMAKE_BUILD_TYPE=Debug -DCODE_COVERAGE=ON

# 构建并运行测试
cmake --build .
ctest

# 生成覆盖率报告
make coverage

# 查看报告（HTML 格式）
firefox coverage/index.html
```

### 性能基准测试

```bash
# 编译基准测试
cmake --build . --target bench_atomic_logging

# 运行基准测试
./bench_atomic_logging

# 输出示例：
# Benchmark: atomic_logging_write
# Iterations: 1000000
# Avg Latency: 45.2 ns
# Throughput: 22.1 M ops/sec
```

---

## 📋 最佳实践

### 1. 错误处理

```c
// ✅ 推荐：使用错误链追踪
agentos_error_t func() {
    if (error) {
        AGENTOS_ERROR(AGENTOS_ERR_UNKNOWN, "Detailed message");
    }
    return AGENTOS_OK;
}

// ❌ 避免：仅返回错误码
agentos_error_t func() {
    if (error) {
        return AGENTOS_ERR_UNKNOWN;  // 缺少上下文
    }
    return AGENTOS_OK;
}
```

### 2. 日志使用

```c
// ✅ 推荐：结构化日志
LOG_INFO("User login", 
    KV("user_id", user_id),
    KV("ip", ip),
    KV("duration_ms", duration));

// ❌ 避免：非结构化长文本
LOG_INFO("User %d logged in from %s, took %d ms", user_id, ip, duration);
```

### 3. 内存管理

```c
// ✅ 推荐：使用内存池（频繁分配场景）
memory_pool_t* pool = memory_pool_create(1024, 100, GFP_ZERO);
for (int i = 0; i < 1000; i++) {
    void* obj = memory_pool_alloc(pool, sizeof(obj_t));
}
memory_pool_destroy(pool);  // 一次性释放

// ❌ 避免：频繁 malloc/free
for (int i = 0; i < 1000; i++) {
    void* obj = malloc(sizeof(obj_t));
    free(obj);  // 性能低下
}
```

### 4. 配置管理

```c
// ✅ 推荐：使用统一配置框架
core_config_t* config = core_config_load("/etc/app/config.yaml");
const char* value = core_config_get_string(config, "key", "default");

// ❌ 避免：手动解析配置文件
FILE* f = fopen("config.yaml", "r");
// 手动解析 YAML...
```

---

## 🔗 相关文档

- [内核层架构](../atoms/README.md) - Atoms 模块文档
- [服务层架构](../daemon/README.md) - Daemon 模块文档
- [安全穹顶](../cupolas/README.md) - Cupolas 模块文档
- [配置管理规范](../manuals/guides/configuration.md) - 配置最佳实践
- [日志规范](../manuals/specifications/logging.md) - 日志编写标准
- [错误处理规范](../manuals/specifications/error_handling.md) - 错误处理指南

---

## 📄 许可证

commons 公共基础库采用 **Apache License 2.0** 开源协议。

详见项目根目录的 [LICENSE](../LICENSE) 文件。

---

<div align="center">

**commons - AgentOS 的通用工具基础设施**

[返回顶部](#agentos-公共基础库 -commons)

© 2026 SPHARX Ltd. All Rights Reserved.

</div>
