#!/usr/bin/env bash
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# test-integration.sh — AgentRT 集成测试环境启动与验证脚本
# P1.17.3: 编写启动验证脚本
#
# 用法:
#   bash scripts/ci/pipeline/test-integration.sh [--up] [--down] [--verify] [--logs]
#
# 选项:
#   --up       启动所有测试服务
#   --down     停止并清理所有测试服务
#   --verify   验证所有服务健康状态
#   --logs     查看所有服务日志
#   --all      完整流程: up → verify → down

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
COMPOSE_FILE="${PROJECT_ROOT}/deploy/docker/docker-compose.test.yml"

# 颜色
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
BLUE='\033[0;34m'; NC='\033[0m'

log_info()  { echo -e "${BLUE}[INTEG]${NC}   $*"; }
log_ok()    { echo -e "${GREEN}[INTEG-OK]${NC}  $*"; }
log_warn()  { echo -e "${YELLOW}[INTEG-WARN]${NC} $*"; }
log_error() { echo -e "${RED}[INTEG-ERR]${NC}  $*" >&2; }

###############################################################################
# 服务健康检查
###############################################################################
verify_service() {
    local name="$1"
    local host="$2"
    local port="$3"
    local max_retries="${4:-30}"
    local retry_interval="${5:-2}"

    log_info "Verifying ${name} (${host}:${port})..."

    for i in $(seq 1 "${max_retries}"); do
        if wget -qO- --timeout=3 "http://${host}:${port}/healthz" 2>/dev/null | grep -qE "ok|healthy|200"; then
            log_ok "${name}: healthy (attempt ${i})"
            return 0
        fi
        # 备选检查：HTTP 200 状态码
        if curl -s -o /dev/null -w "%{http_code}" --max-time 3 "http://${host}:${port}/healthz" 2>/dev/null | grep -q "200"; then
            log_ok "${name}: healthy (HTTP 200, attempt ${i})"
            return 0
        fi
        sleep "${retry_interval}"
    done

    log_error "${name}: UNHEALTHY after ${max_retries} attempts"
    return 1
}

###############################################################################
# 启动所有服务
###############################################################################
do_up() {
    log_info "Starting AgentRT integration test environment..."
    log_info "Compose file: ${COMPOSE_FILE}"

    cd "${PROJECT_ROOT}"

    # 清理旧容器
    docker compose -f "${COMPOSE_FILE}" down -v --remove-orphans 2>/dev/null || true

    # 启动服务（按依赖顺序）
    log_info "Starting infrastructure services..."
    docker compose -f "${COMPOSE_FILE}" up -d redis postgres

    log_info "Starting core atom services..."
    docker compose -f "${COMPOSE_FILE}" up -d corekern coreloopthree taskflow memory

    log_info "Starting base daemon services..."
    docker compose -f "${COMPOSE_FILE}" up -d channel_d monit_d observe_d

    log_info "Starting business daemon services..."
    docker compose -f "${COMPOSE_FILE}" up -d llm_d tool_d market_d sched_d

    log_info "Starting extension daemon services..."
    docker compose -f "${COMPOSE_FILE}" up -d hook_d plugin_d info_d notify_d

    log_info "Starting gateway service..."
    docker compose -f "${COMPOSE_FILE}" up -d gateway_d

    log_info "All services started. Running health verification..."

    # 健康检查
    local failures=0

    verify_service "redis"      "localhost" "${TEST_REDIS_PORT:-16379}" 10 2 || ((failures++))
    verify_service "postgres"   "localhost" "${TEST_POSTGRES_PORT:-15432}" 10 2 || ((failures++))
    verify_service "corekern"   "localhost" "${TEST_COREKERN_PORT:-19001}" 20 2 || ((failures++))
    verify_service "coreloopthree" "localhost" "${TEST_CL3_PORT:-19002}" 20 2 || ((failures++))
    verify_service "taskflow"   "localhost" "${TEST_TASKFLOW_PORT:-19003}" 15 2 || ((failures++))
    verify_service "memory"     "localhost" "${TEST_MEMORY_PORT:-19004}" 15 2 || ((failures++))
    verify_service "channel_d"  "localhost" "${TEST_CHANNEL_PORT:-19101}" 15 2 || ((failures++))
    verify_service "monit_d"    "localhost" "${TEST_MONIT_PORT:-19102}" 15 2 || ((failures++))
    verify_service "observe_d"  "localhost" "${TEST_OBSERVE_PORT:-19103}" 15 2 || ((failures++))
    verify_service "llm_d"      "localhost" "${TEST_LLM_PORT:-19201}" 20 2 || ((failures++))
    verify_service "tool_d"     "localhost" "${TEST_TOOL_PORT:-19202}" 15 2 || ((failures++))
    verify_service "market_d"   "localhost" "${TEST_MARKET_PORT:-19203}" 15 2 || ((failures++))
    verify_service "sched_d"    "localhost" "${TEST_SCHED_PORT:-19204}" 15 2 || ((failures++))
    verify_service "hook_d"     "localhost" "${TEST_HOOK_PORT:-19301}" 15 2 || ((failures++))
    verify_service "plugin_d"   "localhost" "${TEST_PLUGIN_PORT:-19302}" 15 2 || ((failures++))
    verify_service "info_d"     "localhost" "${TEST_INFO_PORT:-19303}" 15 2 || ((failures++))
    verify_service "notify_d"   "localhost" "${TEST_NOTIFY_PORT:-19304}" 15 2 || ((failures++))
    verify_service "gateway_d"  "localhost" "${TEST_GATEWAY_HTTP_PORT:-8080}" 30 2 || ((failures++))

    echo ""
    if [[ "${failures}" -eq 0 ]]; then
        log_ok "All 18 services are healthy!"
        echo ""
        echo "=== Quick Access ==="
        echo "  Gateway HTTP:    http://localhost:${TEST_GATEWAY_HTTP_PORT:-8080}"
        echo "  Gateway Metrics: http://localhost:${TEST_GATEWAY_METRICS_PORT:-9090}/metrics"
        echo "  Monit Metrics:   http://localhost:${TEST_MONIT_METRICS_PORT:-19902}/metrics"
        echo "  Prometheus:      http://localhost:${TEST_PROMETHEUS_PORT:-19090}"
        echo ""
        echo "=== Useful Commands ==="
        echo "  View all logs:       docker compose -f ${COMPOSE_FILE} logs -f"
        echo "  View service status: docker compose -f ${COMPOSE_FILE} ps"
        echo "  Stop all services:   bash $0 --down"
        return 0
    else
        log_error "${failures} service(s) failed health check!"
        log_info "Check logs: docker compose -f ${COMPOSE_FILE} logs"
        return 1
    fi
}

