#!/usr/bin/env bash
# shellcheck shell=bash
# =============================================================================
# agentrt-bootstrap.sh — AgentRT 一键启动脚本
# Copyright (C) 2025-2026 SPHARX Ltd.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
#
# P1.23.3: 按 DAG 层级顺序启动所有 daemon，等待每个 daemon
#          健康检查通过后再启动下一层。
#
# 用法:
#   bash agentrt-bootstrap.sh [选项]
#
# 选项:
#   -c <config>    指定 agentos.yaml 配置文件
#   -b <bindir>    指定 daemon 二进制目录 (默认: /usr/local/bin)
#   -r <runtimedir> 指定运行时目录 (默认: /tmp/agentos)
#   -t <timeout>   全局健康检查超时秒数 (默认: 120)
#   -s             静默模式（减少输出）
#   -n             dry-run（只打印启动计划，不实际启动）
#   -h             显示帮助
#
# 验收: bash agentrt-bootstrap.sh → 所有 daemon 按序启动 → agentrt status 全部在线
# =============================================================================

set -euo pipefail

# ==================== 颜色/输出 ====================

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

SILENT=0
DRY_RUN=0

log_info()  { ((SILENT)) || echo -e "${GREEN}[INFO]${NC} $*"; }
log_warn()  { ((SILENT)) || echo -e "${YELLOW}[WARN]${NC} $*"; }
log_error() { echo -e "${RED}[ERROR]${NC} $*" >&2; }
log_step()  { ((SILENT)) || echo -e "${CYAN}[STEP]${NC} $*"; }
log_debug() { ((SILENT)) || echo -e "${BLUE}[DEBUG]${NC} $*"; }

# ==================== 默认值 ====================

AGENTOS_BINDIR="${AGENTOS_BINDIR:-/usr/local/bin}"
AGENTOS_RUNTIME_DIR="${AGENTOS_RUNTIME_DIR:-/tmp/agentos}"
AGENTOS_CONFIG="${AGENTOS_CONFIG:-}"
GLOBAL_TIMEOUT_SEC=120
HEALTH_CHECK_INTERVAL_SEC=1

# ==================== DAG 定义 ====================
#
# 与 daemon_startup.h 保持一致，5 层启动 DAG。
# 同层内可并行启动，跨层必须等待前层健康检查通过。
#

# Layer 0: 基础设施（无依赖）
DAEMON_LAYER_0=("monit_d" "observe_d" "info_d" "notify_d")

# Layer 1: 核心服务
DAEMON_LAYER_1=("sched_d" "channel_d")

# Layer 2: Agent 服务
DAEMON_LAYER_2=("llm_d" "tool_d" "hook_d" "plugin_d")

# Layer 3: 业务服务
DAEMON_LAYER_3=("market_d")

# Layer 4: 网关
DAEMON_LAYER_4=("gateway_d")

ALL_LAYERS=("DAEMON_LAYER_0" "DAEMON_LAYER_1" "DAEMON_LAYER_2" "DAEMON_LAYER_3" "DAEMON_LAYER_4")

# daemon 健康检查超时 (秒)
declare -A DAEMON_HEALTH_TIMEOUT=(
    [monit_d]=15    [observe_d]=15   [info_d]=15     [notify_d]=15
    [sched_d]=20    [channel_d]=20
    [llm_d]=30      [tool_d]=30      [hook_d]=20     [plugin_d]=30
    [market_d]=30
    [gateway_d]=30
)

# daemon 默认端口 (0 = Unix Socket)
declare -A DAEMON_PORT=(
    [gateway_d]=8080 [tool_d]=8082
)

# daemon 二进制名称映射 (daemon_name -> binary_name)
# CMake 构建产出使用 agentos-<name>-d 命名，channel_d/gateway_d 例外
declare -A DAEMON_BIN_NAME=(
    [monit_d]="agentos-monit-d"
    [observe_d]="agentos-observe-d"
    [info_d]="agentos-info-d"
    [notify_d]="agentos-notify-d"
    [sched_d]="agentos-sched-d"
    [channel_d]="channel_d"
    [llm_d]="agentos-llm-d"
    [tool_d]="agentos-tool-d"
    [hook_d]="agentos-hook-d"
    [plugin_d]="agentos-plugin-d"
    [market_d]="agentos-market-d"
    [gateway_d]="gateway_d"
)

# ==================== 运行时状态 ====================

declare -A DAEMON_PIDS=()       # daemon_name -> PID
FAILED_DAEMONS=()               # 启动失败的 daemon 列表

# ==================== 工具函数 ====================

