# AgentOS heapstore - 数据分区存储系统

**版本**: v1.0.0  
**最后更新**: 2026-03-28  
**开发语言**: C11  
**许可证**: Apache License 2.0

---

## 📖 概述

`heapstore` 是 AgentOS 的**数据分区存储系统**，为整个系统提供统一、可靠、高性能的持久化存储解决方案。它管理所有运行时产生的数据，包括日志、注册表、追踪数据、IPC 通信数据和内存管理数据。

### 核心特性

- 🎯 **生产级可靠性**: 完整的错误处理和资源管理
- 🔒 **线程安全**: 所有操作均经过互斥锁保护
- 📈 **高性能**: 批量写入、异步刷新、最小化锁竞争
- 🔄 **自动管理**: 数据生命周期管理、自动清理、日志轮转
- 📊 **可观测性**: 完整的统计信息和健康检查接口
- 🛡️ **数据安全**: 参数验证、SQL 注入防护、资源隔离

### 在 AgentOS 中的位置

```
AgentOS 架构
├── corekern/          # 纯净微内核 (IPC/Mem/Task/Time)
├── coreloopthree/     # 三层认知运行时
├── memoryrovol/       # 四层记忆系统
├── syscall/           # 系统调用接口
├── cupolas/           # 安全穹顶
└── heapstore/ ⭐      # 数据分区存储（本模块）
```

---

## 🚀 快速开始

### 环境要求

- **编译器**: GCC 5.0+ / Clang 3.5+ / MSVC 2015+
- **C 标准**: C11
- **依赖库**: pthread、SQLite3
- **构建系统**: CMake 3.10+
- **操作系统**: Linux / macOS / Windows

### 构建安装

```bash
# 1. 进入模块目录
cd AgentOS/heapstore

# 2. 创建构建目录
mkdir build && cd build

# 3. 配置 CMake
cmake .. -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release

# 4. 编译
make -j$(nproc)

# 5. 运行测试（可选）
ctest --output-on-failure

# 6. 安装（可选）
sudo make install
```

### 基本使用

```c
#include <heapstore.h>
#include <heapstore_log.h>
#include <stdio.h>

int main(void) {
    // 1. 配置
    heapstore_config_t config = {
        .root_path = "heapstore_data",
        .max_log_size_mb = 100,
        .log_retention_days = 7,
        .enable_auto_cleanup = true,
        .enable_log_rotation = true
    };

    // 2. 初始化
    heapstore_error_t err = heapstore_init(&config);
    if (err != heapstore_SUCCESS) {
        fprintf(stderr, "初始化失败：%s\n", heapstore_strerror(err));
        return 1;
    }

    // 3. 写入日志
    heapstore_LOG_INFO("my_service", "trace_001", "服务启动成功");

    // 4. 健康检查
    bool registry_ok, log_ok;
    heapstore_health_check(&registry_ok, NULL, &log_ok, NULL, NULL);

    // 5. 关闭系统
    heapstore_shutdown();
    return 0;
}
```

### 编译示例

```bash
gcc -o example example.c -lheapstore -lsqlite3 -lpthread
./example
```

---

## 📦 核心功能

### 1. 分层日志系统 📝

提供三级（内核/服务/应用）日志存储，支持 4 级日志（ERROR/WARN/INFO/DEBUG）。

**特性**:
- 按服务分离日志文件
- 支持 Trace ID 追踪
- 自动日志轮转（可配置大小和时间）
- 线程安全的日志写入

**示例**:
```c
heapstore_LOG_INFO("llm_d", "trace_123", "LLM 服务启动成功");
heapstore_LOG_ERROR("tool_d", NULL, "工具执行失败：%s", error_msg);
```

---

### 2. SQLite 注册表 🗄️

使用嵌入式 SQLite 数据库管理 Agent、Skill 和 Session 的注册信息。

**特性**:
- WAL 模式优化写入性能
- 参数化查询防止 SQL 注入
- 自动 Vacuum 优化数据库
- 完整的 CRUD 操作接口

