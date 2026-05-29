#!/bin/bash
# AgentOS Setup Script
# Uses library/ for shared utilities

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIB_DIR="${SCRIPT_DIR}/library"

source "${LIB_DIR}/common.sh" 2>/dev/null || {
    echo "ERROR: Cannot load ${LIB_DIR}/common.sh"
    exit 1
}

PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

print_header() {
    echo ""
    echo "========================================"
    echo "  AgentOS Setup"
    echo "========================================"
    echo ""
}

show_menu() {
    echo ""
    echo "Available options:"
    echo "  1) Install dependencies (vcpkg, Python packages)"
    echo "  2) Build AgentOS (CMake)"
    echo "  3) Run tests"
    echo "  4) Full setup (all of the above)"
    echo "  5) Exit"
    echo ""
}

install_dependencies() {
    agentos_log_info "Installing dependencies..."
    
    OS=$(agentos_platform_detect_os)
    agentos_log_info "Detected OS: $OS"
    
    if [ "$OS" = "linux" ]; then
        if command -v vcpkg &> /dev/null; then
            agentos_log_info "vcpkg found: $(vcpkg version | head -1)"
        else
            agentos_log_warn "vcpkg not found in PATH"
            agentos_log_info "Please install vcpkg and set VCPKG_ROOT"
            agentos_log_info "See: https://github.com/microsoft/vcpkg"
        fi
        
        if command -v python3 &> /dev/null; then
            agentos_log_info "Python3: $(python3 --version)"
        else
            agentos_log_error "Python3 not found"
        fi
        
    elif [ "$OS" = "macos" ]; then
        if command -v brew &> /dev/null; then
            agentos_log_info "Homebrew installed"
        else
            agentos_log_warn "Homebrew not found"
        fi
        
        if command -v python3 &> /dev/null; then
            agentos_log_info "Python3: $(python3 --version)"
        else
            agentos_log_error "Python3 not found"
        fi
        
    elif [ "$OS" = "windows" ]; then
        agentos_log_error "Windows not supported by this script"
        agentos_log_info "Please use setup.ps1 on Windows"
        return 1
    fi
    
    agentos_log_info "Dependencies check complete"
}

build_project() {
    agentos_log_info "Building AgentOS..."
    
    BUILD_SCRIPT="${SCRIPT_DIR}/pipeline/build-module.sh"
    
    if [ ! -f "$BUILD_SCRIPT" ]; then
        agentos_log_error "Build script not found: $BUILD_SCRIPT"
        agentos_log_info "Available scripts in pipeline/:"
        ls -la "${SCRIPT_DIR}/pipeline/" 2>/dev/null || agentos_log_warn "Cannot list pipeline directory"
        return 1
    fi
    
    chmod +x "$BUILD_SCRIPT"
    "$BUILD_SCRIPT" Release
}

run_tests() {
    agentos_log_info "Running tests..."
    
    TEST_SCRIPT="${SCRIPT_DIR}/tests/shell/test_framework.sh"
    
    if [ -f "$TEST_SCRIPT" ]; then
        chmod +x "$TEST_SCRIPT"
        bash "$TEST_SCRIPT"
    else
        agentos_log_warn "Test script not found: $TEST_SCRIPT"
        agentos_log_info "Running basic validation..."
        
        if [ -f "${PROJECT_ROOT}/CMakeLists.txt" ]; then
            agentos_log_info "CMakeLists.txt exists"
        else
            agentos_log_error "CMakeLists.txt not found!"
        fi
    fi
}

full_setup() {
    agentos_log_info "Starting full setup..."
    install_dependencies && build_project && run_tests
}

main() {
    print_header
    
    if [ $# -eq 0 ]; then
        show_menu
        read -p "Select option [1-5]: " choice
        
        case $choice in
            1) install_dependencies ;;
            2) build_project ;;
            3) run_tests ;;
            4) full_setup ;;
            5|q|Q) 
                agentos_log_info "Exiting..."
                exit 0
                ;;
            *) 
                agentos_log_error "Invalid option: $choice"
                exit 1
                ;;
        esac
    else
        case "${1:-}" in
            --deps|deps|1) install_dependencies ;;
            --build|build|2) build_project ;;
            --test|test|3) run_tests ;;
            --all|all|4) full_setup ;;
            --help|-h|help)
                show_menu
                exit 0
                ;;
            *)
                agentos_log_error "Unknown option: $1"
                show_menu
                exit 1
                ;;
        esac
    fi
    
    echo ""
    agentos_log_info "Setup complete!"
}

main "$@"
