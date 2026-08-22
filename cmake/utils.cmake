# =============================================================================
# utils.cmake — agentrt 通用 CMake 工具模块
# 版本：1.0.0
# 创建：2026-07-06
# 归属：agentrt 管理仓直属（v0.1.2 起自伞仓迁入，IRON-9 [IND] 独立层）
#
# 设计目标：
#   提供构建期统一的打印与辅助工具函数，与运行时日志系统（log_write）对齐：
#     [YYYY-MM-DD HH:MM:SS] [LEVEL] message
#   ANSI 彩色编码（与 project_memory 约束一致）：
#     OK    = 绿色  — 成功/确认
#     INFO  = 蓝色  — 信息性输出
#     WARN  = 黄色  — 警告
#     ERROR = 瓦红色 — 错误（不终止）
#     FATAL = 品红  — 致命错误（终止）
#     DEBUG = 灰色  — 调试信息
#     SECTION = 青色加粗 — 章节标题
#
#   自动检测终端环境：管道/文件重定向时禁用彩色，避免 ANSI 码污染日志文件。
#   可通过 AIRY_BUILD_COLOR=1/0 强制启用/禁用。
#
# 使用方式（在任意 CMakeLists.txt 中）：
#   list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
#   include(utils)
#   airy_print_info("Build started")
# =============================================================================

include_guard(GLOBAL)

# ---- 彩色控制 ----
if(DEFINED ENV{AIRY_BUILD_COLOR})
    if("$ENV{AIRY_BUILD_COLOR}" STREQUAL "1")
        set(_AIRY_PRINT_COLOR ON)
    else()
        set(_AIRY_PRINT_COLOR OFF)
    endif()
else()
    # 自动检测：CMAKE_COLOR_MAKEFILE 在终端环境为 ON
    if(CMAKE_COLOR_MAKEFILE OR CMAKE_GENERATOR MATCHES "Ninja|Makefiles")
        set(_AIRY_PRINT_COLOR ON)
    else()
        set(_AIRY_PRINT_COLOR OFF)
    endif()
endif()

if(_AIRY_PRINT_COLOR)
    # CMake 不支持 \033/\x1b 转义，用 string(ASCII 27) 生成 ESC 字符
    string(ASCII 27 _ESC)
    set(_AIRY_C_RESET   "${_ESC}[0m")
    set(_AIRY_C_BOLD    "${_ESC}[1m")
    set(_AIRY_C_GREEN   "${_ESC}[32m")
    set(_AIRY_C_BLUE    "${_ESC}[34m")
    set(_AIRY_C_YELLOW  "${_ESC}[33m")
    set(_AIRY_C_RED     "${_ESC}[31m")
    set(_AIRY_C_MAGENTA "${_ESC}[35m")
    set(_AIRY_C_GRAY    "${_ESC}[90m")
    set(_AIRY_C_CYAN    "${_ESC}[36m")
else()
    set(_AIRY_C_RESET   "")
    set(_AIRY_C_BOLD    "")
    set(_AIRY_C_GREEN   "")
    set(_AIRY_C_BLUE    "")
    set(_AIRY_C_YELLOW  "")
    set(_AIRY_C_RED     "")
    set(_AIRY_C_MAGENTA "")
    set(_AIRY_C_GRAY    "")
    set(_AIRY_C_CYAN    "")
endif()

# ---- 内部：时间戳生成 ----
# 使用本地系统时间（CLOCK_REALTIME 对齐），符合项目约束：
# "Log timestamps must use CLOCK_REALTIME to align with Beijing time"
macro(_AIRY_TIMESTAMP _out_var)
    string(TIMESTAMP ${_out_var} "%Y-%m-%d %H:%M:%S")
endmacro()

# ---- 公开 API ----

# OK — 绿色，成功/确认
function(airy_print_ok msg)
    _AIRY_TIMESTAMP(_ts)
    message("${_AIRY_C_GREEN}[${_ts}] [  OK  ] ${msg}${_AIRY_C_RESET}")
endfunction()

# INFO — 蓝色，信息性输出
function(airy_print_info msg)
    _AIRY_TIMESTAMP(_ts)
    message("${_AIRY_C_BLUE}[${_ts}] [ INFO ] ${msg}${_AIRY_C_RESET}")
endfunction()

# WARN — 黄色，警告
function(airy_print_warn msg)
    _AIRY_TIMESTAMP(_ts)
    message("${_AIRY_C_YELLOW}[${_ts}] [ WARN ] ${msg}${_AIRY_C_RESET}")
endfunction()

# ERROR — 红色，错误（不终止构建）
function(airy_print_error msg)
    _AIRY_TIMESTAMP(_ts)
    message("${_AIRY_C_RED}[${_ts}] [ ERROR] ${msg}${_AIRY_C_RESET}")
endfunction()

# FATAL — 品红，致命错误（终止构建）
function(airy_print_fatal msg)
    _AIRY_TIMESTAMP(_ts)
    message(FATAL_ERROR "${_AIRY_C_MAGENTA}[${_ts}] [ FATAL] ${msg}${_AIRY_C_RESET}")
endfunction()

# DEBUG — 灰色，调试信息
function(airy_print_debug msg)
    _AIRY_TIMESTAMP(_ts)
    message("${_AIRY_C_GRAY}[${_ts}] [DEBUG ] ${msg}${_AIRY_C_RESET}")
endfunction()

# SECTION — 青色加粗，章节标题
function(airy_print_section msg)
    _AIRY_TIMESTAMP(_ts)
    message("${_AIRY_C_CYAN}${_AIRY_C_BOLD}[${_ts}] [======] ${msg}${_AIRY_C_RESET}")
endfunction()

# STATUS — 兼容旧 message(STATUS)，蓝色
function(airy_print_status msg)
    airy_print_info("${msg}")
endfunction()

# ---- 辅助：条件打印 ----
# 当 AIRY_VERBOSE=1 时输出 DEBUG 级别信息
function(airy_print_verbose msg)
    if(DEFINED ENV{AIRY_VERBOSE} AND "$ENV{AIRY_VERBOSE}" STREQUAL "1")
        airy_print_debug("${msg}")
    endif()
endfunction()

# ---- 辅助：构建环境摘要 ----
function(airy_print_build_summary)
    airy_print_section("Airymax Build Environment Summary")
    airy_print_info("Project:      ${CMAKE_PROJECT_NAME}")
    airy_print_info("Version:      ${CMAKE_PROJECT_VERSION}")
    airy_print_info("Build type:   ${CMAKE_BUILD_TYPE}")
    airy_print_info("Platform:     ${CMAKE_SYSTEM_NAME} (${CMAKE_SYSTEM_PROCESSOR})")
    airy_print_info("Compiler:     ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
    airy_print_info("Source dir:   ${CMAKE_SOURCE_DIR}")
    airy_print_info("Binary dir:   ${CMAKE_BINARY_DIR}")
    airy_print_info("Install prefix: ${CMAKE_INSTALL_PREFIX}")
endfunction()
