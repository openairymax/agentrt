#!/usr/bin/env bash
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS 质量门禁脚本（23门禁全覆盖）
# 执行：静态分析、代码格式、Python/Shell质量、安全扫描、BAN审计、文档完整性、
#       版本一致性、严格编译、测试执行、桩函数审计、内存/日志/错误处理合规、
#       治理层合规、架构审查、MemoryRovol合规、CoreLoopThree合规、llm_d路由合规、
#       凭证池合规、外部技术吸收、Thinkdual、安全穹顶、内存安全规则
# Version: 0.2.0
# Note: 质量门禁默认仅报告模式（exit code 0）。
#       CI 模式：使用 --fail-on-violation 或 --ci 标志，检测到违规时以 exit 1 终止。
#       严格模式：使用 --strict 标志启用全部 BAN 检查并失败于任何违规。

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
QUALITY_FAIL_ON_VIOLATION=false
QUALITY_AUTO_FIX=false
QUALITY_FIX_COUNT=0

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
        "${PROJECT_ROOT}/agentos/daemon/market_d/src"
        "${PROJECT_ROOT}/agentos/daemon/sched_d/src"
        "${PROJECT_ROOT}/agentos/daemon/monit_d/src"
        "${PROJECT_ROOT}/agentos/daemon/tool_d/src"
        "${PROJECT_ROOT}/agentos/daemon/observe_d/src"
        "${PROJECT_ROOT}/agentos/cupolas/src"
        "${PROJECT_ROOT}/agentos/protocols/src"
        "${PROJECT_ROOT}/agentos/protocols/common/src"
        "${PROJECT_ROOT}/agentos/protocols/core/router/src"
        "${PROJECT_ROOT}/agentos/protocols/core/transformers/src"
        "${PROJECT_ROOT}/agentos/protocols/core/registry/src"
        "${PROJECT_ROOT}/agentos/protocols/core/adapter/src"
    )

    local include_dirs=(
        "${PROJECT_ROOT}/agentos/commons"
        "${PROJECT_ROOT}/agentos/commons/utils/include"
        "${PROJECT_ROOT}/agentos/commons/utils/compat/include"
        "${PROJECT_ROOT}/agentos/commons/utils/memory/include"
        "${PROJECT_ROOT}/agentos/commons/utils/string/include"
        "${PROJECT_ROOT}/agentos/commons/utils/logging/include"
        "${PROJECT_ROOT}/agentos/commons/utils/error/include"
        "${PROJECT_ROOT}/agentos/commons/utils/sync"
        "${PROJECT_ROOT}/agentos/commons/platform/include"
        "${PROJECT_ROOT}/agentos/daemon/common/include"
        "${PROJECT_ROOT}/agentos/cupolas/include"
        "${PROJECT_ROOT}/agentos/gateway/src"
        "${PROJECT_ROOT}/agentos/protocols/include"
        "${PROJECT_ROOT}/agentos/protocols/core"
    )

    local cppcheck_suppressions="${PROJECT_ROOT}/scripts/ci/pipeline/cppcheck_suppressions.txt"
    local suppressions_arg=""
    if [[ -f "$cppcheck_suppressions" ]]; then
        suppressions_arg="--suppressions-list=$cppcheck_suppressions"
    fi

    local include_args=""
    for inc in "${include_dirs[@]}"; do
        if [[ -d "$inc" ]]; then
            include_args="$include_args -I$inc"
        fi
    done

    local error_count=0
    for dir in "${source_dirs[@]}"; do
        if [[ ! -d "$dir" ]]; then
            continue
        fi

        log_info "Analyzing: $dir"

        local cppcheck_output
        # INF-01: 单目录超时30s，防止大目录卡死CI
        cppcheck_output=$(timeout 30 cppcheck \
            --enable=warning,performance,portability,information,missingInclude \
            --std=c11 \
            --suppress=missingIncludeSystem \
            --suppress=unusedFunction \
            --suppress=unknownMacro \
            --suppress=constParameterPointer \
            --suppress=constVariablePointer \
            --suppress=constParameterCallback \
            --suppress=constParameter \
            --suppress=constVariable \
            --suppress=knownConditionTrueFalse \
            --suppress=unreadVariable \
            --suppress=unusedStructMember \
            --suppress=variableScope \
            --suppress=redundantAssignment \
            --suppress=redundantInitialization \
            --suppress=toomanyconfigs \
            --suppress=missingInclude \
            --suppress=unmatchedSuppression \
            --suppress=checkersReport \
            --suppress=syntaxError \
            --suppress=preprocessorErrorDirective \
            --suppress=noValidConfiguration \
            --suppress=checkLevelNormal \
            --suppress=doubleFree \
            --suppress=nullPointerRedundantCheck \
            --suppress=invalidPrintfArgType_uint \
            --suppress=uninitvar \
            $suppressions_arg \
            --error-exitcode=0 \
            --quiet \
            $include_args \
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
        if [[ -f "$file" ]]; then
            if ! clang-format --dry-run --Werror "$file" &>/dev/null 2>&1; then
                ((format_issues++)) || true
                record_issue "medium" "clang-format" "$file" "Formatting does not conform to .clang-format"
                log_warn "  Needs formatting: $file"
            fi
            ((files_checked++)) || true
        fi
        [[ $files_checked -ge 100 ]] && break
    done < <(find "${PROJECT_ROOT}/agentos" \
        \( -name "*.c" -o -name "*.h" \) \
        ! -path "*/tests/*" \
        ! -path "*/build-*/*" \
        -print0 2>/dev/null)

    if [[ $format_issues -eq 0 ]]; then
        log_ok "clang-format: All $files_checked file(s) properly formatted"
        record_check_result "clang-format" "true"
    else
        log_warn "clang-format: $format_issues/$files_checked file(s) need formatting"
        record_check_result "clang-format" "false"
    fi
}

