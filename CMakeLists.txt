# AgentRT - 智能体运行时平台
# 根目录构建配置文件
# Copyright (c) 2026 SPHARX. All Rights Reserved.

cmake_minimum_required(VERSION 3.20)
# 版本 SSoT：project() 版本号从根 VERSION 文件注入（与 gateway/commons/CLI
# 一致）。VERSION 允许字母后缀（如 0.1.6a），而 CMake/CPack 仅接受数字版本，
# 故剥离尾部字母取数字部分（0.1.6a -> 0.1.6），避免硬编码漂移。
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" AIRY_VERSION_RAW LIMIT_COUNT 1)
string(STRIP "${AIRY_VERSION_RAW}" AIRY_VERSION_RAW)
string(REGEX REPLACE "([0-9.]+).*" "\\1" AIRY_VERSION_NUM "${AIRY_VERSION_RAW}")
if(NOT AIRY_VERSION_NUM)
    set(AIRY_VERSION_NUM "0.1.11")
endif()
# 纯 C 项目（全仓无 .cpp/.cc 源文件）；仅声明 C 可避免 CXX 编译器
# 缺失时（如 aarch64 交叉工具链仅装了 gcc 未装 g++）configure 失败。
project(AgentRT VERSION ${AIRY_VERSION_NUM} LANGUAGES C)

# macOS 链接：coreloopthree 以 weak 引用 MemoryRovol 闭源符号
# （memoryrovol_provider / rs_semantic_cache）。ELF 下未定义弱符号自动
# 置 NULL 可链接；Mach-O 默认 -undefined error 会报 "Undefined symbols
# for architecture arm64"（release run #4 实证）。加 dynamic_lookup 允许
# 弱导入运行期解析（调用侧以 NULL 判空，安全降级为 builtin）。
if(APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-undefined,dynamic_lookup")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-undefined,dynamic_lookup")
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "AgentRT in-source builds are not allowed! (BAN-33)\n"
        "Please use out-of-source build with cmake -B:\n"
        "  cmake -B ../AgentRT-build -DCMAKE_BUILD_TYPE=Release\n"
        "  cmake --build ../AgentRT-build\n"
        "  cd ../AgentRT-build && ctest --output-on-failure\n"
        "See BUILD.md for details."
    )
endif()

# BAN-33强化：禁止在源码树内任何子目录构建
# 使用 cmake_path 进行路径前缀检测（避免 "AgentRT-build" 误匹配 "AgentRT"）
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.20")
    cmake_path(IS_PREFIX CMAKE_SOURCE_DIR "${CMAKE_BINARY_DIR}" _BAN33_IS_SUBDIR)
    cmake_path(IS_PREFIX CMAKE_BINARY_DIR "${CMAKE_SOURCE_DIR}" _BAN33_IS_SAME_OR_PARENT)
    if((NOT _BAN33_IS_SAME_OR_PARENT) AND (_BAN33_IS_SUBDIR OR CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR))
        message(FATAL_ERROR
            "AgentRT build directory MUST be outside the source tree! (BAN-33)\n"
            "Your build directory '${CMAKE_BINARY_DIR}' is inside the source tree.\n"
            "Please use an out-of-source build directory:\n"
            "  cmake -B ../AgentRT-build -DCMAKE_BUILD_TYPE=Release\n"
            "  cmake --build ../AgentRT-build\n"
            "See BUILD.md for details."
        )
    endif()
else()
    # 旧版 CMake 回退方案：用路径分隔符防止子串误匹配
    string(FIND "${CMAKE_BINARY_DIR}/" "${CMAKE_SOURCE_DIR}/" _BAN33_BUILD_INSIDE)
    if(_BAN33_BUILD_INSIDE EQUAL 0 AND NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
        message(FATAL_ERROR
            "AgentRT build directory MUST be outside the source tree! (BAN-33)\n"
            "Your build directory '${CMAKE_BINARY_DIR}' is inside the source tree.\n"
            "Please use an out-of-source build directory:\n"
            "  cmake -B ../AgentRT-build -DCMAKE_BUILD_TYPE=Release\n"
            "  cmake --build ../AgentRT-build\n"
            "See BUILD.md for details."
        )
    endif()
endif()

# 设置默认构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (Debug|Release|RelWithDebInfo|MinSizeRel)" FORCE)
endif()

# 引入统一构建打印系统（彩色 + 时间戳 + 级别）
# v4.1: cmake 模块归位至 agentrt 管理仓 cmake/（v0.1.2 决策：构建系统属
# IRON-9 [IND] 完全独立层，agentrt 与 agent-linux 各自独立，不再共享
# 伞仓直属模块；独立 clone agentrt 仓即可完整构建）
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/airy_print.cmake)

# M1-1b：模块链接白名单门禁函数（collect 阶段在 gateway 子目录配置前
# 就绪；install_checks 阶段在 tools/airy_depgraph 配置后调用）
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/airy_linkgate.cmake)

