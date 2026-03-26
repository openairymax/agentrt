#!/usr/bin/env bash
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS 构建日志管理脚本
# 统一收集、管理和分析 CI/CD 构建日志

set -euo pipefail

###############################################################################
# 配置
###############################################################################
AGENTOS_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)")
AGENTOS_SCRIPTS_DIR="$(dirname "$AGENTOS_SCRIPT_DIR")"
AGENTOS_PROJECT_ROOT="$(dirname "$AGENTOS_SCRIPTS_DIR")"
AGENTOS_LOG_DIR="${AGENTOS_LOG_DIR:-$AGENTOS_PROJECT_ROOT/build/logs}"
AGENTOS_LOG_RETENTION_DAYS="${AGENTOS_LOG_RETENTION_DAYS:-30}"

###############################################################################
# 颜色定义
###############################################################################
COLOR_BOLD='\033[1m'
COLOR_GREEN='\033[0;32m'
COLOR_YELLOW='\033[1;33m'
COLOR_RED='\033[0;31m'
COLOR_CYAN='\033[0;36m'
COLOR_NC='\033[0m'

###############################################################################
# 日志级别
###############################################################################
readonly LOG_LEVEL_DEBUG=0
readonly LOG_LEVEL_INFO=1
readonly LOG_LEVEL_WARN=2
readonly LOG_LEVEL_ERROR=3
readonly LOG_LEVEL_CRITICAL=4

###############################################################################
# 全局变量
###############################################################################
SCRIPT_NAME="$(basename "$0")"
LOG_LEVEL="${LOG_LEVEL:-$LOG_LEVEL_INFO}"
LOG_FORMAT="${LOG_FORMAT:-text}"
LOG_OUTPUT="${LOG_OUTPUT:-stdout}"

###############################################################################
# 日志函数
###############################################################################
log() {
    local level="$1"
    shift || return
    local message="$*"
    local timestamp
    timestamp=$(date -Iseconds)
    local level_str=""

    case "$level" in
        DEBUG)   level_str="DEBUG" ;;
        INFO)    level_str="INFO" ;;
        WARN)    level_str="WARN" ;;
        ERROR)   level_str="ERROR" ;;
        CRITICAL) level_str="CRITICAL" ;;
        *)       level_str="UNKNOWN" ;;
    esac

    if [[ "$LOG_FORMAT" == "json" ]]; then
        local log_entry
        log_entry=$(jq -n \
            --arg ts "$timestamp" \
            --arg lvl "$level_str" \
            --arg msg "$message" \
            --arg script "$SCRIPT_NAME" \
            '{timestamp: $ts, level: $lvl, message: $msg, script: $script}')
        echo "$log_entry"
    else
        echo -e "${COLOR_CYAN}[$timestamp]${COLOR_NC} ${COLOR_BOLD}[$level_str]${COLOR_NC} $message"
    fi

    # 输出到文�?
    if [[ "$LOG_OUTPUT" != "stdout" ]] && [[ -n "$LOG_OUTPUT" ]]; then
        if [[ "$LOG_FORMAT" == "json" ]]; then
            echo "$log_entry" >> "$LOG_OUTPUT"
        else
            echo "[$timestamp] [$level_str] $message" >> "$LOG_OUTPUT"
        fi
    fi
}

log_debug() { log DEBUG "$@"; }
log_info() { log INFO "$@"; }
log_warn() { log WARN "$@"; }
log_error() { log ERROR "$@"; }
log_critical() { log CRITICAL "$@"; }

###############################################################################
# 帮助信息
###############################################################################
print_usage() {
    cat << EOF
${COLOR_BOLD}AgentOS 构建日志管理${COLOR_NC}

${COLOR_BOLD}用法:${COLOR_NC} $0 <命令> [选项]

${COLOR_BOLD}命令:${COLOR_NC}
    collect          收集构建日志
    analyze         分析日志
    report          生成报告
    archive         归档日志
    cleanup         清理过期日志
    tail            实时监控日志
    search          搜索日志

${COLOR_BOLD}选项:${COLOR_NC}
    --build-id <ID>     指定构建 ID
    --level <级别>      日志级别 (debug/info/warn/error)
    --format <格式>     输出格式 (text/json)
    --output <文件>     输出文件
    --days <天数>       保留天数
    --pattern <模式>    搜索模式
    --help             显示帮助

${COLOR_BOLD}示例:${COLOR_CD}
    $0 collect --build-id 12345
    $0 analyze --level error
    $0 report --output build-report.html
    $0 cleanup --days 30
    $0 search --pattern "ERROR.*timeout"

EOF
}

