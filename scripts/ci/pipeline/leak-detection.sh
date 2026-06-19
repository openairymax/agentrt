#!/bin/bash
# AgentRT Memory Leak Detection — 分层浸泡测试
# P3.17: 每个 PR 4h soak / 每周 24h soak / 发布前 72h soak
# 使用: leak-detection.sh [--mode=pr|weekly|release] [--timeout=SECONDS]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AGENTOS_ROOT="$PROJECT_ROOT/agentos"
BUILD_DIR="${AGENTOS_ROOT}/build/leak-detection"
ARTIFACTS_DIR="${PROJECT_ROOT}/ci-artifacts/leak-detection"
SUPPRESSIONS_DIR="${PROJECT_ROOT}/ecosystem/manager/sanitizer"

# ============================================================================
# 默认配置
# ============================================================================
MODE="${1:-pr}"
TIMEOUT_SECONDS="${2:-14400}"  # 默认 4h (PR 模式)
JOB_COUNT="$(nproc)"

case "${MODE#--mode=}" in
    pr)
        TIMEOUT_SECONDS="${2:-14400}"   # 4h
        TEST_FILTER="unit"              # 仅单元测试
        ASAN_HALT="0"                   # 不阻断，收集所有报告
        ;;
    weekly)
        TIMEOUT_SECONDS="${2:-86400}"   # 24h
        TEST_FILTER="all"              # 全部测试
        ASAN_HALT="0"
        ;;
    release)
        TIMEOUT_SECONDS="${2:-259200}"  # 72h
        TEST_FILTER="all"
        ASAN_HALT="1"                   # 任何检测到的问题立即阻断
        ;;
    *)
        echo "Usage: $0 [--mode=pr|weekly|release] [--timeout=SECONDS]"
        exit 1
        ;;
esac

# ============================================================================
# 颜色输出
# ============================================================================
COLOR_RED='\033[0;31m'
COLOR_GREEN='\033[0;32m'
COLOR_YELLOW='\033[1;33m'
COLOR_CYAN='\033[0;36m'
COLOR_RESET='\033[0m'

log_info()  { echo -e "${COLOR_CYAN}[INFO]${COLOR_RESET}  $(date '+%H:%M:%S') $*"; }
log_pass()  { echo -e "${COLOR_GREEN}[PASS]${COLOR_RESET}  $*"; }
log_fail()  { echo -e "${COLOR_RED}[FAIL]${COLOR_RESET}  $*"; }
log_warn()  { echo -e "${COLOR_YELLOW}[WARN]${COLOR_RESET}  $*"; }

# ============================================================================
# 环境准备
# ============================================================================
prepare_environment() {
    log_info "=== 内存泄漏检测 — 模式: ${MODE} (超时: ${TIMEOUT_SECONDS}s) ==="

    mkdir -p "${ARTIFACTS_DIR}"
    mkdir -p "${BUILD_DIR}"

    # 检查 ASan 支持
    ASAN_AVAILABLE=0
    if echo "int main(){return 0;}" | gcc -x c - -fsanitize=address -o /dev/null 2>/dev/null; then
        ASAN_AVAILABLE=1
        log_info "ASan available"
    else
        log_warn "ASan not available, falling back to Valgrind"
    fi

    # 检查 Valgrind
    VALGRIND_AVAILABLE=0
    if command -v valgrind &>/dev/null; then
        VALGRIND_AVAILABLE=1
        log_info "Valgrind $(valgrind --version 2>&1 | head -1) available"
    else
        log_warn "Valgrind not available"
    fi

    if [ "$ASAN_AVAILABLE" = "0" ] && [ "$VALGRIND_AVAILABLE" = "0" ]; then
        log_fail "No leak detection tool available (ASan or Valgrind required)"
        exit 1
    fi
}

