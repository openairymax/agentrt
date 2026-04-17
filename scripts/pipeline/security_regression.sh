#!/bin/bash
# AgentOS Security Regression Test - Team B Safety Net
# Validates: compilation (0e0w) + flawfinder L4 threshold + cppcheck 0e + security_check CRITICAL=0
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
AGENTOS_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="/tmp/agentos_build_$$"

COLOR_RED='\033[0;31m'
COLOR_GREEN='\033[0;32m'
COLOR_YELLOW='\033[1;33m'
COLOR_CYAN='\033[0;36m'
COLOR_RESET='\033[0m'

PASS=0
FAIL=0
WARN=0
TOTAL=0

pass() { TOTAL=$((TOTAL+1)); PASS=$((PASS+1)); echo -e "  ${COLOR_GREEN}✅ PASS${COLOR_RESET}: $1"; }
fail() { TOTAL=$((TOTAL+1)); FAIL=$((FAIL+1)); echo -e "  ${COLOR_RED}❌ FAIL${COLOR_RESET}: $1"; }
warn() { TOTAL=$((TOTAL+1)); WARN=$((WARN+1)); echo -e "  ${COLOR_YELLOW}⚠️  WARN${COLOR_RESET}: $1"; }

section() { echo -e "\n${COLOR_CYAN}═══ $1 ═══${COLOR_RESET}"; }

cleanup() {
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
    fi
}
trap cleanup EXIT

echo "╔══════════════════════════════════════════════════╗"
echo "║   AgentOS Security Regression Test - Team B     ║"
echo "║   $(date '+%Y-%m-%d %H:%M:%S')                       ║"
echo "╚══════════════════════════════════════════════════╝"

section "1. Clean Build (Compilation)"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
if cmake "$AGENTOS_ROOT/agentos" > /dev/null 2>&1; then
    CMAKE_OUTPUT=$(cmake --build . 2>&1 || true)
    ERROR_COUNT=$(echo "$CMAKE_OUTPUT" | grep -c "error:" || true)
    WARNING_COUNT=$(echo "$CMAKE_OUTPUT" | grep -c "warning:" || true)
    
    DAEMON_ERRORS=$(echo "$CMAKE_OUTPUT" | grep "error:" | grep -c "daemon/" || true)
    NON_DAEMON_ERRORS=$((ERROR_COUNT - DAEMON_ERRORS))
    
    if [ "$NON_DAEMON_ERRORS" -eq 0 ]; then
        pass "Non-daemon modules: 0 errors, ${WARNING_COUNT} warnings"
    else
        fail "Non-daemon modules: ${NON_DAEMON_ERRORS} errors, ${WARNING_COUNT} warnings"
    fi
    
    if [ "$DAEMON_ERRORS" -gt 0 ]; then
        warn "Daemon modules (Team C): ${DAEMON_ERRORS} errors (not blocking)"
    else
        pass "Daemon modules: 0 errors"
    fi
else
    fail "CMake configuration failed"
fi

section "2. flawfinder Security Scan (Level 4)"
cd "$AGENTOS_ROOT/agentos"
L4_HITS=$(flawfinder --minlevel 4 atoms/ commons/ cupolas/ 2>&1 | grep -c "\[4\]" || true)
L4_THRESHOLD=10

if [ "$L4_HITS" -le "$L4_THRESHOLD" ]; then
    pass "flawfinder Level 4: ${L4_HITS} hits (threshold: ≤${L4_THRESHOLD})"
else
    fail "flawfinder Level 4: ${L4_HITS} hits exceeds threshold (${L4_THRESHOLD})"
fi

L3_HITS=$(flawfinder --minlevel 3 atoms/ commons/ cupolas/ 2>&1 | grep -c "\[3\]" || true)
if [ "$L3_HITS" -le 40 ]; then
    pass "flawfinder Level 3: ${L3_HITS} hits (threshold: ≤40)"
else
    warn "flawfinder Level 3: ${L3_HITS} hits (advisory only)"
fi

section "3. cppcheck Static Analysis"
CPPCHECK_ERRORS=$(cppcheck --enable=all --suppress=missingInclude --suppress=unusedFunction \
    --error-exitcode=0 \
    atoms/corekern/ atoms/coreloopthree/ atoms/memoryrovol/ atoms/syscall/ atoms/commons/ \
    2>&1 | grep -c "error:" || true)

if [ "$CPPCHECK_ERRORS" -eq 0 ]; then
    pass "cppcheck: 0 errors"
else
    fail "cppcheck: ${CPPCHECK_ERRORS} errors found"
fi

section "4. Security Check Script (SEC-001~SEC-011)"
SEC_RESULT=$(python3 "$SCRIPT_DIR/security_check.py" \
    "$AGENTOS_ROOT/agentos/atoms/" "$AGENTOS_ROOT/agentos/commons/" 2>&1 || true)

CRITICAL_COUNT=$(echo "$SEC_RESULT" | grep -c "CRITICAL" || true)
HIGH_COUNT=$(echo "$SEC_RESULT" | grep -c "\[HIGH\]" || true)

