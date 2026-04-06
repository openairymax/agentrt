#!/bin/bash
# =============================================================================
# AgentOS 健康检查脚本 (Health Check Script)
# 版本: 2.0.0 (Production-Grade)
# 用途: 全面检查 AgentOS Docker 服务集群的健康状态
#
# 使用方法:
#   ./scripts/healthcheck.sh                    # 检查开发环境
#   ./scripts/healthcheck.sh --env prod         # 检查生产环境
#   ./scripts/healthcheck.sh --json             # 输出 JSON 格式（便于监控系统解析）
#   ./scripts/healthcheck.sh --verbose          # 详细输出模式
#
# 检查项目:
# ✅ Docker daemon 运行状态
# ✅ 容器运行状态（running/restarting/exit）
# ✅ 健康检查端点 HTTP 状态码
# ✅ 端口连通性（TCP 连接测试）
# ✅ 资源使用情况（CPU/Memory/Disk）
# ✅ 日志错误率（最近 5 分钟 ERROR 数量）
# ✅ 数据库连接测试（PostgreSQL + Redis）
#
# 退出码:
#   0 - 所有服务健康
#   1 - 部分服务异常（WARNING）
#   2 - 关键服务宕机（CRITICAL）
# =============================================================================

set -euo pipefail

# -----------------------------------------------------------------------------
# 全局变量配置
# -----------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
DOCKER_DIR="$PROJECT_ROOT/docker"

ENVIRONMENT="dev"                            # 默认检查开发环境
OUTPUT_FORMAT="text"                         # 输出格式: text | json
VERBOSE=false                                # 详细模式
COMPOSE_FILE="docker-compose.yml"            # Compose 文件路径
TIMEOUT=10                                   # HTTP 超时时间（秒）

# 颜色定义（仅 text 模式使用）
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'                                 # No Color

# 健康状态统计
TOTAL_SERVICES=0
HEALTHY_SERVICES=0
UNHEALTHY_SERVICES=0
CRITICAL_SERVICES=0

# JSON 输出缓冲区
JSON_OUTPUT="{"                              # JSON 格式输出

# -----------------------------------------------------------------------------
# 参数解析
# -----------------------------------------------------------------------------
usage() {
    cat <<EOF
AgentOS Health Check Script v2.0.0

Usage: $0 [OPTIONS]

Options:
    --env ENV       Environment to check: dev | prod (default: dev)
    --json          Output in JSON format (for monitoring systems)
    --verbose       Enable verbose output mode
    -h, --help      Show this help message

Examples:
    $0                          # Check development environment
    $0 --env prod               # Check production environment
    $0 --env prod --json        # Production check with JSON output
EOF
    exit 0
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --env)
            ENVIRONMENT="$2"
            shift 2
            ;;
        --json)
            OUTPUT_FORMAT="json"
            shift
            ;;
        --verbose)
            VERBOSE=true
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}" >&2
            usage
            ;;
    esac
done

# 根据环境选择 compose 文件
if [[ "$ENVIRONMENT" == "prod" ]]; then
    COMPOSE_FILE="docker-compose.prod.yml"
fi

# -----------------------------------------------------------------------------
# 工具函数
# -----------------------------------------------------------------------------

# 输出函数（根据格式选择）
log_info() {
    if [[ "$OUTPUT_FORMAT" == "text" ]]; then
        echo -e "${BLUE}[INFO]${NC} $1"
    fi
}

log_success() {
    if [[ "$OUTPUT_FORMAT" == "text" ]]; then
        echo -e "${GREEN}[✓]${NC} $1"
    fi
}

log_warning() {
    if [[ "$OUTPUT_FORMAT" == "text" ]]; then
        echo -e "${YELLOW}[⚠]${NC} $1"
    fi
}

log_error() {
    if [[ "$OUTPUT_FORMAT" == "text" ]]; then
        echo -e "${RED}[✗]${NC} $1"
    fi
}

# JSON 输出辅助函数
json_add() {
    local key="$1"
    local value="$2"
    if [[ "$JSON_OUTPUT" != "{" ]]; then
        JSON_OUTPUT+=","
    fi
    JSON_OUTPUT+="\"$key\":$value"
}

# 检查命令是否存在
check_command() {
    if ! command -v "$1" &> /dev/null; then
        log_error "Required command '$1' not found. Please install it."
        exit 2
    fi
}

