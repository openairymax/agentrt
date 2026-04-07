#!/bin/bash
# =============================================================================
# AgentOS 一键部署脚本 (V1.0)
# =============================================================================
#
# 功能特性:
#   ✅ 自动检测系统环境（Docker/Python/编译器）
#   ✅ 交互式配置向导
#   ✅ 一键启动开发/生产环境
#   ✅ 健康检查和状态验证
#   ✅ 日志收集和问题诊断
#   ✅ 优雅停机和清理
#
# 使用方法:
#   # 开发环境部署
#   ./deploy.sh --mode dev
#
#   # 生产环境部署
#   ./deploy.sh --mode prod
#
#   # 仅检查环境
#   ./deploy.sh --check-only
#
#   # 查看帮助
#   ./deploy.sh --help
#
# 环境要求:
#   - Docker >= 20.10 (生产) / Docker Desktop (开发)
#   - Python >= 3.8 (可选，用于SDK)
#   - Git >= 2.30
#   - 内存 >= 4GB RAM
#   - 磁盘空间 >= 20GB
#
# 作者: AgentOS DevOps Team
# 版本: 1.0.0 (2026-04-07)
# =============================================================================

set -euo pipefail

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 全局变量
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
DOCKER_COMPOSE_FILE="${PROJECT_ROOT}/docker/docker-compose.yml"
DOCKER_COMPOSE_PROD="${PROJECT_ROOT}/docker/docker-compose.prod.yml"
ENV_FILE=".env.production"
LOG_DIR="${PROJECT_ROOT}/logs"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# =============================================================================
# 辅助函数
# =============================================================================

print_banner() {
    echo -e "${BLUE}"
    echo "╔══════════════════════════════════════════════════════════╗"
    echo "║                                                              ║"
    echo "║           🚀 AgentOS 一键部署工具 V1.0                      ║"
    echo "║                                                              ║"
    echo "║  'From data intelligence emerges.'                          ║"
    echo "╚══════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
}

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_step() {
    echo -e "\n${BLUE}▶ $1${NC}"
}

confirm() {
    read -r -p "$1 [y/N]: " response
    case "$response" in
        [yY][eE][sS]|[yY]) 
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

check_command() {
    if command -v "$1" &> /dev/null; then
        return 0
    else
        return 1
    fi
}

# =============================================================================
# 环境检测
# =============================================================================

check_system_requirements() {
    log_step "检查系统环境..."
    
    local issues=0
    
    # 检查操作系统
    log_info "操作系统: $(uname -s) $(uname -r)"
    
    # 检查内存
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        TOTAL_MEM=$(free -g | awk '/^Mem:/{print $2}')
        if [[ "$TOTAL_MEM" -lt 4 ]]; then
            log_error "内存不足: 需要 ≥4GB, 当前: ${TOTAL_MEM}GB"
            ((issues++))
        else
            log_info "内存: ${TOTAL_MEM}GB ✅"
        fi
        
        AVAILABLE_DISK=$(df -BG "${PROJECT_ROOT}" | awk 'NR==2{print $4}')
        if [[ "$AVAILABLE_DISK" -lt 20 ]]; then
            log_error "磁盘空间不足: 需要 ≥20GB, 当前: ${AVAILABLE_DISK}GB"
            ((issues++))
        else
            log_info "磁盘空间: ${AVAILABLE_DISK}GB ✅"
        fi
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        log_info "macOS 系统 - 跳过详细资源检查"
    elif [[ "$OSTYPE" == "msys"* ]] || [[ "$OSTYPE" == "cygwin"* ]]; then
        log_info "Windows 系统 - 使用 WSL2 或 Docker Desktop"
    fi
    
    # 检查 Docker
    if check_command docker; then
        DOCKER_VERSION=$(docker --version | awk '{print $3}' | sed 's/,//')
        log_info "Docker 版本: ${DOCKER_VERSION} ✅"
        
        # 检查 Docker 是否运行
        if docker info &> /dev/null; then
            log_info "Docker 服务: 运行中 ✅"
        else
            log_error "Docker 服务未运行，请先启动 Docker"
            ((issues++))
        fi
    else
        log_error "未安装 Docker，请访问 https://docs.docker.com/get-docker/"
        ((issues++))
    fi
    
    # 检查 Docker Compose
    if check_command docker-compose || docker compose version &> /dev/null; then
        COMPOSE_VERSION=$(docker compose version 2>/dev/null || docker-compose --version)
        log_info "Docker Compose: ${COMPOSE_VERSION} ✅"
    else
        log_error "未安装 Docker Compose"
        ((issues++))
    fi
    
    # 检查 Git
    if check_command git; then
        GIT_VERSION=$(git --version)
        log_info "Git: ${GIT_VERSION} ✅"
    else
        log_warn "未安装 Git（非必需但推荐）"
    fi
    
    # 检查 Python（可选）
    if check_command python3; then
        PYTHON_VERSION=$(python3 --version)
        log_info "Python: ${PYTHON_VERSION} ✅"
    else
        log_info "Python 未安装（SDK 功能需要）"
    fi
    
    return $issues
}

