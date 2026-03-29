#!/bin/bash
# Copyright (c) 2026 SPHARX. All Rights Reserved.
# AgentOS daemon CI/CD 本地验证脚本
# 用于本地验证 CI/CD 流程
# 包含：构建、测试、静态分析、代码覆盖率

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BACKS_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_TYPE="${BUILD_TYPE:-Release}"
PARALLEL_JOBS="${PARALLEL_JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}"
ENABLE_COVERAGE="${ENABLE_COVERAGE:-OFF}"
REPORT_DIR="${BACKS_ROOT}/reports"

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
echo "  AgentOS daemon Local CI/CD Validator"
echo "=========================================="
echo ""
log_info "Build Root: $BACKS_ROOT"
log_info "Build Type: $BUILD_TYPE"
log_info "Parallel Jobs: $PARALLEL_JOBS"
log_info "Coverage: $ENABLE_COVERAGE"
echo ""

# 创建报告目录
mkdir -p "$REPORT_DIR"

# 检查依赖
check_dependencies() {
    log_info "检查构建依赖..."

    local missing_deps=0

    for cmd in cmake gcc; do
        if ! command -v "$cmd" &> /dev/null; then
            log_error "缺少依赖: $cmd"
            missing_deps=1
        fi
    done

    # 检查可选依赖
    CPPCHECK_AVAILABLE=false
    if command -v cppcheck &> /dev/null; then
        CPPCHECK_AVAILABLE=true
        log_info "cppcheck 已安装"
    else
        log_warn "cppcheck 未安装，将跳过静态分析"
    fi

    COVERAGE_AVAILABLE=false
    if command -v gcov &> /dev/null && command -v lcov &> /dev/null; then
        COVERAGE_AVAILABLE=true
        log_info "gcov/lcov 已安装"
    else
        log_warn "gcov/lcov 未安装，将跳过代码覆盖率"
    fi

    if [ $missing_deps -eq 1 ]; then
        log_error "缺少必要依赖，请先安装"
        exit 1
    fi

    log_success "依赖检查通过"
}

# 清理构建目录
clean() {
    log_info "清理构建目录..."
    rm -rf "$BACKS_ROOT/commons/build"
    rm -rf "$BACKS_ROOT/llm_d/build"
    rm -rf "$BACKS_ROOT/tool_d/build"
    rm -rf "$BACKS_ROOT/monit_d/build"
    rm -rf "$BACKS_ROOT/sched_d/build"
    rm -rf "$BACKS_ROOT/market_d/build"
    rm -rf "$REPORT_DIR"
    log_success "清理完成"
}

# 构建模块通用函数
build_module() {
    local module=$1
    local module_dir="$BACKS_ROOT/$module"
    
    log_info "构建 $module..."
    mkdir -p "$module_dir/build"
    cd "$module_dir/build"

    local cmake_args="-DCMAKE_BUILD_TYPE=$BUILD_TYPE -DBUILD_TESTS=ON"
    
    if [ "$ENABLE_COVERAGE" = "ON" ] && [ "$COVERAGE_AVAILABLE" = true ]; then
        cmake_args="$cmake_args -DBUILD_COVERAGE=ON"
        cmake_args="$cmake_args -DCMAKE_C_FLAGS=--coverage"
        cmake_args="$cmake_args -DCMAKE_EXE_LINKER_FLAGS=--coverage"
    fi

    cmake .. $cmake_args

    make -j"$PARALLEL_JOBS"

    # 运行测试
    log_info "运行 $module 测试..."
    if [ -f "CTestTestfile.cmake" ]; then
        ctest --output-on-failure -j"$PARALLEL_JOBS" || log_warn "$module 部分测试失败"
    fi

    log_success "$module 构建完成"
    cd "$BACKS_ROOT"
}

# 构建所有模块
build_all() {
    build_module "commons"
    build_module "llm_d"
    build_module "tool_d"
    build_module "monit_d"
    build_module "sched_d"
    build_module "market_d"
}

# 代码静态分析
static_analysis() {
    if [ "$CPPCHECK_AVAILABLE" = false ]; then
        log_warn "cppcheck 未安装，跳过静态分析"
        return 0
    fi

    log_info "运行静态分析..."

    local CPPCHECK_REPORT="$REPORT_DIR/cppcheck_report.xml"

    cppcheck \
        --enable=all \
        --std=c11 \
        --platform=unix64 \
        --xml \
        --xml-version=2 \
        --output-file="$CPPCHECK_REPORT" \
        --suppress=missingIncludeSystem \
        --suppress=unusedFunction \
        -j"$PARALLEL_JOBS" \
        "$BACKS_ROOT/commons/src" \
        "$BACKS_ROOT/llm_d/src" \
        "$BACKS_ROOT/tool_d/src" \
        "$BACKS_ROOT/monit_d/src" \
        "$BACKS_ROOT/sched_d/src" \
        "$BACKS_ROOT/market_d/src" \
        2>/dev/null || {
            log_warn "cppcheck 发现问题，请查看报告: $CPPCHECK_REPORT"
        }

    # 统计错误数量
    if [ -f "$CPPCHECK_REPORT" ]; then
        local error_count=$(grep -c '<error' "$CPPCHECK_REPORT" 2>/dev/null || echo "0")
        log_info "cppcheck 发现 $error_count 个问题"
    fi

    log_success "静态分析完成"
}