###############################################################################
# 停止所有服务
###############################################################################
do_down() {
    log_info "Stopping AgentRT integration test environment..."
    cd "${PROJECT_ROOT}"
    docker compose -f "${COMPOSE_FILE}" down -v --remove-orphans 2>/dev/null || true
    log_ok "All services stopped and cleaned up."
}

###############################################################################
# 验证已运行服务的健康状态
###############################################################################
do_verify() {
    log_info "Verifying running services..."

    local failures=0

    # 检查各服务健康状态
    declare -A SERVICES=(
        ["redis"]="${TEST_REDIS_PORT:-16379}"
        ["corekern"]="${TEST_COREKERN_PORT:-19001}"
        ["coreloopthree"]="${TEST_CL3_PORT:-19002}"
        ["taskflow"]="${TEST_TASKFLOW_PORT:-19003}"
        ["memory"]="${TEST_MEMORY_PORT:-19004}"
        ["channel_d"]="${TEST_CHANNEL_PORT:-19101}"
        ["monit_d"]="${TEST_MONIT_PORT:-19102}"
        ["observe_d"]="${TEST_OBSERVE_PORT:-19103}"
        ["llm_d"]="${TEST_LLM_PORT:-19201}"
        ["tool_d"]="${TEST_TOOL_PORT:-19202}"
        ["market_d"]="${TEST_MARKET_PORT:-19203}"
        ["sched_d"]="${TEST_SCHED_PORT:-19204}"
        ["hook_d"]="${TEST_HOOK_PORT:-19301}"
        ["plugin_d"]="${TEST_PLUGIN_PORT:-19302}"
        ["info_d"]="${TEST_INFO_PORT:-19303}"
        ["notify_d"]="${TEST_NOTIFY_PORT:-19304}"
        ["gateway_d"]="${TEST_GATEWAY_HTTP_PORT:-8080}"
    )

    for svc in "${!SERVICES[@]}"; do
        port="${SERVICES[$svc]}"
        if curl -s -o /dev/null -w "%{http_code}" --max-time 3 "http://localhost:${port}/healthz" 2>/dev/null | grep -q "200"; then
            log_ok "${svc}: healthy"
        else
            log_error "${svc}: unhealthy"
            ((failures++)) || true
        fi
    done

    echo ""
    if [[ "${failures}" -eq 0 ]]; then
        log_ok "All services verified healthy!"
    else
        log_error "${failures} service(s) unhealthy!"
    fi
    return "${failures}"
}

###############################################################################
# 查看日志
###############################################################################
do_logs() {
    cd "${PROJECT_ROOT}"
    docker compose -f "${COMPOSE_FILE}" logs -f --tail=100
}

###############################################################################
# 显示服务状态
###############################################################################
do_status() {
    cd "${PROJECT_ROOT}"
    echo "=== AgentRT Integration Test Environment Status ==="
    docker compose -f "${COMPOSE_FILE}" ps
}

###############################################################################
# 主入口
###############################################################################
ACTION=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --up)    ACTION="up"; shift ;;
        --down)  ACTION="down"; shift ;;
        --verify) ACTION="verify"; shift ;;
        --logs)  ACTION="logs"; shift ;;
        --status) ACTION="status"; shift ;;
        --all)
            do_up && do_verify && do_down
            exit $?
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --up       Start all test services"
            echo "  --down     Stop and clean up all test services"
            echo "  --verify   Verify health of running services"
            echo "  --logs     View all service logs"
            echo "  --status   Show service status"
            echo "  --all      Full cycle: up → verify → down"
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            exit 1
            ;;
    esac
done

if [[ -z "${ACTION}" ]]; then
    log_info "No action specified. Showing status."
    do_status
    exit 0
fi

case "${ACTION}" in
    up)     do_up ;;
    down)   do_down ;;
    verify) do_verify ;;
    logs)   do_logs ;;
    status) do_status ;;
esac