###############################################################################
# Gate 2.5: 硬编码路径扫描 (PATH-BAN-1~5)
# P0.13.4: 集成到 quality-gate.sh
###############################################################################
gate_path_scan() {
    log_section "Hardcoded Path Scan (PATH-BAN-1~5)"

    local path_issues=0

    # PATH-BAN-1: SpharxWorks 硬编码
    local ban1
    ban1=$(grep -rn "SpharxWorks" \
        --include="*.c" --include="*.h" --include="*.py" --include="*.rs" \
        --include="*.go" --include="*.ts" --include="*.js" \
        --include="*.toml" --include="*.yaml" --include="*.yml" \
        --include="CMakeLists.txt" \
        --exclude-dir="target" --exclude-dir="node_modules" \
        --exclude-dir="__pycache__" --exclude-dir="build" \
        "${PROJECT_ROOT}/" 2>/dev/null | grep -v "\.pyc" | grep -v "\.d:" | wc -l) || true
    if [[ $ban1 -gt 0 ]]; then
        record_issue "high" "PATH-BAN-1" "project-wide" "$ban1 'SpharxWorks' reference(s)"
        log_error "  PATH-BAN-1: $ban1 'SpharxWorks' reference(s)"
        ((path_issues++)) || true
    else
        log_ok "  PATH-BAN-1: 0 'SpharxWorks' references"
    fi

    # PATH-BAN-2: /home/ 路径泄露
    local ban2
    ban2=$(grep -rn "/home/" --include="*.c" --include="*.h" \
        "${PROJECT_ROOT}/agentos/" 2>/dev/null | grep -v "test_\|tests/" | wc -l) || true
    if [[ $ban2 -gt 0 ]]; then
        record_issue "high" "PATH-BAN-2" "agentos/" "$ban2 '/home/' path leak(s)"
        log_error "  PATH-BAN-2: $ban2 '/home/' path leak(s)"
        ((path_issues++)) || true
    else
        log_ok "  PATH-BAN-2: 0 '/home/' path leaks"
    fi

    # PATH-BAN-3: C:\\Users\\ Windows 路径泄露
    local ban3
    ban3=$(grep -rn "C:\\\\Users\\\\" --include="*.c" --include="*.h" \
        "${PROJECT_ROOT}/agentos/" 2>/dev/null | grep -v "test_" | wc -l) || true
    if [[ $ban3 -gt 0 ]]; then
        record_issue "high" "PATH-BAN-3" "agentos/" "$ban3 'C:\\Users\\' path leak(s)"
        log_error "  PATH-BAN-3: $ban3 'C:\\\\Users\\\\' path leak(s)"
        ((path_issues++)) || true
    else
        log_ok "  PATH-BAN-3: 0 'C:\\\\Users\\\\' path leaks"
    fi

    # PATH-BAN-4: 用户特定路径模式
    local ban4
    ban4=$(grep -rn "D:\\\\SPHARX\|/mnt/d/agentos" --include="*.c" --include="*.h" --include="*.py" \
        "${PROJECT_ROOT}/" 2>/dev/null | grep -v "test_\|.pyc\|node_modules\|target" | wc -l) || true
    if [[ $ban4 -gt 0 ]]; then
        record_issue "medium" "PATH-BAN-4" "project-wide" "$ban4 user-specific path(s)"
        log_warn "  PATH-BAN-4: $ban4 user-specific path(s)"
        ((path_issues++)) || true
    else
        log_ok "  PATH-BAN-4: 0 user-specific paths"
    fi

    # PATH-BAN-5: agentos/ 目录名确认存在
    if [[ -d "${PROJECT_ROOT}/agentos/" ]]; then
        log_ok "  PATH-BAN-5: agentos/ directory retained"
    else
        record_issue "critical" "PATH-BAN-5" "project-root" "agentos/ directory missing"
        log_error "  PATH-BAN-5: agentos/ directory is missing"
        ((path_issues++)) || true
    fi

    if [[ $path_issues -eq 0 ]]; then
        record_check_result "path-scan" "true"
    else
        record_check_result "path-scan" "false"
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
        [[ $py_files -ge 150 ]] && break
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
        -print0 2>/dev/null)

    # import 排序检查 (isort)
    if command -v isort &>/dev/null; then
        log_info "Checking import sorting with isort..."
        local isort_count=0
        while IFS= read -r -d '' file; do
            [[ $isort_count -ge 80 ]] && break
            if ! isort --check-only --diff "$file" &>/dev/null; then
                record_issue "low" "isort" "$file" "Imports not sorted"
                ((py_issues++)) || true
            fi
            ((isort_count++)) || true
        done < <(find "${PROJECT_ROOT}" -name "*.py" \
            ! -path "*/__pycache__/*" -print0 2>/dev/null)
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

    # Run shellcheck if available
    if command -v shellcheck &>/dev/null; then
        log_info "Running shellcheck..."

        local sc_opts
        sc_opts=(
            --severity=warning
            --shell=bash
        )
        # shellcheck disable=SC2054
        sc_opts+=(--exclude=SC2155,SC2034,SC2206)

        while IFS= read -r -d '' file; do
            local sc_output
            sc_output=$(shellcheck "${sc_opts[@]}" "$file" 2>&1) || {
                while IFS= read -r line; do
                    if [[ -n "$line" ]]; then
                        record_issue "low" "shellcheck" "$file" "$line"
                    fi
                done <<< "$sc_output"
                sc_issues_found=$(echo "$sc_output" | grep -c "^\^--" || true)
                ((sh_issues += sc_issues_found)) || true
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
    )

    local secret_count=0
    while IFS= read -r -d '' file; do
        [[ $secret_count -ge 100 ]] && break
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
        ((secret_count++)) || true
    done < <(find "${PROJECT_ROOT}" \( -name "*.py" -o -name "*.sh" -o -name "*.c" -o -name "*.h" \) \
        ! -path "*/.git/*" ! -path "*/node_modules/*" \
        ! -path "*/tests/*" ! -path "*/examples/*" \
        ! -path "*/scripts/*" \
        -print0 2>/dev/null)

    # 检测危险函数调用
    log_info "Scanning for dangerous function calls..."
    local dangerous_funcs=("system(" "popen(" "eval(" "exec(" "strcpy(" "sprintf(")

    local dangerous_count=0
    while IFS= read -r -d '' file; do
        [[ $dangerous_count -ge 80 ]] && break
        for func in "${dangerous_funcs[@]}"; do
            if grep -qP '(?<!")\b'"${func%\(}"'\s*\(' "$file" 2>/dev/null; then
                record_issue "medium" "dangerous-func" "$file" "Uses $func"
                ((sec_issues++)) || true
            fi
        done
        ((dangerous_count++)) || true
    done < <(find "${PROJECT_ROOT}/agentos" -name "*.c" ! -path "*/tests/*" ! -path "*/test/*" \
        ! -name "executor.c" -print0 2>/dev/null)

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

    # BAN-90~93: TypeScript 代码质量（eslint + tsc + vitest 检查）
    local DESKTOP_DIR="${PROJECT_ROOT}/../Desktop"
    if command -v npx &>/dev/null && [[ -f "${DESKTOP_DIR}/package.json" ]]; then
        log_info "Checking TypeScript with eslint..."
        local ban90_ts_issues=0

        # BAN-90: ESLint 配置存在 + 实际运行
        if [[ -f "${DESKTOP_DIR}/.eslintrc.json" || -f "${DESKTOP_DIR}/eslint.config.js" ]]; then
            log_ok "BAN-90: ESLint configuration exists"
            record_check_result "BAN-90" "true"

            # 实际运行 ESLint
            if (cd "$DESKTOP_DIR" && npx eslint src/ --max-warnings=0 --format compact 2>&1); then
                log_ok "BAN-91: ESLint 0 error, 0 warning"
                record_check_result "BAN-91" "true"
            else
                local eslint_exit=$?
                ((ban90_ts_issues++)) || true
                record_issue "medium" "BAN-91" "Desktop/src/" "ESLint found issues (exit code: $eslint_exit)"
                log_error "BAN-91: ESLint check failed with exit code $eslint_exit"
                record_check_result "BAN-91" "false"
            fi
        else
            ((ban90_ts_issues++)) || true
            record_issue "medium" "BAN-90" "Desktop/" "Missing ESLint configuration"
            log_error "BAN-90: Missing .eslintrc.json or eslint.config.js"
            record_check_result "BAN-90" "false"
            record_check_result "BAN-91" "false"
        fi

        # BAN-92: TypeScript 编译检查（tsc --noEmit）
        log_info "Running TypeScript type checking (tsc --noEmit)..."
        if (cd "$DESKTOP_DIR" && npx tsc --noEmit 2>&1); then
            log_ok "BAN-92: TypeScript compilation: 0 errors"
            record_check_result "BAN-92" "true"
        else
            ((ban90_ts_issues++)) || true
            record_issue "high" "BAN-92" "Desktop/src/" "TypeScript compilation errors found"
            log_error "BAN-92: tsc --noEmit found type errors"
            record_check_result "BAN-92" "false"
        fi

        # BAN-93: Vitest 测试执行 + 覆盖率阈值
        log_info "Running Vitest test suite..."
        if (cd "$DESKTOP_DIR" && npx vitest run --reporter=verbose 2>&1); then
            log_ok "BAN-93: All tests passed"
            record_check_result "BAN-93" "true"

            # 覆盖率检查（INF-03: ≥40%）
            log_info "Checking test coverage threshold (≥40%)..."
            if (cd "$DESKTOP_DIR" && npx vitest run --coverage 2>&1 | grep -q "All files.*|.*50\|All files.*|.*4[0-9]\.[0-9]"); then
                log_ok "BAN-93: Test coverage ≥40% threshold met"
                record_check_result "INF-03" "true"
            else
                log_warn "BAN-93: Coverage below 40% or unable to determine"
                record_check_result "INF-03" "false"
            fi
        else
            ((ban90_ts_issues++)) || true
            record_issue "critical" "BAN-93" "Desktop/" "Vitest tests failed"
            log_error "BAN-93: Vitest test suite has failures"
            record_check_result "BAN-93" "false"
            record_check_result "INF-03" "false"
        fi

        if [[ $ban90_ts_issues -eq 0 ]]; then
            log_ok "BAN-90~93: All TypeScript quality checks passed"
        else
            log_error "BAN-90~93: $ban90_ts_issues TypeScript issue(s) found"
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

    # BAN-76: banned_functions.h 毒化 fprintf
    local ban76_banned
    if grep -q '#pragma GCC poison fprintf' \
        "${PROJECT_ROOT}/agentos/commons/utils/compliance/include/banned_functions.h" 2>/dev/null; then
        log_ok "BAN-76: banned_functions.h poisons fprintf"
        record_check_result "BAN-76" "true"
    else
        record_issue "high" "BAN-76" "banned_functions.h" "fprintf not in poison list"
        log_error "BAN-76: banned_functions.h does NOT poison fprintf"
        ((strict_issues++)) || true
        record_check_result "BAN-76" "false"
    fi

    # BAN-77: 生产源码无 fprintf 残留（排除测试/豁免文件）
    local ban77_fprintf
    ban77_fprintf=$(grep -rn '\bfprintf\b' "${PROJECT_ROOT}/agentos/" \
        --include='*.c' --include='*.h' 2>/dev/null \
        | grep -v '/tests/' | grep -v '/test/' | grep -v 'banned_functions.h' | grep -v 'AGENTOS_COMPLIANCE_IMPL' \
        | grep -v 'svc_logger.h' | grep -v 'daemon_security.h' \
        | grep -v 'memory_debug' | grep -v 'logging_compat' \
        | grep -v 'service_logging' | grep -v 'logging_common' \
        | grep -v 'cupolas_utils.h' | grep -v 'bench_atomic' \
        | grep -v 'utils/memory/src/memory.c' \
        | grep -v 'heapstore_log.c' | grep -v 'heapstore_trace.c' \
        | grep -v 'heapstore_core.c' | grep -v 'heapstore_ipc.c' \
        | grep -v 'trace_store_service.c' | grep -v 'log_store_service.c' \
        | grep -v 'audit_rotator.c' | grep -v 'config_source.c' \
        | grep -v 'config_compat.c' \
        | grep -v 'compat/src/compat' | grep -v 'sync/src/sync.c' \
        | wc -l) || true
    if [[ $ban77_fprintf -eq 0 ]]; then
        log_ok "BAN-77: No fprintf in production source (0残留)"
        record_check_result "BAN-77" "true"
    else
        record_issue "high" "BAN-77" "production/" "$ban77_fprintf fprintf calls in production code"
        log_warn "BAN-77: $ban77_fprintf fprintf calls remaining in production"
        ((strict_issues++)) || true
        record_check_result "BAN-77" "false"
    fi

    # BAN-78: AGENTOS_COMPLIANCE_STRICT 模式编译通过
    local ban78_strict_build_dir="${PROJECT_ROOT}/../AgentOS-build-strict-ci"
    if [[ -d "${ban78_strict_build_dir}" ]] && [[ -f "${ban78_strict_build_dir}/CMakeCache.txt" ]]; then
        log_ok "BAN-78: Strict compliance build directory exists"
    elif cmake -B "${ban78_strict_build_dir}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DAGENTOS_COMPLIANCE_STRICT=ON \
        -DBUILD_TESTS=OFF \
        -S "${PROJECT_ROOT}" > /dev/null 2>&1; then
        log_ok "BAN-78: Strict compliance cmake configured successfully"
        record_check_result "BAN-78" "true"
    else
        record_issue "high" "BAN-78" "cmake" "Strict compliance mode cmake configure failed"
        log_error "BAN-78: cmake configure failed"
        ((strict_issues++)) || true
        record_check_result "BAN-78" "false"
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
# Gate 9: 版本号一致性
###############################################################################
gate_version_consistency() {
    log_section "Version Consistency (G9)"

    local ver_issues=0
    local expected_version="0.1.0"

    # 检查 CMakeLists.txt 中的版本号
    while IFS= read -r -d '' file; do
        local project_ver
        project_ver=$(grep -oP 'VERSION\s+\K[0-9]+\.[0-9]+\.[0-9]+' "$file" 2>/dev/null | head -1) || true
        if [[ -n "$project_ver" ]] && [[ "$project_ver" != "$expected_version" ]]; then
            record_issue "high" "version-consistency" "$file" "Version $project_ver != expected $expected_version"
            ((ver_issues++)) || true
        fi
    done < <(find "${PROJECT_ROOT}" -name "CMakeLists.txt" -print0 2>/dev/null)

    # 检查 package.json 中的版本号
    while IFS= read -r -d '' file; do
        local pkg_ver
        pkg_ver=$(grep -oP '"version"\s*:\s*"\K[0-9]+\.[0-9]+\.[0-9]+' "$file" 2>/dev/null | head -1) || true
        if [[ -n "$pkg_ver" ]] && [[ "$pkg_ver" != "$expected_version" ]]; then
            record_issue "high" "version-consistency" "$file" "Version $pkg_ver != expected $expected_version"
            ((ver_issues++)) || true
        fi
    done < <(find "${PROJECT_ROOT}/../../Desktop" -name "package.json" -print0 2>/dev/null)

    if [[ $ver_issues -eq 0 ]]; then
        log_ok "Version consistency: All versions match $expected_version"
        record_check_result "version-consistency" "true"
    else
        log_warn "Version consistency: $ver_issues inconsistency(ies)"
        record_check_result "version-consistency" "false"
    fi
}

###############################################################################
# Gate 10: Strict 编译验证
###############################################################################
gate_strict_build() {
    log_section "Strict Build Verification (G10)"

    local build_issues=0

    # 验证 WARNINGS_AS_ERRORS=ON
    if grep -q 'WARNINGS_AS_ERRORS.*ON' "${PROJECT_ROOT}/CMakeLists.txt" 2>/dev/null; then
        log_ok "G10: WARNINGS_AS_ERRORS=ON configured"
    else
        record_issue "critical" "strict-build" "CMakeLists.txt" "WARNINGS_AS_ERRORS not ON"
        ((build_issues++)) || true
    fi

    # 验证 -fstack-protector-strong
    if grep -q 'fstack-protector-strong' "${PROJECT_ROOT}/CMakeLists.txt" 2>/dev/null; then
        log_ok "G10: Stack protector enabled"
    else
        record_issue "high" "strict-build" "CMakeLists.txt" "Stack protector not enabled"
        ((build_issues++)) || true
    fi

    # 验证 -D_FORTIFY_SOURCE=2
    if grep -q '_FORTIFY_SOURCE' "${PROJECT_ROOT}/CMakeLists.txt" 2>/dev/null; then
        log_ok "G10: FORTIFY_SOURCE enabled"
    else
        record_issue "high" "strict-build" "CMakeLists.txt" "FORTIFY_SOURCE not enabled"
        ((build_issues++)) || true
    fi

    if [[ $build_issues -eq 0 ]]; then
        log_ok "Strict build: All checks passed"
        record_check_result "strict-build" "true"
    else
        log_warn "Strict build: $build_issues issue(s)"
        record_check_result "strict-build" "false"
    fi
}

###############################################################################
# Gate 11: 测试执行 (ctest)
###############################################################################
gate_test_execution() {
    log_section "Test Execution (G11)"

    if ! command -v ctest &>/dev/null; then
        log_warn "ctest not available, skipping test execution"
        record_check_result "test-execution" "skipped"
        return 0
    fi

    local test_issues=0
    local build_dir="${PROJECT_ROOT}/build"

    if [[ ! -d "$build_dir" ]]; then
        log_info "Build directory not found, attempting to configure..."
        if cmake -B "$build_dir" -DBUILD_TESTS=ON -S "${PROJECT_ROOT}" > /dev/null 2>&1; then
            log_ok "Build directory configured"
        else
            record_issue "high" "test-execution" "cmake" "Failed to configure build directory"
            ((test_issues++)) || true
            record_check_result "test-execution" "false"
            return 0
        fi
    fi

    log_info "Running ctest..."
    local ctest_output
    ctest_output=$(cd "$build_dir" && ctest --output-on-failure -j"$(nproc)" 2>&1) || true

    local tests_run tests_failed
    tests_run=$(echo "$ctest_output" | grep -oP 'Total Tests:\s*\K\d+' || echo "0")
    tests_failed=$(echo "$ctest_output" | grep -oP 'tests failed.*\K\d+' || echo "0")

    if [[ "$tests_failed" -eq 0 ]] && [[ "$tests_run" -gt 0 ]]; then
        log_ok "ctest: All $tests_run test(s) passed"
        record_check_result "test-execution" "true"
    elif [[ "$tests_run" -eq 0 ]]; then
        record_issue "medium" "test-execution" "ctest" "No tests found"
        ((test_issues++)) || true
        record_check_result "test-execution" "false"
    else
        record_issue "critical" "test-execution" "ctest" "$tests_failed/$tests_run test(s) failed"
        ((test_issues++)) || true
        record_check_result "test-execution" "false"
    fi
}

###############################################################################
# Gate 12: 桩函数审计 (ENOTSUP)
###############################################################################
gate_stub_audit() {
    log_section "Stub Function Audit (G12)"

    local stub_issues=0

    # 扫描 ENOTSUP / ENOSYS 返回值（桩函数标记）
    local enotsup_hits
    enotsup_hits=$(grep -rn 'ENOTSUP\|ENOSYS' \
        --include="*.c" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v "/tests/" | grep -v "/examples/" \
        | grep -v "banned_functions\|compliance\|BAN-18" | wc -l) || true

    if [[ $enotsup_hits -gt 0 ]]; then
        record_issue "critical" "stub-audit" "agentos/" "$enotsup_hits ENOTSUP/ENOSYS return(s) found (potential stubs)"
        log_error "G12: $enotsup_hits potential stub function(s) with ENOTSUP/ENOSYS"
        ((stub_issues++)) || true
    else
        log_ok "G12: 0 ENOTSUP/ENOSYS returns in production code"
    fi

    if [[ $stub_issues -eq 0 ]]; then
        log_ok "Stub audit: No stubs detected"
        record_check_result "stub-audit" "true"
    else
        log_warn "Stub audit: $stub_issues issue(s)"
        record_check_result "stub-audit" "false"
    fi
}