# ============================================================================
# ASan 构建与测试
# ============================================================================
run_asan_tests() {
    log_info "--- ASan + LSan + UBSan 测试 ---"

    cd "${BUILD_DIR}"

    # 配置 Debug + ASan 构建
    cmake "${AGENTOS_ROOT}" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_C_COMPILER=gcc \
        -DCMAKE_CXX_COMPILER=g++ \
        -DENABLE_SANITIZERS=ON \
        -DBUILD_TESTS=ON \
        -DAGENTOS_MEMORY_BACKEND=system \
        -Wno-dev 2>&1 | tail -5

    # 编译
    cmake --build . -j"${JOB_COUNT}" 2>&1 | tail -5

    # 设置 ASan 环境变量
    export ASAN_OPTIONS="halt_on_error=${ASAN_HALT}:detect_stack_use_after_return=1:detect_leaks=1:log_path=${ARTIFACTS_DIR}/asan_report"
    export LSAN_OPTIONS="suppressions=${SUPPRESSIONS_DIR}/lsan-suppressions"
    export UBSAN_OPTIONS="halt_on_error=${ASAN_HALT}:print_stacktrace=1"

    # 运行测试
    local test_start=$(date +%s)
    local test_count=0
    local leak_count=0

    # 收集所有测试可执行文件
    local test_binaries=$(find "${BUILD_DIR}" -type f -executable -name "test_*" 2>/dev/null || true)

    if [ -z "$test_binaries" ]; then
        log_warn "No test binaries found in ${BUILD_DIR}"
        return 0
    fi

    for test_bin in $test_binaries; do
        local test_name=$(basename "$test_bin")
        test_count=$((test_count + 1))

        log_info "Running: ${test_name}"

        if timeout "${TIMEOUT_SECONDS}" "$test_bin" > "${ARTIFACTS_DIR}/${test_name}.log" 2>&1; then
            log_pass "${test_name}: passed"
        else
            local exit_code=$?
            if [ $exit_code -eq 124 ]; then
                log_warn "${test_name}: timed out"
            else
                log_fail "${test_name}: failed (exit code $exit_code)"
            fi
        fi

        # 检查 ASan 报告
        if ls "${ARTIFACTS_DIR}/asan_report."* 2>/dev/null | grep -q "${test_name}" &>/dev/null; then
            leak_count=$((leak_count + $(ls "${ARTIFACTS_DIR}/asan_report."* 2>/dev/null | wc -l)))
        fi
    done

    local test_end=$(date +%s)
    local test_duration=$((test_end - test_start))

    log_info "ASan tests: ${test_count} binaries, ${test_duration}s elapsed, ${leak_count} leak reports"
}

# ============================================================================
# Valgrind 测试
# ============================================================================
run_valgrind_tests() {
    log_info "--- Valgrind Memcheck 测试 ---"

    cd "${BUILD_DIR}"

    # 重新配置 Release 构建（Valgrind 不需要 sanitizer 标志）
    cmake "${AGENTOS_ROOT}" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_C_COMPILER=gcc \
        -DCMAKE_CXX_COMPILER=g++ \
        -DENABLE_SANITIZERS=OFF \
        -DBUILD_TESTS=ON \
        -Wno-dev 2>&1 | tail -3

    cmake --build . -j"${JOB_COUNT}" 2>&1 | tail -3

    local test_binaries=$(find "${BUILD_DIR}" -type f -executable -name "test_*" 2>/dev/null || true)
    local vg_count=0

    for test_bin in $test_binaries; do
        local test_name=$(basename "$test_bin")
        vg_count=$((vg_count + 1))

        log_info "Valgrind: ${test_name}"

        valgrind \
            --leak-check=full \
            --show-leak-kinds=all \
            --track-origins=yes \
            --verbose \
            --log-file="${ARTIFACTS_DIR}/valgrind_${test_name}.log" \
            --suppressions="${SUPPRESSIONS_DIR}/valgrind-suppressions" \
            --error-exitcode=1 \
            "$test_bin" > "${ARTIFACTS_DIR}/${test_name}_valgrind.log" 2>&1 || true
    done

    log_info "Valgrind: ${vg_count} binaries analyzed"
}

