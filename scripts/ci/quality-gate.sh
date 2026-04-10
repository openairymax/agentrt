#!/usr/bin/env bash
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS 质量门禁脚本
# 执行：静态分析、代码格式检查、复杂度检测、安全扫描、文档完整性
# Version: 2.0.0
# Note: 质量门禁默认不阻塞 CI（exit code 0），仅报告问题

set -euo pipefail

###############################################################################
# 路径定义
###############################################################################
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

###############################################################################
# 颜色和日志
###############################################################################
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
BLUE='\033[0;34m'; MAGENTA='\033[0;35m'; NC='\033[0m'

log_info()  { echo -e "${BLUE}[QG]${NC}   $*"; }
log_ok()    { echo -e "${GREEN}[QG-OK]${NC}  $*"; }
log_warn()  { echo -e "${YELLOW}[QG-WARN]${NC} $*"; }
log_error() { echo -e "${RED}[QG-ERR]${NC}  $*" >&2; }
log_section() { echo -e "\n${MAGENTA}--- Quality Gate: $* ---${NC}"; }

###############################################################################
# 配置
###############################################################################
QUALITY_STRICT=false
QUALITY_REPORT_FILE="${PROJECT_ROOT}/ci-artifacts/quality-report.json"
QUALITY_FAIL_ON_WARN=false

# 统计
ISSUES_CRITICAL=0
ISSUES_HIGH=0
ISSUES_MEDIUM=0
ISSUES_LOW=0
CHECKS_TOTAL=0
CHECKS_PASSED=0
CHECKS_FAILED=0
declare -a ISSUE_DETAILS=()

###############################################################################
# 工具函数
###############################################################################
record_issue() {
    local severity="$1"
    local check="$2"
    local file="$3"
    local message="$4"

    case "$severity" in
        critical) ((ISSUES_CRITICAL++)) ;;
        high)     ((ISSUES_HIGH++)) ;;
        medium)   ((ISSUES_MEDIUM++)) ;;
        low)      ((ISSUES_LOW++)) ;;
    esac

    ISSUE_DETAILS+=("{\"severity\":\"$severity\",\"check\":\"$check\",\"file\":\"$file\",\"message\":\"$message\"}")
}

record_check_result() {
    local check_name="$1"
    local passed="$2"
    ((CHECKS_TOTAL++))
    if [[ "$passed" == "true" ]]; then
        ((CHECKS_PASSED++))
    else
        ((CHECKS_FAILED++))
    fi
}

###############################################################################
# Gate 1: C/C++ 静态分析 (cppcheck)
###############################################################################
gate_cpp_static_analysis() {
    log_section "C/C++ Static Analysis (cppcheck)"

    if ! command -v cppcheck &>/dev/null; then
        log_warn "cppcheck not installed, skipping static analysis"
        record_check_result "cppcheck" "skipped"
        return 0
    fi

    local source_dirs=(
        "${PROJECT_ROOT}/agentos/daemon/common/src"
        "${PROJECT_ROOT}/agentos/atoms/corekern/src"
        "${PROJECT_ROOT}/agentos/atoms/coreloopthree/src"
        "${PROJECT_ROOT}/agentos/atoms/memoryrovol/src"
        "${PROJECT_ROOT}/agentos/commons/utils"
        "${PROJECT_ROOT}/agentos/cupolas/src"
    )

    local error_count=0
    for dir in "${source_dirs[@]}"; do
        if [[ ! -d "$dir" ]]; then
            continue
        fi

        log_info "Analyzing: $dir"

        local cppcheck_output
        cppcheck_output=$(cppcheck \
            --enable=all \
            --std=c11 \
            --suppress=missingIncludeSystem \
            --suppress=unusedFunction \
            --suppress=unknownMacro \
            --error-exitcode=0 \
            --quiet \
            "$dir" 2>&1) || true

        while IFS= read -r line; do
            if [[ -n "$line" ]] && [[ "$line" != *"Checking"* ]]; then
                ((error_count++))

                local severity="low"
                if echo "$line" | grep -qiE "(error|critical)"; then
                    severity="high"
                    record_issue "high" "cppcheck" "$dir" "$line"
                elif echo "$line" | grep -qiE "(warning|performance|portability)"; then
                    severity="medium"
                    record_issue "medium" "cppcheck" "$dir" "$line"
                else
                    record_issue "low" "cppcheck" "$dir" "$line"
                fi

                if [[ "$QUALITY_STRICT" == "true" ]]; then
                    log_error "  $line"
                else
                    log_warn "  $line"
                fi
            done <<< "$cppcheck_output"
    done

    if [[ $error_count -eq 0 ]]; then
        log_ok "cppcheck: No issues found"
        record_check_result "cppcheck" "true"
    else
        log_warn "cppcheck: $error_count issue(s) found"
        record_check_result "cppcheck" "false"
    fi
}

