#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 SPHARX Ltd.
# SPDX-License-Identifier: GPL-3.0-or-later
#
# AgentOS SDK One-Click Build Verification Script
# Runs: tsc --noEmit + cargo build + go build ./... + pytest
# Output: 4 status lines (PASS/FAIL) + error summary
# Exit code: 0 if all pass, 1 if any fail

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

TS_DIR="$ROOT_DIR/agentos/toolkit/typescript"
RUST_DIR="$ROOT_DIR/agentos/toolkit/rust"
GO_DIR="$ROOT_DIR/agentos/toolkit/go"
PYTHON_DIR="$ROOT_DIR/agentos/toolkit/python"

PASS=0
FAIL=0
ERRORS=""

run_check() {
    local name="$1"
    local cmd="$2"
    local dir="$3"

    printf "[......] %s" "$name"

    if cd "$dir" && eval "$cmd" > /tmp/verify_sdks_${name}.log 2>&1; then
        printf "\r[ PASS ] %s\n" "$name"
        PASS=$((PASS + 1))
    else
        printf "\r[ FAIL ] %s\n" "$name"
        FAIL=$((FAIL + 1))
        ERRORS="${ERRORS}--- ${name} FAILED ---\n"
        ERRORS="${ERRORS}$(head -20 /tmp/verify_sdks_${name}.log)\n\n"
    fi
}

echo "========================================"
echo "  AgentOS SDK Build Verification"
echo "========================================"
echo ""

run_check "TypeScript" "npx tsc --noEmit" "$TS_DIR"
run_check "Rust" "cargo build 2>&1" "$RUST_DIR"
run_check "Go" "go build ./..." "$GO_DIR"
run_check "Python" "python3 -m pytest tests/test_plugin_lifecycle.py tests/test_integration_e2e.py tests/test_cross_platform.py -q -o addopts=" "$PYTHON_DIR"

echo ""
echo "========================================"
echo "  Results: ${PASS} PASS / ${FAIL} FAIL"
echo "========================================"

if [ "$FAIL" -gt 0 ]; then
    echo ""
    echo "Error Summary:"
    echo ""
    printf "%b" "$ERRORS"
    exit 1
fi

exit 0
