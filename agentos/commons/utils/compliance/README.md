# Compliance — 合规性校验与策略执行

**模块路径**: `agentos/commons/utils/compliance/`
**版本**: v0.1.0

## 概述

Compliance 是 AgentRT 的代码合规性实施模块，通过禁止危险函数、强制安全 API 替代、以及合规性策略校验，确保所有 AgentRT 代码满足安全编码标准。该模块是 Commons 安全基础设施的核心，与根 CMakeLists.txt 的 `AGENTOS_COMPLIANCE_STRICT` 选项联动，在编译期强制执行安全编码规范。

## 设计目标

- **编译期安全强制**：通过 `#pragma GCC poison` 在编译期禁止危险函数的使用
- **安全 API 替代**：提供 `AGENTOS_MALLOC`、`AGENTOS_FREE`、`AGENTOS_STRCPY` 等安全替代宏
- **分级合规**：支持 Strict 模式（编译期阻断）和 Standard 模式（编译期警告）
- **可豁免**：提供 `compliance_exempt.h` 豁免机制，允许特定文件在审查后使用受限函数

## 目录结构

```
compliance/
├── include/
│   ├── banned_functions.h       # 危险函数禁止列表（Strict 模式 poison，Standard 模式 deprecated）
│   └── compliance_exempt.h      # 合规豁免声明（允许特定文件使用受限函数）
└── README.md                    # 本文档
```

## 合规模式

### Strict 模式（`AGENTOS_COMPLIANCE_STRICT=ON`）

通过 `#pragma GCC poison` 在编译期直接禁止以下函数，任何使用都会导致编译错误：

| 类别 | 被禁止的函数 | 安全替代 |
|------|-------------|----------|
| **内存分配** | `malloc`、`free`、`calloc`、`realloc` | `AGENTOS_MALLOC`、`AGENTOS_FREE`、`AGENTOS_CALLOC`、`AGENTOS_REALLOC` |
| **字符串复制** | `strcpy`、`strcat`、`strncpy`、`strdup`、`strndup` | `AGENTOS_STRCPY`、`AGENTOS_STRCAT`、`AGENTOS_STRDUP`、`AGENTOS_STRNDUP` |
| **格式化输出** | `sprintf`、`vsprintf`、`fprintf`、`asprintf`、`vasprintf` | `snprintf`、`AGENTOS_SNPRINTF` |
| **输入扫描** | `scanf`、`fscanf`、`sscanf`、`gets` | 结构化解析 API |
| **内存操作** | `memcpy`、`memmove`、`memset` | `AGENTOS_MEMCPY_SAFE`、`AGENTOS_MEMSET_SAFE` |
| **时间** | `localtime`、`gmtime` | `localtime_r`、`gmtime_r` |
| **临时文件** | `tmpnam`、`mktemp` | `mkstemp` |
| **字符串解析** | `strtok` | `strtok_r` |

### Standard 模式（默认）

通过 `__attribute__((deprecated))` 在编译期产生警告，提示开发者使用安全替代 API，但不阻断编译。

## 豁免机制

`compliance_exempt.h` 提供豁免声明，允许特定文件在经过安全审查后使用受限函数：

```c
/* 在文件开头包含豁免声明 */
#define AGENTOS_COMPLIANCE_IMPL
#include "compliance_exempt.h"

/* 此文件可以使用 malloc/free 等受限函数 */
void *ptr = malloc(1024);  /* 通过豁免，允许使用 */
```

**豁免原则**：
- 仅在实现安全包装函数本身时使用（如 `memory_compat.h` 的实现）
- 仅在第三方代码集成时使用
- 每次豁免需要代码审查批准

## 使用示例

### 启用 Strict 模式

```bash
# 构建时启用严格合规模式
cmake -B build -DAGENTOS_COMPLIANCE_STRICT=ON
cmake --build build
```

### 代码中使用安全 API

```c
/* 不安全 — Strict 模式下编译失败 */
char *buf = malloc(1024);
strcpy(buf, src);

/* 安全 — 使用 AgentRT 替代 API */
char *buf = AGENTOS_MALLOC(1024);
AGENTOS_STRCPY(buf, sizeof(buf), src);
```

### 豁免示例

```c
/* 仅限 memory_compat.h 的实现文件使用 */
#define AGENTOS_COMPLIANCE_IMPL
#include "compliance_exempt.h"

void *agentos_malloc_impl(size_t size) {
    return malloc(size);  /* 豁免允许 */
}
```

## 与 CI 集成

Compliance 模块在 CI 流水线中通过以下检查强制执行：

| 检查项 | 规则编号 | 说明 |
|--------|----------|------|
| 禁止函数注入 | BAN-71 | 验证 `banned_functions.h` 已通过 CMake 注入编译 |
| 安全函数使用 | BAN-151~BAN-180 | 通过 `forbidden_functions.sh` 脚本扫描违规使用 |
| 危险函数检测 | BAN-154 | 禁止直接使用 `memcpy`/`memmove`/`memset` |

## 依赖关系

| 依赖 | 说明 |
|------|------|
| 根 `CMakeLists.txt` | 通过 `-include` 编译选项注入 `banned_functions.h` |
| `memory_compat.h` | 提供 `AGENTOS_MALLOC`/`AGENTOS_FREE` 等安全替代宏 |
| `string_compat.h` | 提供 `AGENTOS_STRCPY`/`AGENTOS_STRDUP` 等安全字符串宏 |
| GCC/Clang 编译器 | `#pragma GCC poison` 依赖 GCC/Clang 扩展 |

---

© 2026 SPHARX Ltd. All Rights Reserved.