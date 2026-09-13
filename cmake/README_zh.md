<!-- SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 -->
<!-- Copyright (c) 2025-2026 SPHARX Ltd. All Rights Reserved. -->

# CMake — AgentRT 构建系统模块

`agentrt/cmake/` 目录收录 AgentRT 管理仓的 CMake 构建系统模块：7 个
`.cmake` 模块、1 个 CMake 包配置模板（`.cmake.in`）、1 个测试包装脚本
模板（`.sh.in`）与 1 个 Windows 预包含头（`.h`），为管理仓及其 7 个叶子仓
（atoms / commons / cupolas / daemons / gateway / heapstore / protocols）
提供统一的编译器配置、平台检测、依赖查找、sanitizer 运行时检测与构建期
彩色日志输出。

要求 **CMake ≥ 3.20**（与仓库根 `CMakeLists.txt` 的 `cmake_minimum_required`
一致）。

## 设计目标

- **统一管理**：避免在各 CMakeLists.txt 中散落 `add_compile_options` /
  `find_package` 调用
- **跨平台**：同时支持 MSVC / GCC / Clang 三大编译器，Linux / macOS /
  Windows 三平台
- **安全优先**：内置 ASan / LSan / UBSan / TSan / 栈保护 / FORTIFY_SOURCE
  运行时检测
- **可观测**：构建输出统一使用 ANSI 彩色编码，与运行时日志系统
  （log_write）对齐
- **可重用**：所有逻辑封装为 CMake 函数，通过 `include()` 按需引入

## 当前接线状态

按根 `CMakeLists.txt` 的实际 include 与 configure 情况：

| 模块 | 当前是否被根构建使用 |
|------|----------------------|
| `airy_print.cmake` | **是**（根 CMakeLists 直接 include） |
| `airy_linkgate.cmake` | **是**（根 CMakeLists 直接 include，见下文） |
| `ctest_wrapper.sh.in` | 条件使用：`BUILD_TESTS=ON` 且 `ENABLE_SANITIZERS=ON` 且非 MSVC 时生成 `run_tests.sh`（`ENABLE_SANITIZERS` 默认 **OFF**） |
| `compilerflags.cmake` | 否（可复用模块库，按需 `include()`） |
| `platform.cmake` | 否（同上） |
| `dependencies.cmake` | 否（同上） |
| `sanitizers.cmake` | 否（同上） |
| `utils.cmake` | 否（同上） |
| `AirymaxRTConfig.cmake.in` | 否（供下游 `find_package(AirymaxRT)` 的包配置模板） |

## 文件清单

```
cmake/
├── README.md / README_zh.md    # 本文档（英文 / 简体中文）
├── airy_print.cmake            # AgentRT 统一构建打印系统（根构建实际使用）
├── airy_linkgate.cmake         # 模块链接白名单构建期门禁（根构建实际使用）
├── compilerflags.cmake         # 编译器标志统一配置（安全编译选项/警告/优化/LTO/覆盖率）
├── platform.cmake              # 平台检测与 POSIX 特性宏配置（Linux/macOS/Windows）
├── dependencies.cmake          # 统一依赖查找（必需：Threads；可选：SQLite3/cJSON 等）
├── sanitizers.cmake            # 运行时检测配置（ASan/LSan/UBSan/TSan/栈保护/FORTIFY）
├── utils.cmake                 # 构建期打印工具（airy_print 的超集模块库）
├── AirymaxRTConfig.cmake.in    # CMake 包配置模板（供 find_package(AirymaxRT)）
├── ctest_wrapper.sh.in         # 测试运行包装脚本（自动设置 sanitizer 环境变量）
└── windows_preinclude.h        # Windows MSVC 预包含头（POSIX 兼容层）
```

## 模块说明

### 1. airy_print.cmake — 统一构建打印系统

根构建实际使用的打印模块。构建期输出统一格式
`[YYYY-MM-DD HH:MM:SS] [LEVEL] message`，ANSI 彩色编码与运行时日志系统
（log_write）对齐；管道/文件重定向时自动禁用彩色，可用环境变量
`AIRY_BUILD_COLOR=1/0` 强制启用/禁用。

