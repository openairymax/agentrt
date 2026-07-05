# MemoryRovol — 商业记忆桥接层

`agentrt/atoms/memoryrovol/`

**版本**: v0.1.0

---

## 概述

MemoryRovol 是 AgentRT 的商业桥接层，负责将外部 MemoryRovol PRO 仓库集成到 AgentRT 构建体系中。MemoryRovol PRO 实现 L1-L4 全功能记忆系统（原始存储、特征索引、结构绑定、模式识别），通过实现 `agentrt_memory_provider_t` 接口无缝替换内置免费提供商。

本模块不包含 MemoryRovol 的源代码，而是作为**构建桥接层**，通过 CMake 配置定位外部 MemoryRovol 仓库、注入 AgentRT 兼容头文件、并以 INTERFACE 库形式将其链接到 AgentRT atoms 聚合目标。

---

## 架构定位

```
AgentRT 八层架构：
  Atoms → Commons → Cupolas → Heapstore → Gateway → Protocols → Daemon → OpenLab + Toolkit SDK

Atoms 层记忆架构：
  engine.c → agentrt_memory_provider_t*
                ├── builtin_provider  (免费，L1+L2 基础)
                └── MemoryRovol       (商业，L1-L4 全功能)

MemoryRovol 桥接层在构建时的工作：
  CMakeLists.txt → 定位外部源码 → add_subdirectory → agentrt_memoryrovol_bridge → agentrt_atoms
```

MemoryRovol 模块位于 Atoms 层，与 memory 模块并列。它不提供运行时代码，而是解决**构建时集成**问题：如何将一个独立仓库的代码编译进 AgentRT 的构建树，并确保其能正确使用 AgentRT 的内存分配、日志和平台抽象 API。

---

## 设计理念

| 原则 | 说明 |
|------|------|
| **构建桥接** | 本模块仅负责构建集成，不包含运行时逻辑 |
| **可选启用** | 默认禁用，需显式 `-DAGENTRT_WITH_MEMORYROVOL=ON` 启用 |
| **优雅降级** | 外部源码未找到时回退到内置提供商，不中断构建 |
| **PRO 模式强制** | 集成时强制 `MEMORYROVOL_OSS=OFF`，确保完整功能 |
| **头文件注入** | 通过 force-include 机制为外部源码注入 AgentRT 兼容头文件 |

---

## 目录结构

```
memoryrovol/
├── CMakeLists.txt                # 桥接构建配置
├── src/
│   └── agentrt_mr_force_includes.h   # 强制包含头文件（注入 AgentRT 兼容性）
└── README.md
```

---

## 核心组件

### 1. 桥接构建配置 (`CMakeLists.txt`)

CMake 桥接脚本，负责定位外部 MemoryRovol 源码并将其集成到 AgentRT 构建树。

#### 启用条件

默认禁用，需通过 CMake 选项显式启用：

```bash
cmake -DAGENTRT_WITH_MEMORYROVOL=ON ...
```

未启用时输出：`MemoryRovol integration disabled`

#### 源码定位策略

按以下优先级搜索 MemoryRovol 外部仓库：

| 优先级 | 来源 | 说明 |
|--------|------|------|
| 1 | `MEMORYROVOL_DIR` CMake 变量 | 命令行 `-DMEMORYROVOL_DIR=/path/to/MemoryRovol` |
| 2 | `MEMORYROVOL_ROOT` 环境变量 | `export MEMORYROVOL_ROOT=/path/to/MemoryRovol` |
| 3 | 兄弟目录 | 相对于 AgentRT 根目录的 `../MemoryRovol`（即 `OpenAirymax/MemoryRovol`） |

搜索流程：

```
1. 检查 MEMORYROVOL_DIR cmake 变量
   ↓ 未设置
2. 检查 MEMORYROVOL_ROOT 环境变量
   ↓ 未设置
3. 计算 AgentRT 根目录的父级 → 检查 OpenAirymax/MemoryRovol/CMakeLists.txt 是否存在
   ↓ 未找到
4. 输出 WARNING，回退到内置提供商
```

#### 优雅降级

当源码未找到时：

- 输出 WARNING 提示设置 `MEMORYROVOL_DIR` 或 `MEMORYROVOL_ROOT`
- 强制设置 `AGENTRT_WITH_MEMORYROVOL=OFF`
- 提前返回，不影响后续构建

#### PRO 模式配置

集成时强制覆盖 MemoryRovol 的构建选项：

| 选项 | 值 | 说明 |
|------|-----|------|
| `MEMORYROVOL_OSS` | `OFF` | 禁用开源模式，启用 L1-L4 全功能 |
| `MEMORYROVOL_EMBEDDED` | `ON` | 标记为嵌入式构建（在 AgentRT 树内） |

#### 构建产物

| 目标 | 类型 | 说明 |
|------|------|------|
| `agentrt_memoryrovol` | 由外部仓库定义 | MemoryRovol 核心库（通过 `add_subdirectory` 引入） |
| `agentrt_memoryrovol_bridge` | INTERFACE | 桥接接口库，链接 `agentrt_memoryrovol` 并传播头文件路径 |
| `agentrt_atoms` | INTERFACE | Atoms 聚合目标，自动链接 `agentrt_memoryrovol` |

#### 传播的 AgentRT 路径

以下 AgentRT 路径被传播到 MemoryRovol 构建：

