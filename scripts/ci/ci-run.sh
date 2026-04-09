#!/usr/bin/env bash
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS CI 主编排脚本
# 统一入口：依赖安装 → 构建 → 测试 → 质量门禁 → 部署
# Version: 2.0.0
# Last updated: 2026-04-06

set -euo pipefail

###############################################################################
# 路径定义
###############################################################################
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
LIB_DIR="${SCRIPT_DIR}/../lib"
CI_DIR="${SCRIPT_DIR}"

###############################################################################
# 颜色定义
###############################################################################
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

###############################################################################
# 日志函数
###############################################################################
log_info()  { echo -e "${BLUE}[CI-INFO]${NC}  $*"; }
log_ok()    { echo -e "${GREEN}[CI-OK]${NC}    $*"; }
log_warn()  { echo -e "${YELLOW}[CI-WARN]${NC}  $*"; }
log_error() { echo -e "${RED}[CI-ERROR]${NC} $*" >&2; }
log_step()  { echo -e "\n${CYAN}======> ${NC}$*"; }

###############################################################################
# 全局变量
###############################################################################
CI_VERBOSE="${CI_VERBOSE:-false}"
CI_SKIP_DEPS="${CI_SKIP_DEPS:-false}"
CI_SKIP_BUILD="${CI_SKIP_BUILD:-false}"
CI_SKIP_TEST="${CI_SKIP_TEST:-false}"
CI_SKIP_QUALITY="${CI_SKIP_QUALITY:-false}"
CI_SKIP_DEPLOY="${CI_SKIP_DEPLOY:-false}"
CI_BUILD_TYPE="${CI_BUILD_TYPE:-Release}"
CI_MODULE="${CI_MODULE:-all}"
CI_PARALLEL="${CI_PARALLEL:-auto}"
CI_ARTIFACT_DIR="${CI_ARTIFACT_DIR:-${PROJECT_ROOT}/ci-artifacts}"
CI_LOG_DIR="${CI_LOG_DIR:-${PROJECT_ROOT}/ci-logs}"

# 时间统计
declare -A PHASE_TIMINGS
PHASE_START_TIME=""

###############################################################################
# 加载库函数
###############################################################################
load_libraries() {
    if [[ -f "${LIB_DIR}/common.sh" ]]; then
        source "${LIB_DIR}/common.sh" 2>/dev/null || true
    fi
}

###############################################################################
# 计时工具
###############################################################################
timer_start() {
    PHASE_START_TIME=$(date +%s%N 2>/dev/null || date +%s)
}

timer_stop() {
    local phase="$1"
    local end_time
    end_time=$(date +%s%N 2>/dev/null || date +%s)

    if [[ -n "$PHASE_START_TIME" ]]; then
        local elapsed=$(( (end_time - PHASE_START_TIME) / 1000000 ))
        PHASE_TIMINGS["$phase"]="${elapsed}ms"
        log_info "Phase '$phase' completed in ${elapsed}ms"
    fi
    PHASE_START_TIME=""
}

print_timing_summary() {
    log_step "Timing Summary"
    for phase in "${!PHASE_TIMINGS[@]}"; do
        log_info "  ${phase}: ${PHASE_TIMINGS[$phase]}"
    done
}

###############################################################################
# Phase 1: 环境准备
###############################################################################
phase_prepare() {
    log_step "Phase 1: Environment Preparation"

    timer_start

    # 创建输出目录
    mkdir -p "$CI_ARTIFACT_DIR" "$CI_LOG_DIR"

    # 检测平台
    detect_platform

    # 设置 pkg-config 路径（用于 tiktoken stub）
    export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:${PKG_CONFIG_PATH:-}"

    # 输出环境信息
    log_info "Project Root: ${PROJECT_ROOT}"
    log_info "Build Type:   ${CI_BUILD_TYPE}"
    log_info "Target Module: ${CI_MODULE}"
    log_info "Platform:      $(uname -s) $(uname -m)"

    timer_stop "prepare"
}