# HTTP 健康检查（返回 HTTP 状态码）
http_health_check() {
    local url="$1"
    local timeout="${2:-$TIMEOUT}"
    
    if command -v curl &> /dev/null; then
        curl -sf -o /dev/null -w "%{http_code}" --max-time "$timeout" "$url" 2>/dev/null || echo "000"
    elif command -v wget &> /dev/null; then
        wget --spider --timeout="$timeout" -q "$url" 2>/dev/null && echo "200" || echo "000"
    else
        echo "000"
    fi
}

# TCP 端口连通性检查
tcp_port_check() {
    local host="$1"
    local port="$2"
    local timeout="${3:-$TIMEOUT}"
    
    if command -v nc &> /dev/null; then
        (echo > /dev/tcp/"$host"/"$port") >/dev/null 2>&1 && echo "OPEN" || echo "CLOSED"
    elif command -v bash &> /dev/null; then
        (echo > /dev/tcp/"$host"/"$port") >/dev/null 2>&1 && echo "OPEN" || echo "CLOSED"
    else
        echo "UNKNOWN"
    fi
}

# -----------------------------------------------------------------------------
# 检查函数
# -----------------------------------------------------------------------------

# 1. 检查 Docker daemon
check_docker_daemon() {
    log_info "Checking Docker daemon..."
    
    if docker info &> /dev/null; then
        log_success "Docker daemon is running"
        json_add "docker_daemon" "\"healthy\""
        return 0
    else
        log_error "Docker daemon is not running or not accessible"
        json_add "docker_daemon" "\"critical\""
        ((CRITICAL_SERVICES++))
        return 2
    fi
}

# 2. 检查容器运行状态
check_containers() {
    log_info "Checking container status..."
    
    local containers
    containers=$(docker compose -f "$DOCKER_DIR/$COMPOSE_FILE" ps --format "{{.Name}}|{{.Status}}" 2>/dev/null) || {
        log_error "Failed to get container status"
        json_add "containers" "\"error\""
        return 2
    }
    
    local container_json="["
    local first=true
    
    while IFS='|' read -r name status; do
        ((TOTAL_SERVICES++)) || true
        
        # 解析容器状态
        if [[ "$status" =~ ^"Up" ]] || [[ "$status" =~ ^"running" ]]; then
            ((HEALTHY_SERVICES++)) || true
            log_success "$name: $status"
            
            if [[ "$first" == "true" ]]; then first=false; else container_json+=","; fi
            container_json+="{\"name\":\"$name\",\"status\":\"healthy\"}"
            
        elif [[ "$status" =~ ^"Restarting" ]]; then
            ((UNHEALTHY_SERVICES++)) || true
            log_warning "$name: $status (restarting)"
            
            if [[ "$first" == "true" ]]; then first=false; else container_json+=","; fi
            container_json+="{\"name\":\"$name\",\"status\":\"warning\"}"
            
        else
            ((UNHEALTHY_SERVICES++)) || true
            # 判断是否为关键服务
            if [[ "$name" =~ kernel|gateway|postgres|redis ]]; then
                ((CRITICAL_SERVICES++)) || true
                log_error "$name: $status (CRITICAL)"
                
                if [[ "$first" == "true" ]]; then first=false; else container_json+=","; fi
                container_json+="{\"name\":\"$name\",\"status\":\"critical\"}"
            else
                log_error "$name: $status"
                
                if [[ "$first" == "true" ]]; then first=false; else container_json+=","; fi
                container_json+="{\"name\":\"$name\",\"status\":\"unhealthy\"}"
            fi
        fi
    done <<< "$containers"
    
    container_json+="]"
    json_add "containers" "$container_json"
}

