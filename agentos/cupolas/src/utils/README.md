# Cupolas Utils — 安全工具库

**模块路径**: `agentos/cupolas/src/utils/`
**版本**: v0.1.0

## 概述

Cupolas Utils 是 Cupolas 安全穹顶的内部工具库，提供所有 Cupolas 子模块共享的基础设施，包括安全内存管理、统一错误处理、日志桥接和编译器提示宏。该模块不对外暴露公共 API，仅供 Cupolas 内部子模块使用。

## 设计目标

- **安全内存管理**：零化释放、安全拷贝、分配跟踪
- **统一错误处理**：Cupolas 全模块共享的错误码体系
- **日志桥接**：将 Cupolas 内部日志桥接到 Commons 日志系统
- **编译器提示**：跨编译器的属性注解和分支预测宏

## 目录结构

```
utils/
├── cupolas_utils.h              # 工具库公共头文件
├── cupolas_utils.c              # 工具库实现
└── README.md                    # 本文档
```

## 核心功能

### 安全内存管理

| 宏 | 说明 |
|------|------|
| `CUPOLAS_MALLOC(size)` | 分配内存（失败时返回 NULL） |
| `CUPOLAS_CALLOC(num, size)` | 零初始化分配 |
| `CUPOLAS_FREE(ptr)` | 释放内存并将指针置 NULL |
| `CUPOLAS_SAFE_FREE(ptr)` | 安全释放（零化内存后释放） |
| `CUPOLAS_SAFE_STRDUP(str)` | 安全字符串复制 |
| `CUPOLAS_SAFE_COPY(dst, src, size)` | 安全内存拷贝（边界检查） |

### 编译器提示

| 宏 | 说明 |
|------|------|
| `CUPOLAS_LIKELY(x)` | 分支预测：x 大概率为真 |
| `CUPOLAS_UNLIKELY(x)` | 分支预测：x 大概率为假 |
| `CUPOLAS_UNUSED(x)` | 抑制未使用变量警告 |
| `CUPOLAS_ALIGNED(n)` | 内存对齐属性 |
| `CUPOLAS_PACKED` | 紧凑结构体属性 |
| `CUPOLAS_NOINLINE` | 禁止内联 |
| `CUPOLAS_HOT` | 热路径函数优化提示 |
| `CUPOLAS_COLD` | 冷路径函数优化提示 |

### 错误处理

| 宏 | 说明 |
|------|------|
| `CUPOLAS_CHECK_NULL(ptr, retval)` | NULL 检查，为空则返回 retval |
| `CUPOLAS_CHECK_PARAM(cond, retval)` | 参数校验，条件不满足则返回 retval |
| `CUPOLAS_GOTO_CLEANUP_IF(cond)` | 条件满足则跳转到 cleanup 标签 |
| `CUPOLAS_RETURN_IF_ERROR(err)` | 错误则提前返回 |

### 日志桥接

| 宏 | 说明 |
|------|------|
| `CUPOLAS_LOG_DEBUG(fmt, ...)` | DEBUG 级别日志 |
| `CUPOLAS_LOG_INFO(fmt, ...)` | INFO 级别日志 |
| `CUPOLAS_LOG_WARN(fmt, ...)` | WARN 级别日志 |
| `CUPOLAS_LOG_ERROR(fmt, ...)` | ERROR 级别日志 |

> 日志桥接宏内部调用 Commons 的 `log_write` 函数，模块名自动设为 `cupolas`。

### 安全常量

| 常量 | 值 | 说明 |
|------|-----|------|
| `CUPOLAS_MAX_PATH_LEN` | 4096 | 最大路径长度 |
| `CUPOLAS_MAX_INPUT_LEN` | 65536 | 最大输入长度 |
| `CUPOLAS_MAX_RULE_COUNT` | 1024 | 最大规则数量 |
| `CUPOLAS_MAX_CACHE_SIZE` | 4096 | 最大缓存条目数 |
| `CUPOLAS_HASH_SIZE` | 32 | SHA-256 哈希大小 |
| `CUPOLAS_HMAC_SIZE` | 32 | HMAC-SHA256 大小 |
| `CUPOLAS_AES_KEY_SIZE` | 32 | AES-256 密钥大小 |
| `CUPOLAS_AES_NONCE_SIZE` | 12 | AES-GCM Nonce 大小 |
| `CUPOLAS_AES_TAG_SIZE` | 16 | AES-GCM Tag 大小 |

## 使用示例

```c
#include "cupolas_utils.h"

char *data = CUPOLAS_MALLOC(1024);
if (CUPOLAS_UNLIKELY(data == NULL)) {
    CUPOLAS_LOG_ERROR("内存分配失败");
    return CUPOLAS_ERR_INTERNAL;
}

CUPOLAS_SAFE_COPY(data, source, 1024);

CUPOLAS_SAFE_FREE(data);

CUPOLAS_CHECK_NULL(engine, CUPOLAS_ERR_INVALID_PARAM);
CUPOLAS_CHECK_PARAM(size > 0 && size <= CUPOLAS_MAX_INPUT_LEN, CUPOLAS_ERR_INVALID_PARAM);
```

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `agentos_types.h` | 统一类型定义 |
| `memory_compat.h` | Commons 内存管理宏 |
| `logging.h` | Commons 日志系统（桥接目标） |
| `<string.h>` | 字符串操作 |
| `<stdlib.h>` | 标准库 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
