# CMake — agentrt 管理仓构建系统模块

> **模块路径**: `agentrt/cmake/` | **版本**: v1.1.0 | **归属**: agentrt 管理仓直属
>
> v1.1.0（v0.1.2 决策）：cmake/ 自伞仓直属迁移至 agentrt 管理仓。构建系统属
> IRON-9 四层共享模型 [IND] 完全独立层——agentrt（用户态）与 agentrt-linux
> （内核态）构建系统各自独立，不共享；独立 clone agentrt 仓即可完整构建。
> 迁移前伞仓 cmake/ 声称"agentrt-linux 亦消费"，与实际不符（agentrt-linux
> 各 CMakeLists 均未 include 伞仓模块），迁移后文档与实现一致。

## 概述

`cmake/` 是 agentrt 管理仓的 CMake 构建系统模块，包含 8 个文件（5 个 `.cmake` 模块 + 1 个 `.cmake.in` 模板 + 1 个 `.sh.in` 模板 + 1 个 `.h` 预包含头），为 agentrt 管理仓及其 7 个叶子仓（atoms / commons / cupolas / daemons / gateway / heapstore / protocols）提供统一的编译器配置、平台检测、依赖查找、运行时检测和构建日志输出。agentrt 各 CMakeLists.txt 通过 `include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/airy_print.cmake)` 等相对路径引用此目录。

### 设计目标

- **统一管理**：避免在子模块 CMakeLists.txt 中散落 `add_compile_options` / `find_package` 调用
- **跨平台**：同时支持 MSVC / GCC / Clang 三大编译器，Linux / macOS / Windows 三平台
- **安全优先**：内置 ASan / LSan / UBSan / TSan / 栈保护 / FORTIFY_SOURCE 运行时检测
- **可观测**：构建输出统一使用 ANSI 彩色编码，与运行时日志系统（log_write）对齐
- **可重用**：所有逻辑封装为 CMake 函数，子模块通过 `include()` 按需引入

## 文件清单

```
cmake/
├── README.md                    # 本文件
├── compilerflags.cmake          # 编译器标志统一配置（安全编译选项 / 警告 / 优化 / LTO / 覆盖率）
├── platform.cmake               # 平台检测与 POSIX 特性宏配置（Linux / macOS / Windows）
├── dependencies.cmake           # 统一依赖查找（必需: Threads；可选: SQLite3 / cJSON / YAML / OpenSSL / CURL / 等）
├── sanitizers.cmake             # 运行时检测（ASan / LSan / UBSan / TSan / 栈保护 / FORTIFY_SOURCE）
├── utils.cmake                  # 构建期打印工具（ANSI 彩色编码：OK/INFO/WARN/ERROR/FATAL/DEBUG/SECTION）
├── airy_print.cmake             # AgentRT 统一构建打印系统（兼容旧版名称，与 utils.cmake 功能一致）
├── AirymaxRTConfig.cmake.in     # CMake 包配置模板（供 find_package(AirymaxRT) 使用）
├── ctest_wrapper.sh.in          # 测试运行包装脚本（自动设置 sanitizer 环境变量）
└── windows_preinclude.h         # Windows MSVC 预包含头（POSIX 兼容层：__builtin_* 映射 / ssize_t / 原子操作）
```

## 核心模块说明

### 1. compilerflags.cmake — 编译器标志统一配置

**版本**: 1.0.0 | **创建**: 2026-07-06

统一管理 MSVC / GCC / Clang 三大编译器的安全编译选项、警告级别、调试符号、优化级别、代码覆盖率和 LTO 配置。

| 函数 | 说明 |
|------|------|
| `airy_apply_compiler_flags()` | 应用基础安全编译选项（MSVC: /W4 /GS /guard:cf；GCC/Clang: -Wall -Wextra -fstack-protector-strong） |
| `airy_apply_compliance_strict(BANNED_HEADER)` | 应用 SE-01 合规严格模式，全局注入 banned_functions.h |
| `airy_apply_build_type_flags()` | 应用构建类型相关选项（Debug: -g -O0；Release: -O3 -flto） |
| `airy_apply_coverage()` | 应用代码覆盖率选项（-fprofile-arcs -ftest-coverage） |
| `airy_apply_all_compiler_flags(BANNED_HEADER)` | 一键应用所有编译器配置 |

**编译选项**:

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `WARNINGS_AS_ERRORS` | OFF | 将警告视为错误 |
| `ENABLE_COVERAGE` | OFF | 启用代码覆盖率插桩 |
| `ENABLE_LTO` | ON | 启用链接时优化（Release 构建） |

### 2. platform.cmake — 平台检测与特性宏配置

