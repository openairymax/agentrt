#!/usr/bin/env bash
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS 回滚管理脚本
# 提供版本回滚和部署历史管理功能

set -euo pipefail

###############################################################################
# 配置
###############################################################################
AGENTOS_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
AGENTOS_SCRIPTS_DIR="$(dirname "$AGENTOS_SCRIPT_DIR")"
AGENTOS_PROJECT_ROOT="$(dirname "$AGENTOS_SCRIPTS_DIR")"
AGENTOS_ROLLBACK_DIR="${AGENTOS_ROLLBACK_DIR:-$AGENTOS_PROJECT_ROOT/build/rollback}"
AGENTOS_DEPLOYMENT_HISTORY="${AGENTOS_ROLLBACK_DIR}/deployment_history.json"

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
# 日志函数
###############################################################################
log_info() {
    echo -e "${COLOR_CYAN}[INFO]${COLOR_NC} $1"
}

log_success() {
    echo -e "${COLOR_GREEN}[SUCCESS]${COLOR_NC} $1"
}

log_warn() {
    echo -e "${COLOR_YELLOW}[WARN]${COLOR_NC} $1"
}

log_error() {
    echo -e "${COLOR_RED}[ERROR]${COLOR_NC} $1"
}

###############################################################################
# 帮助信息
###############################################################################
print_usage() {
    cat << EOF
${COLOR_BOLD}AgentOS 回滚管理${COLOR_NC}

${COLOR_BOLD}用法:${COLOR_NC} $0 <命令> [选项]

${COLOR_BOLD}命令:${COLOR_NC}
    history              显示部署历史
    rollback             执行回滚
    cleanup              清理旧版本
    verify               验证当前版本

${COLOR_BOLD}选项:${COLOR_NC}
    --version <版本>     指定回滚版本
    --environment <环境> 指定环境 (staging/production)
    --dry-run            模拟运行
    --help               显示帮助

${COLOR_BOLD}示例:${COLOR_NC}
    $0 history                           # 显示部署历史
    $0 rollback --version v1.0.0       # 回滚到指定版本
    $0 cleanup --keep 5                 # 保留最近5个版本

EOF
}

###############################################################################
# 初始化
###############################################################################
init() {
    mkdir -p "$AGENTOS_ROLLBACK_DIR"
    mkdir -p "$AGENTOS_ROLLBACK_DIR/images"
    mkdir -p "$AGENTOS_ROLLBACK_DIR/configs"

    if [[ ! -f "$AGENTOS_DEPLOYMENT_HISTORY" ]]; then
        echo "[]" > "$AGENTOS_DEPLOYMENT_HISTORY"
    fi
}

###############################################################################
# 获取部署历史
###############################################################################
get_history() {
    local environment="${1:-all}"
    local limit="${2:-10}"

    log_info "部署历史 (环境: $environment, 限制: $limit)"

    if [[ ! -f "$AGENTOS_DEPLOYMENT_HISTORY" ]]; then
        log_warn "没有部署历史记录"
        return 0
    fi

    echo ""
    echo -e "${COLOR_BOLD}版本${COLOR_NC}          ${COLOR_BOLD}环境${COLOR_NC}       ${COLOR_BOLD}状态${COLOR_NC}    ${COLOR_BOLD}部署时间${COLOR_NC}           ${COLOR_BOLD}操作者${COLOR_NC}"
    echo "--------------------------------------------------------------------------------"

    # 解析 JSON 历史记录
    local history
    history=$(cat "$AGENTOS_DEPLOYMENT_HISTORY")

    if [[ "$history" == "[]" ]]; then
        echo "没有部署记录"
        return 0
    fi

    # 显示历史记录
    echo "$history" | jq -r \
        --arg env "$environment" \
        --argjson limit "$limit" \
        '.[] |
        select($env == "all" or .environment == $env) |
        select(.version != "rollback") |
        "\(.version // "N/A")\t\(.environment // "N/A")\t\(.status // "N/A")\t\(.deployed_at // "N/A")\t\(.deployed_by // "N/A")"' \
        2>/dev/null \
        | head -n "$limit"

    echo ""
}