check_project_structure() {
    log_step "检查项目结构..."
    
    local required_files=(
        "docker/Dockerfile.kernel"
        "docker/Dockerfile.daemon"
        "docker/docker-compose.yml"
        "docker/docker-compose.prod.yml"
        ".env.example"
    )
    
    for file in "${required_files[@]}"; do
        if [[ -f "${PROJECT_ROOT}/${file}" ]]; then
            log_info "找到: ${file} ✅"
        else
            log_error "缺失: ${file}"
            return 1
        fi
    done
    
    return 0
}

# =============================================================================
# 配置管理
# =============================================================================

setup_environment() {
    log_step "配置环境变量..."
    
    # 复制环境变量模板
    if [[ ! -f "${PROJECT_ROOT}/${ENV_FILE}" ]]; then
        if [[ -f "${PROJECT_ROOT}/.env.production.example" ]]; then
            cp "${PROJECT_ROOT}/.env.production.example" "${PROJECT_ROOT}/${ENV_FILE}"
            log_info "已创建 ${ENV_FILE}（从模板）"
            
            warn_config
        else
            log_error "找不到 .env.production.example 模板文件"
            return 1
        fi
    else
        log_info "${ENV_FILE} 已存在"
    fi
    
    # 创建日志目录
    mkdir -p "${LOG_DIR}"
    log_info "日志目录: ${LOG_DIR} ✅"
    
    return 0
}

warn_config() {
    echo ""
    echo -e "${YELLOW}⚠️  重要提示:${NC}"
    echo "  请编辑 ${ENV_FILE} 文件，配置以下敏感信息:"
    echo "  - JWT_SECRET_KEY: JWT 密钥"
    echo "  - DATABASE_PASSWORD: 数据库密码"
    echo "  - REDIS_PASSWORD: Redis 密码"
    echo "  - API_KEYS: 外部 API 密钥"
    echo ""
    echo "  示例命令: nano ${ENV_FILE}"
    echo ""
}

# =============================================================================
# 部署功能
# =============================================================================

start_dev_environment() {
    log_step "启动开发环境..."
    
    cd "${PROJECT_ROOT}/docker"
    
    log_info "使用配置: docker-compose.yml"
    
    # 启动基础服务
    docker-compose up -d postgres redis gateway kernel openlab
    
    if [[ $? -eq 0 ]]; then
        echo ""
        log_info "✅ 开发环境启动成功！"
        echo ""
        echo "服务访问地址:"
        echo "  🌐 Gateway API: http://localhost:18789"
        echo "  🧪 OpenLab UI: http://localhost:3000"
        echo "  📊 Grafana 监控: http://localhost:3001"
        echo ""
        echo "常用命令:"
        echo "  查看日志: docker-compose logs -f"
        echo "  停止服务: docker-compose down"
        echo "  重启服务: docker-compose restart"
        echo ""
        
        run_health_check
    else
        log_error "开发环境启动失败！"
        return 1
    fi
}