**版本**: 1.0.0 | **创建**: 2026-07-06

统一检测目标平台（Linux / macOS / Windows）和编译器（GCC / Clang / MSVC），设置对应的 POSIX 特性测试宏和平台定义宏。符合 Airymax 跨 Linux/macOS/Windows 硬约束（用户态运行时，通过 libc/POSIX 跨平台）。

| 函数 | 说明 |
|------|------|
| `airy_detect_platform()` | 检测平台并设置 `AIRY_PLATFORM_LINUX/MACOS/WINDOWS` 和 `AIRY_COMPILER_GCC/CLANG/MSVC` |
| `airy_print_platform_info()` | 打印平台信息摘要 |
| `airy_is_unix_like(result_var)` | 检查是否为 Unix-like 系统 |
| `airy_supports_sanitizers(result_var)` | 检查当前编译器是否支持 sanitizers |

**POSIX 特性宏**:

| 平台 | 宏定义 |
|------|--------|
| Linux | `_POSIX_C_SOURCE=200809L` `_XOPEN_SOURCE=700` `_GNU_SOURCE` |
| macOS | `_POSIX_C_SOURCE=200112L` `_DARWIN_C_SOURCE` |

### 3. dependencies.cmake — 统一依赖查找

**版本**: 1.0.0 | **创建**: 2026-07-06

集中管理所有可选/必需的系统依赖查找逻辑，统一通过 `pkg-config` 或 `find_package` 查找，并设置对应的 `AIRY_HAS_*` 编译宏。

| 函数 | 说明 |
|------|------|
| `airy_find_required_deps()` | 查找必需依赖（当前仅 Threads） |
| `airy_find_optional_deps()` | 查找可选依赖（SQLite3 / cJSON / YAML / OpenSSL / CURL / libmicrohttpd / libwebsockets / libevent / FAISS） |
| `airy_find_all_deps()` | 一键查找所有依赖 |
| `airy_print_deps_summary()` | 打印依赖查找结果摘要 |

**查找的依赖**:

| 依赖 | 宏 | 用途 |
|------|-----|------|
| Threads | — | 多线程（必需） |
| SQLite3 | `AIRY_HAS_SQLITE3` | 嵌入式数据库 |
| cJSON | `AIRY_HAS_CJSON` | JSON 解析 |
| libyaml | `AIRY_HAS_YAML` | YAML 配置解析 |
| OpenSSL | `AIRY_HAS_OPENSSL` | TLS/加密 |
| libcurl | `AIRY_HAS_CURL` | HTTP 客户端 |
| libmicrohttpd | `AIRY_HAS_MICROHTTPD` | 嵌入式 HTTP 服务器 |
| libwebsockets | `AIRY_HAS_LIBWEBSOCKETS` | WebSocket |
| libevent | `AIRY_HAS_LIBEVENT` | 事件循环 |
| FAISS | `AIRY_HAS_FAISS` | 向量检索（MemoryRovol） |

### 4. sanitizers.cmake — 运行时检测

**版本**: 1.0.0 | **创建**: 2026-07-06

统一管理所有运行时检测工具的编译选项（SEC-05/06/08/09）。

| 函数 | 说明 |
|------|------|
| `airy_check_sanitizer_support()` | 检查平台是否支持 sanitizers |
| `airy_enable_asan(target scope)` | 启用 AddressSanitizer + LeakSanitizer |
| `airy_enable_ubsan(target scope)` | 启用 UndefinedBehaviorSanitizer |
| `airy_enable_tsan(target scope)` | 启用 ThreadSanitizer（与 ASan 互斥） |
| `airy_enable_stack_protector(target scope)` | 启用栈保护 (-fstack-protector-strong) |
| `airy_enable_fortify(target scope)` | 启用 FORTIFY_SOURCE=2 |
| `enable_airy_sanitizers(target [scope])` | 一键启用所有安全检测 |
| `airy_print_sanitizer_summary()` | 打印 sanitizer 配置摘要 |

**CMake 选项**:

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `AIRY_ENABLE_ASAN` | ON | AddressSanitizer + LeakSanitizer |
| `AIRY_ENABLE_UBSAN` | ON | UndefinedBehaviorSanitizer |
| `AIRY_ENABLE_TSAN` | OFF | ThreadSanitizer（与 ASan 互斥） |
| `AIRY_ENABLE_STACK_PROTECTOR` | ON | 栈保护 |
| `AIRY_ENABLE_FORTIFY` | ON | FORTIFY_SOURCE=2 |

### 5. utils.cmake — 构建期打印工具

**版本**: 1.0.0 | **创建**: 2026-07-06

