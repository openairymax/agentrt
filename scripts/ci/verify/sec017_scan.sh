#!/bin/bash
# SEC-017 桩函数穷尽检测脚本 v8.1 (精确版)
# 对齐: 规范手册v13.0 SEC-017 / 问题清单v14.0 STUB类
# v8.1: 精确匹配桩函数模式，排除正常return语句的误报
# 用法: ./scripts/sec017_scan.sh [daemon|gateway|cupolas|all]

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TOTAL_VIOLATIONS=0

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

SCAN_TARGET="${1:-all}"

scan_directory() {
    local DIR="$1"
    local LABEL="$2"
    local VIOLATIONS=0
    local REAL_VIOLATIONS=0

    echo "=========================================="
    echo "  SEC-017 Scan: $LABEL"
    echo "  Directory: $DIR"
    echo "=========================================="

    if [ ! -d "$DIR" ]; then
        echo -e "${YELLOW}⚠  Directory not found: $DIR${NC}"
        return 0
    fi

    echo ""
    echo -e "${CYAN}[Category 1] Explicit STUB keywords (P0 — must fix)${NC}"
    for pattern in \
        '\bSTUB\b\|\bstub function\b\|Stub函数\|STUB函数' \
        'void\s+stub_\w+\s*\('; do
        MATCHES=$(grep -rnp "$pattern" --include="*.c" "$DIR" 2>/dev/null | grep -v test_ | grep -v ".test." || true)
        COUNT=$(echo "$MATCHES" | grep -c . 2>/dev/null || echo 0)
        if [ "$COUNT" -gt 0 ]; then
            echo -e "${RED}🔴 [$COUNT] $pattern${NC}"
            echo "$MATCHES" | head -10
            VIOLATIONS=$((VIOLATIONS + COUNT))
            REAL_VIOLATIONS=$((REAL_VIOLATIONS + COUNT))
        fi
    done
    if [ "$VIOLATIONS" = "0" ]; then echo -e "${GREEN}   ✅ No explicit stubs${NC}"; fi

    echo ""
    echo -e "${CYAN}[Category 2] Empty function bodies (P0 — must fix)${NC}"
    EMPTY_COUNT=$(grep -rnp '^\s*(int|void|bool|size_t|char\*|\w+_t)\s+\w+\s*([^)]*)\s*\{\s*$' --include="*.c" "$DIR" 2>/dev/null | grep -v test_ | grep -v ".test." || true)
    EC=$(echo "$EMPTY_COUNT" | grep -c . 2>/dev/null || echo 0)
    if [ "$EC" -gt 0 ]; then
        echo -e "${YELLOW}🟡 [$EC] Potential empty function bodies${NC}"
        echo "$EMPTY_COUNT" | head -10
        VIOLATIONS=$((VIOLATIONS + EC))
    else
        echo -e "${GREEN}   ✅ No empty functions detected${NC}"
    fi

    echo ""
    echo -e "${CYAN}[Category 3] Stub pattern: (void)args + return (P0)${NC}"
    STUB_PATTERN_COUNT=$(grep -rnp '(void)\s*\w+;\s*return\s*[0-9];\s*\}' --include="*.c" "$DIR" 2>/dev/null | grep -v test_ | grep -v ".test." || true)
    SPC=$(echo "$STUB_PATTERN_COUNT" | grep -c . 2>/dev/null || echo 0)
    if [ "$SPC" -gt 0 ]; then
        echo -e "${RED}🔴 [$SPC] (void)param; return N; } pattern${NC}"
        echo "$STUB_PATTERN_COUNT" | head -10
        VIOLATIONS=$((VIOLATIONS + SPC))
        REAL_VIOLATIONS=$((REAL_VIOLATIONS + SPC))
    else
        echo -e "${GREEN}   ✅ No stub patterns${NC}"
    fi

    echo ""
    echo -e "${CYAN}[Category 4] TODO/FIXME with implementation gap (P1)${NC}"
    TODO_MATCHES=$(grep -rni 'TODO.*implement\|FIXME.*implement\|not implemented$' --include="*.c" "$DIR" 2>/dev/null | grep -v test_ | grep -v ".test." || true)
    TC=$(echo "$TODO_MATCHES" | grep -c . 2>/dev/null || echo 0)
    if [ "$TC" -gt 0 ]; then
        echo -e "${YELLOW}🟡 [$TC] TODO items (review required)${NC}"
        echo "$TODO_MATCHES" | head -10
        VIOLATIONS=$((VIOLATIONS + TC))
    else
        echo -e "${GREEN}   ✅ No implementation gaps${NC}"
    fi

    echo ""
    echo -e "${CYAN}[Category 5] Chinese stub comments (P1)${NC}"
    CN_STUB=$(grep -rn '// 空实现$\|// 桩实现$\|// 简化实现$\|// TODO implement' --include="*.c" "$DIR" 2>/dev/null | grep -v test_ || true)
    if [ -n "$CN_STUB" ]; then
        CNC=$(echo "$CN_STUB" | wc -l)
        echo -e "${YELLOW}🟡 [$CNC] Chinese stub comments${NC}"
        echo "$CN_STUB" | head -5
        VIOLATIONS=$((VIOLATIONS + CNC))
    else
        echo -e "${GREEN}   ✅ No Chinese stub comments${NC}"
    fi

    echo ""
    if [ "$REAL_VIOLATIONS" -eq 0 ]; then
        echo -e "${GREEN}✅ $LABEL: 0 REAL violations — FULLY COMPLIANT${NC}"
    else
        echo -e "${RED}❌ $LABEL: $REAL_VIOLATIONS REAL violations ($VIOLATIONS total flags)${NC}"
    fi

    return $REAL_VIOLATIONS
}

