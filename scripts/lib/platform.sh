#!/usr/bin/env bash
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
_AGENTOS_PLATFORM_DETECTED=0
_AGENTOS_PLATFORM=""
_AGENTOS_ARCH=""
_AGENTOS_DISTRO=""
_AGENTOS_DISTRO_VERSION=""

###############################################################################
# 内部函数：检测 WSL
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
# 内部函数：检测 macOS
###############################################################################
_is_macos() {
    if [[ "$(uname)" == "Darwin" ]]; then
        return 0
    fi
    return 1
}

###############################################################################
# 内部函数：检测 Linux
###############################################################################
_is_linux() {
    if [[ "$(uname)" == "Linux" ]]; then
        return 0
    fi
    return 1
}

###############################################################################
# 内部函数：检测 Windows
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
agentos_platform_detect() {
    if [[ $_AGENTOS_PLATFORM_DETECTED -eq 1 ]]; then
        echo "$_AGENTOS_PLATFORM"
        return
    fi

    if _is_wsl; then
        _AGENTOS_PLATFORM="$PLATFORM_WSL"
    elif _is_macos; then
        _AGENTOS_PLATFORM="$PLATFORM_MACOS"
    elif _is_linux; then
        _AGENTOS_PLATFORM="$PLATFORM_LINUX"
    elif _is_windows; then
        _AGENTOS_PLATFORM="$PLATFORM_WINDOWS"
    else
        _AGENTOS_PLATFORM="$PLATFORM_UNKNOWN"
    fi

    _AGENTOS_PLATFORM_DETECTED=1
    echo "$_AGENTOS_PLATFORM"
}

agentos_platform_is_linux() {
    local platform
    platform=$(agentos_platform_detect)
    [[ "$platform" == "$PLATFORM_LINUX" ]] || [[ "$platform" == "$PLATFORM_WSL" ]]
}

agentos_platform_is_macos() {
    local platform
    platform=$(agentos_platform_detect)
    [[ "$platform" == "$PLATFORM_MACOS" ]]
}

agentos_platform_is_windows() {
    local platform
    platform=$(agentos_platform_detect)
    [[ "$platform" == "$PLATFORM_WINDOWS" ]] || [[ "$platform" == "$PLATFORM_WSL" ]]
}

agentos_platform_is_wsl() {
    local platform
    platform=$(agentos_platform_detect)
    [[ "$platform" == "$PLATFORM_WSL" ]]
}

agentos_platform_is_unix() {
    agentos_platform_is_linux || agentos_platform_is_macos
}

###############################################################################
# 公共API：获取架构
###############################################################################
agentos_arch_detect() {
    if [[ -n "$_AGENTOS_ARCH" ]]; then
        echo "$_AGENTOS_ARCH"
        return
    fi

    local arch
    arch=$(uname -m)

    case "$arch" in
        x86_64)
            _AGENTOS_ARCH="$ARCH_X86_64"
            ;;
        amd64)
            _AGENTOS_ARCH="$ARCH_X86_64"
            ;;
        arm64)
            _AGENTOS_ARCH="$ARCH_ARM64"
            ;;
        aarch64)
            _AGENTOS_ARCH="$ARCH_AARCH64"
            ;;
        *)
            _AGENTOS_ARCH="$ARCH_UNKNOWN"
            ;;
    esac

    echo "$_AGENTOS_ARCH"
}

agentos_arch_is_x86_64() {
    [[ "$(agentos_arch_detect)" == "$ARCH_X86_64" ]]
}

agentos_arch_is_arm64() {
    local arch
    arch=$(agentos_arch_detect)
    [[ "$arch" == "$ARCH_ARM64" ]] || [[ "$arch" == "$ARCH_AARCH64" ]]
}

###############################################################################
# 公共API：获取 Linux 发行版信息
###############################################################################
agentos_linux_distro_detect() {
    if [[ -n "$_AGENTOS_DISTRO" ]]; then
        echo "$_AGENTOS_DISTRO"
        return
    fi

    if [[ -f /etc/os-release ]]; then
        _AGENTOS_DISTRO=$(grep "^ID=" /etc/os-release | cut -d= -f2 | tr -d '"')
        _AGENTOS_DISTRO_VERSION=$(grep "^VERSION_ID=" /etc/os-release | cut -d= -f2 | tr -d '"')
    elif [[ -f /etc/lsb-release ]]; then
        _AGENTOS_DISTRO=$(grep "^DISTRIB_ID=" /etc/lsb-release | cut -d= -f2)
        _AGENTOS_DISTRO_VERSION=$(grep "^DISTRIB_RELEASE=" /etc/lsb-release | cut -d= -f2)
    else
        _AGENTOS_DISTRO="unknown"
        _AGENTOS_DISTRO_VERSION=""
    fi

    echo "$_AGENTOS_DISTRO"
}

agentos_linux_distro_version() {
    if [[ -z "$_AGENTOS_DISTRO_VERSION" ]]; then
        agentos_linux_distro_detect > /dev/null
    fi
    echo "$_AGENTOS_DISTRO_VERSION"
}

###############################################################################
# 公共API：检测包管理器
###############################################################################
agentos_package_manager_detect() {
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
agentos_check_command() {
    local cmd="$1"
    if ! command -v "$cmd" &> /dev/null; then
        agentos_log_error "Required command not found: $cmd"
        return 1
    fi
    return 0
}

agentos_check_commands() {
    local missing=()
    local cmd
    for cmd in "$@"; do
        if ! command -v "$cmd" &> /dev/null; then
            missing+=("$cmd")
        fi
    done

    if [[ ${#missing[@]} -gt 0 ]]; then
        agentos_log_error "Missing required commands: ${missing[*]}"
        return 1
    fi
    return 0
}

###############################################################################
# 公共API：获取系统信息
###############################################################################
agentos_system_info() {
    local info=""
    info+="Platform: $(agentos_platform_detect)\n"
    info+="Architecture: $(agentos_arch_detect)\n"

    if agentos_platform_is_linux; then
        info+="Distribution: $(agentos_linux_distro_detect) $(agentos_linux_distro_version)\n"
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
agentos_cpu_count() {
    if [[ "$(uname)" == "Darwin" ]]; then
        sysctl -n hw.ncpu 2>/dev/null || echo "1"
    else
        nproc 2>/dev/null || grep -c ^processor /proc/cpuinfo 2>/dev/null || echo "1"
    fi
}

###############################################################################
# 公共API：内存信息
###############################################################################
agentos_total_memory() {
    if [[ "$(uname)" == "Darwin" ]]; then
        sysctl -n hw.memsize 2>/dev/null | awk '{printf "%.0f GB", $1/1024/1024/1024}'
    else
        awk '/MemTotal/ {printf "%.0f GB", $2/1024/1024}' /proc/meminfo 2>/dev/null || echo "unknown"
    fi
}

###############################################################################
# 导出公共API
###############################################################################
export -f agentos_platform_detect agentos_platform_is_linux agentos_platform_is_macos
export -f agentos_platform_is_windows agentos_platform_is_wsl agentos_platform_is_unix
export -f agentos_arch_detect agentos_arch_is_x86_64 agentos_arch_is_arm64
export -f agentos_linux_distro_detect agentos_linux_distro_version
export -f agentos_package_manager_detect agentos_check_command agentos_check_commands
export -f agentos_system_info agentos_cpu_count agentos_total_memory