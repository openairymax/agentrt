# Include — 公共头文件模块

**模块路径**: `agentos/commons/utils/include/`
**版本**: v0.0.5

## 概述

Include 模块提供 AgentOS 项目范围内的公共头文件，包含跨平台原子操作兼容层和通用检查宏。这些头文件供项目中的其他模块直接引用，提供一致的底层工具支持。

## 设计目标

- **跨平台原子操作**：统一 C11 `stdatomic.h`、Windows `Interlocked` API 和 GCC/Clang `__atomic` builtins 三套原子操作接口，提供跨平台的原子操作能力
- **通用检查宏**：消除项目中分散的参数验证和错误处理代码，提供一致的验证模式
- **零依赖**：尽可能减少对外部模块的依赖，提供独立的工具支持

## 目录结构

```
include/
├── atomic_compat.h              # 跨平台原子操作兼容层
├── check.h                      # 通用检查宏定义
└── README.md                    # 本文档
```

## 文件说明

### atomic_compat.h — 跨平台原子操作兼容层

提供 C11 `<stdatomic.h>` 的跨平台兼容实现，支持以下平台：

- **C11+ (Linux/macOS)**：使用系统 `<stdatomic.h>`
- **Windows**：使用 `Interlocked` API（`intrin.h`）
- **POSIX fallback**：使用 GCC/Clang `__atomic` builtins

#### 支持的操作类型

| 位宽 | 加载 | 存储 | 交换 | CAS | 算术加 | 算术减 |
|------|------|------|------|-----|--------|--------|
| 8 位 | `atomic_load_8` | `atomic_store_8` | `atomic_exchange_8` | `atomic_compare_exchange_strong_8` | `atomic_fetch_add_8` | `atomic_fetch_sub_8` |
| 16 位 | `atomic_load_16` | `atomic_store_16` | `atomic_exchange_16` | `atomic_compare_exchange_strong_16` | `atomic_fetch_add_16` | `atomic_fetch_sub_16` |
| 32 位 | `atomic_load_32` | `atomic_store_32` | `atomic_exchange_32` | `atomic_compare_exchange_strong_32` | `atomic_fetch_add_32` | `atomic_fetch_sub_32` |
| 64 位 | `atomic_load_64` | `atomic_store_64` | `atomic_exchange_64` | `atomic_compare_exchange_strong_64` | `atomic_fetch_add_64` | `atomic_fetch_sub_64` |
| 指针 | `atomic_load_ptr` | `atomic_store_ptr` | `atomic_exchange_ptr` | `atomic_compare_exchange_strong_ptr` | — | — |
| bool | `atomic_load_bool` | `atomic_store_bool` | `atomic_exchange_bool` | — | — | — |
| double | `atomic_load_double` | `atomic_store_double` | `atomic_exchange_double` | — | `atomic_fetch_add_double` | — |

#### 统一类型别名

| 类型 | 说明 |
|------|------|
| `atomic_bool` | 原子布尔 |
| `atomic_int` | 原子 int |
| `atomic_uint` | 原子 unsigned int |
| `atomic_long` | 原子 long |
| `atomic_ulong` | 原子 unsigned long |
| `atomic_int64_t` | 原子 int64_t |
| `atomic_uint64_t` | 原子 uint64_t |
| `atomic_size_t` | 原子 size_t |
| `atomic_double` | 原子 double |

#### 内存顺序

| 枚举值 | 说明 |
|------|------|
| `memory_order_relaxed` | 宽松顺序 |
| `memory_order_consume` | 消费顺序 |
| `memory_order_acquire` | 获取顺序 |
| `memory_order_release` | 释放顺序 |
| `memory_order_acq_rel` | 获取-释放顺序 |
| `memory_order_seq_cst` | 顺序一致性 |

#### 通用宏

| 宏 | 说明 |
|------|------|
| `atomic_init(ptr, val)` | 初始化原子变量 |
| `atomic_load(ptr)` | 原子加载（seq_cst） |
| `atomic_store(ptr, val)` | 原子存储（seq_cst） |
| `atomic_exchange(ptr, val)` | 原子交换（seq_cst） |
| `atomic_compare_exchange_strong(ptr, expected, desired)` | CAS 操作（seq_cst） |
| `atomic_fetch_add(ptr, val)` | 原子加法（seq_cst） |
| `atomic_fetch_sub(ptr, val)` | 原子减法（seq_cst） |
| `atomic_thread_fence(order)` | 内存屏障 |