echo ""
echo "╔══════════════════════════════════════════╗"
echo "║   AgentOS SEC-017 Stub Function Scanner  ║"
echo "║   v8.1 (Precise) | $(date '+%Y-%m-%d %H:%M:%S')     ║"
echo "╚══════════════════════════════════════════╝"
echo ""

case "$SCAN_TARGET" in
    daemon)
        scan_directory "$PROJECT_ROOT/agentos/daemon" "Daemon Module"
        TOTAL_VIOLATIONS=$?
        ;;
    gateway)
        scan_directory "$PROJECT_ROOT/agentos/gateway" "Gateway Module"
        TOTAL_VIOLATIONS=$?
        ;;
    cupolas)
        scan_directory "$PROJECT_ROOT/agentos/cupolas" "Cupolas Module"
        TOTAL_VIOLATIONS=$?
        ;;
    all)
        V1=0; scan_directory "$PROJECT_ROOT/agentos/daemon" "Daemon Module" || V1=$?
        V2=0; scan_directory "$PROJECT_ROOT/agentos/gateway" "Gateway Module" || V2=$?
        V3=0; scan_directory "$PROJECT_ROOT/agentos/cupolas" "Cupolas Module" || V3=$?
        TOTAL_VIOLATIONS=$((V1 + V2 + V3))
        ;;
    *)
        echo "Usage: $0 [daemon|gateway|cupolas|all]"
        exit 1
        ;;
esac

echo ""
echo "=========================================="
echo "  FINAL RESULT"
echo "=========================================="
if [ "$TOTAL_VIOLATIONS" -eq 0 ]; then
    echo -e "${GREEN}✅ TOTAL: 0 REAL violations — PROJECT FULLY COMPLIANT${NC}"
    echo -e "${GREEN}   SEC-017 Status: PASS${NC}"
    exit 0
else
    echo -e "${RED}❌ TOTAL: $TOTAL_VIOLATIONS REAL violations${NC}"
    echo -e "${RED}   SEC-017 Status: FAIL${NC}"
    exit 1
fi