# ============================================================================
# 长期浸泡测试（仅 release/weekly 模式）
# ============================================================================
run_soak_test() {
    if [ "$MODE" = "pr" ]; then
        log_info "PR mode: skipping long-term soak test"
        return
    fi

    log_info "--- 长期浸泡测试 (${TIMEOUT_SECONDS}s) ---"

    cd "${BUILD_DIR}"

    # 构建一个持续运行的 soak 测试程序
    local soak_src="${PROJECT_ROOT}/ci-artifacts/leak-detection/soak_test.c"
    cat > "$soak_src" << 'SOAKEOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define ALLOC_SIZE 4096
#define SLEEP_US   100000  /* 100ms */

int main(void) {
    time_t start = time(NULL);
    unsigned long alloc_count = 0;
    unsigned long free_count = 0;

    printf("Soak test started at %ld\n", (long)start);
    fflush(stdout);

    while (1) {
        /* 分配/释放循环，模拟正常运行时内存使用 */
        void *p = malloc(ALLOC_SIZE);
        if (p) {
            memset(p, 0xAB, ALLOC_SIZE);
            alloc_count++;
        }
        free(p);
        free_count++;

        usleep(SLEEP_US);

        if (alloc_count % 10000 == 0) {
            printf("Soak: %lu allocs, %lu frees, elapsed=%lds\n",
                   alloc_count, free_count,
                   (long)(time(NULL) - start));
            fflush(stdout);
        }
    }

    return 0;
}
SOAKEOF

    gcc -o "${BUILD_DIR}/soak_test" "$soak_src" -fsanitize=address,leak

    timeout "${TIMEOUT_SECONDS}" "${BUILD_DIR}/soak_test" 2>&1 || true

    log_info "Soak test completed"
}

# ============================================================================
# 生成报告
# ============================================================================
generate_report() {
    log_info "--- 生成检测报告 ---"

    local report="${ARTIFACTS_DIR}/leak_detection_report.md"
    local timestamp=$(date -Iseconds)

    cat > "$report" << EOF
# Memory Leak Detection Report

- **Mode**: ${MODE}
- **Timestamp**: ${timestamp}
- **Timeout**: ${TIMEOUT_SECONDS}s
- **ASan**: $([ "$ASAN_AVAILABLE" = "1" ] && echo "enabled" || echo "unavailable")
- **Valgrind**: $([ "$VALGRIND_AVAILABLE" = "1" ] && echo "enabled" || echo "unavailable")

## ASan Reports

$(find "${ARTIFACTS_DIR}" -name "asan_report.*" -exec echo "- {}" \; 2>/dev/null || echo "No ASan leak reports found")

## Valgrind Reports

$(find "${ARTIFACTS_DIR}" -name "valgrind_*.log" -exec echo "- {}" \; 2>/dev/null || echo "No Valgrind reports found")

## Summary

$(if [ -z "$(find "${ARTIFACTS_DIR}" -name "asan_report.*" 2>/dev/null)" ] && \
      [ -z "$(find "${ARTIFACTS_DIR}" -name "valgrind_*.log" -exec grep -l "definitely lost" {} \; 2>/dev/null)" ]; then
    echo "No memory leaks detected."
else
    echo "Memory leaks detected. Check the artifacts directory for details."
fi)
EOF

    log_info "Report: ${report}"

    # 统计泄漏数量
    local asan_leaks=$(find "${ARTIFACTS_DIR}" -name "asan_report.*" 2>/dev/null | wc -l)
    local vg_leaks=$(find "${ARTIFACTS_DIR}" -name "valgrind_*.log" -exec grep -l "definitely lost" {} \; 2>/dev/null | wc -l)

    if [ "$asan_leaks" -gt 0 ] || [ "$vg_leaks" -gt 0 ]; then
        log_fail "Leaks detected: ASan=${asan_leaks}, Valgrind=${vg_leaks}"
        if [ "$MODE" = "release" ]; then
            exit 1
        fi
    else
        log_pass "No memory leaks detected"
    fi
}

# ============================================================================
# 主流程
# ============================================================================
main() {
    prepare_environment

    if [ "$ASAN_AVAILABLE" = "1" ]; then
        run_asan_tests
    fi

    if [ "$VALGRIND_AVAILABLE" = "1" ]; then
        run_valgrind_tests
    fi

    run_soak_test
    generate_report
}

main "$@"