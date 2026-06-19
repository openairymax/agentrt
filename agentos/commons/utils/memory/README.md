# Memory — 内存管理模块

**模块路径**: `agentos/commons/utils/memory/`
**版本**: v0.1.0

## 概述

Memory 模块提供 AgentRT 统一的内存管理框架，包括安全内存分配/释放、内存池管理、内存调试和统计追踪。该模块是 AgentRT 中所有内存操作的基础设施，旨在消除项目中分散的内存管理代码，提供一致的内存管理策略，并支持泄漏检测、边界检查和 OOM 水位监控。

## 设计目标

- **安全分配**：带标签的内存分配接口，支持零初始化和对齐分配
- **内存池**：高效的内存池管理，减少频繁分配/释放的内存碎片和开销
- **调试支持**：可选的泄漏检测、边界检查、释放后使用检查和双重释放检查
- **统计追踪**：全局内存统计、按类别跟踪、水位监控和 OOM 响应
- **向后兼容**：提供与标准 C 库兼容的 `AGENTOS_MALLOC` / `AGENTOS_FREE` 等宏，便于渐进式迁移
- **线程安全**：所有公共接口均为线程安全

## 目录结构

```
memory/
├── include/
│   ├── agentos_memory.h          # 核心层 API（内存分配/释放/统计）
│   ├── memory_common.h           # 内存池与安全分配接口
│   ├── memory_pool.h             # 内存池管理（创建/分配/释放/统计）
│   ├── memory_debug.h            # 内存调试（泄漏检测/边界检查/堆栈跟踪）
│   └── memory_compat.h           # 向后兼容层（安全包装器/迁移宏/OOM 水位监控）
├── src/
│   ├── memory.c                  # 核心内存分配实现
│   ├── memory_common.c           # 内存池与安全分配实现
│   ├── memory_pool.c             # 内存池实现
│   └── memory_debug.c            # 内存调试实现
├── test/
│   └── test_memory.c             # 单元测试
└── README.md                     # 本文档
```

## 核心数据结构

### memory_stats_t — 内存统计信息

| 字段 | 类型 | 说明 |
|------|------|------|
| `total_allocated` | `size_t` | 总分配内存（字节） |
| `total_freed` | `size_t` | 总释放内存（字节） |
| `current_allocated` | `size_t` | 当前分配内存（字节） |
| `peak_allocated` | `size_t` | 峰值分配内存（字节） |
| `allocation_count` | `size_t` | 分配次数 |
| `free_count` | `size_t` | 释放次数 |
| `leak_count` | `size_t` | 泄漏次数 |

### memory_pool_options_t — 内存池选项

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `block_size` | `size_t` | — | 内存块大小（字节） |
| `initial_blocks` | `size_t` | 16 | 初始预分配块数 |
| `max_blocks` | `size_t` | 0（无限制） | 最大块数 |
| `expansion_size` | `size_t` | 8 | 池满时扩展的块数 |
| `thread_safe` | `bool` | true | 是否线程安全 |
| `name` | `const char *` | NULL | 内存池名称（调试用） |

### memory_pool_stats_t — 内存池统计信息

| 字段 | 类型 | 说明 |
|------|------|------|
| `block_size` | `size_t` | 内存块大小 |
| `total_blocks` | `size_t` | 总块数 |
| `allocated_blocks` | `size_t` | 已分配块数 |
| `free_blocks` | `size_t` | 空闲块数 |
| `total_memory` | `size_t` | 总内存（字节） |
| `used_memory` | `size_t` | 已使用内存（字节） |
| `allocation_count` | `size_t` | 分配次数 |
| `free_count` | `size_t` | 释放次数 |
| `hit_count` | `size_t` | 缓存命中次数 |
| `miss_count` | `size_t` | 缓存未命中次数 |

### memory_debug_options_t — 内存调试选项

| 字段 | 类型 | 说明 |
|------|------|------|
| `enable_leak_check` | `bool` | 是否启用泄漏检查 |
| `enable_boundary_check` | `bool` | 是否启用边界检查 |
| `enable_use_after_free_check` | `bool` | 是否启用释放后使用检查 |
| `enable_double_free_check` | `bool` | 是否启用双重释放检查 |
| `enable_invalid_free_check` | `bool` | 是否启用无效释放检查 |
| `track_allocations` | `bool` | 是否跟踪分配信息 |
| `fill_pattern_on_alloc` | `bool` | 分配时填充模式 |
| `fill_pattern_on_free` | `bool` | 释放时填充模式 |
| `redzone_size` | `size_t` | 红区大小（边界检查） |
| `verbosity_level` | `int` | 详细级别（0-3） |

### memory_stats_extended_t — 扩展统计（SEC-15 合规）

