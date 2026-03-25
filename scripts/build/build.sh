#!/usr/bin/env bash
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS 核心构建脚本
# 遵循 AgentOS 架构设计原则：工程美学、反馈闭环、跨平台一致性

###############################################################################
# 严格模式
###############################################################################
set -euo pipefail

###############################################################################
# 来源依赖
###############################################################################
AGENTOS_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
AGENTOS_SCRIPTS_DIR="$(dirname "$AGENTOS_SCRIPT_DIR")"
AGENTOS_PROJECT_ROOT="$(dirname "$AGENTOS_SCRIPTS_DIR")"

# shellcheck source=../lib/common.sh
source "$AGENTOS_SCRIPTS_DIR/lib/common.sh"

###############################################################################
# 版本信息
###############################################################################
declare -r AGENTOS_VERSION="1.0.0.6"
declare -r AGENTOS_BUILD_NUMBER="${AGENTOS_BUILD_NUMBER:-$(date +%Y%m%d%H%M)}"

###############################################################################
# 构建配置
###############################################################################
BUILD_TYPE="${BUILD_TYPE:-release}"
BUILD_PARALLEL="${BUILD_PARALLEL:-}"
BUILD_OUTPUT_DIR="$AGENTOS_PROJECT_ROOT/build"
BUILD_CONFIG="${BUILD_CONFIG:-}"
BUILD_TARGETS="${BUILD_TARGETS:-all}"
BUILD_CLEAN="${BUILD_CLEAN:-0}"
BUILD_TEST="${BUILD_TEST:-0}"
BUILD_VERIFY="${BUILD_VERIFY:-0}"
BUILD_SHARED="${BUILD_SHARED:-1}"
BUILD_STATIC="${BUILD_STATIC:-1}"

###############################################################################
# CMake 配置选项
###############################################################################
CMAKE_MIN_VERSION="3.20"
CMAKE_PREFIX_PATH="$AGENTOS_PROJECT_ROOT"
CMAKE_INSTALL_PREFIX="${CMAKE_INSTALL_PREFIX:-/usr/local}"

###############################################################################
# 颜色定义
###############################################################################
COLOR_BOLD='\033[1m'
COLOR_DIM='\033[2m'
COLOR_RED='\033[0;31m'
COLOR_GREEN='\033[0;32m'
COLOR_YELLOW='\033[1;33m'
COLOR_BLUE='\033[0;34m'
COLOR_CYAN='\033[0;36m'
COLOR_NC='\033[0m'

###############################################################################
# 构建状态变量
###############################################################################
BUILD_START_TIME=0
BUILD_END_TIME=0
BUILD_SUCCESS=0
BUILD_ERRORS=()

###############################################################################
# 打印函数
###############################################################################
print_banner() {
    echo -e "${COLOR_CYAN}"
    cat << "EOF"
   ____          _        __  __                                                   _
  / __ \        | |      |  \/  |                                                 | |
 | |  | |_ __   | | __ _| \  / | __ _ _ __   __ _  ___ _ __ ___   ___  _ __  __ _| |_
 | |  | | '_ \ / _` |/ _` | |\/| |/ _` | '_ \ / _` |/ _ \ '_ ` _ \ / _ \| '_ \/ _` | __|
 | |__| | |_) | (_| | (_| | |  | | (_| | | | | (_| |  __/ | | | | | (_) | | | | (_| | |_
  \____/| .__/ \__,_|\__,_|_|  |_|\__,_|_| |_|\__, |\___|_| |_| |_|\___/|_| |_|\__,_|\__|
        | |                                     __/ |
        |_|                                    |___/
EOF
    echo -e "${COLOR_NC}"
    echo -e "${COLOR_BOLD}AgentOS Build System v${AGENTOS_VERSION}${COLOR_NC}"
    echo -e "${COLOR_DIM}Build Number: ${AGENTOS_BUILD_NUMBER}${COLOR_NC}"
    echo ""
}

print_usage() {
    cat << EOF
${COLOR_BOLD}用法:${COLOR_NC} $0 [选项]

${COLOR_BOLD}构建选项:${COLOR_NC}
    --debug               构建调试版本
    --release             构建发布版本 (默认)
    --config <name>       CMake 配置名称 (Debug/Release/Rekase)
    --parallel <n>       并行编译任务数 (默认: CPU 核心数)
    --targets <targets>   构建目标 (默认: all)
    --output <dir>        构建输出目录 (默认: ./build)

${COLOR_BOLD}操作选项:${COLOR_NC}
    --clean               清理构建产物
    --test                构建后运行测试
    --verify              验证构建产物完整性

${COLOR_BOLD}库选项:${COLOR_NC}
    --shared              构建共享库 (默认: 开启)
    --static             构建静态库 (默认: 开启)
    --no-shared          禁用共享库
    --no-static          禁用静态库

${COLOR_BOLD}安装选项:${COLOR_NC}
    --prefix <path>       安装前缀 (默认: /usr/local)

${COLOR_BOLD}输出选项:${COLOR_NC}
    --verbose             详细输出
    --quiet               静默输出
    --log <file>          输出日志到文件

${COLOR_BOLD}示例:${COLOR_NC}
    $0 --release                      # 发布版本构建
    $0 --debug --test                 # 调试版本构建并测试
    $0 --clean --parallel 4           # 清理后4线程构建
    $0 --targets coreloopthree        # 仅构建核心运行时

EOF
}

print_section() {
    echo ""
    echo -e "${COLOR_BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${COLOR_NC}"
    echo -e "${COLOR_BLUE}▶ $1${COLOR_NC}"
    echo -e "${COLOR_BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${COLOR_NC}"
    echo ""
}

print_status() {
    local status=$1
    local message=$2
    case "$status" in
        ok)      echo -e "${COLOR_GREEN}[✓]${COLOR_NC} $message" ;;
        fail)    echo -e "${COLOR_RED}[✗]${COLOR_NC} $message" ;;
        info)    echo -e "${COLOR_BLUE}[•]${COLOR_NC} $message" ;;
        warn)    echo -e "${COLOR_YELLOW}[!]${COLOR_NC} $message" ;;
        skip)    echo -e "${COLOR_DIM}[-]${COLOR_NC} $message" ;;
    esac
}