###############################################################################
# 记录部署
###############################################################################
record_deployment() {
    local version="$1"
    local environment="$2"
    local status="$3"
    local deployed_by="${4:-system}"

    local timestamp
    timestamp=$(date -Iseconds)

    local new_record
    new_record=$(jq -n \
        --arg version "$version" \
        --arg environment "$environment" \
        --arg status "$status" \
        --arg deployed_at "$timestamp" \
        --arg deployed_by "$deployed_by" \
        '{
            version: $version,
            environment: $environment,
            status: $status,
            deployed_at: $deployed_at,
            deployed_by: $deployed_by,
            rollback_available: true
        }')

    # 读取现有历史
    local history="[]"
    if [[ -f "$AGENTOS_DEPLOYMENT_HISTORY" ]]; then
        history=$(cat "$AGENTOS_DEPLOYMENT_HISTORY")
    fi

    # 添加新记录
    local updated_history
    updated_history=$(echo "$history" | jq ". += [$new_record]")

    # 保存
    echo "$updated_history" > "$AGENTOS_DEPLOYMENT_HISTORY"

    log_success "部署记录已保存: $version -> $environment"
}

###############################################################################
# 获取可用版本
###############################################################################
get_available_versions() {
    local environment="$1"

    log_info "可用的回滚版本:"
    echo ""
    echo -e "${COLOR_BOLD}版本${COLOR_NC}          ${COLOR_BOLD}标签${COLOR_NC}                      ${COLOR_BOLD}大小${COLOR_NC}    ${COLOR_BOLD}创建时间${COLOR_NC}"
    echo "--------------------------------------------------------------------------------"

    # 从 Docker 镜像获取
    if command -v docker &> /dev/null; then
        docker images "spharx/agentos-scripts:*" \
            --format "{{.Tag}}\t{{.CreatedAt}}\t{{.Size}}" \
            2>/dev/null \
            | while IFS=$'\t' read -r tag created size; do
                if [[ "$tag" == *"rollback"* ]] || [[ "$tag" == "latest"* ]] || [[ "$tag" == "staging"* ]]; then
                    echo -e "$tag\t$size\t$created"
                fi
            done
    fi

    # 从本地备份获取
    if [[ -d "$AGENTOS_ROLLBACK_DIR/images" ]]; then
        ls -lh "$AGENTOS_ROLLBACK_DIR/images/" 2>/dev/null | tail -n +2 | awk '{print $9, $5, $6, $7, $8}'
    fi

    echo ""
}

###############################################################################
# 执行回滚
###############################################################################
do_rollback() {
    local version="${1:-}"
    local environment="${2:-staging}"
    local dry_run="${3:-false}"

    if [[ -z "$version" ]]; then
        log_error "未指定回滚版本"
        return 1
    fi

    log_info "开始回滚..."
    log_info "目标版本: $version"
    log_info "目标环境: $environment"

    if [[ "$dry_run" == "true" ]]; then
        log_warn "[DRY RUN] 模拟回滚操作"
    fi

    # 确认操作
    if [[ "$dry_run" != "true" ]] && [[ -t 0 ]]; then
        echo -n "确认回滚到版本 $version (y/N): "
        read -r confirm
        if [[ "$confirm" != "y" ]] && [[ "$confirm" != "Y" ]]; then
            log_info "取消回滚"
            return 0
        fi
    fi

    # 保存当前版本用于回滚
    if command -v docker &> /dev/null; then
        local current_tag="production"
        if [[ "$environment" == "staging" ]]; then
            current_tag="staging"
        fi

        if [[ -n "$(docker images -q spharx/agentos-scripts:$current_tag 2>/dev/null)" ]]; then
            local current_version
            current_version=$(docker images -q spharx/agentos-scripts:$current_tag 2>/dev/null | head -1)

            if [[ "$dry_run" != "true" ]]; then
                # 标记当前版本为可回滚
                docker tag "spharx/agentos-scripts:$current_tag" "spharx/agentos-scripts:rollback-$version"
                # 记录当前部署
                record_deployment "rollback-$version" "$environment" "rollback_initiated" "system"
            fi

            log_info "当前版本已备份为: rollback-$version"
        fi
    fi

    # 执行回滚
    if [[ "$dry_run" != "true" ]]; then
        if command -v docker &> /dev/null; then
            # 拉取目标版本
            log_info "拉取镜像: spharx/agentos-scripts:$version"
            docker pull "spharx/agentos-scripts:$version" || true

            # 标记为目标版本
            local target_tag="production"
            if [[ "$environment" == "staging" ]]; then
                target_tag="staging"
            fi

            docker tag "spharx/agentos-scripts:$version" "spharx/agentos-scripts:$target_tag"

            # 重启服务
            log_info "重启服务..."
            case "$environment" in
                staging)
                    docker-compose -f "$AGENTOS_SCRIPTS_DIR/deploy/docker/docker-compose.staging.yml" up -d
                    ;;
                production)
                    docker-compose -f "$AGENTOS_SCRIPTS_DIR/deploy/docker/docker-compose.yml" up -d
                    ;;
                *)
                    log_error "未知环境: $environment"
                    return 1
                    ;;
            esac

            # 记录回滚
            record_deployment "$version" "$environment" "rolled_back" "system"
        fi
    fi

    log_success "回滚完成: $version"
    return 0
}

