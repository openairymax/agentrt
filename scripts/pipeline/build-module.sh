#!/usr/bin/env bash
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS 模块构建脚本
# 支持：多模块并行构建、多构建类型、增量构建、缓存感知
# Version: 2.0.0

set -euo pipefail

###############################################################################
# 路径定义
###############################################################################
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
LIB_DIR="${SCRIPT_DIR}/../lib"

###############################################################################
# 颜色和日志
###############################################################################
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
BLUE='\033[0;34m'; NC='\033[0m'

log_info()  { echo -e "${BLUE}[BUILD]${NC}  $*"; }
log_ok()    { echo -e "${GREEN}[BUILD-OK]${NC} $*"; }
log_warn()  { echo -e "${YELLOW}[BUILD-WARN]${NC} $*"; }
log_error() { echo -e "${RED}[BUILD-ERR]${NC} $*" >&2; }

###############################################################################
# 默认配置
###############################################################################
BUILD_MODULE="all"
BUILD_TYPE="Release"
PARALLEL_JOBS="auto"
INCREMENTAL=false
CLEAN_BUILD=false
VERBOSE=false
INSTALL_PREFIX="${AGENTOS_INSTALL_PREFIX:-/usr/local}"
CMAKE_EXTRA_ARGS=()

# 模块定义（含源码路径和 CMake 选项）
declare -A MODULE_SOURCES=(
    [daemon]="agentos/daemon"
    [atoms]="agentos/atoms"
    [commons]="agentos/commons"
    [cupolas]="agentos/cupolas"
    [gateway]="agentos/gateway"
    [heapstore]="agentos/heapstore"
    [manager]="agentos/manager"
)

declare -A MODULE_CMAKE_OPTIONS=(
    [daemon]="-DBUILD_TESTS=ON -DENABLE_LLM_DUMMY=ON"
    [atoms]="-DBUILD_TESTS=ON"
    [commons]="-DBUILD_TESTS=ON"
    [cupolas]="-DBUILD_TESTS=ON"
    [gateway]="-DBUILD_TESTS=ON"
    [heapstore]="-DBUILD_TESTS=ON"
    [manager]="-DBUILD_TESTS=ON"
)

###############################################################################
# 参数解析
###############################################################################
parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --module|-m)   BUILD_MODULE="$2"; shift ;;
            --type|-t)     BUILD_TYPE="$2"; shift ;;
            --parallel|-j) PARALLEL_JOBS="$2"; shift ;;
            --incremental|-i) INCREMENTAL=true ;;
            --clean|-c)    CLEAN_BUILD=true ;;
            --verbose|-v)  VERBOSE=true ;;
            --prefix|-p)   INSTALL_PREFIX="$2"; shift ;;
            --cmake-arg)   CMAKE_EXTRA_ARGS+=("$2"); shift ;;
            --help|-h)
                show_help; exit 0
                ;;
            *) log_warn "Unknown option: $1" ;;
        esac
        shift
    done
}

show_help() {
    cat << 'EOF'
AgentOS Module Build Script v2.0.0

Usage: ./build-module.sh [OPTIONS]

Options:
    -m, --module NAME     Target module (default: all)
                           Values: daemon, atoms, commons, cupolas,
                                   gateway, heapstore, all
    -t, --type TYPE       Build type (default: Release)
                           Values: Debug, Release, RelWithDebInfo, MinSizeRel
    -j, --parallel N      Parallel jobs (default: auto)
    -i, --incremental     Incremental build (reuse build dir)
    -c, --clean           Clean build (remove build dir first)
    -v, --verbose         Verbose cmake output
    -p, --prefix PATH     Install prefix (default: /usr/local)
    --cmake-arg ARG       Extra CMake argument (repeatable)
    -h, --help            Show this help

Examples:
    ./build-module.sh                          # Build all modules
    ./build-module.sh -m daemon                # Build daemon only
    ./build-module.sh -t Debug -j8             # Debug, 8 parallel
    ./build-module.sh -m atoms -c              # Clean build atoms
EOF
}