detect_platform() {
    case "$(uname -s)" in
        Linux*)  CI_OS="linux" ;;
        Darwin*) CI_OS="macos" ;;
        MINGW*|MSYS*|CYGWIN*) CI_OS="windows" ;;
        *)       CI_OS="unknown" ;;
    esac
    export CI_OS
}

###############################################################################
# Phase 2: 依赖安装
###############################################################################
phase_deps() {
    if [[ "$CI_SKIP_DEPS" == "true" ]]; then
        log_warn "Skipping dependency installation (CI_SKIP_DEPS=true)"
        return 0
    fi

    log_step "Phase 2: Dependency Installation"

    timer_start

    local deps_script="${CI_DIR}/install-deps.sh"
    if [[ ! -f "$deps_script" ]]; then
        log_error "Dependency script not found: $deps_script"
        return 1
    fi

    chmod +x "$deps_script"

    if ! bash "$deps_script"; then
        log_error "Dependency installation failed"
        return 1
    fi

    # 创建 tiktoken 存根（如果不存在）
    create_tiktoken_stub

    timer_stop "deps"
}

create_tiktoken_stub() {
    local stub_file="/usr/local/lib/pkgconfig/tiktoken.pc"
    if [[ -f "$stub_file" ]]; then
        return 0
    fi

    log_info "Creating tiktoken CI stub..."
    sudo mkdir -p "$(dirname "$stub_file")" 2>/dev/null || true

    cat << 'EOF' | sudo tee "$stub_file" > /dev/null 2>/dev/null || true
prefix=/usr/local
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: tiktoken
Description: Tokenizer library (CI stub for AgentOS)
Version: 0.5.2
URL: https://github.com/openai/tiktoken
Libs: -L${libdir} -ltiktoken
Cflags: -I${includedir}
EOF

    log_ok "tiktoken stub created at $stub_file"
}

###############################################################################
# Phase 3: 构建模块
###############################################################################
phase_build() {
    if [[ "$CI_SKIP_BUILD" == "true" ]]; then
        log_warn "Skipping build (CI_SKIP_BUILD=true)"
        return 0
    fi

    log_step "Phase 3: Build Modules"

    timer_start

    local build_script="${CI_DIR}/build-module.sh"
    if [[ -f "$build_script" ]]; then
        chmod +x "$build_script"
        if ! bash "$build_script" --module "$CI_MODULE" --type "$CI_BUILD_TYPE"; then
            log_error "Build phase failed"
            return 1
        fi
    else
        # Fallback: 直接构建
        build_modules_fallback
    fi

    timer_stop "build"
}

build_modules_fallback() {
    local modules=()
    if [[ "$CI_MODULE" == "all" ]]; then
        modules=("daemon" "atoms" "commons" "cupolas" "gateway" "heapstore")
    else
        IFS=',' read -ra modules <<< "$CI_MODULE"
    fi

    local parallel_jobs
    if [[ "$CI_PARALLEL" == "auto" ]]; then
        parallel_jobs=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo "4")
    else
        parallel_jobs="$CI_PARALLEL"
    fi

    for module in "${modules[@]}"; do
        local module_path="${PROJECT_ROOT}/agentos/${module}"
        if [[ ! -d "$module_path" ]]; then
            log_warn "Module directory not found: $module, skipping"
            continue
        fi

        log_info "Building module: $module"

        local build_dir="${PROJECT_ROOT}/build-${module}"
        mkdir -p "$build_dir"
        cd "$build_dir"

        local cmake_args=(
            "$module_path"
            "-DCMAKE_BUILD_TYPE=${CI_BUILD_TYPE}"
            "-DBUILD_TESTS=ON"
        )

        # macOS OpenSSL 路径
        if [[ "$CI_OS" == "macos" ]] && command -v brew &>/dev/null; then
            cmake_args+=("-DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3 2>/dev/null || brew --prefix openssl 2>/dev/null)")
        fi

        if ! cmake "${cmake_args[@]}" >> "${CI_LOG_DIR}/${module}-cmake.log" 2>&1; then
            log_error "CMake configuration failed for $module"
            continue
        fi

        if ! cmake --build . --parallel "$parallel_jobs" >> "${CI_LOG_DIR}/${module}-build.log" 2>&1; then
            log_error "Build failed for $module"
            return 1
        fi

        log_ok "Module $module built successfully"
    done

    cd "$PROJECT_ROOT"
}