# 项目范围选项
option(BUILD_SHARED_LIBS "Build shared libraries instead of static libraries" OFF)
option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(BUILD_TESTS "Build unit tests" ON)
option(ENABLE_COVERAGE "Enable code coverage reporting" OFF)
# 生产默认 OFF：ASan/LSan/UBSan 仅用于开发与测试（CI 泄漏检测显式 ON）。
# 默认 ON 曾导致发布二进制携带 sanitizer 运行时（性能开销 2-3x、退出时
# 输出泄漏报告、TUI 构建因库被判定 ASan 插桩而拒绝链接 IME）。
# 需要检测时显式传 -DENABLE_SANITIZERS=ON。
option(ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
option(AIRY_BUILD_ALL "Build all AgentRT components" ON)
# IRON-8 生产就绪：MemoryRovol 为默认记忆提供商（L1-L4 四层记忆卷载），
# 默认启用而非显式开关。外部源缺失时 atoms/memory/memoryrovol/CMakeLists.txt
# 自动降级为 builtin provider，不会破坏构建。
option(AIRY_WITH_MEMORYROVOL "Build with MemoryRovol commercial memory provider" ON)
option(AIRY_COMPLIANCE_STRICT "Enable strict compliance mode (poison unsafe functions)" ON)
set(AIRY_MEMORY_BACKEND "builtin" CACHE STRING "Memory backend (builtin|memoryrovol)")

# 版本宏 SSoT 注入（0.1.8 审计修复）：commons/include/airyrt_version.h 的
# AIRYRT_VERSION 此前是无保护手工副本，每次发版须人肉同步 VERSION，是
# "构建后才发现版本硬编码漂移"的系统性根因。现统一从 VERSION 文件（RAW
# 含字母后缀，如 0.1.6a）经 add_compile_definitions 注入，覆盖本目录及全部
# 子目录（各 daemon svc_adapter / gateway / commons）；头文件保留 #ifndef
# 回退，供未走 CMake 的独立编译（语法检查等）兜底。与 AIRY_CLI_VERSION
# 注入方式对齐（tools/airy_cli/CMakeLists.txt）。
set(AIRYRT_VERSION_INJECT "${AIRY_VERSION_RAW}")
if(NOT AIRYRT_VERSION_INJECT)
    set(AIRYRT_VERSION_INJECT "${AIRY_VERSION_NUM}")
endif()
if(AIRYRT_VERSION_INJECT)
    add_compile_definitions(AIRYRT_VERSION="${AIRYRT_VERSION_INJECT}")
    airy_print_ok("AIRYRT_VERSION injected from VERSION SSoT: ${AIRYRT_VERSION_INJECT}")
else()
    airy_print_warn("VERSION 文件缺失，AIRYRT_VERSION 回退头文件缺省值")
endif()

# CTest必须在顶级启用（在任何add_subdirectory之前）
# R-09-SEC: Strict compliance mode and tests are now independent.
# Tests MUST run under strict compliance to catch BAN violations at compile time.
# Previously, strict mode forced BUILD_TESTS=OFF, which prevented test execution
# in the default configuration. This was a design error (BAN-181 compliance fix).
if(BUILD_TESTS)
    if(AIRY_COMPLIANCE_STRICT)
        airy_print_ok("Tests enabled with strict compliance (AIRY_COMPLIANCE_STRICT=ON)")
    endif()
endif()
if(BUILD_TESTS)
    enable_testing()
    airy_print_info("CTest enabled at root level (before subdirectories)")
    # 为所有测试设置 sanitizer 环境变量（set(ENV{}) 不会传递给 ctest）
    if(ENABLE_SANITIZERS AND NOT MSVC)
        set(AIRY_ASAN_OPTIONS "halt_on_error=1:detect_stack_use_after_return=1:detect_leaks=1:allocator_may_return_null=1")
        set(AIRY_LSAN_OPTIONS "suppressions=${CMAKE_SOURCE_DIR}/../ecosystem/manager/sanitizer/lsan-suppressions:print_suppressions=0")
        set(AIRY_UBSAN_OPTIONS "halt_on_error=1:print_stacktrace=1")
    endif()
endif()

# 输出构建配置信息
airy_print_section("AgentRT v${PROJECT_VERSION} Build Configuration")
airy_print_info("Build type:       ${CMAKE_BUILD_TYPE}")
airy_print_info("Platform:         ${CMAKE_SYSTEM_NAME} (${CMAKE_SYSTEM_PROCESSOR})")
airy_print_info("Compiler:         ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
airy_print_info("Shared libs:      ${BUILD_SHARED_LIBS}")
airy_print_info("Build tests:      ${BUILD_TESTS}")
airy_print_info("Compliance strict:${AIRY_COMPLIANCE_STRICT}")
airy_print_info("Memory backend:   ${AIRY_MEMORY_BACKEND}")
airy_print_info("MemoryRovol Pro:  ${AIRY_WITH_MEMORYROVOL}")
airy_print_info("Sanitizers:       ${ENABLE_SANITIZERS}")

# 运行时库自包含（2026-08-29 社区 bug 根治：agentrt-tui/daemons 依赖
# libcjson.so.1 等非系统库，目标机无该库则 "error while loading shared
# libraries"）。全部二进制携带 $ORIGIN/../lib RUNPATH；发布打包时把
# 非系统核心 .so 随包放入 lib/，安装后无需目标机预装 cJSON/OpenSSL 等。
set(CMAKE_BUILD_RPATH "$ORIGIN/../lib")
set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)

# 关键路径验证：确认核心目录存在
airy_print_info("Path check: Source root = ${CMAKE_CURRENT_SOURCE_DIR}")
foreach(_dir atoms commons protocols gateway daemons cupolas heapstore)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_dir}")
        airy_print_ok("  ${_dir}/")
    else()
        airy_print_error("  ${_dir}/ — MISSING!")
    endif()
endforeach()

# 导出编译命令数据库（供clangd/IDE使用）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 全局编译选项
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# 平台特定的安全编译选项
if(MSVC)
    # MSVC安全选项
    add_compile_options(/W4 /WX- /permissive-)  # 最高警告级别，但警告不视为错误
    # _CRT_DECLARE_NONSTDC_NAMES：/std:c11 下恢复 POSIX 系声明（_access/
    # S_ISDIR 等，#116 C4013 根因；windows_preinclude.h 已同款定义，此处
    # 为叶子仓独立构建（不走 /FI 预包含）时的命令行级双保险）。
    # _CRT_RAND_S：rand_s 声明开关，preinclude stdlib.h 时序同理。
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS
                            _CRT_DECLARE_NONSTDC_NAMES=1 _CRT_RAND_S=1)
    add_compile_options(/GS /sdl- /guard:cf /utf-8)  # 安全选项

    # 禁用特定警告
    add_compile_options(/wd4828)  # 文件包含在当前源字符集中无效的字符
    add_compile_options(/wd4996)  # 不安全函数警告（通过_CRT_SECURE_NO_WARNINGS处理）
    add_compile_options(/wd4255)  # 无函数原型
    add_compile_options(/wd4668)  # 未定义的宏替换为0

    # 链接器安全选项
    add_link_options(/DYNAMICBASE /NXCOMPAT /GUARD:CF)
