#!/usr/bin/env bash
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
AgentOS Docker Image Build Script
# Usage: ./build.sh [kernel|service|all] [dev|release]

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 配置变量
AGENTOS_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
AGENTOS_SCRIPTS_DIR="$(dirname "${AGENTOS_SCRIPT_DIR}")"
AGENTOS_PROJECT_ROOT="$(dirname "${AGENTOS_SCRIPTS_DIR}")"
VERSION="0.1.0"
DOCKER_REGISTRY="${DOCKER_REGISTRY:-spharx}"
KERNEL_IMAGE="${DOCKER_REGISTRY}/agentos-kernel"
SERVICE_IMAGE="${DOCKER_REGISTRY}/agentos-services"

# Print usage information
print_usage() {

    cat << EOF
Usage: $0 [options] [target]

目标:
    kernel      构建内核镜像 (默认)
    service     构建服务镜像
    all         Build all images

构建类型:
    dev         Development (with debug tools)
    release     Production (optimized size)

示例:
    $0 kernel dev       # 构建开发版内核镜像
    $0 service release  # Build production service image
    $0 all              # Build all images (default release)

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

# Check if Docker is installed
check_docker() {
    if ! command -v docker &> /dev/null; then
        log_error "Docker not installed, please install Docker first"
        exit 1
    fi

    if ! docker info &> /dev/null; then
        log_error "Docker not running, please start Docker service"
        exit 1
    fi

    log_info "Docker version: $(docker --version)"
}

# 构建内核镜像
build_kernel() {
    local build_type="${1:-release}"

    log_info "Building AgentOS kernel image (${build_type})..."

    cd "${AGENTOS_PROJECT_ROOT}"

    if [ "${build_type}" = "dev" ]; then
        # Development version: with all debug tools
        docker build \
            -f "${AGENTOS_SCRIPT_DIR}/Dockerfile.kernel" \
            -t ${KERNEL_IMAGE}:${VERSION}-dev \
            -t ${KERNEL_IMAGE}:dev \
            --target builder \
            --build-arg BUILD_TYPE=development \
            .
    else
        # 生产版本：多阶段构建，最小化镜像
        docker build \
            -f "${AGENTOS_SCRIPT_DIR}/Dockerfile.kernel" \
            -t ${KERNEL_IMAGE}:${VERSION} \
            -t ${KERNEL_IMAGE}:latest \
            --target runtime \
            --build-arg BUILD_TYPE=release \
            .
    fi

Kernel image build complete
    docker images | grep agentos-kernel
}

# 构建服务镜像
build_service() {
    local build_type="${1:-release}"

    log_info "开始构�?AgentOS 服务镜像 (${build_type})..."

    cd "${AGENTOS_PROJECT_ROOT}"

# Ensure kernel image exists
    if ! docker image inspect ${KERNEL_IMAGE}:${VERSION} &> /dev/null; then
Kernel image not found, building first
        build_kernel "release"
    fi

    if [ "${build_type}" = "dev" ]; then
# Development version
        docker build \
            -f "${AGENTOS_SCRIPT_DIR}/Dockerfile.service" \
            -t ${SERVICE_IMAGE}:${VERSION}-dev \
            -t ${SERVICE_IMAGE}:dev \
            --target dev \
            --build-arg BUILD_TYPE=development \
            .
    else
        # 生产版本
        docker build \
            -f "${AGENTOS_SCRIPT_DIR}/Dockerfile.service" \
            -t ${SERVICE_IMAGE}:${VERSION} \
            -t ${SERVICE_IMAGE}:latest \
            .
    fi

Service image build complete
    docker images | grep agentos-services
}

# Build all images
build_all() {
    local build_type="${1:-release}"

    log_info "开始构建所�?AgentOS 镜像 (${build_type})..."

    build_kernel "${build_type}"
    build_service "${build_type}"

All images build complete

    # 显示镜像列表
    echo ""
Current AgentOS images:
    docker images | grep agentos
}

# Clean up old images
cleanup_images() {
Cleaning up old AgentOS images...

    # 删除悬空镜像
    docker image prune -f

Clean up complete
}

# Main function
main() {
    local target="${1:-kernel}"
    local build_type="${2:-release}"

    echo ""
    log_info "========================================"
    log_info "AgentOS Docker 镜像构建脚本"
    log_info "========================================"
    echo ""

    # 检�?Docker
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
            log_error "未知目标�?{target}"
            print_usage
            exit 1
            ;;
    esac

    echo ""
    log_success "构建完成�?"
    echo ""

    # 提示
    if [ "${build_type}" = "release" ]; then
        log_info "提示：使�?docker-compose 启动服务:"
        log_info "  cd ${AGENTOS_SCRIPT_DIR} && docker-compose up -d"
    fi
}

# 执行主函�?
main "$@"
