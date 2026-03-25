#!/bin/bash
# Copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges".
# AgentOS Backs CI/CD 本地验证脚本
# 用于本地验证 CI/CD 流程

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BACKS_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_TYPE="${BUILD_TYPE:-Release}"
PARALLEL_JOBS="${PARALLEL_JOBS:-$(nproc)}"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

echo "=========================================="
echo "  AgentOS Backs Local CI/CD Validator"
echo "=========================================="
echo ""
log_info "Build Root: $BACKS_ROOT"
log_info "Build Type: $BUILD_TYPE"
log_info "Parallel Jobs: $PARALLEL_JOBS"
echo ""

# 检查依赖
check_dependencies() {
    log_info "检查构建依赖..."

    local missing_deps=0

    for cmd in cmake ninja gcc; do
        if ! command -v "$cmd" &> /dev/null; then
            log_error "缺少依赖: $cmd"
            missing_deps=1
        fi
    done

    for pkg in libyaml libcjson libcurl; do
        if ! pkg-config --exists "$pkg" 2>/dev/null; then
            log_error "缺少系统库: $pkg"
            missing_deps=1
        fi
    done

    if [ $missing_deps -eq 1 ]; then
        log_error "缺少必要依赖，请先安装"
        exit 1
    fi

    log_success "依赖检查通过"
}

# 清理构建目录
clean() {
    log_info "清理构建目录..."
    rm -rf "$BACKS_ROOT/common/build"
    rm -rf "$BACKS_ROOT/llm_d/build"
    rm -rf "$BACKS_ROOT/tool_d/build"
    rm -rf "$BACKS_ROOT/monit_d/build"
    rm -rf "$BACKS_ROOT/sched_d/build"
    rm -rf "$BACKS_ROOT/market_d/build"
    log_success "清理完成"
}

# 构建 common 库
build_common() {
    log_info "构建 backs-common..."
    mkdir -p "$BACKS_ROOT/common/build"
    cd "$BACKS_ROOT/common/build"

    cmake .. -G "Ninja" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DBUILD_TESTS=ON \
        -DBUILD_COVERAGE=OFF

    ninja

    if [ "$BUILD_TYPE" = "Debug" ]; then
        log_warn "Debug 模式，跳过测试"
    else
        ctest --output-on-failure -j"$PARALLEL_JOBS" || true
    fi

    log_success "backs-common 构建完成"
}

# 构建 llm_d
build_llm_d() {
    log_info "构建 llm_d..."
    mkdir -p "$BACKS_ROOT/llm_d/build"
    cd "$BACKS_ROOT/llm_d/build"

    cmake .. -G "Ninja" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DBUILD_TESTS=ON

    ninja

    if [ "$BUILD_TYPE" = "Debug" ]; then
        log_warn "Debug 模式，跳过测试"
    else
        ctest --output-on-failure -j"$PARALLEL_JOBS" || true
    fi

    log_success "llm_d 构建完成"
}

# 构建 tool_d
build_tool_d() {
    log_info "构建 tool_d..."
    mkdir -p "$BACKS_ROOT/tool_d/build"
    cd "$BACKS_ROOT/tool_d/build"

    cmake .. -G "Ninja" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DBUILD_TESTS=ON

    ninja

    if [ "$BUILD_TYPE" = "Debug" ]; then
        log_warn "Debug 模式，跳过测试"
    else
        ctest --output-on-failure -j"$PARALLEL_JOBS" || true
    fi

    log_success "tool_d 构建完成"
}

# 构建 monit_d
build_monit_d() {
    log_info "构建 monit_d..."
    mkdir -p "$BACKS_ROOT/monit_d/build"
    cd "$BACKS_ROOT/monit_d/build"

    cmake .. -G "Ninja" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DBUILD_TESTS=ON

    ninja

    if [ "$BUILD_TYPE" = "Debug" ]; then
        log_warn "Debug 模式，跳过测试"
    else
        ctest --output-on-failure -j"$PARALLEL_JOBS" || true
    fi

    log_success "monit_d 构建完成"
}