###############################################################################
# 检查函数
###############################################################################
check_cmake() {
    if ! command -v cmake &> /dev/null; then
        print_status "fail" "CMake 未安装"
        echo -e "${COLOR_DIM}请安装 CMake ${CMAKE_MIN_VERSION} 或更高版本:${COLOR_NC}"
        echo "    Ubuntu/Debian: sudo apt-get install cmake"
        echo "    macOS: brew install cmake"
        echo "    Windows: winget install Kitware.CMake"
        return 1
    fi

    local cmake_version
    cmake_version=$(cmake --version | head -n1 | awk '{print $3}')
    print_status "info" "CMake 版本: $cmake_version"

    if ! agentos_version_check "$CMAKE_MIN_VERSION" "$cmake_version"; then
        print_status "fail" "CMake 版本过低 (需要 >= ${CMAKE_MIN_VERSION})"
        return 1
    fi

    return 0
}

check_compiler() {
    local compiler="${CC:-gcc}"

    if ! command -v "$compiler" &> /dev/null; then
        compiler="cc"
    fi

    if ! command -v "$compiler" &> /dev/null; then
        print_status "fail" "C 编译器未安装"
        echo -e "${COLOR_DIM}请安装 GCC 或 Clang:${COLOR_NC}"
        echo "    Ubuntu/Debian: sudo apt-get install gcc g++ build-essential"
        echo "    macOS: brew install gcc llvm"
        return 1
    fi

    local cc_version
    cc_version=$("$compiler" --version | head -n1)
    print_status "info" "编译器: $cc_version"

    return 0
}