| 函数 | 颜色 | 用途 |
|------|------|------|
| `airy_print_ok(msg)` | 绿色 | 成功/确认 |
| `airy_print_info(msg)` | 蓝色 | 信息性输出 |
| `airy_print_warn(msg)` | 黄色 | 警告 |
| `airy_print_error(msg)` | 红色 | 错误（不终止构建） |
| `airy_print_fatal(msg)` | 品红 | 致命错误（终止构建） |
| `airy_print_debug(msg)` | 灰色 | 调试信息 |
| `airy_print_section(msg)` | 青色加粗 | 章节标题 |
| `airy_print_status(msg)` | 蓝色 | 兼容旧 `message(STATUS)` |

### 2. airy_linkgate.cmake — 链接白名单构建期门禁

把模块间链接关系固化为构建期断言，与 `tools/airy_depgraph` 配合工作：

- 白名单文件为仓库根 `link-whitelist.txt`（单一权威），声明
  目标 → 允许链接的库；
- `airy_linkgate_collect(TARGET_NAME WHITELIST_FILE)`：在目标定义后调用，
  收集目标实际链接（`LINK_LIBRARIES`，过滤生成器表达式与链接器选项），
  写入 `${CMAKE_BINARY_DIR}/linkgate/<target>.links.txt`；
- `airy_linkgate_install_checks()`：在 `airy_depgraph` 目标就绪后由根
  CMakeLists 调用，为每个已登记目标创建门禁 target，执行
  `airy_depgraph --links <白名单> --actual <实际链接>`；越权链接（如
  gateway 系目标链接 coreloopthree / cognition）即 fail-closed（退出码 2），
  阻断构建。

### 3. compilerflags.cmake — 编译器标志统一配置

统一管理 MSVC / GCC / Clang 三大编译器的安全编译选项、警告级别、调试
符号、优化级别、代码覆盖率和 LTO 配置。

| 函数 | 说明 |
|------|------|
| `airy_apply_compiler_flags()` | 应用基础安全编译选项（MSVC: /W4 /GS /guard:cf；GCC/Clang: -Wall -Wextra -fstack-protector-strong） |
| `airy_apply_compliance_strict(BANNED_HEADER <路径>)` | 严格合规模式：定义 `AIRY_COMPLIANCE_STRICT` 并全局注入 banned_functions.h（仅 GCC/Clang） |
| `airy_apply_build_type_flags()` | 应用构建类型相关选项（Debug: -g -O0 -fno-inline；Release: -O3，`ENABLE_LTO=ON` 时加 -flto） |
| `airy_apply_coverage()` | 应用代码覆盖率选项（-fprofile-arcs -ftest-coverage，仅 GCC/Clang） |
| `airy_apply_all_compiler_flags([BANNED_HEADER <路径>])` | 一键应用以上全部配置 |

**选项**（定义于仓库根 CMakeLists）:

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `WARNINGS_AS_ERRORS` | OFF | 将警告视为错误 |
| `ENABLE_COVERAGE` | OFF | 启用代码覆盖率插桩 |
| `ENABLE_LTO` | ON | 启用链接时优化（Release 构建） |

### 4. platform.cmake — 平台检测与特性宏配置

统一检测目标平台（Linux / macOS / Windows）和编译器（GCC / Clang / MSVC），
设置对应的 POSIX 特性测试宏和平台定义宏。

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

### 5. dependencies.cmake — 统一依赖查找

集中管理必需/可选系统依赖的查找逻辑，并设置对应的 `AIRY_HAS_*` 编译宏。

| 函数 | 说明 |
|------|------|
| `airy_find_required_deps()` | 查找必需依赖（当前仅 Threads） |
| `airy_find_optional_deps()` | 查找可选依赖（见下表） |
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
| FAISS | `AIRY_HAS_FAISS` | 向量检索（MemoryRovol 组件） |

### 6. sanitizers.cmake — 运行时检测配置

统一管理 sanitizer 与硬化选项。是否应用由仓库根选项 `ENABLE_SANITIZERS`
（默认 **OFF**）控制；启用后各子开关默认值如下。