###############################################################################
# 工具函数
###############################################################################
get_parallel_jobs() {
    if [[ "$PARALLEL_JOBS" == "auto" ]]; then
        nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo "4"
    else
        echo "$PARALLEL_JOBS"
    fi
}

validate_module() {
    local module="$1"
    if [[ "$module" == "all" ]]; then
        return 0
    fi
    if [[ -z "${MODULE_SOURCES[$module]:-}" ]]; then
        log_error "Unknown module: $module"
        log_info "Available modules: ${!MODULE_SOURCES[@]}"
        return 1
    fi
    return 0
}

get_module_list() {
    if [[ "$BUILD_MODULE" == "all" ]]; then
        echo "${!MODULE_SOURCES[@]}"
    else
        echo "$BUILD_MODULE"
    fi
}

setup_build_env() {
    log_info "Setting up build environment..."

    # pkg-config 路径（tiktoken stub）
    export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:${PKG_CONFIG_PATH:-}"

    # macOS 特殊处理
    if [[ "$(uname -s)" == "Darwin" ]] && command -v brew &>/dev/null; then
        local openssl_prefix
        openssl_prefix=$(brew --prefix openssl@3 2>/dev/null || brew --prefix openssl 2>/dev/null || echo "")
        if [[ -n "$openssl_prefix" ]]; then
            export OPENSSL_ROOT_DIR="$openssl_prefix"
            log_info "macOS OpenSSL root: $OPENSSL_ROOT_DIR"
        fi
    fi

    # CMake 编译器 flags
    case "$BUILD_TYPE" in
        Debug)
            export CMAKE_C_FLAGS="${CMAKE_C_FLAGS:--g -O0 -Wall -Wextra}"
            export CMAKE_CXX_FLAGS="${CMAKE_CXX_FLAGS:--g -O0 -Wall -Wextra}"
            ;;
        Release)
            export CMAKE_C_FLAGS="${CMAKE_C_FLAGS:--O2 -DNDEBUG}"
            export CMAKE_CXX_FLAGS="${CMAKE_CXX_FLAGS:--O2 -DNDEBUG}"
            ;;
        RelWithDebInfo)
            export CMAKE_C_FLAGS="${CMAKE_C_FLAGS:--g -O2 -DNDEBUG}"
            export CMAKE_CXX_FLAGS="${CMAKE_CXX_FLAGS:--g -O2 -DNDEBUG}"
            ;;
        MinSizeRel)
            export CMAKE_C_FLAGS="${CMAKE_C_FLAGS:--Os -DNDEBUG}"
            export CMAKE_CXX_FLAGS="${CMAKE_CXX_FLAGS:--Os -DNDEBUG}"
            ;;
    esac
}