check_build_tools() {
    print_section "检查构建工具"

    local tools=("make" "cmake")
    local missing=()

    for tool in "${tools[@]}"; do
        if ! command -v "$tool" &> /dev/null; then
            missing+=("$tool")
        fi
    done

    if [[ ${#missing[@]} -gt 0 ]]; then
        print_status "fail" "缺少必需工具: ${missing[*]}"
        return 1
    fi

    print_status "ok" "所有构建工具就绪"

    check_cmake
    check_compiler
}

###############################################################################
# 构建函数
###############################################################################
configure_cmake() {
    print_section "配置 CMake"

    mkdir -p "$BUILD_OUTPUT_DIR"

    local cmake_args=(
        -S "$AGENTOS_PROJECT_ROOT"
        -B "$BUILD_OUTPUT_DIR"
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
        -DCMAKE_INSTALL_PREFIX="$CMAKE_INSTALL_PREFIX"
        -DAGENTOS_VERSION="$AGENTOS_VERSION"
        -DAGENTOS_BUILD_NUMBER="$AGENTOS_BUILD_NUMBER"
    )

    if [[ "$BUILD_SHARED" == "1" ]]; then
        cmake_args+=(-DBUILD_SHARED_LIBS=ON)
    fi

    if [[ "$BUILD_STATIC" == "1" ]]; then
        cmake_args+=(-DBUILD_STATIC_LIBS=ON)
    fi

    if [[ -n "$BUILD_CONFIG" ]]; then
        cmake_args+=(-C "$BUILD_CONFIG")
    fi

    echo -e "${COLOR_DIM}执行: cmake ${cmake_args[*]}${COLOR_NC}"
    echo ""

    if ! cmake "${cmake_args[@]}"; then
        print_status "fail" "CMake 配置失败"
        return 1
    fi

    print_status "ok" "CMake 配置完成"
    return 0
}

build_targets() {
    print_section "编译构建"

    local parallel_arg="-j$(nproc)"
    if [[ -n "$BUILD_PARALLEL" ]]; then
        parallel_arg="-j$BUILD_PARALLEL"
    fi

    echo -e "${COLOR_DIM}执行: cmake --build $BUILD_OUTPUT_DIR --target $BUILD_TARGETS $parallel_arg${COLOR_NC}"
    echo ""

    if ! cmake --build "$BUILD_OUTPUT_DIR" --target "$BUILD_TARGETS" $parallel_arg; then
        print_status "fail" "编译失败"
        return 1
    fi

    print_status "ok" "编译完成"
    return 0
}

run_tests() {
    if [[ "$BUILD_TEST" != "1" ]]; then
        return 0
    fi

    print_section "运行测试"

    local ctest_args=(
        --output-on-failure
        --test-dir "$BUILD_OUTPUT_DIR"
    )

    if ! ctest "${ctest_args[@]}"; then
        print_status "fail" "测试失败"
        return 1
    fi

    print_status "ok" "所有测试通过"
    return 0
}

verify_build() {
    if [[ "$BUILD_VERIFY" != "1" ]]; then
        return 0
    fi

    print_section "验证构建产物"

    local libs_found=0
    local libs_expected=(
        "libagentos_corekern.so"
        "libagentos_coreloopthree.so"
        "libagentos_memoryrovol.so"
    )

    for lib in "${libs_expected[@]}"; do
        if [[ -f "$BUILD_OUTPUT_DIR/lib/$lib" ]] || [[ -f "$BUILD_OUTPUT_DIR/lib/$lib.1.0.0" ]]; then
            ((libs_found++))
        fi
    done

    if [[ $libs_found -ge ${#libs_expected[@]} ]]; then
        print_status "ok" "构建产物验证通过"
        return 0
    else
        print_status "fail" "构建产物验证失败 (找到 $libs_found/${#libs_expected[@]})"
        return 1
    fi
}

clean_build() {
    print_section "清理构建"

    if [[ -d "$BUILD_OUTPUT_DIR" ]]; then
        rm -rf "$BUILD_OUTPUT_DIR"
        print_status "ok" "构建目录已清理: $BUILD_OUTPUT_DIR"
    else
        print_status "skip" "构建目录不存在"
    fi
}

###############################################################################
# 主流程
###############################################################################
main() {
    BUILD_START_TIME=$(date +%s)

    print_banner

    # 解析参数
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --debug)        BUILD_TYPE="Debug"; shift ;;
            --release)      BUILD_TYPE="Release"; shift ;;
            --config)       BUILD_CONFIG="$2"; shift 2 ;;
            --parallel)     BUILD_PARALLEL="$2"; shift 2 ;;
            --targets)      BUILD_TARGETS="$2"; shift 2 ;;
            --output)       BUILD_OUTPUT_DIR="$2"; shift 2 ;;
            --clean)        BUILD_CLEAN=1; shift ;;
            --test)         BUILD_TEST=1; shift ;;
            --verify)       BUILD_VERIFY=1; shift ;;
            --shared)       BUILD_SHARED=1; shift ;;
            --static)       BUILD_STATIC=1; shift ;;
            --no-shared)    BUILD_SHARED=0; shift ;;
            --no-static)    BUILD_STATIC=0; shift ;;
            --prefix)       CMAKE_INSTALL_PREFIX="$2"; shift 2 ;;
            --verbose)      VERBOSE=1; shift ;;
            --quiet)        VERBOSE=0; shift ;;
            --log)          exec > >(tee "$2"); shift 2 ;;
            --help|-h)      print_usage; exit 0 ;;
            *)              echo "未知选项: $1"; print_usage; exit 1 ;;
        esac
    done

    # 执行清理
    if [[ "$BUILD_CLEAN" == "1" ]]; then
        clean_build
        if [[ $# -eq 0 ]]; then
            exit 0
        fi
    fi

    # 检查环境
    if ! check_build_tools; then
        exit 1
    fi

    # 构建
    if ! configure_cmake; then
        BUILD_END_TIME=$(date +%s)
        exit 1
    fi

    if ! build_targets; then
        BUILD_END_TIME=$(date +%s)
        exit 1
    fi

    # 测试
    if ! run_tests; then
        BUILD_END_TIME=$(date +%s)
        exit 1
    fi

    # 验证
    if ! verify_build; then
        BUILD_END_TIME=$(date +%s)
        exit 1
    fi

    # 完成
    BUILD_END_TIME=$(date +%s)
    BUILD_SUCCESS=1

    print_section "构建完成"

    local duration=$((BUILD_END_TIME - BUILD_START_TIME))
    echo -e "${COLOR_GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${COLOR_NC}"
    echo -e "${COLOR_BOLD}  构建成功!${COLOR_NC}"
    echo -e "${COLOR_DIM}  版本: ${AGENTOS_VERSION}${COLOR_NC}"
    echo -e "${COLOR_DIM}  构建类型: ${BUILD_TYPE}${COLOR_NC}"
    echo -e "${COLOR_DIM}  耗时: ${duration} 秒${COLOR_NC}"
    echo -e "${COLOR_DIM}  输出目录: ${BUILD_OUTPUT_DIR}${COLOR_NC}"
    echo -e "${COLOR_GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${COLOR_NC}"
    echo ""

    return 0
}

###############################################################################
# 错误处理
###############################################################################
trap '[[ $? -ne 0 ]] && print_status "fail" "构建过程出错"' EXIT

###############################################################################
# 执行入口
###############################################################################
main "$@"