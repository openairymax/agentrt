#!/usr/bin/env bash
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS Scripts 模块 CI/CD 入口脚本
# 统一触发 CI/CD 各阶段任�?

set -euo pipefail

###############################################################################
# 配置
###############################################################################
AGENTOS_CICD_VERSION="1.0.0"
AGENTOS_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)")
AGENTOS_SCRIPTS_DIR="$(dirname "$AGENTOS_SCRIPT_DIR")"
AGENTOS_PROJECT_ROOT="$(dirname "$AGENTOS_SCRIPTS_DIR")"

# 导入通用函数
# shellcheck source=../lib/common.sh
source "$AGENTOS_SCRIPTS_DIR/lib/common.sh"

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
${COLOR_BOLD}AgentOS Scripts CI/CD 入口${COLOR_NC}

${COLOR_BOLD}用法:${COLOR_NC} $0 <命令> [选项]

${COLOR_BOLD}命令:${COLOR_NC}
    ci              运行 CI 流程（代码检查、测试）
    build           运行构建流程
    test            运行测试
    security        运行安全扫描
    deploy          部署到指定环�?
    rollback        回滚到上一版本
    release         创建发布版本

${COLOR_BOLD}部署环境:${COLOR_NC}
    dev             开发环�?
    staging         预发布环�?
    preview         PR预览环境
    production      生产环境

${COLOR_BOLD}选项:${COLOR_NC}
    --version       显示版本信息
    --verbose       详细输出
    --dry-run       模拟运行
    --help          显示帮助

${COLOR_BOLD}示例:${COLOR_NC}
    $0 ci                           # 运行 CI 流程
    $0 deploy staging                # 部署到预发布环境
    $0 rollback production           # 回滚生产环境
    $0 release --tag v1.0.0        # 创建发布版本

EOF
}

###############################################################################
# CI 流程
###############################################################################
run_ci() {
    log_info "开�?CI 流程..."

    local exit_code=0

    # 1. 代码质量检�?
    log_info "阶段 1: 代码质量检�?
    if ! run_code_quality; then
        exit_code=1
    fi

    # 2. Shell 脚本检�?
    log_info "阶段 2: Shell 脚本检�?
    if ! run_shell_check; then
        exit_code=1
    fi

    # 3. Python 代码检�?
    log_info "阶段 3: Python 代码检�?
    if ! run_python_check; then
        exit_code=1
    fi

    # 4. 单元测试
    log_info "阶段 4: 单元测试"
    if ! run_unit_tests; then
        exit_code=1
    fi

    if [[ $exit_code -eq 0 ]]; then
        log_success "CI 流程完成"
    else
        log_error "CI 流程失败"
    fi

    return $exit_code
}

run_code_quality() {
    log_info "检查代码质�?.."

    # 检查文件格�?
    local has_errors=0

    # 检查所�?shell 脚本语法
    while IFS= read -r -d '' script; do
        if ! bash -n "$script" 2>/dev/null; then
            log_error "Shell 语法错误: $script"
            has_errors=1
        fi
    done < <(find "$AGENTOS_SCRIPTS_DIR" -name "*.sh" -type f -print0)

    # 检查所�?Python 文件语法
    while IFS= read -r -d '' pyfile; do
        if ! python3 -m py_compile "$pyfile" 2>/dev/null; then
            log_error "Python 语法错误: $pyfile"
            has_errors=1
        fi
    done < <(find "$AGENTOS_SCRIPTS_DIR" -name "*.py" -type f -print0)

    if [[ $has_errors -eq 0 ]]; then
        log_success "代码质量检查通过"
        return 0
    else
        log_error "代码质量检查失�?
        return 1
    fi
}

run_shell_check() {
    log_info "运行 ShellCheck..."

    if ! command -v shellcheck &> /dev/null; then
        log_warn "ShellCheck 未安装，跳过 Shell 检�?
        return 0
    fi

    local exit_code=0

    # shellcheck -S warning scripts/**/*.sh
    while IFS= read -r -d '' script; do
        if ! shellcheck -S warning "$script" 2>/dev/null; then
            log_error "ShellCheck 失败: $script"
            exit_code=1
        fi
    done < <(find "$AGENTOS_SCRIPTS_DIR" -name "*.sh" -type f -print0)

    if [[ $exit_code -eq 0 ]]; then
        log_success "ShellCheck 检查通过"
    fi

    return $exit_code
}

