# AgentRT - 智能体运行时平台
# 根目录构建配置文件
# Copyright (c) 2026 SPHARX. All Rights Reserved.

cmake_minimum_required(VERSION 3.20)
project(AgentRT VERSION 0.1.1 LANGUAGES C CXX)

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
# v4.0: 29 仓拆分后 scripts/dev/cmake/ 已移除，agentrt 特有 CMake 模块归位至 cmake/
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/agentrt_print.cmake)

# 项目范围选项
option(BUILD_SHARED_LIBS "Build shared libraries instead of static libraries" OFF)
option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(BUILD_TESTS "Build unit tests" ON)
option(ENABLE_COVERAGE "Enable code coverage reporting" OFF)
option(ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" ON)
option(AGENTRT_BUILD_ALL "Build all AgentRT components" ON)
option(AGENTRT_WITH_MEMORYROVOL "Build with MemoryRovol commercial memory provider" OFF)
# 兼容别名：允许使用 AGENTRT_WITH_MEMORYROVOL（旧名）
if(DEFINED AGENTRT_WITH_MEMORYROVOL AND NOT DEFINED AGENTRT_WITH_MEMORYROVOL)
    set(AGENTRT_WITH_MEMORYROVOL ${AGENTRT_WITH_MEMORYROVOL})
endif()
option(AGENTRT_COMPLIANCE_STRICT "Enable strict compliance mode (poison unsafe functions)" ON)
set(AGENTRT_MEMORY_BACKEND "builtin" CACHE STRING "Memory backend (builtin|memoryrovol)")
set(AGENTRT_DOCKER_BUILD OFF CACHE BOOL "Docker build mode")

# CTest必须在顶级启用（在任何add_subdirectory之前）
# R-09-SEC: Strict compliance mode and tests are now independent.
# Tests MUST run under strict compliance to catch BAN violations at compile time.
# Previously, strict mode forced BUILD_TESTS=OFF, which prevented test execution
# in the default configuration. This was a design error (BAN-181 compliance fix).
if(BUILD_TESTS)
    if(AGENTRT_COMPLIANCE_STRICT)
        agentrt_print_ok("Tests enabled with strict compliance (AGENTRT_COMPLIANCE_STRICT=ON)")
    endif()
endif()
if(BUILD_TESTS)
    enable_testing()
    agentrt_print_info("CTest enabled at root level (before subdirectories)")
    # 为所有测试设置 sanitizer 环境变量（set(ENV{}) 不会传递给 ctest）
    if(ENABLE_SANITIZERS AND NOT MSVC)
        set(AGENTRT_ASAN_OPTIONS "halt_on_error=1:detect_stack_use_after_return=1:detect_leaks=1:allocator_may_return_null=1")
        set(AGENTRT_LSAN_OPTIONS "suppressions=${CMAKE_SOURCE_DIR}/../ecosystem/manager/sanitizer/lsan-suppressions:print_suppressions=0")
        set(AGENTRT_UBSAN_OPTIONS "halt_on_error=1:print_stacktrace=1")
    endif()
endif()

# 输出构建配置信息
agentrt_print_section("AgentRT v${PROJECT_VERSION} Build Configuration")
agentrt_print_info("Build type:       ${CMAKE_BUILD_TYPE}")
agentrt_print_info("Platform:         ${CMAKE_SYSTEM_NAME} (${CMAKE_SYSTEM_PROCESSOR})")
agentrt_print_info("Compiler:         ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
agentrt_print_info("Shared libs:      ${BUILD_SHARED_LIBS}")
agentrt_print_info("Build tests:      ${BUILD_TESTS}")
agentrt_print_info("Compliance strict:${AGENTRT_COMPLIANCE_STRICT}")
agentrt_print_info("Memory backend:   ${AGENTRT_MEMORY_BACKEND}")
agentrt_print_info("MemoryRovol Pro:  ${AGENTRT_WITH_MEMORYROVOL}")
agentrt_print_info("Docker build:     ${AGENTRT_DOCKER_BUILD}")
agentrt_print_info("Sanitizers:       ${ENABLE_SANITIZERS}")

# 关键路径验证：确认核心目录存在
agentrt_print_info("Path check: Source root = ${CMAKE_CURRENT_SOURCE_DIR}")
foreach(_dir atoms commons protocols gateway daemons cupolas heapstore)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_dir}")
        agentrt_print_ok("  ${_dir}/")
    else()
        agentrt_print_error("  ${_dir}/ — MISSING!")
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
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
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
    # _FORTIFY_SOURCE=2 需要 -O1 或更高优化级别，Debug 构建时跳过以避免
    # "_FORTIFY_SOURCE requires compiling with optimization" 警告
    if(NOT "${CMAKE_BUILD_TYPE}" STREQUAL "Debug" AND NOT "${CMAKE_BUILD_TYPE}" STREQUAL "")
        add_compile_definitions(_FORTIFY_SOURCE=2)
    endif()

    if(AGENTRT_COMPLIANCE_STRICT)
        add_compile_definitions(AGENTRT_COMPLIANCE_STRICT AGENTRT_COMPLIANCE_STRICT)
        add_compile_options("-include" "${CMAKE_CURRENT_SOURCE_DIR}/commons/utils/compliance/include/banned_functions.h")

        # 注意：内存子系统实现层的 AGENTRT_COMPLIANCE_IMPL 豁免由各子目录的
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
        add_compile_options(-fsanitize=address,undefined,leak,float-divide-by-zero -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined,leak,float-divide-by-zero)
        # CI环境变量：halt_on_error确保任何检测到的问题立即阻断
        set(ENV{ASAN_OPTIONS} "halt_on_error=1:detect_stack_use_after_return=1:detect_leaks=1")
        set(ENV{LSAN_OPTIONS} "suppressions=${CMAKE_SOURCE_DIR}/../ecosystem/manager/sanitizer/lsan-suppressions")
        set(ENV{UBSAN_OPTIONS} "halt_on_error=1:print_stacktrace=1")
        agentrt_print_ok("Sanitizers enabled: ASan + LSan + UBSan")
    else()
        agentrt_print_warn("Sanitizers only supported for GCC/Clang compilers")
    endif()
endif()

# ThreadSanitizer 独立选项（不与 ASan 同时使用）
option(ENABLE_TSAN "Enable ThreadSanitizer (disables other sanitizers)" OFF)
if(ENABLE_TSAN AND NOT MSVC)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
        add_link_options(-fsanitize=thread)
        set(ENV{TSAN_OPTIONS} "halt_on_error=1:history_size=7:second_deadlock_stack=1")
        agentrt_print_ok("ThreadSanitizer enabled")
    endif()
endif()

# MSVC Release模式特殊处理：解决/O2与/RTC1冲突
if(MSVC AND CMAKE_BUILD_TYPE STREQUAL "Release")
    # 移除默认的/RTC1标志（如果存在），因为与/O2不兼容
    string(REPLACE "/RTC1" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
    string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    agentrt_print_info("Adjusted MSVC Release flags to avoid /O2 and /RTC1 conflict")
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
        add_compile_options(-O3 -flto -ffat-lto-objects)
        add_compile_definitions(NDEBUG)
        add_link_options(-flto -ffat-lto-objects)
    endif()
endif()

# 代码覆盖率支持
if(ENABLE_COVERAGE)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fprofile-arcs -ftest-coverage)
        add_link_options(-fprofile-arcs -ftest-coverage -lgcov)
        agentrt_print_ok("Code coverage enabled for ${CMAKE_C_COMPILER_ID}")
    elseif(MSVC)
        agentrt_print_warn("Code coverage for MSVC requires Visual Studio professional edition")
    else()
        agentrt_print_warn("Code coverage only supported for GCC/Clang and MSVC compilers")
    endif()
endif()

# 查找必需的系统依赖
if(WIN32)
    agentrt_print_info("Using Windows built-in thread support")
else()
    find_package(Threads REQUIRED)
    agentrt_print_ok("Threads found: ${CMAKE_THREAD_LIBS_INIT}")
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
    set(AGENTRT_HAS_SQLITE3 TRUE CACHE BOOL "SQLite3 available")
    agentrt_print_ok("SQLite3 found: ${SQLite3_LIBRARIES}${SQLITE3_LIBRARIES}")
else()
    set(AGENTRT_HAS_SQLITE3 FALSE CACHE BOOL "SQLite3 available")
    agentrt_print_warn("SQLite3 not found, registry/vector-store features disabled")
endif()

# --- cJSON ---
find_package(cJSON QUIET)
if(NOT cJSON_FOUND AND PkgConfig_FOUND)
    pkg_check_modules(CJSON QUIET libcjson)
endif()
if(cJSON_FOUND OR CJSON_FOUND)
    set(AGENTRT_HAS_CJSON TRUE CACHE BOOL "cJSON available")
    agentrt_print_ok("cJSON found: ${cJSON_VERSION}${CJSON_VERSION}")
else()
    set(AGENTRT_HAS_CJSON FALSE CACHE BOOL "cJSON available")
    agentrt_print_warn("cJSON not found, JSON features disabled")
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
    set(AGENTRT_HAS_YAML TRUE CACHE BOOL "libyaml available")
    agentrt_print_ok("libyaml found: ${YAML_LIBRARIES}")
else()
    set(AGENTRT_HAS_YAML FALSE CACHE BOOL "libyaml available")
    agentrt_print_warn("libyaml not found, YAML config features disabled")
endif()

# --- OpenSSL ---
find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
    set(AGENTRT_HAS_OPENSSL TRUE CACHE BOOL "OpenSSL available")
    agentrt_print_ok("OpenSSL found: ${OpenSSL_VERSION}")
else()
    set(AGENTRT_HAS_OPENSSL FALSE CACHE BOOL "OpenSSL available")
    agentrt_print_warn("OpenSSL not found, crypto features disabled")
endif()

# --- libcurl ---
find_package(CURL QUIET)
if(NOT CURL_FOUND AND PkgConfig_FOUND)
    pkg_check_modules(LIBCURL QUIET libcurl)
endif()
if(CURL_FOUND OR LIBCURL_FOUND)
    set(AGENTRT_HAS_CURL TRUE CACHE BOOL "libcurl available")
    agentrt_print_ok("libcurl found: ${CURL_LIBRARIES}${LIBCURL_LIBRARIES}")
else()
    set(AGENTRT_HAS_CURL FALSE CACHE BOOL "libcurl available")
    agentrt_print_warn("libcurl not found, HTTP client features disabled")
endif()

# --- libmicrohttpd ---
find_package(libmicrohttpd QUIET)
if(NOT libmicrohttpd_FOUND AND PkgConfig_FOUND)
    pkg_check_modules(MICROHTTPD QUIET libmicrohttpd)
endif()
if(libmicrohttpd_FOUND OR MICROHTTPD_FOUND)
    set(AGENTRT_HAS_MICROHTTPD TRUE CACHE BOOL "libmicrohttpd available")
    agentrt_print_ok("libmicrohttpd found")
else()
    set(AGENTRT_HAS_MICROHTTPD FALSE CACHE BOOL "libmicrohttpd available")
    agentrt_print_warn("libmicrohttpd not found, HTTP gateway may be limited")
endif()

# --- libwebsockets ---
find_package(libwebsockets QUIET)
if(NOT libwebsockets_FOUND AND PkgConfig_FOUND)
    pkg_check_modules(LIBWEBSOCKETS QUIET libwebsockets)
endif()
if(libwebsockets_FOUND OR LIBWEBSOCKETS_FOUND)
    set(AGENTRT_HAS_LIBWEBSOCKETS TRUE CACHE BOOL "libwebsockets available")
    agentrt_print_ok("libwebsockets found")
else()
    set(AGENTRT_HAS_LIBWEBSOCKETS FALSE CACHE BOOL "libwebsockets available")
    agentrt_print_warn("libwebsockets not found, WebSocket features disabled")
endif()

# --- libevent ---
find_package(libevent CONFIG QUIET)
if(NOT libevent_FOUND AND PkgConfig_FOUND)
    pkg_check_modules(LIBEVENT QUIET libevent)
endif()
if(libevent_FOUND OR LIBEVENT_FOUND)
    set(AGENTRT_HAS_LIBEVENT TRUE CACHE BOOL "libevent available")
    agentrt_print_ok("libevent found")
else()
    set(AGENTRT_HAS_LIBEVENT FALSE CACHE BOOL "libevent available")
    agentrt_print_warn("libevent not found, event-loop features disabled")
endif()

# --- GTest ---
if(BUILD_TESTS)
    find_package(GTest QUIET)
    if(GTest_FOUND)
        set(AGENTRT_HAS_GTEST TRUE CACHE BOOL "GTest available")
        agentrt_print_ok("GTest found: ${GTEST_LIBRARIES}")
    else()
        set(AGENTRT_HAS_GTEST FALSE CACHE BOOL "GTest available")
        agentrt_print_warn("GTest not found, tests will use custom framework")
    endif()
endif()

# --- CMocka ---
if(BUILD_TESTS)
    find_package(cmocka QUIET)
    if(NOT cmocka_FOUND)
        find_package(Cmocka QUIET)
    endif()
    if(cmocka_FOUND OR CMOCKA_FOUND)
        set(AGENTRT_HAS_CMOCKA TRUE CACHE BOOL "cmocka available")
        if(NOT CMOCKA_LIBRARIES)
            set(CMOCKA_LIBRARIES ${cmocka_LIBRARIES} CACHE STRING "cmocka libraries" FORCE)
        endif()
        if(NOT CMOCKA_INCLUDE_DIRS)
            set(CMOCKA_INCLUDE_DIRS ${cmocka_INCLUDE_DIRS} CACHE STRING "cmocka include dirs" FORCE)
        endif()
        agentrt_print_ok("cmocka found")
    else()
        set(AGENTRT_HAS_CMOCKA FALSE CACHE BOOL "cmocka available")
        agentrt_print_warn("cmocka not found, some tests may use stub framework")
    endif()
endif()

# 创建 INTERFACE 库用于传递全局依赖编译定义
add_library(agentrt_compile_defs INTERFACE)
if(AGENTRT_HAS_SQLITE3)
    target_compile_definitions(agentrt_compile_defs INTERFACE AGENTRT_HAS_SQLITE3)
endif()
if(AGENTRT_HAS_CJSON)
    target_compile_definitions(agentrt_compile_defs INTERFACE AGENTRT_HAS_CJSON)
endif()
if(AGENTRT_HAS_YAML)
    target_compile_definitions(agentrt_compile_defs INTERFACE AGENTRT_HAS_YAML)
endif()
if(AGENTRT_HAS_OPENSSL)
    target_compile_definitions(agentrt_compile_defs INTERFACE AGENTRT_HAS_OPENSSL)
endif()
if(AGENTRT_HAS_CURL)
    target_compile_definitions(agentrt_compile_defs INTERFACE AGENTRT_HAS_CURL)
endif()
if(AGENTRT_HAS_MICROHTTPD)
    target_compile_definitions(agentrt_compile_defs INTERFACE AGENTRT_HAS_MICROHTTPD)
endif()
if(AGENTRT_HAS_LIBWEBSOCKETS)
    target_compile_definitions(agentrt_compile_defs INTERFACE AGENTRT_HAS_LIBWEBSOCKETS)
endif()
if(AGENTRT_HAS_LIBEVENT)
    target_compile_definitions(agentrt_compile_defs INTERFACE AGENTRT_HAS_LIBEVENT)
endif()

# ============================================================
# 跨平台系统库统一管理（BAN-15: 禁止硬编码平台库）
# agentrt_platform_libs, dl, pthread, m, ws2_32 等通过此 INTERFACE 库统一提供
# ============================================================
add_library(agentrt_platform_libs INTERFACE)
if(UNIX AND NOT APPLE)
    target_link_libraries(agentrt_platform_libs INTERFACE dl)
endif()
if(WIN32)
    target_link_libraries(agentrt_platform_libs INTERFACE ws2_32)
endif()

# ============================================================
# 测试链接选项统一管理（BAN-26: 禁止硬编码链接器标志）
# --allow-multiple-definition 仅在 Linux/GNU ld 下有效
# ============================================================
add_library(agentrt_test_ldflags INTERFACE)
if(UNIX AND NOT APPLE)
    target_link_options(agentrt_test_ldflags INTERFACE -Wl,--allow-multiple-definition)
endif()

# 包含子目录
if(AGENTRT_BUILD_ALL)
    agentrt_print_section("Building AgentRT core components")

    # 全局include路径（确保所有模块都能找到commons的头文件）
    include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/commons
        ${CMAKE_CURRENT_SOURCE_DIR}/commons/utils/include
        ${CMAKE_CURRENT_SOURCE_DIR}/commons/utils/compat/include
        ${CMAKE_CURRENT_SOURCE_DIR}/commons/utils/memory/include
        ${CMAKE_CURRENT_SOURCE_DIR}/commons/utils/string/include
        ${CMAKE_CURRENT_SOURCE_DIR}/commons/utils/logging/include
        ${CMAKE_CURRENT_SOURCE_DIR}/commons/utils/error/include
        ${CMAKE_CURRENT_SOURCE_DIR}/commons/platform/include
        ${CMAKE_CURRENT_SOURCE_DIR}/atoms/corekern/include
        ${CMAKE_CURRENT_SOURCE_DIR}/protocols/include
        ${CMAKE_CURRENT_SOURCE_DIR}/daemons/common/include
        ${CMAKE_CURRENT_SOURCE_DIR}/gateway/src
    )

    # 验证全局 include 路径是否存在
    agentrt_print_info("Verifying global include directories...")
    set(_global_inc_dirs
        commons
        commons/utils/include
        commons/utils/compat/include
        commons/utils/memory/include
        commons/utils/string/include
        commons/utils/logging/include
        commons/utils/error/include
        commons/platform/include
        atoms/corekern/include
        protocols/include
        daemons/common/include
        gateway/src
    )
    foreach(_inc ${_global_inc_dirs})
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_inc}")
            agentrt_print_ok("  ${_inc}")
        else()
            agentrt_print_error("  ${_inc} — MISSING!")
        endif()
    endforeach()

    # 依赖检测汇总
    agentrt_print_section("Dependency Detection Summary")
    foreach(_dep_info
            "SQLite3:       ${AGENTRT_HAS_SQLITE3}"
            "cJSON:         ${AGENTRT_HAS_CJSON}"
            "libyaml:       ${AGENTRT_HAS_YAML}"
            "OpenSSL:       ${AGENTRT_HAS_OPENSSL}"
            "libcurl:       ${AGENTRT_HAS_CURL}"
            "libmicrohttpd: ${AGENTRT_HAS_MICROHTTPD}"
            "libwebsockets: ${AGENTRT_HAS_LIBWEBSOCKETS}"
            "libevent:      ${AGENTRT_HAS_LIBEVENT}")
        if(_dep_info MATCHES ": +(ON|YES|TRUE|1)")
            agentrt_print_ok("  ${_dep_info}")
        else()
            agentrt_print_warn("  ${_dep_info}")
        endif()
    endforeach()

    # 主要AgentRT组件（原 agentrt/agentrt/CMakeLists.txt，取消嵌套后合并至此）
    agentrt_print_info("Configuring AgentRT core components...")

    # 从根 CMakeLists.txt 传播依赖可用性标志
    if(AGENTRT_HAS_CJSON)
        set(AGENTRT_HAS_CJSON TRUE CACHE BOOL "cJSON available for agentrt")
    endif()
    if(AGENTRT_HAS_CURL)
        set(AGENTRT_HAS_CURL TRUE CACHE BOOL "CURL available for agentrt")
    endif()
    if(AGENTRT_HAS_YAML)
        set(AGENTRT_HAS_YAML TRUE CACHE BOOL "libyaml available for agentrt")
    endif()
    if(AGENTRT_HAS_OPENSSL)
        set(AGENTRT_HAS_OPENSSL TRUE CACHE BOOL "OpenSSL available for agentrt")
    endif()
    if(AGENTRT_HAS_MICROHTTPD)
        set(AGENTRT_HAS_MICROHTTPD TRUE CACHE BOOL "libmicrohttpd available for agentrt")
    endif()
    if(AGENTRT_HAS_LIBWEBSOCKETS)
        set(AGENTRT_HAS_LIBWEBSOCKETS TRUE CACHE BOOL "libwebsockets available for agentrt")
    endif()

    # 子模块启用状态汇总
    agentrt_print_info("[MODULE] Submodule build status:")
    agentrt_print_info("[MODULE]   atoms:    ${BUILD_ATOMS}")
    agentrt_print_info("[MODULE]   commons:  ${BUILD_COMMONS}")
    agentrt_print_info("[MODULE]   cupolas:  ${BUILD_CUPOLAS}")
    agentrt_print_info("[MODULE]   daemon:   ${BUILD_DAEMON}")
    agentrt_print_info("[MODULE]   gateway:  ${BUILD_GATEWAY}")
    agentrt_print_info("[MODULE]   heapstore: ${BUILD_HEAPSTORE}")
    agentrt_print_info("[MODULE]   toolkit:  ${BUILD_TOOLKIT}")
    agentrt_print_info("[MODULE]   Memory backend: ${AGENTRT_MEMORY_BACKEND}")
    agentrt_print_info("[MODULE]   With MemoryRovol: ${AGENTRT_WITH_MEMORYROVOL}")

    # 全局特性测试宏定义 - 必须在所有其他配置之前
    if(UNIX AND NOT APPLE)
        add_compile_definitions(
            _POSIX_C_SOURCE=200809L
            _XOPEN_SOURCE=700
            _GNU_SOURCE
        )
        agentrt_print_ok("POSIX feature test macros enabled for Linux")
    elseif(APPLE)
        add_compile_definitions(
            _POSIX_C_SOURCE=200112L
            _DARWIN_C_SOURCE
        )
        agentrt_print_ok("POSIX feature test macros enabled for macOS")
    endif()

    if(MSVC)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/windows_preinclude.h")
            agentrt_print_ok("[MSVC] Force-include file found: cmake/windows_preinclude.h")
        else()
            agentrt_print_warn("[MSVC] Force-include file NOT FOUND: cmake/windows_preinclude.h")
        endif()
        add_compile_options(/FI"${CMAKE_CURRENT_SOURCE_DIR}/cmake/windows_preinclude.h")
    endif()

    # 模块选项设置
    option(BUILD_ATOMS "Build atoms module (core kernel components)" ON)
    option(BUILD_COMMONS "Build commons module (common utilities)" ON)
    option(BUILD_CUPOLAS "Build cupolas module (security framework)" ON)
    option(BUILD_DAEMON "Build daemon modules (system services)" ON)
    option(BUILD_GATEWAY "Build gateway module (API gateway)" ON)
    option(BUILD_HEAPSTORE "Build heapstore module (persistent storage)" ON)
    option(BUILD_TOOLKIT "Build toolkit modules (language SDKs)" OFF)

    # 包含子目录（构建顺序：commons 优先于 atoms）
    if(BUILD_COMMONS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/commons/CMakeLists.txt")
        add_subdirectory(commons)
    else()
        agentrt_print_warn("commons module not found or disabled")
    endif()

    if(BUILD_ATOMS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/atoms/CMakeLists.txt")
        add_subdirectory(atoms)
    else()
        agentrt_print_warn("atoms module not found or disabled")
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/protocols/CMakeLists.txt")
        add_subdirectory(protocols)
    endif()

    if(BUILD_CUPOLAS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cupolas/CMakeLists.txt")
        add_subdirectory(cupolas)
    else()
        agentrt_print_warn("cupolas module not found or disabled")
    endif()

    if(BUILD_DAEMON AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/daemons/CMakeLists.txt")
        add_subdirectory(daemons)
    else()
        agentrt_print_warn("daemons module not found or disabled")
    endif()

    if(BUILD_GATEWAY AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/gateway/CMakeLists.txt")
        add_subdirectory(gateway)
    else()
        agentrt_print_warn("gateway module not found or disabled")
    endif()

    if(BUILD_HEAPSTORE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/heapstore/CMakeLists.txt")
        add_subdirectory(heapstore)
    else()
        agentrt_print_warn("heapstore module not found or disabled")
    endif()

    # toolkit 包含多种语言 SDK，需要特殊处理
    if(BUILD_TOOLKIT)
        agentrt_print_info("Toolkit modules require additional configuration for each language SDK")
    endif()

    # 创建聚合目标
    add_custom_target(agentrt-all
        DEPENDS
            $<IF:$<TARGET_EXISTS:agentrt_atoms>,agentrt_atoms,>
            $<IF:$<TARGET_EXISTS:agentrt_commons>,agentrt_commons,>
            $<IF:$<TARGET_EXISTS:agentrt_cupolas>,agentrt_cupolas,>
        COMMENT "Build all AgentRT core components"
    )

    # 安装头文件
    install(DIRECTORY atoms/corekern/include/ DESTINATION include/agentrt/corekern)
    install(DIRECTORY atoms/coreloopthree/include/ DESTINATION include/agentrt/coreloopthree)
    install(DIRECTORY atoms/syscall/include/ DESTINATION include/agentrt/syscall)
    install(DIRECTORY commons/utils/memory/include/ DESTINATION include/agentrt/commons/memory)
    install(DIRECTORY commons/utils/string/include/ DESTINATION include/agentrt/commons/string)
    install(DIRECTORY commons/utils/logging/include/ DESTINATION include/agentrt/commons/logging)

    agentrt_print_ok("AgentRT core components configuration complete")

    # 测试组件（可选）
    if(BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
        add_subdirectory(tests)
    endif()
endif()

# 安装配置
if(AGENTRT_BUILD_ALL)
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
    agentrt_print_info("Test wrapper script: ${CMAKE_BINARY_DIR}/run_tests.sh")
endif()

# 自定义目标：清理构建
add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_CURRENT_BINARY_DIR}/_deps
    COMMENT "Cleaning all build artifacts"
)

# 构建后信息
agentrt_print_ok("AgentRT v${PROJECT_VERSION} configured successfully!")
agentrt_print_info("  Build directory: ${CMAKE_BINARY_DIR}")
agentrt_print_info("  Install prefix:  ${CMAKE_INSTALL_PREFIX}")
agentrt_print_info("Next steps:")
agentrt_print_info("  cmake --build . --config Release   # Build")
agentrt_print_info("  cmake --install . --config Release # Install")
agentrt_print_info("  ctest --output-on-failure          # Test")

# 检查常见问题
if(MSVC AND CMAKE_C_STANDARD EQUAL 11)
    agentrt_print_warn("MSVC may have limited C11 support. Consider Visual Studio 2019+.")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/atoms/CMakeLists.txt")
    agentrt_print_warn("atoms/CMakeLists.txt not found - atoms module may not build correctly")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/commons/CMakeLists.txt")
    agentrt_print_warn("commons/CMakeLists.txt not found - commons module may not build correctly")
endif()
