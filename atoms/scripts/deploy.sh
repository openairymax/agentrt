#!/bin/bash
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS Atoms Module Deployment Script
# Version: 1.0.0
#
# 本脚本遵循《工程控制论》的反馈闭环原则
# 提供安全的部署和回滚机制

set -e
set -u
set -o pipefail

# ==================== 全局变量 ====================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ATOMS_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
VERSION="1.0.0.5"
DEPLOY_ENV="${DEPLOY_ENV:-staging}"
DEPLOY_DIR="${DEPLOY_DIR:-/opt/agentos}"
BACKUP_DIR="${BACKUP_DIR:-/opt/agentos/backups}"
LOG_DIR="${LOG_DIR:-/var/log/agentos}"
MAX_BACKUPS="${MAX_BACKUPS:-10}"
DRY_RUN="${DRY_RUN:-0}"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# ==================== 辅助函数 ====================

log_info() {
    echo -e "${BLUE}[INFO]${NC} $(date '+%Y-%m-%d %H:%M:%S') $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $(date '+%Y-%m-%d %H:%M:%S') $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $(date '+%Y-%m-%d %H:%M:%S') $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $(date '+%Y-%m-%d %H:%M:%S') $1" >&2
}

die() {
    log_error "$1"
    exit 1
}

check_root() {
    if [[ $EUID -ne 0 ]]; then
        die "This script must be run as root"
    fi
}

detect_os() {
    case "$(uname -s)" in
        Linux*)     echo "linux";;
        Darwin*)    echo "macos";;
        *)          echo "unknown";;
    esac
}

detect_arch() {
    case "$(uname -m)" in
        x86_64|amd64)   echo "x86_64";;
        arm64|aarch64)  echo "arm64";;
        *)              echo "unknown";;
    esac
}

# ==================== 备份函数 ====================

create_backup() {
    local backup_name="atoms_${VERSION}_$(date '+%Y%m%d_%H%M%S')"
    local backup_path="${BACKUP_DIR}/${backup_name}"
    
    log_info "Creating backup: ${backup_name}"
    
    mkdir -p "${BACKUP_DIR}"
    
    if [[ -d "${DEPLOY_DIR}/atoms" ]]; then
        if [[ "${DRY_RUN}" == "1" ]]; then
            log_info "[DRY-RUN] Would backup ${DEPLOY_DIR}/atoms to ${backup_path}"
        else
            cp -r "${DEPLOY_DIR}/atoms" "${backup_path}"
            log_success "Backup created: ${backup_path}"
        fi
    else
        log_warning "No existing deployment found, skipping backup"
    fi
    
    # 清理旧备份
    cleanup_old_backups
    
    echo "${backup_path}"
}

cleanup_old_backups() {
    log_info "Cleaning up old backups (keeping last ${MAX_BACKUPS})"
    
    local backup_count=$(ls -1 "${BACKUP_DIR}" 2>/dev/null | wc -l)
    
    if [[ ${backup_count} -gt ${MAX_BACKUPS} ]]; then
        local delete_count=$((backup_count - MAX_BACKUPS))
        
        ls -1t "${BACKUP_DIR}" | tail -n ${delete_count} | while read backup; do
            if [[ "${DRY_RUN}" == "1" ]]; then
                log_info "[DRY-RUN] Would delete backup: ${backup}"
            else
                rm -rf "${BACKUP_DIR}/${backup}"
                log_info "Deleted old backup: ${backup}"
            fi
        done
    fi
}

# ==================== 部署前检查 ====================