else()
    # GCC/Clang安全选项
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter -Wno-stringop-overflow -Wno-format-truncation)
    if(WARNINGS_AS_ERRORS)
        add_compile_options(-Werror=all -Werror=extra)
        add_compile_options(-Wno-error=format-truncation)
    endif()
    add_compile_options(-fstack-protector-strong -fno-omit-frame-pointer)
    add_compile_definitions(_GNU_SOURCE)
    # IRON-12 路径隐私：日志 __FILE__ 不暴露构建机绝对路径（社区用户可安装任意路径，
    # 日志中的绝对源码路径即信息泄露）。-ffile-prefix-map 将源码前缀映射为相对标识，
    # 映射按最长前缀优先生效，故先映射伞仓根再映射 agentrt 源码树。
    # get_filename_component 规范化前缀，避免 'agentrt/..' 未展开导致前缀失配。
    get_filename_component(_AIRY_HUB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
    add_compile_options(-ffile-prefix-map=${_AIRY_HUB_ROOT}=agentrt-hub)
    add_compile_options(-ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=agentrt)
    # _FORTIFY_SOURCE=2 需要 -O1 或更高优化级别，Debug 构建时跳过以避免
    # "_FORTIFY_SOURCE requires compiling with optimization" 警告
    if(NOT "${CMAKE_BUILD_TYPE}" STREQUAL "Debug" AND NOT "${CMAKE_BUILD_TYPE}" STREQUAL "")
        add_compile_definitions(_FORTIFY_SOURCE=2)
    endif()

    if(AIRY_COMPLIANCE_STRICT)
        add_compile_definitions(AIRY_COMPLIANCE_STRICT)
        add_compile_options("-include" "${CMAKE_CURRENT_SOURCE_DIR}/commons/utils/compliance/banned_functions.h")

        # 注意：内存子系统实现层的 AIRY_COMPLIANCE_IMPL 豁免由各子目录的
        # CMakeLists.txt 中的 set_source_files_properties 设置，因为该属性
        # 仅在目标创建的目录作用域内生效。
        # 详见：atoms/corekern/CMakeLists.txt
        #       cupolas/CMakeLists.txt
        #       commons/CMakeLists.txt
    endif()

    # 链接器安全选项
    if(UNIX AND NOT APPLE)
        add_link_options(-Wl,-z,relro -Wl,-z,now)
    elseif(APPLE)
        add_link_options(-Wl,-dead_strip)
    endif()
    add_link_options(-Wno-stringop-overflow)
endif()

# Sanitizer支持（仅GCC/Clang）
# 四重运行时检测：AddressSanitizer + LeakSanitizer + UndefinedBehaviorSanitizer + ThreadSanitizer
if(ENABLE_SANITIZERS AND NOT MSVC)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        # LSan（leak）仅 Linux 可用：macOS 的 clang 链接期报
        # "unsupported option '-fsanitize=leak'"，Apple 平台排除 LSan
        # 保留 ASan + UBSan（float-divide-by-zero 两个平台均支持）。
        if(APPLE)
            add_compile_options(-fsanitize=address,undefined,float-divide-by-zero -fno-omit-frame-pointer)
            add_link_options(-fsanitize=address,undefined,float-divide-by-zero)
        else()
            add_compile_options(-fsanitize=address,undefined,leak,float-divide-by-zero -fno-omit-frame-pointer)
            add_link_options(-fsanitize=address,undefined,leak,float-divide-by-zero)
        endif()
        # CI环境变量：halt_on_error确保任何检测到的问题立即阻断
        set(ENV{ASAN_OPTIONS} "halt_on_error=1:detect_stack_use_after_return=1:detect_leaks=1")
        if(NOT APPLE)
            set(ENV{LSAN_OPTIONS} "suppressions=${CMAKE_SOURCE_DIR}/../ecosystem/manager/sanitizer/lsan-suppressions")
        endif()
        set(ENV{UBSAN_OPTIONS} "halt_on_error=1:print_stacktrace=1")
        if(APPLE)
            airy_print_ok("Sanitizers enabled: ASan + UBSan (LSan not available on macOS)")
        else()
            airy_print_ok("Sanitizers enabled: ASan + LSan + UBSan")
        endif()
    else()
        airy_print_warn("Sanitizers only supported for GCC/Clang compilers")
    endif()
endif()

# ThreadSanitizer 独立选项（不与 ASan 同时使用）
option(ENABLE_TSAN "Enable ThreadSanitizer (disables other sanitizers)" OFF)
if(ENABLE_TSAN AND NOT MSVC)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
        add_link_options(-fsanitize=thread)
        set(ENV{TSAN_OPTIONS} "halt_on_error=1:history_size=7:second_deadlock_stack=1")
        airy_print_ok("ThreadSanitizer enabled")
    endif()
endif()

# MSVC Release模式特殊处理：解决/O2与/RTC1冲突
if(MSVC AND CMAKE_BUILD_TYPE STREQUAL "Release")
    # 移除默认的/RTC1标志（如果存在），因为与/O2不兼容
    string(REPLACE "/RTC1" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
    string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    airy_print_info("Adjusted MSVC Release flags to avoid /O2 and /RTC1 conflict")
endif()

# 调试符号（由构建类型控制）
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    if(MSVC)
        add_compile_options(/Zi /Ob0 /Od /RTC1)  # 调试信息，禁用优化，运行时检查
        add_link_options(/DEBUG)
    else()
        add_compile_options(-g -O0 -fno-inline)  # 调试信息，无优化，禁用内联
    endif()
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MSVC)
        add_compile_options(/O2 /Ob2 /Oi /Ot /GL)  # 优化选项
        add_link_options(/LTCG)  # 链接时代码生成
    else()
        # -ffat-lto-objects: 让 LTO 对象同时包含 GIMPLE 字节码和机器代码。
        # 这样链接器扫描静态库时可用传统符号表解析，避免 LTO 符号摘要导致的
        # undefined reference 错误（LTO + ASan 组合下尤其常见）。
        # 代价：对象文件体积约增加 2x，但保留了 LTO 跨模块优化能力。
        # 注意：Apple clang（macOS）不支持 -ffat-lto-objects（v0.1.9
        # Release run 实证 "unsupported option"），故探测后启用；探测失败
        # 回退纯 -O3 无 LTO，不影响正确性。
        include(CheckCCompilerFlag)
        check_c_compiler_flag("-flto -ffat-lto-objects" AIRY_HAVE_FAT_LTO_OBJECTS)
        if(AIRY_HAVE_FAT_LTO_OBJECTS)
            add_compile_options(-O3 -flto -ffat-lto-objects)
            add_link_options(-flto -ffat-lto-objects)
        else()
            add_compile_options(-O3)
        endif()
        add_compile_definitions(NDEBUG)
        # 发行制品体积控制：链接期 strip 符号表与残留 debug 段。
        # Sanitizer 构建保留符号（ASan 符号化依赖），仅纯 Release 剥离。
        if(NOT ENABLE_SANITIZERS)
            add_link_options(-s)
        endif()
    endif()
endif()

# 代码覆盖率支持
if(ENABLE_COVERAGE)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fprofile-arcs -ftest-coverage)
        add_link_options(-fprofile-arcs -ftest-coverage -lgcov)
        airy_print_ok("Code coverage enabled for ${CMAKE_C_COMPILER_ID}")
    elseif(MSVC)
        airy_print_warn("Code coverage for MSVC requires Visual Studio professional edition")
    else()
        airy_print_warn("Code coverage only supported for GCC/Clang and MSVC compilers")
    endif()
endif()

# 查找必需的系统依赖
# FindThreads 在 Linux/macOS/Windows 均可用：macOS 与 Linux 返回
# pthread 库，Windows 返回空（线程支持内建于 CRT）。全树大量 target
# 引用 Threads::Threads（airy_common/svc_common/cupolas/各 daemon），
# 必须无条件查找，否则 Windows 配置阶段即报
# "link interface contains Threads::Threads but the target was not found"。
find_package(Threads REQUIRED)
if(CMAKE_THREAD_LIBS_INIT)
    airy_print_ok("Threads found: ${CMAKE_THREAD_LIBS_INIT}")
endif()

# ============================================================
# 集中化第三方依赖检测（BAN-12: 单一依赖检测点）
# 所有子模块通过 target_link_libraries 获取依赖，
# 禁止在子模块中重复 find_package / pkg_check_modules
# ============================================================
find_package(PkgConfig QUIET)

# --- SQLite3 ---
find_package(SQLite3 QUIET)
if(NOT SQLite3_FOUND AND PkgConfig_FOUND)
    pkg_check_modules(SQLITE3 QUIET sqlite3)
endif()
if(NOT SQLite3_FOUND AND NOT SQLITE3_FOUND)
    find_path(_SQLITE3_INCLUDE_DIR sqlite3.h)
    find_library(_SQLITE3_LIBRARY sqlite3)
    if(_SQLITE3_INCLUDE_DIR AND _SQLITE3_LIBRARY)
        set(SQLite3_INCLUDE_DIRS ${_SQLITE3_INCLUDE_DIR})
        set(SQLite3_LIBRARIES ${_SQLITE3_LIBRARY})
        set(SQLite3_FOUND TRUE)
    endif()
endif()
if(SQLite3_FOUND OR SQLITE3_FOUND)
    set(AIRY_HAS_SQLITE3 TRUE CACHE BOOL "SQLite3 available")
    airy_print_ok("SQLite3 found: ${SQLite3_LIBRARIES}${SQLITE3_LIBRARIES}")
else()
    set(AIRY_HAS_SQLITE3 FALSE CACHE BOOL "SQLite3 available")
    airy_print_warn("SQLite3 not found, registry/vector-store features disabled")
endif()

# --- cJSON ---
find_package(cJSON QUIET)
if(NOT cJSON_FOUND AND PkgConfig_FOUND)
    pkg_check_modules(CJSON QUIET libcjson)
endif()
# 部分发行版（如 Ubuntu 20.04）libcjson-dev 不安装 .pc 文件，
# pkg-config 与 find_package 均失效，回退 find_library 直查。
if(NOT cJSON_FOUND AND NOT CJSON_FOUND)
    find_path(CJSON_INCLUDE_DIR cjson/cJSON.h)
    find_library(CJSON_LIBRARY NAMES cjson)
    if(CJSON_INCLUDE_DIR AND CJSON_LIBRARY)
        set(CJSON_INCLUDE_DIRS ${CJSON_INCLUDE_DIR})
        set(CJSON_LIBRARIES ${CJSON_LIBRARY})
        set(CJSON_FOUND TRUE)
    endif()
endif()
if(cJSON_FOUND OR CJSON_FOUND)
    set(AIRY_HAS_CJSON TRUE CACHE BOOL "cJSON available")
    airy_print_ok("cJSON found: ${cJSON_VERSION}${CJSON_VERSION}")
