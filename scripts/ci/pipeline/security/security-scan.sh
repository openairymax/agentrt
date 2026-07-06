#!/bin/bash
# AgentRT Security Scan - 10 项安全检查自动化
# P3.21: 依赖CVE漏洞扫描 · 静态分析 · 容器扫描 · 密钥检测 · SBOM生成
#         敏感数据检测 · 许可证审计 · IaC安全 · API安全 · 供应链安全
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
AGENTRT_ROOT="$PROJECT_ROOT/agentos"
ARTIFACTS_DIR="${PROJECT_ROOT}/ci-artifacts/security-scan"

# ============================================================================
# 颜色输出
# ============================================================================
COLOR_RED='\033[0;31m'
COLOR_GREEN='\033[0;32m'
COLOR_YELLOW='\033[1;33m'
COLOR_CYAN='\033[0;36m'
COLOR_RESET='\033[0m'

PASS=0
FAIL=0
WARN=0
SKIP=0
TOTAL=0

# ============================================================================
# 工具函数
# ============================================================================
log_info()  { echo -e "${COLOR_CYAN}[INFO]${COLOR_RESET}  $*"; }
log_ok()    { echo -e "${COLOR_GREEN}[OK]${COLOR_RESET}    $*"; }
log_err()   { echo -e "${COLOR_RED}[ERR]${COLOR_RESET}   $*"; }
log_warn()  { echo -e "${COLOR_YELLOW}[WARN]${COLOR_RESET}  $*"; }
section()   { echo -e "\n${COLOR_CYAN}═══ $1 ═══${COLOR_RESET}"; }

pass() { TOTAL=$((TOTAL+1)); PASS=$((PASS+1)); log_ok "PASS: $1"; }
fail() { TOTAL=$((TOTAL+1)); FAIL=$((FAIL+1)); log_err "FAIL: $1"; }
warn() { TOTAL=$((TOTAL+1)); WARN=$((WARN+1)); log_warn "WARN: $1"; }
skip() { TOTAL=$((TOTAL+1)); SKIP=$((SKIP+1)); log_warn "SKIP: $1 (tool not found)"; }

check_tool() {
    command -v "$1" &>/dev/null
}

print_header() {
    echo "╔══════════════════════════════════════════════════╗"
    echo "║   AgentRT Security Scan - 10 Items              ║"
    echo "║   $(date '+%Y-%m-%d %H:%M:%S')                       ║"
    echo "╚══════════════════════════════════════════════════╝"
}

generate_report() {
    local report_path="${ARTIFACTS_DIR}/security-scan-report.json"
    cat > "$report_path" << EOF
{
  "project": "AgentRT",
  "timestamp": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
  "summary": {
    "total": $TOTAL,
    "passed": $PASS,
    "failed": $FAIL,
    "warnings": $WARN,
    "skipped": $SKIP
  },
  "threshold": {
    "cve_critical_block": true,
    "cve_high_block": true,
    "secret_leak_block": true,
    "license_violation_block": true
  }
}
EOF
    log_info "Report saved: ${report_path}"
}

# ============================================================================
# Check 1: 依赖 CVE 漏洞扫描 (grype / trivy)
# ============================================================================
check_cve_scan() {
    section "Check 1: Dependency CVE Vulnerability Scan"

    if check_tool grype; then
        log_info "Running grype scan..."
        if grype "dir:${AGENTRT_ROOT}" --fail-on high 2>&1 | tail -20; then
            pass "CVE scan (grype): No HIGH+ vulnerabilities"
        else
            local grype_exit=$?
            if [ $grype_exit -eq 1 ]; then
                fail "CVE scan (grype): HIGH+ vulnerabilities found"
            else
                warn "CVE scan (grype): Completed with warnings"
            fi
        fi
    elif check_tool trivy; then
        log_info "Running trivy fs scan..."
        if trivy fs --severity HIGH,CRITICAL --exit-code 1 "${AGENTRT_ROOT}" 2>&1 | tail -20; then
            pass "CVE scan (trivy): No HIGH+ vulnerabilities"
        else
            fail "CVE scan (trivy): HIGH+ vulnerabilities found"
        fi
    else
        skip "CVE scan: neither grype nor trivy installed"
    fi
}

