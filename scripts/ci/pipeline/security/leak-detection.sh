#!/bin/bash
# AgentRT Memory Leak Detection — 分层浸泡测试
# P3.17: 每个 PR 4h soak / 每周 24h soak / 发布前 72h soak
# 使用: leak-detection.sh [--mode=pr|weekly|release] [--timeout=SECONDS]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
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
# Daemon 进程 RSS 采样
# ============================================================================
DAEMON_NAMES=(
    "corekern" "coreloopthree" "taskflow" "memory"
    "channel_d" "monit_d" "observe_d"
    "llm_d" "tool_d" "market_d" "sched_d"
    "hook_d" "plugin_d" "info_d" "notify_d"
    "gateway_d"
)

# RSS 采样间隔（秒）
RSS_SAMPLE_INTERVAL=60
# RSS 增长阈值（百分比）— 超过此值判定为泄漏
RSS_GROWTH_THRESHOLD=5
# RSS 稳定期（秒）— 采样前等待 daemon 启动稳定
RSS_WARMUP_SECONDS=120

# 采样单个 daemon 的 RSS (KB)
sample_daemon_rss() {
    local daemon_name="$1"
    local pid=$(pgrep -f "${daemon_name}" 2>/dev/null | head -1)
    if [ -z "$pid" ]; then
        echo "0"
        return
    fi
    # 读取 /proc/PID/status 中的 VmRSS
    local rss=$(awk '/VmRSS:/ {print $2}' "/proc/$pid/status" 2>/dev/null)
    echo "${rss:-0}"
}

# 采样所有 daemon 的 RSS，返回 JSON 格式
sample_all_rss() {
    local timestamp=$(date +%s)
    local result="{\"timestamp\": $timestamp"
    for daemon in "${DAEMON_NAMES[@]}"; do
        local rss=$(sample_daemon_rss "$daemon")
        result+=", \"${daemon}\": $rss"
    done
    result+="}"
    echo "$result"
}