**数据表**:
- `agents` - Agent 注册信息
- `skills` - 技能库注册信息
- `sessions` - 会话状态管理

---

### 3. OpenTelemetry 追踪 📊

存储分布式追踪数据（Span），支持 OpenTelemetry 标准格式。

**特性**:
- Span 批量写入优化
- JSON 格式导出
- 支持 Trace-Span 层级关系
- 自动过期清理

---

### 4. IPC 数据存储 🔗

管理进程间通信（IPC）的通道和缓冲区数据。

**特性**:
- Binder 通道状态记录
- 共享内存缓冲区管理
- 线程安全的读写操作

---

### 5. 内存管理数据 🧠

记录内存池状态、分配历史和统计信息。

**特性**:
- 内存池快照
- 分配历史记录
- 实时统计信息
- 内存泄漏检测支持

---

## 🏗️ 架构设计

### 模块架构

```
┌─────────────────────────────────────────┐
│          heapstore 公共 API              │
│  heapstore_init() | shutdown() | ...    │
└─────────────────────────────────────────┘
                    │
        ┌───────────┴───────────┐
        │                       │
┌───────▼────────┐     ┌────────▼────────┐
│ heapstore_core.c│     │   private.h     │
│ - 目录管理     │     │  - 内部接口     │
│ - 配置管理     │     │  - 模块声明     │
└────────────────┘     └─────────────────┘
        │
        ├──────────┬──────────┬──────────┬──────────┬──────────┐
        │          │          │          │          │          │
┌───────▼──┐  ┌───▼────┐  ┌──▼─────┐  ┌─▼──────┐  ┌─▼──────┐
│heapstore │  │heapstore│  │heapstore│  │heapstore│  │heapstore│
│ _log.c   │  │_regist.│  │ _trace.│  │ _ipc.c │  │_memory.│
│          │  │  .c    │  │  .c    │  │        │  │  .c    │
├──────────┤  ├────────┤  ├────────┤  ├────────┤  ├────────┤
│ 日志管理 │  │注册表  │  │追踪管理│  │IPC 管理 │  │内存管理│
└──────────┘  └────────┘  └────────┘  └────────┘  └────────┘
```

### 线程安全设计

所有子模块都使用独立的互斥锁保护：

```c
pthread_mutex_t g_registry.lock;    // 注册表锁
pthread_mutex_t g_log_lock;         // 日志锁
pthread_mutex_t g_trace_queue.lock; // 追踪锁
pthread_mutex_t g_ipc_data.lock;    // IPC 锁
pthread_mutex_t g_memory_data.lock; // 内存锁
```

---

## 📂 目录结构

### 运行时目录

```
heapstore/                          # 数据分区根目录
├── kernel/                         # 内核数据
│   ├── ipc/                        # IPC 通信数据
│   │   ├── channels/              # Binder 通道状态
│   │   └── buffers/               # 共享内存缓冲区
│   └── memory/                     # 内存管理数据
│       ├── pools/                 # 内存池状态
│       ├── allocations/           # 分配记录
│       └── stats/                 # 统计信息
│
├── logs/                           # 日志文件
│   ├── apps/                      # 应用层日志
│   ├── kernel/                    # 内核层日志
│   └── services/                  # 服务层日志
│
├── registry/                       # 注册表
│   ├── agents.db                  # Agent 注册表 (SQLite)
│   ├── skills.db                  # 技能注册表 (SQLite)
│   └── sessions.db                # 会话注册表 (SQLite)
│
├── services/                       # 服务数据
│   ├── llm_d/                     # LLM 服务数据
│   ├── market_d/                  # 市场服务数据
│   └── tool_d/                    # 工具服务数据
│
└── traces/                         # 追踪数据
    └── spans/                     # Span 数据
        ├── trace_abc123.json
        └── trace_def456.json
```

### 代码结构