else()
    set(AIRY_HAS_CJSON FALSE CACHE BOOL "cJSON available")
    airy_print_warn("cJSON not found, JSON features disabled")
endif()
# cJSON 头目录统一纳入全 target 可见 include 路径：find_package/pkg 检测
# 各自写入不同前缀变量（cJSON_*/CJSON_*），且安装目录不在系统默认搜索
# 路径（macOS Homebrew /opt/homebrew 等）时，直接 #include <cjson/cJSON.h>
# 的下游 TU（如 atoms/coreloopthree cognition）会 "file not found"。此处
# 等价于系统默认路径的聚合面，Linux（/usr/include/cjson）下为空无副作用。
if(cJSON_INCLUDE_DIRS)
    include_directories(${cJSON_INCLUDE_DIRS})
endif()
if(CJSON_INCLUDE_DIRS)
    include_directories(${CJSON_INCLUDE_DIRS})
endif()

# --- libyaml ---
find_package(YAML QUIET)
if(NOT YAML_FOUND AND PkgConfig_FOUND)
    pkg_check_modules(YAML QUIET yaml-0.1)
endif()
if(NOT YAML_FOUND AND PkgConfig_FOUND)
    pkg_check_modules(YAML QUIET libyaml)
endif()
if(YAML_FOUND)
    # 下游一律以裸 ${YAML_LIBRARIES}（"yaml"→-lyaml）链接；keg-only 安装
    # （Homebrew libyaml）只把目录给到 pkg-config，-L 到不了链接行，
    # macOS 报 "ld: library 'yaml' not found"。提升为绝对路径后所有
    # 消费点（commons/cupolas/daemons）无需 -L 即可链接。
    if(NOT IS_ABSOLUTE "${YAML_LIBRARIES}")
        find_library(YAML_ABSOLUTE_LIBRARY
                     NAMES yaml yaml-0.1
                     HINTS ${YAML_LIBRARY_DIRS}
                     PATH_SUFFIXES lib)
        if(YAML_ABSOLUTE_LIBRARY)
            set(YAML_LIBRARIES "${YAML_ABSOLUTE_LIBRARY}")
        endif()
    endif()
    set(AIRY_HAS_YAML TRUE CACHE BOOL "libyaml available")
    airy_print_ok("libyaml found: ${YAML_LIBRARIES}")
else()
    set(AIRY_HAS_YAML FALSE CACHE BOOL "libyaml available")
    airy_print_warn("libyaml not found, YAML config features disabled")
endif()

# --- OpenSSL ---
find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
    set(AIRY_HAS_OPENSSL TRUE CACHE BOOL "OpenSSL available")
    airy_print_ok("OpenSSL found: ${OpenSSL_VERSION}")
else()
    set(AIRY_HAS_OPENSSL FALSE CACHE BOOL "OpenSSL available")
    airy_print_warn("OpenSSL not found, crypto features disabled")
endif()

# --- libcurl ---
find_package(CURL QUIET)
if(NOT CURL_FOUND AND PkgConfig_FOUND)
    pkg_check_modules(LIBCURL QUIET libcurl)
endif()
if(CURL_FOUND OR LIBCURL_FOUND)
    set(AIRY_HAS_CURL TRUE CACHE BOOL "libcurl available")
    airy_print_ok("libcurl found: ${CURL_LIBRARIES}${LIBCURL_LIBRARIES}")
else()
    set(AIRY_HAS_CURL FALSE CACHE BOOL "libcurl available")
    airy_print_warn("libcurl not found, HTTP client features disabled")
endif()

# --- libmicrohttpd ---
find_package(libmicrohttpd QUIET)
if(NOT libmicrohttpd_FOUND AND PkgConfig_FOUND)
    pkg_check_modules(MICROHTTPD QUIET libmicrohttpd)
endif()
# 交叉编译（sysroot 无 gnutls.pc 等传递依赖）时 pkg-config 校验失败，
# 回退 find_library 直查（同 SQLite3 既有模式）。
# Windows (vcpkg libmicrohttpd port)：导入库文件名为 libmicrohttpd.lib
# （非 microhttpd.lib），#112 实证只查 microhttpd 导致 AIRY_HAS_MICROHTTPD
# FALSE、gateway_d 26×LNK2001（MHD_* + http_gateway 内部符号）。两名字
# 都查：POSIX 是 -lmicrohttpd（libmicrohttpd.so 软链 = libmicrohttpd.so）。
if(NOT libmicrohttpd_FOUND AND NOT MICROHTTPD_FOUND)
    find_path(_MICROHTTPD_INCLUDE_DIR microhttpd.h)
    find_library(_MICROHTTPD_LIBRARY NAMES microhttpd libmicrohttpd)
    if(_MICROHTTPD_INCLUDE_DIR AND _MICROHTTPD_LIBRARY)
        set(MICROHTTPD_INCLUDE_DIRS ${_MICROHTTPD_INCLUDE_DIR})
        set(MICROHTTPD_LIBRARIES ${_MICROHTTPD_LIBRARY})
        set(MICROHTTPD_FOUND TRUE)
    endif()
endif()
if(libmicrohttpd_FOUND OR MICROHTTPD_FOUND)
    set(AIRY_HAS_MICROHTTPD TRUE CACHE BOOL "libmicrohttpd available")
    airy_print_ok("libmicrohttpd found")
else()
    set(AIRY_HAS_MICROHTTPD FALSE CACHE BOOL "libmicrohttpd available")
    airy_print_warn("libmicrohttpd not found, HTTP gateway may be limited")
endif()

# --- libwebsockets ---
find_package(libwebsockets QUIET)
if(NOT libwebsockets_FOUND AND PkgConfig_FOUND)
    pkg_check_modules(LIBWEBSOCKETS QUIET libwebsockets)
endif()
# 交叉编译：libwebsockets.pc 的 Requires.private（libcap 等）缺失时
# pkg-config 校验失败，回退 find_library 直查。
if(NOT libwebsockets_FOUND AND NOT LIBWEBSOCKETS_FOUND)
    find_path(_LWS_INCLUDE_DIR libwebsockets.h)
    find_library(_LWS_LIBRARY websockets)
    if(_LWS_INCLUDE_DIR AND _LWS_LIBRARY)
        set(LIBWEBSOCKETS_INCLUDE_DIRS ${_LWS_INCLUDE_DIR})
        set(LIBWEBSOCKETS_LIBRARIES ${_LWS_LIBRARY})
        set(LIBWEBSOCKETS_FOUND TRUE)
    endif()
endif()
if(libwebsockets_FOUND OR LIBWEBSOCKETS_FOUND)
    set(AIRY_HAS_LIBWEBSOCKETS TRUE CACHE BOOL "libwebsockets available")
    airy_print_ok("libwebsockets found")
else()
    set(AIRY_HAS_LIBWEBSOCKETS FALSE CACHE BOOL "libwebsockets available")
    airy_print_warn("libwebsockets not found, WebSocket features disabled")
endif()

# --- libevent ---
find_package(libevent CONFIG QUIET)
if(NOT libevent_FOUND AND PkgConfig_FOUND)
    pkg_check_modules(LIBEVENT QUIET libevent)
endif()
if(libevent_FOUND OR LIBEVENT_FOUND)
    set(AIRY_HAS_LIBEVENT TRUE CACHE BOOL "libevent available")
    airy_print_ok("libevent found")
else()
    set(AIRY_HAS_LIBEVENT FALSE CACHE BOOL "libevent available")
    airy_print_warn("libevent not found, event-loop features disabled")
endif()

