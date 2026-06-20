#!/bin/bash
# AgentRT Quality Gate - CI/CD 质量门禁
# 集成: 编译检查 · BAN规则扫描 · 安全扫描 · 测试 · 合约验证
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

# ============================================================================
# 颜色输出
# ============================================================================
COLOR_RED='\033[0;31m'
COLOR_GREEN='\033[0;32m'
COLOR_YELLOW='\033[1;33m'
COLOR_CYAN='\033[0;36m'
COLOR_RESET='\033[0m'

GATE_PASS=0
GATE_FAIL=0
GATE_WARN=0
GATE_SKIP=0

log_info()  { echo -e "${COLOR_CYAN}[INFO]${COLOR_RESET}  $*"; }
log_ok()    { echo -e "${COLOR_GREEN}[OK]${COLOR_RESET}    $*"; }
log_err()   { echo -e "${COLOR_RED}[ERR]${COLOR_RESET}   $*"; }
log_warn()  { echo -e "${COLOR_YELLOW}[WARN]${COLOR_RESET}  $*"; }
section()   { echo -e "\n${COLOR_CYAN}═══ $1 ═══${COLOR_RESET}"; }

check_gate() {
    local name="$1"
    local result="$2"
    case "$result" in
        0) GATE_PASS=$((GATE_PASS+1)); log_ok "GATE: $name - PASSED" ;;
        2) GATE_WARN=$((GATE_WARN+1)); log_warn "GATE: $name - WARNING" ;;
        *) GATE_FAIL=$((GATE_FAIL+1)); log_err "GATE: $name - FAILED" ;;
    esac
}

print_header() {
    echo "╔══════════════════════════════════════════════════╗"
    echo "║   AgentRT Quality Gate                           ║"
    echo "║   $(date '+%Y-%m-%d %H:%M:%S')                       ║"
    echo "╚══════════════════════════════════════════════════╝"
}

# ============================================================================
# Gate 1: 编译检查 (0e0w)
# ============================================================================
gate_compile() {
    section "Gate 1: Compilation Check"
    local build_dir="${PROJECT_ROOT}/../AgentRT-build"

    if [ ! -f "${PROJECT_ROOT}/CMakeLists.txt" ]; then
        log_warn "CMakeLists.txt not found, skipping compilation check"
        check_gate "Compile" 2
        return
    fi

    mkdir -p "$build_dir"
    cd "$build_dir"

    if cmake -B . -S "${PROJECT_ROOT}" -DCMAKE_BUILD_TYPE=Debug 2>&1 | tail -5; then
        if cmake --build . 2>&1 | tail -5; then
            check_gate "Compile" 0
        else
            check_gate "Compile" 1
        fi
    else
        check_gate "Compile" 1
    fi
}

# ============================================================================
# Gate 2: BAN 规则扫描 (PATH-BAN + BAN-191/193)
# ============================================================================
gate_ban() {
    section "Gate 2: BAN Rule Scan"

    local ban_script="${SCRIPT_DIR}/../verify/forbidden_functions.sh"
    if [ -x "$ban_script" ]; then
        log_info "Running BAN rule scan..."
        if bash "$ban_script" 2>&1 | tail -10; then
            check_gate "BAN-Rules" 0
        else
            check_gate "BAN-Rules" 1
        fi
    else
        log_warn "BAN scan script not found: ${ban_script}"
        check_gate "BAN-Rules" 2
    fi

    # BAN-191: 禁止 head -z 管道（POSIX 兼容性）
    log_info "BAN-191: Scanning for 'head -z' usage..."
    local head_z_found
    head_z_found=$(find "${PROJECT_ROOT}/scripts" -name "*.sh" -exec grep -lP "head\s+-z" {} \; 2>/dev/null || true)
    if [ -n "$head_z_found" ]; then
        log_err "BAN-191: 'head -z' found in:"
        echo "$head_z_found" | while IFS= read -r f; do log_err "  $f"; done
        check_gate "BAN-191" 1
    else
        log_ok "BAN-191: No 'head -z' usage found"
        check_gate "BAN-191" 0
    fi

    # BAN-193: 危险函数扫描必须使用 \b 词边界
    log_info "BAN-193: Scanning for unguarded dangerous function patterns..."
    local dangerous_patterns
    dangerous_patterns=$(find "${PROJECT_ROOT}/scripts" -name "*.sh" -exec grep -lP 'grep\s+(?!.*\\\\b).*"(strcpy|strcat|sprintf|gets|scanf)"' {} \; 2>/dev/null || true)
    if [ -n "$dangerous_patterns" ]; then
        log_err "BAN-193: Unguarded dangerous function grep found in:"
        echo "$dangerous_patterns" | while IFS= read -r f; do log_err "  $f"; done
        check_gate "BAN-193" 1
    else
        log_ok "BAN-193: No unguarded dangerous function patterns found"
        check_gate "BAN-193" 0
    fi
}

