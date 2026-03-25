#!/bin/bash
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS Atoms Module Build Script
# Version: 1.0.0
#
# 本脚本遵循《工程控制论》的反馈闭环原则
# 提供跨平台、多编译器的统一构建入口

set -e  # 遇错即停
set -u  # 未定义变量报错
set -o pipefail  # 管道错误传播

# ==================== 全局变量 ====================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ATOMS_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
VERSION="1.0.0.5"
BUILD_TYPE="${BUILD_TYPE:-Release}"
BUILD_DIR="${BUILD_DIR:-build}"
INSTALL_PREFIX="${INSTALL_PREFIX:-${ATOMS_ROOT}/dist}"
PARALLEL_JOBS="${PARALLEL_JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}"
VERBOSE="${VERBOSE:-0}"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# ==================== 辅助函数 ====================

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
    echo -e "${RED}[ERROR]${NC} $1" >&2
}

die() {
    log_error "$1"
    exit 1
}

check_command() {
    if ! command -v "$1" &> /dev/null; then
        die "Required command '$1' not found. Please install it first."
    fi
}

detect_os() {
    case "$(uname -s)" in
        Linux*)     echo "linux";;
        Darwin*)    echo "macos";;
        CYGWIN*|MINGW*|MSYS*)    echo "windows";;
        *)          echo "unknown";;
    esac
}

detect_arch() {
    case "$(uname -m)" in
        x86_64|amd64)   echo "x86_64";;
        arm64|aarch64)  echo "arm64";;
        armv7l)         echo "armv7";;
        *)              echo "unknown";;
    esac
}

# ==================== 构建函数 ====================

configure_cmake() {
    local module="$1"
    local module_dir="${ATOMS_ROOT}/${module}"
    local build_dir="${module_dir}/${BUILD_DIR}"
    
    log_info "Configuring ${module}..."
    
    mkdir -p "${build_dir}"
    cd "${build_dir}"
    
    # 设置编译器
    local cc="${CC:-gcc}"
    local cxx="${CXX:-g++}"
    
    if [[ "${cc}" == "clang" ]]; then
        cxx="clang++"
    fi
    
    # 平台特定配置
    local extra_flags=""
    local os=$(detect_os)
    
    case "${os}" in
        linux)
            extra_flags="-DCMAKE_C_FLAGS=\"-Wall -Wextra -Werror=return-type -fPIC\""
            ;;
        macos)
            extra_flags="-DCMAKE_C_FLAGS=\"-Wall -Wextra -Werror=return-type\""
            ;;
        windows)
            extra_flags=""
            ;;
    esac
    
    # 配置CMake
    cmake "${module_dir}" \
        -G "Ninja" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DCMAKE_C_COMPILER="${cc}" \
        -DCMAKE_CXX_COMPILER="${cxx}" \
        -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
        -DBUILD_TESTS=ON \
        -DBUILD_EXAMPLES=OFF \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        ${extra_flags} \
        ${CMAKE_EXTRA_FLAGS:-}
    
    log_success "Configuration complete for ${module}"
}

build_module() {
    local module="$1"
    local module_dir="${ATOMS_ROOT}/${module}"
    local build_dir="${module_dir}/${BUILD_DIR}"
    
    log_info "Building ${module}..."
    
    cd "${build_dir}"
    
    local verbose_flag=""
    if [[ "${VERBOSE}" == "1" ]]; then
        verbose_flag="-v"
    fi
    
    ninja -j"${PARALLEL_JOBS}" ${verbose_flag}
    
    log_success "Build complete for ${module}"
}

run_tests() {
    local module="$1"
    local module_dir="${ATOMS_ROOT}/${module}"
    local build_dir="${module_dir}/${BUILD_DIR}"
    
    log_info "Running tests for ${module}..."
    
    cd "${build_dir}"
    
    ctest --output-on-failure --timeout 300 -j"${PARALLEL_JOBS}"
    
    log_success "Tests passed for ${module}"
}

install_module() {
    local module="$1"
    local module_dir="${ATOMS_ROOT}/${module}"
    local build_dir="${module_dir}/${BUILD_DIR}"
    
    log_info "Installing ${module}..."
    
    cd "${build_dir}"
    
    ninja install
    
    log_success "Installation complete for ${module}"
}

package_module() {
    local module="$1"
    local module_dir="${ATOMS_ROOT}/${module}"
    local build_dir="${module_dir}/${BUILD_DIR}"
    local os=$(detect_os)
    local arch=$(detect_arch)
    local cc="${CC:-gcc}"
    local package_name="atoms-${module}-${VERSION}-${os}-${arch}-${cc}"
    
    log_info "Packaging ${module}..."
    
    mkdir -p "${build_dir}/package/lib"
    mkdir -p "${build_dir}/package/include"
    
    # 复制静态库
    find "${build_dir}" -name "*.a" -exec cp {} "${build_dir}/package/lib/" \; 2>/dev/null || true
    find "${build_dir}" -name "*.lib" -exec cp {} "${build_dir}/package/lib/" \; 2>/dev/null || true
    
    # 复制头文件
    cp -r "${module_dir}/include/"* "${build_dir}/package/include/" 2>/dev/null || true
    
    # 创建压缩包
    cd "${build_dir}/package"
    tar -czvf "${ATOMS_ROOT}/${package_name}.tar.gz" .
    
    log_success "Package created: ${package_name}.tar.gz"
}