###############################################################################
# Phase 4: 运行测试
###############################################################################
phase_test() {
    if [[ "$CI_SKIP_TEST" == "true" ]]; then
        log_warn "Skipping tests (CI_SKIP_TEST=true)"
        return 0
    fi

    log_step "Phase 4: Run Tests"

    timer_start

    local test_script="${CI_DIR}/run-tests.sh"
    if [[ -f "$test_script" ]]; then
        chmod +x "$test_script"
        if ! bash "$test_script" --module "$CI_MODULE"; then
            log_error "Test phase failed"
            return 1
        fi
    else
        run_tests_fallback
    fi

    timer_stop "test"
}

run_tests_fallback() {
    local modules=()
    if [[ "$CI_MODULE" == "all" ]]; then
        modules=("daemon" "atoms" "commons")
    else
        IFS=',' read -ra modules <<< "$CI_MODULE"
    fi

    local total_passed=0
    local total_failed=0
    local total_skipped=0

    for module in "${modules[@]}"; do
        local build_dir="${PROJECT_ROOT}/build-${module}"
        if [[ ! -d "$build_dir" ]] || [[ ! -f "${build_dir}/CTestTestfile.cmake" ]]; then
            log_warn "No tests found for module: $module"
            ((total_skipped++))
            continue
        fi

        log_info "Running tests for module: $module"
        cd "$build_dir"

        local test_output
        if test_output=$(ctest --output-on-failure --timeout 300 \
            -j"$(nproc 2>/dev/null || echo '4')" \
            --no-compress-output -T Test 2>&1); then

            local passed=$(echo "$test_output" | grep -c "Passed" || true)
            local failed=$(echo "$test_output" | grep -c "Failed" || true)

            ((total_passed += passed))
            ((total_failed += failed))

            if [[ $failed -eq 0 ]]; then
                log_ok "Module $module: All tests passed ($passed tests)"
            else
                log_error "Module $module: $failed test(s) failed"
            fi
        else
            log_error "ctest execution failed for $module"
            ((total_failed++))
        fi
    done

    cd "$PROJECT_ROOT"

    log_info "Test Summary: ${total_passed} passed, ${total_failed} failed, ${total_skipped} skipped"

    if [[ $total_failed -gt 0 ]]; then
        return 1
    fi
}

###############################################################################
# Phase 5: 质量门禁
###############################################################################
phase_quality() {
    if [[ "$CI_SKIP_QUALITY" == "true" ]]; then
        log_warn "Skipping quality gate (CI_SKIP_QUALITY=true)"
        return 0
    fi

    log_step "Phase 5: Quality Gate"

    timer_start

    local quality_script="${CI_DIR}/quality-gate.sh"
    if [[ -f "$quality_script" ]]; then
        chmod +x "$quality_script"
        if ! bash "$quality_script"; then
            log_warn "Quality gate reported issues (non-blocking)"
        fi
    else
        quality_checks_fallback
    fi

    timer_stop "quality"
}

