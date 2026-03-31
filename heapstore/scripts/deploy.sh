#!/bin/bash
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# heapstore Module Deployment Script
# "From data intelligence emerges."

set -e

# Configuration
MODULE_NAME="heapstore"
VERSION="${VERSION:-1.0.0.6}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
DIST_DIR="${PROJECT_ROOT}/dist"
DEPLOY_DIR="${DEPLOY_DIR:-/opt/agentos}"
BACKUP_DIR="${DEPLOY_DIR}/backups"
LOG_DIR="${DEPLOY_DIR}/logs"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Logging functions
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
    echo -e "${RED}[ERROR]${NC} $(date '+%Y-%m-%d %H:%M:%S') $1"
}

# Print banner
print_banner() {
    echo "========================================"
    echo "  AgentOS heapstore Deployment System"
    echo "  Version: ${VERSION}"
    echo "  Target: ${DEPLOY_DIR}"
    echo "========================================"
}

# Create necessary directories
create_directories() {
    log_info "Creating deployment directories..."
    
    sudo mkdir -p "${DEPLOY_DIR}"
    sudo mkdir -p "${DEPLOY_DIR}/lib"
    sudo mkdir -p "${DEPLOY_DIR}/include/agentos/heapstore"
    sudo mkdir -p "${BACKUP_DIR}"
    sudo mkdir -p "${LOG_DIR}"
    
    log_success "Directories created"
}

# Backup current deployment
backup_current() {
    log_info "Backing up current deployment..."
    
    local CURRENT_LIB="${DEPLOY_DIR}/lib/libagentos_heapstore.a"
    
    if [ -f "${CURRENT_LIB}" ]; then
        local BACKUP_NAME="heapstore_backup_${TIMESTAMP}.tar.gz"
        sudo tar -czvf "${BACKUP_DIR}/${BACKUP_NAME}" \
            -C "${DEPLOY_DIR}" lib/libagentos_heapstore.a \
            -C "${DEPLOY_DIR}" include/agentos/heapstore/
        
        log_success "Backup created: ${BACKUP_DIR}/${BACKUP_NAME}"
        
        # Record deployment info
        echo "${VERSION}:${TIMESTAMP}:${BACKUP_NAME}" >> "${BACKUP_DIR}/deployments.log"
    else
        log_warning "No existing deployment to backup"
    fi
}

# Deploy new version
deploy() {
    log_info "Deploying version ${VERSION}..."
    
    local PACKAGE_NAME="agentos-${MODULE_NAME}-${VERSION}-$(uname -s)-$(uname -m).tar.gz"
    local PACKAGE_PATH="${DIST_DIR}/${PACKAGE_NAME}"
    
    if [ ! -f "${PACKAGE_PATH}" ]; then
        log_error "Package not found: ${PACKAGE_PATH}"
        log_info "Please run 'build.sh package' first"
        exit 1
    fi
    
    # Extract and install
    sudo tar -xzvf "${PACKAGE_PATH}" -C "${DEPLOY_DIR}/"
    
    # Update library cache
    if command -v ldconfig &> /dev/null; then
        sudo ldconfig "${DEPLOY_DIR}/lib"
    fi
    
    # Create version file
    echo "${VERSION}" | sudo tee "${DEPLOY_DIR}/lib/heapstore.version"
    
    log_success "Deployment complete"
}

# Verify deployment
verify() {
    log_info "Verifying deployment..."
    
    local LIB_PATH="${DEPLOY_DIR}/lib/libagentos_heapstore.a"
    
    if [ ! -f "${LIB_PATH}" ]; then
        log_error "Library not found: ${LIB_PATH}"
        return 1
    fi
    
    # Check file size
    local SIZE=$(stat -c%s "${LIB_PATH}" 2>/dev/null || stat -f%z "${LIB_PATH}")
    if [ "${SIZE}" -lt 1000 ]; then
        log_error "Library file too small: ${SIZE} bytes"
        return 1
    fi
    
    # Check headers
    if [ ! -d "${DEPLOY_DIR}/include/agentos/heapstore" ]; then
        log_error "Header directory not found"
        return 1
    fi
    
    local HEADER_COUNT=$(find "${DEPLOY_DIR}/include/agentos/heapstore" -name "*.h" | wc -l)
    if [ "${HEADER_COUNT}" -lt 1 ]; then
        log_error "No header files found"
        return 1
    fi
    
    log_success "Verification passed"
    return 0
}