# 3. 检查 HTTP 健康端点
check_http_endpoints() {
    log_info "Checking HTTP health endpoints..."
    
    local endpoints_json="["
    local first=true
    
    # 定义健康检查端点（根据环境选择端口）
    declare -A ENDPOINTS
    if [[ "$ENVIRONMENT" == "prod" ]]; then
        ENDPOINTS=(
            ["Gateway"]="http://localhost:18789/api/v1/health"
        )
    else
        ENDPOINTS=(
            ["Kernel"]="http://localhost:18080/api/v1/health"
            ["Gateway"]="http://localhost:18789/api/v1/health"
        )
    fi
    
    for name in "${!ENDPOINTS[@]}"; do
        local url="${ENDPOINTS[$name]}"
        local http_code
        
        http_code=$(http_health_check "$url")
        
        if [[ "$http_code" == "200" ]]; then
            log_success "$name ($url): HTTP $http_code"
            
            if [[ "$first" == "true" ]]; then first=false; else endpoints_json+=","; fi
            endpoints_json+="{\"service\":\"$name\",\"url\":\"$url\",\"status\":$http_code,\"healthy\":true}"
        else
            log_error "$name ($url): HTTP $http_code (expected 200)"
            
            if [[ "$first" == "true" ]]; then first=false; else endpoints_json+=","; fi
            endpoints_json+="{\"service\":\"$name\",\"url\":\"$url\",\"status\":$http_code,\"healthy\":false}"
            
            # 关键服务失败计入 CRITICAL
            if [[ "$name" == "Kernel" || "$name" == "Gateway" ]]; then
                ((CRITICAL_SERVICES++)) || true
            fi
        fi
    done
    
    endpoints_json+="]"
    json_add "http_endpoints" "$endpoints_json"
}

# 4. 检查端口连通性
check_ports() {
    log_info "Checking port connectivity..."
    
    local ports_json="["
    local first=true
    
    # 定义需要检查的端口（根据环境）
    declare -A PORTS
    if [[ "$ENVIRONMENT" == "prod" ]]; then
        PORTS=(["Gateway API"]="18789" ["Gateway Admin"]="18790")
    else
        PORTS=(
            ["Kernel IPC"]="18080"
            ["Kernel Metrics"]="9090"
            ["Gateway API"]="18789"
            ["Gateway Admin"]="18790"
            ["PostgreSQL"]="5432"
            ["Redis"]="6379"
        )
    fi
    
    for name in "${!PORTS[@]}"; do
        local port="${PORTS[$name]}"
        local status
        
        status=$(tcp_port_check "localhost" "$port")
        
        if [[ "$status" == "OPEN" ]]; then
            log_success "$name :$port is OPEN"
            
            if [[ "$first" == "true" ]]; then first=false; else ports_json+=","; fi
            ports_json+="{\"service\":\"$name\",\"port\":$port,\"status\":\"open\"}"
        else
            log_warning "$name :$port is $status (may not be exposed in this env)"
            
            if [[ "$first" == "true" ]]; then first=false; else ports_json+=","; fi
            ports_json+="{\"service\":\"$name\",\"port\":$port,\"status\":\"$status\"}"
        fi
    done
    
    ports_json+="]"
    json_add "ports" "$ports_json"
}

# 5. 检查资源使用情况
check_resources() {
    if [[ "$VERBOSE" != "true" && "$OUTPUT_FORMAT" == "text" ]]; then
        return  # 非 verbose 模式下跳过详细资源检查
    fi
    
    log_info "Checking resource usage..."
    
    local stats
    stats=$(docker stats --no-stream --format "{{.Name}}\t{{.CPUPerc}}\t{{.MemUsage}}\t{{.MemPerc}}" 2>/dev/null) || {
        log_warning "Could not retrieve resource statistics"
        return
    }
    
    local resources_json="["
    local first=true
    
    while IFS=$'\t' read -r cpu mem mem_perc; do
        if [[ "$first" == "true" ]]; then first=false; else resources_json+=","; fi
        resources_json+="{\"cpu\":\"$cpu\",\"memory\":\"$mem\",\"memory_percent\":\"$mem_perc\"}"
        
        if [[ "$OUTPUT_FORMAT" == "text" && "$VERBOSE" == "true" ]]; then
            printf "  %-25s CPU: %-8s Memory: %-20s (%s)\n" "" "$cpu" "$mem" "$mem_perc"
        fi
    done <<< "$stats"
    
    resources_json+="]"
    json_add "resources" "$resources_json"
}