print_usage() {
    cat <<'EOF'
AgentRT Bootstrap Script — 一键按序启动所有 daemon

Usage: bash agentrt-bootstrap.sh [options]

Options:
  -c <config>      指定 agentos.yaml 配置文件
  -b <bindir>      指定 daemon 二进制目录 (默认: /usr/local/bin)
  -r <runtimedir>  指定运行时目录 (默认: /tmp/agentos)
  -t <timeout>     全局健康检查超时秒数 (默认: 120)
  -s               静默模式（减少输出）
  -n               dry-run（只打印启动计划，不实际启动）
  -h               显示帮助

Startup DAG:
  Layer 0: monit_d, observe_d, info_d, notify_d
  Layer 1: sched_d, channel_d
  Layer 2: llm_d, tool_d, hook_d, plugin_d
  Layer 3: market_d
  Layer 4: gateway_d

Examples:
  bash agentrt-bootstrap.sh
  bash agentrt-bootstrap.sh -b ./build/bin -r /var/run/agentos
  bash agentrt-bootstrap.sh -n  # dry-run
EOF
}

parse_args() {
    while getopts ":c:b:r:t:snh" opt; do
        case "$opt" in
            c) AGENTOS_CONFIG="$OPTARG" ;;
            b) AGENTOS_BINDIR="$OPTARG" ;;
            r) AGENTOS_RUNTIME_DIR="$OPTARG" ;;
            t) GLOBAL_TIMEOUT_SEC="$OPTARG" ;;
            s) SILENT=1 ;;
            n) DRY_RUN=1 ;;
            h) print_usage; exit 0 ;;
            *) log_error "Unknown option: -$OPTARG"; print_usage; exit 1 ;;
        esac
    done
}

# ==================== 健康检查 ====================

check_daemon_health_unix() {
    local name="$1"
    # daemon socket 名称不带 _d 后缀 (monit_d → monit.sock)
    local short_name="${name%_d}"
    local sock_path="${AGENTOS_RUNTIME_DIR}/${short_name}.sock"

    # 检查 Unix Socket 是否存在且可连接
    if [[ -S "$sock_path" ]]; then
        return 0
    fi
    return 1
}

check_daemon_health_tcp() {
    local name="$1"
    local port="${DAEMON_PORT[$name]:-0}"

    if [[ "$port" -eq 0 ]]; then
        # 无 TCP 端口，回退到 Unix Socket 检查
        check_daemon_health_unix "$name"
        return $?
    fi

    # TCP 端口检查
    if command -v nc &>/dev/null; then
        nc -z 127.0.0.1 "$port" 2>/dev/null && return 0
    elif command -v curl &>/dev/null; then
        curl -sf --max-time 2 "http://127.0.0.1:${port}/health" &>/dev/null && return 0
    elif command -v ss &>/dev/null; then
        ss -tln 2>/dev/null | grep -q ":${port} " && return 0
    fi

    # TCP 检查失败，回退到 Unix Socket 检查
    check_daemon_health_unix "$name"
    return $?
}

check_daemon_health() {
    local name="$1"
    local pid="${DAEMON_PIDS[$name]:-}"

    # 先检查进程是否存活
    if [[ -n "$pid" ]] && ! kill -0 "$pid" 2>/dev/null; then
        return 1
    fi

    # 检查健康状态
    check_daemon_health_tcp "$name"
    return $?
}

wait_for_daemon() {
    local name="$1"
    local timeout="${DAEMON_HEALTH_TIMEOUT[$name]:-30}"
    local elapsed=0

    log_debug "Waiting for $name (timeout=${timeout}s)..."

    while [[ $elapsed -lt $timeout ]]; do
        if check_daemon_health "$name"; then
            log_info "$name is healthy (${elapsed}s)"
            return 0
        fi
        sleep "$HEALTH_CHECK_INTERVAL_SEC"
        ((elapsed += HEALTH_CHECK_INTERVAL_SEC))
    done

    log_error "$name health check FAILED after ${timeout}s"
    return 1
}

# ==================== 启动/停止 ====================

start_daemon() {
    local name="$1"
    local bin_name="${DAEMON_BIN_NAME[$name]:-$name}"
    local bin_path="${AGENTOS_BINDIR}/${bin_name}"

    local cmd=("$bin_path")
    if [[ -n "$AGENTOS_CONFIG" ]]; then
        cmd+=("-c" "$AGENTOS_CONFIG")
    fi

    log_step "Starting $name..."
    log_debug "  Command: ${cmd[*]}"

    if ((DRY_RUN)); then
        log_info "[DRY-RUN] Would start: ${cmd[*]}"
        DAEMON_PIDS[$name]=$$
        return 0
    fi

    if [[ ! -x "$bin_path" ]]; then
        log_error "Binary not found or not executable: $bin_path"
        FAILED_DAEMONS+=("$name")
        return 1
    fi

    # 确保 runtime 目录存在
    mkdir -p "$AGENTOS_RUNTIME_DIR"

    # 启动 daemon（后台运行）
    "${cmd[@]}" &
    local pid=$!
    DAEMON_PIDS[$name]=$pid

    log_debug "  PID=$pid"
    return 0
}

