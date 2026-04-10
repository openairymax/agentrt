#!/usr/bin/env bash
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS 制品打包与部署脚本
# 功能：构建产物归档、Docker镜像构建、发布包生成
# Version: 2.0.0

set -euo pipefail

###############################################################################
# 路径定义
###############################################################################
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

###############################################################################
# 错误处理
###############################################################################
cleanup() {
    local exit_code=$?
    if [[ $exit_code -ne 0 ]]; then
        log_error "Deploy script failed with exit code $exit_code"
    fi
}
trap cleanup EXIT

###############################################################################
# 颜色和日志
###############################################################################
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
BLUE='\033[0;34m'; NC='\033[0m'

log_info()  { echo -e "${BLUE}[DEPLOY]${NC} $*"; }
log_ok()    { echo -e "${GREEN}[DEPLOY-OK]${NC} $*"; }
log_warn()  { echo -e "${YELLOW}[DEPLOY-WARN]${NC} $*"; }
log_error() { echo -e "${RED}[DEPLOY-ERR]${NC} $*" >&2; }

###############################################################################
# 配置
###############################################################################
OUTPUT_DIR="${ARTIFACT_OUTPUT_DIR:-${PROJECT_ROOT}/ci-artifacts}"
VERSION="${VERSION_OVERRIDE:-}"
BUILD_NUMBER="${GITHUB_RUN_NUMBER:-$(date +%Y%m%d%H%M)}"
DOCKER_REGISTRY="${DOCKER_REGISTRY:-ghcr.io/spharx}"
DOCKER_PUSH="${DOCKER_PUSH:-false}"
PACKAGE_TYPE="${PACKAGE_TYPE:-tar.gz}"

# 版本信息
extract_version() {
    if [[ -n "$VERSION" ]]; then
        echo "$VERSION"
        return
    fi

    # 从 pyproject.toml 提取
    if [[ -f "${PROJECT_ROOT}/pyproject.toml" ]]; then
        local ver
        ver=$(grep '^version' "${PROJECT_ROOT}/pyproject.toml" | head -1 | sed 's/.*version.*= *"\([^"]*\)".*/\1/')
        if [[ -n "$ver" ]]; then
            echo "${ver}.${BUILD_NUMBER}"
            return
        fi
    fi

    echo "1.0.${BUILD_NUMBER}"
}

###############################################################################
# 参数解析
###############################################################################
parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --output|-o)   OUTPUT_DIR="$2"; shift ;;
            --version|-v)  VERSION="$2"; shift ;;
            --docker-push) DOCKER_PUSH="true" ;;
            --package-type) PACKAGE_TYPE="$2"; shift ;;
            --registry)     DOCKER_REGISTRY="$2"; shift ;;
            --help|-h)      show_help; exit 0 ;;
            *) log_warn "Unknown option: $1" ;;
        esac
        shift
    done
}

show_help() {
    cat << 'EOF'
AgentOS Artifact Deployment Script v2.0.0

Usage: ./deploy-artifacts.sh [OPTIONS]

Options:
    -o, --output DIR      Output directory (default: ci-artifacts)
    -v, --version VER      Override version string
    --docker-push          Push Docker images to registry
    --package-type TYPE    Package type: tar.gz|zip|deb|rpm (default: tar.gz)
    --registry REG         Docker registry URL
    -h, --help             Show this help

Outputs:
    - agentos-{version}.tar.gz  Source archive
    - Docker images (optional)
    - ci-report.json           Build metadata
EOF
}

###############################################################################
# 准备输出目录
###############################################################################
prepare_output() {
    mkdir -p "$OUTPUT_DIR"
    log_info "Output directory: $OUTPUT_DIR"
}

###############################################################################
# 1. 源码打包
###############################################################################
package_source_archive() {
    local version
    version=$(extract_version)
    local archive_name="agentos-${version}.tar.gz"
    local archive_path="${OUTPUT_DIR}/${archive_name}"

    log_info "Creating source archive: $archive_name"

    if ! tar czf "$archive_path" \
        --exclude='.git' \
        --exclude='build-*' \
        --exclude='*.o' \
        --exclude='*.a' \
        --exclude='*.so' \
        --exclude='*.dylib' \
        --exclude='*.exe' \
        --exclude='__pycache__' \
        --exclude='*.pyc' \
        --exclude='node_modules' \
        --exclude='.gitcode' \
        --exclude='.gitee' \
        --exclude='.github' \
        --exclude='vcpkg' \
        --exclude='ci-artifacts' \
        --exclude='ci-logs' \
        --exclude='.venv' \
        --exclude='venv' \
        --exclude='.env' \
        --exclude='*.egg-info' \
        --exclude='dist' \
        --exclude='.bendiwenjian' \
        -C "$(dirname "$PROJECT_ROOT")" \
        "$(basename "$PROJECT_ROOT")" 2>/dev/null; then
        log_warn "Source archive creation encountered errors (some files may be missing)"
    fi

    if [[ -f "$archive_path" ]]; then
        local size
        size=$(du -h "$archive_path" | cut -f1)
        log_ok "Source archive created: $archive_name ($size)"
    else
        log_warn "Source archive creation skipped"
    fi

    echo "$archive_path"
}

