# AgentOS 公共工具库 (Common Utils)

**版本**: v1.0.0.6  
**最后更新**: 2026-03-25  
**许可证**: Apache License 2.0

---

## 🎯 模块定位

`common/utils/` 提供整个 AgentOS 项目共享的**通用工具和基础设施**，按功能领域划分为多个子模块：

- **config/**: 配置管理（加载、验证、热更新）
- **core/**: 核心数据类型和基础工具
- **cost/**: 性能分析和耗时统计
- **error/**: 统一错误处理
- **io/**: IO 操作封装
- **observability/**: 可观测性（日志、指标、追踪）
- **resource/**: 资源管理
- **security/**: 安全工具（加密、认证）
- **token/**: Token 管理和限流
- **types/**: 通用类型定义

**重要**: 本模块是 C 语言实现，所有上层模块（atoms, backs, domes 等）都依赖于此。

---

## 📁 目录结构

```
common/utils/
├── config/                 # 配置管理
│   ├── config_loader.h     # 配置加载器
│   └── config_loader.c
├── core/                   # 核心工具
│   ├── macros.h            # 宏定义
│   └── utils.h             # 通用工具函数
├── cost/                   # 性能分析
│   ├── profiler.h          # 性能分析器
│   └── timer.h             # 高精度计时器
├── error/                  # 错误处理
│   ├── error_code.h        # 错误码定义
│   └── error_handler.h     # 错误处理器
├── io/                     # IO 封装
│   ├── file_utils.h        # 文件操作
│   └── socket_utils.h      # Socket 操作
├── observability/          # 可观测性
│   ├── logger.h            # 日志系统
│   ├── metrics.h           # 指标采集
│   └── tracer.h            # 分布式追踪
├── resource/               # 资源管理
│   ├── handle.h            # 句柄管理
│   └── pool.h              # 资源池
├── security/               # 安全工具
│   ├── crypto.h            # 加密解密
│   └── auth.h              # 认证授权
├── token/                  # Token 管理
│   ├── token_bucket.h      # 令牌桶限流
│   └── rate_limiter.h      # 限流器
└── types/                  # 类型定义
    └── basic_types.h       # 基础类型
```

---

## 🔧 核心功能详解

### 1. 配置管理 (`config/`)

支持 YAML 格式配置的加载、验证和热更新。

#### 使用示例

```c
#include <config_loader.h>

// 加载配置文件
config_t* cfg = config_load("./config/kernel.yaml");
if (!cfg) {
    FATAL("Failed to load config");
}

// 读取基本类型
const char* log_level = config_get_string(cfg, "logging.level");
int timeout = config_get_int(cfg, "network.timeout", 30);  // 默认值 30
bool debug = config_get_bool(cfg, "debug.enabled", false);

// 读取数组
config_array_t* servers = config_get_array(cfg, "cluster.servers");
for (size_t i = 0; i < servers->size; i++) {
    printf("Server %zu: %s\n", i, servers->items[i]);
}

// 配置热更新监听
config_watch(cfg, "llm.model", on_model_changed, NULL);

// 释放配置
config_free(cfg);
```

#### 配置文件示例

```yaml
# config/kernel.yaml
logging:
  level: INFO
  format: json
  
network:
  timeout: 30
  retry_count: 3
  
cluster:
  servers:
    - server1:8080
    - server2:8080
    - server3:8080
    
llm:
  model: gpt-4
  temperature: 0.7
  max_tokens: 2048
```

### 2. 可观测性 (`observability/`)

三位一体的可观测性：日志 + 指标 + 追踪。

#### 日志系统

```c
#include <logger.h>

// 设置日志级别
logger_set_level(LOG_LEVEL_INFO);

// 记录日志（自动包含时间戳、文件名、行号）
LOG_DEBUG("Debug info: %d", value);
LOG_INFO("Service started on port %d", port);
LOG_WARNING("High memory usage: %.2f%%", memory_percent);
LOG_ERROR("Failed to connect: %s", error_msg);
LOG_FATAL("Critical error, exiting...");

// 结构化日志（JSON 格式）
LOG_JSON(INFO, 
    "{\"event\": \"request\", \"method\": \"%s\", \"latency_ms\": %d}",
    "GET", 150
);

// 带 Trace ID 的日志（用于分布式追踪）
log_set_trace_id("trace-abc123");
LOG_INFO("Processing request");  // 自动附加 trace_id
```

#### 指标采集

```c
#include <metrics.h>

// 创建计数器
metrics_counter_t* requests = metrics_counter_create(
    "http_requests_total",
    "Total HTTP requests"
);

// 增加计数
metrics_counter_inc(requests, 1);

// 创建直方图
metrics_histogram_t* latency = metrics_histogram_create(
    "http_request_duration_seconds",
    "Request duration in seconds",
    (double[]){0.01, 0.05, 0.1, 0.5, 1.0, 5.0},  // buckets
    6
);

// 观察值
metrics_histogram_observe(latency, 0.234);

// 导出指标（Prometheus 格式）
char* output = metrics_export_all();
printf("%s\n", output);
// # HELP http_requests_total Total HTTP requests
// # TYPE http_requests_total counter
// http_requests_total{method="GET"} 1234
```

#### 分布式追踪

```c
#include <tracer.h>

// 开始一个 Span
span_t* span = tracer_start_span("process_request");
tracer_set_attribute(span, "user_id", "user-123");
tracer_set_attribute(span, "method", "POST");

// 创建子 Span（嵌套调用）
span_t* child = tracer_start_child_span(span, "db_query");
// ... 数据库操作 ...
tracer_end_span(child);

// 结束 Span
tracer_end_span(span);

// 获取 Trace ID 用于日志关联
const char* trace_id = tracer_get_trace_id(span);
```

### 3. 错误处理 (`error/`)

统一的错误码体系和错误处理机制。

```c
#include <error_handler.h>

// 错误码定义（在 error_code.h 中）
typedef enum {
    OK = 0,
    ERR_INVALID_PARAM = -1,
    ERR_OUT_OF_MEMORY = -2,
    ERR_TIMEOUT = -3,
    ERR_NETWORK = -4,
    // ... 更多错误码
} status_t;

// 返回错误
status_t func() {
    if (invalid_param) {
        return ERR_INVALID_PARAM;
    }
    return OK;
}

// 错误处理宏
status_t result = func();
if (result != OK) {
    LOG_ERROR("Function failed: %s", error_string(result));
    return result;
}

// 断言（调试模式有效）
ASSERT(ptr != NULL);
ASSERT(value > 0);

// 确保清理（类似 defer）
DEFER(free(ptr));
void* ptr = malloc(1024);
```

### 4. 性能分析 (`cost/`)

高精度的性能分析和耗时统计。

```c
#include <profiler.h>

// 方法级性能分析
PROFILE_FUNC();  // 自动记录函数执行时间

void slow_function() {
    PROFILE_FUNC();
    // ... 耗时操作 ...
}

// 代码块性能分析
{
    PROFILE_BLOCK("database_query");
    // ... 数据库查询 ...
}

// 手动计时
profiler_t* prof = profiler_create("custom_operation");
profiler_start(prof);

// ... 执行操作 ...

profiler_stop(prof);
printf("Elapsed: %.3f ms\n", profiler_elapsed_ms(prof));

// 统计信息
profiler_stats_t stats;
profiler_get_stats(prof, &stats);
printf("Avg: %.3f ms, P99: %.3f ms\n", 
    stats.avg_ms, stats.p99_ms);
```

### 5. Token 限流 (`token/`)

令牌桶算法实现的限流器。

```c
#include <token_bucket.h>

// 创建令牌桶
token_bucket_t* bucket = token_bucket_create(
    100,    // 桶容量（最大令牌数）
    10      // 填充速率（每秒 10 个令牌）
);

// 请求令牌（阻塞式）
token_bucket_acquire(bucket, 5);  // 获取 5 个令牌

// 尝试请求（非阻塞）
if (token_bucket_try_acquire(bucket, 1)) {
    // 有足够令牌，执行操作
    process_request();
} else {
    // 限流，拒绝请求
    LOG_WARNING("Rate limit exceeded");
}

// 销毁令牌桶
token_bucket_destroy(bucket);
```

### 6. 安全工具 (`security/`)

常用的加密和认证工具。

```c
#include <crypto.h>

// === 哈希 ===
// SHA256
uint8_t hash[SHA256_DIGEST_LENGTH];
sha256("hello world", 11, hash);

// HMAC-SHA256
hmac_sha256("key", 3, "data", 4, hash);

// Base64 编解码
char* encoded = base64_encode(data, data_len);
uint8_t* decoded = base64_decode(encoded, &decoded_len);

// === AES 加密 ===
aes_ctx_t* ctx = aes_init(key, key_len);
uint8_t* ciphertext = aes_encrypt(ctx, plaintext, plaintext_len, &ciphertext_len);
uint8_t* plaintext = aes_decrypt(ctx, ciphertext, ciphertext_len, &plaintext_len);
aes_free(ctx);

// === JWT ===
#include <auth.h>

// 生成 JWT
char* token = jwt_generate(
    "{\"sub\": \"user123\", \"role\": \"admin\"}",
    secret_key
);

// 验证 JWT
jwt_payload_t* payload = jwt_verify(token, secret_key);
if (payload) {
    printf("User: %s, Role: %s\n", 
        payload->sub, payload->role);
}
```

---

## 🚀 快速开始

### 编译

```bash
cd AgentOS/common
mkdir build && cd build
cmake ..
make                    # 编译所有
make utils-observability # 只编译可观测性模块
```

### 测试

```bash
cd build
ctest                   # 运行所有测试
ctest -R logger         # 只测日志模块
ctest -R config         # 只测配置模块
```

### 集成到你的项目

```cmake
# CMakeLists.txt
add_subdirectory(../common/utils utils_build)
target_link_libraries(your_app PRIVATE utils)
```

---

## 📊 性能指标

| 模块 | 操作 | 延迟 | 吞吐量 |
|------|------|------|--------|
| 配置加载 | 完整加载 | < 10ms | - |
| 日志系统 | 单条写入 | < 1μs | 100,000+/s |
| 指标采集 | 单次记录 | < 100ns | 1,000,000+/s |
| 性能分析 | 启动/停止 | < 50ns | - |
| Token 限流 | 获取令牌 | < 1μs | - |
| SHA256 | 1KB 数据 | < 5μs | - |
| AES 加密 | 1KB 数据 | < 10μs | - |

---

## 🛠️ 最佳实践

### 1. 日志使用规范

```c
// ✅ 推荐：使用宏记录日志
LOG_INFO("User %s logged in", username);

// ❌ 不推荐：直接调用底层函数
logger_log(LOG_LEVEL_INFO, "User %s logged in", username);

// ✅ 推荐：关键路径使用结构化日志
LOG_JSON(INFO, 
    "{\"event\": \"login\", \"user_id\": \"%s\"}",
    user_id
);
```

### 2. 错误处理规范

```c
// ✅ 推荐：检查所有返回值
status_t ret = some_function();
if (ret != OK) {
    LOG_ERROR("Failed: %s", error_string(ret));
    return ret;
}

// ❌ 不推荐：忽略返回值
some_function();  // 可能失败！
```

### 3. 性能分析规范

```c
// ✅ 推荐：在关键函数添加性能分析
void critical_function() {
    PROFILE_FUNC();
    // ...
}

// ✅ 推荐：对热点代码块进行分析
for (int i = 0; i < count; i++) {
    PROFILE_BLOCK("loop_iteration");
    // ...
}
```

---

## 🔗 相关文档

- [配置模块详细指南](../../config/README.md)
- [监控服务](../backs/monit_d/README.md)
- [Domes 审计模块](../domes/README.md)
- [项目根目录](../../README.md)

---

## 🤝 贡献指南

欢迎提交 Issue 或 Pull Request！

## 📞 联系方式

- **维护者**: AgentOS 架构委员会
- **技术支持**: lidecheng@spharx.cn
- **问题反馈**: https://github.com/SpharxTeam/AgentOS/issues

---

© 2026 SPHARX Ltd. All Rights Reserved.

*"Utilities for everything. 万物之利器。"*