###############################################################################
# 初始�?
###############################################################################
init() {
    mkdir -p "$AGENTOS_LOG_DIR"
    mkdir -p "$AGENTOS_LOG_DIR/archive"
    mkdir -p "$AGENTOS_LOG_DIR/reports"
}

###############################################################################
# 收集构建日志
###############################################################################
collect_logs() {
    local build_id="${1:-$(date +%Y%m%d-%H%M%S)}"
    local collection_dir="$AGENTOS_LOG_DIR/builds/$build_id"

    log_info "开始收集构建日�?(ID: $build_id)"

    mkdir -p "$collection_dir"

    # 收集 Shell 脚本日志
    log_info "收集 Shell 脚本日志..."
    find "$AGENTOS_SCRIPTS_DIR" -name "*.sh" -type f -exec \
        bash -c 'echo "=== $1 ===" >> "$2/shell_scripts.log"' _ {} "$collection_dir" \; 2>/dev/null || true

    # 收集 Python 测试日志
    log_info "收集 Python 测试日志..."
    if [[ -d "$AGENTOS_SCRIPTS_DIR/tests/python" ]]; then
        find "$AGENTOS_SCRIPTS_DIR/tests/python" -name "*.log" -type f \
            -exec cp {} "$collection_dir/" \; 2>/dev/null || true
    fi

    # 收集 CI 工作流日�?
    log_info "收集 CI 工作流日�?.."
    if [[ -d "$AGENTOS_PROJECT_ROOT/.github/workflows" ]]; then
        find "$AGENTOS_PROJECT_ROOT/.github/workflows" -name "*.yml" -type f \
            -exec basename {} \; > "$collection_dir/workflows.txt" 2>/dev/null || true
    fi

    # 收集系统信息
    log_info "收集系统信息..."
    {
        echo "=== System Info ==="
        echo "Date: $(date -Iseconds)"
        echo "Hostname: $(hostname)"
        echo "OS: $(uname -a)"
        echo "Bash: ${BASH_VERSION}"
        echo "Python: $(python3 --version 2>&1 || echo 'N/A')"
        echo "Docker: $(docker --version 2>&1 || echo 'N/A')"
    } >> "$collection_dir/system_info.log"

    # 创建日志清单
    create_log_manifest "$collection_dir" "$build_id"

    log_success "日志收集完成: $collection_dir"

    echo "$collection_dir"
}

create_log_manifest() {
    local collection_dir="$1"
    local build_id="$2"

    local manifest_file="$collection_dir/manifest.json"

    # 收集文件信息
    local files_json="[]"
    while IFS= read -r -d '' file; do
        local size
        size=$(stat -c%s "$file" 2>/dev/null || stat -f%z "$file" 2>/dev/null || echo 0)
        local modified
        modified=$(stat -c%y "$file" 2>/dev/null || stat -f%Sm "$file" 2>/dev/null || echo "unknown")

        local file_json
        file_json=$(jq -n \
            --arg path "$file" \
            --arg size "$size" \
            --arg modified "$modified" \
            '{path: $path, size: ($size | tonumber), modified: $modified}')

        files_json=$(echo "$files_json" | jq ". += [$file_json]")
    done < <(find "$collection_dir" -type f -print0)

    # 创建清单
    jq -n \
        --arg build_id "$build_id" \
        --arg timestamp "$(date -Iseconds)" \
        --arg version "$AGENTOS_LOG_RETENTION_DAYS" \
        --argfiles files "$files_json" \
        '{
            build_id: $build_id,
            timestamp: $timestamp,
            retention_days: ($version | tonumber),
            files: $files
        }' > "$manifest_file"

    log_info "清单已创�? $manifest_file"
}

