#!/usr/bin/env bash
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS 测试运行脚本
# 支持：CTest/pytest 双引擎、覆盖率收集、测试分类执行、超时控制
# Version: 2.0.0

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
BLUE='\033[0;34m'; CYAN='\033[0;36m'; NC='\033[0m'

log_info()  { echo -e "${BLUE}[TEST]${NC}   $*"; }
log_ok()    { echo -e "${GREEN}[TEST-OK]${NC} $*"; }
log_warn()  { echo -e "${YELLOW}[TEST-WARN]${NC} $*"; }
log_error() { echo -e "${RED}[TEST-ERR]${NC} $*" >&2; }
log_pass() { echo -e "${GREEN}  PASS${NC}  $*"; }
log_fail() { echo -e "${RED}  FAIL${NC}  $*"; }

###############################################################################
# 配置
###############################################################################
TEST_MODULE="all"
TEST_VERBOSE=false
TEST_COVERAGE=false
TEST_CATEGORY=""
TEST_TIMEOUT=300
TEST_PARALLEL="auto"
TEST_OUTPUT_FORMAT="human"
TEST_PYTEST_ONLY=false
TEST_CTEST_ONLY=false

# 测试结果统计
TOTAL_PASSED=0
TOTAL_FAILED=0
TOTAL_SKIPPED=0
TOTAL_ERRORS=0
declare -a FAILED_TESTS=()

###############################################################################
# 参数解析
###############################################################################
parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --module|-m)     TEST_MODULE="$2"; shift ;;
            --verbose|-v)    TEST_VERBOSE=true ;;
            --coverage|-c)   TEST_COVERAGE=true ;;
            --category)      TEST_CATEGORY="$2"; shift ;;
            --timeout|-t)    TEST_TIMEOUT="$2"; shift ;;
            --parallel|-j)   TEST_PARALLEL="$2"; shift ;;
            --format|-f)     TEST_OUTPUT_FORMAT="$2"; shift ;;
            --pytest-only)   TEST_PYTEST_ONLY=true ;;
            --ctest-only)    TEST_CTEST_ONLY=true ;;
            --help|-h)       show_help; exit 0 ;;
            *) log_warn "Unknown option: $1" ;;
        esac
        shift
    done
}

show_help() {
    cat << 'EOF'
AgentOS Test Runner Script v2.0.0

Usage: ./run-tests.sh [OPTIONS]

Options:
    -m, --module NAME     Target module (default: all)
    -v, --verbose         Verbose test output
    -c, --coverage        Enable coverage collection
    --category CAT        Test category filter (unit/integration/security/fuzz)
    -t, --timeout SEC     Per-test timeout in seconds (default: 300)
    -j, --parallel N      Parallel test jobs (default: auto)
    -f, --format FMT      Output format: human|json|junit|xml
    --pytest-only         Run only Python tests (pytest)
    --ctest-only          Run only C/C++ tests (ctest)
    -h, --help            Show this help

Examples:
    ./run-tests.sh                        # Run all tests
    ./run-tests.sh -m daemon              # Test daemon module only
    ./run-tests.sh --category unit         # Unit tests only
    ./run-tests.sh -c -v                  # Coverage + verbose
    ./run-tests.sh --pytest-only           # Python tests only
EOF
}

###############################################################################
# 工具函数
###############################################################################
get_parallel_jobs() {
    if [[ "$TEST_PARALLEL" == "auto" ]]; then
        nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo "4"
    else
        echo "$TEST_PARALLEL"
    fi
}

get_ctest_modules() {
    local modules=()
    if [[ "$TEST_MODULE" == "all" ]]; then
        modules=("daemon" "atoms" "commons")
    else
        IFS=',' read -ra modules <<< "$TEST_MODULE"
    fi
    echo "${modules[@]}"
}