pre_deploy_check() {
    log_info "Running pre-deployment checks..."
    
    # 检查磁盘空间
    local available_space=$(df -BG "${DEPLOY_DIR}" | awk 'NR==2 {print $4}' | tr -d 'G')
    if [[ ${available_space} -lt 100 ]]; then
        die "Insufficient disk space: ${available_space}GB available, need at least 100GB"
    fi
    
    # 检查依赖
    local required_commands=("cmake" "ninja" "gcc")
    for cmd in "${required_commands[@]}"; do
        if ! command -v "${cmd}" &> /dev/null; then
            die "Required command '${cmd}' not found"
        fi
    done
    
    # 检查端口占用（如果需要）
    # local ports=(8080 9090)
    # for port in "${ports[@]}"; do
    #     if lsof -i :${port} &> /dev/null; then
    #         die "Port ${port} is already in use"
    #     fi
    # done
    
    log_success "Pre-deployment checks passed"
}

# ==================== 部署函数 ====================

deploy() {
    local package_path="$1"
    local backup_path=""
    
    log_info "Starting deployment to ${DEPLOY_ENV} environment"
    log_info "Package: ${package_path}"
    
    # 预检查
    pre_deploy_check
    
    # 创建备份
    backup_path=$(create_backup)
    
    # 停止服务（如果需要）
    stop_services
    
    # 部署新版本
    if [[ "${DRY_RUN}" == "1" ]]; then
        log_info "[DRY-RUN] Would deploy ${package_path} to ${DEPLOY_DIR}"
    else
        mkdir -p "${DEPLOY_DIR}/atoms"
        
        # 解压包
        tar -xzf "${package_path}" -C "${DEPLOY_DIR}/atoms"
        
        log_success "Deployment complete"
    fi
    
    # 启动服务
    start_services
    
    # 健康检查
    health_check
    
    log_success "Deployment to ${DEPLOY_ENV} completed successfully"
}

# ==================== 回滚函数 ====================

rollback() {
    local backup_path="$1"
    
    if [[ ! -d "${backup_path}" ]]; then
        die "Backup not found: ${backup_path}"
    fi
    
    log_info "Starting rollback to: ${backup_path}"
    
    # 停止服务
    stop_services
    
    # 回滚
    if [[ "${DRY_RUN}" == "1" ]]; then
        log_info "[DRY-RUN] Would rollback to ${backup_path}"
    else
        rm -rf "${DEPLOY_DIR}/atoms"
        cp -r "${backup_path}" "${DEPLOY_DIR}/atoms"
        
        log_success "Rollback complete"
    fi
    
    # 启动服务
    start_services
    
    # 健康检查
    health_check
    
    log_success "Rollback completed successfully"
}

# ==================== 服务管理 ====================

stop_services() {
    log_info "Stopping services..."
    
    local services=("agentos-core" "agentos-memory" "agentos-syscall")
    
    for service in "${services[@]}"; do
        if systemctl is-active --quiet "${service}" 2>/dev/null; then
            if [[ "${DRY_RUN}" == "1" ]]; then
                log_info "[DRY-RUN] Would stop service: ${service}"
            else
                systemctl stop "${service}"
                log_info "Stopped service: ${service}"
            fi
        fi
    done
}

start_services() {
    log_info "Starting services..."
    
    local services=("agentos-syscall" "agentos-memory" "agentos-core")
    
    for service in "${services[@]}"; do
        if [[ -f "/etc/systemd/system/${service}.service" ]]; then
            if [[ "${DRY_RUN}" == "1" ]]; then
                log_info "[DRY-RUN] Would start service: ${service}"
            else
                systemctl start "${service}"
                log_info "Started service: ${service}"
            fi
        fi
    done
}

# ==================== 健康检查 ====================

