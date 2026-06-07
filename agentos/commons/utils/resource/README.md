# Resource — 资源管理模块

**模块路径**: `agentos/commons/utils/resource/`
**版本**: v0.1.0

## 概述

Resource 模块提供资源作用域守卫（RAII 模式）和资源配额管理两大功能，确保资源在作用域结束时正确释放，并对内存、CPU、I/O、网络等资源进行配额限制和统计。该模块遵循 E-3 资源确定性原则，确保每个资源的生命周期可预测、可追踪、可验证。

## 设计目标

- **资源作用域守卫**：RAII 模式自动释放资源，支持自定义释放函数，适用于文件句柄、内存、锁、网络连接等
- **资源配额管理**：对内存、CPU 时间、I/O 操作数、网络字节数进行配额限制和实时监控
- **资源追踪**：可选的资源分配追踪（通过 `AGENTOS_RESOURCE_TRACKING` 编译选项启用），检测内存泄漏
- **可观测性**：配额超限日志告警、资源使用统计、超限类型查询

## 目录结构

```
resource/
├── src/
│   ├── resource_guard.h        # 资源作用域守卫接口定义
│   ├── resource_guard.c        # 资源作用域守卫实现
│   ├── resource_quota.h        # 资源配额管理接口定义
│   └── resource_quota.c        # 资源配额管理实现
└── README.md                   # 本文档
```

## 核心数据结构

### agentos_resource_guard_t — 资源守卫

| 字段 | 类型 | 说明 |
|------|------|------|
| `resource` | `void *` | 资源指针 |
| `cleanup` | `agentos_resource_cleanup_t` | 清理函数指针 |
| `file` | `const char *` | 分配所在的文件名 |
| `line` | `int` | 分配所在的行号 |
| `name` | `const char *` | 资源名称 |
| `active` | `int` | 是否激活（1=活跃，0=已取消） |

### agentos_resource_quota_t — 资源配额

| 字段 | 类型 | 说明 |
|------|------|------|
| `max_memory_bytes` | `size_t` | 最大内存使用量（字节） |
| `max_cpu_time_ms` | `uint64_t` | 最大 CPU 时间（毫秒） |
| `max_io_ops` | `size_t` | 最大 I/O 操作数 |
| `max_network_bytes` | `size_t` | 最大网络传输字节数 |
| `timeout_ms` | `uint64_t` | 超时时间（毫秒） |

### agentos_resource_usage_t — 资源使用统计

| 字段 | 类型 | 说明 |
|------|------|------|
| `current_memory_bytes` | `size_t` | 当前内存使用量 |
| `peak_usage` | `size_t` | 峰值内存使用量 |
| `total_cpu_time_ms` | `uint64_t` | 累计 CPU 时间 |
| `total_io_ops` | `size_t` | 累计 I/O 操作数 |
| `total_network_bytes` | `size_t` | 累计网络字节数 |
| `start_time` | `time_t` | 启动时间 |
| `last_update` | `time_t` | 最后更新时间 |
| `operation_count` | `uint64_t` | 操作计数 |

### agentos_resource_manager_t — 资源管理器

管理器是不透明结构体，内部包含配额配置、使用统计、资源 ID 和超限标志位。

## 接口说明

### 资源守卫 API

| 函数 | 说明 |
|------|------|
| `agentos_resource_guard_init(guard, resource, cleanup, file, line, name)` | 初始化资源守卫 |
| `agentos_resource_guard_cleanup(guard)` | 执行资源清理（调用 cleanup 函数） |
| `agentos_resource_guard_dismiss(guard)` | 取消资源清理（转移所有权，不再自动释放） |

### 资源守卫宏

| 宏 | 说明 |
|------|------|
| `AGENTOS_SCOPE_GUARD(resource, cleanup)` | 创建作用域守卫（自动生成变量名，作用域结束时自动清理） |
| `AGENTOS_SCOPE_EXIT(resource, cleanup)` | 创建作用域退出守卫（同 `SCOPE_GUARD` 的别名） |
| `AGENTOS_SCOPE_DISMISS(resource)` | 取消作用域守卫（转移所有权） |

### 资源追踪 API（需 `AGENTOS_RESOURCE_TRACKING`）

| 函数/宏 | 说明 |
|------|------|
| `agentos_resource_track_alloc(resource, type, file, line)` | 注册资源分配 |
| `agentos_resource_track_free(resource)` | 注销资源分配 |
| `agentos_resource_track_report(out_report)` | 获取资源追踪报告（未释放资源列表） |
| `agentos_resource_track_clear()` | 清空资源追踪记录 |
| `AGENTOS_TRACKED_MALLOC(size)` | 追踪的内存分配 |
| `AGENTOS_TRACKED_FREE(ptr)` | 追踪的内存释放 |

### 资源配额 API