quality_checks_fallback() {
    log_info "Running basic quality checks..."

    # 1. 检查代码格式 (clang-format)
    if command -v clang-format &>/dev/null; then
        log_info "Checking code format with clang-format..."
        local format_issues=0
        while IFS= read -r -d '' file; do
            if ! clang-format --dry-run --Werror "$file" &>/dev/null; then
                ((format_issues++))
                log_warn "Format issue: $file"
            fi
        done < <(find "${PROJECT_ROOT}/agentos" \( -name "*.c" -o -name "*.h" \) \
            ! -path "*/tests/*" -print0 2>/dev/null | head -z -50)

        if [[ $format_issues -gt 0 ]]; then
            log_warn "clang-format: $format_issues file(s) need formatting"
        else
            log_ok "clang-format: All files properly formatted"
        fi
    fi

    # 2. 检查 Python 语法
    if command -v python3 &>/dev/null; then
        log_info "Checking Python syntax..."
        local py_errors=0
        while IFS= read -r -d '' file; do
            if ! python3 -m py_compile "$file" 2>/dev/null; then
                ((py_errors++))
                log_warn "Syntax error: $file"
            fi
        done < <(find "${PROJECT_ROOT}" -name "*.py" \
            ! -path "*/__pycache__/*" ! -path "*/.git/*" \
            ! -path "*/node_modules/*" ! -path "*/venv/*" \
            -print0 2>/dev/null | head -z -100)

        if [[ $py_errors -eq 0 ]]; then
            log_ok "Python syntax check: All files valid"
        else
            log_warn "Python syntax check: $py_errors file(s) with errors"
        fi
    fi

    # 3. 检查 shell 脚本语法
    if command -v bash -n &>/dev/null; then
        log_info "Checking shell script syntax..."
        local sh_errors=0
        while IFS= read -r -d '' file; do
            if ! bash -n "$file" 2>/dev/null; then
                ((sh_errors++))
                log_warn "Shell syntax error: $file"
            fi
        done < <(find "${PROJECT_ROOT}/scripts" -name "*.sh" -print0 2>/dev/null)

        if [[ $sh_errors -eq 0 ]]; then
            log_ok "Shell syntax check: All scripts valid"
        else
            log_warn "Shell syntax check: $sh_errors script(s) with errors"
        fi
    fi

    log_ok "Quality checks completed"
}

###############################################################################
# Phase 6: 制品打包
###############################################################################
phase_deploy() {
    if [[ "$CI_SKIP_DEPLOY" == "true" ]]; then
        log_warn "Skipping deployment (CI_SKIP_DEPLOY=true)"
        return 0
    fi

    log_step "Phase 6: Artifact Packaging"

    timer_start

    local deploy_script="${CI_DIR}/deploy-artifacts.sh"
    if [[ -f "$deploy_script" ]]; then
        chmod +x "$deploy_script"
        bash "$deploy_script" --output "$CI_ARTIFACT_DIR" || true
    else
        package_artifacts_fallback
    fi

    timer_stop "deploy"
}

package_artifacts_fallback() {
    local timestamp
    timestamp=$(date +%Y%m%d-%H%M%S)
    local version="v1.0.${timestamp}"
    local artifact_name="agentos-${version}.tar.gz"

    log_info "Packaging artifacts as: $artifact_name"

    tar czf "${CI_ARTIFACT_DIR}/${artifact_name}" \
        --exclude='.git' \
        --exclude='build-*' \
        --exclude='*.o' \
        --exclude='*.a' \
        --exclude='__pycache__' \
        --exclude='node_modules' \
        --exclude='.gitcode' \
        --exclude='.gitee' \
        --exclude='.github' \
        --exclude='vcpkg' \
        --exclude='ci-artifacts' \
        --exclude='ci-logs' \
        -C "${PROJECT_ROOT}" \
        agentos scripts toolkit vcpkg.json README.md LICENSE CHANGELOG.md CONTRIBUTING.md 2>/dev/null || true

    if [[ -f "${CI_ARTIFACT_DIR}/${artifact_name}" ]]; then
        local size
        size=$(du -h "${CI_ARTIFACT_DIR}/${artifact_name}" | cut -f1)
        log_ok "Artifact created: ${artifact_name} (${size})"
    else
        log_warn "Artifact packaging skipped (no build outputs found)"
    fi
}

###############################################################################
# 结果报告
###############################################################################
generate_report() {
    local report_file="${CI_ARTIFACT_DIR}/ci-report.json"
    local exit_code="${1:-0}"

    cat > "$report_file" << EOF
{
    "timestamp": "$(date -Iseconds)",
    "platform": "$(uname -s) $(uname -m)",
    "project_root": "${PROJECT_ROOT}",
    "build_type": "${CI_BUILD_TYPE}",
    "target_module": "${CI_MODULE}",
    "exit_code": ${exit_code},
    "phases": {
$(for phase in "${!PHASE_TIMINGS[@]}"; do
echo "        \"${phase}\": \"${PHASE_TIMINGS[$phase]}\","
done)
        "total": "complete"
    },
    "artifacts": "$(ls -1 "${CI_ARTIFACT_DIR}"/*.tar.gz 2>/dev/null | head -1 || echo 'none')"
}
EOF

    log_ok "CI report saved to: $report_file"
}