### check.h — 通用检查宏

提供一组统一的参数验证、错误处理和资源清理宏。

#### 检查宏

| 宏 | 说明 |
|------|------|
| `CHECK_NULL_RET(ptr, err_code)` | 指针为 NULL 时返回指定错误码 |
| `CHECK_NULL(ptr)` | 指针为 NULL 时返回 `AGENTOS_EINVAL` |
| `CHECK_COND_RET(expr, err_code)` | 表达式为假时返回指定错误码 |
| `CHECK_COND(expr)` | 表达式为假时返回 `AGENTOS_EINVAL` |
| `CHECK_ERR_RET(func_call, err_var)` | 函数调用失败时返回错误码 |
| `CHECK_RANGE_RET(value, min, max, err_code)` | 值超出范围时返回错误码 |
| `CHECK_NONZERO_RET(value, err_code)` | 值为零时返回错误码 |
| `CHECK_STRING_RET(str, err_code)` | 字符串为空或 NULL 时返回错误码 |

#### 跳转标签宏

| 宏 | 说明 |
|------|------|
| `CHECK_ERR_GOTO(func_call, err_var, label)` | 函数调用失败时跳转到清理标签 |
| `CHECK_NULL_GOTO(ptr, label)` | 指针为 NULL 时跳转到清理标签 |
| `CHECK_NULL_GOTO_ERR(ptr, label, err_var, err_code)` | 指针为 NULL 时设置错误码并跳转 |

#### 资源管理宏

| 宏 | 说明 |
|------|------|
| `SAFE_FREE(ptr)` | 安全释放内存并将指针置为 NULL |
| `ALLOC_CHECK(ptr_var, size, label)` | 分配内存，失败则跳转到清理标签 |
| `CALLOC_CHECK(ptr_var, count, size, label)` | 分配并清零内存，失败则跳转到清理标签 |
| `STRDUP_CHECK(dest, src, label)` | 字符串复制，失败则跳转到清理标签 |
| `MALLOC_CHECK_ERR(ptr_var, size, label, err_var, err_code)` | 分配内存，失败则设置错误码并跳转 |
| `CALLOC_CHECK_ERR(ptr_var, count, size, label, err_var, err_code)` | 分配并清零内存，失败则设置错误码并跳转 |
| `STRDUP_CHECK_ERR(dest, src, label, err_var, err_code)` | 字符串复制，失败则设置错误码并跳转 |

## 使用示例

```c
// === atomic_compat.h 使用示例 ===
#include "atomic_compat.h"

atomic_int64_t counter = 0;
atomic_init(&counter, 0);

int64_t old = atomic_fetch_add_64(&counter, 1, memory_order_relaxed);
int64_t current = atomic_load_64(&counter, memory_order_acquire);

atomic_store_bool(&ready, true, memory_order_release);
atomic_thread_fence(memory_order_seq_cst);

// === check.h 使用示例 ===
#include "check.h"

agentos_error_t process_data(void *data, size_t size) {
    CHECK_NULL_RET(data, AGENTOS_ERR_NULL_POINTER);
    CHECK_RANGE_RET(size, 1, MAX_SIZE, AGENTOS_ERR_INVALID_PARAM);

    void *buffer = NULL;
    ALLOC_CHECK(buffer, size, cleanup);

    // 处理数据...

    SAFE_FREE(buffer);
    return AGENTOS_OK;

cleanup:
    SAFE_FREE(buffer);
    return AGENTOS_ERR_OUT_OF_MEMORY;
}
```

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `error/include/error.h` | check.h 依赖错误码定义（`AGENTOS_EINVAL` 等） |
| `stdbool.h` | 布尔类型支持 |
| `stddef.h` | 标准类型定义 |
| `stdint.h` | 固定宽度整数类型 |

---

© 2026 SPHARX Ltd. All Rights Reserved.