#!/usr/bin/env bash
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS 安装脚本 (Linux/macOS)
# 遵循 AgentOS 架构设计原则：反馈闭环、最小特权、安全内生

###############################################################################
# 严格模式
###############################################################################
set -euo pipefail

###############################################################################
# 来源依赖
###############################################################################
AGENTOS_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
AGENTOS_SCRIPTS_DIR="$(dirname "$AGENTOS_SCRIPT_DIR")"
AGENTOS_PROJECT_ROOT="$(dirname "$AGENTOS_SCRIPTS_DIR")"

# shellcheck source=$AGENTOS_SCRIPTS_DIR/lib/common.sh
source "$AGENTOS_SCRIPTS_DIR/lib/common.sh"

###############################################################################
# 版本信息
###############################################################################
declare -r AGENTOS_VERSION="1.0.0.6"
declare -r AGENTOS_INSTALLER_VERSION="1.0.0"

###############################################################################
# 安装配置
###############################################################################
INSTALL_PREFIX="${INSTALL_PREFIX:-/usr/local}"
INSTALL_SYSCONF_DIR="${INSTALL_SYSCONF_DIR:-/etc/agentos}"
INSTALL_LOCAL_STATE_DIR="${INSTALL_LOCAL_STATE_DIR:-/var/lib/agentos}"
INSTALL_LOG_DIR="${INSTALL_LOG_DIR:-/var/log/agentos}"
INSTALL_RUN_DIR="${INSTALL_RUN_DIR:-/var/run/agentos}"
INSTALL_USER="${INSTALL_USER:-}"
INSTALL_GROUP="${INSTALL_GROUP:-}"
INSTALL_MODE="${INSTALL_MODE:-0755}"
INSTALL_SKIP_DEPS="${INSTALL_SKIP_DEPS:-0}"
INSTALL_FORCE="${INSTALL_FORCE:-0}"
INSTALL_UNINSTALL="${INSTALL_UNINSTALL:-0}"
INSTALL_VERIFY="${INSTALL_VERIFY:-1}"

###############################################################################
# 颜色定义
###############################################################################
COLOR_BOLD='\033[1m'
COLOR_DIM='\033[2m'
COLOR_RED='\033[0;31m'
COLOR_GREEN='\033[0;32m'
COLOR_YELLOW='\033[1;33m'
COLOR_BLUE='\033[0;34m'
COLOR_CYAN='\033[0;36m'
COLOR_NC='\033[0m'

###############################################################################
# 全局状态
###############################################################################
INSTALL_START_TIME=0
INSTALL_END_TIME=0
INSTALL_SUCCESS=0
INSTALL_DRY_RUN=0
declare -a INSTALL_STEPS=()
declare -a INSTALL_ERRORS=()