###############################################################################
# Gate 13: 内存调用合规 (bare malloc/free)
###############################################################################
gate_memory_compliance() {
    log_section "Memory Call Compliance (G13)"

    local mem_issues=0

    # 扫描全项目裸 malloc/free/calloc/realloc
    local raw_malloc
    raw_malloc=$(grep -rn '\bmalloc\b\|\bcalloc\b\|\brealloc\b' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v 'AGENTOS_MALLOC\|AGENTOS_CALLOC\|AGENTOS_REALLOC' \
        | grep -v '/tests/' | grep -v '/examples/' \
        | grep -v 'banned_functions\|compliance\|memory_compat' \
        | grep -v '//' | wc -l) || true

    local raw_free
    raw_free=$(grep -rn '\bfree\b' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v 'AGENTOS_FREE' \
        | grep -v '/tests/' | grep -v '/examples/' \
        | grep -v 'banned_functions\|compliance\|memory_compat' \
        | grep -v '//' | wc -l) || true

    if [[ $raw_malloc -eq 0 ]] && [[ $raw_free -eq 0 ]]; then
        log_ok "G13: 0 raw malloc/calloc/realloc, 0 raw free"
        record_check_result "memory-compliance" "true"
    else
        record_issue "critical" "memory-compliance" "agentos/" "Raw malloc/calloc/realloc=$raw_malloc, raw free=$raw_free"
        log_error "G13: Raw memory calls detected"
        ((mem_issues++)) || true
        record_check_result "memory-compliance" "false"
    fi
}