###############################################################################
# 2. 二进制产物收集
###############################################################################
collect_binaries() {
    local version
    version=$(extract_version)
    local bin_dir="${OUTPUT_DIR}/binaries-${version}"

    log_info "Collecting built binaries..."

    mkdir -p "$bin_dir"

    local found=0
    for build_dir in "${PROJECT_ROOT}"/build-*; do
        if [[ ! -d "$build_dir" ]]; then
            continue
        fi

        local module_name
        module_name=$(basename "$build_dir" | sed 's/build-//')

        # 收集可执行文件
        while IFS= read -r -d '' bin_file; do
            if [[ -x "$bin_file" ]] && [[ -f "$bin_file" ]]; then
                cp "$bin_file" "${bin_dir}/${module_name}-$(basename "$bin_file")" 2>/dev/null || true
                ((found++))
            fi
        done < <(find "$build_dir" -type f \( -executable -o -name "*.dll" -o -name "*.exe" \) \
            ! -path "*/tests/*" ! -name "ctest" -print0 2>/dev/null | head -z -20)

        # 收集库文件
        while IFS= read -r -d '' lib_file; do
            cp "$lib_file" "${bin_dir}/" 2>/dev/null || true
        done < <(find "$build_dir" \( -name "*.so" -o -name "*.dylib" -o -name "*.a" \) \
            -print0 2>/dev/null | head -z -20)
    done

    if [[ $found -gt 0 ]]; then
        local size
        size=$(du -sh "$bin_dir" | cut -f1)
        log_ok "Collected $found binary artifacts ($size)"
    else
        log_warn "No binary artifacts found (may need to build first)"
    fi
}

###############################################################################
# 3. Docker 镜像构建
###############################################################################
build_docker_images() {
    local docker_dir="${PROJECT_ROOT}/scripts/deploy/docker"

    if [[ ! -d "$docker_dir" ]]; then
        log_warn "Docker directory not found: $docker_dir"
        return 0
    fi

    if ! command -v docker &>/dev/null; then
        log_warn "Docker not available, skipping image build"
        return 0
    fi

    local version
    version=$(extract_version)
    local image_base="${DOCKER_REGISTRY}/agentos"

    log_info "Building Docker images..."

    # Kernel 镜像
    if [[ -f "${docker_dir}/Dockerfile.kernel" ]]; then
        local kernel_image="${image_base}:kernel-${version}"
        log_info "Building kernel image: $kernel_image"

        if docker build \
            -f "${docker_dir}/Dockerfile.kernel" \
            -t "$kernel_image" \
            -t "${image_base}:kernel-latest" \
            "${docker_dir}" 2>&1 | tail -5; then
            log_ok "Kernel image built: $kernel_image"

            if [[ "$DOCKER_PUSH" == "true" ]]; then
                docker push "$kernel_image" 2>/dev/null || log_warn "Push failed for kernel image"
                docker push "${image_base}:kernel-latest" 2>/dev/null || true
            fi
        else
            log_warn "Kernel image build failed"
        fi
    fi

    # Service 镜像
    if [[ -f "${docker_dir}/Dockerfile.service" ]]; then
        local service_image="${image_base}:service-${version}"
        log_info "Building service image: $service_image"

        if docker build \
            -f "${docker_dir}/Dockerfile.service" \
            -t "$service_image" \
            -t "${image_base}:service-latest" \
            "${docker_dir}" 2>&1 | tail -5; then
            log_ok "Service image built: $service_image"

            if [[ "$DOCKER_PUSH" == "true" ]]; then
                docker push "$service_image" 2>/dev/null || log_warn "Push failed for service image"
                docker push "${image_base}:service-latest" 2>/dev/null || true
            fi
        else
            log_warn "Service image build failed"
        fi
    fi
}

###############################################################################
# 4. 生成部署元数据
###############################################################################
generate_metadata() {
    local version
    version=$(extract_version)
    local meta_file="${OUTPUT_DIR}/deployment-metadata.json"

    log_info "Generating deployment metadata..."

    local total_size
    total_size=$(du -sh "$OUTPUT_DIR" 2>/dev/null | cut -f1 || echo "unknown")

    local artifact_list
    artifact_list=$(find "$OUTPUT_DIR" -maxdepth 1 -type f -name "*.tar.gz" -o -name "*.zip" 2>/dev/null \
        | xargs -I{} basename "{}" 2>/dev/null | tr '\n' ',' | sed 's/,$//')

    cat > "$meta_file" << EOF
{
    "timestamp": "$(date -Iseconds)",
    "project": "AgentOS",
    "version": "${version}",
    "build_number": "${BUILD_NUMBER}",
    "artifacts": {
        "output_dir": "${OUTPUT_DIR}",
        "total_size": "${total_size}",
        "archives": ["${artifact_list}"]
    },
    "docker": {
        "registry": "${DOCKER_REGISTRY}",
        "images": [
            "${DOCKER_REGISTRY}/agentos:kernel-${version}",
            "${DOCKER_REGISTRY}/agentos:service-${version}"
        ]
    },
    "environment": {
        "platform": "$(uname -s) $(uname -m)",
        "hostname": "$(hostname 2>/dev/null || echo unknown)",
        "user": "$(whoami 2>/dev/null || echo unknown)"
    }
}
EOF

    log_ok "Metadata saved to: $meta_file"
}

###############################################################################
# 主流程
###############################################################################
main() {
    parse_args "$@"

    log_info "AgentOS Artifact Deployer v2.0.0"
    log_info "Timestamp: $(date '+%Y-%m-%d %H:%M:%S')"
    local version
    version=$(extract_version)
    log_info "Version: $version"

    prepare_output
    package_source_archive
    collect_binaries
    build_docker_images
    generate_metadata

    log_info ""
    log_info "==========================================="
    log_info "Deployment Summary"
    log_info "==========================================="
    log_info "Output: $OUTPUT_DIR"

    local file_count
    file_count=$(find "$OUTPUT_DIR" -maxdepth 2 -type f 2>/dev/null | wc -l)
    log_info "Artifacts: $file_count file(s)"

    ls -lh "$OUTPUT_DIR" 2>/dev/null | tail -n +2 || true

    log_ok "Deployment preparation complete!"
}

main "$@"
