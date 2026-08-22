# =============================================================================
# sanitizers.cmake — Airymax 运行时检测模块 (SEC-05/06/08/09)
# 版本：1.0.0
# 创建：2026-07-06
# 归属：agentrt 管理仓直属（v0.1.2 起自伞仓迁入，IRON-9 [IND] 独立层）
#
# 统一管理所有运行时检测工具的编译选项:
#   - AddressSanitizer (ASan) — 堆/栈缓冲区溢出、use-after-free 检测
#   - LeakSanitizer (LSan) — 内存泄漏检测
#   - UndefinedBehaviorSanitizer (UBSan) — 整数溢出、除零、未定义行为检测
#   - ThreadSanitizer (TSan) — 数据竞争、死锁检测
#   - 栈保护 (-fstack-protector-strong)
#   - FORTIFY_SOURCE=2 缓冲区溢出编译时检测
#
# 使用方式（在 CMakeLists.txt 中）:
#   list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
#   include(sanitizers)
#   enable_airy_sanitizers(TARGET my_target SCOPE PRIVATE)
# =============================================================================

include_guard(GLOBAL)

# =============================================================================
# 顶层选项定义（供 cmake -D 覆盖）
# =============================================================================
option(AIRY_ENABLE_ASAN  "Enable AddressSanitizer + LeakSanitizer" ON)
option(AIRY_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" ON)
option(AIRY_ENABLE_TSAN  "Enable ThreadSanitizer (disables ASan/LSan)" OFF)
option(AIRY_ENABLE_STACK_PROTECTOR "Enable -fstack-protector-strong" ON)
option(AIRY_ENABLE_FORTIFY "Enable _FORTIFY_SOURCE=2" ON)

# 兼容旧版选项名（过渡期，v1.0.1 后移除）
if(DEFINED ENABLE_SANITIZERS AND NOT DEFINED AIRY_ENABLE_ASAN)
    set(AIRY_ENABLE_ASAN ${ENABLE_SANITIZERS})
endif()
if(DEFINED ENABLE_TSAN)
    set(AIRY_ENABLE_TSAN ${ENABLE_TSAN})
endif()

# =============================================================================
# 平台检查
# =============================================================================
function(airy_check_sanitizer_support)
    if(MSVC)
        message(WARNING "Sanitizers are not supported on MSVC. Disabling all sanitizers.")
        set(AIRY_ENABLE_ASAN OFF PARENT_SCOPE)
        set(AIRY_ENABLE_UBSAN OFF PARENT_SCOPE)
        set(AIRY_ENABLE_TSAN OFF PARENT_SCOPE)
        return()
    endif()

    if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        message(WARNING "Sanitizers require GCC or Clang. Found: ${CMAKE_C_COMPILER_ID}")
        set(AIRY_ENABLE_ASAN OFF PARENT_SCOPE)
        set(AIRY_ENABLE_UBSAN OFF PARENT_SCOPE)
        set(AIRY_ENABLE_TSAN OFF PARENT_SCOPE)
        return()
    endif()
endfunction()

# =============================================================================
# AddressSanitizer + LeakSanitizer (SEC-05)
# =============================================================================
function(airy_enable_asan target scope)
    if(NOT AIRY_ENABLE_ASAN)
        return()
    endif()

    message(STATUS "Enabling AddressSanitizer + LeakSanitizer for target: ${target}")

    target_compile_options(${target} ${scope}
        -fsanitize=address
        -fno-omit-frame-pointer
        -fno-optimize-sibling-calls
    )

    target_link_options(${target} ${scope}
        -fsanitize=address
    )

    # 运行时环境变量（通过 CMake 属性传递给 ctest）
    # suppression 文件路径对齐 29 仓拆分后的 ecosystem/manager 仓
    set(_lsan_suppression "${CMAKE_SOURCE_DIR}/ecosystem/manager/sanitizer/lsan-suppressions")

    set_target_properties(${target} PROPERTIES
        ASAN_OPTIONS "halt_on_error=1:detect_stack_use_after_return=1:detect_leaks=1:allocator_may_return_null=1"
        LSAN_OPTIONS "suppressions=${_lsan_suppression}:print_suppressions=0"
    )
endfunction()

# =============================================================================
# UndefinedBehaviorSanitizer (SEC-09)
# =============================================================================
function(airy_enable_ubsan target scope)
    if(NOT AIRY_ENABLE_UBSAN)
        return()
    endif()

    message(STATUS "Enabling UndefinedBehaviorSanitizer for target: ${target}")

    target_compile_options(${target} ${scope}
        -fsanitize=undefined
        -fsanitize=integer
        -fsanitize=float-divide-by-zero
        -fsanitize=unsigned-integer-overflow
        -fno-sanitize-recover=all
    )

    target_link_options(${target} ${scope}
        -fsanitize=undefined
        -fsanitize=integer
        -fsanitize=float-divide-by-zero
    )

    set_target_properties(${target} PROPERTIES
        UBSAN_OPTIONS "halt_on_error=1:print_stacktrace=1:report_error_type=1"
    )
endfunction()

# =============================================================================
# ThreadSanitizer (SEC-08)
# 注意：TSan 不能与 ASan 同时使用，需要独立编译
# =============================================================================
function(airy_enable_tsan target scope)
    if(NOT AIRY_ENABLE_TSAN)
        return()
    endif()

    message(STATUS "Enabling ThreadSanitizer for target: ${target}")

    target_compile_options(${target} ${scope}
        -fsanitize=thread
        -fno-omit-frame-pointer
        -O1  # TSan 需要至少 O1 优化
    )

    target_link_options(${target} ${scope}
        -fsanitize=thread
    )

    set_target_properties(${target} PROPERTIES
        TSAN_OPTIONS "halt_on_error=1:history_size=7:second_deadlock_stack=1:detect_deadlocks=1"
    )
endfunction()

# =============================================================================
# 栈保护 (SEC-12 — INF-05 合规)
# =============================================================================
function(airy_enable_stack_protector target scope)
    if(NOT AIRY_ENABLE_STACK_PROTECTOR OR MSVC)
        return()
    endif()

    target_compile_options(${target} ${scope}
        -fstack-protector-strong
        -fno-omit-frame-pointer
    )

    message(STATUS "Stack protector (-fstack-protector-strong) enabled for: ${target}")
endfunction()

# =============================================================================
# FORTIFY_SOURCE 编译时缓冲区溢出检测
# =============================================================================
function(airy_enable_fortify target scope)
    if(NOT AIRY_ENABLE_FORTIFY OR MSVC)
        return()
    endif()

    # _FORTIFY_SOURCE=2 需要 -O1 或更高优化级别，Debug 构建时跳过以避免
    # "_FORTIFY_SOURCE requires compiling with optimization" 警告
    if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug" OR "${CMAKE_BUILD_TYPE}" STREQUAL "")
        message(STATUS "FORTIFY_SOURCE=2 skipped for ${target} (Debug build, requires -O1+)")
        return()
    endif()

    target_compile_definitions(${target} ${scope}
        _FORTIFY_SOURCE=2
    )

    message(STATUS "FORTIFY_SOURCE=2 enabled for: ${target}")
endfunction()

# =============================================================================
# 一键启用所有安全检测（推荐在顶级 CMakeLists.txt 调用）
# =============================================================================
function(enable_airy_sanitizers target)
    # 可选参数: scope 默认为 PRIVATE
    set(_scope "PRIVATE")
    if(ARGC GREATER 1)
        set(_scope "${ARGV1}")
    endif()

    airy_check_sanitizer_support()

    # 将结果传回父作用域
    set(AIRY_ENABLE_ASAN ${AIRY_ENABLE_ASAN} PARENT_SCOPE)
    set(AIRY_ENABLE_UBSAN ${AIRY_ENABLE_UBSAN} PARENT_SCOPE)
    set(AIRY_ENABLE_TSAN ${AIRY_ENABLE_TSAN} PARENT_SCOPE)

    # 安全编译基础选项（始终启用，无论 sanitizer）
    airy_enable_stack_protector(${target} ${_scope})
    airy_enable_fortify(${target} ${_scope})

    # TSan 与其他 sanitizer 互斥
    if(AIRY_ENABLE_TSAN)
        message(STATUS "ThreadSanitizer mode: ASan + UBSan disabled (mutual exclusion)")
        airy_enable_tsan(${target} ${_scope})
    else()
        if(AIRY_ENABLE_ASAN)
            airy_enable_asan(${target} ${_scope})
        endif()
        if(AIRY_ENABLE_UBSAN)
            airy_enable_ubsan(${target} ${_scope})
        endif()
    endif()

    # 输出检测矩阵
    message(STATUS "Runtime Detection Matrix for ${target}:")
    message(STATUS "  ASan:    ${AIRY_ENABLE_ASAN}")
    message(STATUS "  LSan:    ${AIRY_ENABLE_ASAN}  (bundled with ASan)")
    message(STATUS "  UBSan:   ${AIRY_ENABLE_UBSAN}")
    message(STATUS "  TSan:    ${AIRY_ENABLE_TSAN}")
    message(STATUS "  Stack:   ${AIRY_ENABLE_STACK_PROTECTOR}")
    message(STATUS "  Fortify: ${AIRY_ENABLE_FORTIFY}")
endfunction()

# =============================================================================
# 打印 sanitizer 配置摘要
# =============================================================================
function(airy_print_sanitizer_summary)
    message(STATUS "=========================================")
    message(STATUS "  Airymax Runtime Detection Configuration")
    message(STATUS "=========================================")
    message(STATUS "AddressSanitizer (ASan):       ${AIRY_ENABLE_ASAN}")
    message(STATUS "LeakSanitizer (LSan):          ${AIRY_ENABLE_ASAN}")
    message(STATUS "UndefinedBehaviorSanitizer:    ${AIRY_ENABLE_UBSAN}")
    message(STATUS "ThreadSanitizer (TSan):        ${AIRY_ENABLE_TSAN}")
    message(STATUS "Stack Protector:               ${AIRY_ENABLE_STACK_PROTECTOR}")
    message(STATUS "FORTIFY_SOURCE=2:              ${AIRY_ENABLE_FORTIFY}")
    if(AIRY_ENABLE_ASAN)
        message(STATUS "ASan halt_on_error:            YES (CI will fail on detection)")
        message(STATUS "LSan suppressions:             ecosystem/manager/sanitizer/lsan-suppressions")
    endif()
    if(AIRY_ENABLE_TSAN)
        message(STATUS "TSan history_size:             7")
        message(STATUS "TSan halt_on_error:            YES")
    endif()
    message(STATUS "=========================================")
endfunction()
