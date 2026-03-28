﻿#!/usr/bin/env bash
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS 示例运行脚本
# 遵循 AgentOS 架构设计原则：反馈闭环、工程美�?

###############################################################################
# 严格模式
###############################################################################
set -euo pipefail

###############################################################################
# 来源依赖
###############################################################################
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIPTS_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_ROOT="$(dirname "$SCRIPTS_DIR")"

# shellcheck source=../lib/bases.sh
source "$SCRIPTS_DIR/lib/bases.sh"

###############################################################################
# 版本信息
###############################################################################
declare -r AGENTOS_VERSION="1.0.0.6"

###############################################################################
# 颜色定义
###############################################################################
COLOR_BOLD='\033[1m'
COLOR_DIM='\033[2m'
COLOR_RED='\033[0;31m'
COLOR_GREEN='\033[0;32m'
COLOR_YELLOW='\033[1;33m'
COLOR_BLUE='\033[0;34m'
COLOR_CYAN='\033[0;36m'
COLOR_NC='\033[0m'

###############################################################################
# 示例定义
###############################################################################
declare -A EXAMPLES=(
    ["hello"]="hello_world      - 基本示例：Hello World"
    ["task"]="task_creation     - 任务创建示例"
    ["memory"]="memory_demo     - 记忆系统示例"
    ["ipc"]="ipc_example        - 进程间通信示例"
    ["agent"]="multi_agent     - 多智能体协作示例"
)

###############################################################################
# 示例目录
###############################################################################
EXAMPLES_DIR="$PROJECT_ROOT/examples"

###############################################################################
# 打印函数
###############################################################################
print_banner() {
    echo -e "${COLOR_CYAN}"
    cat << "EOF"
   ____          _        __  __                                                   _
  / __ \        | |      |  \/  |                                                 | |
 | |  | |_ __   | | __ _| \  / | __ _ _ __   __ _  ___ _ __ ___   ___  _ __  __ _| |_
 | |  | | '_ \ / _` |/ _` | |\/| |/ _` | '_ \ / _` |/ _ \ '_ ` _ \ / _ \| '_ \/ _` | __|
 | |__| | |_) | (_| | (_| | |  | | (_| | | | | (_| |  __/ | | | | | (_) | | | | (_| | |_
  \____/| .__/ \__,_|\__,_|_|  |_|\__,_|_| |_|\__, |\___|_| |_| |_|\___/|_| |_|\__,_|\__|
        | |                                     __/ |
        |_|                                    |___/
EOF
    echo -e "${COLOR_NC}"
    echo -e "${COLOR_BOLD}AgentOS Examples Runner v${AGENTOS_VERSION}${COLOR_NC}"
    echo ""
}

print_usage() {
    cat << EOF
${COLOR_BOLD}用法:${COLOR_NC} $0 [选项] [示例名称]

${COLOR_BOLD}可用示例:${COLOR_NC}
EOF

    for key in "${!EXAMPLES[@]}"; do
        IFS='-' read -r id desc <<< "${EXAMPLES[$key]}"
        echo -e "    ${COLOR_GREEN}${key:<10}${COLOR_NC} ${desc}"
    done

    cat << EOF

${COLOR_BOLD}选项:${COLOR_NC}
    --list          列出所有可用示�?
    --info <name>   显示示例详细信息
    --build         构建示例
    --clean         清理示例构建
    --help, -h      显示此帮助信�?

${COLOR_BOLD}示例:${COLOR_NC}
    $0 hello                # 运行 hello_world 示例
    $0 --list               # 列出所有示�?
    $0 --info task          # 显示任务创建示例详情

EOF
}

print_section() {
    echo ""
    echo -e "${COLOR_BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${COLOR_NC}"
    echo -e "${COLOR_BLUE}�?$1${COLOR_NC}"
    echo -e "${COLOR_BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${COLOR_NC}"
    echo ""
}

print_status() {
    local status=$1
    local message="$2"
    case "$status" in
        ok)      echo -e "${COLOR_GREEN}[✓]${COLOR_NC} $message" ;;
        fail)    echo -e "${COLOR_RED}[✗]${COLOR_NC} $message" ;;
        info)    echo -e "${COLOR_BLUE}[•]${COLOR_NC} $message" ;;
        warn)    echo -e "${COLOR_YELLOW}[!]${COLOR_NC} $message" ;;
    esac
}

###############################################################################
# 示例函数
###############################################################################
list_examples() {
    print_section "可用示例"

    echo -e "${COLOR_BOLD}名称${COLOR_NC}         ${COLOR_BOLD}描述${COLOR_NC}"
    echo -e "${COLOR_DIM}${'─'::<60}${COLOR_NC}"

    for key in "${!EXAMPLES[@]}"; do
        IFS='-' read -r id desc <<< "${EXAMPLES[$key]}"
        printf "${COLOR_GREEN}%-12s${COLOR_NC} %s\n" "$key" "$desc"
    done

    echo ""
    echo -e "运行示例: ${COLOR_CYAN}$0 <示例名称>${COLOR_NC}"
}