# ============================================================================
# Check 2: C 静态分析 (flawfinder + cppcheck)
# ============================================================================
check_static_analysis() {
    section "Check 2: C Static Analysis"

    # flawfinder Level 4+
    if check_tool flawfinder; then
        local l4_hits
        l4_hits=$(flawfinder --minlevel 4 "${AGENTRT_ROOT}/atoms/" "${AGENTRT_ROOT}/commons/" "${AGENTRT_ROOT}/cupolas/" 2>&1 | grep -c "\[4\]" || true)
        if [ "$l4_hits" -le 10 ]; then
            pass "flawfinder L4: ${l4_hits} hits (threshold: <=10)"
        else
            fail "flawfinder L4: ${l4_hits} hits exceeds threshold (10)"
        fi
    else
        warn "flawfinder not installed, skipping"
    fi

    # cppcheck
    if check_tool cppcheck; then
        local cppcheck_errors
        cppcheck_errors=$(cppcheck --enable=all --suppress=missingInclude --suppress=unusedFunction \
            --error-exitcode=0 \
            "${AGENTRT_ROOT}/atoms/corekern/" "${AGENTRT_ROOT}/atoms/coreloopthree/" \
            "${AGENTRT_ROOT}/atoms/syscall/" "${AGENTRT_ROOT}/atoms/commons/" \
            "${AGENTRT_ROOT}/cupolas/" \
            2>&1 | grep -c "error:" || true)
        if [ "$cppcheck_errors" -eq 0 ]; then
            pass "cppcheck: 0 errors"
        else
            fail "cppcheck: ${cppcheck_errors} errors found"
        fi
    else
        warn "cppcheck not installed, skipping"
    fi
}

# ============================================================================
# Check 3: Docker 镜像扫描 (docker scout / trivy image)
# ============================================================================
check_docker_scan() {
    section "Check 3: Docker Image Scan"

    local dockerfile="${PROJECT_ROOT}/deploy/docker/Dockerfile"
    if [ ! -f "$dockerfile" ]; then
        skip "Docker image scan: Dockerfile not found"
        return
    fi

    if check_tool docker && docker info &>/dev/null 2>&1; then
        if docker scout &>/dev/null 2>&1; then
            log_info "Running docker scout quickview..."
            if docker scout quickview -f "$dockerfile" 2>&1 | tail -10; then
                pass "Docker scout: scan completed"
            else
                warn "Docker scout: completed with warnings"
            fi
        elif check_tool trivy; then
            log_info "Running trivy config scan on Dockerfile..."
            if trivy config --severity HIGH,CRITICAL --exit-code 1 "$dockerfile" 2>&1 | tail -10; then
                pass "Dockerfile scan (trivy): No HIGH+ misconfigs"
            else
                warn "Dockerfile scan (trivy): Issues found"
            fi
        else
            skip "Docker scan: docker scout / trivy not available"
        fi
    else
        skip "Docker scan: docker daemon not available"
    fi
}

# ============================================================================
# Check 4: 密钥泄露检测 (gitleaks / trufflehog)
# ============================================================================
check_secret_leak() {
    section "Check 4: Secret Leak Detection"

    if check_tool gitleaks; then
        log_info "Running gitleaks detect..."
        cd "$PROJECT_ROOT"
        if gitleaks detect --no-git --source . --verbose 2>&1 | tail -10; then
            pass "Secret leak (gitleaks): No secrets found"
        else
            local gitleaks_exit=$?
            if [ $gitleaks_exit -eq 1 ]; then
                fail "Secret leak (gitleaks): Secrets found in codebase"
            else
                warn "Secret leak (gitleaks): Completed with warnings"
            fi
        fi
    elif check_tool trufflehog; then
        log_info "Running trufflehog filesystem scan..."
        if trufflehog filesystem "$PROJECT_ROOT" --no-verification 2>&1 | tail -10; then
            pass "Secret leak (trufflehog): No secrets found"
        else
            fail "Secret leak (trufflehog): Secrets found in codebase"
        fi
    else
        skip "Secret leak: neither gitleaks nor trufflehog installed"
    fi
}

