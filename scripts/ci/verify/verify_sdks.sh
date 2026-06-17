#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 SPHARX Ltd.
# SPDX-License-Identifier: Apache-2.0
#
# AgentOS SDK One-Click Build Verification Script
# Runs: tsc --noEmit + cargo build + go build ./... + pytest
# Usage:
#   ./verify_sdks.sh              # Interactive mode
#   ./verify_sdks.sh --ci         # CI mode (JUnit XML output + strict)
# Output: status lines (PASS/FAIL) + error summary
# Exit code: 0 if all pass, 1 if any fail

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

TS_DIR="$ROOT_DIR/sdk/typescript"
RUST_DIR="$ROOT_DIR/sdk/rust"
GO_DIR="$ROOT_DIR/sdk/go"
PYTHON_DIR="$ROOT_DIR/sdk/python"

PASS=0
FAIL=0
ERRORS=""
CI_MODE=false
RESULTS_DIR="/tmp"
START_TIME=$(date +%s)

while [[ $# -gt 0 ]]; do
    case "$1" in
        --ci)
            CI_MODE=true
            RESULTS_DIR="${CI_WORKSPACE:-/tmp}"
            shift
            ;;
        --results-dir)
            RESULTS_DIR="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--ci] [--results-dir <path>]"
            exit 2
            ;;
    esac
done

run_check() {
    local name="$1"
    local cmd="$2"
    local dir="$3"
    local log_file="${RESULTS_DIR}/verify_sdks_${name// /_}.log"

    printf "[......] %s" "$name"

    local start_ns=$(date +%s%N)

    if [ -d "$dir" ] && cd "$dir" && eval "$cmd" > "$log_file" 2>&1; then
        local end_ns=$(date +%s%N)
        local elapsed_ms=$(( (end_ns - start_ns) / 1000000 ))
        printf "\r[ PASS ] %-24s (%dms)\n" "$name" "$elapsed_ms"
        PASS=$((PASS + 1))
    else
        local end_ns=$(date +%s%N)
        local elapsed_ms=$(( (end_ns - start_ns) / 1000000 ))
        printf "\r[ FAIL ] %-24s (%dms)\n" "$name" "$elapsed_ms"
        FAIL=$((FAIL + 1))
        ERRORS="${ERRORS}--- ${name} FAILED ---\n"
        ERRORS="${ERRORS}$(head -30 "$log_file" 2>/dev/null || echo '(no output)')\n\n"
    fi
}

preflight_check() {
    echo "[.....] Preflight checks"
    local preflight_ok=true

    if ! command -v node &> /dev/null; then
        echo "\r[ WARN ] node not found, TypeScript check skipped" >&2
    fi

    if ! command -v cargo &> /dev/null; then
        echo "\r[ WARN ] cargo not found, Rust check skipped" >&2
    fi

    if ! command -v go &> /dev/null; then
        echo "\r[ WARN ] go not found, Go check skipped" >&2
    fi

    if ! command -v python3 &> /dev/null && ! command -v python &> /dev/null; then
        echo "\r[ WARN ] python not found, Python check skipped" >&2
    fi

    printf "\r[ READY] Preflight checks complete\n"
}

echo "========================================"
echo "  AgentOS SDK Build Verification"
echo "========================================"
echo ""

preflight_check

run_check "TypeScript" "npx tsc --noEmit" "$TS_DIR"
run_check "Rust" "cargo build 2>&1" "$RUST_DIR"
run_check "Go" "go build ./..." "$GO_DIR"

PYTHON_CMD="python3 -m pytest tests/test_plugin_lifecycle.py tests/test_integration_e2e.py tests/test_cross_platform.py -q"
if [ "$CI_MODE" = true ]; then
    PYTHON_CMD="$PYTHON_CMD --junitxml=${RESULTS_DIR}/sdk-python-junit.xml"
fi
run_check "Python" "$PYTHON_CMD" "$PYTHON_DIR"

END_TIME=$(date +%s)
ELAPSED=$((END_TIME - START_TIME))

echo ""
echo "========================================"
echo "  Results: ${PASS} PASS / ${FAIL} FAIL (${ELAPSED}s)"
echo "========================================"

if [ "$CI_MODE" = true ]; then
    cat > "${RESULTS_DIR}/sdk-verification-summary.json" <<EOF
{
  "pass": ${PASS},
  "fail": ${FAIL},
  "total": $((PASS + FAIL)),
  "elapsed_sec": ${ELAPSED}
}
EOF
fi

if [ "$FAIL" -gt 0 ]; then
    echo ""
    echo "Error Summary:"
    echo ""
    printf "%b" "$ERRORS"
    exit 1
fi

exit 0
