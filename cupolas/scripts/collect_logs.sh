#!/bin/bash
# =============================================================================
# AgentOS cupolas Module Build Log Collector
# Version: 1.0.0
# Description: Collects and aggregates build logs for analysis
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODULE_DIR="$(dirname "$SCRIPT_DIR")"
LOG_DIR="${MODULE_DIR}/logs"
BUILD_DIR="${MODULE_DIR}/build"

# =============================================================================
# Log Collection Functions
# =============================================================================

init_log_dir() {
    local timestamp=$(date +%Y%m%d_%H%M%S)
    local run_log_dir="${LOG_DIR}/build_${timestamp}"
    
    mkdir -p "$run_log_dir"/{build,test,coverage,security,quality}
    
    echo "$run_log_dir"
}

collect_cmake_logs() {
    local log_dir="$1"
    
    if [[ -d "${BUILD_DIR}/CMakeFiles" ]]; then
        # CMake output
        if [[ -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
            cp "${BUILD_DIR}/CMakeCache.txt" "${log_dir}/build/"
        fi
        
        # CMake error logs
        find "${BUILD_DIR}/CMakeFiles" -name "*.log" -exec cp {} "${log_dir}/build/" \; 2>/dev/null || true
    fi
    
    echo "CMake logs collected"
}

collect_build_logs() {
    local log_dir="$1"
    
    # Build output (if redirected)
    if [[ -f "${BUILD_DIR}/build.log" ]]; then
        cp "${BUILD_DIR}/build.log" "${log_dir}/build/"
    fi
    
    # Compiler warnings/errors
    if [[ -d "${BUILD_DIR}" ]]; then
        grep -r "warning:" "${BUILD_DIR}" --include="*.log" > "${log_dir}/build/warnings.txt" 2>/dev/null || true
        grep -r "error:" "${BUILD_DIR}" --include="*.log" > "${log_dir}/build/errors.txt" 2>/dev/null || true
    fi
    
    echo "Build logs collected"
}

collect_test_logs() {
    local log_dir="$1"
    
    # CTest logs
    if [[ -d "${BUILD_DIR}/Testing" ]]; then
        cp -r "${BUILD_DIR}/Testing"/* "${log_dir}/test/" 2>/dev/null || true
    fi
    
    # Test XML results
    find "${BUILD_DIR}" -name "*.xml" -path "*/tests/*" -exec cp {} "${log_dir}/test/" \; 2>/dev/null || true
    
    # Test output
    if [[ -f "${BUILD_DIR}/test_output.log" ]]; then
        cp "${BUILD_DIR}/test_output.log" "${log_dir}/test/"
    fi
    
    echo "Test logs collected"
}

collect_coverage_logs() {
    local log_dir="$1"
    
    # Coverage data files
    find "${BUILD_DIR}" -name "*.gcda" -o -name "*.gcno" | head -100 > "${log_dir}/coverage/gcov_files.txt" 2>/dev/null || true
    
    # Coverage info
    if [[ -f "${BUILD_DIR}/coverage.info" ]]; then
        cp "${BUILD_DIR}/coverage.info" "${log_dir}/coverage/"
    fi
    
    if [[ -f "${BUILD_DIR}/coverage.filtered.info" ]]; then
        cp "${BUILD_DIR}/coverage.filtered.info" "${log_dir}/coverage/"
    fi
    
    # Coverage summary
    if [[ -f "${BUILD_DIR}/coverage_report" ]]; then
        cp -r "${BUILD_DIR}/coverage_report" "${log_dir}/coverage/"
    fi
    
    echo "Coverage logs collected"
}

collect_security_logs() {
    local log_dir="$1"
    
    # Static analysis reports
    if [[ -f "${MODULE_DIR}/cppcheck_report.txt" ]]; then
        cp "${MODULE_DIR}/cppcheck_report.txt" "${log_dir}/security/"
    fi
    
    if [[ -f "${MODULE_DIR}/trivy-results.sarif" ]]; then
        cp "${MODULE_DIR}/trivy-results.sarif" "${log_dir}/security/"
    fi
    
    if [[ -f "${MODULE_DIR}/bandit_report.json" ]]; then
        cp "${MODULE_DIR}/bandit_report.json" "${log_dir}/security/"
    fi
    
    echo "Security logs collected"
}

collect_quality_logs() {
    local log_dir="$1"
    
    # Benchmark results
    if [[ -f "${BUILD_DIR}/benchmark_report.md" ]]; then
        cp "${BUILD_DIR}/benchmark_report.md" "${log_dir}/quality/"
    fi
    
    # Fuzzing results
    find "${BUILD_DIR}" -name "crash-*" -o -name "timeout-*" -o -name "leak-*" | \
        head -20 > "${log_dir}/quality/fuzzing_issues.txt" 2>/dev/null || true
    
    echo "Quality logs collected"
}

generate_build_summary() {
    local log_dir="$1"
    local summary_file="${log_dir}/BUILD_SUMMARY.md"
    
    cat > "$summary_file" << EOF
# cupolas Module Build Summary

**Build Date:** $(date -u +%Y-%m-%dT%H:%M:%SZ)
**Git Commit:** $(git rev-parse HEAD 2>/dev/null || echo 'unknown')
**Git Branch:** $(git symbolic-ref --short HEAD 2>/dev/null || git describe --tags --exact-match 2>/dev/null || echo 'unknown')
**Build Host:** $(hostname 2>/dev/null || echo 'unknown')

## Build Statistics

| Metric | Value |
|--------|-------|
| Build Duration | N/A |
| Warnings | $(wc -l < "${log_dir}/build/warnings.txt" 2>/dev/null || echo '0') |
| Errors | $(wc -l < "${log_dir}/build/errors.txt" 2>/dev/null || echo '0') |
| Tests Run | $(find "${log_dir}/test" -name "*.xml" | wc -l 2>/dev/null || echo '0') |
| Test Failures | $(grep -c 'failures="[1-9]' "${log_dir}/test"/*.xml 2>/dev/null || echo '0') |

## Files Generated

$(find "$log_dir" -type f | sort | while read f; do echo "- $f"; done)

## Next Steps

1. Review any warnings or errors
2. Check test results
3. Verify coverage meets threshold
4. Review security scan results

---
*Generated by cupolas build log collector*
EOF
    
    echo "Build summary generated: $summary_file"
}

archive_logs() {
    local log_dir="$1"
    local archive_name="cupolas_build_logs_$(basename "$log_dir").tar.gz"
    
    tar -czvf "${LOG_DIR}/${archive_name}" -C "$(dirname "$log_dir")" "$(basename "$log_dir")"
    
    echo "Logs archived: ${LOG_DIR}/${archive_name}"
}

cleanup_old_logs() {
    local max_days="${1:-7}"
    
    find "${LOG_DIR}" -name "build_*" -type d -mtime +${max_days} -exec rm -rf {} \; 2>/dev/null || true
    find "${LOG_DIR}" -name "*.tar.gz" -mtime +${max_days} -delete 2>/dev/null || true
    
    echo "Cleaned up logs older than ${max_days} days"
}

# =============================================================================
# CLI Interface
# =============================================================================

print_usage() {
    cat << EOF
AgentOS cupolas Module Build Log Collector

Usage: $0 <command> [options]

Commands:
    collect         Collect all build logs
    cmake           Collect CMake logs only
    build           Collect build logs only
    test            Collect test logs only
    coverage        Collect coverage logs only
    security        Collect security logs only
    quality         Collect quality logs only
    summary         Generate build summary
    archive         Archive collected logs
    cleanup [days]  Clean up old logs (default: 7 days)

Options:
    -h, --help      Show this help message
    -d, --dir DIR   Specify log directory

Examples:
    $0 collect
    $0 collect -d /tmp/logs
    $0 summary
    $0 archive
    $0 cleanup 30
EOF
}

main() {
    local command="${1:-help}"
    shift || true
    
    local log_dir=""
    
    while [[ $# -gt 0 ]]; do
        case "$1" in
            -d|--dir)
                log_dir="$2"
                shift 2
                ;;
            -h|--help)
                print_usage
                exit 0
                ;;
            *)
                shift
                ;;
        esac
    done
    
    case "$command" in
        collect)
            log_dir="${log_dir:-$(init_log_dir)}"
            collect_cmake_logs "$log_dir"
            collect_build_logs "$log_dir"
            collect_test_logs "$log_dir"
            collect_coverage_logs "$log_dir"
            collect_security_logs "$log_dir"
            collect_quality_logs "$log_dir"
            generate_build_summary "$log_dir"
            echo ""
            echo "All logs collected to: $log_dir"
            ;;
        cmake)
            log_dir="${log_dir:-${LOG_DIR}/latest}"
            mkdir -p "$log_dir/build"
            collect_cmake_logs "$log_dir"
            ;;
        build)
            log_dir="${log_dir:-${LOG_DIR}/latest}"
            mkdir -p "$log_dir/build"
            collect_build_logs "$log_dir"
            ;;
        test)
            log_dir="${log_dir:-${LOG_DIR}/latest}"
            mkdir -p "$log_dir/test"
            collect_test_logs "$log_dir"
            ;;
        coverage)
            log_dir="${log_dir:-${LOG_DIR}/latest}"
            mkdir -p "$log_dir/coverage"
            collect_coverage_logs "$log_dir"
            ;;
        security)
            log_dir="${log_dir:-${LOG_DIR}/latest}"
            mkdir -p "$log_dir/security"
            collect_security_logs "$log_dir"
            ;;
        quality)
            log_dir="${log_dir:-${LOG_DIR}/latest}"
            mkdir -p "$log_dir/quality"
            collect_quality_logs "$log_dir"
            ;;
        summary)
            log_dir="${log_dir:-${LOG_DIR}/latest}"
            generate_build_summary "$log_dir"
            ;;
        archive)
            log_dir="${log_dir:-${LOG_DIR}/latest}"
            archive_logs "$log_dir"
            ;;
        cleanup)
            cleanup_old_logs "${1:-7}"
            ;;
        help|-h|--help)
            print_usage
            ;;
        *)
            echo "ERROR: Unknown command: $command" >&2
            print_usage
            exit 1
            ;;
    esac
}

main "$@"