generate_coverage() {
    local module="$1"
    local module_dir="${ATOMS_ROOT}/${module}"
    local build_dir="${module_dir}/${BUILD_DIR}"
    
    log_info "Generating coverage report for ${module}..."
    
    # 重新配置以启用覆盖率
    cd "${build_dir}"
    cmake "${module_dir}" \
        -DCMAKE_C_FLAGS="--coverage -fprofile-arcs -ftest-coverage"
    
    ninja clean
    ninja
    ctest --output-on-failure
    
    # 生成覆盖率报告
    gcovr -r "${module_dir}" --xml -o coverage.xml --html-details coverage.html
    
    log_success "Coverage report generated for ${module}"
}

# ==================== 清理函数 ====================

clean_module() {
    local module="$1"
    local module_dir="${ATOMS_ROOT}/${module}"
    
    log_info "Cleaning ${module}..."
    
    rm -rf "${module_dir}/${BUILD_DIR}"
    rm -f "${ATOMS_ROOT}/atoms-${module}-*.tar.gz"
    
    log_success "Clean complete for ${module}"
}

clean_all() {
    log_info "Cleaning all modules..."
    
    for module in corekern coreloopthree memoryrovol syscall utils; do
        if [[ -d "${ATOMS_ROOT}/${module}" ]]; then
            clean_module "${module}"
        fi
    done
    
    rm -rf "${INSTALL_PREFIX}"
    
    log_success "Clean complete for all modules"
}

# ==================== 主构建流程 ====================

build_all() {
    log_info "Building all atoms modules..."
    log_info "OS: $(detect_os), Arch: $(detect_arch)"
    log_info "Build Type: ${BUILD_TYPE}"
    log_info "Parallel Jobs: ${PARALLEL_JOBS}"
    
    local modules=("corekern" "coreloopthree" "memoryrovol" "syscall" "utils")
    local failed_modules=()
    
    for module in "${modules[@]}"; do
        if [[ ! -d "${ATOMS_ROOT}/${module}" ]]; then
            log_warning "Module ${module} not found, skipping..."
            continue
        fi
        
        if ! configure_cmake "${module}"; then
            failed_modules+=("${module}")
            continue
        fi
        
        if ! build_module "${module}"; then
            failed_modules+=("${module}")
            continue
        fi
    done
    
    if [[ ${#failed_modules[@]} -gt 0 ]]; then
        log_error "Failed modules: ${failed_modules[*]}"
        return 1
    fi
    
    log_success "All modules built successfully!"
}

# ==================== 帮助信息 ====================

show_help() {
    cat << EOF
AgentOS Atoms Module Build Script
Version: ${VERSION}

Usage: $0 [command] [options]

Commands:
    configure <module>    Configure a specific module
    build <module>        Build a specific module
    test <module>         Run tests for a specific module
    install <module>      Install a specific module
    package <module>      Package a specific module
    coverage <module>     Generate coverage report for a module
    clean <module>        Clean a specific module
    clean-all             Clean all modules
    build-all             Build all modules
    help                  Show this help message

Options:
    BUILD_TYPE=Release|Debug    Set build type (default: Release)
    BUILD_DIR=build             Set build directory (default: build)
    INSTALL_PREFIX=/path        Set install prefix (default: dist/)
    CC=gcc|clang                Set C compiler (default: gcc)
    PARALLEL_JOBS=N             Set parallel jobs (default: auto-detect)
    VERBOSE=1                   Enable verbose output

Examples:
    $0 build-all
    $0 build corekern
    $0 test coreloopthree
    BUILD_TYPE=Debug $0 build memoryrovol
    CC=clang $0 build-all

Environment Variables:
    BUILD_TYPE          Build type (Release/Debug/RelWithDebInfo)
    BUILD_DIR           Build directory name
    INSTALL_PREFIX      Installation prefix
    CC                  C compiler
    CXX                 C++ compiler
    PARALLEL_JOBS       Number of parallel jobs
    VERBOSE             Enable verbose output (0/1)
    CMAKE_EXTRA_FLAGS   Extra CMake flags

EOF
}

# ==================== 入口点 ====================

main() {
    local command="${1:-help}"
    shift || true
    
    # 检查必需工具
    check_command cmake
    check_command ninja
    
    case "${command}" in
        configure)
            [[ $# -lt 1 ]] && die "Usage: $0 configure <module>"
            configure_cmake "$1"
            ;;
        build)
            [[ $# -lt 1 ]] && die "Usage: $0 build <module>"
            configure_cmake "$1"
            build_module "$1"
            ;;
        test)
            [[ $# -lt 1 ]] && die "Usage: $0 test <module>"
            run_tests "$1"
            ;;
        install)
            [[ $# -lt 1 ]] && die "Usage: $0 install <module>"
            install_module "$1"
            ;;
        package)
            [[ $# -lt 1 ]] && die "Usage: $0 package <module>"
            package_module "$1"
            ;;
        coverage)
            [[ $# -lt 1 ]] && die "Usage: $0 coverage <module>"
            generate_coverage "$1"
            ;;
        clean)
            [[ $# -lt 1 ]] && die "Usage: $0 clean <module>"
            clean_module "$1"
            ;;
        clean-all)
            clean_all
            ;;
        build-all)
            build_all
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
