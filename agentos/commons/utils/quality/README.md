# Quality — 质量保障模块

**模块路径**: `agentos/commons/utils/quality/`
**版本**: v0.0.5

## 概述

Quality 模块是 AgentOS 的代码质量保证框架，以 header-only 方式提供标准化的输入验证、错误处理、资源管理和边界检查工具。该模块遵循 E-1 安全内生、E-3 资源确定性、E-6 错误可追溯等架构原则，通过编译期宏和内联函数在零运行时开销的前提下提供全面的代码质量保障。

## 设计目标

- **输入验证**：NULL 检查、范围检查、字符串长度检查、数组索引检查、空串检查
- **错误处理**：安全执行模式、goto 清理标签模式、错误传播
- **资源管理**：RAII 风格的作用域守卫、自动释放、安全内存操作
- **边界检查**：整数溢出检测、安全类型转换、缓冲区边界检查
- **零开销**：全部为宏和内联函数，编译期展开，无运行时成本

## 目录结构

```
quality/
├── agentos_quality.h           # Header-only 质量保障框架（宏和内联函数）
└── README.md                   # 本文档
```

## 核心功能

### 输入验证宏

| 宏 | 说明 |
|------|------|
| `AGENTOS_CHECK_NULL(ptr, error_code)` | 检查指针是否为 NULL，是则返回错误码 |
| `AGENTOS_CHECK_NULL_GOTO(ptr, label, error_code)` | 检查指针是否为 NULL，是则跳转到清理标签 |
| `AGENTOS_CHECK_CONDITION(cond, error_code)` | 检查条件是否成立，不成立则返回错误码 |
| `AGENTOS_CHECK_CONDITION_GOTO(cond, label, error_code)` | 检查条件是否成立，不成立则跳转 |
| `AGENTOS_CHECK_RANGE(value, min, max, error_code)` | 检查值是否在 `[min, max]` 范围内 |
| `AGENTOS_CHECK_MIN(value, min, error_code)` | 检查值是否 >= 最小值 |
| `AGENTOS_CHECK_MAX(value, max, error_code)` | 检查值是否 <= 最大值 |
| `AGENTOS_CHECK_STR_LEN(str, max_len, error_code)` | 检查字符串长度是否在允许范围内 |
| `AGENTOS_CHECK_ARRAY_INDEX(index, size, error_code)` | 检查数组索引是否有效 |
| `AGENTOS_CHECK_EMPTY(str, error_code)` | 检查字符串是否为空或 NULL |
| `AGENTOS_CHECK_BOUNDS(idx, size, error_code)` | 数组越界检查（兼容别名） |

### 错误处理宏

| 宏 | 说明 |
|------|------|
| `AGENTOS_SAFE_EXEC(expr, cleanup_label, error_var)` | 安全执行操作，失败时跳转到清理标签 |
| `AGENTOS_SAFE_ALLOC(var, size, cleanup_label, error_var)` | 安全分配内存，失败时跳转到清理标签 |
| `AGENTOS_SAFE_CALLOC(var, size, cleanup_label, error_var)` | 安全分配并清零内存，失败时跳转 |
| `AGENTOS_LOG_ERROR_AND_RETURN(error_code, fmt, ...)` | 记录错误并返回 |

### 资源管理宏

| 宏 | 说明 |
|------|------|
| `AGENTOS_RESOURCE_GUARD_SCOPE_BEGIN()` | RAII 风格作用域开始 |
| `AGENTOS_RESOURCE_GUARD_SCOPE_END()` | RAII 风格作用域结束 |
| `AGENTOS_AUTO_FREE(ptr)` | 自动释放内存（使用 GCC `cleanup` 属性） |
| `AGENTOS_AUTO_CLOSE(fd)` | 自动关闭文件描述符（使用 GCC `cleanup` 属性） |
| `AGENTOS_SAFE_FREE(ptr)` | 安全释放内存并置为 NULL |

### 数值验证函数

| 函数 | 说明 |
|------|------|
| `agentos_validate_non_negative(value)` | 验证数值是否非负 |
| `agentos_validate_positive(value)` | 验证数值是否为正数 |
| `agentos_validate_percentage(value)` | 验证是否为有效百分比 [0, 100] |
| `agentos_validate_probability(value)` | 验证是否为有效概率 [0, 1] |
| `agentos_validate_priority(priority, min, max)` | 验证优先级是否在有效范围内 |

### 边界检查函数