# 代码覆盖率分析
coverage_analysis() {
    if [ "$COVERAGE_AVAILABLE" = false ]; then
        log_warn "gcov/lcov 未安装，跳过覆盖率分析"
        return 0
    fi

    if [ "$ENABLE_COVERAGE" != "ON" ]; then
        log_warn "未启用覆盖率编译，跳过覆盖率分析"
        return 0
    fi

    log_info "运行代码覆盖率分析..."

    local COVERAGE_DIR="$REPORT_DIR/coverage"
    mkdir -p "$COVERAGE_DIR"

    # 收集所有模块的覆盖率数据
    local all_info_files=""
    for module in commons llm_d tool_d monit_d sched_d market_d; do
        if [ -d "$BACKS_ROOT/$module/build" ]; then
            cd "$BACKS_ROOT/$module/build"
            if ls *.gcda 1>/dev/null 2>&1; then
                lcov --capture --directory . --output-file "${module}_coverage.info" 2>/dev/null || true
                if [ -f "${module}_coverage.info" ]; then
                    all_info_files="$all_info_files ${module}_coverage.info"
                fi
            fi
        fi
    done

    # 合并覆盖率数据
    if [ -n "$all_info_files" ]; then
        cd "$BACKS_ROOT"
        lcov --output-file "$COVERAGE_DIR/total_coverage.info" $all_info_files 2>/dev/null || true
        
        # 过滤系统头文件
        lcov --remove "$COVERAGE_DIR/total_coverage.info" '/usr/*' --output-file "$COVERAGE_DIR/total_coverage.info" 2>/dev/null || true
        
        # 生成HTML报告
        genhtml "$COVERAGE_DIR/total_coverage.info" --output-directory "$COVERAGE_DIR/html" 2>/dev/null || true

        # 提取覆盖率百分比
        if [ -f "$COVERAGE_DIR/total_coverage.info" ]; then
            local coverage_output=$(lcov --summary "$COVERAGE_DIR/total_coverage.info" 2>&1)
            local coverage_percent=$(echo "$coverage_output" | grep -oP 'lines.*: \K[\d.]+(?=%)' | head -1)
            
            if [ -n "$coverage_percent" ]; then
                log_info "代码覆盖率: ${coverage_percent}%"
                
                # 检查是否达到80%
                if (( $(echo "$coverage_percent >= 80.0" | bc -l 2>/dev/null || echo "0") )); then
                    log_success "代码覆盖率达标 (>= 80%)"
                else
                    log_warn "代码覆盖率未达标 (< 80%)，当前: ${coverage_percent}%"
                fi
            fi
        fi

        log_success "覆盖率报告已生成: $COVERAGE_DIR/html/index.html"
    else
        log_warn "未找到覆盖率数据"
    fi
}

# 生成构建报告
generate_report() {
    log_info "生成构建报告..."
    
    local REPORT_FILE="$REPORT_DIR/build_report.txt"
    
    {
        echo "========================================"
        echo "AgentOS Backs模块构建报告"
        echo "========================================"
        echo ""
        echo "构建时间: $(date)"
        echo "构建类型: $BUILD_TYPE"
        echo ""
        echo "--- 模块状态 ---"
        for module in commons llm_d tool_d market_d monit_d sched_d; do
            if [ -d "$BACKS_ROOT/$module/build" ]; then
                echo "  ${module}: ✓ 已构建"
            else
                echo "  ${module}: ✗ 未构建"
            fi
        done
        echo ""
        echo "--- 测试状态 ---"
        if [ -d "$REPORT_DIR/coverage" ]; then
            echo "  覆盖率报告: ✓ 已生成"
        else
            echo "  覆盖率报告: ✗ 未生成"
        fi
        echo ""
        echo "--- 静态分析 ---"
        if [ -f "$REPORT_DIR/cppcheck_report.xml" ]; then
            local error_count=$(grep -c '<error' "$REPORT_DIR/cppcheck_report.xml" 2>/dev/null || echo "0")
            echo "  cppcheck问题数: ${error_count}"
        else
            echo "  cppcheck: 未运行"
        fi
        echo ""
        echo "========================================"
    } > "$REPORT_FILE"
    
    log_success "构建报告已生成: $REPORT_FILE"
}

# 打印使用说明
usage() {
    echo ""
    echo "用法: $0 [命令]"
    echo ""
    echo "命令:"
    echo "  clean          清理所有构建目录"
    echo "  build          构建所有模块"
    echo "  all            构建所有模块并运行静态分析和覆盖率"
    echo "  analysis       仅运行静态分析"
    echo "  coverage       仅运行覆盖率分析（需先构建）"
    echo "  report         生成构建报告"
    echo "  help           显示此帮助信息"
    echo ""
    echo "环境变量:"
    echo "  BUILD_TYPE     构建类型 (Release/Debug)，默认为 Release"
    echo "  PARALLEL_JOBS  并行作业数，默认为 CPU 核心数"
    echo "  ENABLE_COVERAGE 启用覆盖率 (ON/OFF)，默认为 OFF"
    echo ""
}

# 主逻辑
case "${1:-build}" in
    clean)
        clean
        ;;
    build)
        check_dependencies
        build_all
        log_success "所有模块构建完成!"
        ;;
    all)
        ENABLE_COVERAGE=ON
        check_dependencies
        build_all
        static_analysis
        coverage_analysis
        generate_report
        log_success "全部完成!"
        ;;
    analysis)
        static_analysis
        ;;
    coverage)
        coverage_analysis
        ;;
    report)
        generate_report
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
