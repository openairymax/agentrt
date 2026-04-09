#!/usr/bin/env bash
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS 通用工具函数模块
# 遵循 AgentOS 架构设计原则：接口最小化原则 (E-5)

###############################################################################
# 严格模式
###############################################################################
set -euo pipefail

###############################################################################
# 路径常量
###############################################################################
AGENTOS_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
AGENTOS_SCRIPTS_DIR="$(dirname "$AGENTOS_SCRIPT_DIR")"
AGENTOS_PROJECT_ROOT="$(dirname "$AGENTOS_SCRIPTS_DIR")"
AGENTOS_LIB_DIR="$AGENTOS_SCRIPTS_DIR/lib"
AGENTOS_CONFIG_DIR="$AGENTOS_PROJECT_ROOT/manager"
AGENTOS_heapstore_DIR="$AGENTOS_PROJECT_ROOT/heapstore"

###############################################################################
# 加载依赖模块
###############################################################################
agentos_load_libs() {
    local libs=("log.sh" "error.sh" "platform.sh")
    local lib

    for lib in "${libs[@]}"; do
        local lib_path="$AGENTOS_LIB_DIR/$lib"
        if [[ -f "$lib_path" ]]; then
            # 使用 shellcheck 忽略 SC1090
            # shellcheck source=/dev/null
            source "$lib_path"
        else
            echo -e "\033[0;31m[ERROR]\033[0m Missing required library: $lib_path"
            return 1
        fi
    done
}

agentos_load_libs

###############################################################################
# 字符串工具
###############################################################################

agentos_to_lower() {
    echo "$1" | tr '[:upper:]' '[:lower:]'
}

agentos_to_upper() {
    echo "$1" | tr '[:lower:]' '[:upper:]'
}

