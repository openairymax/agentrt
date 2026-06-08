# Error — 错误处理模块

**模块路径**: `agentos/commons/utils/error/`
**版本**: v0.0.5

## 概述

Error 模块提供 AgentOS 统一的错误处理框架，涵盖错误码定义、错误链追踪、多语言错误描述和便捷的检查宏。该模块是 AgentOS 中所有组件错误报告的基础设施，遵循"错误可追溯"原则，支持从内核层到服务层的分层错误码体系。

## 设计目标

- **统一错误码**：所有错误码为负值，成功为 0（`AGENTOS_OK`），分段管理避免冲突
- **错误链追踪**：支持最多 16 层深度的错误上下文记录，包含源文件、行号、函数名和时间戳
- **线程安全**：所有公共接口均为线程安全，每线程独立错误链
- **多语言支持**：内建 8 种语言的错误描述（中/英/日/韩/德/法等）
- **便捷宏**：提供 `AGENTOS_ERROR`、`AGENTOS_CHECK`、`AGENTOS_PROPAGATE` 等宏减少样板代码

## 目录结构

```
error/
├── include/
│   ├── error.h                  # 错误处理框架核心接口定义
│   └── error_compat.h           # 向后兼容层（映射旧模块错误码）
├── src/
│   └── handler.c                # 错误处理实现
└── README.md                    # 本文档
```

## 核心数据结构

### 错误码分段

| 范围 | 说明 |
|------|------|
| `-1 ~ -99` | 通用基础错误（`AGENTOS_ERR_INVALID_PARAM`、`AGENTOS_ERR_OUT_OF_MEMORY` 等） |
| `-100 ~ -199` | 系统与平台错误（`AGENTOS_ERR_SYS_NOT_INIT`、`AGENTOS_ERR_SYS_THREAD` 等） |
| `-200 ~ -299` | 内核层错误（`AGENTOS_ERR_KERN_IPC`、`AGENTOS_ERR_KERN_TASK` 等） |
| `-300 ~ -399` | 服务层错误（`AGENTOS_ERR_SVC_NOT_READY`、`AGENTOS_ERR_SVC_CONFIG` 等） |
| `-400 ~ -499` | LLM/AI 服务错误（`AGENTOS_ERR_LLM_NO_PROVIDER`、`AGENTOS_ERR_LLM_RATE_LIMIT` 等） |
| `-500 ~ -599` | 执行/工具错误（`AGENTOS_ERR_EXEC_TIMEOUT`、`AGENTOS_ERR_EXEC_SANDBOX` 等） |
| `-600 ~ -699` | 记忆/存储错误（`AGENTOS_ERR_MEM_WRITE`、`AGENTOS_ERR_MEM_FULL` 等） |
| `-700 ~ -799` | 安全/沙箱错误（`AGENTOS_ERR_SEC_VIOLATION`、`AGENTOS_ERR_SEC_PATH_TRAV` 等） |
| `-800 ~ -899` | 协调/规划错误（`AGENTOS_ERR_COORD_PLAN_FAIL`、`AGENTOS_ERR_COORD_RETRY_EXCEED` 等） |

### 常用错误码

| 宏 | 值 | 说明 |
|------|------|------|
| `AGENTOS_OK` | `0` | 操作成功 |
| `AGENTOS_ERR_UNKNOWN` | `-1` | 未知错误 |
| `AGENTOS_ERR_INVALID_PARAM` | `-2` | 无效参数 |
| `AGENTOS_ERR_NULL_POINTER` | `-3` | 空指针 |
| `AGENTOS_ERR_OUT_OF_MEMORY` | `-4` | 内存不足 |
| `AGENTOS_ERR_NOT_FOUND` | `-6` | 未找到 |
| `AGENTOS_ERR_TIMEOUT` | `-8` | 超时 |
| `AGENTOS_ERR_PERMISSION_DENIED` | `-10` | 权限不足 |
| `AGENTOS_ERR_IO` | `-11` | I/O 错误 |

### agentos_error_severity_t — 错误严重程度

| 枚举值 | 说明 |
|------|------|
| `AGENTOS_ERR_SEVERITY_INFO` | 信息 |
| `AGENTOS_ERR_SEVERITY_WARNING` | 警告 |
| `AGENTOS_ERR_SEVERITY_ERROR` | 错误 |
| `AGENTOS_ERR_SEVERITY_CRITICAL` | 严重 |

### agentos_error_chain_t — 错误链

| 字段 | 类型 | 说明 |
|------|------|------|
| `code` | `agentos_error_t` | 错误码 |
| `depth` | `int` | 当前链深度 |
| `contexts` | `agentos_error_context_entry_t[16]` | 错误上下文条目数组 |