stop_daemon() {
    local name="$1"
    local pid="${DAEMON_PIDS[$name]:-}"

    if [[ -z "$pid" ]] || ! kill -0 "$pid" 2>/dev/null; then
        return 0
    fi

    log_step "Stopping $name (PID=$pid)..."
    kill -TERM "$pid" 2>/dev/null || true

    local elapsed=0
    while kill -0 "$pid" 2>/dev/null && [[ $elapsed -lt 5 ]]; do
        sleep 1
        ((elapsed++))
    done

    if kill -0 "$pid" 2>/dev/null; then
        log_warn "$name did not stop gracefully, force killing..."
        kill -9 "$pid" 2>/dev/null || true
    fi

    unset DAEMON_PIDS[$name]
}

stop_all_daemons() {
    log_step "Stopping all daemons (reverse order)..."

    # 逆序停止
    for ((layer=${#ALL_LAYERS[@]}-1; layer>=0; layer--)); do
        local layer_var="${ALL_LAYERS[$layer]}"
        local -n daemons="$layer_var"
        for ((i=${#daemons[@]}-1; i>=0; i--)); do
            stop_daemon "${daemons[$i]}"
        done
    done
}

# ==================== 状态查询 ====================

show_status() {
    echo ""
    echo "=============================="
    echo "  AgentRT Daemon Status"
    echo "=============================="

    local all_online=true
    for layer_var in "${ALL_LAYERS[@]}"; do
        local -n daemons="$layer_var"
        for name in "${daemons[@]}"; do
            local pid="${DAEMON_PIDS[$name]:-}"
            if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
                if check_daemon_health "$name"; then
                    log_info "$name: ONLINE (PID=$pid)"
                else
                    log_warn "$name: RUNNING but UNHEALTHY (PID=$pid)"
                    all_online=false
                fi
            else
                log_error "$name: OFFLINE"
                all_online=false
            fi
        done
    done

    echo "=============================="
    if $all_online; then
        log_info "All daemons are ONLINE"
    else
        log_error "Some daemons are NOT online"
    fi
}

# ==================== 信号处理 ====================

cleanup() {
    log_warn "Received shutdown signal, stopping all daemons..."
    stop_all_daemons
    exit 130
}

trap cleanup SIGINT SIGTERM

# ==================== 主流程 ====================

main() {
    parse_args "$@"

    log_info "AgentRT Bootstrap v0.1.1"
    log_info "  Bindir:    $AGENTOS_BINDIR"
    log_info "  Runtime:   $AGENTOS_RUNTIME_DIR"
    log_info "  Config:    ${AGENTOS_CONFIG:-<none>}"
    log_info "  Timeout:   ${GLOBAL_TIMEOUT_SEC}s"
    log_info "  Dry-run:   $DRY_RUN"
    echo ""

    # 前置检查
    if ! ((DRY_RUN)) && [[ ! -d "$AGENTOS_BINDIR" ]]; then
        log_error "Binary directory not found: $AGENTOS_BINDIR"
        exit 1
    fi

    # 逐层启动
    local layer_num=0
    local total_started=0
    local total_failed=0

    for layer_var in "${ALL_LAYERS[@]}"; do
        local -n daemons="$layer_var"
        log_step "=== Layer $layer_num: ${daemons[*]} ==="

        # 同层并行启动
        for name in "${daemons[@]}"; do
            if start_daemon "$name"; then
                total_started=$((total_started + 1))
            else
                total_failed=$((total_failed + 1))
            fi
        done

        # 等待同层所有 daemon 健康检查通过
        if ! ((DRY_RUN)); then
            for name in "${daemons[@]}"; do
                if [[ -n "${DAEMON_PIDS[$name]:-}" ]]; then
                    if ! wait_for_daemon "$name"; then
                        log_error "$name failed health check, aborting..."
                        FAILED_DAEMONS+=("$name")
                        stop_all_daemons
                        exit 1
                    fi
                fi
            done
        fi

        ((layer_num++)) || true
        echo ""
    done

    # 最终状态
    show_status

    if [[ ${#FAILED_DAEMONS[@]} -gt 0 ]]; then
        log_error "Failed daemons: ${FAILED_DAEMONS[*]}"
        exit 1
    fi

    log_info "Bootstrap complete — all ${total_started} daemons started successfully"
    return 0
}

main "$@"