###############################################################################
# 打印函数
###############################################################################
print_banner() {
    echo -e "${COLOR_CYAN}"
    cat << "EOF"
   ____          _        __  __                                                   _
  / __ \        | |      |  \/  |                                                 | |
 | |  | |_ __   | | __ _| \  / | __ _ _ __   __ _  ___ _ __ ___   ___  _ __  __ _| |_
 | |  | | '_ \ / _` |/ _` | |\/| |/ _` | '_ \ / _` |/ _ \ '_ ` _ \ / _ \| '_ \/ _` | __|
 | |__| | |_) | (_| | (_| | |  | | (_| | | | | (_| |  __/ | | | | | (_) | | | | (_| | |_
  \____/| .__/ \__,_|\__,_|_|  |_|\__,_|_| |_|\__, |\___|_| |_| |_|\___/|_| |_|\__,_|\__|
        | |                                     __/ |
        |_|                                    |___/
EOF
    echo -e "${COLOR_NC}"
    echo -e "${COLOR_BOLD}AgentOS Installer v${AGENTOS_INSTALLER_VERSION}${COLOR_NC}"
    echo ""
}

print_usage() {
    cat << EOF
${COLOR_BOLD}用法:${COLOR_NC} $0 [选项]

${COLOR_BOLD}安装选项:${COLOR_NC}
    --prefix <path>         安装前缀 (默认: ${INSTALL_PREFIX})
    --sysconfdir <path>     系统配置目录 (默认: ${INSTALL_SYSCONF_DIR})
    --localstatedir <path>  本地状态目录 (默认: ${INSTALL_LOCAL_STATE_DIR})
    --logdir <path>         日志目录 (默认: ${INSTALL_LOG_DIR})

${COLOR_BOLD}用户选项:${COLOR_NC}
    --user <name>           运行用户 (默认: 当前用户)
    --group <name>          运行组 (默认: 与用户同组)
    --mode <octal>          权限模式 (默认: ${INSTALL_MODE})

${COLOR_BOLD}行为选项:${COLOR_NC}
    --force                 强制安装，覆盖已有文件
    --dry-run               模拟安装，不实际写入文件
    --skip-deps            跳过依赖检查
    --no-verify            跳过安装后验证
    --uninstall            卸载 AgentOS

${COLOR_BOLD}输出选项:${COLOR_NC}
    --verbose               详细输出
    --quiet                 静默输出

${COLOR_BOLD}示例:${COLOR_NC}
    $0 --prefix /opt/agentos                   # 自定义安装路径
    $0 --user agentos --group agentos          # 创建专用用户
    $0 --uninstall                             # 卸载 AgentOS
    $0 --dry-run --verbose                    # 模拟安装

EOF
}

print_section() {
    echo ""
    echo -e "${COLOR_BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${COLOR_NC}"
    echo -e "${COLOR_BLUE}▶ $1${COLOR_NC}"
    echo -e "${COLOR_BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${COLOR_NC}"
    echo ""
}

print_status() {
    local status=$1
    local message=$2
    case "$status" in
        ok)      echo -e "${COLOR_GREEN}[✓]${COLOR_NC} $message" ;;
        fail)    echo -e "${COLOR_RED}[✗]${COLOR_NC} $message" ;;
        info)    echo -e "${COLOR_BLUE}[•]${COLOR_NC} $message" ;;
        warn)    echo -e "${COLOR_YELLOW}[!]${COLOR_NC} $message" ;;
        skip)    echo -e "${COLOR_DIM}[-]${COLOR_NC} $message" ;;
    esac
}

###############################################################################
# 前置检查
###############################################################################
check_root() {
    if [[ $INSTALL_UNINSTALL -eq 1 ]]; then
        return 0
    fi

    if [[ $INSTALL_DRY_RUN -eq 1 ]]; then
        return 0
    fi

    if [[ "$(id -u)" != "0" ]]; then
        if [[ "$INSTALL_PREFIX" == /usr/local ]]; then
            print_status "warn" "非 root 用户安装到系统目录可能需要 sudo"
        fi
    fi
}

check_platform() {
    if ! agentos_platform_is_unix; then
        print_status "fail" "此脚本仅支持 Linux 和 macOS"
        return 1
    fi
    print_status "info" "平台: $(agentos_platform_detect) $(agentos_arch_detect)"
    return 0
}