###############################################################################
# Gate 2: 代码格式检查 (clang-format)
###############################################################################
gate_code_format() {
    log_section "Code Format Check (clang-format)"

    if ! command -v clang-format &>/dev/null; then
        log_warn "clang-format not installed, skipping format check"
        record_check_result "clang-format" "skipped"
        return 0
    fi

    local format_issues=0
    local files_checked=0

    while IFS= read -r -d '' file; do
        ((files_checked++))
        if ! clang-format --dry-run --Werror "$file" &>/dev/null; then
            ((format_issues++))
            record_issue "medium" "clang-format" "$file" "Formatting does not conform to .clang-format"
            log_warn "  Needs formatting: $file"
        fi
    done < <(find "${PROJECT_ROOT}/agentos" \
        \( -name "*.c" -o -name "*.h" \) \
        ! -path "*/tests/*" \
        ! -path "*/build-*/*" \
        -print0 2>/dev/null | head -z -100)

    if [[ $format_issues -eq 0 ]]; then
        log_ok "clang-format: All $files_checked file(s) properly formatted"
        record_check_result "clang-format" "true"
    else
        log_warn "clang-format: $format_issues/$files_checked file(s) need formatting"
        record_check_result "clang-format" "false"
    fi
}

###############################################################################
# Gate 3: Python 质量检查
###############################################################################
gate_python_quality() {
    log_section "Python Quality Checks"

    if ! command -v python3 &>/dev/null; then
        log_warn "python3 not available, skipping Python checks"
        record_check_result "python-quality" "skipped"
        return 0
    fi

    local py_issues=0
    local py_files=0

    # 语法检查
    while IFS= read -r -d '' file; do
        ((py_files++))
        local syntax_error
        syntax_error=$(python3 -m py_compile "$file" 2>&1) || {
            ((py_issues++))
            record_issue "high" "python-syntax" "$file" "$syntax_error"
            log_error "  Syntax error: $file"
        }
    done < <(find "${PROJECT_ROOT}" -name "*.py" \
        ! -path "*/__pycache__/*" \
        ! -path "*/.git/*" \
        ! -path "*/node_modules/*" \
        ! -path "*/venv/*" \
        ! -path "*/build-*/*" \
        -print0 2>/dev/null | head -z -150)

    # import 排序检查 (isort)
    if command -v isort &>/dev/null; then
        log_info "Checking import sorting with isort..."
        while IFS= read -r -d '' file; do
            if ! isort --check-only --diff "$file" &>/dev/null; then
                record_issue "low" "isort" "$file" "Imports not sorted"
                ((py_issues++))
            fi
        done < <(find "${PROJECT_ROOT}" -name "*.py" \
            ! -path "*/__pycache__/*" -print0 2>/dev/null | head -z -80)
    fi

    if [[ $py_issues -eq 0 ]]; then
        log_ok "Python quality: All $py_files file(s) passed"
        record_check_result "python-quality" "true"
    else
        log_warn "Python quality: $py_issues issue(s) in $py_files file(s)"
        record_check_result "python-quality" "false"
    fi
}

###############################################################################
# Gate 4: Shell 脚本质量
###############################################################################
gate_shell_quality() {
    log_section "Shell Script Quality"

    local sh_issues=0
    local sh_files=0

    while IFS= read -r -d '' file; do
        ((sh_files++))
        local shell_errors
        shell_errors=$(bash -n "$file" 2>&1) || {
            ((sh_issues++))
            record_issue "high" "shell-syntax" "$file" "$shell_errors"
            log_error "  Shell syntax error: $file"
        }
    done < <(find "${PROJECT_ROOT}/scripts" -name "*.sh" -print0 2>/dev/null)

    # shellcheck (如果可用)
    if command -v shellcheck &>/dev/null; then
        log_info "Running shellcheck..."
        while IFS= read -r -d '' file; do
            local sc_output
            sc_output=$(shellcheck -s warning "$file" 2>&1) || {
                while IFS= read -r line; do
                    if [[ -n "$line" ]]; then
                        record_issue "low" "shellcheck" "$file" "$line"
                        ((sh_issues++))
                    fi
                done <<< "$sc_output"
            }
        done < <(find "${PROJECT_ROOT}/scripts" -name "*.sh" -print0 2>/dev/null)
    fi

    if [[ $sh_issues -eq 0 ]]; then
        log_ok "Shell quality: All $sh_files script(s) passed"
        record_check_result "shell-quality" "true"
    else
        log_warn "Shell quality: $sh_issues issue(s) in $sh_files script(s)"
        record_check_result "shell-quality" "false"
    fi
}