| 函数 | 说明 |
|------|------|
| `airy_check_sanitizer_support()` | 检查平台是否支持 sanitizers |
| `airy_enable_asan(target scope)` | 启用 AddressSanitizer + LeakSanitizer |
| `airy_enable_ubsan(target scope)` | 启用 UndefinedBehaviorSanitizer |
| `airy_enable_tsan(target scope)` | 启用 ThreadSanitizer（与 ASan 互斥） |
| `airy_enable_stack_protector(target scope)` | 启用栈保护 (-fstack-protector-strong) |
| `airy_enable_fortify(target scope)` | 启用 FORTIFY_SOURCE=2 |
| `enable_airy_sanitizers(target)` | 一键启用所有安全检测 |
| `airy_print_sanitizer_summary()` | 打印 sanitizer 配置摘要 |

**选项**:

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `AIRY_ENABLE_ASAN` | ON | AddressSanitizer + LeakSanitizer |
| `AIRY_ENABLE_UBSAN` | ON | UndefinedBehaviorSanitizer |
| `AIRY_ENABLE_TSAN` | OFF | ThreadSanitizer（与 ASan 互斥） |
| `AIRY_ENABLE_STACK_PROTECTOR` | ON | 栈保护 |
| `AIRY_ENABLE_FORTIFY` | ON | FORTIFY_SOURCE=2 |

### 7. utils.cmake — 构建期打印工具（超集模块库）

包含 `airy_print.cmake` 的全部 8 个打印函数（彩色控制行为相同），并额外
提供：

| 函数 | 用途 |
|------|------|
| `airy_print_verbose(msg)` | 条件输出（环境变量 `AIRY_VERBOSE=1` 时才打印） |
| `airy_print_build_summary()` | 打印构建环境摘要 |

### 8. AirymaxRTConfig.cmake.in — 包配置模板

供下游项目 `find_package(AirymaxRT CONFIG)` 使用的 CMake 包配置模板：
暴露 AirymaxRT 的 include 路径（头文件安装于 `<prefix>/include/agentrt`）
与版本信息。

### 9. ctest_wrapper.sh.in — 测试运行包装脚本模板

配置为 `run_tests.sh`：自动设置 `ASAN_OPTIONS` / `LSAN_OPTIONS` /
`UBSAN_OPTIONS` 后执行 `ctest "$@"`，确保 sanitizer 在测试时正确生效。
仅在 `BUILD_TESTS` 且 `ENABLE_SANITIZERS`（默认 OFF）且非 MSVC 时由根
CMakeLists 生成。

### 10. windows_preinclude.h — Windows MSVC 预包含头

为 Windows MSVC 编译器提供 POSIX 兼容层：

- `__builtin_*` 函数映射到标准 C 函数（`__builtin_memcpy` → `memcpy`）
- `ssize_t` / `pid_t` 类型定义
- `PATH_MAX` / `strcasecmp` / `strdup` / `strtok_r` 等 POSIX 函数映射
- 原子操作常量定义（`__ATOMIC_RELAXED` 等）
- cJSON 桩函数（当 `AIRY_HAS_CJSON` 未定义时）

## 使用方式

根构建已自动引入 `airy_print.cmake` 与 `airy_linkgate.cmake`。其余模块为
按需引入的模块库，在 CMakeLists.txt 中：

```cmake
# 引入需要的模块（路径相对 agentrt 仓库根）
include(${CMAKE_SOURCE_DIR}/cmake/platform.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/compilerflags.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/dependencies.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/sanitizers.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

# 使用
airy_detect_platform()
airy_apply_all_compiler_flags()
airy_find_all_deps()

add_executable(my_target main.c)
enable_airy_sanitizers(my_target)

airy_print_section("Build completed")
airy_print_ok("my_target configured")
```

所有 `.cmake` 模块均使用 `include_guard(GLOBAL)`，可安全重复 include。

## 上游依赖

无 — 本目录是仓库最底层的基础设施，不依赖任何其他 Airymax 模块。仅要求
CMake ≥ 3.20；`dependencies.cmake` 的可选依赖查找另需系统 `pkg-config`。

## 许可证

Copyright (c) 2025-2026 SPHARX Ltd. All Rights Reserved.

双许可证：**AGPL-3.0-or-later OR Apache-2.0**（SPDX:
`AGPL-3.0-or-later OR Apache-2.0`）。详见 [LICENSE](../LICENSE)。