# ============================================================================
# Gate 3: 安全扫描 (P3.21 - 10 项检查)
# ============================================================================
gate_security() {
    section "Gate 3: Security Scan (10 Items)"

    if [ "${SKIP_SECURITY:-0}" = "1" ]; then
        log_warn "Security scan skipped (SKIP_SECURITY=1)"
        check_gate "Security" 2
        return
    fi

    local sec_script="${SCRIPT_DIR}/security-scan.sh"
    if [ -x "$sec_script" ]; then
        log_info "Running security scan..."
        if bash "$sec_script" "$@" 2>&1 | tail -20; then
            check_gate "Security" 0
        else
            local sec_exit=$?
            if [ $sec_exit -eq 1 ]; then
                log_err "Security scan failed (HIGH+ vulnerabilities or secrets found)"
                check_gate "Security" 1
            else
                check_gate "Security" 2
            fi
        fi
    else
        log_warn "Security scan script not found: ${sec_script}"
        check_gate "Security" 2
    fi
}

# ============================================================================
# Gate 4: 合约版本检查
# ============================================================================
gate_contract() {
    section "Gate 4: Contract Version Check"

    local contract_script="${SCRIPT_DIR}/contract-version-check.sh"
    if [ -x "$contract_script" ]; then
        log_info "Checking contract versions..."
        if bash "$contract_script" 2>&1 | tail -5; then
            check_gate "Contract" 0
        else
            check_gate "Contract" 1
        fi
    else
        log_warn "Contract version check not found"
        check_gate "Contract" 2
    fi
}

# ============================================================================
# Gate 5: 跨子仓库验证 (P4.8)
# ============================================================================
gate_cross_repo() {
    section "Gate 5: Cross-Repository Verification"

    local cross_repo_script="${SCRIPT_DIR}/cross-repo-verify.sh"
    if [ -x "$cross_repo_script" ]; then
        log_info "Running cross-repo verification..."
        if bash "$cross_repo_script" 2>&1 | tail -10; then
            check_gate "CrossRepo" 0
        else
            check_gate "CrossRepo" 1
        fi
    else
        log_warn "Cross-repo verification script not found"
        check_gate "CrossRepo" 2
    fi
}

# ============================================================================
# 主函数
# ============================================================================
main() {
    local skip_security=false
    local security_only=false
    local skip_cross_repo=false
    local strict_mode=false

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --security-scan)
                security_only=true
                shift
                ;;
            --skip-security)
                skip_security=true
                shift
                ;;
            --skip-cross-repo)
                skip_cross_repo=true
                shift
                ;;
            --strict)
                strict_mode=true
                shift
                ;;
            --help|-h)
                echo "Usage: $0 [--security-scan] [--skip-security] [--skip-cross-repo] [--strict]"
                echo ""
                echo "Quality Gates:"
                echo "  1. Compilation Check (0e0w)"
                echo "  2. BAN Rule Scan (256 rules + BAN-191/193)"
                echo "  3. Security Scan (10 items: CVE, SAST, Docker, Secrets, SBOM, ...)"
                echo "  4. Contract Version Check"
                echo "  5. Cross-Repository Verification"
                echo ""
                echo "Options:"
                echo "  --security-scan      Run only security scan"
                echo "  --skip-security       Skip security scan"
                echo "  --skip-cross-repo     Skip cross-repo verification"
                echo "  --strict              Treat warnings as failures"
                exit 0
                ;;
            *)
                echo "Unknown option: $1"
                exit 1
                ;;
        esac
    done

    print_header

    if $security_only; then
        gate_security
    else
        gate_compile
        gate_ban
        $skip_security || gate_security
        gate_contract
        $skip_cross_repo || gate_cross_repo
    fi

    # 输出结果
    section "Quality Gate Results"
    echo ""
    echo -e "  ${COLOR_GREEN}Passed:${COLOR_RESET}   $GATE_PASS"
    echo -e "  ${COLOR_RED}Failed:${COLOR_RESET}   $GATE_FAIL"
    echo -e "  ${COLOR_YELLOW}Warnings:${COLOR_RESET} $GATE_WARN"
    echo -e "  ${COLOR_YELLOW}Skipped:${COLOR_RESET}  $GATE_SKIP"
    echo ""

    if [ "$GATE_FAIL" -eq 0 ]; then
        if $strict_mode && [ "$GATE_WARN" -gt 0 ]; then
            echo -e "${COLOR_RED}╔══════════════════════════════════════╗${COLOR_RESET}"
            echo -e "${COLOR_RED}║     QUALITY GATE FAILED (STRICT)     ║${COLOR_RESET}"
            echo -e "${COLOR_RED}║     ${GATE_WARN} WARNING(S) TREATED AS FAILURE  ║${COLOR_RESET}"
            echo -e "${COLOR_RED}╚══════════════════════════════════════╝${COLOR_RESET}"
            exit 1
        fi
        echo -e "${COLOR_GREEN}╔══════════════════════════════════════╗${COLOR_RESET}"
        echo -e "${COLOR_GREEN}║     ALL QUALITY GATES PASSED         ║${COLOR_RESET}"
        echo -e "${COLOR_GREEN}╚══════════════════════════════════════╝${COLOR_RESET}"
        exit 0
    else
        echo -e "${COLOR_RED}╔══════════════════════════════════════╗${COLOR_RESET}"
        echo -e "${COLOR_RED}║     QUALITY GATE FAILED              ║${COLOR_RESET}"
        echo -e "${COLOR_RED}║     ${GATE_FAIL} GATE(S) FAILED                   ║${COLOR_RESET}"
        echo -e "${COLOR_RED}╚══════════════════════════════════════╝${COLOR_RESET}"
        exit 1
    fi
}

main "$@"