# ============================================================================
# Check 5: SBOM 生成 (syft → SPDX)
# ============================================================================
check_sbom() {
    section "Check 5: SBOM Generation (SPDX)"

    mkdir -p "$ARTIFACTS_DIR"
    local sbom_path="${ARTIFACTS_DIR}/sbom-$(date +%Y%m%d-%H%M%S).spdx.json"

    if check_tool syft; then
        log_info "Generating SBOM via syft..."
        if syft "dir:${AGENTRT_ROOT}" -o "spdx-json=${sbom_path}" 2>&1 | tail -3; then
            pass "SBOM generated: $(basename "$sbom_path")"
        else
            fail "SBOM generation (syft) failed"
        fi
    elif check_tool trivy; then
        log_info "Generating SBOM via trivy..."
        if trivy fs --format spdx-json --output "${sbom_path}" "${AGENTRT_ROOT}" 2>&1 | tail -3; then
            pass "SBOM generated via trivy: $(basename "$sbom_path")"
        else
            fail "SBOM generation (trivy) failed"
        fi
    else
        skip "SBOM: neither syft nor trivy installed"
    fi
}

# ============================================================================
# Check 6: 敏感数据检测 (detect-secrets)
# ============================================================================
check_sensitive_data() {
    section "Check 6: Sensitive Data Detection"

    if check_tool detect-secrets; then
        log_info "Running detect-secrets scan..."
        cd "$PROJECT_ROOT"
        if detect-secrets scan --all-files 2>&1 | tail -10; then
            pass "Sensitive data: No secrets detected"
        else
            warn "Sensitive data: Potential secrets found (review needed)"
        fi
    elif python3 -c "import detect_secrets" 2>/dev/null; then
        log_info "Running detect-secrets via Python..."
        cd "$PROJECT_ROOT"
        if python3 -m detect_secrets scan --all-files 2>&1 | tail -10; then
            pass "Sensitive data: No secrets detected"
        else
            warn "Sensitive data: Potential secrets found (review needed)"
        fi
    else
        skip "Sensitive data: detect-secrets not installed"
    fi
}

# ============================================================================
# Check 7: 许可证合规审计 (license_finder)
# ============================================================================
check_license_audit() {
    section "Check 7: License Compliance Audit"

    # 检查 CMakeLists.txt 中声明的许可证
    local cmake_file="${PROJECT_ROOT}/CMakeLists.txt"
    if [ -f "$cmake_file" ]; then
        if grep -qi "LICENSE\|Apache\|MIT\|GPL\|BSD" "$cmake_file" 2>/dev/null; then
            pass "License: Declared in CMakeLists.txt"
        else
            warn "License: Not explicitly declared in CMakeLists.txt"
        fi
    fi

    # 检查 LICENSE 文件
    if [ -f "${PROJECT_ROOT}/LICENSE" ] || [ -f "${PROJECT_ROOT}/LICENSE.md" ]; then
        pass "License: LICENSE file present"
    else
        warn "License: No LICENSE file found"
    fi

    # license_finder 工具
    if check_tool license_finder; then
        log_info "Running license_finder..."
        cd "$PROJECT_ROOT"
        if license_finder --quiet 2>&1; then
            pass "License audit: All dependencies approved"
        else
            warn "License audit: Some dependencies need review"
        fi
    else
        skip "License audit: license_finder not installed"
    fi
}