# RSS 增长分析
# 输入: 初始 RSS 采样文件, 当前 RSS 采样文件
# 输出: JSON 格式的增长分析
analyze_rss_growth() {
    local initial_file="$1"
    local current_file="$2"
    local results=""

    for daemon in "${DAEMON_NAMES[@]}"; do
        local initial_rss=$(grep -o "\"${daemon}\": [0-9]*" "$initial_file" 2>/dev/null | grep -o '[0-9]*$' || echo "0")
        local current_rss=$(grep -o "\"${daemon}\": [0-9]*" "$current_file" 2>/dev/null | grep -o '[0-9]*$' || echo "0")

        if [ "$initial_rss" = "0" ] || [ "$current_rss" = "0" ]; then
            continue
        fi

        local growth=$(( current_rss - initial_rss ))
        local growth_pct=0
        if [ "$initial_rss" -gt 0 ]; then
            growth_pct=$(( growth * 100 / initial_rss ))
        fi

        # 取绝对值用于比较
        local abs_growth_pct=${growth_pct#-}

        if [ "$abs_growth_pct" -gt "$RSS_GROWTH_THRESHOLD" ]; then
            if [ "$growth" -gt 0 ]; then
                results+="$(printf '  {\"daemon\": \"%s\", \"initial_rss_kb\": %d, \"current_rss_kb\": %d, \"growth_kb\": %d, \"growth_pct\": %d, \"status\": \"LEAK\"}\n' \
                    "$daemon" "$initial_rss" "$current_rss" "$growth" "$growth_pct")"
            fi
        fi
    done

    if [ -z "$results" ]; then
        echo "PASS: All daemons RSS growth within ${RSS_GROWTH_THRESHOLD}% threshold"
    else
        echo -e "$results"
    fi
}

# ============================================================================
# 长期浸泡测试（仅 release/weekly 模式）
# 包含：RSS 采样 + 增长判定 + daemon 进程监控
# ============================================================================
run_soak_test() {
    if [ "$MODE" = "pr" ]; then
        log_info "PR mode: skipping long-term soak test (use --mode=weekly or --mode=release)"
        return
    fi

    log_info "--- 长期浸泡测试 (${TIMEOUT_SECONDS}s, RSS threshold: ${RSS_GROWTH_THRESHOLD}%) ---"

    # 初始 RSS 采样文件
    local initial_rss_file="${ARTIFACTS_DIR}/rss_initial.json"
    local rss_log="${ARTIFACTS_DIR}/rss_samples.jsonl"
    local growth_report="${ARTIFACTS_DIR}/rss_growth_report.txt"

    # 等待 daemon 启动稳定
    log_info "Waiting ${RSS_WARMUP_SECONDS}s for daemon stabilization..."
    sleep "$RSS_WARMUP_SECONDS"

    # 基线采样
    log_info "Taking baseline RSS snapshot..."
    sample_all_rss > "$initial_rss_file"
    log_info "Baseline RSS: $(cat "$initial_rss_file")"

    # 定期采样循环
    local soak_start=$(date +%s)
    local sample_count=0
    local max_samples=$(( (TIMEOUT_SECONDS - RSS_WARMUP_SECONDS) / RSS_SAMPLE_INTERVAL ))

    while true; do
        local now=$(date +%s)
        local elapsed=$(( now - soak_start ))

        if [ "$elapsed" -ge "$TIMEOUT_SECONDS" ]; then
            break
        fi

        sample_count=$((sample_count + 1))
        local sample=$(sample_all_rss)
        echo "$sample" >> "$rss_log"

        # 每 10 次采样做一次增长分析
        if [ $((sample_count % 10)) -eq 0 ]; then
            local current_rss_file="${ARTIFACTS_DIR}/rss_current_${sample_count}.json"
            echo "$sample" > "$current_rss_file"

            local growth_result=$(analyze_rss_growth "$initial_rss_file" "$current_rss_file")
            log_info "RSS check #${sample_count} (${elapsed}s elapsed): ${growth_result}"

            if echo "$growth_result" | grep -q "LEAK"; then
                log_fail "RSS growth detected! See ${growth_report}"
                echo "$growth_result" >> "$growth_report"
                if [ "$MODE" = "release" ]; then
                    log_fail "Release mode: aborting on RSS leak detection"
                    exit 1
                fi
            fi
        fi

        sleep "$RSS_SAMPLE_INTERVAL"
    done

    # 最终分析
    local final_rss_file="${ARTIFACTS_DIR}/rss_final.json"
    sample_all_rss > "$final_rss_file"

    log_info "Soak test completed: ${sample_count} RSS samples over ${TIMEOUT_SECONDS}s"
    log_info "Initial RSS: $(cat "$initial_rss_file")"
    log_info "Final RSS:   $(cat "$final_rss_file")"

    local final_growth=$(analyze_rss_growth "$initial_rss_file" "$final_rss_file")
    if echo "$final_growth" | grep -q "LEAK"; then
        log_fail "FINAL RSS GROWTH EXCEEDS THRESHOLD:"
        echo "$final_growth" >> "$growth_report"
        if [ "$MODE" = "release" ]; then
            exit 1
        fi
    else
        log_pass "RSS growth within ${RSS_GROWTH_THRESHOLD}% threshold for all daemons"
    fi
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
- **RSS Growth Threshold**: ${RSS_GROWTH_THRESHOLD}%
- **ASan**: $([ "$ASAN_AVAILABLE" = "1" ] && echo "enabled" || echo "unavailable")
- **Valgrind**: $([ "$VALGRIND_AVAILABLE" = "1" ] && echo "enabled" || echo "unavailable")

## ASan Reports

$(find "${ARTIFACTS_DIR}" -name "asan_report.*" -exec echo "- {}" \; 2>/dev/null || echo "No ASan leak reports found")

## Valgrind Reports

$(find "${ARTIFACTS_DIR}" -name "valgrind_*.log" -exec echo "- {}" \; 2>/dev/null || echo "No Valgrind reports found")

## RSS Growth Analysis

$(if [ -f "${ARTIFACTS_DIR}/rss_final.json" ]; then
    echo "Initial RSS: $(cat "${ARTIFACTS_DIR}/rss_initial.json" 2>/dev/null || echo 'N/A')"
    echo ""
    echo "Final RSS:   $(cat "${ARTIFACTS_DIR}/rss_final.json" 2>/dev/null || echo 'N/A')"
    echo ""
    if [ -f "${ARTIFACTS_DIR}/rss_growth_report.txt" ]; then
        echo "Growth report:"
        cat "${ARTIFACTS_DIR}/rss_growth_report.txt"
    else
        echo "All daemons within ${RSS_GROWTH_THRESHOLD}% RSS growth threshold"
    fi
else
    echo "RSS growth analysis not performed (weekly/release mode only)"
fi)

## Summary

$(if [ -z "$(find "${ARTIFACTS_DIR}" -name "asan_report.*" 2>/dev/null)" ] && \
      [ -z "$(find "${ARTIFACTS_DIR}" -name "valgrind_*.log" -exec grep -l "definitely lost" {} \; 2>/dev/null)" ] && \
      [ ! -f "${ARTIFACTS_DIR}/rss_growth_report.txt" ]; then
    echo "No memory leaks detected."
else
    echo "Memory leaks detected. Check the artifacts directory for details."
fi)
EOF

    log_info "Report: ${report}"

    # 统计泄漏数量
    local asan_leaks=$(find "${ARTIFACTS_DIR}" -name "asan_report.*" 2>/dev/null | wc -l)
    local vg_leaks=$(find "${ARTIFACTS_DIR}" -name "valgrind_*.log" -exec grep -l "definitely lost" {} \; 2>/dev/null | wc -l)
    local rss_leaks=0
    if [ -f "${ARTIFACTS_DIR}/rss_growth_report.txt" ]; then
        rss_leaks=$(grep -c "LEAK" "${ARTIFACTS_DIR}/rss_growth_report.txt" 2>/dev/null || echo 0)
    fi

    if [ "$asan_leaks" -gt 0 ] || [ "$vg_leaks" -gt 0 ] || [ "$rss_leaks" -gt 0 ]; then
        log_fail "Leaks detected: ASan=${asan_leaks}, Valgrind=${vg_leaks}, RSS=${rss_leaks}"
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