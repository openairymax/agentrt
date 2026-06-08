#!/bin/bash
# =============================================================================
# check_memcpy_dynamic.sh — 动态长度 memcpy/memmove/memset 前置校验扫描 (SEC-11)
# Phase 2.5: 内存安全四层防御体系 — Tier1 预防层
# 用途: 自动扫描项目中所有动态长度的内存拷贝操作
#       标记需要人工审查或替换为 AGENTOS_MEMCPY_SAFE 的调用点
# 调用: bash check_memcpy_dynamic.sh [project_root] [--fix] [--report FILE]
# =============================================================================

set -euo pipefail

PROJECT_ROOT="${1:-$(cd "$(dirname "$0")/../../.." && pwd)}"
REPORT_FILE=""
FIX_MODE=false

# 参数解析
for arg in "$@"; do
    case "$arg" in
        --fix)     FIX_MODE=true ;;
        --report)  shift; REPORT_FILE="$1" ;;
    esac
done

# 颜色输出
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'

total_violations=0
total_safe=0
total_review=0
declare -a VIOLATIONS=()
declare -a REVIEW_NEEDED=()

log_info()  { echo -e "${CYAN}[SCAN]${NC}  $*"; }
log_ok()    { echo -e "${GREEN}[SAFE]${NC}  $*"; }
log_warn()  { echo -e "${YELLOW}[REVIEW]${NC} $*"; }
log_err()   { echo -e "${RED}[VIOLATION]${NC} $*"; }

echo "================================================================"
echo "  AgentOS Dynamic memcpy/memmove/memset Scanner (SEC-11)"
echo "  Project: ${PROJECT_ROOT}"
echo "  Mode:   $( $FIX_MODE && echo 'AUTO-FIX' || echo 'SCAN-ONLY' )"
echo "================================================================"
echo ""

# =========================================================================
# 规则引擎：判断一个 memcpy 类调用的安全性
# 返回: safe | review | violation
# =========================================================================
classify_memcpy_call() {
    local line="$1"

    # 已使用安全宏 → safe
    echo "$line" | grep -q 'AGENTOS_MEMCPY_SAFE' && { echo "safe"; return; }

    # 第三参数是 sizeof(...) → safe
    echo "$line" | grep -qE 'sizeof\s*\(.+\)' && { echo "safe"; return; }

    # 第三参数是纯数字常量 → safe
    local third_arg
    third_arg=$(echo "$line" | grep -oP '(?:memcpy|memmove|memset)\s*\([^,]+,\s*[^,]+,\s*\K[^)]+')
    if [[ -n "$third_arg" ]]; then
        if echo "$third_arg" | grep -qP '^(?:\d+|0x[0-9a-fA-F]+)$'; then
            echo "safe"; return
        fi

        # 第三参数是常量表达式（如 SIZE_MAX, PAGE_SIZE, BUFFER_SIZE 等）
        if echo "$third_arg" | grep -qP '^[A-Z_][A-Z0-9_]*$'; then
            # 可能是常量宏 → review（需确认宏定义值）
            echo "review"; return
        fi

        # 第三参数包含变量/函数调用 → violation
        if echo "$third_arg" | grep -qP '[a-z_]\w*\s*[\[(]' ; then
            echo "violation"; return
        fi

        # 其他情况 → review
        echo "review"; return
    fi

    echo "review"
}

# =========================================================================
# 主扫描逻辑
# =========================================================================
log_info "Scanning for memcpy/memmove/memset calls..."

while IFS= read -r -d '' file; do
    # 排除测试、示例、banned_functions 本身和 compliance 目录
    [[ "$file" == *"/tests/"* ]] && continue
    [[ "$file" == *"/examples/"* ]] && continue
    [[ "$file" == *"compliance"* ]] && continue
    [[ "$file" == *"banned_functions"* ]] && continue

    while IFS= read -r line; do
        # 只匹配 memcpy/memmove/memset 行
        echo "$line" | grep -qE '\b(memcpy|memmove|memset)\s*\(' || continue

        filepath=$(echo "$line" | cut -d: -f1)
        lineno=$(echo "$line" | cut -d: -f2)
        content=$(echo "$line" | cut -d: -f3-)

        result=$(classify_memcpy_call "$content")

        case "$result" in
            safe)
                ((total_safe++)) || true
                ;;
            review)
                ((total_review++)) || true
                REVIEW_NEEDED+=("$filepath:$lineno $content")
                log_warn "$filepath:$lineno needs manual review: $content"
                ;;
            violation)
                ((total_violations++)) || true
                VIOLATIONS+=("$filepath:$lineno $content")
                log_err "$filepath:$lineno DYNAMIC SIZE DETECTED: $content"

                # --fix 模式：生成建议的修复代码
                if $FIX_MODE; then
                    echo "  FIX SUGGESTION: Replace with AGENTOS_MEMCPY_SAFE(dst, dst_cap, src, src_size)"
                fi
                ;;
        esac
    done < <(grep -rn '\bmemcpy\b\|\bmemmove\b\|\bmemset\b' "$file" 2>/dev/null || true)
done < <(find "${PROJECT_ROOT}/agentos" -name "*.c" -print0 2>/dev/null)

# =========================================================================
# 汇总报告
# =========================================================================
echo ""
echo "================================================================"
echo "  Scan Results Summary"
echo "================================================================"
log_ok    "Safe (sizeof/constant):     $total_safe"
log_warn  "Needs manual review:         $total_review"
log_err   "Violations (dynamic size):  $total_violations"
echo ""

if [[ -n "$REPORT_FILE" ]]; then
    mkdir -p "$(dirname "$REPORT_FILE")"
    {
        echo '{'
        echo '  "timestamp": "'$(date -Iseconds)'",'
        echo '  "project": "AgentOS",'
        echo '  "version": "0.1.0",'
        echo '  "scan_type": "dynamic_memcpy",'
        echo '  "results": {'
        echo '    "safe": '$total_safe','
        echo '    "review_required": '$total_review','
        echo '    "violations": '$total_violations
        echo '  },'
        echo '  "violations": ['
        local first=true
        for v in "${VIOLATIONS[@]}"; do
            $first || echo ','
            first=false
            echo -n "    \"$v\""
        done
        $first || echo ""
        echo '  ],'
        echo '  "review_needed": ['
        first=true
        for r in "${REVIEW_NEEDED[@]}"; do
            $first || echo ','
            first=false
            echo -n "    \"$r\""
        done
        $first || echo ""
        echo '  ]'
        echo '}'
    } > "$REPORT_FILE"
    log_info "Report saved to: $REPORT_FILE"
fi

# =========================================================================
# 退出码
# =========================================================================
if [[ $total_violations -eq 0 ]]; then
    echo -e "${GREEN}No dynamic-size memcpy violations found!${NC}"
    exit 0
else
    echo -e "${RED}$total_violations dynamic-size memcpy violation(s) require attention${NC}"

    if [[ -n "${VIOLATIONS+x]}" ]] && [[ ${#VIOLATIONS[@]} -gt 0 ]]; then
        echo ""
        echo "--- Violation Details ---"
        for v in "${VIOLATIONS[@]}"; do
            echo "  $v"
        done
    fi

    exit 1
fi