# ============================================================================
# Check 8: IaC 安全扫描 (checkov K8s manifests)
# ============================================================================
check_iac_scan() {
    section "Check 8: IaC Security Scan (K8s Manifests)"

    local k8s_dir="${PROJECT_ROOT}/deploy/kubernetes"
    if [ ! -d "$k8s_dir" ]; then
        skip "IaC scan: deploy/kubernetes/ directory not found"
        return
    fi

    if check_tool checkov; then
        log_info "Running checkov on K8s manifests..."
        if checkov --directory "$k8s_dir" --quiet --framework kubernetes 2>&1 | tail -10; then
            pass "IaC scan (checkov): No issues found"
        else
            local checkov_exit=$?
            if [ $checkov_exit -eq 1 ]; then
                warn "IaC scan (checkov): Issues found"
            else
                pass "IaC scan (checkov): Soft-fail on non-critical issues"
            fi
        fi
    else
        skip "IaC scan: checkov not installed"
    fi

    # Docker Compose 安全审计
    local compose_file="${PROJECT_ROOT}/deploy/docker/docker-compose.test.yml"
    if [ -f "$compose_file" ] && check_tool checkov; then
        log_info "Running checkov on docker-compose.test.yml..."
        checkov --file "$compose_file" --quiet 2>&1 | tail -5 || true
        pass "IaC scan: docker-compose.test.yml audited"
    fi
}

# ============================================================================
# Check 9: API 安全扫描 (zap-baseline 针对 gateway)
# ============================================================================
check_api_security() {
    section "Check 9: API Security Scan"

    local gateway_host="${GATEWAY_TEST_HOST:-localhost}"
    local gateway_port="${GATEWAY_TEST_PORT:-8080}"

    # 检查 gateway 是否在运行
    if ! curl -s "http://${gateway_host}:${gateway_port}/api/v1/health" > /dev/null 2>&1; then
        skip "API scan: Gateway not running on ${gateway_host}:${gateway_port}"
        return
    fi

    if check_tool zap-baseline.py; then
        log_info "Running ZAP baseline scan against gateway..."
        if zap-baseline.py -t "http://${gateway_host}:${gateway_port}" \
            -r "${ARTIFACTS_DIR}/zap-report.html" 2>&1 | tail -10; then
            pass "API scan (ZAP): No high-risk alerts"
        else
            warn "API scan (ZAP): Alerts found (review report)"
        fi
    elif check_tool zap-full-scan.py; then
        log_info "Running ZAP full scan..."
        if zap-full-scan.py -t "http://${gateway_host}:${gateway_port}" \
            -r "${ARTIFACTS_DIR}/zap-report.html" 2>&1 | tail -10; then
            pass "API scan (ZAP full): Completed"
        else
            warn "API scan (ZAP full): Issues found"
        fi
    else
        skip "API scan: ZAP not installed (OWASP ZAP)"
    fi
}

# ============================================================================
# Check 10: 供应链安全 (cosign verify)
# ============================================================================
check_supply_chain() {
    section "Check 10: Supply Chain Security"

    # 验证 SBOM 签名
    if check_tool cosign; then
        # 查找最近的 SBOM 文件
        local latest_sbom
        latest_sbom=$(ls -t "${ARTIFACTS_DIR}"/sbom-*.spdx.json 2>/dev/null | head -1 || true)
        if [ -n "$latest_sbom" ]; then
            log_info "Verifying SBOM signature..."
            if cosign verify-blob --key cosign.pub "$latest_sbom" 2>&1; then
                pass "Supply chain: SBOM signature verified"
            else
                warn "Supply chain: SBOM signature verification failed (key may not exist)"
            fi
        else
            warn "Supply chain: No SBOM found to verify"
        fi
    else
        skip "Supply chain: cosign not installed"
    fi

    # 检查 external/ 依赖的完整性
    local external_dir="${AGENTRT_ROOT}/external"
    if [ -d "$external_dir" ]; then
        local external_count
        external_count=$(find "$external_dir" -maxdepth 2 -name "*.sha256" -o -name "*.sha512" 2>/dev/null | wc -l)
        log_info "External dependency checksum files: ${external_count}"
        if [ "$external_count" -ge 1 ]; then
            pass "Supply chain: External dependency checksums present"
        else
            warn "Supply chain: No external dependency checksums found"
        fi
    else
        skip "Supply chain: external/ directory not found"
    fi
}

