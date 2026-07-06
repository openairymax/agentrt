#!/usr/bin/env bash
# shellcheck shell=bash
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS 平台检测和环境工具模块
# 遵循 AgentOS 架构设计原则：跨平台一致性原则 (E-4)

###############################################################################
# 来源此脚本
###############################################################################
set -e

###############################################################################
# 平台定义
###############################################################################
declare -r PLATFORM_LINUX="linux"
declare -r PLATFORM_MACOS="macos"
declare -r PLATFORM_WINDOWS="windows"
declare -r PLATFORM_WSL="wsl"
declare -r PLATFORM_UNKNOWN="unknown"

###############################################################################
# 架构定义
###############################################################################
declare -r ARCH_X86_64="x86_64"
declare -r ARCH_ARM64="arm64"
declare -r ARCH_AARCH64="aarch64"
declare -r ARCH_UNKNOWN="unknown"

###############################################################################
# 全局变量（缓存）
###############################################################################
_AGENTRT_PLATFORM_DETECTED=0
_AGENTRT_PLATFORM=""
_AGENTRT_ARCH=""
_AGENTRT_DISTRO=""
_AGENTRT_DISTRO_VERSION=""

###############################################################################
# 内部函数：检测WSL
###############################################################################
_is_wsl() {
    if [[ -f /proc/version ]]; then
        if grep -qi "microsoft\|wsl" /proc/version 2>/dev/null; then
            return 0
        fi
    fi
    return 1
}

###############################################################################
# 内部函数：检测macOS
###############################################################################
_is_macos() {
    if [[ "$(uname)" == "Darwin" ]]; then
        return 0
    fi
    return 1
}

###############################################################################
# 内部函数：检测Linux
###############################################################################
_is_linux() {
    if [[ "$(uname)" == "Linux" ]]; then
        return 0
    fi
    return 1
}

###############################################################################
# 内部函数：检测Windows
###############################################################################
_is_windows() {
    if _is_wsl; then
        return 0
    fi
    if [[ "$(uname)" == *"MINGW"* ]] || [[ "$(uname)" == *"CYGWIN"* ]]; then
        return 0
    fi
    return 1
}

###############################################################################
# 公共API：获取平台
###############################################################################
agentrt_platform_detect() {
    if [[ $_AGENTRT_PLATFORM_DETECTED -eq 1 ]]; then
        echo "$_AGENTRT_PLATFORM"
        return
    fi

    if _is_wsl; then
        _AGENTRT_PLATFORM="$PLATFORM_WSL"
    elif _is_macos; then
        _AGENTRT_PLATFORM="$PLATFORM_MACOS"
    elif _is_linux; then
        _AGENTRT_PLATFORM="$PLATFORM_LINUX"
    elif _is_windows; then
        _AGENTRT_PLATFORM="$PLATFORM_WINDOWS"
    else
        _AGENTRT_PLATFORM="$PLATFORM_UNKNOWN"
    fi

    _AGENTRT_PLATFORM_DETECTED=1
    echo "$_AGENTRT_PLATFORM"
}

agentrt_platform_is_linux() {
    local platform
    platform=$(agentrt_platform_detect)
    [[ "$platform" == "$PLATFORM_LINUX" ]] || [[ "$platform" == "$PLATFORM_WSL" ]]
}

agentrt_platform_is_macos() {
    local platform
    platform=$(agentrt_platform_detect)
    [[ "$platform" == "$PLATFORM_MACOS" ]]
}

agentrt_platform_is_windows() {
    local platform
    platform=$(agentrt_platform_detect)
    [[ "$platform" == "$PLATFORM_WINDOWS" ]] || [[ "$platform" == "$PLATFORM_WSL" ]]
}

agentrt_platform_is_wsl() {
    local platform
    platform=$(agentrt_platform_detect)
    [[ "$platform" == "$PLATFORM_WSL" ]]
}

agentrt_platform_is_unix() {
    agentrt_platform_is_linux || agentrt_platform_is_macos
}

###############################################################################
# 公共API：获取架构
###############################################################################
agentrt_arch_detect() {
    if [[ -n "$_AGENTRT_ARCH" ]]; then
        echo "$_AGENTRT_ARCH"
        return
    fi

    local arch
    arch=$(uname -m)

    case "$arch" in
        x86_64)
            _AGENTRT_ARCH="$ARCH_X86_64"
            ;;
        amd64)
            _AGENTRT_ARCH="$ARCH_X86_64"
            ;;
        arm64)
            _AGENTRT_ARCH="$ARCH_ARM64"
            ;;
        aarch64)
            _AGENTRT_ARCH="$ARCH_AARCH64"
            ;;
        *)
            _AGENTRT_ARCH="$ARCH_UNKNOWN"
            ;;
    esac

    echo "$_AGENTRT_ARCH"
}

agentrt_arch_is_x86_64() {
    [[ "$(agentrt_arch_detect)" == "$ARCH_X86_64" ]]
}