start_prod_environment() {
    log_step "启动生产环境..."
    
    cd "${PROJECT_ROOT}/docker"
    
    # 验证环境变量文件
    if [[ ! -f "../${ENV_FILE}" ]]; then
        log_error "缺少生产环境配置文件: ${ENV_FILE}"
        return 1
    fi
    
    log_info "使用配置: docker-compose.prod.yml"
    log_info "环境变量: ../${ENV_FILE}"
    
    # 启动生产服务
    docker-compose --env-file "../${ENV_FILE}" -f docker-compose.prod.yml up -d
    
    if [[ $? -eq 0 ]]; then
        echo ""
        log_info "✅ 生产环境启动成功！"
        echo ""
        echo "生产环境信息:"
        echo "  📡 Gateway: :18789 (HTTPS 通过反向代理)"
        echo "  🐳 容器数: $(docker ps --format '{{.Names}}' | grep agentos | wc -l)"
        echo ""
        echo "运维命令:"
        echo "  查看状态: docker-compose -f docker-compose.prod.yml ps"
        echo "  查看日志: docker-compose -f docker-compose.prod.yml logs -f"
        echo "  扩展实例: docker-compose -f docker-compose.prod.yml up -d --scale kernel=3"
        echo "  优雅停止: docker-compose -f docker-compose.prod.yml down --timeout 60"
        echo ""
        
        run_health_check
    else
        log_error "生产环境启动失败！"
        return 1
    fi
}

stop_environment() {
    log_step "停止所有服务..."
    
    cd "${PROJECT_ROOT}/docker"
    
    # 停止开发环境
    if docker-compose ps &> /dev/null; then
        docker-compose down
        log_info "开发环境已停止 ✅"
    fi
    
    # 停止生产环境
    if docker-compose -f docker-compose.prod.yml ps &> /dev/null; then
        docker-compose -f docker-compose.prod.yml down
        log_info "生产环境已停止 ✅"
    fi
    
    echo ""
    log_info "所有服务已停止"
}

run_health_check() {
    log_step "执行健康检查..."
    
    sleep 5  # 等待服务启动
    
    local services=("gateway:18789" "postgres:5432" "redis:6379")
    local all_healthy=true
    
    for service in "${services[@]}"; do
        IFS=':' read -r name port <<< "$service"
        
        if curl -sf "http://localhost:${port}/api/v1/health" &> /dev/null || \
           nc -z localhost "${port}" &> /dev/null; then
            log_info "${name}: 健康 ✅"
        else
            log_warn "${name}: 未响应 ⚠️"
            all_healthy=false
        fi
    done
    
    if $all_healthy; then
        echo ""
        log_info "🎉 所有服务健康检查通过！"
        return 0
    else
        echo ""
        log_warn "部分服务未就绪，请查看日志排查问题"
        return 1
    fi
}

show_status() {
    log_step "显示当前状态..."
    
    cd "${PROJECT_ROOT}/docker"
    
    echo ""
    echo "容器状态:"
    docker-compose ps 2>/dev/null || true
    docker-compose -f docker-compose.prod.yml ps 2>/dev/null || true
    
    echo ""
    echo "资源使用:"
    docker stats --no-stream --format "table {{.Name}}\t{{.CPUPerc}}\t{{.MemUsage}}\t{{.NetIO}}" 2>/dev/null || \
        log_info "Docker 统计信息不可用"
    
    echo ""
    echo "最近日志 (最后20行):"
    docker-compose logs --tail=20 2>/dev/null | head -20 || true
}