与运行时日志系统（log_write）对齐，构建期所有打印输出统一格式：`[YYYY-MM-DD HH:MM:SS] [LEVEL] message`，使用 ANSI 彩色编码。

| 函数 | 颜色 | 用途 |
|------|------|------|
| `airy_print_ok(msg)` | 绿色 | 成功/确认 |
| `airy_print_info(msg)` | 蓝色 | 信息性输出 |
| `airy_print_warn(msg)` | 黄色 | 警告 |
| `airy_print_error(msg)` | 红色 | 错误（不终止） |
| `airy_print_fatal(msg)` | 品红 | 致命错误（终止构建） |
| `airy_print_debug(msg)` | 灰色 | 调试信息 |
| `airy_print_section(msg)` | 青色加粗 | 章节标题 |
| `airy_print_status(msg)` | 蓝色 | 兼容旧 `message(STATUS)` |
| `airy_print_verbose(msg)` | 灰色 | 条件输出（`AIRY_VERBOSE=1`） |
| `airy_print_build_summary()` | — | 打印构建环境摘要 |

**彩色控制**:

- 自动检测终端环境：管道/文件重定向时禁用彩色，避免 ANSI 码污染日志文件
- 可通过 `AIRY_BUILD_COLOR=1/0` 强制启用/禁用

### 6. AirymaxRTConfig.cmake.in — 包配置模板

供 `find_package(AirymaxRT CONFIG)` 使用的 CMake 包配置模板。下游项目通过此文件获取 AirymaxRT 的 include 路径和版本信息。

### 7. ctest_wrapper.sh.in — 测试运行包装脚本

自动设置 ASan / LSan / UBSan 环境变量后运行 `ctest`，确保 sanitizer 在测试时正确配置。

### 8. windows_preinclude.h — Windows MSVC 预包含头

为 Windows MSVC 编译器提供 POSIX 兼容层：

- `__builtin_*` 函数映射到标准 C 函数（`__builtin_memcpy` → `memcpy`）
- `ssize_t` 类型定义
- `PATH_MAX` / `strcasecmp` / `strdup` / `strtok_r` 等 POSIX 函数映射
- 原子操作常量定义（`__ATOMIC_RELAXED` 等）
- cJSON 桩函数（当 `AIRY_HAS_CJSON` 未定义时）

## 使用方式

在任意子模块的 CMakeLists.txt 中：

```cmake
# 添加 cmake 模块路径（agentrt 管理仓内，随仓可移植）
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# 引入需要的模块
include(platform)
include(compilerflags)
include(dependencies)
include(sanitizers)
include(utils)

# 使用
airy_detect_platform()
airy_apply_all_compiler_flags()
airy_find_all_deps()

add_executable(my_target main.c)
enable_airy_sanitizers(my_target PRIVATE)

airy_print_section("Build completed")
airy_print_ok("my_target built successfully")
```

## 上游依赖

无 — `cmake/` 是 agentrt 管理仓最底层的基础设施模块，不依赖任何其他 Airymax 模块。仅依赖 CMake ≥ 3.16 和系统的 `pkg-config`。

## 下游消费者

| 消费者 | 使用方式 |
|--------|----------|
| **agentrt** 管理仓 | 根 CMakeLists.txt `include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/airy_print.cmake)` 等 |
| **agentrt** 7 个叶子仓（atoms 等） | 经 `CMAKE_SOURCE_DIR`（= agentrt 根）相对引用 `cmake/` 模块 |

> 注意：agentrt-linux（内核态）不消费本模块——构建系统属 IRON-9 [IND] 完全独立层，agentrt-linux 有独立的 Kbuild / CMake 体系（见 agentrt-linux 管理仓）。

## 设计原则

- **agentrt 管理仓直属**：随 agentrt 仓独立演进、独立 clone 即可完整构建；通过 `cmake/` 相对路径引用（v4.1 起不再依赖伞仓）
- **函数封装**：所有逻辑封装为可重用的 CMake 函数，避免全局副作用
- **include_guard(GLOBAL)**：所有模块使用全局 include guard，防止重复加载
- **跨平台**：同时支持 MSVC / GCC / Clang，Linux / macOS / Windows
- **安全默认**：sanitizers 默认启用（TSan 除外），安全编译选项默认开启

## 许可证

Copyright (c) 2025-2026 SPHARX Ltd. All Rights Reserved.

双许可证：**AGPL-3.0-or-later OR Apache-2.0**（SPDX: `AGPL-3.0-or-later OR Apache-2.0`）。详见 [LICENSE](../LICENSE)。

---

> **文档结束** | 1.0.0（5 个 .cmake 模块 + 3 个辅助文件，伞仓级构建基础设施）