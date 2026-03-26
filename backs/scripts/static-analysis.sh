#!/bin/bash
# Copyright (c) 2026 SPHARX. All Rights Reserved.
# 静态分析脚本
# 使用 cppcheck 进行代码静态分析

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BACKS_ROOT="$(dirname "$SCRIPT_DIR")"
REPORT_DIR="${BACKS_ROOT}/reports"
CPPCHECK_CONFIG="${BACKS_ROOT}/cppcheck.xml"

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
echo "  AgentOS Backs Static Analysis"
echo "=========================================="
echo ""

# 检查 cppcheck
check_cppcheck() {
    if ! command -v cppcheck &> /dev/null; then
        log_error "cppcheck 未安装"
        log_info "安装方法:"
        log_info "  Ubuntu/Debian: sudo apt-get install cppcheck"
        log_info "  macOS: brew install cppcheck"
        log_info "  Windows: choco install cppcheck"
        exit 1
    fi
    
    CPPCHECK_VERSION=$(cppcheck --version | head -1)
    log_info "cppcheck 版本: $CPPCHECK_VERSION"
}

# 运行静态分析
run_analysis() {
    log_info "运行静态分析..."
    
    mkdir -p "$REPORT_DIR"
    
    local CPPCHECK_ARGS="--enable=all --std=c11 --platform=unix64"
    CPPCHECK_ARGS="$CPPCHECK_ARGS --xml --xml-version=2"
    CPPCHECK_ARGS="$CPPCHECK_ARGS --suppress=missingIncludeSystem"
    CPPCHECK_ARGS="$CPPCHECK_ARGS --suppress=unusedFunction"
    CPPCHECK_ARGS="$CPPCHECK_ARGS --check-level=exhaustive"
    CPPCHECK_ARGS="$CPPCHECK_ARGS --inline-suppr"
    
    # 包含目录
    CPPCHECK_ARGS="$CPPCHECK_ARGS -I ${BACKS_ROOT}/common/include"
    CPPCHECK_ARGS="$CPPCHECK_ARGS -I ${BACKS_ROOT}/llm_d/include"
    CPPCHECK_ARGS="$CPPCHECK_ARGS -I ${BACKS_ROOT}/tool_d/include"
    CPPCHECK_ARGS="$CPPCHECK_ARGS -I ${BACKS_ROOT}/market_d/include"
    CPPCHECK_ARGS="$CPPCHECK_ARGS -I ${BACKS_ROOT}/monit_d/include"
    CPPCHECK_ARGS="$CPPCHECK_ARGS -I ${BACKS_ROOT}/sched_d/include"
    
    # 定义
    CPPCHECK_ARGS="$CPPCHECK_ARGS -DAGENTOS_PLATFORM_LINUX=1"
    CPPCHECK_ARGS="$CPPCHECK_ARGS -DAGENTOS_PLATFORM_WINDOWS=0"
    CPPCHECK_ARGS="$CPPCHECK_ARGS -DAGENTOS_PLATFORM_MACOS=0"
    
    # 使用配置文件（如果存在）
    if [ -f "$CPPCHECK_CONFIG" ]; then
        log_info "使用配置文件: $CPPCHECK_CONFIG"
        CPPCHECK_ARGS="$CPPCHECK_ARGS --project=$CPPCHECK_CONFIG"
    fi
    
    # 输出文件
    local XML_REPORT="$REPORT_DIR/cppcheck_report.xml"
    local TXT_REPORT="$REPORT_DIR/cppcheck_report.txt"
    
    # 运行分析
    log_info "分析源代码..."
    cppcheck $CPPCHECK_ARGS \
        --output-file="$XML_REPORT" \
        "${BACKS_ROOT}/common/src" \
        "${BACKS_ROOT}/llm_d/src" \
        "${BACKS_ROOT}/tool_d/src" \
        "${BACKS_ROOT}/market_d/src" \
        "${BACKS_ROOT}/monit_d/src" \
        "${BACKS_ROOT}/sched_d/src" \
        2>/dev/null || true
    
    # 生成文本报告
    if [ -f "$XML_REPORT" ]; then
        log_info "生成文本报告..."
        {
            echo "========================================"
            echo "AgentOS Backs 静态分析报告"
            echo "========================================"
            echo ""
            echo "生成时间: $(date)"
            echo ""
            echo "--- 问题统计 ---"
            local error_count=$(grep -c 'severity="error"' "$XML_REPORT" 2>/dev/null || echo "0")
            local warning_count=$(grep -c 'severity="warning"' "$XML_REPORT" 2>/dev/null || echo "0")
            local style_count=$(grep -c 'severity="style"' "$XML_REPORT" 2>/dev/null || echo "0")
            local performance_count=$(grep -c 'severity="performance"' "$XML_REPORT" 2>/dev/null || echo "0")
            local portability_count=$(grep -c 'severity="portability"' "$XML_REPORT" 2>/dev/null || echo "0")
            local information_count=$(grep -c 'severity="information"' "$XML_REPORT" 2>/dev/null || echo "0")
            
            echo "  错误: $error_count"
            echo "  警告: $warning_count"
            echo "  风格: $style_count"
            echo "  性能: $performance_count"
            echo "  可移植性: $portability_count"
            echo "  信息: $information_count"
            echo ""
            echo "--- 详细问题 ---"
            echo ""
            
            # 提取错误信息
            grep -E '<error|<location' "$XML_REPORT" | \
                sed 's/<error/\n<error/g' | \
                grep '<error' | \
                head -100 || true
            
            echo ""
            echo "========================================"
        } > "$TXT_REPORT"
        
        log_success "报告已生成:"
        log_info "  XML: $XML_REPORT"
        log_info "  TXT: $TXT_REPORT"
    fi
}

