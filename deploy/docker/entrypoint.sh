#!/usr/bin/env bash
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# entrypoint.sh — AgentRT Multi-Daemon Entrypoint
# P1.17: 通过 AGENTOS_SERVICE_NAME 环境变量选择启动对应的 daemon
#
# 支持的 daemon:
#   corekern, coreloopthree, taskflow, memory,
#   channel_d, monit_d, observe_d,
#   llm_d, tool_d, market_d, sched_d,
#   hook_d, plugin_d, info_d, notify_d,
#   gateway_d

set -euo pipefail

SERVICE_NAME="${AGENTOS_SERVICE_NAME:-gateway_d}"
SERVICE_PORT="${AGENTOS_SERVICE_PORT:-8080}"
CONFIG_PATH="${AGENTOS_CONFIG_PATH:-/etc/agentrt/agentos.yaml}"

echo "=== AgentRT Daemon Starting ==="
echo "  Service:    ${SERVICE_NAME}"
echo "  Port:       ${SERVICE_PORT}"
echo "  Config:     ${CONFIG_PATH}"
echo "  Log Level:  ${AGENTOS_LOG_LEVEL:-INFO}"
echo "  Test Mode:  ${AGENTOS_TEST_MODE:-0}"
echo "================================"

# 根据服务名选择二进制文件
# 二进制文件命名约定: agentos-<daemon_name>
# 例如: agentos-corekern, agentos-llm_d, agentos-gateway_d
BINARY=""

case "${SERVICE_NAME}" in
    corekern)
        BINARY="agentos-corekern"
        ;;
    coreloopthree)
        BINARY="agentos-coreloopthree"
        ;;
    taskflow)
        BINARY="agentos-taskflow"
        ;;
    memory)
        BINARY="agentos-memory"
        ;;
    channel_d)
        BINARY="agentos-channel_d"
        ;;
    monit_d)
        BINARY="agentos-monit_d"
        ;;
    observe_d)
        BINARY="agentos-observe_d"
        ;;
    llm_d)
        BINARY="agentos-llm_d"
        ;;
    tool_d)
        BINARY="agentos-tool_d"
        ;;
    market_d)
        BINARY="agentos-market_d"
        ;;
    sched_d)
        BINARY="agentos-sched_d"
        ;;
    hook_d)
        BINARY="agentos-hook_d"
        ;;
    plugin_d)
        BINARY="agentos-plugin_d"
        ;;
    info_d)
        BINARY="agentos-info_d"
        ;;
    notify_d)
        BINARY="agentos-notify_d"
        ;;
    gateway_d)
        BINARY="agentos-gateway_d"
        ;;
    *)
        echo "ERROR: Unknown service name: ${SERVICE_NAME}" >&2
        echo "Supported: corekern, coreloopthree, taskflow, memory, channel_d, monit_d, observe_d, llm_d, tool_d, market_d, sched_d, hook_d, plugin_d, info_d, notify_d, gateway_d" >&2
        exit 1
        ;;
esac

# 查找二进制文件路径
BINARY_PATH=""
for candidate in \
    "/usr/bin/${BINARY}" \
    "/usr/local/bin/${BINARY}" \
    "/opt/agentrt/bin/${BINARY}" \
    "/usr/sbin/${BINARY}"; do
    if [[ -x "${candidate}" ]]; then
        BINARY_PATH="${candidate}"
        break
    fi
done

if [[ -z "${BINARY_PATH}" ]]; then
    echo "ERROR: Binary not found: ${BINARY}" >&2
    echo "Searched paths: /usr/bin, /usr/local/bin, /opt/agentrt/bin, /usr/sbin" >&2
    exit 1
fi

echo "Launching: ${BINARY_PATH}"

# 设置默认参数
DAEMON_ARGS=(
    "--port" "${SERVICE_PORT}"
    "--config" "${CONFIG_PATH}"
)

# 服务特定参数
case "${SERVICE_NAME}" in
    monit_d)
        if [[ -n "${AGENTOS_METRICS_PORT:-}" ]]; then
            DAEMON_ARGS+=("--metrics-port" "${AGENTOS_METRICS_PORT}")
        fi
        ;;
    gateway_d)
        if [[ -n "${AGENTOS_HTTP_PORT:-}" ]]; then
            DAEMON_ARGS+=("--http-port" "${AGENTOS_HTTP_PORT}")
        fi
        if [[ -n "${AGENTOS_WS_PORT:-}" ]]; then
            DAEMON_ARGS+=("--ws-port" "${AGENTOS_WS_PORT}")
        fi
        if [[ -n "${AGENTOS_METRICS_PORT:-}" ]]; then
            DAEMON_ARGS+=("--metrics-port" "${AGENTOS_METRICS_PORT}")
        fi
        ;;
esac

# 启动 daemon
exec "${BINARY_PATH}" "${DAEMON_ARGS[@]}"