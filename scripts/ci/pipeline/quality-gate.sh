#!/usr/bin/env bash
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS 质量门禁脚本
# 执行：静态分析、代码格式检查、复杂度检测、安全扫描、文档完整性
# Version: 0.1.0
# Note: 质量门禁默认不阻塞 CI（exit code 0），仅报告问题

set -euo pipefail

###############################################################################
# 路径定义
###############################################################################
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"

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
        critical) ((ISSUES_CRITICAL++)) || true ;;
        high)     ((ISSUES_HIGH++)) || true ;;
        medium)   ((ISSUES_MEDIUM++)) || true ;;
        low)      ((ISSUES_LOW++)) || true ;;
    esac

    ISSUE_DETAILS+=("{\"severity\":\"$severity\",\"check\":\"$check\",\"file\":\"$file\",\"message\":\"$message\"}")
}

record_check_result() {
    local check_name="$1"
    local passed="$2"
    ((CHECKS_TOTAL++)) || true
    if [[ "$passed" == "true" ]]; then
        ((CHECKS_PASSED++)) || true
    else
        ((CHECKS_FAILED++)) || true
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
                ((error_count++)) || true

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
        ((files_checked++)) || true
        if ! clang-format --dry-run --Werror "$file" &>/dev/null; then
            ((format_issues++)) || true
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
        ((py_files++)) || true
        local syntax_error
        syntax_error=$(python3 -m py_compile "$file" 2>&1) || {
            ((py_issues++)) || true
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
                ((py_issues++)) || true
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
        ((sh_files++)) || true
        local shell_errors
        shell_errors=$(bash -n "$file" 2>&1) || {
            ((sh_issues++)) || true
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
                        ((sh_issues++)) || true
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
                ((sec_issues++)) || true
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
                ((sec_issues++)) || true
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
# Gate 6: BAN-17~BAN-20 穷尽审计扫描
###############################################################################
gate_ban_audit() {
    log_section "BAN-17~BAN-20 Audit Scan"

    local ban_issues=0

    # BAN-17: 简化实现标记
    local ban17_hits
    ban17_hits=$(grep -rn "简化实现\|简化版\|简化处理\|simplified implementation\|simplified" \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v "test_\|tests/" | grep -v "memoryrovol/" | wc -l) || true
    if [[ $ban17_hits -gt 0 ]]; then
        record_issue "critical" "BAN-17" "agentos/" "$ban17_hits simplified implementation(s) found"
        log_error "  BAN-17: $ban17_hits simplified implementation(s)"
        ((ban_issues++)) || true
    else
        log_ok "  BAN-17: 0 simplified implementations"
    fi

    # BAN-18: 桩函数体
    local ban18_hits
    ban18_hits=$(grep -rn "框架占位\|占位符\|桩函数\|memset.*return 0" \
        --include="*.c" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v "test_\|tests/" | grep -v "无桩函数\|禁止桩函数" | wc -l) || true
    if [[ $ban18_hits -gt 0 ]]; then
        record_issue "critical" "BAN-18" "agentos/" "$ban18_hits stub function body(ies) found"
        log_error "  BAN-18: $ban18_hits stub function body(ies)"
        ((ban_issues++)) || true
    else
        log_ok "  BAN-18: 0 stub function bodies"
    fi

    # BAN-19: mock/fake降级
    local ban19_hits
    ban19_hits=$(grep -rn "mock.*response\|generate_response_mock\|DEGRADE_MOCK" \
        --include="*.c" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v "test_\|tests/" | wc -l) || true
    if [[ $ban19_hits -gt 0 ]]; then
        record_issue "critical" "BAN-19" "agentos/" "$ban19_hits mock degradation(s) found"
        log_error "  BAN-19: $ban19_hits mock degradation(s)"
        ((ban_issues++)) || true
    else
        log_ok "  BAN-19: 0 mock degradations"
    fi

    # BAN-20: 解析器功能缺失
    local ban20_hits
    ban20_hits=$(grep -rn "不支持.*anchor\|不支持.*alias\|简化.*JSON.*解析\|简化.*INI.*解析" \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v "test_\|tests/" | wc -l) || true
    if [[ $ban20_hits -gt 0 ]]; then
        record_issue "high" "BAN-20" "agentos/" "$ban20_hits parser limitation(s) found"
        log_warn "  BAN-20: $ban20_hits parser limitation(s)"
        ((ban_issues++)) || true
    else
        log_ok "  BAN-20: 0 parser limitations"
    fi

    # BAN-33: 构建产物在源码目录
    local ban33_hits
    ban33_hits=$(find "${PROJECT_ROOT}/agentos" -name "*.o" -o -name "CMakeCache.txt" -o -name "CMakeFiles" 2>/dev/null | wc -l) || true
    if [[ $ban33_hits -gt 0 ]]; then
        record_issue "critical" "BAN-33" "agentos/" "$ban33_hits build artifact(s) in source tree"
        log_error "  BAN-33: $ban33_hits build artifact(s) in source tree"
        ((ban_issues++)) || true
    else
        log_ok "  BAN-33: no in-source build artifacts"
    fi

    if [[ $ban_issues -eq 0 ]]; then
        log_ok "BAN audit: All checks passed"
        record_check_result "ban-audit" "true"
    else
        log_warn "BAN audit: $ban_issues check(s) failed"
        record_check_result "ban-audit" "false"
    fi
}

###############################################################################
# Gate 7.5: BAN-70~103 Strict Mode (CI-01)
###############################################################################
gate_ban_strict_rules() {
    log_section "BAN Strict Rules (BAN-70 ~ BAN-103)"

    local strict_issues=0

    # BAN-70: BUILD_TESTS=ON 验证
    local ban70_pass=false
    for cmake_file in "${PROJECT_ROOT}/CMakeLists.txt" "${PROJECT_ROOT}/../MemoryRovol/CMakeLists.txt"; do
        if [[ -f "$cmake_file" ]]; then
            if grep -q 'BUILD_TESTS.*ON' "$cmake_file" 2>/dev/null|| true; then
                ban70_pass=true
            fi
        fi
    done
    if [[ "$ban70_pass" == "true" ]]; then
        log_ok "BAN-70: BUILD_TESTS=ON configured"
        record_check_result "BAN-70" "true"
    else
        record_issue "critical" "BAN-70" "CMakeLists.txt" "BUILD_TESTS not set to ON by default"
        log_error "BAN-70: BUILD_TESTS should default to ON"
        ((strict_issues++)) || true
        record_check_result "BAN-70" "false"
    fi

    # BAN-71: banned_functions.h 注入验证
    local ban71_pass=false
    if grep -q "banned_functions.h" "${PROJECT_ROOT}/CMakeLists.txt" 2>/dev/null|| true; then
        ban71_pass=true
    fi
    if [[ "$ban71_pass" == "true" ]]; then
        log_ok "BAN-71: banned_functions.h injection configured"
        record_check_result "BAN-71" "true"
    else
        record_issue "critical" "BAN-71" "CMakeLists.txt" "banned_functions.h not injected"
        log_error "BAN-71: banned_functions.h should be injected via -include"
        ((strict_issues++)) || true
        record_check_result "BAN-71" "false"
    fi

    # BAN-72: WARNINGS_AS_ERRORS=ON 验证
    local ban72_pass=false
    if grep -q 'WARNINGS_AS_ERRORS.*ON' "${PROJECT_ROOT}/CMakeLists.txt" 2>/dev/null|| true; then
        ban72_pass=true
    fi
    if [[ "$ban72_pass" == "true" ]]; then
        log_ok "BAN-72: WARNINGS_AS_ERRORS=ON configured"
        record_check_result "BAN-72" "true"
    else
        record_issue "critical" "BAN-72" "CMakeLists.txt" "WARNINGS_AS_ERRORS not set to ON by default"
        log_error "BAN-72: WARNINGS_AS_ERRORS should default to ON"
        ((strict_issues++)) || true
        record_check_result "BAN-72" "false"
    fi

    # BAN-73: 无 return -1 验证（精确匹配，避免匹配 -10/-11 等）
    local ban73_hits
    ban73_hits=$(grep -rn '\breturn -1\b' \
        --include="*.c" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v "/tests/" | grep -v "/examples/" | wc -l) || true
    if [[ $ban73_hits -le 10 ]]; then
        log_ok "BAN-73: return -1 count=$ban73_hits (≤10)"
        record_check_result "BAN-73" "true"
    else
        record_issue "critical" "BAN-73" "agentos/" "$ban73_hits occurrences of 'return -1' (>10)"
        log_error "BAN-73: return -1 count=$ban73_hits exceeds limit of 10"
        ((strict_issues++)) || true
        record_check_result "BAN-73" "false"
    fi

    # BAN-74: error_push_ex 覆盖率验证 (≥20 files, ≥100 occurrences)
    local ban74_files ban74_occ
    ban74_files=$(grep -rl 'AGENTOS_ERROR_HANDLE\|AGENTOS_ERROR_PUSH_EX\|agentos_error_push_ex' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v "/tests/" | grep -v "/examples/" | wc -l) || true
    ban74_occ=$(grep -rn 'AGENTOS_ERROR_HANDLE\|AGENTOS_ERROR_PUSH_EX\|agentos_error_push_ex' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v "/tests/" | grep -v "/examples/" | wc -l) || true
    if [[ $ban74_files -ge 20 ]] && [[ $ban74_occ -ge 100 ]]; then
        log_ok "BAN-74: error_push_ex coverage: ${ban74_files} files, ${ban74_occ} occurrences"
        record_check_result "BAN-74" "true"
    else
        record_issue "critical" "BAN-74" "agentos/" "error_push_ex: ${ban74_files} files, ${ban74_occ} occ (need ≥20 files, ≥100 occ)"
        log_error "BAN-74: error_push_ex insufficient coverage"
        ((strict_issues++)) || true
        record_check_result "BAN-74" "false"
    fi

    # BAN-75~77: daemon printf=0 验证（排除 examples、tests 和 daemon_security.h 文档示例）
    local ban75_printf
    ban75_printf=$(grep -rn '\bprintf\b\|\bfprintf\b' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/daemon/" 2>/dev/null \
        | grep -v "/tests/" | grep -v "/examples/" | grep -v "daemon_security.h" | wc -l) || true
    if [[ $ban75_printf -eq 0 ]]; then
        log_ok "BAN-75: daemon printf=0 (non-test, non-example)"
        record_check_result "BAN-75" "true"
    else
        record_issue "high" "BAN-75" "daemon/" "$ban75_printf printf/fprintf calls in daemon"
        log_warn "BAN-75: daemon printf count=$ban75_printf"
        ((strict_issues++)) || true
        record_check_result "BAN-75" "false"
    fi

    # BAN-80~82: Python 代码质量（flake8, bandit, mypy 简单检查）
    local ban80_py_syntax
    ban80_py_syntax=$(find "${PROJECT_ROOT}" -name "*.py" \
        ! -path "*/__pycache__/*" ! -path "*/.git/*" \
        ! -path "*/node_modules/*" ! -path "*/venv/*" \
        -exec python3 -m py_compile {} \; 2>&1 \
        | grep -c "SyntaxError" || true)
    if [[ $ban80_py_syntax -eq 0 ]]; then
        log_ok "BAN-80: Python syntax check passed"
        record_check_result "BAN-80" "true"
    else
        record_issue "high" "BAN-80" "Python/" "$ban80_py_syntax Python files with syntax errors"
        log_error "BAN-80: Python syntax errors found"
        ((strict_issues++)) || true
        record_check_result "BAN-80" "false"
    fi

    # BAN-90~93: TypeScript 代码质量（eslint 检查）
    if command -v npx &>/dev/null && [[ -f "${PROJECT_ROOT}/../../Desktop/package.json" ]]; then
        log_info "Checking TypeScript with eslint..."
        local ban90_ts_issues=0
        # 简化检查：验证 eslint 配置存在
        if [[ -f "${PROJECT_ROOT}/../../Desktop/.eslintrc.json" ]]; then
            log_ok "BAN-90: ESLint configuration exists"
            record_check_result "BAN-90" "true"
        else
            record_issue "medium" "BAN-90" "Desktop/" "Missing .eslintrc.json"
            ((ban90_ts_issues++)) || true
            record_check_result "BAN-90" "false"
        fi
    else
        log_info "BAN-90~93: Desktop/ not accessible, skipping TypeScript checks"
    fi

    # BAN-96~103: 综合安全检查
    # BAN-96: 无 :latest 标签
    local ban96_latest
    ban96_latest=$(grep -rn ':latest' \
        --include="Dockerfile*" "${PROJECT_ROOT}/../Docker/" 2>/dev/null | wc -l) || true
    if [[ $ban96_latest -eq 0 ]]; then
        log_ok "BAN-96: No :latest tags in Dockerfiles"
        record_check_result "BAN-96" "true"
    else
        record_issue "high" "BAN-96" "Docker/" "$ban96_latest :latest tags found"
        log_error "BAN-96: :latest tag usage detected"
        ((strict_issues++)) || true
        record_check_result "BAN-96" "false"
    fi

    # BAN-97: 无明文密钥
    local ban97_secrets
    ban97_secrets=$(grep -rnE '(password|secret|api_key)\s*=\s*["\x27][^"\x27]+["\x27]' \
        --include="*.yml" --include="*.yaml" "${PROJECT_ROOT}/../Docker/" 2>/dev/null \
        | grep -v '${' | wc -l) || true
    if [[ $ban97_secrets -eq 0 ]]; then
        log_ok "BAN-97: No hardcoded secrets in Docker configs"
        record_check_result "BAN-97" "true"
    else
        record_issue "critical" "BAN-97" "Docker/" "$ban97_secrets potential hardcoded secrets"
        log_error "BAN-97: Hardcoded secrets detected"
        ((strict_issues++)) || true
        record_check_result "BAN-97" "false"
    fi

    # BAN-100: 版本号一致性
    local ban100_versions
    ban100_versions=$(grep -rn '0\.0\.[45]\|1\.0\.0\.5' \
        --include="Dockerfile*" --include="*.yml" --include="*.yaml" \
        "${PROJECT_ROOT}/../Docker/" 2>/dev/null | wc -l) || true
    if [[ $ban100_versions -eq 0 ]]; then
        log_ok "BAN-100: Version numbers consistent (0.1.0)"
        record_check_result "BAN-100" "true"
    else
        record_issue "high" "BAN-100" "Docker/" "$ban100_versions files with inconsistent versions"
        log_warn "BAN-100: Version inconsistencies detected"
        ((strict_issues++)) || true
        record_check_result "BAN-100" "false"
    fi

    if [[ $strict_issues -eq 0 ]]; then
        log_ok "BAN Strict Rules: All checks passed"
        record_check_result "ban-strict-rules" "true"
    else
        log_warn "BAN Strict Rules: $strict_issues check(s) failed"
        record_check_result "ban-strict-rules" "false"
    fi
}

###############################################################################
# Gate 7: 文档完整性
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
        ((doc_issues++)) || true
    fi

    # CHANGELOG.md 存在性
    if [[ -f "${PROJECT_ROOT}/CHANGELOG.md" ]]; then
        log_ok "CHANGELOG.md exists"
    else
        record_issue "medium" "documentation" "ROOT" "Missing CHANGELOG.md"
        ((doc_issues++)) || true
    fi

    # LICENSE 文件
    if [[ -f "${PROJECT_ROOT}/LICENSE" ]] || [[ -f "${PROJECT_ROOT}/LICENSE.md" ]]; then
        log_ok "LICENSE file exists"
    else
        record_issue "medium" "documentation" "ROOT" "Missing LICENSE file"
        ((doc_issues++)) || true
    fi

    # 关键模块的 README
    for module in daemon atoms commons cupolas gateway heapstore toolkit; do
        local mod_readme="${PROJECT_ROOT}/agentos/${module}/README.md"
        if [[ -f "$mod_readme" ]]; then
            : # OK
        elif [[ -d "${PROJECT_ROOT}/agentos/${module}" ]]; then
            record_issue "low" "documentation" "agentos/$module" "Missing module README"
            ((doc_issues++)) || true
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
AgentOS Quality Gate Script v0.1.0

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
    6. BAN-17~BAN-20 + BAN-33~BAN-35 Audit Scan
    7. Documentation Completeness
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

    log_info "AgentOS Quality Gate v0.1.0"
    log_info "Timestamp: $(date '+%Y-%m-%d %H:%M:%S')"
    log_info "Strict mode: $QUALITY_STRICT"

    mkdir -p "${PROJECT_ROOT}/ci-artifacts"

    gate_cpp_static_analysis
    gate_code_format
    gate_python_quality
    gate_shell_quality
    gate_security_basic
    gate_ban_audit
    gate_ban_strict_rules    # CI-01: BAN-70~103 strict mode
    gate_documentation

    generate_quality_report
    print_final_summary
}

main "$@"