run_python_check() {
    log_info "运行 Python 代码检�?.."

    local exit_code=0

    # 使用 ruff 进行 lint 检�?
    if command -v ruff &> /dev/null; then
        if ! ruff check "$AGENTOS_SCRIPTS_DIR" --select=E,F,W; then
            log_error "Ruff 检查失�?
            exit_code=1
        fi
    else
        log_warn "Ruff 未安装，使用 flake8"
        if command -v flake8 &> /dev/null; then
            flake8 "$AGENTOS_SCRIPTS_DIR" --select=E,F,W || exit_code=1
        fi
    fi

    # 使用 mypy 进行类型检�?
    if command -v mypy &> /dev/null; then
        if ! mypy "$AGENTOS_SCRIPTS_DIR/core" --ignore-missing-imports; then
            log_warn "Mypy 类型检查有警告"
        fi
    fi

    if [[ $exit_code -eq 0 ]]; then
        log_success "Python 代码检查通过"
    fi

    return $exit_code
}

run_unit_tests() {
    log_info "运行单元测试..."

    local exit_code=0

    # 运行 Shell 测试
    if [[ -d "$AGENTOS_SCRIPTS_DIR/tests/shell" ]]; then
        for test_script in "$AGENTOS_SCRIPTS_DIR/tests/shell"/test_*.sh; do
            if [[ -x "$test_script" ]]; then
                log_info "运行 Shell 测试: $(basename "$test_script")"
                if ! bash "$test_script"; then
                    log_error "Shell 测试失败: $test_script"
                    exit_code=1
                fi
            fi
        done
    fi

    # 运行 Python 测试
    if [[ -d "$AGENTOS_SCRIPTS_DIR/tests/python" ]]; then
        if command -v pytest &> /dev/null; then
            log_info "运行 Python 测试..."
            if ! pytest "$AGENTOS_SCRIPTS_DIR/tests/python/" -v --tb=short; then
                log_error "Python 测试失败"
                exit_code=1
            fi
        else
            log_warn "pytest 未安装，跳过 Python 测试"
        fi
    fi

    if [[ $exit_code -eq 0 ]]; then
        log_success "单元测试通过"
    fi

    return $exit_code
}

###############################################################################
# 构建流程
###############################################################################
run_build() {
    log_info "开始构建流�?.."

    local version="${1:-}"
    local build_type="${2:-release}"

    if [[ -z "$version" ]]; then
        version=$(git -C "$AGENTOS_PROJECT_ROOT" describe --tags 2>/dev/null || echo "0.0.0")
    fi

    log_info "构建版本: $version (类型: $build_type)"

    local build_dir="$AGENTOS_PROJECT_ROOT/build/scripts/$version"
    mkdir -p "$build_dir"

    # 1. 构建 Shell 脚本�?
    log_info "打包 Shell 脚本..."
    local shell_tar="$build_dir/agentos-scripts-${version}-shell.tar.gz"
    tar -czvf "$shell_tar" \
        -C "$AGENTOS_SCRIPTS_DIR" \
        --exclude='*.pyc' \
        --exclude='__pycache__' \
        --exclude='*.pyo' \
        --exclude='.pytest_cache' \
        . || return 1
    log_success "Shell 包已创建: $shell_tar"

    # 2. 构建 Python wheel
    log_info "构建 Python �?.."
    if [[ -f "$AGENTOS_PROJECT_ROOT/pyproject.toml" ]]; then
        cd "$AGENTOS_PROJECT_ROOT"
        python3 -m build || log_warn "Python 构建失败，跳�?
        cd - > /dev/null
    fi

    # 3. 构建 Docker 镜像
    log_info "构建 Docker 镜像..."
    if [[ -f "$AGENTOS_SCRIPTS_DIR/deploy/docker/Dockerfile.service" ]]; then
        local docker_tag="spharx/agentos-scripts:${version}"
        if docker build \
            -t "$docker_tag" \
            -t "spharx/agentos-scripts:latest" \
            -f "$AGENTOS_SCRIPTS_DIR/deploy/docker/Dockerfile.service" \
            "$AGENTOS_PROJECT_ROOT"; then
            log_success "Docker 镜像已构�? $docker_tag"
        else
            log_warn "Docker 构建失败"
        fi
    fi

    log_success "构建流程完成"

    # 生成构建清单
    local manifest="$build_dir/build-manifest.json"
    cat > "$manifest" << EOF
{
    "version": "$version",
    "build_type": "$build_type",
    "build_time": "$(date -Iseconds)",
    "build_dir": "$build_dir",
    "artifacts": [
        {"type": "shell", "path": "$shell_tar"},
        {"type": "docker", "tag": "spharx/agentos-scripts:${version}"}
    ]
}
EOF
    log_info "构建清单: $manifest"

    return 0
}

