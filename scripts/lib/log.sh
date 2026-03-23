#!/usr/bin/env bash
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS 统一日志和错误处理模块
# 遵循 AgentOS 架构设计原则：反馈闭环、工程美学

###############################################################################
# 颜色定义
###############################################################################
declare -r COLOR_RED='\033[0;31m'
declare -r COLOR_GREEN='\033[0;32m'
declare -r COLOR_YELLOW='\033[1;33m'
declare -r COLOR_BLUE='\033[0;34m'
declare -r COLOR_CYAN='\033[0;36m'
declare -r COLOR_MAGENTA='\033[0;35m'
declare -r COLOR_NC='\033[0m'

###############################################################################
# 日志级别定义
###############################################################################
declare -r LOG_LEVEL_DEBUG=0
declare -r LOG_LEVEL_INFO=1
declare -r LOG_LEVEL_WARN=2
declare -r LOG_LEVEL_ERROR=3
declare -r LOG_LEVEL_FATAL=4

declare -r LOG_LEVEL_NAMES=("DEBUG" "INFO" "WARN" "ERROR" "FATAL")
declare -r LOG_LEVEL_COLORS=("$COLOR_CYAN" "$COLOR_BLUE" "$COLOR_YELLOW" "$COLOR_RED" "$COLOR_MAGENTA")

###############################################################################
# 全局变量
###############################################################################
_AGENTOS_LOG_LEVEL="${AGENTOS_LOG_LEVEL:-$LOG_LEVEL_INFO}"
_AGENTOS_LOG_PREFIX="${AGENTOS_LOG_PREFIX:-[AgentOS]}"
_AGENTOS_LOG_TIMESTAMP="${AGENTOS_LOG_TIMESTAMP:-1}"
_AGENTOS_LOG_FILE="${AGENTOS_LOG_FILE:-}"
_AGENTOS_SCRIPT_ERRORS=0
_AGENTOS_SCRIPT_WARNINGS=0
_AGENTOS_SCRIPT_NAME="${0##*/}"
_AGENTOS_SCRIPT_PID=$$
_AGENTOS_TRACE_ID="${AGENTOS_TRACE_ID:-$(date +%s)-$$}"

###############################################################################
# 内部函数：获取时间戳
###############################################################################
_agentos_timestamp() {
    if [[ "$_AGENTOS_LOG_TIMESTAMP" == "1" ]]; then
        date '+%Y-%m-%d %H:%M:%S'
    fi
}

###############################################################################
# 内部函数：写入日志
###############################################################################
_agentos_log_write() {
    local level=$1
    local message="$2"
    local timestamp=$(_agentos_timestamp)
    local level_name="${LOG_LEVEL_NAMES[$level]}"
    local level_color="${LOG_LEVEL_COLORS[$level]}"
    local formatted_msg

    if [[ -n "$timestamp" ]]; then
        formatted_msg="${timestamp} ${_AGENTOS_LOG_PREFIX} ${level_name} ${message}"
    else
        formatted_msg="${_AGENTOS_LOG_PREFIX} ${level_name} ${message}"
    fi

    echo -e "${level_color}${formatted_msg}${COLOR_NC}"

    if [[ -n "$_AGENTOS_LOG_FILE" ]]; then
        echo "$formatted_msg" >> "$_AGENTOS_LOG_FILE"
    fi
}

###############################################################################
# 公共API：日志函数
###############################################################################
agentos_log_debug() {
    if [[ $_AGENTOS_LOG_LEVEL -le $LOG_LEVEL_DEBUG ]]; then
        _agentos_log_write $LOG_LEVEL_DEBUG "$1"
    fi
}

agentos_log_info() {
    if [[ $_AGENTOS_LOG_LEVEL -le $LOG_LEVEL_INFO ]]; then
        _agentos_log_write $LOG_LEVEL_INFO "$1"
    fi
}

agentos_log_warn() {
    ((_AGENTOS_SCRIPT_WARNINGS++))
    if [[ $_AGENTOS_LOG_LEVEL -le $LOG_LEVEL_WARN ]]; then
        _agentos_log_write $LOG_LEVEL_WARN "$1"
    fi
}

agentos_log_error() {
    ((_AGENTOS_SCRIPT_ERRORS++))
    if [[ $_AGENTOS_LOG_LEVEL -le $LOG_LEVEL_ERROR ]]; then
        _agentos_log_write $LOG_LEVEL_ERROR "$1"
    fi
}

agentos_log_fatal() {
    ((_AGENTOS_SCRIPT_ERRORS++))
    _agentos_log_write $LOG_LEVEL_FATAL "$1"
    agentos_exit 1
}

###############################################################################
# 公共API：设置日志级别
###############################################################################
agentos_log_set_level() {
    case "$1" in
        debug|DEBUG) _AGENTOS_LOG_LEVEL=$LOG_LEVEL_DEBUG ;;
        info|INFO)   _AGENTOS_LOG_LEVEL=$LOG_LEVEL_INFO ;;
        warn|WARN)  _AGENTOS_LOG_LEVEL=$LOG_LEVEL_WARN ;;
        error|ERROR)_AGENTOS_LOG_LEVEL=$LOG_LEVEL_ERROR ;;
        *)          agentos_log_warn "Unknown log level: $1, using INFO"; _AGENTOS_LOG_LEVEL=$LOG_LEVEL_INFO ;;
    esac
}