check_dependencies() {
    if [[ "$INSTALL_SKIP_DEPS" == "1" ]]; then
        print_status "skip" "跳过依赖检查"
        return 0
    fi

    print_section "检查依赖"

    local deps=("bash" "coreutils")
    local missing=()

    if agentos_platform_is_linux; then
        deps+=("systemd" "procps")
    fi

    for dep in "${deps[@]}"; do
        case "$dep" in
            bash)
                if [[ "${BASH_VERSION:-0}" < "4.0" ]]; then
                    missing+=("bash >= 4.0")
                fi
                ;;
            *)
                if ! command -v "$dep" &> /dev/null; then
                    missing+=("$dep")
                fi
                ;;
        esac
    done

    if [[ ${#missing[@]} -gt 0 ]]; then
        print_status "fail" "缺少依赖: ${missing[*]}"
        return 1
    fi

    print_status "ok" "所有依赖已满足"
    return 0
}

check_existing_installation() {
    if [[ "$INSTALL_FORCE" == "1" ]]; then
        return 0
    fi

    local install_marker="$INSTALL_PREFIX/lib/agentos/agentos.version"

    if [[ -f "$install_marker" ]]; then
        local existing_version
        existing_version=$(cat "$install_marker" 2>/dev/null || echo "unknown")
        print_status "warn" "检测到已安装版本: $existing_version"
        if ! agentos_confirm "是否继续安装?"; then
            exit 0
        fi
    fi
}

###############################################################################
# 安装步骤
###############################################################################
create_directories() {
    print_section "创建目录结构"

    local dirs=(
        "$INSTALL_PREFIX"
        "$INSTALL_PREFIX/bin"
        "$INSTALL_PREFIX/lib/agentos"
        "$INSTALL_PREFIX/lib/agentos/backends"
        "$INSTALL_PREFIX/lib/agentos/modules"
        "$INSTALL_PREFIX/include/agentos"
        "$INSTALL_SYSCONF_DIR"
        "$INSTALL_LOCAL_STATE_DIR"
        "$INSTALL_LOG_DIR"
        "$INSTALL_RUN_DIR"
    )

    for dir in "${dirs[@]}"; do
        if [[ -d "$dir" ]]; then
            print_status "skip" "目录已存在: $dir"
        else
            if [[ "$INSTALL_DRY_RUN" == "1" ]]; then
                print_status "info" "[DRY] 创建目录: $dir"
            else
                mkdir -p "$dir"
                print_status "ok" "创建目录: $dir"
            fi
        fi
    done
}

install_binaries() {
    print_section "安装二进制文件"

    local build_dir="$AGENTOS_PROJECT_ROOT/build"
    local binaries=(
        "$build_dir/bin/agentos"
        "$build_dir/bin/agentosd"
        "$build_dir/bin/agentos-cli"
    )

    if [[ "$INSTALL_DRY_RUN" == "1" ]]; then
        print_status "info" "[DRY] 将安装 ${#binaries[@]} 个二进制文件"
        return 0
    fi

    for binary in "${binaries[@]}"; do
        if [[ -f "$binary" ]]; then
            local dest="$INSTALL_PREFIX/bin/$(basename "$binary")"
            cp "$binary" "$dest"
            chmod "$INSTALL_MODE" "$dest"
            print_status "ok" "安装: $(basename "$binary")"
        fi
    done
}

install_libraries() {
    print_section "安装库文件"

    local build_dir="$AGENTOS_PROJECT_ROOT/build"
    local libs=(
        "libagentos_corekern.so"
        "libagentos_coreloopthree.so"
        "libagentos_memoryrovol.so"
        "libagentos_syscall.so"
    )

    if [[ "$INSTALL_DRY_RUN" == "1" ]]; then
        print_status "info" "[DRY] 将安装 ${#libs[@]} 个库文件"
        return 0
    fi

    for lib in "${libs[@]}"; do
        local lib_path
        lib_path=$(find "$build_dir" -name "$lib*" -type f 2>/dev/null | head -1)
        if [[ -n "$lib_path" ]]; then
            cp "$lib_path" "$INSTALL_PREFIX/lib/agentos/"
            chmod "$INSTALL_MODE" "$INSTALL_PREFIX/lib/agentos/$(basename "$lib_path")"
            print_status "ok" "安装: $(basename "$lib_path")"
        fi
    done

    if command -v ldconfig &> /dev/null; then
        ldconfig "$INSTALL_PREFIX/lib/agentos" 2>/dev/null || true
    fi
}

install_headers() {
    print_section "安装头文件"

    local headers_dir="$AGENTOS_PROJECT_ROOT/atoms"
    local include_dir="$INSTALL_PREFIX/include/agentos"

    if [[ ! -d "$headers_dir" ]]; then
        print_status "skip" "未找到头文件目录"
        return 0
    fi

    if [[ "$INSTALL_DRY_RUN" == "1" ]]; then
        print_status "info" "[DRY] 将安装头文件到: $include_dir"
        return 0
    fi

    find "$headers_dir" -name "*.h" -type f 2>/dev/null | while read -r header; do
        local rel_path="${header#$headers_dir/}"
        local dest_dir="$include_dir/$(dirname "$rel_path")"
        mkdir -p "$dest_dir"
        cp "$header" "$dest_dir/"
        print_status "ok" "安装: $rel_path"
    done
}

install_config() {
    print_section "安装配置文件"

    local config_files=(
        "$AGENTOS_PROJECT_ROOT/config/agentos.conf"
        "$AGENTOS_PROJECT_ROOT/config/logging.conf"
        "$AGENTOS_PROJECT_ROOT/config/memory.conf"
    )

    if [[ "$INSTALL_DRY_RUN" == "1" ]]; then
        print_status "info" "[DRY] 将安装配置文件到: $INSTALL_SYSCONF_DIR"
        return 0
    fi

    mkdir -p "$INSTALL_SYSCONF_DIR"

    for config in "${config_files[@]}"; do
        if [[ -f "$config" ]]; then
            local dest="$INSTALL_SYSCONF_DIR/$(basename "$config")"
            if [[ -f "$dest" ]] && [[ "$INSTALL_FORCE" != "1" ]]; then
                print_status "skip" "配置文件已存在: $(basename "$config")"
            else
                cp "$config" "$dest"
                print_status "ok" "安装: $(basename "$config")"
            fi
        fi
    done
}

install_systemd_service() {
    if ! agentos_platform_is_linux; then
        return 0
    fi

    print_section "安装系统服务"

    local service_file="$AGENTOS_PROJECT_ROOT/deploy/systemd/agentos.service"
    local service_dest="/etc/systemd/system/agentos.service"

    if [[ ! -f "$service_file" ]]; then
        print_status "skip" "未找到 systemd 服务文件"
        return 0
    fi

    if [[ "$INSTALL_DRY_RUN" == "1" ]]; then
        print_status "info" "[DRY] 将安装 systemd 服务"
        return 0
    fi

    if [[ "$(id -u)" != "0" ]]; then
        print_status "skip" "需要 root 权限安装 systemd 服务"
        return 0
    fi

    cp "$service_file" "$service_dest"
    systemctl daemon-reload
    print_status "ok" "systemd 服务已安装"
}

install_scripts() {
    print_section "安装脚本工具"

    local scripts=(
        "$AGENTOS_SCRIPTS_DIR/build/build.sh"
        "$AGENTOS_SCRIPTS_DIR/ops/benchmark.py"
        "$AGENTOS_SCRIPTS_DIR/ops/doctor.py"
    )

    if [[ "$INSTALL_DRY_RUN" == "1" ]]; then
        print_status "info" "[DRY] 将安装脚本到: $INSTALL_PREFIX/lib/agentos/scripts"
        return 0
    fi

    mkdir -p "$INSTALL_PREFIX/lib/agentos/scripts"

    for script in "${scripts[@]}"; do
        if [[ -f "$script" ]]; then
            cp "$script" "$INSTALL_PREFIX/lib/agentos/scripts/"
            chmod +x "$INSTALL_PREFIX/lib/agentos/scripts/$(basename "$script")"
            print_status "ok" "安装: $(basename "$script")"
        fi
    done
}

create_version_marker() {
    print_section "创建版本标记"

    local marker="$INSTALL_PREFIX/lib/agentos/agentos.version"

    if [[ "$INSTALL_DRY_RUN" == "1" ]]; then
        print_status "info" "[DRY] 创建版本标记: $marker"
        return 0
    fi

    echo "$AGENTOS_VERSION" > "$marker"
    echo "Install date: $(date -Iseconds)" >> "$marker"
    echo "Install prefix: $INSTALL_PREFIX" >> "$marker"

    print_status "ok" "版本标记已创建"
}

verify_installation() {
    if [[ "$INSTALL_VERIFY" != "1" ]]; then
        return 0
    fi

    print_section "验证安装"

    local errors=0

    if [[ ! -d "$INSTALL_PREFIX/lib/agentos" ]]; then
        print_status "fail" "库目录不存在: $INSTALL_PREFIX/lib/agentos"
        ((errors++))
    fi

    if [[ ! -d "$INSTALL_SYSCONF_DIR" ]]; then
        print_status "fail" "配置目录不存在: $INSTALL_SYSCONF_DIR"
        ((errors++))
    fi

    if [[ $errors -eq 0 ]]; then
        print_status "ok" "安装验证通过"
        return 0
    else
        print_status "fail" "安装验证失败 ($errors 个错误)"
        return 1
    fi
}

###############################################################################
# 卸载步骤
###############################################################################
uninstall_agentos() {
    print_section "卸载 AgentOS"

    if [[ "$(id -u)" != "0" ]]; then
        print_status "warn" "建议使用 root 权限执行卸载"
    fi

    if ! agentos_confirm "确定要卸载 AgentOS 吗?"; then
        exit 0
    fi

    print_status "info" "移除二进制文件..."
    rm -f "$INSTALL_PREFIX/bin/agentos" "$INSTALL_PREFIX/bin/agentosd" "$INSTALL_PREFIX/bin/agentos-cli" 2>/dev/null || true

    print_status "info" "移除库文件..."
    rm -rf "$INSTALL_PREFIX/lib/agentos" 2>/dev/null || true

    print_status "info" "移除头文件..."
    rm -rf "$INSTALL_PREFIX/include/agentos" 2>/dev/null || true

    print_status "info" "移除配置文件..."
    rm -rf "$INSTALL_SYSCONF_DIR" 2>/dev/null || true

    print_status "info" "移除 systemd 服务..."
    rm -f "/etc/systemd/system/agentos.service" 2>/dev/null || true

    print_status "ok" "卸载完成"
}

###############################################################################
# 主流程
###############################################################################
main() {
    INSTALL_START_TIME=$(date +%s)

    print_banner

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --prefix)          INSTALL_PREFIX="$2"; shift 2 ;;
            --sysconfdir)      INSTALL_SYSCONF_DIR="$2"; shift 2 ;;
            --localstatedir)   INSTALL_LOCAL_STATE_DIR="$2"; shift 2 ;;
            --logdir)          INSTALL_LOG_DIR="$2"; shift 2 ;;
            --user)            INSTALL_USER="$2"; shift 2 ;;
            --group)           INSTALL_GROUP="$2"; shift 2 ;;
            --mode)            INSTALL_MODE="$2"; shift 2 ;;
            --force)           INSTALL_FORCE=1; shift ;;
            --dry-run)         INSTALL_DRY_RUN=1; shift ;;
            --skip-deps)       INSTALL_SKIP_DEPS=1; shift ;;
            --no-verify)       INSTALL_VERIFY=0; shift ;;
            --uninstall)       INSTALL_UNINSTALL=1; shift ;;
            --verbose)         set -x; shift ;;
            --quiet)           exec > /dev/null; shift ;;
            --help|-h)         print_usage; exit 0 ;;
            *)                  echo "未知选项: $1"; print_usage; exit 1 ;;
        esac
    done

    if [[ "$INSTALL_UNINSTALL" == "1" ]]; then
        uninstall_agentos
        exit 0
    fi

    check_platform
    check_root
    check_dependencies
    check_existing_installation

    create_directories
    install_binaries
    install_libraries
    install_headers
    install_config
    install_scripts
    install_systemd_service
    create_version_marker
    verify_installation

    INSTALL_END_TIME=$(date +%s)
    INSTALL_SUCCESS=1

    print_section "安装完成"

    local duration=$((INSTALL_END_TIME - INSTALL_START_TIME))
    echo -e "${COLOR_GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${COLOR_NC}"
    echo -e "${COLOR_BOLD}  安装成功!${COLOR_NC}"
    echo -e "${COLOR_DIM}  版本: ${AGENTOS_VERSION}${COLOR_NC}"
    echo -e "${COLOR_DIM}  安装路径: ${INSTALL_PREFIX}${COLOR_NC}"
    echo -e "${COLOR_DIM}  耗时: ${duration} 秒${COLOR_NC}"
    echo -e "${COLOR_GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${COLOR_NC}"
    echo ""

    if [[ "$INSTALL_DRY_RUN" == "1" ]]; then
        echo -e "${COLOR_YELLOW}注意: 这是模拟安装，未实际写入任何文件${COLOR_NC}"
        echo ""
    fi

    return 0
}

###############################################################################
# 错误处理
###############################################################################
trap '[[ $? -ne 0 ]] && print_status "fail" "安装过程出错"' EXIT

###############################################################################
# 执行入口
###############################################################################
main "$@"