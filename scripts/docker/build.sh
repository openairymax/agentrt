#!/bin/bash
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved. "From data intelligence emerges".
# AgentOS Docker 镜像构建脚本
# 用法：./build.sh [kernel|service|all] [dev|release]

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 配置变量
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "${SCRIPT_DIR}")")"
VERSION="1.0.0.5"

# 打印帮助信息
print_usage() {
    cat << EOF
用法：$0 [选项] [目标]

目标:
    kernel      构建内核镜像 (默认)
    service     构建服务镜像
    all         构建所有镜像

构建类型:
    dev         开发版本 (包含调试工具)
    release     生产版本 (优化大小)

示例:
    $0 kernel dev       # 构建开发版内核镜像
    $0 service release  # 构建生产版服务镜像
    $0 all              # 构建所有镜像 (默认 release)

EOF
}

# 日志函数
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 检查 Docker 是否安装
check_docker() {
    if ! command -v docker &> /dev/null; then
        log_error "Docker 未安装，请先安装 Docker"
        exit 1
    fi
    
    if ! docker info &> /dev/null; then
        log_error "Docker 未运行，请启动 Docker 服务"
        exit 1
    fi
    
    log_info "Docker 版本：$(docker --version)"
}

# 构建内核镜像
build_kernel() {
    local build_type="${1:-release}"
    
    log_info "开始构建 AgentOS 内核镜像 (${build_type})..."
    
    cd "${PROJECT_ROOT}"
    
    if [ "${build_type}" == "dev" ]; then
        # 开发版本：包含所有调试工具
        docker build \
            -f "${SCRIPT_DIR}/Dockerfile.kernel" \
            -t spharx/agentos-kernel:${VERSION}-dev \
            -t spharx/agentos-kernel:dev \
            --target builder \
            --build-arg BUILD_TYPE=development \
            .
    else
        # 生产版本：多阶段构建，最小化镜像
        docker build \
            -f "${SCRIPT_DIR}/Dockerfile.kernel" \
            -t spharx/agentos-kernel:${VERSION} \
            -t spharx/agentos-kernel:latest \
            --target runtime \
            --build-arg BUILD_TYPE=release \
            .
    fi
    
    log_success "内核镜像构建完成"
    docker images | grep agentos-kernel
}

# 构建服务镜像
build_service() {
    local build_type="${1:-release}"
    
    log_info "开始构建 AgentOS 服务镜像 (${build_type})..."
    
    cd "${PROJECT_ROOT}"
    
    # 确保内核镜像已存在
    if ! docker image inspect spharx/agentos-kernel:${VERSION} &> /dev/null; then
        log_warning "内核镜像不存在，先构建内核镜像"
        build_kernel "release"
    fi
    
    if [ "${build_type}" == "dev" ]; then
        # 开发版本
        docker build \
            -f "${SCRIPT_DIR}/Dockerfile.service" \
            -t spharx/agentos-services:${VERSION}-dev \
            -t spharx/agentos-services:dev \
            --target dev \
            --build-arg BUILD_TYPE=development \
            .
    else
        # 生产版本
        docker build \
            -f "${SCRIPT_DIR}/Dockerfile.service" \
            -t spharx/agentos-services:${VERSION} \
            -t spharx/agentos-services:latest \
            .
    fi
    
    log_success "服务镜像构建完成"
    docker images | grep agentos-services
}

# 构建所有镜像
build_all() {
    local build_type="${1:-release}"
    
    log_info "开始构建所有 AgentOS 镜像 (${build_type})..."
    
    build_kernel "${build_type}"
    build_service "${build_type}"
    
    log_success "所有镜像构建完成"
    
    # 显示镜像列表
    echo ""
    log_info "当前 AgentOS 镜像:"
    docker images | grep agentos
}

# 清理旧镜像
cleanup_images() {
    log_info "清理旧的 AgentOS 镜像..."
    
    # 删除悬空镜像
    docker image prune -f
    
    log_success "清理完成"
}

# 主函数
main() {
    local target="${1:-kernel}"
    local build_type="${2:-release}"
    
    echo ""
    log_info "========================================"
    log_info "AgentOS Docker 镜像构建脚本"
    log_info "========================================"
    echo ""
    
    # 检查 Docker
    check_docker
    echo ""
    
    # 解析参数
    case "${target}" in
        kernel)
            build_kernel "${build_type}"
            ;;
        service)
            build_service "${build_type}"
            ;;
        all)
            build_all "${build_type}"
            ;;
        help|-h|--help)
            print_usage
            exit 0
            ;;
        *)
            log_error "未知目标：${target}"
            print_usage
            exit 1
            ;;
    esac
    
    echo ""
    log_success "构建完成！"
    echo ""
    
    # 提示
    if [ "${build_type}" == "release" ]; then
        log_info "提示：使用 docker-compose 启动服务:"
        log_info "  cd ${SCRIPT_DIR} && docker-compose up -d"
    fi
}

# 执行主函数
main "$@"
