# =============================================================================
# compilerflags.cmake — Airymax 编译器标志统一配置模块
# 版本：1.0.0
# 创建：2026-07-06
# 归属：agentrt 管理仓直属（v0.1.2 起自伞仓迁入，IRON-9 [IND] 独立层）
#
# 设计目标：
#   统一管理 MSVC / GCC / Clang 三大编译器的安全编译选项、警告级别、
#   调试符号、优化级别、代码覆盖率、LTO 等配置。所有逻辑封装为可重用函数，
#   避免在 CMakeLists.txt 中散落 add_compile_options 调用。
#
# 使用方式（在 CMakeLists.txt 中）:
#   list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
#   include(compilerflags)
#   airy_apply_compiler_flags()
#   airy_apply_build_type_flags()
#   airy_apply_coverage()
# =============================================================================

include_guard(GLOBAL)

# 选项定义
option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)
option(ENABLE_LTO "Enable Link-Time Optimization for Release builds" ON)

# =============================================================================
# airy_apply_compiler_flags — 应用基础安全编译选项
# 跨平台：MSVC / GCC / Clang
# 包含：警告级别、安全选项（栈保护/FORTIFY）、链接器安全选项
# =============================================================================
function(airy_apply_compiler_flags)
    if(MSVC)
        # MSVC 安全选项
        add_compile_options(/W4 /WX- /permissive-)  # 最高警告级别，但警告不视为错误
        add_compile_definitions(_CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
        add_compile_options(/GS /sdl- /guard:cf /utf-8)  # 安全选项

        # 禁用特定警告
        add_compile_options(/wd4828)  # 文件包含在当前源字符集中无效的字符
        add_compile_options(/wd4996)  # 不安全函数警告（通过 _CRT_SECURE_NO_WARNINGS 处理）
        add_compile_options(/wd4255)  # 无函数原型
        add_compile_options(/wd4668)  # 未定义的宏替换为 0

        # 链接器安全选项
        add_link_options(/DYNAMICBASE /NXCOMPAT /GUARD:CF)
    else()
        # GCC/Clang 安全选项
        add_compile_options(-Wall -Wextra -Wpedantic
                            -Wno-unused-parameter
                            -Wno-stringop-overflow
                            -Wno-format-truncation)
        if(WARNINGS_AS_ERRORS)
            add_compile_options(-Werror=all -Werror=extra)
            add_compile_options(-Wno-error=format-truncation)
        endif()
        add_compile_options(-fstack-protector-strong -fno-omit-frame-pointer)
        add_compile_definitions(_GNU_SOURCE)

        # _FORTIFY_SOURCE=2 需要 -O1 或更高优化级别，Debug 构建时跳过
        if(NOT "${CMAKE_BUILD_TYPE}" STREQUAL "Debug" AND NOT "${CMAKE_BUILD_TYPE}" STREQUAL "")
            add_compile_definitions(_FORTIFY_SOURCE=2)
        endif()

        # 链接器安全选项
        if(UNIX AND NOT APPLE)
            add_link_options(-Wl,-z,relro -Wl,-z,now)
        elseif(APPLE)
            add_link_options(-Wl,-dead_strip)
        endif()
        add_link_options(-Wno-stringop-overflow)
    endif()
endfunction()

# =============================================================================
# airy_apply_compliance_strict — 应用 SE-01 合规严格模式
# 全局注入 banned_functions.h，禁止使用被毒化的不安全函数（memcpy/strncpy 等）
# 参数:
#   BANNED_HEADER — banned_functions.h 的绝对路径
# =============================================================================
function(airy_apply_compliance_strict)
    set(_options "")
    set(_one_value_args BANNED_HEADER)
    set(_multi_value_args "")
    cmake_parse_arguments(_ARG "${_options}" "${_one_value_args}" "${_multi_value_args}" ${ARGN})

    if(NOT _ARG_BANNED_HEADER)
        message(FATAL_ERROR "airy_apply_compliance_strict requires BANNED_HEADER argument")
    endif()

    if(NOT EXISTS "${_ARG_BANNED_HEADER}")
        message(FATAL_ERROR "banned_functions.h not found at: ${_ARG_BANNED_HEADER}")
    endif()

    if(NOT MSVC)
        add_compile_definitions(AIRY_COMPLIANCE_STRICT)
        add_compile_options("-include" "${_ARG_BANNED_HEADER}")
        message(STATUS "SE-01 compliance strict mode enabled: ${_ARG_BANNED_HEADER}")
    endif()
endfunction()

# =============================================================================
# airy_apply_build_type_flags — 应用构建类型相关的编译选项
# Debug: -g -O0 -fno-inline (GCC/Clang) 或 /Zi /Ob0 /Od /RTC1 (MSVC)
# Release: -O3 -flto -ffat-lto-objects (GCC/Clang) 或 /O2 /Ob2 /Oi /Ot /GL (MSVC)
# RelWithDebInfo: 同 Debug 的符号选项
# =============================================================================
function(airy_apply_build_type_flags)
    # MSVC Release 模式特殊处理：解决 /O2 与 /RTC1 冲突
    if(MSVC AND CMAKE_BUILD_TYPE STREQUAL "Release")
        string(REPLACE "/RTC1" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
        string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    endif()

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
            if(ENABLE_LTO)
                add_compile_options(-O3 -flto -ffat-lto-objects)
                add_link_options(-flto -ffat-lto-objects)
            else()
                add_compile_options(-O3)
            endif()
            add_compile_definitions(NDEBUG)
        endif()
    endif()
endfunction()

# =============================================================================
# airy_apply_coverage — 应用代码覆盖率编译选项
# 仅支持 GCC/Clang，MSVC 需要 Visual Studio Professional
# =============================================================================
function(airy_apply_coverage)
    if(NOT ENABLE_COVERAGE)
        return()
    endif()

    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fprofile-arcs -ftest-coverage)
        add_link_options(-fprofile-arcs -ftest-coverage -lgcov)
        message(STATUS "Code coverage enabled for ${CMAKE_C_COMPILER_ID}")
    elseif(MSVC)
        message(WARNING "Code coverage for MSVC requires Visual Studio Professional edition")
    else()
        message(WARNING "Code coverage only supported for GCC/Clang and MSVC compilers")
    endif()
endfunction()

# =============================================================================
# airy_apply_all_compiler_flags — 一键应用所有编译器配置
# 推荐在顶级 CMakeLists.txt 中调用
# 参数:
#   BANNED_HEADER — banned_functions.h 的绝对路径（可选，用于 SE-01 合规）
# =============================================================================
function(airy_apply_all_compiler_flags)
    set(_options "")
    set(_one_value_args BANNED_HEADER)
    set(_multi_value_args "")
    cmake_parse_arguments(_ARG "${_options}" "${_one_value_args}" "${_multi_value_args}" ${ARGN})

    airy_apply_compiler_flags()

    if(_ARG_BANNED_HEADER)
        airy_apply_compliance_strict(BANNED_HEADER "${_ARG_BANNED_HEADER}")
    endif()

    airy_apply_build_type_flags()
    airy_apply_coverage()
endfunction()