###############################################################################
# 验证版本
###############################################################################
verify_version() {
    local expected_version="$1"
    local max_retries=30
    local retry_count=0

    log_info "验证版本: $expected_version"

    while [[ $retry_count -lt $max_retries ]]; do
        # 检查容器健康状态
        if docker ps --filter "name=agentos" --filter "status=running" | grep -q agentos; then
            log_success "容器运行正常"

            # 可选：验证版本号
            # 如果服务提供版本 API，可以在这里调用验证

            return 0
        fi

        sleep 2
        ((retry_count++))
        echo -n "."
    done

    echo ""
    log_error "验证超时"
    return 1
}

###############################################################################
# 清理旧版本
###############################################################################
cleanup_old_versions() {
    local keep="${1:-5}"

    log_info "清理旧版本，保留最近 $keep 个..."

    # 清理 Docker 镜像
    if command -v docker &> /dev/null; then
        # 获取所有回滚镜像
        local rollback_images
        rollback_images=$(docker images "spharx/agentos-scripts:rollback-*" --format "{{.Tag}}" 2>/dev/null || true)

        if [[ -n "$rollback_images" ]]; then
            local count
            count=$(echo "$rollback_images" | wc -l)

            if [[ $count -gt $keep ]]; then
                local to_delete
                to_delete=$(echo "$rollback_images" | tail -n $((count - keep)))

                while IFS= read -r image; do
                    if [[ -n "$image" ]]; then
                        log_info "删除镜像: $image"
                        docker rmi "spharx/agentos-scripts:$image" 2>/dev/null || true
                    fi
                done <<< "$to_delete"
            fi
        fi

        # 清理悬空镜像
        docker image prune -f > /dev/null 2>&1 || true

        # 清理本地备份
        if [[ -d "$AGENTOS_ROLLBACK_DIR/images" ]]; then
            find "$AGENTOS_ROLLBACK_DIR/images" -name "*.tar.gz" \
                -type f -mtime +30 \
                -delete 2>/dev/null || true
        fi

        # 清理过期的部署记录
        if [[ -f "$AGENTOS_DEPLOYMENT_HISTORY" ]]; then
            local updated_history
            updated_history=$(cat "$AGENTOS_DEPLOYMENT_HISTORY" | jq \
                --argjson keep "$keep" \
                '.[-$keep:]')

            echo "$updated_history" > "$AGENTOS_DEPLOYMENT_HISTORY"
        fi

    fi

    log_success "清理完成"
}

###############################################################################
# 主入口
###############################################################################
main() {
    init

    local command="${1:-}"
    shift || true

    local version=""
    local environment="staging"
    local dry_run="false"
    local keep="5"

    # 解析参数
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --version)
                version="$2"
                shift 2
                ;;
            --environment)
                environment="$2"
                shift 2
                ;;
            --dry-run)
                dry_run="true"
                shift
                ;;
            --keep)
                keep="$2"
                shift 2
                ;;
            --help|-h)
                print_usage
                return 0
                ;;
            *)
                break
                ;;
        esac
    done

    case "$command" in
        history)
            get_history "$environment" "$keep"
            ;;
        rollback)
            if [[ -z "$version" ]]; then
                log_error "未指定版本"
                print_usage
                return 1
            fi
            do_rollback "$version" "$environment" "$dry_run"
            ;;
        cleanup)
            cleanup_old_versions "$keep"
            ;;
        verify)
            if [[ -z "$version" ]]; then
                log_error "未指定版本"
                print_usage
                return 1
            fi
            verify_version "$version"
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