agentrt_arch_is_arm64() {
    local arch
    arch=$(agentrt_arch_detect)
    [[ "$arch" == "$ARCH_ARM64" ]] || [[ "$arch" == "$ARCH_AARCH64" ]]
}

###############################################################################
# 公共API：获取Linux发行版信息
###############################################################################
agentrt_linux_distro_detect() {
    if [[ -n "$_AGENTRT_DISTRO" ]]; then
        echo "$_AGENTRT_DISTRO"
        return
    fi

    if [[ -f /etc/os-release ]]; then
        _AGENTRT_DISTRO=$(grep "^ID=" /etc/os-release | cut -d= -f2 | tr -d '"')
        _AGENTRT_DISTRO_VERSION=$(grep "^VERSION_ID=" /etc/os-release | cut -d= -f2 | tr -d '"')
    elif [[ -f /etc/lsb-release ]]; then
        _AGENTRT_DISTRO=$(grep "^DISTRIB_ID=" /etc/lsb-release | cut -d= -f2)
        _AGENTRT_DISTRO_VERSION=$(grep "^DISTRIB_RELEASE=" /etc/lsb-release | cut -d= -f2)
    else
        _AGENTRT_DISTRO="unknown"
        _AGENTRT_DISTRO_VERSION=""
    fi

    echo "$_AGENTRT_DISTRO"
}

agentrt_linux_distro_version() {
    if [[ -z "$_AGENTRT_DISTRO_VERSION" ]]; then
        agentrt_linux_distro_detect > /dev/null
    fi
    echo "$_AGENTRT_DISTRO_VERSION"
}

###############################################################################
# 公共API：检测包管理器
###############################################################################
agentrt_package_manager_detect() {
    if command -v apt-get &> /dev/null; then
        echo "apt"
    elif command -v yum &> /dev/null; then
        echo "yum"
    elif command -v dnf &> /dev/null; then
        echo "dnf"
    elif command -v apk &> /dev/null; then
        echo "apk"
    elif command -v brew &> /dev/null; then
        echo "brew"
    elif command -v pacman &> /dev/null; then
        echo "pacman"
    else
        echo "unknown"
    fi
}

###############################################################################
# 公共API：检测必需命令
###############################################################################
agentrt_check_command() {
    local cmd="$1"
    if ! command -v "$cmd" &> /dev/null; then
        agentrt_log_error "Required command not found: $cmd"
        return 1
    fi
    return 0
}

agentrt_check_commands() {
    local missing=()
    local cmd
    for cmd in "$@"; do
        if ! command -v "$cmd" &> /dev/null; then
            missing+=("$cmd")
        fi
    done

    if [[ ${#missing[@]} -gt 0 ]]; then
        agentrt_log_error "Missing required commands: ${missing[*]}"
        return 1
    fi
    return 0
}

###############################################################################
# 公共API：获取系统信息
###############################################################################
agentrt_system_info() {
    local info=""
    info+="Platform: $(agentrt_platform_detect)\n"
    info+="Architecture: $(agentrt_arch_detect)\n"

    if agentrt_platform_is_linux; then
        info+="Distribution: $(agentrt_linux_distro_detect) $(agentrt_linux_distro_version)\n"
    fi

    if command -v cmake &> /dev/null; then
        info+="CMake: $(cmake --version | head -n1)\n"
    fi

    if command -v gcc &> /dev/null; then
        info+="GCC: $(gcc --version | head -n1)\n"
    fi

    if command -v docker &> /dev/null; then
        info+="Docker: $(docker --version 2>/dev/null || echo 'not running')\n"
    fi

    echo -e "$info"
}

###############################################################################
# 公共API：CPU核心数
###############################################################################
agentrt_cpu_count() {
    if [[ "$(uname)" == "Darwin" ]]; then
        sysctl -n hw.ncpu 2>/dev/null || echo "1"
    else
        nproc 2>/dev/null || grep -c ^processor /proc/cpuinfo 2>/dev/null || echo "1"
    fi
}

###############################################################################
# 公共API：内存信息
###############################################################################
agentrt_total_memory() {
    if [[ "$(uname)" == "Darwin" ]]; then
        sysctl -n hw.memsize 2>/dev/null | awk '{printf "%.0f GB", $1/1024/1024/1024}'
    else
        awk '/MemTotal/ {printf "%.0f GB", $2/1024/1024}' /proc/meminfo 2>/dev/null || echo "unknown"
    fi
}

###############################################################################
# 导出公共API
###############################################################################
export -f agentrt_platform_detect agentrt_platform_is_linux agentrt_platform_is_macos
export -f agentrt_platform_is_windows agentrt_platform_is_wsl agentrt_platform_is_unix
export -f agentrt_arch_detect agentrt_arch_is_x86_64 agentrt_arch_is_arm64
export -f agentrt_linux_distro_detect agentrt_linux_distro_version
export -f agentrt_package_manager_detect agentrt_check_command agentrt_check_commands
export -f agentrt_system_info agentrt_cpu_count agentrt_total_memory