# --- nghttp2（HTTP/2）---
# 前置探测：daemons/（gateway_d）在 gateway/ 之前 add_subdirectory，
# 若在子模块探测则首次 configure 时 AIRY_HAS_HTTP2 恒为 FALSE，HTTP/2
# 首次构建被静默禁用、二次 configure 才生效。根级先行探测保证首次即正确
# （gateway/ 子模块保留同逻辑，CACHE 已设时幂等）。
#
# WIN32 在本权威写入点强制关闭：http2 家族实现是 POSIX-only
# （pthread 事件循环 + fcntl F_GETFL/O_NONBLOCK + poll/nfds_t），
# gateway_lib_obj 未接 winpthreads，MSVC 编译必报 C2065。子模块侧
# 后续无 FORCE 的 CACHE 写覆盖不了此处已定的值，故门控必须落在根级。
# Windows 原生线程模型移植完成后再恢复（fc527de 同裁决）。
if(WIN32)
    set(AIRY_HAS_HTTP2 FALSE CACHE BOOL "nghttp2 available" FORCE)
    airy_print_warn("HTTP/2 gateway disabled on Windows (POSIX-only implementation)")
else()
    find_library(_NGHTTP2_LIBRARY NAMES nghttp2)
    if(NOT _NGHTTP2_LIBRARY)
        find_file(_NGHTTP2_LIBRARY
            NAMES libnghttp2.so.14 libnghttp2.so.19 libnghttp2.so
            PATHS /usr/lib/x86_64-linux-gnu /usr/lib /usr/local/lib
        )
    endif()
    if(_NGHTTP2_LIBRARY)
        set(AIRY_HAS_HTTP2 TRUE CACHE BOOL "nghttp2 available")
        airy_print_ok("nghttp2 found: ${_NGHTTP2_LIBRARY} (HTTP/2 gateway enabled)")
    else()
        set(AIRY_HAS_HTTP2 FALSE CACHE BOOL "nghttp2 available")
        airy_print_warn("nghttp2 not found, HTTP/2 gateway disabled")
    endif()
endif()

# --- GTest ---
if(BUILD_TESTS)
    find_package(GTest QUIET)
    if(GTest_FOUND)
        set(AIRY_HAS_GTEST TRUE CACHE BOOL "GTest available")
        airy_print_ok("GTest found: ${GTEST_LIBRARIES}")
    else()
        set(AIRY_HAS_GTEST FALSE CACHE BOOL "GTest available")
        airy_print_warn("GTest not found, tests will use custom framework")
    endif()
endif()

# --- CMocka ---
if(BUILD_TESTS)
    find_package(cmocka QUIET)
    if(NOT cmocka_FOUND)
        find_package(Cmocka QUIET)
    endif()
    if(cmocka_FOUND OR CMOCKA_FOUND)
        set(AIRY_HAS_CMOCKA TRUE CACHE BOOL "cmocka available")
        if(NOT CMOCKA_LIBRARIES)
            set(CMOCKA_LIBRARIES ${cmocka_LIBRARIES} CACHE STRING "cmocka libraries" FORCE)
        endif()
        if(NOT CMOCKA_INCLUDE_DIRS)
            set(CMOCKA_INCLUDE_DIRS ${cmocka_INCLUDE_DIRS} CACHE STRING "cmocka include dirs" FORCE)
        endif()
        airy_print_ok("cmocka found")
    else()
        set(AIRY_HAS_CMOCKA FALSE CACHE BOOL "cmocka available")
        airy_print_warn("cmocka not found, some tests may use stub framework")
    endif()
endif()

# 创建 INTERFACE 库用于传递全局依赖编译定义
add_library(airy_compile_defs INTERFACE)
if(AIRY_HAS_SQLITE3)
    target_compile_definitions(airy_compile_defs INTERFACE AIRY_HAS_SQLITE3)
endif()
if(AIRY_HAS_CJSON)
    target_compile_definitions(airy_compile_defs INTERFACE AIRY_HAS_CJSON)
endif()
if(AIRY_HAS_YAML)
    target_compile_definitions(airy_compile_defs INTERFACE AIRY_HAS_YAML)
endif()
if(AIRY_HAS_OPENSSL)
    target_compile_definitions(airy_compile_defs INTERFACE AIRY_HAS_OPENSSL)
endif()
if(AIRY_HAS_CURL)
    target_compile_definitions(airy_compile_defs INTERFACE AIRY_HAS_CURL)
endif()
if(AIRY_HAS_MICROHTTPD)
    target_compile_definitions(airy_compile_defs INTERFACE AIRY_HAS_MICROHTTPD)
endif()
if(AIRY_HAS_LIBWEBSOCKETS)
    target_compile_definitions(airy_compile_defs INTERFACE AIRY_HAS_LIBWEBSOCKETS)
endif()
if(AIRY_HAS_LIBEVENT)
    target_compile_definitions(airy_compile_defs INTERFACE AIRY_HAS_LIBEVENT)
endif()
# 检测到的依赖标志同时全局下发（add_compile_definitions），保证未链接
# airy_compile_defs 的目标与 MSVC /FI 强制预包含（windows_preinclude.h）
# 的宏视图一致：否则 windows_preinclude.h 的 cJSON 兜底桩在 AIRY_HAS_CJSON
# 未达时先定义 struct cJSON+旧 API，随后真 <cjson/cJSON.h> 即 C2011
# 重定义（cupolas 测试 Windows build 实证）。检测点仍单一（上方 find_*）。
if(AIRY_HAS_SQLITE3)
    add_compile_definitions(AIRY_HAS_SQLITE3)
endif()
if(AIRY_HAS_CJSON)
    add_compile_definitions(AIRY_HAS_CJSON)
endif()
if(AIRY_HAS_YAML)
    add_compile_definitions(AIRY_HAS_YAML)
endif()
if(AIRY_HAS_OPENSSL)
    add_compile_definitions(AIRY_HAS_OPENSSL)
endif()
if(AIRY_HAS_CURL)
    add_compile_definitions(AIRY_HAS_CURL)
endif()
if(AIRY_HAS_MICROHTTPD)
    add_compile_definitions(AIRY_HAS_MICROHTTPD)
endif()
if(AIRY_HAS_LIBWEBSOCKETS)
    add_compile_definitions(AIRY_HAS_LIBWEBSOCKETS)
endif()
if(AIRY_HAS_LIBEVENT)
    add_compile_definitions(AIRY_HAS_LIBEVENT)
endif()

# ============================================================
# 跨平台系统库统一管理（BAN-15: 禁止硬编码平台库）
# airy_platform_libs, dl, pthread, m, ws2_32 等通过此 INTERFACE 库统一提供
# 平台映射：
#   - m（libm）：POSIX 平台存在；MSVC 无独立 m.lib（数学函数在 CRT 内），
#     直接链接裸 `m` 会报 LNK1104，故仅非 MSVC 提供。
#   - dl：仅 Linux（macOS libSystem 内建，Windows 无）。
#   - ws2_32：仅 Windows。
# ============================================================
add_library(airy_platform_libs INTERFACE)
if(UNIX AND NOT APPLE)
    target_link_libraries(airy_platform_libs INTERFACE dl)
    # shm_open/shm_unlink：glibc<2.34 位于 librt，需显式链接；
    # glibc>=2.34 已并入 libc（librt 存在但为空，链接无副作用）。
    find_library(RT_LIBRARY NAMES rt)
    if(RT_LIBRARY)
        target_link_libraries(airy_platform_libs INTERFACE ${RT_LIBRARY})
    endif()
endif()
if(NOT MSVC)
    # macOS 的 libSystem 已内建 dl/uuid/m，无需独立库；此分支同时覆盖 Linux 的 m
    target_link_libraries(airy_platform_libs INTERFACE m)
endif()
if(WIN32)
    target_link_libraries(airy_platform_libs INTERFACE ws2_32)