###############################################################################
# CTest (C/C++ 测试)
###############################################################################
run_ctest() {
    if [[ "$TEST_PYTEST_ONLY" == "true" ]]; then
        return 0
    fi

    log_info "==========================================="
    log_info "CTest: C/C++ Tests"
    log_info "==========================================="

    local modules
    modules=$(get_ctest_modules)
    local jobs=$(get_parallel_jobs)

    for module in $modules; do
        local build_dir="${PROJECT_ROOT}/build-${module}"

        if [[ ! -d "$build_dir" ]]; then
            log_warn "Build directory not found: $build_dir, skipping ctest for $module"
            continue
        fi

        if [[ ! -f "${build_dir}/CTestTestfile.cmake" ]]; then
            log_warn "No CTestTestfile.cmake in $build_dir, skipping"
            continue
        fi

        log_info "Running CTest for module: $module"
        cd "$build_dir"

        local ctest_args=(
            "--output-on-failure"
            "--timeout" "$TEST_TIMEOUT"
            "-j$jobs"
        )

        # 覆盖率支持
        if [[ "$TEST_COVERAGE" == "true" ]]; then
            ctest_args+=("-T" "Coverage")
        fi

        # 输出格式
        case "$TEST_OUTPUT_FORMAT" in
            json)  ctest_args+=("--no-compress-output" "-T" "Test" "-O" "${PROJECT_ROOT}/ci-logs/${module}-ctest.json") ;;
            junit) ctest_args+=("--no-compress-output" "-T" "Test" "-O" "JUnitTestResults.xml") ;;
            xml)   ctest_args+=("--no-compress-output" "-T" "Test" "-O" "TestResults.xml") ;;
            *)     ctest_args+=("-V") ;;
        esac

        # 执行测试
        local test_output
        local test_exit_code=0
        test_output=$(ctest "${ctest_args[@]}" 2>&1) || test_exit_code=$?

        # 解析结果 - 从ctest输出的总结行提取
        local passed=0
        local failed=0
        local total=0

        # 尝试从 "X% tests passed, Y tests failed out of Z" 格式解析
        local summary_line
        summary_line=$(echo "$test_output" | grep -E "[0-9]+% tests passed" | tail -1 || true)
        if [[ -n "$summary_line" ]]; then
            total=$(echo "$summary_line" | grep -oP 'out of \K[0-9]+' || echo "0")
            failed=$(echo "$summary_line" | grep -oP '[0-9]+ tests failed' | grep -oP '[0-9]+' || echo "0")
            passed=$((total - failed))
        else
            # 回退：从 "tests passed" 和 "tests failed" 行计数
            passed=$(echo "$test_output" | grep -oP '\d+(?= tests? passed)' | tail -1 || echo "0")
            failed=$(echo "$test_output" | grep -oP '\d+(?= tests? failed)' | tail -1 || echo "0")
        fi

        if [[ "$test_exit_code" -eq 0 ]]; then
            ((TOTAL_PASSED += passed))
            log_ok "Module $module: CTest passed ($passed tests)"
        else
            ((TOTAL_FAILED += failed))
            FAILED_TESTS+=("ctest:${module}")
            log_fail "Module $module: CTest failed ($failed test(s))"

            # 输出失败详情
            if [[ "$TEST_VERBOSE" == "true" ]]; then
                echo "$test_output" | tail -50
            fi
        fi

        cd "$PROJECT_ROOT"
    done
}