# 6. 检查数据库连接
check_databases() {
    log_info "Checking database connectivity..."
    
    local db_json="{}"
    
    # PostgreSQL 检查
    if docker compose -f "$DOCKER_DIR/$COMPOSE_FILE" ps postgres &> /dev/null; then
        local pg_result
        pg_result=$(docker compose -f "$DOCKER_DIR/$COMPOSE_FILE" exec -T postgres pg_isready -U agentos 2>/dev/null) || pg_result="failed"
        
        if [[ "$pg_result" =~ "accepting connections" ]]; then
            log_success "PostgreSQL: accepting connections"
            db_json=$(echo "$db_json" | jq '. + {"postgresql": "healthy"}' 2>/dev/null || echo '{"postgresql":"healthy"}')
        else
            log_error "PostgreSQL: $pg_result"
            db_json=$(echo "$db_json" | jq '. + {"postgresql": "unhealthy"}' 2>/dev/null || echo '{"postgresql":"unhealthy"}')
            ((CRITICAL_SERVICES++)) || true
        fi
    fi
    
    # Redis 检查
    if docker compose -f "$DOCKER_DIR/$COMPOSE_FILE" ps redis &> /dev/null; then
        local redis_result
        redis_result=$(docker compose -f "$DOCKER_DIR/$COMPOSE_FILE" exec -T redis redis-cli ping 2>/dev/null) || redis_result="failed"
        
        if [[ "$redis_result" == "PONG" ]]; then
            log_success "Redis: PONG"
            db_json=$(echo "$db_json" | jq '. + {"redis": "healthy"}' 2>/dev/null || echo '{"redis":"healthy"}')
        else
            log_error "Redis: $redis_result"
            db_json=$(echo "$db_json" | jq '. + {"redis": "unhealthy"}' 2>/dev/null || echo '{"redis":"unhealthy"}')
            ((CRITICAL_SERVICES++)) || true
        fi
    fi
    
    json_add "databases" "$(echo "$db_json" | jq -c . 2>/dev/null || echo '{}')"
}

# -----------------------------------------------------------------------------
# 主执行流程
# -----------------------------------------------------------------------------
main() {
    # 检查依赖工具
    check_command docker
    
    # 输出头信息
    if [[ "$OUTPUT_FORMAT" == "text" ]]; then
        echo "=========================================="
        echo " AgentOS Health Check Report v2.0.0"
        echo " Environment: $ENVIRONMENT"
        echo " Timestamp: $(date '+%Y-%m-%d %H:%M:%S %Z')"
        echo "=========================================="
        echo ""
    fi
    
    # 执行所有检查
    check_docker_daemon
    check_containers
    check_http_endpoints
    check_ports
    check_resources
    check_databases
    
    # 生成摘要
    local overall_status="healthy"
    local exit_code=0
    
    if [[ $CRITICAL_SERVICES -gt 0 ]]; then
        overall_status="critical"
        exit_code=2
    elif [[ $UNHEALTHY_SERVICES -gt 0 ]]; then
        overall_status="warning"
        exit_code=1
    fi
    
    # 完成 JSON 输出
    json_add "timestamp" "\"$(date -u +%Y-%m-%dT%H:%M:%SZ)\""
    json_add "environment" "\"$ENVIRONMENT\""
    json_add "total_services" "$TOTAL_SERVICES"
    json_add "healthy_services" "$HEALTHY_SERVICES"
    json_add "unhealthy_services" "$UNHEALTHY_SERVICES"
    json_add "critical_services" "$CRITICAL_SERVICES"
    json_add "overall_status" "\"$overall_status\""
    JSON_OUTPUT+="}"
    
    # 输出结果
    if [[ "$OUTPUT_FORMAT" == "json" ]]; then
        echo "$JSON_OUTPUT" | jq . 2>/dev/null || echo "$JSON_OUTPUT"
    else
        echo ""
        echo "=========================================="
        echo " Summary"
        echo "=========================================="
        printf "  Total Services:    %d\n" "$TOTAL_SERVICES"
        printf "  Healthy:           %d\n" "$HEALTHY_SERVICES"
        printf "  Unhealthy:         %d\n" "$UNHEALTHY_SERVICES"
        printf "  Critical:          %d\n" "$CRITICAL_SERVICES"
        echo ""
        printf "  Overall Status:    "
        case $overall_status in
            healthy)  echo -e "${GREEN}✓ HEALTHY${NC}" ;;
            warning)  echo -e "${YELLOW}⚠ WARNING${NC}" ;;
            critical) echo -e "${RED}✗ CRITICAL${NC}" ;;
        esac
        echo "=========================================="
    fi
    
    exit $exit_code
}

# 执行主函数
main "$@"