health_check() {
    log_info "Running health checks..."
    
    local max_retries=10
    local retry_interval=5
    local retry=0
    
    while [[ ${retry} -lt ${max_retries} ]]; do
        # 检查服务状态
        local all_healthy=true
        
        # 检查进程
        if pgrep -f "agentos-core" > /dev/null; then
            log_info "Core process is running"
        else
            log_warning "Core process is not running"
            all_healthy=false
        fi
        
        # 检查端口（示例）
        # if lsof -i :8080 > /dev/null 2>&1; then
        #     log_info "Port 8080 is listening"
        # else
        #     log_warning "Port 8080 is not listening"
        #     all_healthy=false
        # fi
        
        if [[ "${all_healthy}" == "true" ]]; then
            log_success "Health check passed"
            return 0
        fi
        
        retry=$((retry + 1))
        log_warning "Health check failed, retry ${retry}/${max_retries}"
        sleep ${retry_interval}
    done
    
    log_error "Health check failed after ${max_retries} retries"
    return 1
}

# ==================== 版本管理 ====================

get_current_version() {
    if [[ -f "${DEPLOY_DIR}/atoms/VERSION" ]]; then
        cat "${DEPLOY_DIR}/atoms/VERSION"
    else
        echo "unknown"
    fi
}

list_versions() {
    log_info "Available versions:"
    
    echo "Current version: $(get_current_version)"
    echo ""
    echo "Available backups:"
    
    ls -1t "${BACKUP_DIR}" 2>/dev/null | while read backup; do
        echo "  - ${backup}"
    done
}

# ==================== 日志管理 ====================

setup_logging() {
    mkdir -p "${LOG_DIR}"
    
    local log_file="${LOG_DIR}/deploy_$(date '+%Y%m%d_%H%M%S').log"
    
    # 重定向输出到日志文件
    exec > >(tee -a "${log_file}") 2>&1
    
    log_info "Logging to: ${log_file}"
}

# ==================== 帮助信息 ====================

show_help() {
    cat << EOF
AgentOS Atoms Module Deployment Script
Version: ${VERSION}

Usage: $0 [command] [options]

Commands:
    deploy <package>      Deploy a package
    rollback <backup>     Rollback to a backup
    backup                Create a backup of current deployment
    list                  List available versions and backups
    health-check          Run health checks
    help                  Show this help message

Options:
    DEPLOY_ENV=env        Set deployment environment (default: staging)
    DEPLOY_DIR=/path      Set deployment directory (default: /opt/agentos)
    BACKUP_DIR=/path      Set backup directory (default: /opt/agentos/backups)
    MAX_BACKUPS=N         Set maximum number of backups to keep (default: 10)
    DRY_RUN=1             Run in dry-run mode (no actual changes)

Examples:
    $0 deploy atoms-corekern-1.0.0.5-linux-x86_64.tar.gz
    $0 rollback /opt/agentos/backups/atoms_1.0.0.4_20260324_120000
    $0 backup
    $0 list
    DRY_RUN=1 $0 deploy atoms-corekern-1.0.0.5-linux-x86_64.tar.gz

Environment Variables:
    DEPLOY_ENV            Deployment environment (staging/production)
    DEPLOY_DIR            Target deployment directory
    BACKUP_DIR            Backup storage directory
    LOG_DIR               Log directory
    MAX_BACKUPS           Maximum backups to retain
    DRY_RUN               Enable dry-run mode

EOF
}

# ==================== 入口点 ====================

main() {
    local command="${1:-help}"
    shift || true
    
    # 设置日志
    setup_logging
    
    log_info "=== AgentOS Atoms Deployment ==="
    log_info "Environment: ${DEPLOY_ENV}"
    log_info "OS: $(detect_os), Arch: $(detect_arch)"
    
    case "${command}" in
        deploy)
            [[ $# -lt 1 ]] && die "Usage: $0 deploy <package>"
            deploy "$1"
            ;;
        rollback)
            [[ $# -lt 1 ]] && die "Usage: $0 rollback <backup>"
            rollback "$1"
            ;;
        backup)
            create_backup
            ;;
        list)
            list_versions
            ;;
        health-check)
            health_check
            ;;
        help|--help|-h)
            show_help
            ;;
        *)
            die "Unknown command: ${command}. Use 'help' for usage information."
            ;;
    esac
}

main "$@"
