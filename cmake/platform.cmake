# =============================================================================
# platform.cmake — Airymax 平台检测与特性宏配置模块
# 版本：1.0.0
# 创建：2026-07-06
# 归属：agentrt 管理仓直属（v0.1.2 起自伞仓迁入，IRON-9 [IND] 独立层）
#
# 设计目标：
#   统一检测目标平台（Linux/macOS/Windows/MSVC/GCC/Clang），并设置对应的
#   POSIX 特性测试宏、平台定义宏、编译器特性变量。所有平台相关逻辑集中
#   在此模块，避免在 CMakeLists.txt 中散落 if(UNIX)/if(APPLE)/if(MSVC) 判断。
#
#   符合 Airymax 跨 Linux/macOS/Windows 硬约束（用户态运行时，通过 libc/POSIX
#   跨平台，禁止内核态化）。
#
# 使用方式（在 CMakeLists.txt 中）:
#   list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
#   include(platform)
#   airy_detect_platform()
# =============================================================================

include_guard(GLOBAL)

# =============================================================================
# airy_detect_platform — 检测平台并设置特性宏
# 设置以下变量（PARENT_SCOPE）:
#   AIRY_PLATFORM_LINUX / AIRY_PLATFORM_MACOS / AIRY_PLATFORM_WINDOWS
#   AIRY_COMPILER_GCC / AIRY_COMPILER_CLANG / AIRY_COMPILER_MSVC
#   AIRY_PLATFORM_NAME — 人类可读的平台名
# 同时调用 add_compile_definitions 设置 POSIX 特性宏
# =============================================================================
function(airy_detect_platform)
    # ---- 平台检测 ----
    set(AIRY_PLATFORM_LINUX OFF PARENT_SCOPE)
    set(AIRY_PLATFORM_MACOS OFF PARENT_SCOPE)
    set(AIRY_PLATFORM_WINDOWS OFF PARENT_SCOPE)

    if(UNIX AND NOT APPLE)
        set(AIRY_PLATFORM_LINUX ON PARENT_SCOPE)
        set(AIRY_PLATFORM_NAME "Linux" PARENT_SCOPE)
    elseif(APPLE)
        set(AIRY_PLATFORM_MACOS ON PARENT_SCOPE)
        set(AIRY_PLATFORM_NAME "macOS" PARENT_SCOPE)
    elseif(WIN32)
        set(AIRY_PLATFORM_WINDOWS ON PARENT_SCOPE)
        set(AIRY_PLATFORM_NAME "Windows" PARENT_SCOPE)
    else()
        set(AIRY_PLATFORM_NAME "Unknown" PARENT_SCOPE)
    endif()

    # ---- 编译器检测 ----
    set(AIRY_COMPILER_GCC OFF PARENT_SCOPE)
    set(AIRY_COMPILER_CLANG OFF PARENT_SCOPE)
    set(AIRY_COMPILER_MSVC OFF PARENT_SCOPE)

    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        set(AIRY_COMPILER_GCC ON PARENT_SCOPE)
    elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
        set(AIRY_COMPILER_CLANG ON PARENT_SCOPE)
    elseif(MSVC)
        set(AIRY_COMPILER_MSVC ON PARENT_SCOPE)
    endif()

    # ---- POSIX 特性测试宏 ----
    # Airymax 跨 Linux/macOS/Windows 硬约束：用户态运行时，通过 libc/POSIX 跨平台
    if(UNIX AND NOT APPLE)
        add_compile_definitions(
            _POSIX_C_SOURCE=200809L
            _XOPEN_SOURCE=700
            _GNU_SOURCE
        )
        message(STATUS "POSIX feature test macros enabled for Linux")
    elseif(APPLE)
        add_compile_definitions(
            _POSIX_C_SOURCE=200112L
            _DARWIN_C_SOURCE
        )
        message(STATUS "POSIX feature test macros enabled for macOS")
    endif()
endfunction()

# =============================================================================
# airy_print_platform_info — 打印平台信息摘要
# =============================================================================
function(airy_print_platform_info)
    message(STATUS "=========================================")
    message(STATUS "  Airymax Platform Detection Summary")
    message(STATUS "=========================================")
    message(STATUS "Platform:      ${CMAKE_SYSTEM_NAME} (${CMAKE_SYSTEM_PROCESSOR})")
    message(STATUS "Platform flag: ${AIRY_PLATFORM_NAME}")
    message(STATUS "Compiler:      ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
    if(AIRY_COMPILER_GCC)
        message(STATUS "Compiler flag: GCC")
    elseif(AIRY_COMPILER_CLANG)
        message(STATUS "Compiler flag: Clang")
    elseif(AIRY_COMPILER_MSVC)
        message(STATUS "Compiler flag: MSVC")
    endif()
    message(STATUS "Build type:    ${CMAKE_BUILD_TYPE}")
    message(STATUS "=========================================")
endfunction()

# =============================================================================
# airy_is_unix_like — 检查是否为 Unix-like 系统（Linux/macOS/BSD）
# 参数:
#   result_var — 输出变量名，设置为 ON 或 OFF
# =============================================================================
function(airy_is_unix_like result_var)
    if(UNIX)
        set(${result_var} ON PARENT_SCOPE)
    else()
        set(${result_var} OFF PARENT_SCOPE)
    endif()
endfunction()

# =============================================================================
# airy_supports_sanitizers — 检查当前编译器是否支持 sanitizers
# 参数:
#   result_var — 输出变量名，设置为 ON 或 OFF
# =============================================================================
function(airy_supports_sanitizers result_var)
    if(MSVC)
        set(${result_var} OFF PARENT_SCOPE)
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        set(${result_var} ON PARENT_SCOPE)
    else()
        set(${result_var} OFF PARENT_SCOPE)
    endif()
endfunction()