###############################################################################
# 核心构建函数
###############################################################################
build_module() {
    local module="$1"
    local source_dir="${PROJECT_ROOT}/${MODULE_SOURCES[$module]}"
    local build_dir="${PROJECT_ROOT}/build-${module}"

    # 验证源码目录
    if [[ ! -d "$source_dir" ]]; then
        log_warn "Source directory not found for '$module': $source_dir, skipping"
        return 0
    fi

    # 检查 CMakeLists.txt
    if [[ ! -f "${source_dir}/CMakeLists.txt" ]]; then
        log_warn "No CMakeLists.txt found for '$module', skipping"
        return 0
    fi

    log_info "==========================================="
    log_info "Building Module: $module"
    log_info "  Source: $source_dir"
    log_info "  Type:   $BUILD_TYPE"
    log_info "  Jobs:   $(get_parallel_jobs)"
    log_info "==========================================="

    # Clean build
    if [[ "$CLEAN_BUILD" == "true" ]] && [[ -d "$build_dir" ]]; then
        log_info "Cleaning build directory: $build_dir"
        rm -rf "$build_dir"
    fi

    mkdir -p "$build_dir"
    cd "$build_dir"

    # CMake 配置（增量构建时跳过已配置的模块）
    local need_cmake_config=true
    if [[ "$INCREMENTAL" == "true" ]] && [[ -f "${build_dir}/CMakeCache.txt" ]]; then
        local cache_source_dir
        cache_source_dir=$(grep -m1 "CMAKE_HOME_DIRECTORY:INTERNAL=" "${build_dir}/CMakeCache.txt" 2>/dev/null | cut -d= -f2 || echo "")
        if [[ "$cache_source_dir" == "$source_dir" ]]; then
            log_info "Incremental build: reusing existing CMake configuration"
            need_cmake_config=false
        else
            log_info "Incremental build: source directory changed, reconfiguring"
        fi
    fi

    if [[ "$need_cmake_config" == "true" ]]; then
        local cmake_args=(
            "$source_dir"
            "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
            "-DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX}"
            ${MODULE_CMAKE_OPTIONS[$module]}
            "${CMAKE_EXTRA_ARGS[@]}"
        )

        local cmake_log="${PROJECT_ROOT}/ci-logs/${module}-cmake.log"
        mkdir -p "$(dirname "$cmake_log")"

        log_info "Running CMake configuration..."
        if [[ "$VERBOSE" == "true" ]]; then
            cmake "${cmake_args[@]}" | tee "$cmake_log"
        else
            if ! cmake "${cmake_args[@]}" > "$cmake_log" 2>&1; then
                log_error "CMake configuration failed for $module"
                log_error "See log: $cmake_log"
                cd "$PROJECT_ROOT"
                return 1
            fi
        fi
    fi

    # 构建
    local build_log="${PROJECT_ROOT}/ci-logs/${module}-build.log"
    local jobs=$(get_parallel_jobs)

    log_info "Building with $jobs parallel jobs..."
    if [[ "$VERBOSE" == "true" ]]; then
        cmake --build . --parallel "$jobs" | tee "$build_log"
    else
        if ! cmake --build . --parallel "$jobs" > "$build_log" 2>&1; then
            log_error "Build failed for $module"
            log_error "See log: $build_log"
            cd "$PROJECT_ROOT"
            return 1
        fi
    fi

    # 统计构建产物
    local obj_count
    obj_count=$(find . -name "*.o" 2>/dev/null | wc -l)
    local lib_count
    lib_count=$(find . \( -name "*.a" -o -name "*.so" -o -name "*.dylib" \) 2>/dev/null | wc -l)
    local bin_count
    bin_count=$(find . -type f -executable ! -name "*.sh" 2>/dev/null | wc -l)

    log_ok "Module $module built successfully!"
    log_info "  Objects: $obj_count, Libraries: $lib_count, Binaries: $bin_count"

    cd "$PROJECT_ROOT"
    return 0
}

###############################################################################
# 主流程
###############################################################################
main() {
    parse_args "$@"

    log_info "AgentOS Build Script v2.0.0"
    log_info "Timestamp: $(date '+%Y-%m-%d %H:%M:%S')"

    # 验证模块
    if ! validate_module "$BUILD_MODULE"; then
        exit 1
    fi

    # 设置环境
    setup_build_env

    # 构建计时
    local start_time
    start_time=$(date +%s)

    # 获取模块列表并构建
    local modules
    modules=$(get_module_list)
    local failed_modules=()
    local success_count=0

    for module in $modules; do
        if build_module "$module"; then
            ((success_count++))
        else
            failed_modules+=("$module")
        fi
    done

    # 构建摘要
    local end_time
    end_time=$(date +%s)
    local duration=$(( end_time - start_time ))

    log_info "==========================================="
    log_info "Build Summary"
    log_info "==========================================="
    log_info "Total time: ${duration}s"
    log_info "Successful: $success_count"
    log_info "Failed:     ${#failed_modules[@]}"

    if [[ ${#failed_modules[@]} -gt 0 ]]; then
        log_error "Failed modules: ${failed_modules[*]}"
        exit 1
    fi

    log_ok "All modules built successfully!"
    exit 0
}

main "$@"
