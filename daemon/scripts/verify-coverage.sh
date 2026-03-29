#!/bin/bash
# Copyright (c) 2026 SPHARX. All Rights Reserved.
# 代码覆盖率验证脚本
# 用于验证代码覆盖率是否达到目标值

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BACKS_ROOT="$(dirname "$SCRIPT_DIR")"
COVERAGE_TARGET="${COVERAGE_TARGET:-80}"
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
echo "  AgentOS daemon Coverage Validator"
echo "=========================================="
echo ""
log_info "Coverage Target: ${COVERAGE_TARGET}%"
log_info "Report Directory: $REPORT_DIR"
echo ""

# 检查覆盖率工具
check_tools() {
    if ! command -v lcov &> /dev/null; then
        log_error "lcov 未安装"
        exit 1
    fi
    
    if ! command -v genhtml &> /dev/null; then
        log_error "genhtml 未安装"
        exit 1
    fi
    
    log_success "覆盖率工具检查通过"
}

# 收集覆盖率数据
collect_coverage() {
    log_info "收集覆盖率数据..."
    
    mkdir -p "$REPORT_DIR/coverage"
    local all_info_files=""
    
    for module in commons llm_d tool_d monit_d sched_d market_d; do
        local module_build="$BACKS_ROOT/$module/build"
        if [ -d "$module_build" ]; then
            cd "$module_build"
            
            # 查找 gcda 文件
            if find . -name "*.gcda" | grep -q .; then
                log_info "  处理模块: $module"
                lcov --capture --directory . --output-file "${module}_coverage.info" 2>/dev/null || true
                
                if [ -f "${module}_coverage.info" ]; then
                    all_info_files="$all_info_files -a ${module}_coverage.info"
                fi
            else
                log_warn "  模块 $module 无覆盖率数据"
            fi
        fi
    done
    
    if [ -z "$all_info_files" ]; then
        log_error "未找到任何覆盖率数据"
        log_info "请先运行: ./scripts/local-ci.sh build (with ENABLE_COVERAGE=ON)"
        exit 1
    fi
    
    # 合并所有覆盖率数据
    cd "$BACKS_ROOT"
    lcov $all_info_files -o "$REPORT_DIR/coverage/total_coverage.info" 2>/dev/null || true
    
    # 过滤系统头文件
    if [ -f "$REPORT_DIR/coverage/total_coverage.info" ]; then
        lcov --remove "$REPORT_DIR/coverage/total_coverage.info" '/usr/*' --output-file "$REPORT_DIR/coverage/total_coverage.info" 2>/dev/null || true
        lcov --remove "$REPORT_DIR/coverage/total_coverage.info" '*/tests/*' --output-file "$REPORT_DIR/coverage/total_coverage.info" 2>/dev/null || true
    fi
    
    log_success "覆盖率数据收集完成"
}

# 生成HTML报告
generate_html_report() {
    log_info "生成HTML报告..."
    
    if [ -f "$REPORT_DIR/coverage/total_coverage.info" ]; then
        genhtml "$REPORT_DIR/coverage/total_coverage.info" \
            --output-directory "$REPORT_DIR/coverage/html" \
            --title "AgentOS daemon Coverage Report" \
            --legend \
            --show-details \
            2>/dev/null || true
        
        log_success "HTML报告已生成: $REPORT_DIR/coverage/html/index.html"
    else
        log_error "未找到覆盖率数据文件"
        exit 1
    fi
}

# 验证覆盖率
verify_coverage() {
    log_info "验证覆盖率..."
    
    if [ ! -f "$REPORT_DIR/coverage/total_coverage.info" ]; then
        log_error "覆盖率数据文件不存在"
        exit 1
    fi
    
    # 提取覆盖率百分比
    local coverage_output=$(lcov --summary "$REPORT_DIR/coverage/total_coverage.info" 2>&1)
    
    echo ""
    echo "----------------------------------------"
    echo "  覆盖率统计"
    echo "----------------------------------------"
    echo "$coverage_output" | grep -E "lines|functions|branches" || true
    echo "----------------------------------------"
    echo ""
    
    # 提取行覆盖率
    local line_coverage=$(echo "$coverage_output" | grep -oP 'lines.*: \K[\d.]+' | head -1)
    
    if [ -n "$line_coverage" ]; then
        log_info "行覆盖率: ${line_coverage}%"
        
        # 检查是否达到目标
        local coverage_check=$(echo "$line_coverage >= $COVERAGE_TARGET" | bc -l 2>/dev/null || echo "0")
        
        if [ "$coverage_check" = "1" ]; then
            log_success "覆盖率达标 (>= ${COVERAGE_TARGET}%)"
            echo ""
            echo "=========================================="
            echo "  ✅ COVERAGE VALIDATION PASSED"
            echo "=========================================="
            return 0
        else
            log_warn "覆盖率未达标 (< ${COVERAGE_TARGET}%)"
            log_warn "当前: ${line_coverage}%"
            log_warn "目标: ${COVERAGE_TARGET}%"
            echo ""
            echo "=========================================="
            echo "  ⚠️ COVERAGE VALIDATION WARNING"
            echo "=========================================="
            echo ""
            log_info "建议增加更多单元测试以提高覆盖率"
            
            # 输出未覆盖文件列表
            log_info "未充分覆盖的文件:"
            lcov --list "$REPORT_DIR/coverage/total_coverage.info" 2>/dev/null | \
                grep -v "100%" | \
                grep -E "\.c:" | \
                head -20 || true
            
            return 1
        fi
    else
        log_error "无法解析覆盖率数据"
        return 1
    fi
}

# 主流程
main() {
    check_tools
    collect_coverage
    generate_html_report
    verify_coverage
}

# 执行
main
