# =============================================================================
# agentos_print.cmake — AgentRT 统一构建打印系统
# 版本：1.0.0
# 创建：2026-07-01
#
# 设计目标：
#   与运行时日志系统（log_write）对齐，构建期所有打印输出统一格式：
#     [YYYY-MM-DD HH:MM:SS] [LEVEL] message
#   并使用 ANSI 彩色编码，使开发者能快速识别状态/问题：
#     OK    = 绿色  — 成功/确认
#     INFO  = 蓝色  — 信息性输出
#     WARN  = 黄色  — 警告
#     ERROR = 红色  — 错误（不终止）
#     FATAL = 品红  — 致命错误（终止）
#     DEBUG = 灰色  — 调试信息
#     SECTION = 青色加粗 — 章节标题
#
#   自动检测终端环境：管道/文件重定向时禁用彩色，避免 ANSI 码污染日志文件。
#   可通过 AGENTRT_BUILD_COLOR=1/0 强制启用/禁用。
# =============================================================================

# ---- 彩色控制 ----
if(DEFINED ENV{AGENTRT_BUILD_COLOR})
    if("$ENV{AGENTRT_BUILD_COLOR}" STREQUAL "1")
        set(_AGENTOS_PRINT_COLOR ON)
    else()
        set(_AGENTOS_PRINT_COLOR OFF)
    endif()
else()
    # 自动检测：CMAKE_COLOR_MAKEFILE 在终端环境为 ON
    # CMAKE_CURRENT_LIST_DIR 仅在 include() 后可用，此处用 ISATTY 检测
    # CMake 没有直接 ISATTY，用 CMAKE_COLOR_MAKEFILE 作为代理
    if(CMAKE_COLOR_MAKEFILE OR CMAKE_GENERATOR MATCHES "Ninja|Makefiles")
        set(_AGENTOS_PRINT_COLOR ON)
    else()
        set(_AGENTOS_PRINT_COLOR OFF)
    endif()
endif()

if(_AGENTOS_PRINT_COLOR)
    # CMake 不支持 \033/\x1b 转义，用 string(ASCII 27) 生成 ESC 字符
    string(ASCII 27 _ESC)
    set(_AGENTOS_C_RESET   "${_ESC}[0m")
    set(_AGENTOS_C_BOLD    "${_ESC}[1m")
    set(_AGENTOS_C_GREEN   "${_ESC}[32m")
    set(_AGENTOS_C_BLUE    "${_ESC}[34m")
    set(_AGENTOS_C_YELLOW  "${_ESC}[33m")
    set(_AGENTOS_C_RED     "${_ESC}[31m")
    set(_AGENTOS_C_MAGENTA "${_ESC}[35m")
    set(_AGENTOS_C_GRAY    "${_ESC}[90m")
    set(_AGENTOS_C_CYAN    "${_ESC}[36m")
else()
    set(_AGENTOS_C_RESET   "")
    set(_AGENTOS_C_BOLD    "")
    set(_AGENTOS_C_GREEN   "")
    set(_AGENTOS_C_BLUE    "")
    set(_AGENTOS_C_YELLOW  "")
    set(_AGENTOS_C_RED     "")
    set(_AGENTOS_C_MAGENTA "")
    set(_AGENTOS_C_GRAY    "")
    set(_AGENTOS_C_CYAN    "")
endif()

# ---- 内部：时间戳生成 ----
# 使用本地系统时间（CLOCK_REALTIME 对齐），符合项目约束：
# "Log timestamps must use CLOCK_REALTIME to align with network Beijing time
#  or host system time"。CMake string(TIMESTAMP) 默认即本地时间，此处显式
# 不传 UTC 参数，确保与运行时日志系统时区一致。
macro(_AGENTOS_TIMESTAMP _out_var)
    string(TIMESTAMP ${_out_var} "%Y-%m-%d %H:%M:%S")
endmacro()

# ---- 公开 API ----

# OK — 绿色，成功/确认
function(agentos_print_ok msg)
    _AGENTOS_TIMESTAMP(_ts)
    message("${_AGENTOS_C_GREEN}[${_ts}] [  OK  ] ${msg}${_AGENTOS_C_RESET}")
endfunction()

# INFO — 蓝色，信息性输出
function(agentos_print_info msg)
    _AGENTOS_TIMESTAMP(_ts)
    message("${_AGENTOS_C_BLUE}[${_ts}] [ INFO ] ${msg}${_AGENTOS_C_RESET}")
endfunction()

# WARN — 黄色，警告
function(agentos_print_warn msg)
    _AGENTOS_TIMESTAMP(_ts)
    message("${_AGENTOS_C_YELLOW}[${_ts}] [ WARN ] ${msg}${_AGENTOS_C_RESET}")
endfunction()

# ERROR — 红色，错误（不终止构建）
function(agentos_print_error msg)
    _AGENTOS_TIMESTAMP(_ts)
    message("${_AGENTOS_C_RED}[${_ts}] [ ERROR] ${msg}${_AGENTOS_C_RESET}")
endfunction()

# FATAL — 品红，致命错误（终止构建）
function(agentos_print_fatal msg)
    _AGENTOS_TIMESTAMP(_ts)
    message(FATAL_ERROR "${_AGENTOS_C_MAGENTA}[${_ts}] [ FATAL] ${msg}${_AGENTOS_C_RESET}")
endfunction()

# DEBUG — 灰色，调试信息
function(agentos_print_debug msg)
    _AGENTOS_TIMESTAMP(_ts)
    message("${_AGENTOS_C_GRAY}[${_ts}] [DEBUG ] ${msg}${_AGENTOS_C_RESET}")
endfunction()

# SECTION — 青色加粗，章节标题
function(agentos_print_section msg)
    _AGENTOS_TIMESTAMP(_ts)
    message("${_AGENTOS_C_CYAN}${_AGENTOS_C_BOLD}[${_ts}] [======] ${msg}${_AGENTOS_C_RESET}")
endfunction()

# STATUS — 兼容旧 message(STATUS)，蓝色
function(agentos_print_status msg)
    agentos_print_info("${msg}")
endfunction()

# ---- 兼容宏：将 message(STATUS ...) 自动映射（可选 include 后使用） ----
# 不覆盖 message()，避免破坏第三方 CMake。各模块主动调用 agentos_print_* 函数。