```
heapstore/
├── CMakeLists.txt                  # CMake 构建配置
├── README.md                       # 本文档
├── include/                        # 公共头文件
│   ├── heapstore.h                 # 核心接口
│   ├── heapstore_log.h             # 日志接口
│   ├── heapstore_registry.h        # 注册表接口
│   ├── heapstore_trace.h           # 追踪接口
│   ├── heapstore_ipc.h             # IPC 接口
│   └── heapstore_memory.h          # 内存接口
│
├── src/                            # 源代码
│   ├── heapstore_core.c            # 核心实现
│   ├── heapstore_log.c             # 日志实现
│   ├── heapstore_registry.c        # 注册表实现
│   ├── heapstore_trace.c           # 追踪实现
│   ├── heapstore_ipc.c             # IPC 实现
│   ├── heapstore_memory.c          # 内存实现
│   └── private.h                   # 内部头文件
│
├── tests/                          # 测试代码
│   ├── test_heapstore_core.c       # 核心测试
│   ├── test_heapstore_log.c        # 日志测试
│   ├── test_heapstore_registry.c   # 注册表测试
│   ├── test_heapstore_trace.c      # 追踪测试
│   ├── test_heapstore_ipc.c        # IPC 测试
│   ├── test_heapstore_memory.c     # 内存测试
│   ├── test_heapstore_integration.c# 集成测试
│   └── benchmark_heapstore.c       # 性能基准测试
│
└── scripts/                        # 辅助脚本
    ├── build.sh                    # Linux/macOS 构建
    ├── build.bat                   # Windows 构建
    └── deploy.sh                   # 部署脚本
```

---

## 📚 API 参考

### 核心 API

#### heapstore_init()

初始化数据分区系统。

```c
heapstore_error_t heapstore_init(const heapstore_config_t* config);
```

**参数**:
- `config`: 配置结构指针，可为 NULL（使用默认配置）

**返回值**:
- `heapstore_SUCCESS`: 成功
- `heapstore_ERR_INVALID_PARAM`: 参数无效
- `heapstore_ERR_ALREADY_INITIALIZED`: 已初始化
- `heapstore_ERR_DIR_CREATE_FAILED`: 目录创建失败

---

#### heapstore_shutdown()

关闭数据分区系统并清理所有资源。

```c
void heapstore_shutdown(void);
```

---

#### heapstore_health_check()

健康检查接口，检查各子系统状态。

```c
heapstore_error_t heapstore_health_check(
    bool* registry_ok,
    bool* trace_ok,
    bool* log_ok,
    bool* ipc_ok,
    bool* memory_ok
);
```

**参数**: 所有参数均为输出参数，可为 NULL

**返回值**: `heapstore_SUCCESS` 表示所有子系统健康

---

#### heapstore_get_stats()

获取系统统计信息。

```c
heapstore_error_t heapstore_get_stats(heapstore_stats_t* stats);
```

**统计信息结构**:
```c
typedef struct {
    uint64_t total_disk_usage_bytes;  // 总磁盘使用量
    uint64_t log_usage_bytes;         // 日志使用量
    uint64_t registry_usage_bytes;    // 注册表使用量
    uint64_t trace_usage_bytes;       // 追踪数据使用量
    uint64_t ipc_usage_bytes;         // IPC 数据使用量
    uint64_t memory_usage_bytes;      // 内存数据使用量
    uint32_t log_file_count;          // 日志文件数量
    uint32_t trace_file_count;        // 追踪文件数量
} heapstore_stats_t;
```

---

### 配置结构

```c
typedef struct heapstore_config {
    const char* root_path;            // 数据分区根路径
    size_t max_log_size_mb;           // 最大日志文件大小 (MB)
    int log_retention_days;           // 日志保留天数
    int trace_retention_days;         // 追踪数据保留天数
    bool enable_auto_cleanup;         // 启用自动清理
    bool enable_log_rotation;         // 启用日志轮转
    bool enable_trace_export;         // 启用追踪导出
    int db_vacuum_interval_days;      // 数据库 Vacuum 间隔 (天)
} heapstore_config_t;
```