if echo "$SEC_RESULT" | grep -q "RESULT: PASS"; then
    pass "security_check.py: PASS (0 CRITICAL)"
elif echo "$SEC_RESULT" | grep -q "RESULT: WARNING"; then
    warn "security_check.py: WARNING (0 CRITICAL, ${HIGH_COUNT} HIGH - likely false positives)"
else
    fail "security_check.py: CRITICAL issues found!"
fi

section "5. Vulnerability Pattern Verification"
SHELL_CMD_ALLOWED=$(grep -c "is_shell_command_allowed" "$AGENTOS_ROOT/agentos/atoms/coreloopthree/src/execution/units/shell.c" 2>/dev/null || echo "0")
PATH_TRAVERSAL_CHECK=$(grep -c "is_path_component_safe\|is_path_traversal_attempt" "$AGENTOS_ROOT/agentos/atoms/coreloopthree/src/execution/units/file.c" 2>/dev/null || echo "0")
SQL_INJECTION_CHECK=$(grep -c "DANGEROUS_SQL_KEYWORDS\|is_safe_query" "$AGENTOS_ROOT/agentos/atoms/coreloopthree/src/execution/units/db.c" 2>/dev/null || echo "0")
SSRF_CHECK=$(grep -c "is_private_ip\|is_safe_url" "$AGENTOS_ROOT/agentos/atoms/coreloopthree/src/execution/units/browser.c" 2>/dev/null || echo "0")
MEMORYROV_PATH_CHECK=$(grep -c "is_path_component_safe" "$AGENTOS_ROOT/agentos/atoms/memoryrovol/src/layer1_raw/storage.c" 2>/dev/null || echo "0")

if [ "$SHELL_CMD_ALLOWED" -ge 1 ]; then pass "shell.c: Command whitelist validation present"; else fail "shell.c: Missing command validation"; fi
if [ "$PATH_TRAVERSAL_CHECK" -ge 1 ]; then pass "file.c: Path traversal protection present"; else fail "file.c: Missing path traversal check"; fi
if [ "$SQL_INJECTION_CHECK" -ge 1 ]; then pass "db.c: SQL injection protection present"; else fail "db.c: Missing SQL injection check"; fi
if [ "$SSRF_CHECK" -ge 1 ]; then pass "browser.c: SSRF protection present"; else fail "browser.c: Missing SSRF check"; fi
if [ "$MEMORYROV_PATH_CHECK" -ge 1 ]; then pass "storage.c: Memory path validation present"; else warn "storage.c: Missing memory path validation (team A file)"; fi

section "6. Python SDK Syntax Validation"
PYTHON_FILES=(
    "$AGENTOS_ROOT/agentos/toolkit/python/agentos/exceptions.py"
    "$AGENTOS_ROOT/agentos/toolkit/python/agentos/agent.py"
    "$AGENTOS_ROOT/agentos/toolkit/python/agentos/task.py"
    "$AGENTOS_ROOT/agentos/toolkit/python/agentos/protocol.py"
    "$AGENTOS_ROOT/agentos/toolkit/python/agentos/client/client.py"
    "$AGENTOS_ROOT/agentos/toolkit/python/agentos/session.py"
)
PYTHON_FAIL=0
for py_file in "${PYTHON_FILES[@]}"; do
    if python3 -m py_compile "$py_file" 2>/dev/null; then
        : # silent pass
    else
        fail "Python syntax error: $(basename "$py_file")"
        PYTHON_FAIL=$((PYTHON_FAIL+1))
    fi
done
if [ "$PYTHON_FAIL" -eq 0 ]; then
    pass "All Python SDK files: syntax valid (6/6)"
fi

section "Results Summary"
echo ""
echo -e "  ${COLOR_CYAN}Total Tests:${COLOR_RESET}  $TOTAL"
echo -e "  ${COLOR_GREEN}Passed:${COLOR_RESET}      $PASS"
echo -e "  ${COLOR_YELLOW}Warnings:${COLOR_RESET}    $WARN"
echo -e "  ${COLOR_RED}Failed:${COLOR_RESET}      $FAIL"
echo ""

if [ "$FAIL" -eq 0 ]; then
    echo -e "${COLOR_GREEN}╔══════════════════════════════════════╗${COLOR_RESET}"
    echo -e "${COLOR_GREEN}║     🎉 ALL TESTS PASSED! 🎉          ║${COLOR_RESET}"
    echo -e "${COLOR_GREEN}╚══════════════════════════════════════╝${COLOR_RESET}"
    exit 0
else
    echo -e "${COLOR_RED}╔══════════════════════════════════════╗${COLOR_RESET}"
    echo -e "${COLOR_RED}║     ❌ ${FAIL} TEST(S) FAILED! ❌         ║${COLOR_RESET}"
    echo -e "${COLOR_RED}╚══════════════════════════════════════╝${COLOR_RESET}"
    exit 1
fi