| 函数 | 说明 |
|------|------|
| `safe_add_int(a, b, result)` | 安全整数加法（检测溢出），返回 0 成功 |
| `safe_mul_int(a, b, result)` | 安全整数乘法（检测溢出），返回 0 成功 |
| `safe_add_size(a, b, result)` | 安全 size_t 加法（检测溢出），返回 0 成功 |
| `safe_mul_size(a, b, result)` | 安全 size_t 乘法（检测溢出），返回 0 成功 |
| `is_safe_array_access(index, size)` | 检查数组访问是否安全 |
| `is_safe_ptr_offset(ptr, offset, size)` | 检查指针偏移是否安全 |
| `is_safe_str_copy(src, dest, dest_size)` | 检查字符串拷贝是否安全 |

### 内存安全辅助函数

| 函数 | 说明 |
|------|------|
| `safe_memcpy(dest, dest_size, src, src_size)` | 安全内存复制（带边界检查） |
| `safe_memset(dest, dest_size, value, count)` | 安全内存设置（带边界检查） |
| `safe_strcpy(dest, dest_size, src)` | 安全字符串复制（含终止符空间检查） |
| `safe_strcat(dest, dest_size, src)` | 安全字符串拼接（含长度检查） |
| `safe_strlen(str)` | 安全字符串长度获取（NULL 返回 0） |
| `safe_strcmp(str1, str2)` | 安全字符串比较（NULL 视为空字符串） |

### 类型转换安全函数

| 函数 | 说明 |
|------|------|
| `safe_int_to_size(value, result)` | 安全 int 到 size_t 转换（检查负数） |
| `safe_size_to_int(value, result)` | 安全 size_t 到 int 转换（检查溢出） |
| `safe_double_to_int(value, result)` | 安全 double 到 int 转换（检查范围） |

## 使用示例

```c
#include "agentos_quality.h"

// ===== 输入验证 =====
agentos_error_t process_data(const char *name, int *values, size_t count) {
    // 检查指针非空
    AGENTOS_CHECK_NULL(name, AGENTOS_EINVAL);
    AGENTOS_CHECK_NULL(values, AGENTOS_EINVAL);

    // 检查字符串非空
    AGENTOS_CHECK_EMPTY(name, AGENTOS_EINVAL);

    // 检查字符串长度
    AGENTOS_CHECK_STR_LEN(name, 256, AGENTOS_EINVAL);

    // 检查范围
    AGENTOS_CHECK_RANGE(count, 1, 1024, AGENTOS_EINVAL);

    // 处理数据...
    return AGENTOS_SUCCESS;
}

// ===== 错误处理 + goto 清理模式 =====
agentos_error_t allocate_and_process(const char *path) {
    int err = AGENTOS_SUCCESS;
    char *buffer = NULL;
    FILE *file = NULL;

    AGENTOS_SAFE_ALLOC(buffer, 4096, cleanup, err);

    file = fopen(path, "r");
    AGENTOS_CHECK_NULL_GOTO(file, cleanup, AGENTOS_EIO);

    // 处理文件...
    fread(buffer, 1, 4096, file);

cleanup:
    if (file) fclose(file);
    AGENTOS_SAFE_FREE(buffer);
    return err;
}

// ===== 边界检查 =====
int result;
if (safe_add_int(a, b, &result) == 0) {
    printf("Safe addition: %d\n", result);
} else {
    fprintf(stderr, "Integer overflow detected!\n");
}

// ===== 安全字符串操作 =====
char dest[64];
if (safe_strcpy(dest, sizeof(dest), user_input) == 0) {
    printf("Safe copy: %s\n", dest);
}

// ===== RAII 自动释放 =====
{
    AGENTOS_RESOURCE_GUARD_SCOPE_BEGIN();
    char *data = malloc(1024);
    AGENTOS_AUTO_FREE(data);
    // ... 使用 data ...
    // 作用域结束时 data 自动释放
    AGENTOS_RESOURCE_GUARD_SCOPE_END();
}
```

## 设计原则

| 原则 | 说明 |
|------|------|
| E-1 安全内生 | 所有输入在进入系统前必须经过验证，白名单优于黑名单 |
| E-3 资源确定性 | 资源分配和释放成对出现，RAII 模式确保无泄漏 |
| E-6 错误可追溯 | 统一的错误码体系，错误信息可追踪到具体文件和行号 |

## 兼容别名

为兼容旧代码，以下别名保留：

| 别名 | 目标 |
|------|------|
| `agentos_safe_strcpy(dest, dest_size, src)` | `safe_strcpy(dest, dest_size, src)` |
| `agentos_safe_strcat(dest, dest_size, src)` | `safe_strcat(dest, dest_size, src)` |

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `error.h` | 统一错误码定义（`AGENTOS_SUCCESS`、`AGENTOS_EINVAL` 等） |
| `<stddef.h>` | `size_t` 类型定义 |
| `<stdint.h>` | 定长整数类型 |

---

© 2026 SPHARX Ltd. All Rights Reserved.