###############################################################################
# 安全扫描
###############################################################################
run_security() {
    log_info "开始安全扫�?.."

    local exit_code=0
    local report_dir="$AGENTOS_PROJECT_ROOT/build/security-reports"
    mkdir -p "$report_dir"

    # 1. Bandit 安全扫描
    log_info "运行 Bandit 安全扫描..."
    if command -v bandit &> /dev/null; then
        bandit -r "$AGENTOS_SCRIPTS_DIR/core" \
            -f json \
            -o "$report_dir/bandit-report.json" || true
        log_success "Bandit 报告: $report_dir/bandit-report.json"
    else
        log_warn "Bandit 未安装，跳过"
    fi

    # 2. Safety 依赖检�?
    log_info "运行 Safety 依赖检�?.."
    if command -v safety &> /dev/null; then
        safety check --json || true
    else
        log_warn "Safety 未安装，跳过"
    fi

    # 3. 秘钥扫描
    log_info "运行秘钥扫描..."
    if command -v trufflehog &> /dev/null; then
        trufflehog filesystem "$AGENTOS_SCRIPTS_DIR" \
            --json > "$report_dir/trufflehog-report.json" || true
        log_success "TruffleHog 报告: $report_dir/trufflehog-report.json"
    else
        log_warn "TruffleHog 未安装，跳过"
    fi

    log_success "安全扫描完成"

    return $exit_code
}

###############################################################################
# 部署流程
###############################################################################
run_deploy() {
    local environment="${1:-staging}"
    local version="${2:-latest}"

    log_info "部署�?$environment 环境 (版本: $version)..."

    case "$environment" in
        dev)
            deploy_to_dev "$version"
            ;;
        staging)
            deploy_to_staging "$version"
            ;;
        preview)
            deploy_to_preview "$version"
            ;;
        production)
            deploy_to_production "$version"
            ;;
        *)
            log_error "未知环境: $environment"
            return 1
            ;;
    esac
}

deploy_to_dev() {
    local version="$1"
    log_info "部署到开发环�?.."

    export AGENTOS_ENV="development"
    export AGENTOS_LOG_LEVEL="DEBUG"

    log_success "开发环境部署完�?
}

deploy_to_staging() {
    local version="$1"
    log_info "部署到预发布环境..."

    if ! command -v docker &> /dev/null; then
        log_error "Docker 未安装，无法部署�?staging"
        return 1
    fi

    # 使用 docker-compose 部署
    local compose_file="$AGENTOS_SCRIPTS_DIR/deploy/docker/docker-compose.staging.yml"

    if [[ -f "$compose_file" ]]; then
        docker-compose -f "$compose_file" pull
        docker-compose -f "$compose_file" up -d
        log_success "预发布环境部署完�?
    else
        log_warn "未找�?staging 配置文件，使用默认配�?
        docker-compose -f "$AGENTOS_SCRIPTS_DIR/deploy/docker/docker-compose.yml" up -d
    fi

    # 验证部署
    if validate_deployment; then
        log_success "预发布环境部署验证通过"
    else
        log_error "预发布环境部署验证失�?
        return 1
    fi
}

deploy_to_preview() {
    local version="$1"
    log_info "部署�?PR 预览环境..."

    local pr_number="${GITHUB_PR_NUMBER:-${CI_MERGE_REQUEST_ID:-0}}"
    local preview_tag="pr-$pr_number"

    if ! command -v docker &> /dev/null; then
        log_error "Docker 未安�?
        return 1
    fi

    # 构建预览镜像
    docker build \
        -t "spharx/agentos-scripts:preview-$preview_tag" \
        -f "$AGENTOS_SCRIPTS_DIR/deploy/docker/Dockerfile.service" \
        "$AGENTOS_PROJECT_ROOT"

    log_success "PR 预览环境部署完成 (Tag: $preview_tag)"
}

deploy_to_production() {
    local version="$1"
    log_info "部署到生产环�?.."

    # 生产部署需要确�?
    if [[ -t 0 ]] && ! agentos_confirm "确认部署到生产环�?"; then
        log_info "取消生产部署"
        return 0
    fi

    # 执行蓝绿部署
    perform_blue_green_deploy "$version"

    log_success "生产环境部署完成"
}