###############################################################################
# Gate 5: 安全扫描基础检查
###############################################################################
gate_security_basic() {
    log_section "Basic Security Scan"

    local sec_issues=0

    # 检测硬编码密钥/凭证
    log_info "Scanning for hardcoded secrets..."
    local secret_patterns=(
        "password\s*=\s*[\"'][^\"']+[\"']"
        "api_key\s*=\s*[\"'][^\"']{10,}[\"']"
        "secret\s*=\s*[\"'][^\"']{10,}[\"']"
        "AKIA[0-9A-Z]{16}"
        "[A-Za-z0-9+/]{40,}={0,2}"
    )

    while IFS= read -r -d '' file; do
        for pattern in "${secret_patterns[@]}"; do
            if grep -qE "$pattern" "$file" 2>/dev/null; then
                local matches
                matches=$(grep -nE "$pattern" "$file" 2>/dev/null | head -3)
                record_issue "critical" "secrets" "$file" "Potential secret detected"
                log_error "  Potential secret in $file:"
                echo "$matches" | sed 's/^/    /'
                ((sec_issues++))
            fi
        done
    done < <(find "${PROJECT_ROOT}" \( -name "*.py" -o -name "*.sh" -o -name "*.c" -o -name "*.h" \) \
        ! -path "*/.git/*" ! -path "*/node_modules/*" \
        ! -path "*/tests/*" ! -path "*/examples/*" \
        -print0 2>/dev/null | head -z -100)

    # 检测危险函数调用
    log_info "Scanning for dangerous function calls..."
    local dangerous_funcs=("system(" "popen(" "eval(" "exec(" "strcpy(" "sprintf(")

    while IFS= read -r -d '' file; do
        for func in "${dangerous_funcs[@]}"; do
            if grep -q "$func" "$file" 2>/dev/null; then
                record_issue "medium" "dangerous-func" "$file" "Uses $func"
                ((sec_issues++))
            fi
        done
    done < <(find "${PROJECT_ROOT}/agentos" -name "*.c" -print0 2>/dev/null | head -z -80)

    if [[ $sec_issues -eq 0 ]]; then
        log_ok "Security scan: No issues found"
        record_check_result "security-basic" "true"
    else
        log_warn "Security scan: $sec_issues potential issue(s) found"
        record_check_result "security-basic" "false"
    fi
}

###############################################################################
# Gate 6: 文档完整性
###############################################################################
gate_documentation() {
    log_section "Documentation Completeness"

    local doc_issues=0

    # README.md 存在性
    if [[ -f "${PROJECT_ROOT}/README.md" ]]; then
        log_ok "README.md exists"
    else
        record_issue "high" "documentation" "ROOT" "Missing README.md"
        log_error "Missing README.md"
        ((doc_issues++))
    fi

    # CHANGELOG.md 存在性
    if [[ -f "${PROJECT_ROOT}/CHANGELOG.md" ]]; then
        log_ok "CHANGELOG.md exists"
    else
        record_issue "medium" "documentation" "ROOT" "Missing CHANGELOG.md"
        ((doc_issues++))
    fi

    # LICENSE 文件
    if [[ -f "${PROJECT_ROOT}/LICENSE" ]] || [[ -f "${PROJECT_ROOT}/LICENSE.md" ]]; then
        log_ok "LICENSE file exists"
    else
        record_issue "medium" "documentation" "ROOT" "Missing LICENSE file"
        ((doc_issues++))
    fi

    # 关键模块的 README
    for module in daemon atoms commons cupolas gateway heapstore toolkit; do
        local mod_readme="${PROJECT_ROOT}/agentos/${module}/README.md"
        if [[ -f "$mod_readme" ]]; then
            : # OK
        elif [[ -d "${PROJECT_ROOT}/agentos/${module}" ]]; then
            record_issue "low" "documentation" "agentos/$module" "Missing module README"
            ((doc_issues++))
        fi
    done

    if [[ $doc_issues -eq 0 ]]; then
        log_ok "Documentation: Complete"
        record_check_result "documentation" "true"
    else
        log_warn "Documentation: $doc_issues missing item(s)"
        record_check_result "documentation" "false"
    fi
}