collect_logs() {
    log_step "收集日志..."
    
    local log_file="${LOG_DIR}/agentos_${TIMESTAMP}.log"
    
    mkdir -p "${LOG_DIR}"
    
    cd "${PROJECT_ROOT}/docker"
    
    # 收集所有服务日志
    {
        echo "=== AgentOS Deployment Log ==="
        echo "Timestamp: $(date)"
        echo "================================"
        echo ""
        
        echo "--- Container Logs ---"
        docker-compose logs --timestamps 2>/dev/null || true
        docker-compose -f docker-compose.prod.yml logs --timestamps 2>/dev/null || true
        
        echo ""
        echo "--- System Info ---"
        uname -a
        docker --version
        docker-compose --version
        echo ""
        
        echo "--- Resource Usage ---"
        docker system df
        echo ""
        
    } > "${log_file}" 2>&1
    
    log_info "日志已保存到: ${log_file}"
    
    # 同时输出到屏幕
    tail -50 "${log_file}"
}

cleanup() {
    log_step "清理资源..."
    
    if confirm "确定要清理所有 Docker 资源吗？这将删除容器、镜像和卷"; then
        cd "${PROJECT_ROOT}/docker"
        
        # 停止并删除容器
        docker-compose down -v --remove-orphans 2>/dev/null || true
        docker-compose -f docker-compose.prod.yml down -v --remove-orphans 2>/dev/null || true
        
        # 清理悬空镜像
        docker image prune -f
        
        # 清理构建缓存
        docker builder prune -f
        
        log_info "清理完成 ✅"
    else
        log_info "取消清理操作"
    fi
}

show_help() {
    cat << EOF
AgentOS 一键部署工具 V1.0

用法: $0 [选项]

选项:
  --mode MODE      部署模式: dev|prod (默认: dev)
  --check-only     仅检查环境和项目结构
  --status         显示当前运行状态
  --stop           停止所有服务
  --logs           收集并显示日志
  --cleanup        清理 Docker 资源
  --help           显示此帮助信息

示例:
  $0                          # 启动开发环境
  $0 --mode prod              # 启动生产环境
  $0 --check-only             # 仅检查环境
  $0 --status                 # 查看状态
  $0 --stop                   # 停止服务
  $0 --logs                   # 收集日志
  $0 --cleanup                # 清理资源

更多信息:
  项目文档: docs/
  Docker 配置: docker/
  问题反馈: https://github.com/spharx/agentos/issues
EOF
}

# =============================================================================
# 主程序
# =============================================================================

main() {
    print_banner
    
    # 解析参数
    local mode="dev"
    local action="deploy"
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            --mode)
                mode="$2"
                shift 2
                ;;
            --check-only)
                action="check"
                shift
                ;;
            --status)
                action="status"
                shift
                ;;
            --stop)
                action="stop"
                shift
                ;;
            --logs)
                action="logs"
                shift
                ;;
            --cleanup)
                action="cleanup"
                shift
                ;;
            --help|-h)
                show_help
                exit 0
                ;;
            *)
                log_error "未知选项: $1"
                show_help
                exit 1
                ;;
        esac
    done
    
    # 切换到项目根目录
    cd "${PROJECT_ROOT}" || {
        log_error "无法进入项目目录: ${PROJECT_ROOT}"
        exit 1
    }
    
    # 执行相应操作
    case $action in
        check)
            log_info "模式: 环境检查"
            
            local issues=0
            check_system_requirements || issues=$?
            check_project_structure || ((issues++))
            
            echo ""
            if [[ $issues -eq 0 ]]; then
                log_info "✅ 所有检查通过，可以开始部署！"
                exit 0
            else
                log_error "❌ 发现 ${issues} 个问题，请修复后重试"
                exit 1
            fi
            ;;
            
        deploy)
            log_info "模式: ${mode} 部署"
            
            # 环境检查
            check_system_requirements
            check_project_structure
            
            # 配置环境
            setup_environment
            
            # 根据模式启动
            case $mode in
                dev)
                    start_dev_environment
                    ;;
                prod)
                    start_prod_environment
                    ;;
                *)
                    log_error "未知模式: ${mode} (支持: dev, prod)"
                    exit 1
                    ;;
            esac
            ;;
            
        status)
            show_status
            ;;
            
        stop)
            stop_environment
            ;;
            
        logs)
            collect_logs
            ;;
            
        cleanup)
            cleanup
            ;;
    esac
}

# 运行主程序
main "$@"