---

## ⚙️ 配置说明

### 默认配置

```c
heapstore_config_t default_config = {
    .root_path = "heapstore",
    .max_log_size_mb = 100,
    .log_retention_days = 7,
    .trace_retention_days = 3,
    .enable_auto_cleanup = true,
    .enable_log_rotation = true,
    .enable_trace_export = true,
    .db_vacuum_interval_days = 7
};
```

### 生产环境配置

```c
heapstore_config_t prod_config = {
    .root_path = "/var/lib/agentos/heapstore",
    .max_log_size_mb = 500,          // 500MB
    .log_retention_days = 30,        // 保留 30 天
    .trace_retention_days = 7,       // 保留 7 天
    .enable_auto_cleanup = true,
    .enable_log_rotation = true,
    .enable_trace_export = true,
    .db_vacuum_interval_days = 7     // 每周优化
};
```

### 开发环境配置

```c
heapstore_config_t dev_config = {
    .root_path = ".heapstore_dev",
    .max_log_size_mb = 10,           // 小文件，方便调试
    .log_retention_days = 1,         // 只保留 1 天
    .trace_retention_days = 1,
    .enable_auto_cleanup = true,
    .enable_log_rotation = false,    // 开发环境不轮转
    .enable_trace_export = true,
    .db_vacuum_interval_days = 0     // 禁用优化
};
```

---

## 📊 性能指标

### 基准测试结果

测试环境：Intel i7-10700K, 32GB RAM, NVMe SSD

| 操作 | 吞吐量 | 延迟 (P50) | 延迟 (P99) |
|------|--------|-----------|-----------|
| 日志写入 | 125,000 ops/s | 8 μs | 45 μs |
| 批量日志写入 (100 条) | 2,500 batch/s | 400 μs | 1.2 ms |
| 注册表插入 | 8,500 ops/s | 118 μs | 350 μs |
| 注册表查询 | 15,000 ops/s | 67 μs | 180 μs |
| Span 写入 | 45,000 ops/s | 22 μs | 85 μs |
| 健康检查 | 500,000 ops/s | 2 μs | 8 μs |

### 内存占用

| 组件 | 内存占用 |
|------|---------|
| 核心模块 | 45 KB |
| 日志模块 | 28 KB |
| 注册表模块 | 120 KB |
| 追踪模块 | 85 KB |
| IPC 模块 | 32 KB |
| 内存模块 | 28 KB |
| **总计** | **~338 KB** |

---

## ✅ 最佳实践

### 1. 日志使用规范

**推荐做法**:
```c
// ✅ 使用宏，自动填充文件和行号
heapstore_LOG_INFO("service_name", trace_id, "操作成功");

// ✅ 包含关键上下文信息
heapstore_LOG_INFO("llm_d", task_id, "任务完成，耗时 %dms, token 数 %d", 
                   duration, token_count);

// ✅ 使用合适的日志级别
heapstore_LOG_ERROR("auth", NULL, "认证失败");  // 错误
heapstore_LOG_WARN("cache", NULL, "缓存未命中"); // 警告
heapstore_LOG_INFO("startup", NULL, "服务启动");  // 信息
heapstore_LOG_DEBUG("perf", trace_id, "性能数据：%f", perf_data); // 调试
```

**避免做法**:
```c
// ❌ 记录敏感信息
heapstore_LOG_INFO("auth", NULL, "密码：%s", password);

// ❌ 过多调试日志
for (int i = 0; i < 10000; i++) {
    heapstore_LOG_DEBUG("loop", NULL, "i=%d", i);
}
```

### 2. 错误处理规范