###############################################################################
# 分析日志
###############################################################################
analyze_logs() {
    local log_file="${1:-}"
    local level="${2:-ERROR}"

    log_info "分析日志文件: $log_file (级别: $level)"

    if [[ ! -f "$log_file" ]]; then
        log_error "日志文件不存�? $log_file"
        return 1
    fi

    echo ""
    echo "=== 日志分析报告 ==="
    echo ""

    # 统计各级别日志数�?
    echo "日志级别统计:"
    echo "  DEBUG:   $(grep -c "DEBUG" "$log_file" 2>/dev/null || echo 0)"
    echo "  INFO:    $(grep -c "INFO" "$log_file" 2>/dev/null || echo 0)"
    echo "  WARN:    $(grep -c "WARN" "$log_file" 2>/dev/null || echo 0)"
    echo "  ERROR:   $(grep -c "ERROR" "$log_file" 2>/dev/null || echo 0)"
    echo ""

    # 显示错误详情
    if [[ "$level" == "ERROR" ]] || [[ "$level" == "error" ]]; then
        echo "ERROR 日志详情:"
        grep -n "ERROR" "$log_file" 2>/dev/null || echo "  �?ERROR 日志"
        echo ""
    fi

    # 分析常见错误模式
    echo "错误模式分析:"
    local patterns=(
        "timeout"
        "connection refused"
        "permission denied"
        "not found"
        "out of memory"
        "segmentation fault"
    )

    for pattern in "${patterns[@]}"; do
        local count
        count=$(grep -ic "$pattern" "$log_file" 2>/dev/null || echo 0)
        if [[ $count -gt 0 ]]; then
            echo "  $pattern: $count �?
        fi
    done

    echo ""
}

