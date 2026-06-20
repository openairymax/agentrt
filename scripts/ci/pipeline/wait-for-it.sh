#!/usr/bin/env bash
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# wait-for-it.sh — 等待服务就绪的通用健康检查脚本
# P1.17.2: 实现健康检查等待
#
# 用法:
#   wait-for-it.sh <host>:<port> [--timeout=<seconds>] [--interval=<seconds>] [--command=<cmd>]
#
# 示例:
#   wait-for-it.sh redis:6379 --timeout=30
#   wait-for-it.sh corekern:9001/healthz --timeout=60 --interval=3
#   wait-for-it.sh postgres:5432 --timeout=30 --command="echo 'DB ready'"
#
# 退出码:
#   0 - 服务在超时前就绪
#   1 - 超时，服务未就绪
#   2 - 参数错误

set -euo pipefail

# 默认值
TIMEOUT=30
INTERVAL=2
CMD=""
QUIET=0
PROTOCOL="tcp"
HEALTHZ_PATH=""

usage() {
    cat <<EOF
Usage: $(basename "$0") <host>:<port>[/path] [OPTIONS]

等待 TCP 服务或 HTTP 健康检查端点就绪。

参数:
  host:port[/path]    目标主机和端口。如果提供 /path，则使用 HTTP 健康检查模式。

选项:
  --timeout=<seconds>   最大等待时间（默认: 30）
  --interval=<seconds>  重试间隔（默认: 2）
  --command=<cmd>       服务就绪后执行的命令
  --quiet               静默模式，不输出日志
  -h, --help            显示此帮助信息

退出码:
  0  服务就绪
  1  超时
  2  参数错误

示例:
  $(basename "$0") redis:6379
  $(basename "$0") corekern:9001/healthz --timeout=60
  $(basename "$0") postgres:5432 --command="echo 'DB ready'"
EOF
    exit 2
}

# 解析参数
parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --timeout=*)
                TIMEOUT="${1#*=}"
                shift
                ;;
            --timeout)
                TIMEOUT="$2"
                shift 2
                ;;
            --interval=*)
                INTERVAL="${1#*=}"
                shift
                ;;
            --interval)
                INTERVAL="$2"
                shift 2
                ;;
            --command=*)
                CMD="${1#*=}"
                shift
                ;;
            --command)
                CMD="$2"
                shift 2
                ;;
            --quiet)
                QUIET=1
                shift
                ;;
            -h|--help)
                usage
                ;;
            *)
                # 解析 host:port[/path]
                if [[ "$1" =~ ^([^:]+):([0-9]+)(/(.*))?$ ]]; then
                    HOST="${BASH_REMATCH[1]}"
                    PORT="${BASH_REMATCH[2]}"
                    HEALTHZ_PATH="${BASH_REMATCH[4]:-}"
                else
                    echo "ERROR: Invalid target format: $1" >&2
                    echo "Expected: host:port[/path]" >&2
                    usage
                fi
                shift
                ;;
        esac
    done

    if [[ -z "${HOST:-}" ]] || [[ -z "${PORT:-}" ]]; then
        echo "ERROR: Missing host:port argument" >&2
        usage
    fi
}

log() {
    if [[ "${QUIET}" -eq 0 ]]; then
        echo "[$(date '+%H:%M:%S')] $*"
    fi
}

# HTTP 健康检查模式
check_http() {
    local url="http://${HOST}:${PORT}/${HEALTHZ_PATH}"
    if curl -s -o /dev/null -w "%{http_code}" --max-time 3 "${url}" 2>/dev/null | grep -qE "^(200|204)$"; then
        return 0
    fi
    return 1
}

# TCP 端口检查模式
check_tcp() {
    if timeout 3 bash -c "echo > /dev/tcp/${HOST}/${PORT}" 2>/dev/null; then
        return 0
    fi
    return 1
}

# 主等待循环
wait_for() {
    local start_time
    start_time=$(date +%s)

    log "Waiting for ${HOST}:${PORT}${HEALTHZ_PATH:+/${HEALTHZ_PATH}} (timeout: ${TIMEOUT}s, interval: ${INTERVAL}s)..."

    while true; do
        local elapsed
        elapsed=$(($(date +%s) - start_time))

        if [[ "${elapsed}" -ge "${TIMEOUT}" ]]; then
            log "TIMEOUT: ${HOST}:${PORT} not ready after ${TIMEOUT}s"
            return 1
        fi

        if [[ -n "${HEALTHZ_PATH}" ]]; then
            if check_http; then
                log "READY: ${HOST}:${PORT}/${HEALTHZ_PATH} (after ${elapsed}s)"
                return 0
            fi
        else
            if check_tcp; then
                log "READY: ${HOST}:${PORT} (after ${elapsed}s)"
                return 0
            fi
        fi

        sleep "${INTERVAL}"
    done
}

# 执行
main() {
    parse_args "$@"

    if wait_for; then
        if [[ -n "${CMD}" ]]; then
            log "Executing command: ${CMD}"
            eval "${CMD}"
        fi
        exit 0
    else
        exit 1
    fi
}

main "$@"