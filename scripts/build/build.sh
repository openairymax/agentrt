#!/bin/bash
# AgentOS Cross-platform Build Script (Linux/macOS)
# Uses library/ for shared utilities

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIB_DIR="${SCRIPT_DIR}/../library"

source "${LIB_DIR}/common.sh" 2>/dev/null || {
    echo "ERROR: Cannot load ${LIB_DIR}/common.sh"
    exit 1
}

main() {
    agentos_log_info "AgentOS Build Script v1.0.0"
    agentos_log_info "================================"
    
    OS=$(agentos_platform_detect_os)
    ARCH=$(agentos_platform_detect_arch)
    
    agentos_log_info "Operating System: $OS"
    agentos_log_info "Architecture: $ARCH"
    
    if [ "$OS" = "windows" ]; then
        agentos_log_warn "Windows detected, switching to PowerShell..."
        agentos_log_info "Switching to build.ps1..."
        if [ -f "./build.ps1" ]; then
            powershell -ExecutionPolicy Bypass -File ./build.ps1 "$@"
            exit $?
        else
            agentos_log_error "build.ps1 not found"
            exit 1
        fi
    fi
    
    BUILD_TYPE="${1:-Release}"
    case "$BUILD_TYPE" in
        Debug|Release|RelWithDebInfo|MinSizeRel)
            agentos_log_info "Build Type: $BUILD_TYPE"
            ;;
        *)
            agentos_log_error "Invalid build type: $BUILD_TYPE"
            agentos_log_error "Options: Debug, Release, RelWithDebInfo, MinSizeRel"
            exit 1
            ;;
    esac
    
    BUILD_DIR="build_${OS}_${ARCH}_${BUILD_TYPE}"
    agentos_log_info "Build Directory: $BUILD_DIR"
    
    CLEAN=false
    if [ "${2:-}" = "--clean" ] || [ "${2:-}" = "-c" ]; then
        CLEAN=true
    fi
    
    if [ "$CLEAN" = true ] && [ -d "$BUILD_DIR" ]; then
        agentos_log_info "Cleaning build directory: $BUILD_DIR"
        rm -rf "$BUILD_DIR"
    fi
    
    if [ ! -d "$BUILD_DIR" ]; then
        mkdir -p "$BUILD_DIR"
    fi
    
    cd "$BUILD_DIR"
    
    agentos_log_info "Configuring CMake..."
    
    CMAKE_OPTIONS="
        -DCMAKE_BUILD_TYPE=$BUILD_TYPE
        -DBUILD_TESTS=OFF
        -DBUILD_EXAMPLES=ON
        -DBUILD_DOCS=OFF
        -DENABLE_COVERAGE=OFF
    "
    
    if [ "$OS" = "linux" ]; then
        CMAKE_OPTIONS="$CMAKE_OPTIONS -DENABLE_SANITIZERS=OFF"
    elif [ "$OS" = "macos" ]; then
        CMAKE_OPTIONS="$CMAKE_OPTIONS -DCMAKE_OSX_ARCHITECTURES=$ARCH"
    fi
    
    CMAKE_OPTIONS="$CMAKE_OPTIONS
        -DBUILD_MANAGER=OFF
        -DBUILD_DAEMON=OFF
        -DBUILD_OPENLAB=OFF
        -DBUILD_TOOLKIT=OFF
        -DBUILD_HEAPSTORE=OFF
        -DBUILD_GATEWAY=OFF
    "
    
    cmake .. $CMAKE_OPTIONS
    
    agentos_log_info "Building..."
    CORES=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    agentos_log_info "Using $CORES cores"
    cmake --build . --config $BUILD_TYPE --parallel $CORES
    
    if [ "${3:-}" = "--install" ] || [ "${3:-}" = "-i" ]; then
        agentos_log_info "Installing..."
        cmake --install . --config $BUILD_TYPE --prefix ../install
        agentos_log_info "Installed to: ../install"
    fi
    
    agentos_log_info "Build Complete!"
    agentos_log_info "Directory: $(pwd)"
    agentos_log_info "Type: $BUILD_TYPE"
    
    cd ..
}

main "$@"