###############################################################################
# 生成报告
###############################################################################
generate_report() {
    local output_file="${1:-build-report.html}"
    local build_id="${2:-latest}"

    log_info "生成构建报告: $output_file"

    local report_dir="$AGENTOS_LOG_DIR/reports"
    mkdir -p "$report_dir"

    # 生成 HTML 报告
    cat > "$report_dir/$output_file" << 'HTMLEOF'
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>AgentOS 构建报告</title>
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, sans-serif;
            line-height: 1.6;
            color: #333;
            max-width: 1200px;
            margin: 0 auto;
            padding: 20px;
            background: #f5f5f5;
        }
        .header {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 30px;
            border-radius: 10px;
            margin-bottom: 20px;
        }
        .header h1 {
            margin: 0 0 10px 0;
        }
        .header .meta {
            opacity: 0.9;
            font-size: 0.9em;
        }
        .card {
            background: white;
            border-radius: 10px;
            padding: 20px;
            margin-bottom: 20px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }
        .card h2 {
            margin-top: 0;
            color: #667eea;
            border-bottom: 2px solid #667eea;
            padding-bottom: 10px;
        }
        .stats {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 15px;
            margin: 20px 0;
        }
        .stat {
            background: #f8f9fa;
            padding: 15px;
            border-radius: 8px;
            text-align: center;
        }
        .stat .value {
            font-size: 2em;
            font-weight: bold;
            color: #667eea;
        }
        .stat .label {
            color: #666;
            font-size: 0.9em;
        }
        .success { color: #28a745; }
        .warning { color: #ffc107; }
        .error { color: #dc3545; }
        .info { color: #17a2b8; }
        table {
            width: 100%;
            border-collapse: collapse;
            margin: 15px 0;
        }
        th, td {
            padding: 12px;
            text-align: left;
            border-bottom: 1px solid #ddd;
        }
        th {
            background: #f8f9fa;
            font-weight: 600;
        }
        .badge {
            display: inline-block;
            padding: 4px 8px;
            border-radius: 4px;
            font-size: 0.85em;
            font-weight: 600;
        }
        .badge-pass { background: #d4edda; color: #155724; }
        .badge-fail { background: #f8d7da; color: #721c24; }
        .badge-warn { background: #fff3cd; color: #856404; }
        .badge-info { background: #d1ecf1; color: #0c5460; }
        .footer {
            text-align: center;
            color: #666;
            font-size: 0.9em;
            margin-top: 30px;
            padding-top: 20px;
            border-top: 1px solid #ddd;
        }
    </style>
</head>
<body>
    <div class="header">
        <h1>AgentOS 构建报告</h1>
        <div class="meta">
            <div>生成时间: TIMESTAMP</div>
            <div>构建 ID: BUILD_ID</div>
            <div>分支: BRANCH</div>
        </div>
    </div>

    <div class="card">
        <h2>构建统计</h2>
        <div class="stats">
            <div class="stat">
                <div class="value success">STATS_SUCCESS</div>
                <div class="label">成功</div>
            </div>
            <div class="stat">
                <div class="value error">STATS_FAILED</div>
                <div class="label">失败</div>
            </div>
            <div class="stat">
                <div class="value warning">STATS_WARNINGS</div>
                <div class="label">警告</div>
            </div>
            <div class="stat">
                <div class="value info">STATS_DURATION</div>
                <div class="label">耗时</div>
            </div>
        </div>
    </div>

    <div class="card">
        <h2>阶段状�?/h2>
        <table>
            <thead>
                <tr>
                    <th>阶段</th>
                    <th>状�?/th>
                    <th>耗时</th>
                    <th>详情</th>
                </tr>
            </thead>
            <tbody>
STAGES_TABLE
            </tbody>
        </table>
    </div>

    <div class="card">
        <h2>安全扫描结果</h2>
        <table>
            <thead>
                <tr>
                    <th>工具</th>
                    <th>发现�?/th>
                    <th>高危</th>
                    <th>中危</th>
                    <th>低危</th>
                </tr>
            </thead>
            <tbody>
SECURITY_TABLE
            </tbody>
        </table>
    </div>

    <div class="card">
        <h2>测试覆盖</h2>
        <div class="stats">
            <div class="stat">
                <div class="value">COVERAGE_LINE%</div>
                <div class="label">行覆盖率</div>
            </div>
            <div class="stat">
                <div class="value">COVERAGE_BRANCH%</div>
                <div class="label">分支覆盖�?/div>
            </div>
            <div class="stat">
                <div class="value">TESTS_PASSED/TESTS_TOTAL</div>
                <div class="label">测试通过</div>
            </div>
        </div>
    </div>

    <div class="footer">
        <p>AgentOS CI/CD 构建报告 | Generated by AgentOS Scripts v1.0.0</p>
    </div>
</body>
</html>
HTMLEOF

    # 替换占位�?
    sed -i "s/TIMESTAMP/$(date -Iseconds)/g" "$report_dir/$output_file"
    sed -i "s/BUILD_ID/$build_id/g" "$report_dir/$output_file"
    sed -i "s/BRANCH/$(git branch --show-current 2>/dev/null || echo 'N/A')/g" "$report_dir/$output_file"

    # 如果有日志数据，替换统计数据
    if [[ -f "$AGENTOS_LOG_DIR/latest/stats.json" ]]; then
        local stats
        stats=$(cat "$AGENTOS_LOG_DIR/latest/stats.json")

        local success failed warnings duration
        success=$(echo "$stats" | jq -r '.success // 0')
        failed=$(echo "$stats" | jq -r '.failed // 0')
        warnings=$(echo "$stats" | jq -r '.warnings // 0')
        duration=$(echo "$stats" | jq -r '.duration // "N/A"')

        sed -i "s/STATS_SUCCESS/$success/g" "$report_dir/$output_file"
        sed -i "s/STATS_FAILED/$failed/g" "$report_dir/$output_file"
        sed -i "s/STATS_WARNINGS/$warnings/g" "$report_dir/$output_file"
        sed -i "s/STATS_DURATION/${duration}s/g" "$report_dir/$output_file"
    fi

    log_success "报告已生�? $report_dir/$output_file"

    echo "$report_dir/$output_file"
}

###############################################################################
# 归档日志
###############################################################################
archive_logs() {
    local build_id="${1:-}"
    local archive_file=""

    log_info "归档日志..."

    if [[ -z "$build_id" ]]; then
        # 归档所有未归档的日�?
        build_id="all-$(date +%Y%m%d)"
    fi

    local source_dir="$AGENTOS_LOG_DIR/builds"
    local archive_dir="$AGENTOS_LOG_DIR/archive"
    local archive_name="agentos-logs-${build_id}-$(date +%Y%m%d-%H%M%S).tar.gz"

    mkdir -p "$archive_dir"

    if [[ -d "$source_dir" ]]; then
        archive_file="$archive_dir/$archive_name"
        tar -czvf "$archive_file" -C "$source_dir" . 2>/dev/null || true
        log_success "日志已归�? $archive_file"
    else
        log_warn "没有找到要归档的日志"
    fi

    echo "$archive_file"
}

###############################################################################
# 清理过期日志
###############################################################################
cleanup_logs() {
    local days="${1:-$AGENTOS_LOG_RETENTION_DAYS}"

    log_info "清理 ${days} 天前的日�?.."

    local deleted_count=0

    # 清理构建日志
    if [[ -d "$AGENTOS_LOG_DIR/builds" ]]; then
        while IFS= read -r -d '' dir; do
            local age_days
            age_days=$((($(date +%s) - $(stat -c%Y "$dir" 2>/dev/null || stat -f%m "$dir" 2>/dev/null || echo 0)) / 86400))

            if [[ $age_days -gt $days ]]; then
                rm -rf "$dir"
                ((deleted_count++))
                log_debug "已删�? $dir"
            fi
        done < <(find "$AGENTOS_LOG_DIR/builds" -type d -maxdepth 1 -print0)
    fi

    # 清理归档日志
    if [[ -d "$AGENTOS_LOG_DIR/archive" ]]; then
        find "$AGENTOS_LOG_DIR/archive" -name "*.tar.gz" -type f -mtime "+$days" -delete 2>/dev/null || true
    fi

    # 清理临时文件
    find "$AGENTOS_LOG_DIR" -name "*.tmp" -type f -mtime "+1" -delete 2>/dev/null || true

    log_success "清理完成，删除了 $deleted_count 个目�?

    return 0
}

###############################################################################
# 实时监控日志
###############################################################################
tail_logs() {
    local log_file="${1:-}"
    local filter="${2:-}"

    log_info "实时监控日志: $log_file"

    if [[ ! -f "$log_file" ]]; then
        log_error "日志文件不存�? $log_file"
        return 1
    fi

    if [[ -n "$filter" ]]; then
        tail -f "$log_file" | grep --line-buffered "$filter"
    else
        tail -f "$log_file"
    fi
}

###############################################################################
# 搜索日志
###############################################################################
search_logs() {
    local pattern="${1:-}"
    local log_dir="${2:-$AGENTOS_LOG_DIR/builds}"
    local context="${3:-3}"

    if [[ -z "$pattern" ]]; then
        log_error "未指定搜索模�?
        return 1
    fi

    log_info "搜索模式: $pattern"

    local results_file="$AGENTOS_LOG_DIR/search-results-$(date +%Y%m%d-%H%M%S).txt"
    local match_count=0

    echo "搜索结果 - 模式: $pattern" > "$results_file"
    echo "时间: $(date -Iseconds)" >> "$results_file"
    echo "========================================" >> "$results_file"
    echo "" >> "$results_file"

    # 在所有日志文件中搜索
    while IFS= read -r -d '' log; do
        if grep -q "$pattern" "$log" 2>/dev/null; then
            echo "文件: $log" >> "$results_file"
            grep -n -A "$context" -B "$context" "$pattern" "$log" 2>/dev/null >> "$results_file" || true
            echo "" >> "$results_file"
            ((match_count++))
        fi
    done < <(find "$log_dir" -name "*.log" -type f -print0)

    log_success "找到 $match_count 个匹配项"
    log_info "结果保存�? $results_file"

    # 显示摘要
    grep -n "$pattern" "$results_file" | head -20

    return 0
}

###############################################################################
# 主入�?
###############################################################################
main() {
    init

    local command="${1:-}"
    shift || true

    local build_id=""
    local log_file=""
    local level="INFO"
    local format="text"
    local output=""
    local days="$AGENTOS_LOG_RETENTION_DAYS"
    local pattern=""
    local dry_run="false"

    # 解析参数
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --build-id)
                build_id="$2"
                shift 2
                ;;
            --level)
                level="$2"
                shift 2
                ;;
            --format)
                format="$2"
                shift 2
                ;;
            --output)
                output="$2"
                shift 2
                ;;
            --days)
                days="$2"
                shift 2
                ;;
            --pattern)
                pattern="$2"
                shift 2
                ;;
            --dry-run)
                dry_run="true"
                shift
                ;;
            --help|-h)
                print_usage
                return 0
                ;;
            *)
                if [[ -z "$command" ]]; then
                    command="$1"
                elif [[ -z "$log_file" ]]; then
                    log_file="$1"
                fi
                shift
                ;;
        esac
    done

    LOG_FORMAT="$format"

    case "$command" in
        collect)
            collect_logs "$build_id"
            ;;
        analyze)
            analyze_logs "$log_file" "$level"
            ;;
        report)
            generate_report "$output" "$build_id"
            ;;
        archive)
            archive_logs "$build_id"
            ;;
        cleanup)
            if [[ "$dry_run" == "true" ]]; then
                log_warn "[DRY RUN] 模拟清理"
            else
                cleanup_logs "$days"
            fi
            ;;
        tail)
            tail_logs "$log_file" "$pattern"
            ;;
        search)
            search_logs "$pattern" "$log_dir" "$context"
            ;;
        "")
            print_usage
            ;;
        *)
            log_error "未知命令: $command"
            print_usage
            return 1
            ;;
    esac
}

main "$@"