# Rollback to previous version
rollback() {
    log_info "Rolling back to previous version..."
    
    if [ ! -f "${BACKUP_DIR}/deployments.log" ]; then
        log_error "No deployment history found"
        exit 1
    fi
    
    # Get last backup
    local LAST_BACKUP=$(tail -n 1 "${BACKUP_DIR}/deployments.log")
    local BACKUP_NAME=$(echo "${LAST_BACKUP}" | cut -d: -f3)
    local BACKUP_PATH="${BACKUP_DIR}/${BACKUP_NAME}"
    
    if [ ! -f "${BACKUP_PATH}" ]; then
        log_error "Backup not found: ${BACKUP_PATH}"
        exit 1
    fi
    
    log_info "Restoring from: ${BACKUP_NAME}"
    
    # Backup current before rollback
    backup_current
    
    # Restore
    sudo tar -xzvf "${BACKUP_PATH}" -C "${DEPLOY_DIR}/"
    
    # Update library cache
    if command -v ldconfig &> /dev/null; then
        sudo ldconfig "${DEPLOY_DIR}/lib"
    fi
    
    log_success "Rollback complete"
}

# List deployment history
history_list() {
    log_info "Deployment history:"
    
    if [ -f "${BACKUP_DIR}/deployments.log" ]; then
        echo "Version | Timestamp | Backup File"
        echo "--------|-----------|------------"
        while IFS=: read -r VER TIME FILE; do
            echo "${VER} | ${TIME} | ${FILE}"
        done < "${BACKUP_DIR}/deployments.log"
    else
        log_warning "No deployment history found"
    fi
}

# Clean old backups
clean_backups() {
    log_info "Cleaning old backups..."
    
    local KEEP_COUNT="${1:-5}"
    
    if [ -d "${BACKUP_DIR}" ]; then
        # Remove old backups, keep last N
        ls -t "${BACKUP_DIR}"/heapstore_backup_*.tar.gz 2>/dev/null | \
            tail -n +$((KEEP_COUNT + 1)) | \
            xargs -r rm -f
        
        log_success "Cleaned old backups, keeping last ${KEEP_COUNT}"
    fi
}

# Health check
health_check() {
    log_info "Running health check..."
    
    local ISSUES=0
    
    # Check library exists
    if [ ! -f "${DEPLOY_DIR}/lib/libagentos_heapstore.a" ]; then
        log_error "Library file missing"
        ISSUES=$((ISSUES + 1))
    fi
    
    # Check headers
    if [ ! -d "${DEPLOY_DIR}/include/agentos/heapstore" ]; then
        log_error "Header directory missing"
        ISSUES=$((ISSUES + 1))
    fi
    
    # Check permissions
    if [ ! -r "${DEPLOY_DIR}/lib/libagentos_heapstore.a" ]; then
        log_error "Library file not readable"
        ISSUES=$((ISSUES + 1))
    fi
    
    # Check disk space
    local AVAILABLE=$(df -P "${DEPLOY_DIR}" | awk 'NR==2 {print $4}')
    if [ "${AVAILABLE}" -lt 1048576 ]; then
        log_warning "Low disk space: $((AVAILABLE / 1024)) MB available"
    fi
    
    if [ "${ISSUES}" -eq 0 ]; then
        log_success "Health check passed"
        return 0
    else
        log_error "Health check failed with ${ISSUES} issues"
        return 1
    fi
}

# Show help
show_help() {
    echo "Usage: $0 [command] [options]"
    echo ""
    echo "Commands:"
    echo "  deploy          Deploy the module"
    echo "  rollback        Rollback to previous version"
    echo "  verify          Verify current deployment"
    echo "  history         Show deployment history"
    echo "  health          Run health check"
    echo "  clean [N]       Clean old backups (keep last N, default: 5)"
    echo "  help            Show this help message"
    echo ""
    echo "Environment Variables:"
    echo "  VERSION         Version to deploy [default: 1.0.0.6]"
    echo "  DEPLOY_DIR      Deployment directory [default: /opt/agentos]"
    echo ""
    echo "Examples:"
    echo "  $0 deploy                    # Deploy current version"
    echo "  $0 rollback                  # Rollback to previous version"
    echo "  VERSION=1.0.0.7 $0 deploy    # Deploy specific version"
}

# Main entry point
main() {
    print_banner
    
    local command="${1:-help}"
    
    case "${command}" in
        deploy)
            create_directories
            backup_current
            deploy
            verify
            ;;
        rollback)
            rollback
            verify
            ;;
        verify)
            verify
            ;;
        history)
            history_list
            ;;
        health)
            health_check
            ;;
        clean)
            clean_backups "${2:-5}"
            ;;
        help|--help|-h)
            show_help
            ;;
        *)
            log_error "Unknown command: ${command}"
            show_help
            exit 1
            ;;
    esac
}

main "$@"