agentos_trim() {
    local var="$1"
    var="${var#"${var%%[![:space:]]*}"}"
    var="${var%"${var##*[![:space:]]}"}"
    echo -n "$var"
}

agentos_contains() {
    local haystack="$1"
    local needle="$2"
    [[ "$haystack" == *"$needle"* ]]
}

agentos_random_string() {
    local length="${1:-16}"
    LC_ALL=C tr -dc 'a-zA-Z0-9' </dev/urandom | head -c "$length"
}

###############################################################################
# 文件工具
###############################################################################

agentos_mkdir() {
    local dir="$1"
    local mode="${2:-0755}"

    if [[ -d "$dir" ]]; then
        return 0
    fi

    if ! mkdir -p "$dir" 2>/dev/null; then
        agentos_log_error "Failed to create directory: $dir"
        return 1
    fi

    if ! chmod "$mode" "$dir" 2>/dev/null; then
        agentos_log_warn "Failed to set permissions on: $dir"
    fi

    return 0
}

agentos_safe_rm() {
    local file="$1"

    if [[ -f "$file" ]]; then
        rm -f "$file"
    fi
}

agentos_backup_file() {
    local file="$1"
    local backup=""

    if [[ ! -f "$file" ]]; then
        return 1
    fi

    backup="${file}.backup.$(date +%Y%m%d_%H%M%S)"

    if ! cp "$file" "$backup"; then
        agentos_log_error "Failed to backup file: $file"
        return 1
    fi

    echo "$backup"
    return 0
}

agentos_file_size() {
    local file="$1"

    if [[ ! -f "$file" ]]; then
        echo "0 B"
        return
    fi

    local size
    size=$(stat -c%s "$file" 2>/dev/null || stat -f%z "$file" 2>/dev/null)

    if [[ $size -lt 1024 ]]; then
        echo "$size B"
    elif [[ $size -lt 1048576 ]]; then
        echo "$(( size / 1024 )) KB"
    elif [[ $size -lt 1073741824 ]]; then
        echo "$(( size / 1048576 )) MB"
    else
        echo "$(( size / 1073741824 )) GB"
    fi
}

agentos_is_executable() {
    local file="$1"
    [[ -x "$file" ]] || [[ -f "$file" && "${file: -3}" == ".sh" ]]
}

###############################################################################
# 进程工具
###############################################################################

agentos_is_process_running() {
    local pid="$1"
    kill -0 "$pid" 2>/dev/null
}

agentos_wait_for_process() {
    local pid="$1"
    local timeout="${2:-60}"
    local elapsed=0

    while agentos_is_process_running "$pid"; do
        if [[ $elapsed -ge $timeout ]]; then
            return 124
        fi
        sleep 1
        ((elapsed++))
    done

    return 0
}

agentos_kill_process() {
    local pid="$1"
    local sig="${2:-TERM}"

    if ! agentos_is_process_running "$pid"; then
        return 0
    fi

    kill -$sig "$pid" 2>/dev/null || true
    agentos_wait_for_process "$pid" 5
    if agentos_is_process_running "$pid"; then
        kill -9 "$pid" 2>/dev/null || true
    fi
}

###############################################################################
# 网络工具
###############################################################################

agentos_is_port_available() {
    local port="$1"

    if command -v lsof &> /dev/null; then
        ! lsof -i :$port &> /dev/null
    elif command -v netstat &> /dev/null; then
        ! netstat -tuln 2>/dev/null | grep -q ":$port "
    else
        return 0
    fi
}

agentos_wait_for_url() {
    local url="$1"
    local timeout="${2:-60}"
    local elapsed=0

    while [[ $elapsed -lt $timeout ]]; do
        if curl -sf --max-time 2 "$url" &> /dev/null; then
            return 0
        fi
        sleep 2
        ((elapsed+=2))
    done

    return 1
}

###############################################################################
# 数组工具
###############################################################################

agentos_in_array() {
    local element="$1"
    shift
    local array=("$@")

    for item in "${array[@]}"; do
        if [[ "$item" == "$element" ]]; then
            return 0
        fi
    done
    return 1
}

agentos_array_length() {
    local array=("$@")
    echo "${#array[@]}"
}

###############################################################################
# 版本比较
###############################################################################

agentos_version_compare() {
    local v1="$1"
    local v2="$2"

    if [[ "$v1" == "$v2" ]]; then
        return 0
    fi

    local IFS='.'
    local i ver1=($v1) ver2=($v2)

    for ((i=0; i<${#ver1[@]} || i<${#ver2[@]}; i++)); do
        local num1=${ver1[i]:-0}
        local num2=${ver2[i]:-0}

        if ((10#$num1 > 10#$num2)); then
            return 1
        elif ((10#$num1 < 10#$num2)); then
            return 2
        fi
    done

    return 0
}

agentos_version_check() {
    local required="$1"
    local actual="$2"

    agentos_version_compare "$actual" "$required"
    local result=$?

    [[ $result -ne 2 ]]
}

###############################################################################
# 配置文件工具
###############################################################################

agentos_config_get() {
    local file="$1"
    local key="$2"
    local default="${3:-}"

    if [[ ! -f "$file" ]]; then
        echo "$default"
        return
    fi

    local value
    value=$(grep "^${key}=" "$file" 2>/dev/null | head -1 | cut -d'=' -f2- | tr -d '"' | tr -d "'")

    if [[ -z "$value" ]]; then
        echo "$default"
    else
        echo "$value"
    fi
}

agentos_config_set() {
    local file="$1"
    local key="$2"
    local value="$3"

    if [[ ! -f "$file" ]]; then
        touch "$file"
    fi

    if grep -q "^${key}=" "$file" 2>/dev/null; then
        sed -i "s|^${key}=.*|${key}=${value}|" "$file"
    else
        echo "${key}=${value}" >> "$file"
    fi
}

###############################################################################
# 用户交互工具
###############################################################################

agentos_confirm() {
    local prompt="${1:-Are you sure?}"
    local default="${2:-N}"

    local yn
    if [[ "$default" == "Y" ]]; then
        read -p "$prompt [Y/n]: " yn
        [[ -z "$yn" ]] && yn="Y"
    else
        read -p "$prompt [y/N]: " yn
        [[ -z "$yn" ]] && yn="N"
    fi

    [[ "$yn" =~ ^[Yy]$ ]]
}

agentos_select() {
    local prompt="$1"
    shift
    local options=("$@")
    local n=${#options[@]}

    echo "$prompt"

    for i in "${!options[@]}"; do
        echo "$((i+1))) ${options[$i]}"
    done

    local choice
    read -p "Select [1-$n]: " choice

    if [[ "$choice" =~ ^[0-9]+$ ]] && [[ $choice -ge 1 ]] && [[ $choice -le $n ]]; then
        echo "${options[$((choice-1))]}"
        return 0
    fi

    return 1
}

###############################################################################
# 下载工具
###############################################################################

agentos_download() {
    local url="$1"
    local dest="$2"
    local timeout="${3:-60}"

    local curl_opts=("-fsSL" "--max-time" "$timeout" "-o" "$dest")

    if [[ -n "${AGENTOS_HTTP_PROXY:-}" ]]; then
        curl_opts+=("--proxy" "$AGENTOS_HTTP_PROXY")
    fi

    if [[ -n "${AGENTOS_HTTPS_PROXY:-}" ]]; then
        curl_opts+=("--proxy" "$AGENTOS_HTTPS_PROXY")
    fi

    if ! curl "${curl_opts[@]}" "$url"; then
        agentos_log_error "Failed to download: $url"
        return 1
    fi

    return 0
}

###############################################################################
# 导出公共API
###############################################################################
export -f agentos_load_libs
export -f agentos_to_lower agentos_to_upper agentos_trim agentos_contains agentos_random_string
export -f agentos_mkdir agentos_safe_rm agentos_backup_file agentos_file_size agentos_is_executable
export -f agentos_is_process_running agentos_wait_for_process agentos_kill_process
export -f agentos_is_port_available agentos_wait_for_url
export -f agentos_in_array agentos_array_length
export -f agentos_version_compare agentos_version_check
export -f agentos_config_get agentos_config_set
export -f agentos_confirm agentos_select
export -f agentos_download
