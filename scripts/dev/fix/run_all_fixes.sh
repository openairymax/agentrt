#!/usr/bin/env bash
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# run_all_fixes.sh — 批量执行所有 auto-fixable BAN 规则修复脚本
# 用法: bash scripts/dev/fix/run_all_fixes.sh [--dry-run] [--verbose]
# P0.15: BAN 基线扫描 + 自动修复

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
UTILS_DIR="${SCRIPT_DIR}/../utils"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"

DRY_RUN=false
VERBOSE=false

for arg in "$@"; do
    case "$arg" in
        --dry-run)  DRY_RUN=true ;;
        --verbose)  VERBOSE=true ;;
        -h|--help)
            echo "Usage: $0 [--dry-run] [--verbose]"
            echo "  --dry-run  : Show what would be fixed without making changes"
            echo "  --verbose  : Show detailed output"
            exit 0
            ;;
    esac
done

echo "=== AgentRT BAN Auto-Fix Runner ==="
echo "Project root: ${PROJECT_ROOT}"
echo "Utils dir:    ${UTILS_DIR}"
echo "Dry run:      ${DRY_RUN}"
echo ""

TOTAL_FIXES=0
FIXES_APPLIED=0
FIXES_FAILED=0

# 修复脚本列表（按依赖顺序）
declare -a FIX_SCRIPTS=(
    "fix_agentos_efail.py"
    "fix_agentos_efail_macro.py"
    "fix_return_neg_N.py"
    "fix_strncpy.py"
    "fix_sec22.py"
    "fix_error_handle.py"
    "fix_error_push_ex_order.py"
    "fix_braces_and_codes.py"
    "fix_indent_and_codes.py"
    "fix_includes_and_braces.py"
)

cd "${PROJECT_ROOT}"

for script in "${FIX_SCRIPTS[@]}"; do
    script_path="${UTILS_DIR}/${script}"
    if [[ ! -f "${script_path}" ]]; then
        echo "[SKIP] ${script} — not found"
        continue
    fi

    ((TOTAL_FIXES++)) || true
    echo -n "[RUN ] ${script} ... "

    if [[ "${DRY_RUN}" == "true" ]]; then
        echo "DRY-RUN (skipped)"
        continue
    fi

    if python3 "${script_path}" 2>&1 | tail -1 | grep -qiE "error|fail"; then
        echo "FAILED"
        ((FIXES_FAILED++)) || true
    else
        echo "OK"
        ((FIXES_APPLIED++)) || true
    fi
done

echo ""
echo "=== Summary ==="
echo "Total scripts:  ${TOTAL_FIXES}"
echo "Applied:        ${FIXES_APPLIED}"
echo "Failed:         ${FIXES_FAILED}"
echo ""

if [[ "${FIXES_FAILED}" -gt 0 ]]; then
    echo "Some fixes failed. Review output above."
    exit 1
fi

echo "All auto-fixable BAN violations have been processed."
echo "Run 'bash scripts/ci/pipeline/quality-gate.sh' to verify."