| 函数 | 说明 |
|------|------|
| `agentos_resource_manager_create(quota, resource_id, out_manager)` | 创建资源管理器 |
| `agentos_resource_manager_destroy(manager)` | 销毁资源管理器 |
| `agentos_resource_check_memory(manager, requested_bytes)` | 检查内存配额是否充足（不足返回 `AGENTOS_ENOMEM`） |
| `agentos_resource_record_allocation(manager, bytes)` | 记录内存分配（更新峰值） |
| `agentos_resource_record_free(manager, bytes)` | 记录内存释放 |
| `agentos_resource_record_io(manager)` | 记录 I/O 操作（超限返回 `AGENTOS_EBUSY`） |
| `agentos_resource_is_exceeded(manager)` | 检查是否有资源超限 |
| `agentos_resource_get_usage(manager, out_usage)` | 获取资源使用统计 |
| `agentos_resource_get_exceeded_info(manager)` | 获取超限资源类型信息 |

## 使用示例

```c
#include "resource_guard.h"
#include "resource_quota.h"

// ===== 资源作用域守卫 =====
void process_file(const char *path) {
    FILE *file = fopen(path, "r");
    if (!file) return;

    // 作用域结束时自动调用 fclose(file)
    AGENTOS_SCOPE_EXIT(file, (agentos_resource_cleanup_t)fclose);

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), file)) {
        // 处理文件内容...
    }
    // 无需手动 fclose，作用域守卫自动处理
}

void allocate_buffer(void) {
    void *buffer = malloc(1024 * 1024);
    if (!buffer) return;

    // 作用域结束时自动调用 free(buffer)
    AGENTOS_SCOPE_GUARD(buffer, free);

    // 使用 buffer...
    // 如果需要在某个条件分支转移所有权，调用 AGENTOS_SCOPE_DISMISS
    if (success) {
        AGENTOS_SCOPE_DISMISS(buffer);
        return buffer;  // 所有权转移给调用者
    }
    // 否则 buffer 自动释放
}

// ===== 资源配额管理 =====
void run_with_quota(void) {
    agentos_resource_quota_t quota = {
        .max_memory_bytes = 100 * 1024 * 1024,  // 100 MB
        .max_cpu_time_ms = 5000,                 // 5 秒
        .max_io_ops = 1000,                      // 1000 次 I/O
        .max_network_bytes = 10 * 1024 * 1024,   // 10 MB
        .timeout_ms = 10000                       // 10 秒超时
    };

    agentos_resource_manager_t *manager = NULL;
    agentos_resource_manager_create(&quota, "task-001", &manager);

    // 分配内存前检查配额
    size_t request = 50 * 1024 * 1024;  // 50 MB
    if (agentos_resource_check_memory(manager, request) == AGENTOS_SUCCESS) {
        void *buffer = malloc(request);
        agentos_resource_record_allocation(manager, request);
        // 使用 buffer...
        free(buffer);
        agentos_resource_record_free(manager, request);
    } else {
        AGENTOS_LOG_WARN("Memory quota exceeded for task-001");
    }

    // 执行 I/O 操作
    for (int i = 0; i < 100; i++) {
        if (agentos_resource_record_io(manager) != AGENTOS_SUCCESS) {
            AGENTOS_LOG_WARN("I/O quota exceeded");
            break;
        }
        // 执行 I/O...
    }

    // 检查资源状态
    if (agentos_resource_is_exceeded(manager)) {
        printf("Resource exceeded: %s\n",
               agentos_resource_get_exceeded_info(manager));
    }

    // 获取使用统计
    agentos_resource_usage_t usage;
    agentos_resource_get_usage(manager, &usage);
    printf("Memory: %zu / %zu (peak: %zu)\n",
           usage.current_memory_bytes, quota.max_memory_bytes,
           usage.peak_usage);

    agentos_resource_manager_destroy(manager);
}
```

## 资源超限标志位

| 标志位 | 值 | 说明 |
|------|-----|------|
| `RESOURCE_FLAG_MEMORY_EXCEEDED` | 0x01 | 内存超限 |
| `RESOURCE_FLAG_CPU_EXCEEDED` | 0x02 | CPU 时间超限 |
| `RESOURCE_FLAG_IO_EXCEEDED` | 0x04 | I/O 操作超限 |
| `RESOURCE_FLAG_NETWORK_EXCEEDED` | 0x08 | 网络字节数超限 |

## 资源追踪报告格式

启用 `AGENTOS_RESOURCE_TRACKING` 后，`agentos_resource_track_report()` 输出格式：

```
Resource leak report:
===================
Total leaks: 3

[1] Type: memory, Ptr: 0x7f8a4c001000, File: main.c:42, Time: 1234567890 ns
[2] Type: memory, Ptr: 0x7f8a4c002000, File: main.c:58, Time: 1234567891 ns
[3] Type: memory, Ptr: 0x7f8a4c003000, File: main.c:73, Time: 1234567892 ns
```

## 配置选项

| 参数 | 说明 |
|------|------|
| `AGENTOS_RESOURCE_TRACKING` | 编译时定义以启用资源追踪功能（默认关闭） |

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `memory_compat.h` | 统一内存管理宏 |
| `agentos_memory.h` | 内存分配/释放函数 |
| `agentos_string.h` | 字符串操作 |
| `sync.h` | 互斥锁（资源追踪使用） |
| `atomic_compat.h` | 跨平台原子操作 |
| `platform.h` | 时间戳获取 |
| `logger.h` | 日志记录（配额超限告警） |
| `error.h` | 统一错误码定义 |

---

© 2026 SPHARX Ltd. All Rights Reserved.