endif()

# ============================================================
# 64 位原子操作架构兼容（0.1.6f 强化）
# C11 _Atomic int64_t/uint64_t/double 在部分 32 位目标上无法编译为
# 原生指令：riscv32（RV32A 无 64 位原子指令）、armv6 等会生成对
# libatomic 的 libcall（__atomic_load_8/__atomic_store_8/__atomic_*_8）。
# i686（cmpxchg8b）与 armv7l（ldrexd/strexd）通常原生 lock-free。
# 通过编译链接探测决定是否需要显式链接 libatomic，避免 riscv32 等
# 架构链接期 undefined reference。探测代码故意做一次 64 位原子
# 读写+CAS：若无原生支持，链接阶段将因 libatomic libcall 失败。
# ============================================================
include(CheckCSourceCompiles)
# libatomic 是 GCC/Clang 生态概念。MSVC 无 _Atomic（C11 原子走 Interlocked
# 原生实现），探测代码会编译失败而被误判为"需链接 libatomic"，进而产生
# LNK1181 cannot open input file 'atomic.lib'（build-test #109 实证）。
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    check_c_source_compiles("
    #include <stdatomic.h>
    _Atomic long long g_atomic64;
    int main(void) {
        long long v = 0;
        atomic_store_explicit(&g_atomic64, 42LL, memory_order_relaxed);
        (void)atomic_load_explicit(&g_atomic64, memory_order_relaxed);
        (void)atomic_compare_exchange_strong_explicit(&g_atomic64, &v, 1LL,
                                                      memory_order_relaxed, memory_order_relaxed);
        return 0;
    }
    " AIRY_HAVE_NATIVE_ATOMIC64)
    if(NOT AIRY_HAVE_NATIVE_ATOMIC64)
        message(STATUS "64-bit atomics not native on this target; linking libatomic")
        target_link_libraries(airy_platform_libs INTERFACE atomic)
    endif()
endif()

# ============================================================
# 测试链接选项统一管理（BAN-26: 禁止硬编码链接器标志）
# --allow-multiple-definition 仅在 Linux/GNU ld 下有效
# ============================================================
add_library(airy_test_ldflags INTERFACE)
if(UNIX AND NOT APPLE)
    target_link_options(airy_test_ldflags INTERFACE -Wl,--allow-multiple-definition)
endif()

# 包含子目录
if(AIRY_BUILD_ALL)
    airy_print_section("Building AgentRT core components")

    # 全局include路径（确保所有模块都能找到commons的头文件）
    # P0.17 IRON-6 跨层耦合禁令修复:
    # commons 权威路径必须声明在 daemons/common/include 之前。
    # include_directories() 是全局命令，影响所有目标，路径顺序决定 -I 查找顺序。
    # 若 daemons/common/include 在前，atoms 源代码 #include "daemon_bootstrap_ipc.h"
    # 等会先命中 daemons 重导出兼容头（虽然内部经相对路径到达 commons，但仍构成
    # 编译期 atoms→daemons 反向依赖）。将 daemons/common/include 移至 commons 路径之后，
    # 确保 atoms 直接解析 commons 权威版本。
    include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/commons
        ${CMAKE_CURRENT_SOURCE_DIR}/commons/utils/include
        ${CMAKE_CURRENT_SOURCE_DIR}/commons/utils/compat
        ${CMAKE_CURRENT_SOURCE_DIR}/commons/utils/memory
        ${CMAKE_CURRENT_SOURCE_DIR}/commons/utils/string
        ${CMAKE_CURRENT_SOURCE_DIR}/commons/utils/logging
        ${CMAKE_CURRENT_SOURCE_DIR}/commons/utils/error
        ${CMAKE_CURRENT_SOURCE_DIR}/commons/platform/include
        # P0.17 阶段 3: svc_common.h、ipc_service_bus.h 迁移至 commons/utils/ipc
        # P0.17 阶段 4: daemon_bootstrap_ipc.h、ipc_bus_helper.h、ipc_backpressure.h 迁移至 commons/utils/ipc
        ${CMAKE_CURRENT_SOURCE_DIR}/commons/utils/ipc
        # P0.17 阶段 4: daemon_bootstrap_sd.h、service_discovery.h、service_discovery_helper.h 迁移至 commons/utils/sd
        ${CMAKE_CURRENT_SOURCE_DIR}/commons/utils/sd
        # P0.17 阶段 5: thread_pool.h 迁移至 commons/utils/sync
        ${CMAKE_CURRENT_SOURCE_DIR}/commons/utils/sync
        # P0.17 阶段 1: airy_defaults.h 位于 commons/include
        ${CMAKE_CURRENT_SOURCE_DIR}/commons/include
        ${CMAKE_CURRENT_SOURCE_DIR}/atoms/corekern/include
        ${CMAKE_CURRENT_SOURCE_DIR}/protocols/include
        # daemons/common/include 作为兼容层放在最后（仅 daemons 自己需要，
        # atoms 应通过上述 commons 权威路径解析已迁移的头文件）
        ${CMAKE_CURRENT_SOURCE_DIR}/daemons/common/include
        ${CMAKE_CURRENT_SOURCE_DIR}/gateway/src
    )

    # 闭源 atoms 预编译模式：头文件树镜像源码相对结构（atoms/<...>/include 等）。
    # 追加在全局 include 末尾（不做 BEFORE）：commons 等公开源码路径优先，
    # 避免 atoms 与 commons 同名头（如 tcache.h）被遮蔽；atoms 公共头由
    # prebuilt 路径补缺。源码路径缺失时编译器自动忽略对应 -I，
    # 因此该注入对源码构建零影响（仅当 AIRY_ATOMS_PREBUILT_DIR 设置时生效）。
    if(AIRY_ATOMS_PREBUILT_DIR)
        airy_print_ok("Atoms prebuilt include dirs: ${AIRY_ATOMS_PREBUILT_DIR}")
        include_directories(
            ${AIRY_ATOMS_PREBUILT_DIR}/atoms/corekern/include
            ${AIRY_ATOMS_PREBUILT_DIR}/atoms/coreloopthree/include
            ${AIRY_ATOMS_PREBUILT_DIR}/atoms/coreloopthree/src/cognition
            ${AIRY_ATOMS_PREBUILT_DIR}/atoms/coreloopthree/src/cognition/grad
            ${AIRY_ATOMS_PREBUILT_DIR}/atoms/coreloopthree/src/cognition/foundation
            ${AIRY_ATOMS_PREBUILT_DIR}/atoms/memory
            ${AIRY_ATOMS_PREBUILT_DIR}/atoms/memory/builtin
            ${AIRY_ATOMS_PREBUILT_DIR}/atoms/taskflow/include
            ${AIRY_ATOMS_PREBUILT_DIR}/atoms/syscall/include
        )
    endif()

    # 验证全局 include 路径是否存在
    airy_print_info("Verifying global include directories...")
    set(_global_inc_dirs
        commons
        commons/utils/include
        commons/utils/compat
        commons/utils/memory
        commons/utils/string
        commons/utils/logging
        commons/utils/error
        commons/platform/include
        commons/utils/ipc
        commons/utils/sd
        commons/utils/sync
        commons/include
        atoms/corekern/include
        protocols/include
        daemons/common/include
        gateway/src
    )
    foreach(_inc ${_global_inc_dirs})
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_inc}")
            airy_print_ok("  ${_inc}")
        else()
            airy_print_error("  ${_inc} — MISSING!")
        endif()
    endforeach()

    # 依赖检测汇总
    airy_print_section("Dependency Detection Summary")
    foreach(_dep_info
            "SQLite3:       ${AIRY_HAS_SQLITE3}"
            "cJSON:         ${AIRY_HAS_CJSON}"
            "libyaml:       ${AIRY_HAS_YAML}"
            "OpenSSL:       ${AIRY_HAS_OPENSSL}"
            "libcurl:       ${AIRY_HAS_CURL}"
            "libmicrohttpd: ${AIRY_HAS_MICROHTTPD}"
            "libwebsockets: ${AIRY_HAS_LIBWEBSOCKETS}"
            "libevent:      ${AIRY_HAS_LIBEVENT}")
        if(_dep_info MATCHES ": +(ON|YES|TRUE|1)")
            airy_print_ok("  ${_dep_info}")
        else()
            airy_print_warn("  ${_dep_info}")
        endif()
    endforeach()

    # 主要AgentRT组件（原 agentrt/agentrt/CMakeLists.txt，取消嵌套后合并至此）
    airy_print_info("Configuring AgentRT core components...")

    # 从根 CMakeLists.txt 传播依赖可用性标志
    if(AIRY_HAS_CJSON)
        set(AIRY_HAS_CJSON TRUE CACHE BOOL "cJSON available for agentrt")
    endif()
    if(AIRY_HAS_CURL)
        set(AIRY_HAS_CURL TRUE CACHE BOOL "CURL available for agentrt")
    endif()
    if(AIRY_HAS_YAML)
        set(AIRY_HAS_YAML TRUE CACHE BOOL "libyaml available for agentrt")
    endif()
    if(AIRY_HAS_OPENSSL)
        set(AIRY_HAS_OPENSSL TRUE CACHE BOOL "OpenSSL available for agentrt")
    endif()
    if(AIRY_HAS_MICROHTTPD)
        set(AIRY_HAS_MICROHTTPD TRUE CACHE BOOL "libmicrohttpd available for agentrt")
    endif()
    if(AIRY_HAS_LIBWEBSOCKETS)
        set(AIRY_HAS_LIBWEBSOCKETS TRUE CACHE BOOL "libwebsockets available for agentrt")
    endif()

    # ============================================================
    # 模块选项设置（含平台支持矩阵默认值）
    # 默认值按平台能力对齐；显式 -DBUILD_*=... 传参仍可覆盖，为
    # Windows 移植预留显式 opt-in 开发路径（不 FORCE、不吞用户意图）。
    # ============================================================
    option(BUILD_ATOMS "Build atoms module (core kernel components)" ON)
    option(BUILD_COMMONS "Build commons module (common utilities)" ON)
    option(BUILD_CUPOLAS "Build cupolas module (security framework)" ON)
    # Windows 支持矩阵（2026-09-04 决策）：daemons 子系统（svc/ipc/
    # discovery/各 *_d 服务）现为 POSIX 绑定——DIR/strings.h/sys/socket/
    # unistd 直引（MSVC 实证 C1083/C2065 大面积失败），Windows 侧默认
    # 不构建；全量 Windows 移植随 0.1.12/13 GitHub 三平台打包里程碑落地。
    # CLI（airy_cli）依赖 daemon 控制平面（svc_common/gateway RPC），
    # daemons 缺席时一并退出默认矩阵。
    if(WIN32)
        set(AIRY_DAEMON_DEFAULT OFF)
        set(AIRY_CLI_DEFAULT OFF)
        airy_print_warn("Support matrix: daemons/CLI excluded on Windows (POSIX-bound, port milestone 0.1.12/13)")
    else()
        set(AIRY_DAEMON_DEFAULT ON)
        set(AIRY_CLI_DEFAULT ON)
    endif()
    option(BUILD_DAEMON "Build daemon modules (system services)" ${AIRY_DAEMON_DEFAULT})
    option(BUILD_GATEWAY "Build gateway module (API gateway)" ON)
    option(BUILD_HEAPSTORE "Build heapstore module (persistent storage)" ON)
    option(BUILD_CLI "Build interactive CLI product (GCCP + WorkHall)" ${AIRY_CLI_DEFAULT})
    option(BUILD_TOOLKIT "Build toolkit modules (language SDKs)" OFF)

    # 子模块启用状态汇总
    airy_print_info("[MODULE] Submodule build status:")
    airy_print_info("[MODULE]   atoms:    ${BUILD_ATOMS}")
    airy_print_info("[MODULE]   commons:  ${BUILD_COMMONS}")
    airy_print_info("[MODULE]   cupolas:  ${BUILD_CUPOLAS}")
    airy_print_info("[MODULE]   daemon:   ${BUILD_DAEMON}")
    airy_print_info("[MODULE]   gateway:  ${BUILD_GATEWAY}")
    airy_print_info("[MODULE]   heapstore: ${BUILD_HEAPSTORE}")
    airy_print_info("[MODULE]   toolkit:  ${BUILD_TOOLKIT}")
    airy_print_info("[MODULE]   Memory backend: ${AIRY_MEMORY_BACKEND}")
    airy_print_info("[MODULE]   With MemoryRovol: ${AIRY_WITH_MEMORYROVOL}")

    # 全局特性测试宏定义 - 必须在所有其他配置之前
    if(UNIX AND NOT APPLE)
        add_compile_definitions(
            _POSIX_C_SOURCE=200809L
            _XOPEN_SOURCE=700
            _GNU_SOURCE
        )
        airy_print_ok("POSIX feature test macros enabled for Linux")
    elseif(APPLE)
        add_compile_definitions(
            _POSIX_C_SOURCE=200112L
            _DARWIN_C_SOURCE
        )
        airy_print_ok("POSIX feature test macros enabled for macOS")
    endif()

    if(MSVC)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/windows_preinclude.h")
            airy_print_ok("[MSVC] Force-include file found: cmake/windows_preinclude.h")
        else()
            airy_print_warn("[MSVC] Force-include file NOT FOUND: cmake/windows_preinclude.h")
        endif()
        add_compile_options(/FI"${CMAKE_CURRENT_SOURCE_DIR}/cmake/windows_preinclude.h")
    endif()

    # 包含子目录（构建顺序：commons 优先于 atoms）
    if(BUILD_COMMONS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/commons/CMakeLists.txt")
        add_subdirectory(commons)
    else()
        airy_print_warn("commons module not found or disabled")
    endif()

    if(BUILD_ATOMS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/atoms/CMakeLists.txt")
        add_subdirectory(atoms)
    else()
        airy_print_warn("atoms module not found or disabled")
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/protocols/CMakeLists.txt")
        add_subdirectory(protocols)
    endif()

    if(BUILD_CUPOLAS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cupolas/CMakeLists.txt")
        add_subdirectory(cupolas)
    else()
        airy_print_warn("cupolas module not found or disabled")
    endif()

    if(BUILD_DAEMON AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/daemons/CMakeLists.txt")
        add_subdirectory(daemons)
    else()
        airy_print_warn("daemons module not found or disabled")
    endif()

    if(BUILD_GATEWAY AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/gateway/CMakeLists.txt")
        add_subdirectory(gateway)
    else()
        airy_print_warn("gateway module not found or disabled")
    endif()

    if(BUILD_HEAPSTORE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/heapstore/CMakeLists.txt")
        add_subdirectory(heapstore)
    else()
        airy_print_warn("heapstore module not found or disabled")
    endif()

    # 产品化交互式 CLI（GCCP 意图确认 + 工作大厅 + agent 驱动闭环）
    if(BUILD_CLI AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/airy_cli/CMakeLists.txt")
        add_subdirectory(tools/airy_cli)
    else()
        airy_print_warn("airy_cli module not found or disabled")
    endif()

    # 依赖图门禁工具（0.1.6 P1-2）：commons 子域依赖声明 + Kahn 拓扑排序 +
    # 环检测（fail-closed）+ 依赖图报告；复用 taskflow graph_engine
    if(BUILD_COMMONS AND BUILD_ATOMS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/airy_depgraph/CMakeLists.txt")
        add_subdirectory(tools/airy_depgraph)
        # M1-1b：airy_depgraph 配置完成后安装链接白名单门禁（gateway 系
        # 禁链 coreloopthree/cognition，fail-closed）
        airy_linkgate_install_checks()
    else()
        airy_print_warn("airy_depgraph module not found or disabled")
    endif()

    # toolkit 包含多种语言 SDK，需要特殊处理
    if(BUILD_TOOLKIT)
        airy_print_info("Toolkit modules require additional configuration for each language SDK")
    endif()

    # 创建聚合目标（目标名按各子目录实际定义：
    #   atoms→airy_atoms、commons→airy_commons、cupolas→cupolas、
    #   daemons→各 daemon 可执行文件（无统一 agentrt_daemons 目标）、
    #   gateway→gateway 库、heapstore→airy_heapstore）
    add_custom_target(agentrt-all
        DEPENDS
            $<IF:$<TARGET_EXISTS:airy_atoms>,airy_atoms,>
            $<IF:$<TARGET_EXISTS:airy_commons>,airy_commons,>
            $<IF:$<TARGET_EXISTS:cupolas>,cupolas,>
            $<IF:$<TARGET_EXISTS:gateway>,gateway,>
            $<IF:$<TARGET_EXISTS:airy_heapstore>,airy_heapstore,>
            $<IF:$<TARGET_EXISTS:svc_common>,svc_common,>
        COMMENT "Build all AgentRT core components (atoms/commons/cupolas/gateway/heapstore/svc_common)"
    )

    # 安装头文件（0.1.9 0d 扁平化后仅装公共头，排除 .c 与内部头）
    install(DIRECTORY atoms/corekern/include/ DESTINATION include/agentrt/corekern)
    install(DIRECTORY atoms/coreloopthree/include/ DESTINATION include/agentrt/coreloopthree)
    install(DIRECTORY atoms/syscall/include/ DESTINATION include/agentrt/syscall)
    install(DIRECTORY commons/utils/memory/ DESTINATION include/agentrt/commons/memory
            FILES_MATCHING PATTERN "*.h" PATTERN "*_internal.h" EXCLUDE)
    install(DIRECTORY commons/utils/string/ DESTINATION include/agentrt/commons/string
            FILES_MATCHING PATTERN "*.h" PATTERN "*_internal.h" EXCLUDE)
    install(DIRECTORY commons/utils/logging/ DESTINATION include/agentrt/commons/logging
            FILES_MATCHING PATTERN "*.h" PATTERN "*_internal.h" EXCLUDE)

    airy_print_ok("AgentRT core components configuration complete")

    # 测试组件（可选）
    if(BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
        add_subdirectory(tests)
    endif()

    # 集成测试：当从伞仓构建时，包含 tools（v0.1.4 由 devtools 改名）中的
    # 连接线集成测试（12 条 C-L 测试 + 端到端测试）
    # Task #38: 将 tools/tests/integration/c/ 的 12 个连接线测试纳入构建系统
    if(BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../tools/tests/integration/c/CMakeLists.txt")
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../tools/tests/integration/c integration_c_tests)
        airy_print_ok("Integration tests (tools/tests/integration/c) included — 12 C-L link tests + E2E")
    else()
        airy_print_info("Integration tests skipped: tools/tests/integration/c not found (standalone agentrt build)")
    endif()
endif()

# 安装配置
if(AIRY_BUILD_ALL)
    # 创建安装目标
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/docs")
        install(DIRECTORY docs/ DESTINATION share/doc/agentrt
                PATTERN ".gitignore" EXCLUDE
                PATTERN "*.pyc" EXCLUDE)
    endif()

    # 安装许可证文件
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
        install(FILES LICENSE DESTINATION share/licenses/agentrt)
    endif()

    # 安装README文件
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
        install(FILES README.md DESTINATION share/doc/agentrt)
    endif()
endif()

# 包生成支持
include(InstallRequiredSystemLibraries)
set(CPACK_SET_DESTDIR ON)
set(CPACK_PACKAGE_NAME "AgentRT")
set(CPACK_PACKAGE_VENDOR "SPHARX")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Intelligent Agent Runtime Platform")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_PACKAGE_CONTACT "support@spharx.com")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "SPHARX <support@spharx.com>")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
else()
    set(CPACK_GENERATOR "TGZ;DEB")
endif()

include(CPack)

# 为所有测试设置 sanitizer 环境变量
# 使用 CMake 的 CMAKE_TEST_LAUNCHER 或在子目录的 CMakeLists.txt 中设置 set_tests_properties
# 最可靠的方式：创建一个 wrapper 脚本，但更简单的是直接在 shell 中设置环境变量
# 这里我们通过 configure_file 生成一个 ctest 包装脚本
if(BUILD_TESTS AND ENABLE_SANITIZERS AND NOT MSVC)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ctest_wrapper.sh.in"
        "${CMAKE_BINARY_DIR}/run_tests.sh"
        @ONLY
    )
    airy_print_info("Test wrapper script: ${CMAKE_BINARY_DIR}/run_tests.sh")
endif()

# 系统性修复（v6）：Release 构建全局 -DNDEBUG 会把 assert() 编译为 no-op，
# 导致全部单元测试的断言失效——测试"通过"只是未崩溃的假象。
# 对所有测试目标强制 -UNDEBUG，恢复断言有效性（生产 target 不受影响）。
if(BUILD_TESTS)
    function(airy_undef_ndebug_for_tests _dir)
        get_property(_tgts DIRECTORY "${_dir}" PROPERTY BUILDSYSTEM_TARGETS)
        foreach(_tgt IN LISTS _tgts)
            # 跳过 alias/imported target（以 :: 结尾），仅处理可编译目标
            if(_tgt MATCHES "::" )
                continue()
            endif()
            if(_tgt MATCHES "^(test_|integ_|check_)")
                target_compile_options("${_tgt}" PRIVATE -UNDEBUG)
            endif()
        endforeach()
        get_property(_subs DIRECTORY "${_dir}" PROPERTY SUBDIRECTORIES)
        foreach(_sub IN LISTS _subs)
            airy_undef_ndebug_for_tests("${_sub}")
        endforeach()
    endfunction()
    airy_undef_ndebug_for_tests("${CMAKE_CURRENT_SOURCE_DIR}")
    airy_print_info("Test targets: assert() re-enabled via -UNDEBUG (NDEBUG no-op fix)")
endif()

# 自定义目标：清理构建
add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_CURRENT_BINARY_DIR}/_deps
    COMMENT "Cleaning all build artifacts"
)

# 构建后信息
airy_print_ok("AgentRT v${PROJECT_VERSION} configured successfully!")
airy_print_info("  Build directory: ${CMAKE_BINARY_DIR}")
airy_print_info("  Install prefix:  ${CMAKE_INSTALL_PREFIX}")
airy_print_info("Next steps:")
airy_print_info("  cmake --build . --config Release   # Build")
airy_print_info("  cmake --install . --config Release # Install")
airy_print_info("  ctest --output-on-failure          # Test")

# 检查常见问题
if(MSVC AND CMAKE_C_STANDARD EQUAL 11)
    airy_print_warn("MSVC may have limited C11 support. Consider Visual Studio 2019+.")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/atoms/CMakeLists.txt")
    airy_print_warn("atoms/CMakeLists.txt not found - atoms module may not build correctly")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/commons/CMakeLists.txt")
    airy_print_warn("commons/CMakeLists.txt not found - commons module may not build correctly")
endif()