# 生成HTML报告
generate_html_report() {
    if command -v cppcheck-htmlreport &> /dev/null; then
        log_info "生成HTML报告..."
        
        local HTML_DIR="$REPORT_DIR/cppcheck_html"
        local XML_REPORT="$REPORT_DIR/cppcheck_report.xml"
        
        if [ -f "$XML_REPORT" ]; then
            cppcheck-htmlreport \
                --file="$XML_REPORT" \
                --report-dir="$HTML_DIR" \
                --source-dir="$BACKS_ROOT" \
                2>/dev/null || true
            
            log_success "HTML报告已生成: $HTML_DIR/index.html"
        fi
    else
        log_warn "cppcheck-htmlreport 未找到，跳过HTML报告生成"
    fi
}

# 显示问题摘要
show_summary() {
    local XML_REPORT="$REPORT_DIR/cppcheck_report.xml"
    
    if [ -f "$XML_REPORT" ]; then
        echo ""
        echo "----------------------------------------"
        echo "  问题摘要"
        echo "----------------------------------------"
        
        local error_count=$(grep -c 'severity="error"' "$XML_REPORT" 2>/dev/null || echo "0")
        local warning_count=$(grep -c 'severity="warning"' "$XML_REPORT" 2>/dev/null || echo "0")
        
        if [ "$error_count" -gt 0 ]; then
            log_error "发现 $error_count 个严重错误"
            echo ""
            log_info "错误详情:"
            grep -A2 'severity="error"' "$XML_REPORT" | head -50 || true
            echo ""
        fi
        
        if [ "$warning_count" -gt 0 ]; then
            log_warn "发现 $warning_count 个警告"
        fi
        
        if [ "$error_count" -eq 0 ] && [ "$warning_count" -eq 0 ]; then
            log_success "未发现严重问题"
        fi
        
        echo "----------------------------------------"
        echo ""
        
        # 返回码
        if [ "$error_count" -gt 0 ]; then
            return 1
        fi
    fi
    
    return 0
}

# 使用说明
usage() {
    echo ""
    echo "用法: $0 [命令]"
    echo ""
    echo "命令:"
    echo "  analyze    运行静态分析"
    echo "  html       生成HTML报告"
    echo "  all        运行完整分析流程"
    echo "  help       显示此帮助信息"
    echo ""
}

# 主流程
case "${1:-all}" in
    analyze)
        check_cppcheck
        run_analysis
        show_summary
        ;;
    html)
        generate_html_report
        ;;
    all)
        check_cppcheck
        run_analysis
        generate_html_report
        show_summary
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
