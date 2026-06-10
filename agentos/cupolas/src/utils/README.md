# Cupolas Utils — 安全工具库

**模块路径**: `agentos/cupolas/src/utils/`
**版本**: v0.1.0

## 概述

Cupolas Utils 是 Cupolas 安全穹顶的内部工具库，提供所有 Cupolas 子模块共享的基础设施，包括安全内存管理、统一错误检查、线程同步原语、日志桥接、编译器提示、位操作、对齐工具和时间工具。该模块不对外暴露公共 API，仅供 Cupolas 内部子模块使用。

## 设计目标

- **安全内存管理**：零初始化分配、NULL 安全释放、类型安全
- **统一错误检查**：Cupolas 全模块共享的参数校验和提前返回宏
- **线程同步**：跨平台互斥锁抽象（Windows CRITICAL_SECTION / POSIX pthread）
- **日志桥接**：将 Cupolas 内部日志桥接到 Commons 日志系统
- **编译器提示**：跨编译器的属性注解和分支预测宏
- **零成本抽象**：所有宏展开为直接 API 调用

## 目录结构

```
utils/
├── cupolas_utils.h              # 工具库公共头文件
├── cupolas_utils.c              # 工具库实现
└── README.md                    # 本文档
```

## 核心功能

### 线程同步原语

| 宏 | 说明 |
|------|------|
| `CUPOLAS_MUTEX_TYPE` | 互斥锁类型（`cupolas_mutex_t`） |
| `CUPOLAS_MUTEX_INIT(m)` | 初始化互斥锁 |
| `CUPOLAS_MUTEX_LOCK(m)` | 加锁 |
| `CUPOLAS_MUTEX_UNLOCK(m)` | 解锁 |
| `CUPOLAS_MUTEX_DESTROY(m)` | 销毁互斥锁 |

> 底层使用 `platform.h` 提供的跨平台互斥锁实现。非递归锁，同线程重复加锁将死锁。

### 安全内存管理

| 宏 | 说明 |
|------|------|
| `CUPOLAS_ALLOC(type, count)` | 零初始化分配数组（calloc） |
| `CUPOLAS_ALLOC_STRUCT(type)` | 零初始化分配单个结构体 |
| `CUPOLAS_ALLOC_ARRAY(type, count)` | 未初始化分配数组（malloc，更快） |
| `CUPOLAS_REALLOC(ptr, type, count)` | 类型安全的重新分配 |
| `CUPOLAS_FREE(ptr)` | 释放内存并将指针置 NULL |
| `CUPOLAS_FREE_ARRAY(ptr)` | NULL 检查后释放并置 NULL |

> 底层使用 Commons 的 `AGENTOS_CALLOC`/`AGENTOS_MALLOC`/`AGENTOS_FREE` 宏。

### 错误检查宏

| 宏 | 说明 |
|------|------|
| `CUPOLAS_CHECK_NULL(ptr)` | NULL 检查，为空则返回 `AGENTOS_EINVAL` |
| `CUPOLAS_CHECK_NULL_RET(ptr, ret)` | NULL 检查，自定义返回值 |
| `CUPOLAS_CHECK_RESULT(expr)` | 表达式非零则返回 `AGENTOS_EINVAL` |
| `CUPOLAS_CHECK_RESULT_RET(expr, ret)` | 表达式非零则返回自定义值 |
| `CUPOLAS_CHECK_TRUE(cond)` | 条件为假则返回 `AGENTOS_EINVAL` |
| `CUPOLAS_CHECK_TRUE_RET(cond, ret)` | 条件为假则返回自定义值 |

### 编译器提示

| 宏 | 说明 |
|------|------|
| `CUPOLAS_LIKELY(x)` | 分支预测：x 大概率为真 |
| `CUPOLAS_UNLIKELY(x)` | 分支预测：x 大概率为假 |
| `CUPOLAS_INLINE` | 强制内联 |
| `CUPOLAS_NOINLINE` | 禁止内联 |
| `CUPOLAS_DEPRECATED(msg)` | 标记为已弃用 |

### 日志宏

| 宏 | 说明 |
|------|------|
| `CUPOLAS_LOG(fmt, ...)` | INFO 级别日志（需 `CUPOLAS_ENABLE_LOGGING`） |
| `CUPOLAS_LOG_ERROR(fmt, ...)` | ERROR 级别日志（始终启用） |
| `CUPOLAS_LOG_DEBUG(fmt, ...)` | DEBUG 级别日志（需 `CUPOLAS_ENABLE_LOGGING`） |

