#!/bin/bash
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# Partdata Module Build Script
# "From data intelligence emerges."

set -e

# Configuration
MODULE_NAME="partdata"
VERSION="1.0.0.6"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
BUILD_TYPE="${BUILD_TYPE:-Release}"
INSTALL_PREFIX="${INSTALL_PREFIX:-/usr/local}"
JOBS="${JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Logging functions
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

# Print banner
print_banner() {
    echo "========================================"
    echo "  AgentOS Partdata Build System"
    echo "  Version: ${VERSION}"
    echo "  Build Type: ${BUILD_TYPE}"
    echo "========================================"
}

# Check prerequisites
check_prerequisites() {
    log_info "Checking prerequisites..."
    
    local missing=()
    
    if ! command -v cmake &> /dev/null; then
        missing+=("cmake")
    fi
    
    if ! command -v gcc &> /dev/null && ! command -v clang &> /dev/null; then
        missing+=("gcc or clang")
    fi
    
    if ! command -v make &> /dev/null && ! command -v ninja &> /dev/null; then
        missing+=("make or ninja")
    fi
    
    if [ ${#missing[@]} -ne 0 ]; then
        log_error "Missing prerequisites: ${missing[*]}"
        log_info "Please install the missing tools and try again."
        exit 1
    fi
    
    log_success "All prerequisites satisfied"
}

# Detect generator
detect_generator() {
    if command -v ninja &> /dev/null; then
        GENERATOR="Ninja"
        BUILD_CMD="ninja"
    else
        GENERATOR="Unix Makefiles"
        BUILD_CMD="make -j${JOBS}"
    fi
    log_info "Using generator: ${GENERATOR}"
}

# Configure the build
configure() {
    log_info "Configuring build..."
    
    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"
    
    cmake "${PROJECT_ROOT}" \
        -G "${GENERATOR}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
        -DBUILD_TESTS="${BUILD_TESTS:-ON}" \
        -DBUILD_BENCHMARK="${BUILD_BENCHMARK:-ON}" \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    
    log_success "Configuration complete"
}

# Build the project
build() {
    log_info "Building project (${JOBS} parallel jobs)..."
    
    cd "${BUILD_DIR}"
    ${BUILD_CMD}
    
    log_success "Build complete"
}

# Run tests
test() {
    log_info "Running tests..."
    
    cd "${BUILD_DIR}"
    
    if [ -f "tests/partdata_tests" ]; then
        ./tests/partdata_tests --output-on-failure
    else
        ctest --output-on-failure -j${JOBS}
    fi
    
    log_success "All tests passed"
}

# Run benchmarks
benchmark() {
    log_info "Running benchmarks..."
    
    cd "${BUILD_DIR}"
    
    if [ -f "tests/partdata_benchmark" ]; then
        ./tests/partdata_benchmark
    else
        log_warning "Benchmark binary not found"
    fi
}

# Install the library
install() {
    log_info "Installing to ${INSTALL_PREFIX}..."
    
    cd "${BUILD_DIR}"
    ${BUILD_CMD} install
    
    log_success "Installation complete"
}

# Clean build artifacts
clean() {
    log_info "Cleaning build artifacts..."
    
    rm -rf "${BUILD_DIR}"
    
    log_success "Clean complete"
}

# Create distribution package
package() {
    log_info "Creating distribution package..."
    
    local DIST_DIR="${PROJECT_ROOT}/dist"
    local PACKAGE_NAME="agentos-${MODULE_NAME}-${VERSION}-$(uname -s)-$(uname -m)"
    
    mkdir -p "${DIST_DIR}"
    
    cd "${BUILD_DIR}"
    
    # Create tarball
    tar -czvf "${DIST_DIR}/${PACKAGE_NAME}.tar.gz" \
        libagentos_partdata.a \
        ../include/*.h
    
    # Create zip
    cd "${PROJECT_ROOT}"
    zip -r "${DIST_DIR}/${PACKAGE_NAME}.zip" \
        partdata/include/*.h \
        build/libagentos_partdata.a
    
    log_success "Package created: ${DIST_DIR}/${PACKAGE_NAME}.tar.gz"
}

# Generate coverage report
coverage() {
    log_info "Generating coverage report..."
    
    cd "${BUILD_DIR}"
    
    if command -v lcov &> /dev/null; then
        lcov --capture --directory . --output-file coverage.info
        lcov --remove coverage.info '/usr/*' '*/tests/*' --output-file coverage.filtered.info
        
        if command -v genhtml &> /dev/null; then
            genhtml coverage.filtered.info --output-directory coverage_report
            log_success "Coverage report generated: ${BUILD_DIR}/coverage_report/"
        fi
    else
        log_warning "lcov not installed, skipping coverage report"
    fi
}

# Show help
show_help() {
    echo "Usage: $0 [command] [options]"
    echo ""
    echo "Commands:"
    echo "  configure    Configure the build system"
    echo "  build        Build the project"
    echo "  test         Run unit tests"
    echo "  benchmark    Run performance benchmarks"
    echo "  install      Install the library"
    echo "  clean        Remove build artifacts"
    echo "  package      Create distribution package"
    echo "  coverage     Generate coverage report"
    echo "  all          Run configure, build, and test"
    echo "  help         Show this help message"
    echo ""
    echo "Environment Variables:"
    echo "  BUILD_TYPE       Build type (Release|Debug|RelWithDebInfo) [default: Release]"
    echo "  BUILD_TESTS      Build tests (ON|OFF) [default: ON]"
    echo "  BUILD_BENCHMARK  Build benchmarks (ON|OFF) [default: ON]"
    echo "  INSTALL_PREFIX   Installation prefix [default: /usr/local]"
    echo "  JOBS             Number of parallel jobs [default: auto]"
    echo ""
    echo "Examples:"
    echo "  $0 all                          # Configure, build, and test"
    echo "  BUILD_TYPE=Debug $0 build       # Debug build"
    echo "  $0 clean && $0 all              # Clean rebuild"
}

# Main entry point
main() {
    print_banner
    
    local command="${1:-all}"
    
    case "${command}" in
        configure)
            check_prerequisites
            detect_generator
            configure
            ;;
        build)
            check_prerequisites
            detect_generator
            build
            ;;
        test)
            test
            ;;
        benchmark)
            benchmark
            ;;
        install)
            install
            ;;
        clean)
            clean
            ;;
        package)
            package
            ;;
        coverage)
            coverage
            ;;
        all)
            check_prerequisites
            detect_generator
            configure
            build
            test
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