| 字段 | 类型 | 说明 |
|------|------|------|
| `current_allocated` | `size_t` | 当前分配内存 |
| `peak_allocated` | `size_t` | 峰值分配内存 |
| `leak_suspected` | `size_t` | 疑似泄漏字节数 |
| `short_lived_high_water` | `size_t` | 短生命周期分配高水位 |
| `alloc_count_by_category[3]` | `size_t` | 按类别统计分配次数 |
| `bytes_by_category[3]` | `size_t` | 按类别统计分配字节 |
| `oom_event_count` | `uint64_t` | OOM 事件总数 |
| `current_watermark` | `watermark_level_t` | 当前水位级别 |
| `total_system_memory` | `size_t` | 系统总内存（字节） |

## 接口说明

### 核心层 API（agentos_memory.h）

| 函数 | 说明 |
|------|------|
| `memory_init(options)` | 初始化内存管理模块 |
| `memory_cleanup()` | 清理内存管理模块 |
| `memory_alloc(size, tag)` | 分配内存（带标签） |
| `memory_calloc(size, tag)` | 分配并清零内存 |
| `memory_aligned_alloc(alignment, size, tag)` | 分配对齐内存 |
| `memory_realloc(ptr, new_size, tag)` | 重新分配内存 |
| `memory_free(ptr)` | 释放内存 |
| `memory_get_stats(stats)` | 获取全局内存统计 |
| `memory_reset_stats()` | 重置全局内存统计 |
| `memory_get_current_usage()` | 获取当前分配总量 |
| `memory_get_peak_usage()` | 获取峰值分配量 |
| `memory_check_leaks(dump_to_stderr)` | 检查内存泄漏 |
| `memory_dump_debug_info(file)` | 转储内存调试信息 |
| `memory_validate(ptr)` | 验证内存块完整性 |
| `memory_set_fail_callback(callback, user_data)` | 设置分配失败回调 |

### 内存池 API（memory_pool.h）

| 函数 | 说明 |
|------|------|
| `memory_pool_create(options)` | 创建内存池 |
| `memory_pool_destroy(pool)` | 销毁内存池 |
| `memory_pool_alloc(pool)` | 从内存池分配内存块 |
| `memory_pool_calloc(pool)` | 从内存池分配并清零内存块 |
| `memory_pool_free(pool, ptr)` | 释放内存块回内存池 |
| `memory_pool_get_stats(pool, stats)` | 获取内存池统计 |
| `memory_pool_reset_stats(pool)` | 重置内存池统计 |
| `memory_pool_prealloc(pool, count)` | 预分配内存块 |
| `memory_pool_clear(pool)` | 清空空闲块 |
| `memory_pool_is_empty(pool)` | 检查内存池是否为空 |
| `memory_pool_is_full(pool)` | 检查内存池是否已满 |
| `memory_pool_expand(pool, additional_blocks)` | 扩展内存池 |
| `memory_pool_shrink(pool, blocks_to_keep)` | 收缩内存池 |
| `memory_pool_validate(pool)` | 验证内存池完整性 |
| `memory_pool_iterate(pool, callback, user_data)` | 遍历所有块 |
| `memory_pool_create_default(block_size)` | 创建默认选项的内存池 |

### 内存调试 API（memory_debug.h）

| 函数 | 说明 |
|------|------|
| `memory_debug_init(options)` | 初始化内存调试 |
| `memory_debug_enable(enable)` | 启用/禁用内存调试 |
| `memory_debug_is_enabled()` | 检查调试是否启用 |
| `memory_debug_set_callback(callback, user_data)` | 设置调试回调 |
| `memory_debug_check_leaks(report, dump_to_log)` | 检查内存泄漏 |
| `memory_debug_validate(ptr, error)` | 验证内存块完整性 |
| `memory_debug_validate_all(error_count, dump_to_log)` | 验证所有已分配内存块 |
| `memory_debug_dump_info(file, include_stack_trace)` | 转储调试信息 |
| `memory_debug_get_allocation_info(ptr, ...)` | 获取内存块分配信息 |
| `memory_debug_set_tag(ptr, tag)` | 设置内存块标签 |
| `memory_debug_set_feature(feature, enable)` | 启用/禁用特定调试功能 |
| `memory_debug_get_stats(...)` | 获取调试统计 |
| `memory_debug_enable_stack_trace(enable, max_depth)` | 启用堆栈跟踪 |
| `memory_debug_get_stack_trace(ptr, frames, max_frames)` | 获取堆栈跟踪 |
| `memory_debug_checkpoint(name)` | 创建内存状态检查点 |
| `memory_debug_compare_checkpoints(cp1, cp2, diff_report)` | 比较检查点 |
| `memory_debug_set_log_level(level)` | 设置日志级别 |
| `memory_debug_log_operation(op, ptr, size, file, line, func)` | 记录内存操作 |