###############################################################################
# 公共API：设置日志文件
###############################################################################
agentos_log_set_file() {
    _AGENTOS_LOG_FILE="$1"
}

###############################################################################
# 公共API：打印消息（不带日志级别前缀）
###############################################################################
agentos_echo() {
    echo -e "$1"
}

agentos_echo_info() {
    echo -e "${COLOR_BLUE}[INFO]${COLOR_NC} $1"
}

agentos_echo_success() {
    echo -e "${COLOR_GREEN}[SUCCESS]${COLOR_NC} $1"
}

agentos_echo_warning() {
    echo -e "${COLOR_YELLOW}[WARNING]${COLOR_NC} $1"
}

agentos_echo_error() {
    echo -e "${COLOR_RED}[ERROR]${COLOR_NC} $1"
}

###############################################################################
# 公共API：错误处理
###############################################################################
agentos_die() {
    agentos_log_fatal "$1"
    exit "${2:-1}"
}

agentos_exit() {
    local exit_code=$1
    if [[ $exit_code -eq 0 ]]; then
        agentos_log_info "Script $_AGENTOS_SCRIPT_NAME completed successfully"
    else
        agentos_log_error "Script $_AGENTOS_SCRIPT_NAME failed with exit code $exit_code"
    fi
    exit $exit_code
}

###############################################################################
# 公共API：错误统计获取
###############################################################################
agentos_get_error_count() {
    echo $_AGENTOS_SCRIPT_ERRORS
}

agentos_get_warning_count() {
    echo $_AGENTOS_SCRIPT_WARNINGS
}

###############################################################################
# 公共API：断言函数
###############################################################################
agentos_assert() {
    local condition="$1"
    local message="${2:-Assertion failed}"
    if ! eval "$condition"; then
        agentos_log_fatal "$message (condition: $condition)"
    fi
}

agentos_assert_not_empty() {
    local value="$1"
    local name="${2:-value}"
    if [[ -z "$value" ]]; then
        agentos_log_fatal "Assert failed: $name must not be empty"
    fi
}

agentos_assert_file_exists() {
    local file="$1"
    local name="${2:-file}"
    if [[ ! -f "$file" ]]; then
        agentos_log_fatal "Assert failed: $name does not exist: $file"
    fi
}

agentos_assert_dir_exists() {
    local dir="$1"
    local name="${2:-directory}"
    if [[ ! -d "$dir" ]]; then
        agentos_log_fatal "Assert failed: $name does not exist: $dir"
    fi
}

agentos_assert_command_exists() {
    local cmd="$1"
    if ! command -v "$cmd" &> /dev/null; then
        agentos_log_fatal "Assert failed: required command not found: $cmd"
    fi
}

###############################################################################
# 公共API：追踪ID
###############################################################################
agentos_get_trace_id() {
    echo "$_AGENTOS_TRACE_ID"
}

agentos_set_trace_id() {
    _AGENTOS_TRACE_ID="$1"
}

###############################################################################
# 公共API：进度显示
###############################################################################
agentos_progress_start() {
    local message="$1"
    echo -ne "${COLOR_BLUE}[......]${COLOR_NC} $message"
}

agentos_progress_update() {
    local step="$1"
    echo -ne "\b\b\b\b\b\b"
    case "$step" in
        1) echo -ne "\b\b\b\b\b\b" ;;
        2) echo -ne "\b/     ]" ;;
        3) echo -ne "\b-     ]" ;;
        4) echo -ne "\b\\     ]" ;;
        5) echo -ne "\b|     ]" ;;
        *) echo -ne "\b*     ]" ;;
    esac
}

agentos_progress_done() {
    local message="$1"
    local status="${2:-SUCCESS}"
    echo -ne "\b\b\b\b\b\b"
    case "$status" in
        SUCCESS) echo -e "\b\b\b\b\b\b${COLOR_GREEN}[DONE]${COLOR_NC} $message" ;;
        FAIL)    echo -e "\b\b\b\b\b\b${COLOR_RED}[FAIL]${COLOR_NC} $message" ;;
        SKIP)    echo -e "\b\b\b\b\b\b${COLOR_YELLOW}[SKIP]${COLOR_NC} $message" ;;
        *)       echo -e "\b\b\b\b\b\b${COLOR_BLUE}[$status]${COLOR_NC} $message" ;;
    esac
}

###############################################################################
# 导出公共API
###############################################################################
export -f agentos_log_debug agentos_log_info agentos_log_warn agentos_log_error agentos_log_fatal
export -f agentos_log_set_level agentos_log_set_file
export -f agentos_echo agentos_echo_info agentos_echo_success agentos_echo_warning agentos_echo_error
export -f agentos_die agentos_exit
export -f agentos_get_error_count agentos_get_warning_count
export -f agentos_assert agentos_assert_not_empty agentos_assert_file_exists agentos_assert_dir_exists agentos_assert_command_exists
export -f agentos_get_trace_id agentos_set_trace_id
export -f agentos_progress_start agentos_progress_update agentos_progress_done