###############################################################################
# 生成报告
###############################################################################
generate_quality_report() {
    mkdir -p "$(dirname "$QUALITY_REPORT_FILE")"

    cat > "$QUALITY_REPORT_FILE" << EOF
{
    "timestamp": "$(date -Iseconds)",
    "project": "AgentOS",
    "version": "1.0.0.6",
    "summary": {
        "checks_total": ${CHECKS_TOTAL},
        "checks_passed": ${CHECKS_PASSED},
        "checks_failed": ${CHECKS_FAILED},
        "pass_rate": "$(awk "BEGIN {if (${CHECKS_TOTAL} > 0) printf \"%.1f\", (${CHECKS_PASSED}/${CHECKS_TOTAL})*100; else print \"0.0\"}")%"
    },
    "issues": {
        "critical": ${ISSUES_CRITICAL},
        "high": ${ISSUES_HIGH},
        "medium": ${ISSUES_MEDIUM},
        "low": ${ISSUES_LOW},
        "total": $(( ISSUES_CRITICAL + ISSUES_HIGH + ISSUES_MEDIUM + ISSUES_LOW ))
    },
    "details": [
$(local first=true
for detail in "${ISSUE_DETAILS[@]}"; do
    if [[ "$first" == "true" ]]; then
        first=false
    else
        echo ","
    fi
    echo -n "        $detail"
done)
    ]
}
EOF

    log_ok "Quality report saved to: $QUALITY_REPORT_FILE"
}

###############################################################################
# 结果输出
###############################################################################
print_final_summary() {
    log_info ""
    log_info "==========================================="
    log_info "Quality Gate Summary"
    log_info "==========================================="
    log_info "Checks:  ${CHECKS_PASSED}/${CHECKS_TOTAL} passed"
    log_info "Issues:"
    log_info "  Critical: ${ISSUES_CRITICAL}"
    log_info "  High:     ${ISSUES_HIGH}"
    log_info "  Medium:   ${ISSUES_MEDIUM}"
    log_info "  Low:      ${ISSUES_LOW}"
    log_info ""

    local total_issues=$(( ISSUES_CRITICAL + ISSUES_HIGH + ISSUES_MEDIUM + ISSUES_LOW ))

    if [[ "$QUALITY_STRICT" == "true" ]] && [[ $total_issues -gt 0 ]]; then
        log_error "Quality gate FAILED (strict mode)"
        return 1
    elif [[ "$QUALITY_FAIL_ON_WARN" == "true" ]] && [[ $(( ISSUES_CRITICAL + ISSUES_HIGH )) -gt 0 ]]; then
        log_error "Quality gate FAILED (fail-on-warn mode, critical/high issues)"
        return 1
    elif [[ $total_issues -eq 0 ]]; then
        log_ok "Quality gate PASSED - No issues found"
    else
        log_warn "Quality gate completed with $total_issues issue(s) (non-blocking)"
    fi

    return 0
}

###############################################################################
# 帮助
###############################################################################
show_help() {
    cat << 'EOF'
AgentOS Quality Gate Script v2.0.0

Usage: ./quality-gate.sh [OPTIONS]

Options:
    --strict          Fail on any issue (default: report only)
    --fail-on-warn   Fail on high/critical issues
    --report FILE    Custom report output path
    -h, --help       Show this help

Gates:
    1. C/C++ Static Analysis (cppcheck)
    2. Code Format Check (clang-format)
    3. Python Quality (syntax, isort)
    4. Shell Script Quality (bash -n, shellcheck)
    5. Basic Security Scan (secrets, dangerous functions)
    6. Documentation Completeness
EOF
}

###############################################################################
# 参数解析
###############################################################################
parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --strict)        QUALITY_STRICT=true ;;
            --fail-on-warn)  QUALITY_FAIL_ON_WARN=true ;;
            --report)        QUALITY_REPORT_FILE="$2"; shift ;;
            --help|-h)       show_help; exit 0 ;;
            *) log_warn "Unknown option: $1" ;;
        esac
        shift
    done
}

###############################################################################
# 主流程
###############################################################################
main() {
    parse_args "$@"

    log_info "AgentOS Quality Gate v2.0.0"
    log_info "Timestamp: $(date '+%Y-%m-%d %H:%M:%S')"
    log_info "Strict mode: $QUALITY_STRICT"

    mkdir -p "${PROJECT_ROOT}/ci-artifacts"

    gate_cpp_static_analysis
    gate_code_format
    gate_python_quality
    gate_shell_quality
    gate_security_basic
    gate_documentation

    generate_quality_report
    print_final_summary
}

main "$@"