###############################################################################
# pytest (Python 测试)
###############################################################################
run_pytest() {
    if [[ "$TEST_CTEST_ONLY" == "true" ]]; then
        return 0
    fi

    # 检查 pytest 是否可用
    if ! command -v python3 &>/dev/null || ! python3 -m pytest --version &>/dev/null; then
        log_warn "pytest not available, skipping Python tests"
        return 0
    fi

    log_info "==========================================="
    log_info "pytest: Python Tests"
    log_info "==========================================="

    local tests_dir="${PROJECT_ROOT}/tests"
    if [[ ! -d "$tests_dir" ]]; then
        log_warn "Tests directory not found: $tests_dir"
        return 0
    fi

    cd "$tests_dir"

    local pytest_args=(
        "-m" "${TEST_CATEGORY:-not slow}"
        "--tb=short"
        "-q"
        "-W" "ignore::DeprecationWarning"
    )

    # 并行
    pytest_args+=("-n" "auto" 2>/dev/null || true)

    # 覆盖率
    if [[ "$TEST_COVERAGE" == "true" ]]; then
        pytest_args+=(
            "--cov=."
            "--cov-report=term-missing"
            "--cov-report=html:${PROJECT_ROOT}/ci-artifacts/coverage-html"
            "--cov-report=xml:${PROJECT_ROOT}/ci-artifacts/coverage.xml"
        )
    fi

    # 详细输出
    if [[ "$TEST_VERBOSE" == "true" ]]; then
        pytest_args=("-v" "--tb=long")
    fi

    # 输出格式
    case "$TEST_OUTPUT_FORMAT" in
        json) pytest_args+=("--json-report" "--json-report-file=${PROJECT_ROOT}/ci-artifacts/pytest-results.json") ;;
        junit) pytest_args+=("--junitxml=${PROJECT_ROOT}/ci-artifacts/pytest-results.xml") ;;
    esac

    # 执行测试
    local pytest_exit_code=0
    local pytest_output
    pytest_output=$(python3 -m pytest "${pytest_args[@]}" 2>&1) || pytest_exit_code=$?

    # 解析 pytest 结果
    local py_passed=$(echo "$pytest_output" | grep -oP '\d+ passed' | grep -oP '\d+' || echo "0")
    local py_failed=$(echo "$pytest_output" | grep -oP '\d+ failed' | grep -oP '\d+' || echo "0")
    local py_errors=$(echo "$pytest_output" | grep -oP '\d+ error' | grep -oP '\d+' || echo "0")
    local py_skipped=$(echo "$pytest_output" | grep -oP '\d+ skipped' | grep -oP '\d+' || echo "0")

    ((TOTAL_PASSED += py_passed))
    ((TOTAL_FAILED += py_failed))
    ((TOTAL_ERRORS += py_errors))
    ((TOTAL_SKIPPED += py_skipped))

    if [[ "$py_failed" -gt 0 ]] || [[ "$py_errors" -gt 0 ]]; then
        FAILED_TESTS+=("pytest")
        log_fail "pytest: $py_failed failed, $py_errors errors"
    else
        log_ok "pytest: $py_passed passed, $py_skipped skipped"
    fi

    cd "$PROJECT_ROOT"
}

###############################################################################
# Fuzz 测试（特殊处理）
###############################################################################
run_fuzz_tests() {
    if [[ "$TEST_CATEGORY" != "" ]] && [[ "$TEST_CATEGORY" != "fuzz" ]]; then
        return 0
    fi

    local fuzz_dir="${PROJECT_ROOT}/tests/fuzz"
    if [[ ! -d "$fuzz_dir" ]]; then
        return 0
    fi

    log_info "Running fuzz framework validation..."

    if command -v python3 &>/dev/null; then
        if python3 -c "import sys; sys.path.insert(0, '${fuzz_dir}'); import fuzz_framework" 2>/dev/null; then
            log_ok "Fuzz framework imports successfully"
        else
            log_warn "Fuzz framework import failed (non-blocking)"
        fi
    fi
}

###############################################################################
# 结果汇总
###############################################################################
print_summary() {
    log_info ""
    log_info "==========================================="
    log_info "Test Results Summary"
    log_info "==========================================="
    log_info "  Passed:  ${TOTAL_PASSED}"
    log_info "  Failed:  ${TOTAL_FAILED}"
    log_info "  Errors:  ${TOTAL_ERRORS}"
    log_info "  Skipped: ${TOTAL_SKIPPED}"
    log_info ""

    if [[ ${#FAILED_TESTS[@]} -gt 0 ]]; then
        log_error "Failed test suites: ${FAILED_TESTS[*]}"
        log_info ""
        return 1
    fi

    log_ok "All tests passed!"
    return 0
}

###############################################################################
# 主流程
###############################################################################
main() {
    parse_args "$@"

    log_info "AgentOS Test Runner v2.0.0"
    log_info "Timestamp: $(date '+%Y-%m-%d %H:%M:%S')"
    log_info "Module: ${TEST_MODULE}, Timeout: ${TEST_TIMEOUT}s"

    mkdir -p "${PROJECT_ROOT}/ci-artifacts" "${PROJECT_ROOT}/ci-logs"

    run_ctest
    run_pytest
    run_fuzz_tests

    print_summary
}

main "$@"