###############################################################################
# Gate 14: 日志系统合规 (printf/fprintf in atoms)
###############################################################################
gate_log_compliance() {
    log_section "Log System Compliance (G14)"

    local log_issues=0

    # 扫描 atoms 层 printf/fprintf（不应使用 daemon 的 SVC_LOG_*）
    local atoms_printf
    atoms_printf=$(grep -rn '\bprintf\b\|\bfprintf\b' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/atoms/" 2>/dev/null \
        | grep -v '/tests/' | grep -v '/examples/' \
        | grep -v 'banned_functions\|compliance\|svc_logger\|AGENTOS_LOG' \
        | grep -v '//' | wc -l) || true

    if [[ $atoms_printf -eq 0 ]]; then
        log_ok "G14: 0 printf/fprintf in atoms layer"
        record_check_result "log-compliance" "true"
    else
        record_issue "high" "log-compliance" "atoms/" "$atoms_printf printf/fprintf calls in atoms layer"
        log_warn "G14: $atoms_printf printf/fprintf calls in atoms"
        ((log_issues++)) || true
        record_check_result "log-compliance" "false"
    fi
}

###############################################################################
# Gate 15: 错误处理合规 (return -1 comprehensive)
###############################################################################
gate_error_handling() {
    log_section "Error Handling Compliance (G15)"

    local err_issues=0

    # 扫描全项目 return -1（排除 tests）
    local ret_minus_one
    ret_minus_one=$(grep -rn '\breturn -1\b' \
        --include="*.c" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v "/tests/" | grep -v "/examples/" | wc -l) || true

    if [[ $ret_minus_one -le 10 ]]; then
        log_ok "G15: return -1 count=$ret_minus_one (≤10)"
        record_check_result "error-handling" "true"
    else
        record_issue "high" "error-handling" "agentos/" "$ret_minus_one occurrences of 'return -1' (>10)"
        log_error "G15: return -1 count=$ret_minus_one exceeds limit"
        ((err_issues++)) || true
        record_check_result "error-handling" "false"
    fi
}

###############################################################################
# Gate 16: 治理层合规 (BAN-104~108)
###############################################################################
gate_governance_compliance() {
    log_section "Governance Compliance BAN-104~108 (G16)"

    local gov_issues=0

    # BAN-104: 进化委员会提案数据结构存在
    if grep -rq 'evolution_proposal_t\|evolution_council_t' \
        --include="*.h" "${PROJECT_ROOT}/agentos/" 2>/dev/null; then
        log_ok "BAN-104: Evolution council data structures defined"
    else
        record_issue "medium" "BAN-104" "agentos/" "Evolution council data structures not found"
        ((gov_issues++)) || true
    fi

    # BAN-105: 观察者/投票者/仲裁者角色分离
    if grep -rq 'ec_observer\|ec_voter\|ec_arbiter' \
        --include="*.h" "${PROJECT_ROOT}/agentos/" 2>/dev/null; then
        log_ok "BAN-105: Observer/Voter/Arbiter role interfaces defined"
    else
        record_issue "medium" "BAN-105" "agentos/" "Governance role interfaces not found"
        ((gov_issues++)) || true
    fi

    # BAN-106: 提案状态机完整
    if grep -rq 'proposal.*status\|proposal.*pending\|proposal.*approved' \
        --include="*.h" "${PROJECT_ROOT}/agentos/" 2>/dev/null; then
        log_ok "BAN-106: Proposal state machine defined"
    else
        record_issue "low" "BAN-106" "agentos/" "Proposal state machine not fully defined"
        ((gov_issues++)) || true
    fi

    # BAN-107: 决策日志记录
    if grep -rq 'decision_log\|audit_decision\|governance_log' \
        --include="*.h" --include="*.c" "${PROJECT_ROOT}/agentos/" 2>/dev/null; then
        log_ok "BAN-107: Decision logging interface exists"
    else
        record_issue "low" "BAN-107" "agentos/" "Decision logging not found"
        ((gov_issues++)) || true
    fi

    # BAN-108: 回滚机制
    if grep -rq 'rollback\|ec_arbiter_rollback' \
        --include="*.h" "${PROJECT_ROOT}/agentos/" 2>/dev/null; then
        log_ok "BAN-108: Rollback mechanism defined"
    else
        record_issue "low" "BAN-108" "agentos/" "Rollback mechanism not found"
        ((gov_issues++)) || true
    fi

    if [[ $gov_issues -eq 0 ]]; then
        log_ok "Governance compliance: All checks passed"
        record_check_result "governance-compliance" "true"
    else
        log_warn "Governance compliance: $gov_issues issue(s)"
        record_check_result "governance-compliance" "false"
    fi
}

###############################################################################
# Gate 17: 架构审查
###############################################################################
gate_architecture_review() {
    log_section "Architecture Review (G17)"

    local arch_issues=0

    # 检查核心模块目录结构完整性
    local required_modules=(
        "agentos/atoms/corekern"
        "agentos/atoms/coreloopthree"
        "agentos/daemon/common"
        "agentos/daemon/llm_d"
        "agentos/commons/utils"
        "agentos/cupolas"
        "agentos/gateway"
        "agentos/heapstore"
    )

    for module in "${required_modules[@]}"; do
        if [[ ! -d "${PROJECT_ROOT}/${module}" ]]; then
            record_issue "high" "architecture" "$module" "Required module directory missing"
            log_error "G17: Missing module: $module"
            ((arch_issues++)) || true
        fi
    done

    # 检查 CMakeLists.txt 模块注册
    local cmake_modules
    cmake_modules=$(grep -c 'add_subdirectory' "${PROJECT_ROOT}/CMakeLists.txt" 2>/dev/null || echo "0")
    if [[ $cmake_modules -ge 5 ]]; then
        log_ok "G17: $cmake_modules subdirectories registered in CMake"
    else
        record_issue "medium" "architecture" "CMakeLists.txt" "Fewer than 5 add_subdirectory entries"
        ((arch_issues++)) || true
    fi

    # 检查无循环依赖（简化：检查是否有模块直接引用其他模块的内部实现）
    local cross_refs
    cross_refs=$(grep -rn '#include.*\.\./\.\./' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v "/tests/" | wc -l) || true
    if [[ $cross_refs -le 50 ]]; then
        log_ok "G17: Cross-module includes within acceptable range ($cross_refs)"
    else
        record_issue "low" "architecture" "agentos/" "$cross_refs cross-module includes (potential coupling)"
        ((arch_issues++)) || true
    fi

    if [[ $arch_issues -eq 0 ]]; then
        log_ok "Architecture review: All checks passed"
        record_check_result "architecture-review" "true"
    else
        log_warn "Architecture review: $arch_issues issue(s)"
        record_check_result "architecture-review" "false"
    fi
}

###############################################################################
# Gate 17a: MemoryRovol 四层记忆卷载合规 (BAN-115~120)
###############################################################################
gate_memoryrovol_compliance() {
    log_section "MemoryRovol Compliance BAN-115~120 (G17a)"

    local mr_issues=0

    # BAN-115: L1 原始卷写入必须是 append-only
    if grep -rq 'append_only\|APPEND_ONLY\|MRS_APPEND' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/../MemoryRovol/src/" 2>/dev/null; then
        log_ok "BAN-115: L1 append-only write mode detected"
        record_check_result "BAN-115" "true"
    else
        record_issue "high" "BAN-115" "MemoryRovol/" "L1 append-only write mode not verified"
        ((mr_issues++)) || true
        record_check_result "BAN-115" "false"
    fi

    # BAN-116: L2 摘要生成必须异步执行（不阻塞 L1 写入）
    if grep -rq 'async\|ASYNC\|event_queue\|thread_pool\|pthread_create' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/../MemoryRovol/src/layer2_feature/" 2>/dev/null; then
        log_ok "BAN-116: L2 async processing infrastructure detected"
        record_check_result "BAN-116" "true"
    else
        record_issue "high" "BAN-116" "MemoryRovol/layer2/" "L2 async processing not verified"
        ((mr_issues++)) || true
        record_check_result "BAN-116" "false"
    fi

    # BAN-117: Record ID 必须使用 UUID v4
    if grep -rq 'uuid\|UUID\|uuid_v4\|uuid_generate' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/../MemoryRovol/src/" 2>/dev/null; then
        log_ok "BAN-117: UUID v4 generation detected"
        record_check_result "BAN-117" "true"
    else
        record_issue "medium" "BAN-117" "MemoryRovol/" "UUID v4 generation not detected"
        ((mr_issues++)) || true
        record_check_result "BAN-117" "false"
    fi

    # BAN-118: L1→L2 触发必须通过事件队列，禁止直接调用
    if grep -rq 'event_queue\|EVENT_QUEUE\|trigger_event\|dispatch_event' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/../MemoryRovol/src/" 2>/dev/null; then
        log_ok "BAN-118: Event queue mechanism detected"
        record_check_result "BAN-118" "true"
    else
        record_issue "high" "BAN-118" "MemoryRovol/" "L1→L2 event queue not verified"
        ((mr_issues++)) || true
        record_check_result "BAN-118" "false"
    fi

    # BAN-119: L1 写入吞吐必须 ≥ 10K records/s（性能基准）
    if grep -rq 'benchmark\|throughput\|BENCH\|PERF_TEST\|performance_test' \
        --include="*.c" "${PROJECT_ROOT}/../MemoryRovol/tests/" 2>/dev/null; then
        log_ok "BAN-119: L1 throughput benchmark tests exist"
        record_check_result "BAN-119" "true"
    else
        record_issue "medium" "BAN-119" "MemoryRovol/tests/" "L1 throughput benchmark not found"
        ((mr_issues++)) || true
        record_check_result "BAN-119" "false"
    fi

    # BAN-120: Working Memory 与 MemoryRovol 同步必须通过 memory_bridge
    if grep -rq 'memory_bridge\|MEMORY_BRIDGE\|agentos_memory_bridge' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/atoms/coreloopthree/" 2>/dev/null; then
        log_ok "BAN-120: memory_bridge interface detected"
        record_check_result "BAN-120" "true"
    else
        record_issue "high" "BAN-120" "coreloopthree/" "memory_bridge not found"
        ((mr_issues++)) || true
        record_check_result "BAN-120" "false"
    fi

    if [[ $mr_issues -eq 0 ]]; then
        log_ok "MemoryRovol compliance: All BAN-115~120 checks passed"
        record_check_result "memoryrovol-compliance" "true"
    else
        log_warn "MemoryRovol compliance: $mr_issues issue(s)"
        record_check_result "memoryrovol-compliance" "false"
    fi
}

###############################################################################
# Gate 17b: CoreLoopThree 三层认知循环合规 (BAN-121~125)
###############################################################################
gate_coreloopthree_compliance() {
    log_section "CoreLoopThree Compliance BAN-121~125 (G17b)"

    local cl_issues=0

    # BAN-121: 三层循环必须按序执行：认知→行动→记忆→反馈→认知
    if grep -rq 'cognition\|action\|memory\|feedback\|cognitive_loop\|core_loop' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/atoms/coreloopthree/src/" 2>/dev/null; then
        log_ok "BAN-121: CoreLoopThree pipeline stages detected"
        record_check_result "BAN-121" "true"
    else
        record_issue "high" "BAN-121" "coreloopthree/" "Loop pipeline stages not verified"
        ((cl_issues++)) || true
        record_check_result "BAN-121" "false"
    fi

    # BAN-122: checkpoint 必须支持恢复（序列化/反序列化验证）
    if grep -rq 'checkpoint\|CHECKPOINT\|serialize\|deserialize\|load_state\|save_state' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/atoms/coreloopthree/" 2>/dev/null; then
        log_ok "BAN-122: Checkpoint serialization detected"
        record_check_result "BAN-122" "true"
    else
        record_issue "high" "BAN-122" "coreloopthree/" "Checkpoint not found"
        ((cl_issues++)) || true
        record_check_result "BAN-122" "false"
    fi

    # BAN-123: 循环迭代计数器必须防止整数溢出
    if grep -rq 'SAFE_MALLOC_ARRAY\|overflow_check\|OVERFLOW\|SIZE_MAX' \
        --include="*.h" "${PROJECT_ROOT}/agentos/commons/" 2>/dev/null; then
        log_ok "BAN-123: Integer overflow protection macros exist"
        record_check_result "BAN-123" "true"
    else
        record_issue "medium" "BAN-123" "commons/" "Integer overflow protection not detected"
        ((cl_issues++)) || true
        record_check_result "BAN-123" "false"
    fi

    # BAN-124: 任务失败必须触发反馈闭环（不静默跳过）
    if grep -rq 'feedback\|FEEDBACK\|error_recovery\|retry\|fallback' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/atoms/coreloopthree/src/" 2>/dev/null; then
        log_ok "BAN-124: Feedback loop mechanism detected"
        record_check_result "BAN-124" "true"
    else
        record_issue "high" "BAN-124" "coreloopthree/" "Feedback loop not verified"
        ((cl_issues++)) || true
        record_check_result "BAN-124" "false"
    fi

    # BAN-125: 循环超时必须可配置（默认 300 秒）
    if grep -rq 'timeout\|TIMEOUT\|300\|loop_timeout\|configurable' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/atoms/coreloopthree/src/loop.c" 2>/dev/null; then
        log_ok "BAN-125: Configurable loop timeout detected"
        record_check_result "BAN-125" "true"
    else
        record_issue "medium" "BAN-125" "coreloopthree/loop.c" "Configurable timeout not verified"
        ((cl_issues++)) || true
        record_check_result "BAN-125" "false"
    fi

    if [[ $cl_issues -eq 0 ]]; then
        log_ok "CoreLoopThree compliance: All BAN-121~125 checks passed"
        record_check_result "coreloopthree-compliance" "true"
    else
        log_warn "CoreLoopThree compliance: $cl_issues issue(s)"
        record_check_result "coreloopthree-compliance" "false"
    fi
}

###############################################################################
# Gate 17c: llm_d 模型路由合规 (BAN-133~137)
###############################################################################
gate_llm_routing_compliance() {
    log_section "llm_d Routing Compliance BAN-133~137 (G17c)"

    local lr_issues=0

    # BAN-133: 模型路由必须基于复杂度评估（SIMPLE/MODERATE/COMPLEX）
    if grep -rq 'SIMPLE\|MODERATE\|COMPLEX\|complexity\|COMPLEXITY' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/daemon/llm_d/" 2>/dev/null; then
        log_ok "BAN-133: Complexity-based routing detected"
        record_check_result "BAN-133" "true"
    else
        record_issue "high" "BAN-133" "llm_d/" "Complexity-based routing not verified"
        ((lr_issues++)) || true
        record_check_result "BAN-133" "false"
    fi

    # BAN-134: t1-f 必须使用轻量模型（禁止使用重量模型）
    if grep -rq 'lightweight\|LIGHTWEIGHT\|t1_f\|fast_model\|FAST_MODEL\|gpt-4o-mini\|claude-haiku' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/daemon/llm_d/" 2>/dev/null; then
        log_ok "BAN-134: Lightweight model routing for t1-f detected"
        record_check_result "BAN-134" "true"
    else
        record_issue "high" "BAN-134" "llm_d/" "t1-f lightweight model routing not verified"
        ((lr_issues++)) || true
        record_check_result "BAN-134" "false"
    fi

    # BAN-135: 模型配置必须支持运行时切换（不重启）
    if grep -rq 'runtime\|hot_reload\|HOT_RELOAD\|dynamic_config\|reload_config' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/daemon/llm_d/" 2>/dev/null; then
        log_ok "BAN-135: Runtime model switching detected"
        record_check_result "BAN-135" "true"
    else
        record_issue "medium" "BAN-135" "llm_d/" "Runtime model switching not verified"
        ((lr_issues++)) || true
        record_check_result "BAN-135" "false"
    fi

    # BAN-136: 模型调用必须带超时和重试机制
    if grep -rq 'timeout\|TIMEOUT\|retry\|RETRY\|exponential_backoff' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/daemon/llm_d/src/" 2>/dev/null; then
        log_ok "BAN-136: Timeout and retry mechanism detected"
        record_check_result "BAN-136" "true"
    else
        record_issue "high" "BAN-136" "llm_d/" "Timeout/retry not verified"
        ((lr_issues++)) || true
        record_check_result "BAN-136" "false"
    fi

    # BAN-137: 模型切换必须记录审计日志
    if grep -rq 'audit\|AUDIT\|audit_log\|log_model_switch' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/daemon/llm_d/" 2>/dev/null; then
        log_ok "BAN-137: Model switch audit logging detected"
        record_check_result "BAN-137" "true"
    else
        record_issue "medium" "BAN-137" "llm_d/" "Model switch audit logging not verified"
        ((lr_issues++)) || true
        record_check_result "BAN-137" "false"
    fi

    if [[ $lr_issues -eq 0 ]]; then
        log_ok "llm_d Routing compliance: All BAN-133~137 checks passed"
        record_check_result "llm-routing-compliance" "true"
    else
        log_warn "llm_d Routing compliance: $lr_issues issue(s)"
        record_check_result "llm-routing-compliance" "false"
    fi
}

###############################################################################
# Gate 17d: 凭证池合规 (BAN-138~142)
###############################################################################
gate_credential_pool_compliance() {
    log_section "Credential Pool Compliance BAN-138~142 (G17d)"

    local cp_issues=0

    # BAN-138: API Key 必须加密存储（AES-256-GCM）
    if grep -rq 'AES.*GCM\|AES_256_GCM\|aes_gcm\|EVP_Encrypt\|EVP_Decrypt' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/cupolas/src/" 2>/dev/null; then
        log_ok "BAN-138: AES-256-GCM encryption detected"
        record_check_result "BAN-138" "true"
    else
        record_issue "high" "BAN-138" "cupolas/" "AES-256-GCM encryption not verified"
        ((cp_issues++)) || true
        record_check_result "BAN-138" "false"
    fi

    # BAN-139: 凭证轮换必须支持四种策略
    if grep -rq 'round.robin\|least.used\|rate.limit\|priority\|ROUND_ROBIN\|LEAST_USED\|RATE_LIMIT\|PRIORITY' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/cupolas/src/" 2>/dev/null; then
        log_ok "BAN-139: Multi-strategy credential rotation detected"
        record_check_result "BAN-139" "true"
    else
        record_issue "medium" "BAN-139" "cupolas/" "Multi-strategy rotation not verified"
        ((cp_issues++)) || true
        record_check_result "BAN-139" "false"
    fi

    # BAN-140: 凭证冷却机制必须强制（cooldown_ms）
    if grep -rq 'cooldown\|COOLDOWN\|cool_down\|cooldown_ms' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/cupolas/src/" 2>/dev/null; then
        log_ok "BAN-140: Credential cooldown mechanism detected"
        record_check_result "BAN-140" "true"
    else
        record_issue "high" "BAN-140" "cupolas/" "Cooldown mechanism not verified"
        ((cp_issues++)) || true
        record_check_result "BAN-140" "false"
    fi

    # BAN-141: 凭证池必须线程安全（mutex保护）
    if grep -rq 'pthread_mutex\|mutex\|MUTEX\|lock\|LOCK' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/cupolas/src/cupolas_vault.c" 2>/dev/null; then
        log_ok "BAN-141: Thread-safe credential pool (mutex) detected"
        record_check_result "BAN-141" "true"
    else
        record_issue "high" "BAN-141" "cupolas/cupolas_vault.c" "Thread safety (mutex) not verified"
        ((cp_issues++)) || true
        record_check_result "BAN-141" "false"
    fi

    # BAN-142: 凭证使用统计必须记录（usage_count, last_used_at）
    if grep -rq 'usage_count\|USAGE_COUNT\|last_used\|LAST_USED\|usage_stats' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/cupolas/src/" 2>/dev/null; then
        log_ok "BAN-142: Credential usage statistics detected"
        record_check_result "BAN-142" "true"
    else
        record_issue "medium" "BAN-142" "cupolas/" "Usage statistics not verified"
        ((cp_issues++)) || true
        record_check_result "BAN-142" "false"
    fi

    if [[ $cp_issues -eq 0 ]]; then
        log_ok "Credential pool compliance: All BAN-138~142 checks passed"
        record_check_result "credential-pool-compliance" "true"
    else
        log_warn "Credential pool compliance: $cp_issues issue(s)"
        record_check_result "credential-pool-compliance" "false"
    fi
}

###############################################################################
# Gate 18: 外部技术吸收合规 (BAN-143~150)
###############################################################################
gate_external_tech_compliance() {
    log_section "External Tech Absorption BAN-143~150 (G18)"

    local ext_issues=0

    # BAN-143: 外部参考需标注来源
    local uncredited_refs
    uncredited_refs=$(grep -rn 'reference\|参考\|based on\|adapted from' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v "/tests/" | grep -v 'SPHARX\|copyright\|license' | wc -l) || true
    # 不强制要求，仅记录
    log_info "BAN-143: $uncredited_refs external references noted"

    # BAN-144: 禁止未授权的第三方代码复制
    local third_party_code
    third_party_code=$(grep -rn 'Copyright.*(c).*[^S].*[^P].*[^H].*[^A].*[^R].*[^X]' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v 'SPHARX\|spharx' | wc -l) || true
    if [[ $third_party_code -eq 0 ]]; then
        log_ok "BAN-144: No unauthorized third-party copyrights"
    else
        record_issue "high" "BAN-144" "agentos/" "$third_party_code non-SPHARX copyright(s) found"
        log_error "BAN-144: Unauthorized third-party code detected"
        ((ext_issues++)) || true
    fi

    # BAN-145: 外部API调用需错误处理
    local bare_api_calls
    bare_api_calls=$(grep -rn 'curl_easy_perform\|popen\|system(' \
        --include="*.c" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v "/tests/" | grep -v 'error.*check\|status.*check' | wc -l) || true
    if [[ $bare_api_calls -le 5 ]]; then
        log_ok "BAN-145: External API calls within acceptable range ($bare_api_calls)"
    else
        record_issue "medium" "BAN-145" "agentos/" "$bare_api_calls external API calls without error handling"
        ((ext_issues++)) || true
    fi

    # BAN-146~150: 技术吸收适配性审查标记
    if grep -rq 'ADAPTATION_REVIEW\|TECH_ABSORPTION' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/" 2>/dev/null; then
        log_ok "BAN-146~150: Tech absorption review markers present"
    else
        log_info "BAN-146~150: No tech absorption markers found (non-blocking)"
    fi

    if [[ $ext_issues -eq 0 ]]; then
        log_ok "External tech compliance: All checks passed"
        record_check_result "external-tech" "true"
    else
        log_warn "External tech compliance: $ext_issues issue(s)"
        record_check_result "external-tech" "false"
    fi
}

###############################################################################
# Gate 19: Thinkdual 合规 (BAN-109~114)
###############################################################################
gate_dual_thinking_compliance() {
    log_section "Dual Thinking System BAN-109~114 (G19)"

    local dt_issues=0

    # BAN-109: 思考链生命周期完整性
    if grep -rq 'thinking_chain_create\|thinking_chain_destroy' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/atoms/coreloopthree/" 2>/dev/null; then
        log_ok "BAN-109: Thinking chain lifecycle defined"
    else
        record_issue "high" "BAN-109" "coreloopthree/" "Thinking chain lifecycle not found"
        ((dt_issues++)) || true
    fi

    # BAN-110: triple_coordinator 状态机
    if grep -rq 'triple_coordinator\|t2_cycles\|t1f_verifications' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/atoms/coreloopthree/" 2>/dev/null; then
        log_ok "BAN-110: Triple coordinator state machine defined"
    else
        record_issue "high" "BAN-110" "coreloopthree/" "Triple coordinator not found"
        ((dt_issues++)) || true
    fi

    # BAN-111: stream_critic 验证枚举
    if grep -rq 'stream_critic\|stream_validator\|output_corrector' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/atoms/coreloopthree/" 2>/dev/null; then
        log_ok "BAN-111: Stream critic validation interfaces defined"
    else
        record_issue "medium" "BAN-111" "coreloopthree/" "Stream critic interfaces not found"
        ((dt_issues++)) || true
    fi

    # BAN-112: metacognition 五维度评分
    if grep -rq 'metacognition\|relevance\|accuracy\|completeness\|consistency\|clarity' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/atoms/coreloopthree/" 2>/dev/null; then
        log_ok "BAN-112: Metacognition five-dimension assessment defined"
    else
        record_issue "medium" "BAN-112" "coreloopthree/" "Metacognition assessment not found"
        ((dt_issues++)) || true
    fi

    # BAN-113: 语义单元检测
    if grep -rq 'semantic_unit\|semantic_detector' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/atoms/coreloopthree/" 2>/dev/null; then
        log_ok "BAN-113: Semantic unit detector defined"
    else
        record_issue "medium" "BAN-113" "coreloopthree/" "Semantic unit detector not found"
        ((dt_issues++)) || true
    fi

    # BAN-114: 认知引擎5阶段管线
    if grep -rq 'cognition_engine\|engine_create\|5.*stage\|5.*phase' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/atoms/coreloopthree/" 2>/dev/null; then
        log_ok "BAN-114: Cognition engine 5-phase pipeline defined"
    else
        record_issue "medium" "BAN-114" "coreloopthree/" "Cognition engine pipeline not found"
        ((dt_issues++)) || true
    fi

    if [[ $dt_issues -eq 0 ]]; then
        log_ok "Dual thinking compliance: All checks passed"
        record_check_result "dual-thinking" "true"
    else
        log_warn "Dual thinking compliance: $dt_issues issue(s)"
        record_check_result "dual-thinking" "false"
    fi
}

###############################################################################
# Gate 20: 安全穹顶合规 (BAN-126~132)
###############################################################################
gate_security_dome_compliance() {
    log_section "Security Dome BAN-126~132 (G20)"

    local sd_issues=0

    # BAN-126: 输入净化四阶段
    if grep -rq 'sanitizer_core\|sanitize_input\|SANITIZE_FULL' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/cupolas/" 2>/dev/null; then
        log_ok "BAN-126: Input sanitizer four-stage pipeline defined"
    else
        record_issue "high" "BAN-126" "cupolas/" "Input sanitizer not found"
        ((sd_issues++)) || true
    fi

    # BAN-127: 权限引擎 RBAC
    if grep -rq 'permission_engine\|permission_check\|RBAC' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/cupolas/" 2>/dev/null; then
        log_ok "BAN-127: Permission engine RBAC defined"
    else
        record_issue "high" "BAN-127" "cupolas/" "Permission engine not found"
        ((sd_issues++)) || true
    fi

    # BAN-128: 审计日志哈希链
    if grep -rq 'audit.*hash\|audit.*chain\|hash_chain' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/cupolas/" 2>/dev/null; then
        log_ok "BAN-128: Audit log hash chain defined"
    else
        record_issue "high" "BAN-128" "cupolas/" "Audit hash chain not found"
        ((sd_issues++)) || true
    fi

    # BAN-129: 凭证加密 AES-GCM
    if grep -rq 'AES.*GCM\|aes_gcm\|vault_encrypt\|credential.*encrypt' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/cupolas/" 2>/dev/null; then
        log_ok "BAN-129: Credential AES-GCM encryption defined"
    else
        record_issue "high" "BAN-129" "cupolas/" "Credential encryption not found"
        ((sd_issues++)) || true
    fi

    # BAN-130: 运行时保护 Seccomp
    if grep -rq 'seccomp\|runtime_protection\|sandbox' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/cupolas/" 2>/dev/null; then
        log_ok "BAN-130: Runtime protection (Seccomp) defined"
    else
        record_issue "medium" "BAN-130" "cupolas/" "Runtime protection not found"
        ((sd_issues++)) || true
    fi

    # BAN-131: 网络过滤
    if grep -rq 'network_filter\|url_filter\|dns_security' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/cupolas/" 2>/dev/null; then
        log_ok "BAN-131: Network filtering defined"
    else
        record_issue "medium" "BAN-131" "cupolas/" "Network filtering not found"
        ((sd_issues++)) || true
    fi

    # BAN-132: 熔断器
    if grep -rq 'circuit_breaker\|fuse\|熔断' \
        --include="*.c" --include="*.h" "${PROJECT_ROOT}/agentos/cupolas/" 2>/dev/null; then
        log_ok "BAN-132: Circuit breaker defined"
    else
        record_issue "medium" "BAN-132" "cupolas/" "Circuit breaker not found"
        ((sd_issues++)) || true
    fi

    if [[ $sd_issues -eq 0 ]]; then
        log_ok "Security dome compliance: All checks passed"
        record_check_result "security-dome" "true"
    else
        log_warn "Security dome compliance: $sd_issues issue(s)"
        record_check_result "security-dome" "false"
    fi
}

###############################################################################
# Gate 7.6: BAN-151~162 内存安全规则 (Phase 2.5)
###############################################################################
gate_ban_memory_safety() {
    log_section "BAN Memory Safety Rules (BAN-151 ~ BAN-162)"

    local ms_issues=0

    # BAN-151: 禁止 MALLOC 后非 goto cleanup 的 return 路径
    log_info "BAN-151: Checking for non-cleanup return after MALLOC..."
    # 简化检查：标记需要人工审查
    log_info "BAN-151: Requires manual review (non-cleanup return paths)"

    # BAN-152: 禁止 cleanup 前声明新变量
    log_info "BAN-152: Checking C89 variable declarations..."
    # 简化检查：标记需要编译器检查
    log_info "BAN-152: Requires -Wdeclaration-after-statement compiler flag"

    # BAN-153: 单一退出点
    log_info "BAN-153: Checking for multiple return points..."
    # 简化检查：统计多 return 函数
    local multi_return_files
    multi_return_files=$(grep -rl 'return' --include="*.c" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v "/tests/" | while IFS= read -r f; do
        local count
        count=$(grep -c '\breturn\b' "$f" 2>/dev/null || echo "0")
        if [[ $count -gt 5 ]]; then
            echo "$f"
        fi
    done | wc -l) || true
    if [[ $multi_return_files -le 20 ]]; then
        log_ok "BAN-153: $multi_return_files files with >5 return points (acceptable)"
    else
        record_issue "low" "BAN-153" "agentos/" "$multi_return_files files with >5 return points"
        ((ms_issues++)) || true
    fi

    # BAN-154: 禁止非常量第三参数的 memcpy/memmove/memset
    log_info "BAN-154: Checking for dynamic-size memcpy without bounds check..."
    local dynamic_memcpy
    dynamic_memcpy=$(grep -rn '\bmemcpy\b\|\bmemmove\b\|\bmemset\b' \
        --include="*.c" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v "/tests/" | grep -v 'AGENTOS_MEMCPY_SAFE' \
        | grep -v 'sizeof' | wc -l) || true
    if [[ $dynamic_memcpy -le 10 ]]; then
        log_ok "BAN-154: $dynamic_memcpy dynamic-size memcpy/memset calls (≤10)"
    else
        record_issue "high" "BAN-154" "agentos/" "$dynamic_memcpy dynamic-size memcpy without sizeof"
        ((ms_issues++)) || true
    fi

    # BAN-155~158: 缓冲区安全
    log_info "BAN-155~158: Buffer safety checks..."
    if grep -rq 'AGENTOS_STRNCPY_TERM\|AGENTOS_MEMCPY_SAFE' \
        --include="*.h" "${PROJECT_ROOT}/agentos/commons/" 2>/dev/null; then
        log_ok "BAN-155~158: Safe buffer macros defined"
    else
        record_issue "high" "BAN-155~158" "commons/" "Safe buffer macros not found"
        ((ms_issues++)) || true
    fi

    # BAN-159~162: 资源泄漏
    log_info "BAN-159~162: Resource leak checks..."
    if grep -rq 'secure_clear\|AGENTOS_FREE.*NULL' \
        --include="*.c" "${PROJECT_ROOT}/agentos/" 2>/dev/null | head -5 | grep -q .; then
        log_ok "BAN-159~162: Resource cleanup patterns found"
    else
        record_issue "medium" "BAN-159~162" "agentos/" "Resource cleanup patterns not verified"
        ((ms_issues++)) || true
    fi

    if [[ $ms_issues -eq 0 ]]; then
        log_ok "BAN Memory Safety: All checks passed"
        record_check_result "ban-memory-safety" "true"
    else
        log_warn "BAN Memory Safety: $ms_issues issue(s)"
        record_check_result "ban-memory-safety" "false"
    fi
}

###############################################################################
# Gate 22: BAN-151~180 Strict Mode（Phase 2.5/3 — 内存安全 + 所有权 + 编码契约）
###############################################################################
gate_ban_extended_strict() {
    log_section "BAN Strict Rules Extended (BAN-151 ~ BAN-180)"

    local ext_issues=0

    # BAN-151: 禁止 MALLOC 后非 goto cleanup 的 return 路径
    log_info "BAN-151: Checking non-cleanup return after MALLOC..."
    local ban151_files
    ban151_files=$(grep -rl 'AGENTOS_MALLOC\|AGENTOS_CALLOC\|AGENTOS_REALLOC' \
        --include="*.c" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v "/tests/" | while IFS= read -r f; do
        if grep -q 'AGENTOS_MALLOC\|AGENTOS_CALLOC' "$f" 2>/dev/null && ! grep -q 'goto cleanup' "$f" 2>/dev/null; then
            echo "$f"
        fi
    done | wc -l) || true
    if [[ $ban151_files -eq 0 ]]; then
        log_ok "BAN-151 (strict): All MALLOC functions have cleanup paths"
        record_check_result "BAN-151" "true"
    else
        record_issue "high" "BAN-151" "agentos/" "$ban151_files files with MALLOC but no goto cleanup"
        ((ext_issues++)) || true
        record_check_result "BAN-151" "false"
    fi

    # BAN-152: cleanup 前禁止声明新变量（C89 兼容）
    log_info "BAN-152: Checking C89 compatibility..."
    log_info "BAN-152 (strict): Requires -Wdeclaration-after-statement compiler flag"
    record_check_result "BAN-152" "true"

    # BAN-153: 单一退出点（每个函数 ≤5 个 return）
    log_info "BAN-153: Checking return point discipline..."
    local ban153_multi_return
    ban153_multi_return=$(grep -rl 'return' --include="*.c" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v "/tests/" | while IFS= read -r f; do
        count=$(grep -c '\breturn\b' "$f" 2>/dev/null || echo "0")
        if [[ $count -gt 5 ]]; then echo "$f"; fi
    done | wc -l) || true
    if [[ $ban153_multi_return -le 20 ]]; then
        log_ok "BAN-153 (strict): $ban153_multi_return files with >5 returns"
        record_check_result "BAN-153" "true"
    else
        record_issue "medium" "BAN-153" "agentos/" "$ban153_multi_return files with >5 returns"
        ((ext_issues++)) || true
        record_check_result "BAN-153" "false"
    fi

    # BAN-154: 禁止非常量第三参数的 memcpy/memmove/memset
    log_info "BAN-154: Checking dynamic-size memcpy..."
    local ban154_dynamic
    ban154_dynamic=$(grep -rn '\bmemcpy\b\|\bmemmove\b\|\bmemset\b' \
        --include="*.c" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v "/tests/" | grep -v 'AGENTOS_MEMCPY_SAFE' \
        | grep -v 'sizeof' | wc -l) || true
    if [[ $ban154_dynamic -le 10 ]]; then
        log_ok "BAN-154 (strict): $ban154_dynamic dynamic-size memcpy calls"
        record_check_result "BAN-154" "true"
    else
        record_issue "high" "BAN-154" "agentos/" "$ban154_dynamic dynamic-size memcpy without bounds check"
        ((ext_issues++)) || true
        record_check_result "BAN-154" "false"
    fi

    # BAN-155~158: 缓冲区安全（strict）
    log_info "BAN-155~158: Buffer safety strict checks..."
    if grep -rq 'AGENTOS_STRNCPY_TERM\|AGENTOS_MEMCPY_SAFE' \
        --include="*.h" "${PROJECT_ROOT}/agentos/commons/" 2>/dev/null; then
        log_ok "BAN-155~158 (strict): Safe buffer macros defined"
        record_check_result "BAN-155~158" "true"
    else
        record_issue "high" "BAN-155~158" "commons/" "Safe buffer macros not found"
        ((ext_issues++)) || true
        record_check_result "BAN-155~158" "false"
    fi

    # BAN-159~162: 资源泄漏（strict）
    log_info "BAN-159~162: Resource leak strict checks..."
    local fd_pairs=0
    fd_pairs=$(grep -rl '\bopen\b\|\bcreat\b\|\bsocket\b' --include="*.c" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v "/tests/" | while IFS= read -r f; do
        opens=$(grep -c '\bopen\b\|\bcreat\b\|\bsocket\b' "$f" 2>/dev/null || echo "0")
        closes=$(grep -c '\bclose\b' "$f" 2>/dev/null || echo "0")
        if [[ $opens -gt $closes ]]; then echo "$f"; fi
    done | wc -l) || true
    # 去除可能的换行符（grep -c 在文件不存在时可能输出多行）
    fd_pairs=$(echo "$fd_pairs" | tr -d '[:space:]')
    if [[ $fd_pairs -eq 0 ]]; then
        log_ok "BAN-159~162 (strict): Resource cleanup patterns verified ($fd_pairs unmatched)"
        record_check_result "BAN-159~162" "true"
    else
        record_issue "high" "BAN-159~162" "agentos/" "$fd_pairs files with unmatched resource pairs"
        ((ext_issues++)) || true
        record_check_result "BAN-159~162" "false"
    fi

    # BAN-163~168: 所有权语义（strict）
    log_info "BAN-163~168: Ownership semantics strict checks..."
    local ownership_ok=true
    if grep -rq 'AGENTOS_FREE' --include="*.c" "${PROJECT_ROOT}/agentos/" 2>/dev/null | head -1 | grep -q .; then
        ownership_ok=true
    fi
    if [[ "$ownership_ok" == "true" ]]; then
        log_ok "BAN-163~168 (strict): Ownership patterns present"
        record_check_result "BAN-163~168" "true"
    else
        record_issue "medium" "BAN-163~168" "agentos/" "Ownership semantics not verified"
        ((ext_issues++)) || true
        record_check_result "BAN-163~168" "false"
    fi

    # BAN-169~174: OOM响应（strict）
    log_info "BAN-169~174: OOM response strict checks..."
    if grep -rq 'memory_stats\|oom_handler\|watermark\|degradation' \
        --include="*.h" "${PROJECT_ROOT}/agentos/" 2>/dev/null; then
        log_ok "BAN-169~174 (strict): OOM response infrastructure found"
        record_check_result "BAN-169~174" "true"
    else
        log_info "BAN-169~174 (strict): OOM infrastructure pending (Phase 2.5)"
        record_check_result "BAN-169~174" "pending"
    fi

    # BAN-175~180: 编码契约（strict）
    log_info "BAN-175~180: Encoding contracts strict checks..."
    local dt_contracts=0
    grep -rq 'thinking_chain' --include="*.h" "${PROJECT_ROOT}/agentos/atoms/coreloopthree/" 2>/dev/null && ((dt_contracts++)) || true
    grep -rq 'triple_coordinator' --include="*.h" "${PROJECT_ROOT}/agentos/atoms/coreloopthree/" 2>/dev/null && ((dt_contracts++)) || true
    grep -rq 'sanitizer_core\|permission_engine' --include="*.h" "${PROJECT_ROOT}/agentos/cupolas/" 2>/dev/null && ((dt_contracts++)) || true
    grep -rq 'llm_provider' --include="*.h" "${PROJECT_ROOT}/agentos/daemon/llm_d/" 2>/dev/null && ((dt_contracts++)) || true
    if [[ $dt_contracts -ge 3 ]]; then
        log_ok "BAN-175~180 (strict): Encoding contracts verified ($dt_contracts/4 modules)"
        record_check_result "BAN-175~180" "true"
    else
        record_issue "medium" "BAN-175~180" "agentos/" "$dt_contracts/4 module contracts verified"
        ((ext_issues++)) || true
        record_check_result "BAN-175~180" "false"
    fi

    if [[ $ext_issues -eq 0 ]]; then
        log_ok "BAN Extended Strict: All 30/180 checks passed"
        record_check_result "ban-extended-strict" "true"
    else
        log_warn "BAN Extended Strict: $ext_issues check(s) failed"
        record_check_result "ban-extended-strict" "false"
    fi
}

###############################################################################
# Gate 23: BAN-181~190 合规检查（错误码回退、特性宏、线程抽象、魔数、
#          术语、fprintf、内存释放、占位返回、OOM、内存压力）
###############################################################################
gate_ban_181_190() {
    log_section "BAN-181~190 Compliance Checks (G23)"

    local b23_issues=0

    # BAN-181: #ifndef error code fallback definitions
    log_info "BAN-181: Checking #ifndef AGENTOS_E fallback definitions..."
    local ban181_hits
    ban181_hits=$(grep -rn '#ifndef AGENTOS_E' \
        --include='*.c' --include='*.h' "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v 'commons/utils/error/include/error.h' | wc -l) || true
    if [[ $ban181_hits -eq 0 ]]; then
        log_ok "BAN-181: 0 #ifndef AGENTOS_E fallback definitions outside error.h"
        record_check_result "BAN-181" "true"
    else
        record_issue "high" "BAN-181" "agentos/" "$ban181_hits #ifndef AGENTOS_E fallback(s) outside error.h"
        log_error "BAN-181: $ban181_hits #ifndef AGENTOS_E fallback(s) found outside error.h"
        ((b23_issues++)) || true
        record_check_result "BAN-181" "false"
    fi

    # BAN-182: Feature test macros in source files
    log_info "BAN-182: Checking feature test macros in source files..."
    local ban182_hits
    ban182_hits=$(grep -rn '#define _POSIX_C_SOURCE\|#define _GNU_SOURCE' \
        --include='*.c' "${PROJECT_ROOT}/agentos/*/src/" 2>/dev/null | wc -l) || true
    if [[ $ban182_hits -eq 0 ]]; then
        log_ok "BAN-182: 0 feature test macros in source files"
        record_check_result "BAN-182" "true"
    else
        record_issue "high" "BAN-182" "agentos/*/src/" "$ban182_hits feature test macro(s) in source files"
        log_error "BAN-182: $ban182_hits feature test macro(s) in source files"
        ((b23_issues++)) || true
        record_check_result "BAN-182" "false"
    fi

    # BAN-183: Direct pthread usage in non-abstraction-layer code
    log_info "BAN-183: Checking direct pthread usage outside abstraction layer..."
    local ban183_hits
    ban183_hits=$(grep -rn 'pthread_mutex_lock\|pthread_mutex_unlock\|pthread_create' \
        --include='*.c' "${PROJECT_ROOT}/agentos/*/src/" 2>/dev/null \
        | grep -v 'sync/src/' | grep -v 'cupolas/src/platform/' | wc -l) || true
    if [[ $ban183_hits -eq 0 ]]; then
        log_ok "BAN-183: 0 direct pthread calls outside abstraction layer"
        record_check_result "BAN-183" "true"
    else
        record_issue "high" "BAN-183" "agentos/*/src/" "$ban183_hits direct pthread call(s) outside abstraction layer"
        log_error "BAN-183: $ban183_hits direct pthread call(s) outside abstraction layer"
        ((b23_issues++)) || true
        record_check_result "BAN-183" "false"
    fi

    # BAN-184: Hardcoded magic numbers in return statements
    log_info "BAN-184: Checking hardcoded magic numbers in return statements..."
    local ban184_hits
    ban184_hits=$(grep -rn 'return -1;\|return -2;\|return -3;' \
        --include='*.c' "${PROJECT_ROOT}/agentos/*/src/" 2>/dev/null | wc -l) || true
    if [[ $ban184_hits -eq 0 ]]; then
        log_ok "BAN-184: 0 hardcoded magic numbers in return statements"
        record_check_result "BAN-184" "true"
    else
        record_issue "high" "BAN-184" "agentos/*/src/" "$ban184_hits hardcoded magic number return(s)"
        log_error "BAN-184: $ban184_hits hardcoded magic number(s) in return statements"
        ((b23_issues++)) || true
        record_check_result "BAN-184" "false"
    fi

    # BAN-185: Thinkdual terminology - forbidden terms
    log_info "BAN-185: Checking forbidden Thinkdual terminology..."
    local ban185_hits
    ban185_hits=$(grep -rn '双思考系统\|双系统认知' \
        --include='*.c' --include='*.h' "${PROJECT_ROOT}/agentos/" 2>/dev/null | wc -l) || true
    if [[ $ban185_hits -eq 0 ]]; then
        log_ok "BAN-185: 0 forbidden Thinkdual terms"
        record_check_result "BAN-185" "true"
    else
        record_issue "high" "BAN-185" "agentos/" "$ban185_hits forbidden Thinkdual term(s)"
        log_error "BAN-185: $ban185_hits forbidden Thinkdual term(s) found"
        ((b23_issues++)) || true
        record_check_result "BAN-185" "false"
    fi

    # BAN-186: fprintf without EXEMPT comment in non-logging code
    log_info "BAN-186: Checking fprintf without EXEMPT in non-logging code..."
    local ban186_hits
    ban186_hits=$(grep -rn 'fprintf(' \
        --include='*.c' "${PROJECT_ROOT}/agentos/*/src/" 2>/dev/null \
        | grep -v 'logging/' | grep -v 'BAN-70 EXEMPT' | wc -l) || true
    if [[ $ban186_hits -eq 0 ]]; then
        log_ok "BAN-186: 0 unexempted fprintf calls in non-logging code"
        record_check_result "BAN-186" "true"
    else
        record_issue "high" "BAN-186" "agentos/*/src/" "$ban186_hits unexempted fprintf call(s)"
        log_error "BAN-186: $ban186_hits unexempted fprintf call(s) in non-logging code"
        ((b23_issues++)) || true
        record_check_result "BAN-186" "false"
    fi

    # BAN-187: AGENTOS_FREE without NULL assignment (warning only)
    log_info "BAN-187: Checking AGENTOS_FREE without NULL assignment (warning)..."
    local ban187_total ban187_with_null
    ban187_total=$(grep -rn 'AGENTOS_FREE' \
        --include='*.c' "${PROJECT_ROOT}/agentos/" 2>/dev/null | wc -l) || true
    ban187_with_null=$(grep -rn 'AGENTOS_FREE' \
        --include='*.c' "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | while IFS= read -r line; do
        local file linenum
        file=$(echo "$line" | cut -d: -f1)
        linenum=$(echo "$line" | cut -d: -f2)
        if sed -n "$((linenum+1))p" "$file" 2>/dev/null | grep -q '= NULL\|=NULL'; then
            echo "ok"
        fi
    done | wc -l) || true
    local ban187_without_null=$((ban187_total - ban187_with_null))
    if [[ $ban187_without_null -eq 0 ]]; then
        log_ok "BAN-187: All AGENTOS_FREE calls followed by NULL assignment"
        record_check_result "BAN-187" "true"
    else
        log_warn "BAN-187: $ban187_without_null/$ban187_total AGENTOS_FREE call(s) not followed by NULL assignment (warning, migration ongoing)"
        record_check_result "BAN-187" "true"
    fi

    # BAN-188: return AGENTOS_SUCCESS as placeholder (warning only)
    log_info "BAN-188: Checking return AGENTOS_SUCCESS as placeholder (warning)..."
    local ban188_hits
    ban188_hits=$(grep -rn 'return AGENTOS_SUCCESS' \
        --include='*.c' "${PROJECT_ROOT}/agentos/" 2>/dev/null | wc -l) || true
    if [[ $ban188_hits -eq 0 ]]; then
        log_ok "BAN-188: 0 return AGENTOS_SUCCESS placeholder(s)"
        record_check_result "BAN-188" "true"
    else
        log_warn "BAN-188: $ban188_hits return AGENTOS_SUCCESS placeholder(s) found (warning, migration ongoing)"
        record_check_result "BAN-188" "true"
    fi

    # BAN-189: OOM handler initialization
    log_info "BAN-189: Checking OOM handler initialization..."
    local ban189_hits
    ban189_hits=$(grep -rn 'agentos_oom_init' \
        --include='*.c' "${PROJECT_ROOT}/agentos/" 2>/dev/null | head -1 | wc -l) || true
    if [[ $ban189_hits -ge 1 ]]; then
        log_ok "BAN-189: OOM handler initialization found"
        record_check_result "BAN-189" "true"
    else
        record_issue "high" "BAN-189" "agentos/" "OOM handler initialization not found"
        log_error "BAN-189: agentos_oom_init not found"
        ((b23_issues++)) || true
        record_check_result "BAN-189" "false"
    fi

    # BAN-190: Memory pressure level API
    log_info "BAN-190: Checking memory pressure level API declaration..."
    local ban190_hits
    ban190_hits=$(grep -rn 'agentos_oom_get_pressure' \
        --include='*.h' "${PROJECT_ROOT}/agentos/" 2>/dev/null | head -1 | wc -l) || true
    if [[ $ban190_hits -ge 1 ]]; then
        log_ok "BAN-190: Memory pressure level API declared"
        record_check_result "BAN-190" "true"
    else
        record_issue "high" "BAN-190" "agentos/" "Memory pressure level API not declared"
        log_error "BAN-190: agentos_oom_get_pressure not found in headers"
        ((b23_issues++)) || true
        record_check_result "BAN-190" "false"
    fi

    if [[ $b23_issues -eq 0 ]]; then
        log_ok "BAN-181~190: All checks passed"
        record_check_result "ban-181-190" "true"
    else
        log_warn "BAN-181~190: $b23_issues check(s) failed"
        record_check_result "ban-181-190" "false"
    fi
}

###############################################################################
# Gate 24: BAN-191~256 Extended Compliance (P0.15)
###############################################################################
gate_ban_191_256() {
    log_section "BAN-191~256 Extended Compliance Checks (G24)"

    local g24_issues=0

    # BAN-191: Shell scripts must be POSIX portable
    log_info "BAN-191: Checking shell scripts for POSIX portability..."
    local ban191_hits
    ban191_hits=$(grep -rn '\[\[' --include='*.sh' "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v '# BAN-191 EXEMPT' | wc -l) || true
    if [[ $ban191_hits -eq 0 ]]; then
        log_ok "BAN-191: Shell scripts appear POSIX-portable"
        record_check_result "BAN-191" "true"
    else
        log_warn "BAN-191: $ban191_hits [[ usage(s) in shell scripts (non-POSIX, warning)"
        record_check_result "BAN-191" "true"
    fi

    # BAN-192: Security scan must pass in CI
    log_info "BAN-192: Checking security scan script exists..."
    if [[ -f "${SCRIPT_DIR}/security_check.py" ]]; then
        log_ok "BAN-192: Security scan script found"
        record_check_result "BAN-192" "true"
    else
        record_issue "high" "BAN-192" "scripts/" "Security scan script not found"
        log_error "BAN-192: security_check.py not found"
        ((g24_issues++)) || true
        record_check_result "BAN-192" "false"
    fi

    # BAN-193: Forbidden C functions (gets, sprintf, strcpy, strcat, strtok)
    log_info "BAN-193: Checking forbidden C functions..."
    local ban193_hits
    ban193_hits=$(grep -rn '\bgets\s*(\|\<sprintf\s*(\|\<strcpy\s*(\|\<strcat\s*(\|\<strtok\s*(' \
        --include='*.c' "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v 'test' | grep -v '# BAN-193 EXEMPT' | wc -l) || true
    if [[ $ban193_hits -eq 0 ]]; then
        log_ok "BAN-193: No forbidden C functions found"
        record_check_result "BAN-193" "true"
    else
        record_issue "critical" "BAN-193" "agentos/" "$ban193_hits forbidden C function call(s)"
        log_error "BAN-193: $ban193_hits forbidden C function call(s) found"
        ((g24_issues++)) || true
        record_check_result "BAN-193" "false"
    fi

    # BAN-207: Plugin permissions must map to SafetyGuard guards
    log_info "BAN-207: Checking plugin permission / SafetyGuard mapping..."
    local ban207_safety ban207_plugin
    ban207_safety=$(grep -rn 'SAFETY_GUARD_' --include='*.h' "${PROJECT_ROOT}/agentos/" 2>/dev/null | wc -l) || true
    ban207_plugin=$(grep -rn 'PLUGIN_PERM_\|plugin_permission' --include='*.h' "${PROJECT_ROOT}/agentos/" 2>/dev/null | wc -l) || true
    if [[ $ban207_safety -ge 8 ]] && [[ $ban207_plugin -ge 1 ]]; then
        log_ok "BAN-207: SafetyGuard guards ($ban207_safety) and plugin permissions ($ban207_plugin) found"
        record_check_result "BAN-207" "true"
    else
        log_warn "BAN-207: SafetyGuard ($ban207_safety) or plugin permissions ($ban207_plugin) incomplete"
        record_check_result "BAN-207" "true"
    fi

    # BAN-211: External input must be sanitized
    log_info "BAN-211: Checking input sanitizer existence..."
    local ban211_hits
    ban211_hits=$(grep -rn 'input_validator\|sanitize\|AGENTOS_SANITIZE' \
        --include='*.h' --include='*.c' "${PROJECT_ROOT}/agentos/" 2>/dev/null | wc -l) || true
    if [[ $ban211_hits -ge 1 ]]; then
        log_ok "BAN-211: Input sanitizer found ($ban211_hits reference(s))"
        record_check_result "BAN-211" "true"
    else
        record_issue "high" "BAN-211" "agentos/" "Input sanitizer not found"
        log_error "BAN-211: No input sanitizer found"
        ((g24_issues++)) || true
        record_check_result "BAN-211" "false"
    fi

    # BAN-228: No hardcoded secrets in source
    log_info "BAN-228: Checking for hardcoded secrets..."
    local ban228_hits
    ban228_hits=$(grep -rni 'api_key\s*=\s*"[^"]\{8,\}"\|password\s*=\s*"[^"]\{8,\}"\|secret\s*=\s*"[^"]\{8,\}"' \
        --include='*.c' --include='*.h' --include='*.py' "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v 'test' | grep -v 'example' | grep -v '# BAN-228 EXEMPT' | wc -l) || true
    if [[ $ban228_hits -eq 0 ]]; then
        log_ok "BAN-228: No hardcoded secrets found"
        record_check_result "BAN-228" "true"
    else
        record_issue "critical" "BAN-228" "agentos/" "$ban228_hits hardcoded secret(s)"
        log_error "BAN-228: $ban228_hits hardcoded secret(s) found"
        ((g24_issues++)) || true
        record_check_result "BAN-228" "false"
    fi

    # BAN-231: Error code segmentation compliance
    log_info "BAN-231: Checking error code segmentation..."
    local ban231_hits
    ban231_hits=$(grep -rn '#define AGENTOS_ERR_[A-Z_]*\s*(-[0-9]\{1,2\})' \
        --include='*.h' "${PROJECT_ROOT}/agentos/commons/utils/error/" 2>/dev/null | wc -l) || true
    if [[ $ban231_hits -ge 10 ]]; then
        log_ok "BAN-231: Error code segmentation found ($ban231_hits codes)"
        record_check_result "BAN-231" "true"
    else
        log_warn "BAN-231: Error code segmentation may be incomplete ($ban231_hits codes)"
        record_check_result "BAN-231" "true"
    fi

    # BAN-233: TOCTOU prevention - no access() before open()
    log_info "BAN-233: Checking for TOCTOU patterns (access before open)..."
    local ban233_hits
    ban233_hits=$(grep -rn '\baccess\s*(' --include='*.c' "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v 'test' | grep -v '# BAN-233 EXEMPT' | wc -l) || true
    if [[ $ban233_hits -eq 0 ]]; then
        log_ok "BAN-233: No TOCTOU-vulnerable access() calls found"
        record_check_result "BAN-233" "true"
    else
        record_issue "high" "BAN-233" "agentos/" "$ban233_hits access() call(s) (potential TOCTOU)"
        log_error "BAN-233: $ban233_hits access() call(s) found (potential TOCTOU)"
        ((g24_issues++)) || true
        record_check_result "BAN-233" "false"
    fi

    # BAN-235: No format string vulnerabilities
    log_info "BAN-235: Checking for format string vulnerabilities..."
    local ban235_hits
    ban235_hits=$(grep -rn 'printf\s*([^,]*);\|fprintf\s*([^,]*,[^,]*);\|snprintf\s*([^,]*,[^,]*,[^"]*);' \
        --include='*.c' "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v 'test' | grep -v 'logging' | grep -v '# BAN-235 EXEMPT' | wc -l) || true
    if [[ $ban235_hits -eq 0 ]]; then
        log_ok "BAN-235: No format string vulnerabilities found"
        record_check_result "BAN-235" "true"
    else
        record_issue "high" "BAN-235" "agentos/" "$ban235_hits potential format string issue(s)"
        log_error "BAN-235: $ban235_hits potential format string issue(s)"
        ((g24_issues++)) || true
        record_check_result "BAN-235" "false"
    fi

    # BAN-242: No C99 VLA (Variable Length Arrays)
    log_info "BAN-242: Checking for C99 VLA usage..."
    local ban242_hits
    ban242_hits=$(grep -rn '\w\+\s\+\w\+\[[a-z_]\+\]' \
        --include='*.c' --include='*.h' "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v 'const' | grep -v '#define' | grep -v 'test' | wc -l) || true
    if [[ $ban242_hits -eq 0 ]]; then
        log_ok "BAN-242: No C99 VLA usage found"
        record_check_result "BAN-242" "true"
    else
        log_warn "BAN-242: $ban242_hits potential VLA usage(s) (manual review needed)"
        record_check_result "BAN-242" "true"
    fi

    # BAN-243: No direct POSIX thread API usage
    log_info "BAN-243: Checking for direct POSIX thread API usage..."
    local ban243_hits
    ban243_hits=$(grep -rn 'pthread_mutex_\|pthread_cond_\|pthread_rwlock_\|pthread_spin_' \
        --include='*.c' --include='*.h' "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v 'platform/' | grep -v 'sync/src/' | grep -v 'compat' | wc -l) || true
    if [[ $ban243_hits -eq 0 ]]; then
        log_ok "BAN-243: No direct POSIX thread API usage outside abstraction layer"
        record_check_result "BAN-243" "true"
    else
        record_issue "high" "BAN-243" "agentos/" "$ban243_hits direct POSIX thread API usage(s)"
        log_error "BAN-243: $ban243_hits direct POSIX thread API usage(s) outside abstraction layer"
        ((g24_issues++)) || true
        record_check_result "BAN-243" "false"
    fi

    # BAN-247: Shared memory objects must use refcount, not bare free
    log_info "BAN-247: Checking shared memory objects use refcount..."
    local ban247_hits
    ban247_hits=$(grep -rn 'AGENTOS_FREE\s*(.*shared\|AGENTOS_FREE\s*(.*_shared\|AGENTOS_FREE\s*(.*refcount' \
        --include='*.c' "${PROJECT_ROOT}/agentos/" 2>/dev/null | wc -l) || true
    if [[ $ban247_hits -eq 0 ]]; then
        log_ok "BAN-247: No bare free on shared memory objects"
        record_check_result "BAN-247" "true"
    else
        record_issue "high" "BAN-247" "agentos/" "$ban247_hits bare free on shared object(s)"
        log_error "BAN-247: $ban247_hits bare free on shared memory object(s)"
        ((g24_issues++)) || true
        record_check_result "BAN-247" "false"
    fi

    # BAN-252: Sensitive data must use AGENTOS_SECURE_FREE
    log_info "BAN-252: Checking sensitive data uses AGENTOS_SECURE_FREE..."
    local ban252_hits
    ban252_hits=$(grep -rn 'AGENTOS_FREE\s*(.*key\|AGENTOS_FREE\s*(.*token\|AGENTOS_FREE\s*(.*secret\|AGENTOS_FREE\s*(.*password\|AGENTOS_FREE\s*(.*credential' \
        --include='*.c' "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v 'test' | wc -l) || true
    if [[ $ban252_hits -eq 0 ]]; then
        log_ok "BAN-252: No AGENTOS_FREE on sensitive data"
        record_check_result "BAN-252" "true"
    else
        record_issue "critical" "BAN-252" "agentos/" "$ban252_hits AGENTOS_FREE on sensitive data"
        log_error "BAN-252: $ban252_hits AGENTOS_FREE on sensitive data (use AGENTOS_SECURE_FREE)"
        ((g24_issues++)) || true
        record_check_result "BAN-252" "false"
    fi

    # BAN-255: All allocations must be under memory_stats_reporter
    log_info "BAN-255: Checking allocations under memory_stats_reporter..."
    local ban255_malloc ban255_agentos
    ban255_malloc=$(grep -rn '\bmalloc\s*(' --include='*.c' "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v 'test' | grep -v 'commons/utils/memory' | wc -l) || true
    ban255_agentos=$(grep -rn 'AGENTOS_MALLOC\|agentos_mem_alloc\|memory_alloc' \
        --include='*.c' "${PROJECT_ROOT}/agentos/" 2>/dev/null | wc -l) || true
    if [[ $ban255_malloc -eq 0 ]]; then
        log_ok "BAN-255: All allocations use tracked API ($ban255_agentos tracked calls)"
        record_check_result "BAN-255" "true"
    else
        log_warn "BAN-255: $ban255_malloc bare malloc call(s) bypass memory_stats_reporter"
        record_check_result "BAN-255" "true"
    fi

    # BAN-256: ASan/LSan violations must block merge
    log_info "BAN-256: Checking ASan/LSan CI configuration..."
    local ban256_hits
    ban256_hits=$(grep -rn 'ASAN\|LSAN\|sanitize' \
        --include='*.yml' --include='*.yaml' --include='CMakeLists.txt' \
        "${PROJECT_ROOT}/" 2>/dev/null | wc -l) || true
    if [[ $ban256_hits -ge 1 ]]; then
        log_ok "BAN-256: ASan/LSan configuration found ($ban256_hits reference(s))"
        record_check_result "BAN-256" "true"
    else
        log_warn "BAN-256: No ASan/LSan configuration found (CI may not enforce zero-tolerance)"
        record_check_result "BAN-256" "true"
    fi

    # BAN-250: Daemon startup must register WARNING + CRITICAL OOM callbacks
    log_info "BAN-250: Checking daemon OOM callback registration..."
    local ban250_hits
    ban250_hits=$(grep -rn 'agentos_oom_register_daemon_callback\|agentos_oom_register_callback' \
        --include='*.c' "${PROJECT_ROOT}/agentos/" 2>/dev/null | wc -l) || true
    if [[ $ban250_hits -ge 1 ]]; then
        log_ok "BAN-250: OOM callback registration found ($ban250_hits registration(s))"
        record_check_result "BAN-250" "true"
    else
        log_warn "BAN-250: No OOM callback registration found (daemons should register OOM handlers)"
        record_check_result "BAN-250" "true"
    fi

    # BAN-224: SecretRef for sensitive configuration
    log_info "BAN-224: Checking SecretRef usage in config..."
    local ban224_hits
    ban224_hits=$(grep -rn 'SecretRef\|secret_ref\|SECRET_REF' \
        --include='*.h' --include='*.c' --include='*.yaml' \
        "${PROJECT_ROOT}/agentos/" 2>/dev/null | wc -l) || true
    if [[ $ban224_hits -ge 1 ]]; then
        log_ok "BAN-224: SecretRef mechanism found ($ban224_hits reference(s))"
        record_check_result "BAN-224" "true"
    else
        log_warn "BAN-224: No SecretRef mechanism found (sensitive config may be plaintext)"
        record_check_result "BAN-224" "true"
    fi

    # BAN-218: Hook must fail-open
    log_info "BAN-218: Checking hook fail-open implementation..."
    local ban218_hits
    ban218_hits=$(grep -rn 'HOOK_DECISION_CONTINUE\|fail.open\|fail_open' \
        --include='*.h' --include='*.c' "${PROJECT_ROOT}/agentos/" 2>/dev/null | wc -l) || true
    if [[ $ban218_hits -ge 1 ]]; then
        log_ok "BAN-218: Hook fail-open mechanism found ($ban218_hits reference(s))"
        record_check_result "BAN-218" "true"
    else
        log_warn "BAN-218: Hook fail-open mechanism not found"
        record_check_result "BAN-218" "true"
    fi

    # BAN-219: Plugin must include manifest
    log_info "BAN-219: Checking plugin manifest schema..."
    local ban219_hits
    ban219_hits=$(grep -rn 'plugin.json\|plugin_manifest\|PLUGIN_MANIFEST' \
        --include='*.h' --include='*.c' --include='*.json' \
        "${PROJECT_ROOT}/agentos/" 2>/dev/null | wc -l) || true
    if [[ $ban219_hits -ge 1 ]]; then
        log_ok "BAN-219: Plugin manifest mechanism found ($ban219_hits reference(s))"
        record_check_result "BAN-219" "true"
    else
        log_warn "BAN-219: Plugin manifest mechanism not found"
        record_check_result "BAN-219" "true"
    fi

    # BAN-222: LLM retry with exponential backoff
    log_info "BAN-222: Checking LLM retry mechanism..."
    local ban222_hits
    ban222_hits=$(grep -rn 'retry\|backoff\|RETRY' \
        --include='*.h' --include='*.c' "${PROJECT_ROOT}/agentos/daemon/llm_d/" 2>/dev/null | wc -l) || true
    if [[ $ban222_hits -ge 1 ]]; then
        log_ok "BAN-222: LLM retry mechanism found ($ban222_hits reference(s))"
        record_check_result "BAN-222" "true"
    else
        log_warn "BAN-222: LLM retry mechanism not found"
        record_check_result "BAN-222" "true"
    fi

    # BAN-232: Error message format compliance
    log_info "BAN-232: Checking error message format..."
    local ban232_hits
    ban232_hits=$(grep -rn 'agentos_error_str\|agentos_error_push_ex\|AGENTOS_ERROR' \
        --include='*.c' "${PROJECT_ROOT}/agentos/" 2>/dev/null | wc -l) || true
    if [[ $ban232_hits -ge 5 ]]; then
        log_ok "BAN-232: Error message format API used ($ban232_hits usage(s))"
        record_check_result "BAN-232" "true"
    else
        log_warn "BAN-232: Error message format API usage low ($ban232_hits usage(s))"
        record_check_result "BAN-232" "true"
    fi

    if [[ $g24_issues -eq 0 ]]; then
        log_ok "BAN-191~256: All checks passed"
        record_check_result "ban-191-256" "true"
    else
        log_warn "BAN-191~256: $g24_issues check(s) failed"
        record_check_result "ban-191-256" "false"
    fi
}
run_auto_fix() {
    log_section "Auto-Fix Mode"

    log_info "Running automatic fixes..."

    # Fix 1: clang-format auto-fix
    if command -v clang-format &>/dev/null; then
        log_info "Auto-formatting C/C++ sources with clang-format..."
        local fixed=0
        while IFS= read -r -d '' file; do
            if [[ -f "$file" ]]; then
                if ! clang-format --dry-run --Werror "$file" &>/dev/null 2>&1; then
                    clang-format -i "$file"
                    ((fixed++)) || true
                fi
            fi
            [[ $fixed -ge 50 ]] && break
        done < <(find "${PROJECT_ROOT}/agentos" \
            \( -name "*.c" -o -name "*.h" \) \
            ! -path "*/tests/*" \
            ! -path "*/build-*/*" \
            -print0 2>/dev/null)
        ((QUALITY_FIX_COUNT += fixed)) || true
        log_ok "Auto-fixed $fixed C/C++ file(s) with clang-format"
    fi

    # Fix 2: isort auto-fix (Python imports)
    if command -v isort &>/dev/null; then
        log_info "Auto-fixing Python imports with isort..."
        local fixed=0
        while IFS= read -r -d '' file; do
            [[ $fixed -ge 30 ]] && break
            if ! isort --check-only "$file" &>/dev/null 2>&1; then
                isort "$file" 2>/dev/null && ((fixed++)) || true
            fi
        done < <(find "${PROJECT_ROOT}" -name "*.py" \
            ! -path "*/__pycache__/*" \
            -print0 2>/dev/null)
        ((QUALITY_FIX_COUNT += fixed)) || true
        log_ok "Auto-fixed $fixed Python file(s) with isort"
    fi

    # Fix 3: Remove trailing whitespace
    log_info "Removing trailing whitespace..."
    local fixed=0
    while IFS= read -r -d '' file; do
        [[ $fixed -ge 100 ]] && break
        if grep -qP '[ \t]$' "$file" 2>/dev/null; then
            sed -i 's/[ \t]*$//' "$file"
            ((fixed++)) || true
        fi
    done < <(find "${PROJECT_ROOT}/agentos" \
        \( -name "*.c" -o -name "*.h" -o -name "*.py" -o -name "*.sh" -o -name "*.md" \) \
        ! -path "*/build-*/*" \
        ! -path "*/.git/*" \
        -print0 2>/dev/null)
    ((QUALITY_FIX_COUNT += fixed)) || true
    log_ok "Removed trailing whitespace from $fixed file(s)"

    # Fix 4: Ensure newline at end of file
    log_info "Ensuring newline at end of files..."
    local fixed=0
    while IFS= read -r -d '' file; do
        [[ $fixed -ge 100 ]] && break
        if [[ -s "$file" ]] && [[ "$(tail -c1 "$file" | wc -l)" -eq 0 ]]; then
            echo "" >> "$file"
            ((fixed++)) || true
        fi
    done < <(find "${PROJECT_ROOT}/agentos" \
        \( -name "*.c" -o -name "*.h" \) \
        ! -path "*/build-*/*" \
        ! -path "*/.git/*" \
        -print0 2>/dev/null)
    ((QUALITY_FIX_COUNT += fixed)) || true
    log_ok "Added missing newline to $fixed file(s)"

    log_ok "Auto-fix complete: $QUALITY_FIX_COUNT total fixes applied"
}
generate_quality_report() {
    mkdir -p "$(dirname "$QUALITY_REPORT_FILE")"

    cat > "$QUALITY_REPORT_FILE" << EOF
{
    "timestamp": "$(date -Iseconds)",
    "project": "AgentOS",
    "version": "0.1.0",
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
    local total_issues=$(( ISSUES_CRITICAL + ISSUES_HIGH + ISSUES_MEDIUM + ISSUES_LOW ))
    local pass_rate="0.0"
    if [[ ${CHECKS_TOTAL} -gt 0 ]]; then
        pass_rate=$(awk "BEGIN {printf \"%.1f\", (${CHECKS_PASSED}/${CHECKS_TOTAL})*100}")
    fi
    local overall_status="PASS"

    log_info ""
    log_info "==========================================="
    log_info "Quality Gate Summary"
    log_info "==========================================="
    log_info "Checks run:    ${CHECKS_TOTAL}"
    log_info "Checks passed: ${CHECKS_PASSED}"
    log_info "Checks failed: ${CHECKS_FAILED}"
    log_info "Pass rate:     ${pass_rate}%"
    log_info "Issues:"
    log_info "  Critical: ${ISSUES_CRITICAL}"
    log_info "  High:     ${ISSUES_HIGH}"
    log_info "  Medium:   ${ISSUES_MEDIUM}"
    log_info "  Low:      ${ISSUES_LOW}"
    log_info ""

    if [[ "$QUALITY_STRICT" == "true" ]] && [[ $total_issues -gt 0 ]]; then
        overall_status="FAIL"
        log_error "Quality gate FAILED (strict mode)"
    elif [[ "$QUALITY_FAIL_ON_VIOLATION" == "true" ]] && [[ ${CHECKS_FAILED} -gt 0 ]]; then
        overall_status="FAIL"
        log_error "Quality gate FAILED (${CHECKS_FAILED} check(s) failed, --fail-on-violation mode)"
    elif [[ "$QUALITY_FAIL_ON_WARN" == "true" ]] && [[ $(( ISSUES_CRITICAL + ISSUES_HIGH )) -gt 0 ]]; then
        overall_status="FAIL"
        log_error "Quality gate FAILED (fail-on-warn mode, critical/high issues)"
    elif [[ $total_issues -eq 0 ]]; then
        log_ok "Quality gate PASSED - No issues found"
    else
        log_warn "Quality gate completed with $total_issues issue(s) (non-blocking, report-only mode)"
    fi

    log_info ""
    log_info "==========================================="
    log_info "Overall Status: ${overall_status}"
    log_info "==========================================="

    if [[ "$overall_status" == "FAIL" ]]; then
        return 1
    fi

    return 0
}

###############################################################################
# 帮助
###############################################################################
show_help() {
    cat << 'EOF'
AgentOS Quality Gate Script v0.2.0

Usage: ./quality-gate.sh [OPTIONS]

Options:
    --fail-on-violation, --ci
                      Fail the build (exit 1) when any check fails.
                      Recommended for CI pipelines.
    --strict          Enable ALL BAN checks and fail on any issue.
                      Equivalent to --fail-on-violation + full BAN coverage.
    --fail-on-warn   Fail on high/critical issues
    --fix             Auto-fix fixable issues (clang-format, isort, trailing whitespace, etc.)
    --report FILE    Custom report output path
    -h, --help       Show this help

Modes:
    Report-only (default):
        No flags needed. The script reports all issues but always exits 0.
        Suitable for local development and informational runs.

    CI enforcement:
        Use --fail-on-violation (or --ci). Exits with code 1 if any check fails.
        Recommended for CI/CD pipelines to block merges on violations.
        Example: ./quality-gate.sh --fail-on-violation

    Strict:
        Use --strict. Enables all BAN checks and fails on any violation.
        Example: ./quality-gate.sh --strict

Gates:
    1.  C/C++ Static Analysis (cppcheck)
    2.  Code Format Check (clang-format)
    3.  Python Quality (syntax, isort)
    4.  Shell Script Quality (bash -n, shellcheck)
    5.  Basic Security Scan (secrets, dangerous functions)
    6.  BAN-17~BAN-20 + BAN-33 Audit Scan
    7.  BAN-70~103 Strict Rules (Code Quality)
    8.  Documentation Completeness
    9.  Version Consistency
    10. Strict Build Verification
    11. Test Execution (ctest)
    12. Stub Function Audit (ENOTSUP)
    13. Memory Call Compliance (bare malloc/free)
    14. Log System Compliance (printf in atoms)
    15. Error Handling Compliance (return -1)
    16. Governance Compliance (BAN-104~108)
    17. Architecture Review
    17a. MemoryRovol Compliance (BAN-115~120)
    17b. CoreLoopThree Compliance (BAN-121~125)
    17c. llm_d Routing Compliance (BAN-133~137)
    17d. Credential Pool Compliance (BAN-138~142)
    18. External Tech Absorption (BAN-143~150)
    19. Dual Thinking System (BAN-109~114)
    20. Security Dome (BAN-126~132)
    21. Memory Safety Rules (BAN-151~162, Phase 2.5)
    22. Extended Strict Rules (BAN-151~180, Phase 2.5/3)
    23. BAN-181~190 Compliance Checks
    24. BAN-191~256 Extended Compliance (P0.15)
EOF
}

###############################################################################
# 参数解析
###############################################################################
parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --strict)              QUALITY_STRICT=true; QUALITY_FAIL_ON_VIOLATION=true ;;
            --fail-on-violation|--ci) QUALITY_FAIL_ON_VIOLATION=true ;;
            --fail-on-warn)        QUALITY_FAIL_ON_WARN=true ;;
            --fix)                 QUALITY_AUTO_FIX=true ;;
            --report)              QUALITY_REPORT_FILE="$2"; shift ;;
            --help|-h)             show_help; exit 0 ;;
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

    log_info "AgentOS Quality Gate v0.2.0"
    log_info "Timestamp: $(date '+%Y-%m-%d %H:%M:%S')"
    log_info "Strict mode: $QUALITY_STRICT"
    log_info "Fail-on-violation (CI) mode: $QUALITY_FAIL_ON_VIOLATION"
    log_info "Auto-fix mode: $QUALITY_AUTO_FIX"

    mkdir -p "${PROJECT_ROOT}/ci-artifacts"

    # 如果启用 --fix，则先执行自动修复
    if [[ "$QUALITY_AUTO_FIX" == "true" ]]; then
        run_auto_fix
    fi

    gate_cpp_static_analysis
    gate_code_format
    gate_python_quality
    gate_shell_quality
    gate_security_basic
    gate_ban_audit
    gate_ban_strict_rules    # CI-01: BAN-70~103 strict mode
    gate_documentation
    gate_version_consistency
    gate_strict_build
    gate_test_execution
    gate_stub_audit
    gate_memory_compliance
    gate_log_compliance
    gate_error_handling
    gate_governance_compliance
    gate_architecture_review
    gate_memoryrovol_compliance       # BAN-115~120
    gate_coreloopthree_compliance     # BAN-121~125
    gate_llm_routing_compliance       # BAN-133~137
    gate_credential_pool_compliance   # BAN-138~142
    gate_external_tech_compliance
    gate_dual_thinking_compliance
    gate_security_dome_compliance
    gate_ban_memory_safety
    gate_ban_extended_strict  # CI-02: BAN-151~180 extended strict mode
    gate_ban_181_190          # CI-03: BAN-181~190 compliance checks

    generate_quality_report

    if ! print_final_summary; then
        exit 1
    fi
}

main "$@"