| CMake 变量 | 路径 | 说明 |
|-------------|------|------|
| `AGENTRT_ROOT` | AgentRT 工作区根目录 | 供 MemoryRovol 定位 AgentRT 资源 |
| `AGENTRT_INCLUDE_DIR` | `agentos/` 目录 | AgentRT 头文件根目录 |
| `AGENTRT_ATOMS_DIR` | `agentrt/atoms/` 目录 | Atoms 源码目录 |

#### 头文件搜索路径

为 `agentrt_memoryrovol` 目标注入以下 AgentRT 头文件目录：

- `commons/utils/memory/include` — `AGENTRT_MALLOC/CALLOC/FREE`
- `commons/utils/logging/include` — `service_log_output_record`
- `commons/utils/include` — 通用工具
- `commons/platform/include` — `agentrt_sleep_ms`, `agentrt_thread_*`
- `daemons/common/include` — 守护进程公共接口

---

### 2. 强制包含头文件 (`agentrt_mr_force_includes.h`)

解决外部 MemoryRovol 源码缺少 AgentRT 头文件 `#include` 的问题。

#### 包含内容

```c
#include "../../commons/utils/memory/include/memory_compat.h"  // AGENTRT_MALLOC/CALLOC/FREE
#include "../../commons/platform/include/platform.h"            // agentrt_thread_*, agentrt_sleep_ms
```

条件编译：

```c
#ifdef AGENTRT_USE_SCHEDULER_THREAD_IMPL
int agentrt_thread_create(agentrt_thread_t *thread, agentrt_thread_func_t func, void *arg);
int agentrt_thread_join(agentrt_thread_t thread, void **retval);
#endif
```

#### 注入机制

通过 GCC/Clang 的 `-include` 编译选项实现，在编译每个 MemoryRovol 源文件之前自动包含此头文件：

```cmake
target_compile_options(agentrt_memoryrovol PRIVATE
    "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/src/agentrt_mr_force_includes.h"
)
```

这使得 MemoryRovol 源码中可以直接使用 `AGENTRT_MALLOC`、`AGENTRT_CALLOC`、`AGENTRT_FREE` 等宏以及平台线程 API，而无需在每个文件中显式 `#include`。

---

## 构建说明

### 启用 MemoryRovol 集成

```bash
# 方式一：MemoryRovol 位于默认兄弟目录 (OpenAirymax/MemoryRovol)
cmake -B build -DAGENTRT_WITH_MEMORYROVOL=ON
cmake --build build

# 方式二：指定 MemoryRovol 源码路径
cmake -B build -DAGENTRT_WITH_MEMORYROVOL=ON -DMEMORYROVOL_DIR=/path/to/MemoryRovol
cmake --build build

# 方式三：通过环境变量指定
export MEMORYROVOL_ROOT=/path/to/MemoryRovol
cmake -B build -DAGENTRT_WITH_MEMORYROVOL=ON
cmake --build build
```

### 不启用 MemoryRovol（默认）

```bash
cmake -B build
cmake --build build
# 将使用内置免费提供商 (builtin_provider)
```

### 构建验证

启用后，CMake 配置阶段应输出：

```
-- MemoryRovol source found: /path/to/MemoryRovol
-- MemoryRovol integrated into AgentRT build (PRO mode)
```

若源码未找到，将输出 WARNING 并回退：

```
CMake Warning: MemoryRovol source not found. Set MEMORYROVOL_DIR or MEMORYROVOL_ROOT environment variable. Falling back to builtin provider only.
```

---

## 与其他模块的关系

| 模块 | 关系 |
|------|------|
| **memory** | 提供提供商接口定义（`agentrt_memory_provider_t`），MemoryRovol 实现同一接口 |
| **commons/memory_compat** | 提供 `AGENTRT_MALLOC/CALLOC/FREE/STRDUP/REALLOC` 宏，通过 force-include 注入 |
| **commons/platform** | 提供平台线程和睡眠 API，通过 force-include 注入 |
| **commons/logging** | 提供服务日志接口，通过 include 目录传播 |
| **daemons/common** | 提供守护进程公共接口，通过 include 目录传播 |
| **agentrt_atoms** | Atoms 聚合目标，MemoryRovol 自动链接入此目标 |

---

## 集成流程图

```
┌─────────────────────────────────────────────────────────┐
│  CMake 配置阶段                                         │
│                                                         │
│  AGENTRT_WITH_MEMORYROVOL=ON?                           │
│       │                                                 │
│    NO → 跳过，使用 builtin_provider                     │
│       │                                                 │
│      YES                                                │
│       │                                                 │
│  定位 MemoryRovol 源码                                   │
│  (MEMORYROVOL_DIR → ENV → 兄弟目录)                     │
│       │                                                 │
│  找到? ── NO → WARNING, 回退 builtin_provider           │
│       │                                                 │
│      YES                                                │
│       │                                                 │
│  设置 PRO 模式 (OSS=OFF, EMBEDDED=ON)                   │
│       │                                                 │
│  add_subdirectory(MemoryRovol)                          │
│       │                                                 │
│  注入 AgentRT 头文件路径 + force-include                 │
│       │                                                 │
│  创建 agentrt_memoryrovol_bridge INTERFACE               │
│       │                                                 │
│  链接到 agentrt_atoms                                   │
└─────────────────────────────────────────────────────────┘
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