### agentos_error_context_entry_t — 错误上下文条目

| 字段 | 类型 | 说明 |
|------|------|------|
| `file` | `const char *` | 源文件名 |
| `line` | `int` | 行号 |
| `function` | `const char *` | 函数名 |
| `message` | `const char *` | 错误消息 |
| `error_code` | `agentos_error_t` | 错误码 |
| `timestamp_ns` | `uint64_t` | 纳秒时间戳 |

## 接口说明

### 基础错误处理

| 函数 | 说明 |
|------|------|
| `agentos_error_str(code)` | 获取错误码的可读描述 |
| `agentos_error_get_severity(code)` | 获取错误码的严重程度 |
| `agentos_error_get_chain()` | 获取当前线程的错误链 |
| `agentos_error_clear()` | 清除当前线程的错误链 |
| `agentos_error_push_ex(code, file, line, func, fmt, ...)` | 推送错误上下文到错误链 |
| `agentos_error_print_chain(chain)` | 打印错误链（调试用） |
| `agentos_error_chain_to_json(chain)` | 将错误链转为 JSON 字符串 |

### 错误链增强

| 函数 | 说明 |
|------|------|
| `agentos_error_chain_iter_init(chain, iter)` | 初始化错误链迭代器 |
| `agentos_error_chain_iter_next(iter)` | 获取下一个错误上下文条目 |
| `agentos_error_chain_get_depth(chain)` | 获取错误链深度 |
| `agentos_error_chain_get_root_error(chain)` | 获取最早的错误码 |
| `agentos_error_chain_get_latest_error(chain)` | 获取最新的错误码 |
| `agentos_error_chain_format(chain, lang)` | 格式化错误链为可读字符串 |

### 多语言支持

| 函数 | 说明 |
|------|------|
| `agentos_error_set_language(lang)` | 设置当前语言环境 |
| `agentos_error_get_language()` | 获取当前语言环境 |
| `agentos_error_str_i18n(code, lang)` | 获取错误码的本地化描述 |
| `agentos_error_register_i18n(entries, count)` | 注册自定义错误码的本地化描述 |

### 便捷宏

| 宏 | 说明 |
|------|------|
| `AGENTOS_ERROR(code, msg)` | 设置错误并返回 |
| `AGENTOS_ERROR_FMT(code, fmt, ...)` | 设置格式化错误并返回 |
| `AGENTOS_CHECK(cond, code, msg)` | 条件检查，失败时返回错误 |
| `AGENTOS_CHECK_NULL(ptr, name)` | 空指针检查 |
| `AGENTOS_CHECK_ALLOC(ptr)` | 内存分配检查 |
| `AGENTOS_PROPAGATE(expr)` | 错误传播（自动记录传播路径） |
| `AGENTOS_TRY(expr)` | 错误检查并返回（不记录额外上下文） |

### 错误统计

| 函数 | 说明 |
|------|------|
| `agentos_error_get_stats(stats)` | 获取全局错误统计 |
| `agentos_error_reset_stats()` | 重置全局错误统计 |

## 使用示例

```c
#include "error.h"

// 基本错误处理
agentos_error_t do_something(void *ptr) {
    AGENTOS_CHECK_NULL(ptr, "ptr");
    // 等价于: if (ptr == NULL) { AGENTOS_ERROR(AGENTOS_ERR_NULL_POINTER, "ptr is NULL"); }

    if (some_condition_fails) {
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "Invalid parameter");
    }

    return AGENTOS_OK;
}

// 错误传播
agentos_error_t caller() {
    AGENTOS_PROPAGATE(do_something(NULL));
    // 自动记录: Propagated from do_something(NULL)
    return AGENTOS_OK;
}

// 带格式化消息的错误
agentos_error_t validate(size_t size) {
    if (size > MAX_SIZE) {
        AGENTOS_ERROR_FMT(AGENTOS_ERR_OVERFLOW,
                          "Size %zu exceeds maximum %zu", size, (size_t)MAX_SIZE);
    }
    return AGENTOS_OK;
}

// 错误链调试
agentos_error_chain_t *chain = agentos_error_get_chain();
if (chain->depth > 0) {
    agentos_error_print_chain(chain);
    char *json = agentos_error_chain_to_json(chain);
    printf("Error chain: %s\n", json);
    free(json);
    agentos_error_clear();
}
```

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `types/include/types.h` | 基础类型定义（`agentos_timestamp_t` 等） |
| `stddef.h` | 标准类型定义 |
| `stdint.h` | 固定宽度整数类型 |

---

© 2026 SPHARX Ltd. All Rights Reserved.