> 未定义 `CUPOLAS_ENABLE_LOGGING` 时，`CUPOLAS_LOG` 和 `CUPOLAS_LOG_DEBUG` 编译为空操作，`CUPOLAS_LOG_ERROR` 始终启用。

### 字符串与工具宏

| 宏 | 说明 |
|------|------|
| `CUPOLAS_STRINGIFY(x)` | 转字符串字面量 |
| `CUPOLAS_TOSTRING(x)` | 二级字符串化（支持宏展开） |
| `CUPOLAS_CONCAT(a, b)` | 拼接两个 token |
| `CUPOLAS_CONCAT3(a, b, c)` | 拼接三个 token |
| `CUPOLAS_ARRAY_SIZE(arr)` | 计算静态数组元素数 |
| `CUPOLAS_MIN(a, b)` | 最小值 |
| `CUPOLAS_MAX(a, b)` | 最大值 |
| `CUPOLAS_CLAMP(x, lo, hi)` | 值域限制 |
| `CUPOLAS_ABS(x)` | 绝对值 |

### 对齐工具

| 宏 | 说明 |
|------|------|
| `CUPOLAS_ALIGN_UP(x, align)` | 向上对齐 |
| `CUPOLAS_ALIGN_DOWN(x, align)` | 向下对齐 |
| `CUPOLAS_IS_ALIGNED(x, align)` | 检查是否对齐 |

### 位操作

| 宏 | 说明 |
|------|------|
| `CUPOLAS_BIT(n)` | 创建第 n 位掩码 |
| `CUPOLAS_BIT_SET(x, n)` | 设置第 n 位 |
| `CUPOLAS_BIT_CLEAR(x, n)` | 清除第 n 位 |
| `CUPOLAS_BIT_TEST(x, n)` | 测试第 n 位 |
| `CUPOLAS_BIT_FLIP(x, n)` | 翻转第 n 位 |

### 时间工具

| 宏 | 说明 |
|------|------|
| `CUPOLAS_SLEEP_MS(ms)` | 跨平台毫秒级休眠 |

### 编译时断言

| 宏 | 说明 |
|------|------|
| `CUPOLAS_STATIC_ASSERT(cond, msg)` | 编译时断言 |

### 工具函数

| 函数 | 说明 |
|------|------|
| `cupolas_strdup(str)` | NULL 安全的字符串复制 |
| `cupolas_strlcpy(dest, src, len)` | 安全字符串拷贝（始终 null 终止） |
| `cupolas_memset_s(ptr, len)` | 安全内存清零（防止编译器优化移除） |
| `cupolas_get_timestamp_ms()` | 获取当前时间戳（毫秒） |
| `cupolas_get_timestamp_ns()` | 获取高精度时间戳（纳秒） |
| `cupolas_hash_string(str)` | djb2 字符串哈希（32 位） |
| `cupolas_log_message(level, fmt, ...)` | 统一日志函数 |

## 使用示例

```c
#include "cupolas_utils.h"

int my_function(my_config_t *config) {
    CUPOLAS_CHECK_NULL(config);

    CUPOLAS_MUTEX_TYPE lock;
    CUPOLAS_MUTEX_INIT(&lock);

    my_data_t *data = CUPOLAS_ALLOC_STRUCT(my_data_t);
    if (CUPOLAS_UNLIKELY(data == NULL)) {
        CUPOLAS_LOG_ERROR("内存分配失败");
        return cupolas_ERR_OUT_OF_MEMORY;
    }

    CUPOLAS_MUTEX_LOCK(&lock);
    /* 临界区... */
    CUPOLAS_MUTEX_UNLOCK(&lock);

    /* 安全清零敏感数据 */
    cupolas_memset_s(data->secret, sizeof(data->secret));

    CUPOLAS_FREE(data);
    CUPOLAS_MUTEX_DESTROY(&lock);
    return 0;
}
```

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `platform.h` | 平台抽象层（互斥锁定义） |
| `error.h`（Commons） | 统一错误码定义 |
| `memory_compat.h`（Commons） | 内存管理宏 |
| `<string.h>` | 字符串操作 |
| `<stdlib.h>` | 标准库 |
| `<stdint.h>` | 固定宽度整数类型 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