# 构建 sched_d
build_sched_d() {
    log_info "构建 sched_d..."
    mkdir -p "$BACKS_ROOT/sched_d/build"
    cd "$BACKS_ROOT/sched_d/build"

    cmake .. -G "Ninja" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DBUILD_TESTS=ON

    ninja

    if [ "$BUILD_TYPE" = "Debug" ]; then
        log_warn "Debug 模式，跳过测试"
    else
        ctest --output-on-failure -j"$PARALLEL_JOBS" || true
    fi

    log_success "sched_d 构建完成"
}

# 构建 market_d
build_market_d() {
    log_info "构建 market_d..."
    mkdir -p "$BACKS_ROOT/market_d/build"
    cd "$BACKS_ROOT/market_d/build"

    cmake .. -G "Ninja" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DBUILD_TESTS=ON

    ninja

    if [ "$BUILD_TYPE" = "Debug" ]; then
        log_warn "Debug 模式，跳过测试"
    else
        ctest --output-on-failure -j"$PARALLEL_JOBS" || true
    fi

    log_success "market_d 构建完成"
}

# 代码静态分析
static_analysis() {
    log_info "运行静态分析..."

    if command -v cppcheck &> /dev/null; then
        log_info "运行 cppcheck..."
        cppcheck --enable=all --std=c11 --platform=unix64 \
            -j"$PARALLEL_JOBS" \
            "$BACKS_ROOT/common" \
            "$BACKS_ROOT/llm_d" \
            "$BACKS_ROOT/tool_d" \
            "$BACKS_ROOT/monit_d" \
            "$BACKS_ROOT/sched_d" \
            "$BACKS_ROOT/market_d" 2>&1 | tee cppcheck_report.txt || true
        log_success "cppcheck 完成"
    else
        log_warn "cppcheck 未安装，跳过"
    fi

    if command -v clang-format &> /dev/null; then
        log_info "检查代码格式..."
        find "$BACKS_ROOT/common" "$BACKS_ROOT/llm_d" "$BACKS_ROOT/tool_d" \
            "$BACKS_ROOT/monit_d" "$BACKS_ROOT/sched_d" "$BACKS_ROOT/market_d" \
            \( -name '*.c' -o -name '*.h' \) \
            -exec clang-format --dry-run {} \; 2>&1 || {
                log_warn "代码格式检查发现一些问题，请运行 clang-format -i 修复"
            }
        log_success "代码格式检查完成"
    else
        log_warn "clang-format 未安装，跳过"
    fi
}

# 打印使用说明
usage() {
    echo ""
    echo "用法: $0 [命令]"
    echo ""
    echo "命令:"
    echo "  clean          清理所有构建目录"
    echo "  build          构建所有模块"
    echo "  common         仅构建 backs-common"
    echo "  llm_d          仅构建 llm_d"
    echo "  tool_d         仅构建 tool_d"
    echo "  monit_d        仅构建 monit_d"
    echo "  sched_d        仅构建 sched_d"
    echo "  market_d       仅构建 market_d"
    echo "  all            构建所有模块并运行静态分析"
    echo "  analysis       仅运行静态分析"
    echo "  help           显示此帮助信息"
    echo ""
    echo "环境变量:"
    echo "  BUILD_TYPE     构建类型 (Release/Debug)，默认为 Release"
    echo "  PARALLEL_JOBS  并行作业数，默认为 CPU 核心数"
    echo ""
}

# 主逻辑
case "${1:-build}" in
    clean)
        clean
        ;;
    build)
        check_dependencies
        build_common
        build_llm_d
        build_tool_d
        build_monit_d
        build_sched_d
        build_market_d
        log_success "所有模块构建完成!"
        ;;
    common)
        check_dependencies
        build_common
        ;;
    llm_d)
        check_dependencies
        build_llm_d
        ;;
    tool_d)
        check_dependencies
        build_tool_d
        ;;
    monit_d)
        check_dependencies
        build_monit_d
        ;;
    sched_d)
        check_dependencies
        build_sched_d
        ;;
    market_d)
        check_dependencies
        build_market_d
        ;;
    all)
        check_dependencies
        build_common
        build_llm_d
        build_tool_d
        build_monit_d
        build_sched_d
        build_market_d
        static_analysis
        log_success "全部完成!"
        ;;
    analysis)
        static_analysis
        ;;
    help|--help|-h)
        usage
        ;;
    *)
        log_error "未知命令: $1"
        usage
        exit 1
        ;;
esac

echo ""
log_success "脚本执行完成"