### 兼容层 API（memory_compat.h）

| 函数/宏 | 说明 |
|------|------|
| `AGENTOS_MALLOC(size)` | 安全内存分配（兼容 `malloc`） |
| `AGENTOS_CALLOC(num, size)` | 安全内存分配并清零（兼容 `calloc`） |
| `AGENTOS_REALLOC(ptr, new_size)` | 安全内存重分配（兼容 `realloc`） |
| `AGENTOS_FREE(ptr)` | 安全内存释放（兼容 `free`） |
| `AGENTOS_STRDUP(str)` | 安全字符串复制（兼容 `strdup`） |
| `AGENTOS_STRNDUP(str, n)` | 安全字符串复制（带长度限制） |
| `AGENTOS_STRNCPY_TERM(dst, src, size)` | 安全字符串复制（保证 null 终止） |
| `AGENTOS_MEMCPY_SAFE(dst, src, size, dst_capacity)` | 带边界检查的 `memcpy` |
| `AGENTOS_MEMSET(ptr, value, size)` | 带零大小保护的 `memset` |
| `SAFE_MALLOC(ptr, size)` | 安全分配（失败返回 NULL） |
| `SAFE_CALLOC(ptr, num, size)` | 安全清零分配（失败返回 NULL） |
| `SAFE_MALLOC_ARRAY(ptr, count, element_size)` | 安全数组分配（带溢出检查） |
| `SAFE_CALLOC_ARRAY(ptr, count, element_size)` | 安全数组清零分配（带溢出检查） |
| `MEMORY_FREE_SAFE(ptr_ptr)` | 安全释放并置 NULL |

### 扩展统计（SEC-15 合规）

| 函数 | 说明 |
|------|------|
| `agentos_memory_stats_extended_init(ext_stats, capacity)` | 初始化扩展统计跟踪器 |
| `agentos_memory_track_alloc(ext_stats, ptr, size, category, file, line)` | 记录一次分配 |
| `agentos_memory_track_free(ext_stats, ptr)` | 记录一次释放 |
| `agentos_check_leaks_scheduled(ext_stats, max_age_ms)` | 定期检测疑似泄漏 |
| `agentos_memory_calc_watermark(ext_stats)` | 计算当前内存水位级别 |
| `agentos_register_watermark_callback(ext_stats, callback, context)` | 注册水位变化回调 |
| `agentos_memory_stats_report(ext_stats, tag)` | 内存统计定期上报 |
| `agentos_memory_stats_extended_destroy(ext_stats)` | 销毁扩展统计跟踪器 |

## 使用示例

```c
#include "agentos_memory.h"
#include "memory_pool.h"
#include "memory_debug.h"

// === 基本内存分配 ===
memory_init(NULL);

char *buf = memory_alloc(1024, "my_buffer");
memory_free(buf);
MEMORY_FREE_SAFE(&buf);  // buf 现在为 NULL

// === 使用兼容层宏 ===
void *data = AGENTOS_MALLOC(4096);
AGENTOS_FREE(data);

// === 内存池 ===
memory_pool_options_t opts = {
    .block_size = 256,
    .initial_blocks = 32,
    .max_blocks = 1024,
    .expansion_size = 16,
    .thread_safe = true,
    .name = "request_pool"
};
memory_pool_t *pool = memory_pool_create(&opts);

void *block = memory_pool_alloc(pool);
// 使用 block...
memory_pool_free(pool, block);

memory_pool_stats_t pool_stats;
memory_pool_get_stats(pool, &pool_stats);
printf("Pool usage: %zu/%zu blocks\n",
       pool_stats.allocated_blocks, pool_stats.total_blocks);

memory_pool_destroy(pool);

// === 内存调试 ===
memory_debug_options_t debug_opts = {
    .enable_leak_check = true,
    .enable_boundary_check = true,
    .enable_double_free_check = true,
    .track_allocations = true,
    .verbosity_level = 2
};
memory_debug_init(&debug_opts);
memory_debug_enable(true);

// 发现泄漏
size_t leaked = memory_debug_check_leaks(NULL, true);
if (leaked > 0) {
    printf("Leaked %zu bytes\n", leaked);
}

// === 全局统计 ===
memory_stats_t stats;
memory_get_stats(&stats);
printf("Current: %zu, Peak: %zu, Allocs: %zu, Frees: %zu\n",
       stats.current_allocated, stats.peak_allocated,
       stats.allocation_count, stats.free_count);

memory_cleanup();
```

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `error.h` | 错误码定义（`agentos_error_t`） |
| `stdbool.h` | 布尔类型支持 |
| `stddef.h` | 标准类型定义 |
| `stdint.h` | 固定宽度整数类型 |

---

© 2026 SPHARX Ltd. All Rights Reserved.