show_example_info() {
    local example=$1

    if [[ ! -v "EXAMPLES[$example]" ]]; then
        print_status "fail" "未知的示�? $example"
        echo "运行 '${COLOR_CYAN}$0 --list${COLOR_NC}' 查看可用示例"
        return 1
    fi

    print_section "示例信息: $example"

    IFS='-' read -r id desc <<< "${EXAMPLES[$example]}"
    echo -e "${COLOR_BOLD}名称:${COLOR_NC} $example"
    echo -e "${COLOR_BOLD}ID:${COLOR_NC} $id"
    echo -e "${COLOR_BOLD}描述:${COLOR_NC} $desc"

    local example_path="$EXAMPLES_DIR/$id"
    if [[ -d "$example_path" ]]; then
        echo ""
        echo -e "${COLOR_BOLD}路径:${COLOR_NC} $example_path"

        local readme="$example_path/README.md"
        if [[ -f "$readme" ]]; then
            echo ""
            echo -e "${COLOR_BOLD}说明:${COLOR_NC}"
            head -20 "$readme" | sed 's/^/    /'
        fi
    else
        echo ""
        echo -e "${COLOR_YELLOW}示例目录不存�?{COLOR_NC}"
    fi
}

run_example() {
    local example=$1

    if [[ ! -v "EXAMPLES[$example]" ]]; then
        print_status "fail" "未知的示�? $example"
        echo "运行 '${COLOR_CYAN}$0 --list${COLOR_NC}' 查看可用示例"
        return 1
    fi

    IFS='-' read -r id desc <<< "${EXAMPLES[$example]}"
    local example_path="$EXAMPLES_DIR/$id"

    print_section "运行示例: $example"

    if [[ ! -d "$example_path" ]]; then
        print_status "fail" "示例目录不存�? $example_path"
        return 1
    fi

    local build_dir="$example_path/build"
    local binary="$build_dir/$id"

    if [[ ! -x "$binary" ]]; then
        print_status "info" "示例未构建，开始构�?.."

        mkdir -p "$build_dir"

        if [[ -f "$example_path/CMakeLists.txt" ]]; then
            cmake -S "$example_path" -B "$build_dir" || {
                print_status "fail" "CMake 配置失败"
                return 1
            }

            cmake --build "$build_dir" || {
                print_status "fail" "构建失败"
                return 1
            }
        else
            print_status "warn" "示例不需要构�?
        fi
    fi

    if [[ -x "$binary" ]]; then
        print_status "ok" "执行示例..."
        echo ""

        if [[ -f "$example_path/manager.conf" ]]; then
            AGENTOS_CONFIG="$example_path/manager.conf" "$binary"
        else
            "$binary"
        fi
    else
        print_status "warn" "示例无可执行文件"
        echo -e "${COLOR_DIM}请手动执�? cd $example_path && make${COLOR_NC}"
    fi
}

build_examples() {
    print_section "构建所有示�?

    local built=0
    local failed=0

    for key in "${!EXAMPLES[@]}"; do
        IFS='-' read -r id desc <<< "${EXAMPLES[$key]}"
        local example_path="$EXAMPLES_DIR/$id"

        if [[ -d "$example_path" ]] && [[ -f "$example_path/CMakeLists.txt" ]]; then
            echo -e "${COLOR_BOLD}构建: $id${COLOR_NC}"

            local build_dir="$example_path/build"
            mkdir -p "$build_dir"

            if cmake -S "$example_path" -B "$build_dir" && cmake --build "$build_dir"; then
                print_status "ok" "$id 构建成功"
                ((built++))
            else
                print_status "fail" "$id 构建失败"
                ((failed++))
            fi
        fi
    done

    echo ""
    print_status "info" "构建完成: $built 成功, $failed 失败"
}

clean_examples() {
    print_section "清理示例构建"

    for key in "${!EXAMPLES[@]}"; do
        IFS='-' read -r id desc <<< "${EXAMPLES[$key]}"
        local example_path="$EXAMPLES_DIR/$id"
        local build_dir="$example_path/build"

        if [[ -d "$build_dir" ]]; then
            rm -rf "$build_dir"
            print_status "ok" "清理: $id"
        fi
    done
}

###############################################################################
# 主流�?
###############################################################################
main() {
    print_banner

    if [[ $# -eq 0 ]]; then
        print_usage
        exit 0
    fi

    case "$1" in
        --list)
            list_examples
            ;;
        --info)
            if [[ -z "${2:-}" ]]; then
                print_status "fail" "--info 需要示例名�?
                exit 1
            fi
            show_example_info "$2"
            ;;
        --build)
            build_examples
            ;;
        --clean)
            clean_examples
            ;;
        --help|-h)
            print_usage
            ;;
        *)
            run_example "$1"
            ;;
    esac
}

###############################################################################
# 执行入口
###############################################################################
main "$@"