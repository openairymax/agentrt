# Compat — 跨版本/跨平台兼容性适配

**模块路径**: `agentrt/commons/utils/compat/`
**版本**: v0.1.0

## 概述

Compat 是 AgentRT 的跨平台兼容性适配层，提供编译器兼容性定义、平台抽象宏、位操作工具、共享库导出控制以及 POSIX 缺失头文件的兼容实现。该模块确保 AgentRT 代码在 GCC、Clang、MSVC 三大编译器以及 Linux、Windows、macOS 三大平台上均能一致编译和运行。

## 设计目标

- **编译器兼容**：统一 MSVC、GCC、Clang 的编译器特性和内建函数差异
- **平台抽象宏**：提供 `AGENTRT_API`、`AGENTRT_UNUSED`、`AGENTRT_LIKELY` 等跨编译器宏
- **POSIX 兼容**：为 Windows 平台提供缺失的 POSIX 头文件（`unistd.h`、`dirent.h`、`netdb.h`、`sys/mman.h`）
- **类型兼容**：为 Windows 提供 `ssize_t` 等缺失类型定义
- **位操作**：提供编译器优化的位操作工具（`popcount`、`clz`、`ctz`、`ffs` 等）

## 目录结构

```
compat/
├── include/
│   ├── compat.h                 # 编译器兼容性、平台抽象宏、导出宏、位操作
│   ├── agentrt_dirent.h         # POSIX dirent 兼容（Windows 替代实现）
│   ├── agentrt_mman.h           # POSIX sys/mman 兼容（内存映射）
│   ├── unistd.h                 # POSIX unistd 兼容（Windows 替代实现）
│   └── netdb.h                  # POSIX netdb 兼容（网络数据库）
├── src/
│   ├── compat.c                 # 兼容性实现
│   └── compat2.c                # 扩展兼容性实现
└── README.md                    # 本文档
```

## 核心组件

### 1. 编译器检测 (`compat.h`)

自动识别编译器并定义对应宏：

| 宏 | 编译器 |
|-----|--------|
| `AGENTRT_COMPILER_GCC` | GCC |
| `AGENTRT_COMPILER_CLANG` | Clang / LLVM |
| `AGENTRT_COMPILER_MSVC` | Microsoft Visual C++ |

### 2. 平台检测

| 宏 | 平台 |
|-----|------|
| `AGENTRT_PLATFORM_LINUX` | Linux |
| `AGENTRT_PLATFORM_WINDOWS` | Windows |
| `AGENTRT_PLATFORM_MACOS` | macOS (Darwin) |

### 3. 导出控制宏

| 宏 | 说明 |
|-----|------|
| `AGENTRT_API` | 公共 API 导出（`__declspec(dllexport)` / `__attribute__((visibility("default")))`） |
| `AGENTRT_LOCAL` | 内部符号隐藏（`__attribute__((visibility("hidden")))`） |

### 4. 编译器属性宏

| 宏 | 说明 |
|-----|------|
| `AGENTRT_UNUSED` | 标记未使用变量（`__attribute__((unused))` / `(void)` 转换） |
| `AGENTRT_NORETURN` | 标记不返回函数（`__attribute__((noreturn))` / `__declspec(noreturn)`） |
| `AGENTRT_LIKELY(x)` | 分支预测优化 — 很可能为真（`__builtin_expect`） |
| `AGENTRT_UNLIKELY(x)` | 分支预测优化 — 很可能为假（`__builtin_expect`） |
| `AGENTRT_INLINE` | 强制内联（`__attribute__((always_inline))` / `__forceinline`） |
| `AGENTRT_DEPRECATED(msg)` | 标记废弃函数（`__attribute__((deprecated))` / `__declspec(deprecated)`） |

### 5. 位操作工具

| 函数 | 说明 |
|------|------|
| `agentrt_popcount32(x)` | 32 位整数中 1 的个数 |
| `agentrt_popcount64(x)` | 64 位整数中 1 的个数 |
| `agentrt_clz32(x)` | 前导零计数 |
| `agentrt_ctz32(x)` | 尾随零计数 |
| `agentrt_ffs32(x)` | 最低有效位位置 |
| `agentrt_rotl32(x, n)` | 循环左移 |
| `agentrt_rotr32(x, n)` | 循环右移 |

### 6. POSIX 兼容头文件

为 Windows 平台提供缺失的 POSIX 头文件实现：

| 头文件 | 说明 |
|--------|------|
| `unistd.h` | 提供 `sleep()`、`usleep()`、`getpid()` 等 POSIX 函数 |
| `agentrt_dirent.h` | 提供 `opendir()`、`readdir()`、`closedir()` 目录遍历 |
| `netdb.h` | 提供 `gethostbyname()`、`getaddrinfo()` 等网络数据库函数 |
| `agentrt_mman.h` | 提供 `mmap()`、`munmap()` 等内存映射函数 |

## 使用示例

```c
#include "compat.h"

/* 导出公共 API */
AGENTRT_API int agentrt_init(void);

/* 分支预测优化 */
if (AGENTRT_LIKELY(ptr != NULL)) {
    /* 快速路径 */
    process(ptr);
} else {
    /* 慢速路径 */
    handle_error();
}

/* 标记未使用变量 */
void callback(void *ctx, AGENTRT_UNUSED int event_id) {
    /* ctx 被使用，event_id 未使用 */
    process(ctx);
}

/* 位操作 */
uint32_t ones = agentrt_popcount32(0x0F0F0F0F);  /* 16 */
uint32_t leading_zeros = agentrt_clz32(0x0000FFFF); /* 16 */
```

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `atomic_compat.h` | 跨平台原子操作兼容层 |
| C 标准库 | `<stddef.h>`、`<stdint.h>`、`<stdbool.h>` |
| Windows SDK（Windows） | `<BaseTsd.h>`、`<WinSock2.h>`、`<direct.h>` 等 |

> Compat 模块是 Commons 最底层的兼容性基础，被所有其他模块通过 `#include "compat.h"` 间接依赖。

---

© 2026 SPHARX Ltd. All Rights Reserved.