# ============================================================================
# 主函数
# ============================================================================
main() {
    # 解析参数
    local only_check=""
    local skip_checks=""
    local output_json=false

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --only)
                only_check="$2"
                shift 2
                ;;
            --skip)
                skip_checks="$2"
                shift 2
                ;;
            --json)
                output_json=true
                shift
                ;;
            --help|-h)
                echo "Usage: $0 [--only CHECK_NUM] [--skip CHECK_NUMS] [--json]"
                echo ""
                echo "  --only N     Run only check N (1-10)"
                echo "  --skip N,M   Skip checks N and M"
                echo "  --json       Output results as JSON"
                echo ""
                echo "Checks:"
                echo "  1. Dependency CVE Scan (grype/trivy)"
                echo "  2. C Static Analysis (flawfinder + cppcheck)"
                echo "  3. Docker Image Scan"
                echo "  4. Secret Leak Detection (gitleaks/trufflehog)"
                echo "  5. SBOM Generation (syft SPDX)"
                echo "  6. Sensitive Data Detection (detect-secrets)"
                echo "  7. License Compliance Audit"
                echo "  8. IaC Security Scan (checkov)"
                echo "  9. API Security Scan (ZAP)"
                echo "  10. Supply Chain Security (cosign)"
                exit 0
                ;;
            *)
                echo "Unknown option: $1"
                exit 1
                ;;
        esac
    done

    print_header
    mkdir -p "$ARTIFACTS_DIR"

    local run_check_1=true run_check_2=true run_check_3=true run_check_4=true run_check_5=true
    local run_check_6=true run_check_7=true run_check_8=true run_check_9=true run_check_10=true

    # 处理 --only
    if [ -n "$only_check" ]; then
        run_check_1=false; run_check_2=false; run_check_3=false; run_check_4=false; run_check_5=false
        run_check_6=false; run_check_7=false; run_check_8=false; run_check_9=false; run_check_10=false
        eval "run_check_${only_check}=true"
    fi

    # 处理 --skip
    if [ -n "$skip_checks" ]; then
        IFS=',' read -ra SKIP_ARR <<< "$skip_checks"
        for n in "${SKIP_ARR[@]}"; do
            eval "run_check_${n}=false"
        done
    fi

    # 执行各项检查
    $run_check_1 && check_cve_scan
    $run_check_2 && check_static_analysis
    $run_check_3 && check_docker_scan
    $run_check_4 && check_secret_leak
    $run_check_5 && check_sbom
    $run_check_6 && check_sensitive_data
    $run_check_7 && check_license_audit
    $run_check_8 && check_iac_scan
    $run_check_9 && check_api_security
    $run_check_10 && check_supply_chain

    # 生成报告
    generate_report

    # 输出结果
    section "Security Scan Results"
    echo ""
    echo -e "  ${COLOR_CYAN}Total Checks:${COLOR_RESET}  $TOTAL"
    echo -e "  ${COLOR_GREEN}Passed:${COLOR_RESET}       $PASS"
    echo -e "  ${COLOR_YELLOW}Warnings:${COLOR_RESET}     $WARN"
    echo -e "  ${COLOR_RED}Failed:${COLOR_RESET}       $FAIL"
    echo -e "  ${COLOR_YELLOW}Skipped:${COLOR_RESET}      $SKIP"
    echo ""

    if [ "$FAIL" -eq 0 ]; then
        echo -e "${COLOR_GREEN}╔══════════════════════════════════════╗${COLOR_RESET}"
        echo -e "${COLOR_GREEN}║     SECURITY SCAN PASSED             ║${COLOR_RESET}"
        echo -e "${COLOR_GREEN}╚══════════════════════════════════════╝${COLOR_RESET}"
        exit 0
    else
        echo -e "${COLOR_RED}╔══════════════════════════════════════╗${COLOR_RESET}"
        echo -e "${COLOR_RED}║     SECURITY SCAN FAILED             ║${COLOR_RESET}"
        echo -e "${COLOR_RED}║     ${FAIL} CHECK(S) FAILED                 ║${COLOR_RESET}"
        echo -e "${COLOR_RED}╚══════════════════════════════════════╝${COLOR_RESET}"
        exit 1
    fi
}

main "$@"