```c
// ✅ 检查所有返回值
heapstore_error_t err = heapstore_init(&config);
if (err != heapstore_SUCCESS) {
    fprintf(stderr, "初始化失败：%s\n", heapstore_strerror(err));
    return 1;
}

// ✅ 优雅降级
err = heapstore_registry_add_agent(&agent);
if (err == heapstore_ERR_DB_QUERY_FAILED) {
    // 数据库失败，尝试重试
    usleep(100000);  // 100ms
    err = heapstore_registry_add_agent(&agent);
}

// ✅ 资源清理
if (initialization_failed) {
    heapstore_shutdown();  // 确保调用
}
```

### 3. 性能优化建议

```c
// ✅ 批量写入日志（减少锁竞争）
for (int i = 0; i < 100; i++) {
    heapstore_LOG_INFO("batch", trace_id, "消息 %d", i);
}

// ✅ 定期刷新（避免缓冲区过大）
heapstore_flush();  // 每 1000 次操作后调用

// ✅ 合理使用日志级别（减少 DEBUG 日志）
heapstore_log_set_level(heapstore_LOG_INFO);  // 生产环境使用 INFO 级别

// ✅ 健康检查避免频繁调用（每 5-10 秒调用一次即可）
```

---

## 🔧 故障排查

### 常见问题

#### 问题 1: 初始化失败

**错误**: `heapstore_ERR_DIR_CREATE_FAILED`

**原因**:
- 目录权限不足
- 磁盘空间不足
- 路径不存在

**解决方案**:
```bash
# 检查权限
ls -la /var/lib/agentos/

# 修复权限
sudo chown -R $USER:$USER /var/lib/agentos/
sudo chmod -R 755 /var/lib/agentos/

# 检查磁盘空间
df -h

# 创建目录
mkdir -p /var/lib/agentos/heapstore
```

---

#### 问题 2: 数据库锁定

**错误**: `heapstore_ERR_DB_QUERY_FAILED`

**原因**:
- 多线程并发写入
- 事务未提交
- 数据库损坏

**解决方案**:
```bash
# 检查数据库状态
sqlite3 heapstore/registry/agents.db "PRAGMA integrity_check;"

# 修复数据库
sqlite3 heapstore/registry/agents.db "VACUUM;"
```

---

#### 问题 3: 日志文件过大

**现象**: 单个日志文件超过配置大小

**解决方案**:
```c
// 启用日志轮转
config.enable_log_rotation = true;
config.max_log_size_mb = 100;

// 提高日志级别
heapstore_log_set_level(heapstore_LOG_WARN);  // 只记录 WARN 及以上
```

---

## 📖 相关文档

### 项目内部文档

- [AgentOS 架构设计](../manuals/architecture/ARCHITECTURAL_PRINCIPLES.md)
- [C 语言编码规范](../manuals/specifications/coding_standard/C_coding_style_guide.md)
- [部署指南](../manuals/guides/deployment.md)

### 外部资源

- [SQLite 官方文档](https://www.sqlite.org/docs.html)
- [OpenTelemetry 规范](https://opentelemetry.io/docs/)
- [POSIX 线程编程](https://man7.org/linux/man-pages/man7/pthreads.7.html)

---

## 🤝 贡献指南

### 报告问题

发现 Bug 或有功能建议？请创建 Issue 并包含：

1. 问题描述
2. 复现步骤
3. 环境信息（OS、编译器版本）
4. 错误日志

### 提交代码

1. Fork 仓库
2. 创建特性分支 (`git checkout -b feature/amazing-feature`)
3. 提交更改 (`git commit -m 'Add amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 创建 Pull Request

---

## 📄 许可证

Copyright © 2026 SPHARX

根据 **Apache License 2.0** 授权。

---

## 📬 联系方式

- **项目主页**: https://github.com/spharx/agentos
- **问题反馈**: https://github.com/spharx/agentos/issues
- **技术讨论**: https://github.com/spharx/agentos/discussions

---

**"From data intelligence emerges."** - SPHARX

---

*最后更新*: 2026-03-28  
*维护者*: AgentOS Architecture Team