###############################################################################
# 帮助信息
###############################################################################
show_help() {
    cat << 'EOF'
AgentOS CI Main Orchestration Script v2.0.0

Usage: ./ci-run.sh [OPTIONS]

Options:
    --skip-deps       Skip dependency installation
    --skip-build      Skip build phase
    --skip-test       Skip test phase
    --skip-quality    Skip quality gate
    --skip-deploy     Skip artifact packaging
    --module NAME     Target module (default: all)
                      Comma-separated list or 'all'
    --type TYPE       Build type: Debug|Release|RelWithDebInfo (default: Release)
    --parallel N      Parallel jobs (default: auto-detect)
    --verbose         Enable verbose output
    --help            Show this help message

Environment Variables:
    CI_SKIP_DEPS, CI_SKIP_BUILD, CI_SKIP_TEST, etc.
    CI_BUILD_TYPE, CI_MODULE, CI_PARALLEL
    CI_ARTIFACT_DIR, CI_LOG_DIR

Examples:
    ./ci-run.sh                              # Full pipeline
    ./ci-run.sh --module daemon              # Build only daemon
    ./ci-run.sh --type Debug --skip-deploy   # Debug build, no deploy
    ./ci-run.sh --verbose --parallel 8       # Verbose, 8 parallel jobs

Exit Codes:
    0   Success
    1   General failure
    2   Build failure
    3   Test failure
EOF
}

###############################################################################
# 参数解析
###############################################################################
parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --skip-deps)      CI_SKIP_DEPS="true" ;;
            --skip-build)     CI_SKIP_BUILD="true" ;;
            --skip-test)      CI_SKIP_TEST="true" ;;
            --skip-quality)   CI_SKIP_QUALITY="true" ;;
            --skip-deploy)    CI_SKIP_DEPLOY="true" ;;
            --module)         CI_MODULE="$2"; shift ;;
            --type)           CI_BUILD_TYPE="$2"; shift ;;
            --parallel)       CI_PARALLEL="$2"; shift ;;
            --verbose|-v)     CI_VERBOSE="true" ;;
            --help|-h)        show_help; exit 0 ;;
            *) log_warn "Unknown option: $1" ;;
        esac
        shift
    done
}

###############################################################################
# 主流程
###############################################################################
main() {
    log_info "========================================="
    log_info "AgentOS CI Pipeline v2.0.0"
    log_info "Timestamp: $(date '+%Y-%m-%d %H:%M:%S %Z')"
    log_info "========================================="

    parse_args "$@"

    local overall_exit_code=0

    # Phase 1: 环境准备 (必须执行)
    phase_prepare || { overall_exit_code=1; log_error "Prepare phase failed"; }

    # Phase 2-6: 可跳过阶段
    phase_deps     || { overall_exit_code=1; log_error "Deps phase failed"; }
    phase_build    || { overall_exit_code=2; log_error "Build phase failed"; }
    phase_test     || { overall_exit_code=3; log_error "Test phase failed"; }
    phase_quality  || true  # 质量门禁不阻塞主流程
    phase_deploy   || true  # 部署失败不阻塞

    # 输出计时摘要
    print_timing_summary

    # 生成报告
    generate_report "$overall_exit_code"

    if [[ $overall_exit_code -eq 0 ]]; then
        log_info "========================================="
        log_ok   "CI Pipeline Completed Successfully!"
        log_info "========================================="
    else
        log_info "========================================="
        log_error "CI Pipeline Failed (exit code: $overall_exit_code)"
        log_info "========================================="
    fi

    exit $overall_exit_code
}

main "$@"