perform_blue_green_deploy() {
    local version="$1"
    local green_tag="green-$version"
    local active_tag="production"

    log_info "执行蓝绿部署..."

    # 获取当前活动版本
    local current_version
    current_version=$(docker tag | grep "spharx/agentos-scripts:$active_tag" || echo "")

    # 部署新版本到非活动环�?
    docker build \
        -t "spharx/agentos-scripts:$green_tag" \
        -f "$AGENTOS_SCRIPTS_DIR/deploy/docker/Dockerfile.service" \
        "$AGENTOS_PROJECT_ROOT"

    # 验证新版�?
    if validate_deployment "green"; then
        # 切换流量到新版本
        docker tag "spharx/agentos-scripts:$green_tag" "spharx/agentos-scripts:$active_tag"

        # 保留旧版本用于回�?
        if [[ -n "$current_version" ]]; then
            docker tag "spharx/agentos-scripts:$current_version" "spharx/agentos-scripts:rollback-$current_version"
        fi

        log_success "蓝绿部署切换完成"
    else
        log_error "新版本验证失败，保持当前版本"
        return 1
    fi
}

validate_deployment() {
    local target="${1:-default}"
    log_info "验证部署 (目标: $target)..."

    local max_retries=30
    local retry_count=0

    while [[ $retry_count -lt $max_retries ]]; do
        # 检查容器健康状�?
        if docker ps --filter "name=agentos" --filter "status=running" | grep -q agentos; then
            log_success "容器运行正常"
            return 0
        fi

        sleep 2
        ((retry_count++))
    done

    log_error "部署验证超时"
    return 1
}

###############################################################################
# 回滚流程
###############################################################################
run_rollback() {
    local environment="${1:-production}"
    local target_version="${2:-}"

    log_info "�?$environment 环境回滚..."

    if [[ "$environment" == "production" ]]; then
        # 生产环境回滚需要确�?
        if [[ -t 0 ]] && ! agentos_confirm "确认回滚生产环境?"; then
            log_info "取消回滚"
            return 0
        fi
    fi

    if [[ -z "$target_version" ]]; then
        # 尝试获取上一个版�?
        target_version=$(docker images "spharx/agentos-scripts:*" \
            --format "{{.Tag}}" \
            | grep -E "^rollback-" \
            | head -1 \
            | sed 's/^rollback-//')

        if [[ -z "$target_version" ]]; then
            log_error "未找到可回滚的版�?
            return 1
        fi
    fi

    log_info "回滚到版�? $target_version"

    # 执行回滚
    if command -v docker &> /dev/null; then
        docker tag "spharx/agentos-scripts:rollback-$target_version" "spharx/agentos-scripts:production"
        docker-compose -f "$AGENTOS_SCRIPTS_DIR/deploy/docker/docker-compose.yml" up -d
    fi

    log_success "回滚完成"
    return 0
}

###############################################################################
# 发布流程
###############################################################################
run_release() {
    local tag="${1:-}"

    if [[ -z "$tag" ]]; then
        log_error "未指定发布标�?
        return 1
    fi

    log_info "创建发布版本: $tag"

    # 1. 构建发布版本
    run_build "$tag" "release"

    # 2. 创建 GitHub Release
    if command -v gh &> /dev/null; then
        gh release create "$tag" \
            --title "AgentOS Scripts Release $tag" \
            --notes "AgentOS Scripts 模块发布版本 $tag" \
            --draft || log_warn "GitHub Release 创建失败"
    else
        log_warn "GitHub CLI 未安装，跳过 Release 创建"
    fi

    # 3. 推�?Docker 镜像
    if command -v docker &> /dev/null; then
        docker push "spharx/agentos-scripts:$tag"
        docker push "spharx/agentos-scripts:latest"
    fi

    log_success "发布版本 $tag 创建完成"
    return 0
}

###############################################################################
# 主入�?
###############################################################################
main() {
    local command="${1:-}"
    shift || true

    case "$command" in
        ci)
            run_ci "$@"
            ;;
        build)
            run_build "$@"
            ;;
        test)
            run_unit_tests "$@"
            ;;
        security)
            run_security "$@"
            ;;
        deploy)
            run_deploy "$@"
            ;;
        rollback)
            run_rollback "$@"
            ;;
        release)
            run_release "$@"
            ;;
        --version)
            echo "AgentOS CI/CD v$AGENTOS_CICD_VERSION"
            ;;
        --help|-h)
            print_usage
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
