#!/bin/bash
# =============================================================================
# AgentOS 跨平台一键部署脚本 V2.0 (Unix: Linux/macOS)
# =============================================================================
#
# 功能特性:
#   ✅ 跨平台支持：Linux (Ubuntu/Debian/CentOS/Fedora/Arch) + macOS
#   ✅ 自动环境检测（操作系统、架构、资源）
#   ✅ 智能依赖安装（自动识别包管理器）
#   ✅ 交互式配置向导 + 非交互模式
#   ✅ Docker 服务管理（开发/生产环境）
#   ✅ 健康检查和状态验证
#   ✅ 日志收集和问题诊断
#   ✅ 优雅停机和资源清理
#   ✅ 可视化进度反馈
#   ✅ 完善的错误处理和回滚机制
#
# 使用方法:
#   # 交互式部署（推荐新手）
#   ./install.sh
#
#   # 一键部署开发环境
#   ./install.sh --mode dev --auto
#
#   # 一键部署生产环境
#   ./install.sh --mode prod --auto
#
#   # 仅检查环境
#   ./install.sh --check-only
#
#   # 查看帮助
#   ./install.sh --help
#
# 环境要求:
#   - Linux 5.10+ / macOS 12+
#   - Docker >= 20.10 / Docker Desktop (macOS/Windows)
#   - 内存 >= 4GB RAM (开发) / 8GB (生产)
#   - 磁盘空间 >= 20GB
#
# 作者: AgentOS DevOps Team
# 版本: 2.0.0 (2026-04-08)
# 兼容: Linux (Ubuntu/Debian/CentOS/Fedora/Arch/Alpine), macOS 12+
# =============================================================================

set -euo pipefail

# =============================================================================
# 全局常量和变量
# =============================================================================

readonly SCRIPT_VERSION="2.0.0"
readonly SCRIPT_NAME="AgentOS Cross-Platform Installer"
readonly MIN_DOCKER_VERSION="20.10"
MIN_PYTHON_VERSION="3.8"
MIN_GIT_VERSION="2.30"
MIN_MEMORY_GB_DEV=4
MIN_MEMORY_GB_PROD=8
MIN_DISK_GB=20

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
DOCKER_COMPOSE_FILE="${PROJECT_ROOT}/docker/docker-compose.yml"
DOCKER_COMPOSE_PROD="${PROJECT_ROOT}/docker/docker-compose.prod.yml"
ENV_FILE=".env.production"
LOG_DIR="${PROJECT_ROOT}/logs"
CONFIG_DIR="${PROJECT_ROOT}/config"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# 状态追踪
TOTAL_STEPS=0
CURRENT_STEP=0
ERROR_COUNT=0
WARNING_COUNT=0
ROLLBACK_NEEDED=false

# =============================================================================
# 颜色定义（跨平台兼容）
# =============================================================================

if [[ -t 1 ]] && [[ "${TERM:-dumb}" != "dumb" ]]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    BLUE='\033[0;34m'
    CYAN='\033[0;36m'
    MAGENTA='\033[0;35m'
    BOLD='\033[1m'
    NC='\033[0m'
else
    RED=''
    GREEN=''
    YELLOW=''
    BLUE=''
    CYAN=''
    MAGENTA=''
    BOLD=''
    NC=''
fi

# =============================================================================
# 加载现有库文件
# =============================================================================

load_libraries() {
    local lib_dir="${SCRIPT_DIR}/lib"

    if [[ -f "${lib_dir}/platform.sh" ]]; then
        source "${lib_dir}/platform.sh"
    fi

    if [[ -f "${lib_dir}/common.sh" ]]; then
        source "${lib_dir}/common.sh"
    fi

    if [[ -f "${lib_dir}/log.sh" ]]; then
        source "${lib_dir}/log.sh"
    fi

    if [[ -f "${lib_dir}/error.sh" ]]; then
        source "${lib_dir}/error.sh"
    fi
}

load_libraries

# =============================================================================
# 进度显示系统
# =============================================================================

init_progress() {
    TOTAL_STEPS="$1"
    CURRENT_STEP=0
    ERROR_COUNT=0
    WARNING_COUNT=0
    echo ""
    echo -e "${CYAN}═══════════════════════════════════════════════════════════${NC}"
    echo -e "${BOLD}${BLUE}  🚀 ${SCRIPT_NAME} V${SCRIPT_VERSION}${NC}"
    echo -e "${CYAN}═══════════════════════════════════════════════════════════${NC}"
    echo ""
}

show_progress() {
    local step_name="$1"
    ((CURRENT_STEP++))

    local percentage=$((CURRENT_STEP * 100 / TOTAL_STEPS))
    local filled=$((percentage / 5))
    local empty=$((20 - filled))

    local bar=""
    for ((i=0; i<filled; i++)); do bar+="█"; done
    for ((i=0; i<empty; i++)); do bar+="░"; done

    echo -e "\n${BLUE}▶ [${CURRENT_STEP}/${TOTAL_STEPS}]${NC} ${BOLD}${step_name}${NC}"
    echo -e "${CYAN}   进度: [${bar}] ${percentage}%${NC}"
}

complete_progress() {
    echo ""
    echo -e "${CYAN}═══════════════════════════════════════════════════════════${NC}"

    if [[ $ERROR_COUNT -eq 0 ]]; then
        echo -e "${GREEN}  ✅ 所有步骤完成！未发现错误${NC}"
        if [[ $WARNING_COUNT -gt 0 ]]; then
            echo -e "${YELLOW}  ⚠️  发现 ${WARNING_COUNT} 个警告${NC}"
        fi
    else
        echo -e "${RED}  ❌ 完成，但发现 ${ERROR_COUNT} 个错误${NC}"
    fi

    echo -e "${CYAN}═══════════════════════════════════════════════════════════${NC}"
    echo ""
}

# =============================================================================
# 日志函数
# =============================================================================

log_info() {
    echo -e "  ${GREEN}[✓]${NC} $1"
}

log_warn() {
    echo -e "  ${YELLOW}[!]${NC} $1"
    ((WARNING_COUNT++)) || true
}

log_error() {
    echo -e "  ${RED}[✗]${NC} $1"
    ((ERROR_COUNT++)) || true
    ROLLBACK_NEEDED=true
}

log_step() {
    echo -e "\n  ${MAGENTA}━━━ $1 ━━━${NC}"
}

# =============================================================================
# 平台检测增强版
# =============================================================================

detect_platform() {
    log_step "检测操作系统平台"

    local os_name
    local os_version
    local arch

    # 检测操作系统
    if type agentos_platform_detect &>/dev/null; then
        OS_PLATFORM=$(agentos_platform_detect)
    else
        case "$(uname -s)" in
            Linux*)  OS_PLATFORM="linux" ;;
            Darwin*) OS_PLATFORM="macos" ;;
            MINGW*|MSYS*|CYGWIN*) OS_PLATFORM="windows" ;;
            *)       OS_PLATFORM="unknown" ;;
        esac
    fi

    # 检测架构
    if type agentos_arch_detect &>/dev/null; then
        OS_ARCH=$(agentos_arch_detect)
    else
        OS_ARCH=$(uname -m)
    fi

    # 获取详细版本信息
    case "$OS_PLATFORM" in
        linux)
            if [[ -f /etc/os-release ]]; then
                os_name=$(grep "^PRETTY_NAME=" /etc/os-release | cut -d'"' -f2)
                os_version=$(grep "^VERSION_ID=" /etc/os-release | cut -d'"' -f2)
            elif command -v lsb_release &>/dev/null; then
                os_name=$(lsb_release -ds 2>/dev/null || echo "Unknown Linux")
                os_version=$(lsb_release -rs 2>/dev/null || echo "")
            else
                os_name="Linux"
                os_version=$(uname -r)
            fi
            ;;
        macos)
            os_name="macOS"
            os_version=$(sw_vers -productVersion 2>/dev/null || echo "Unknown")
            ;;
        *)
            os_name=$(uname -s)
            os_version=$(uname -r)
            ;;
    esac

    log_info "操作系统: ${os_name} ${os_version}"
    log_info "系统架构: ${OS_ARCH}"
    log_info "平台类型: ${OS_PLATFORM}"

    # 平台兼容性检查
    check_platform_compatibility
}

check_platform_compatibility() {
    case "$OS_PLATFORM" in
        linux)
            local kernel_version
            kernel_version=$(uname -r | cut -d. -f1-2)
            if ! agentos_version_check "5.10" "$kernel_version" 2>/dev/null; then
                log_warn "Linux 内核版本 ${kernel_version} < 5.10，可能存在兼容性问题"
            fi
            ;;
        macos)
            local macos_major
            macos_major=$(echo "$os_version" | cut -d. -f1)
            if [[ "$macos_major" -lt 12 ]]; then
                log_error "macOS 版本 ${os_version} 不满足最低要求 (>= 12)"
                return 1
            fi
            ;;
        windows)
            log_warn "Windows 环境建议使用 install.ps1 脚本"
            log_info "当前运行在 WSL/MSYS/Git Bash 环境"
            ;;
        *)
            log_error "不支持的操作系统: $(uname -s)"
            return 1
            ;;
    esac

    return 0
}

# =============================================================================
# 资源检测
# =============================================================================

check_system_resources() {
    log_step "检查系统资源"

    local mode="${1:-dev}"
    local required_memory_gb=$MIN_MEMORY_GB_DEV

    if [[ "$mode" == "prod" ]]; then
        required_memory_gb=$MIN_MEMORY_GB_PROD
    fi

    # 内存检查
    local total_memory_gb
    if type agentos_total_memory &>/dev/null; then
        total_memory_gb=$(agentos_total_memory | grep -o '[0-9]*' | head -1)
    elif [[ "$OS_PLATFORM" == "macos" ]]; then
        total_memory_gb=$(sysctl -n hw.memsize 2>/dev/null | awk '{printf "%.0f", $1/1024/1024/1024}')
    else
        total_memory_gb=$(awk '/MemTotal/ {printf "%.0f", $2/1024/1024}' /proc/meminfo 2>/dev/null || echo "0")
    fi

    if [[ "$total_memory_gb" -lt "$required_memory_gb" ]]; then
        log_error "内存不足: 需要 ≥${required_memory_gb}GB, 当前: ~${total_memory_gb}GB"
        log_warn "生产环境建议使用 16GB+ 内存以获得最佳性能"
    else
        log_info "内存: ~${total_memory_gb}GB (要求: ≥${required_memory_gb}GB) ✓"
    fi

    # CPU 核心数检查
    local cpu_cores
    if type agentos_cpu_count &>/dev/null; then
        cpu_cores=$(agentos_cpu_count)
    elif [[ "$OS_PLATFORM" == "macos" ]]; then
        cpu_cores=$(sysctl -n hw.ncpu 2>/dev/null || echo "1")
    else
        cpu_cores=$(nproc 2>/dev/null || grep -c ^processor /proc/cpuinfo 2>/dev/null || echo "1")
    fi

    if [[ "$cpu_cores" -lt 4 ]]; then
        log_warn "CPU 核心数较少: ${cpu_cores} 核 (推荐 4+ 核)"
    else
        log_info "CPU: ${cpu_cores} 核 ✓"
    fi

    # 磁盘空间检查
    local available_disk_gb
    if [[ "$OS_PLATFORM" == "macos" ]]; then
        available_disk_gb=$(df -g "${PROJECT_ROOT}" 2>/dev/null | awk 'NR==2{print $4}' || echo "0")
    else
        available_disk_gb=$(df -BG "${PROJECT_ROOT}" 2>/dev/null | awk 'NR==2{print $4}' | tr -d 'G' || echo "0")
    fi

    if [[ "$available_disk_gb" -lt "$MIN_DISK_GB" ]]; then
        log_error "磁盘空间不足: 需要 ≥${MIN_DISK_GB}GB, 当前: ~${available_disk_gb}GB"
    else
        log_info "磁盘可用空间: ~${available_disk_gb}GB (要求: ≥${MIN_DISK_GB}GB) ✓"
    fi

    # 磁盘 I/O 性能提示（可选）
    if [[ "$OS_PLATFORM" == "linux" ]] && [[ -f /sys/block/sda/queue/scheduler ]]; then
        local scheduler
        scheduler=$(cat /sys/block/sda/queue/scheduler 2>/dev/null | grep -o '\[.*\]' | tr -d '[]')
        if [[ "$scheduler" != "none" && "$scheduler" != "noop" ]]; then
            log_info "磁盘 I/O 调度器: ${scheduler} (SSD 建议使用 none/noop)"
        fi
    fi
}

# =============================================================================
# 依赖检测与安装
# =============================================================================

detect_package_manager() {
    log_step "检测包管理器"

    if type agentos_package_manager_detect &>/dev/null; then
        PACKAGE_MANAGER=$(agentos_package_manager_detect)
    else
        if command -v apt-get &>/dev/null; then
            PACKAGE_MANAGER="apt"
        elif command -v yum &>/dev/null; then
            PACKAGE_MANAGER="yum"
        elif command -v dnf &>/dev/null; then
            PACKAGE_MANAGER="dnf"
        elif command -v apk &>/dev/null; then
            PACKAGE_MANAGER="apk"
        elif command -v brew &>/dev/null; then
            PACKAGE_MANAGER="brew"
        elif command -v pacman &>/dev/null; then
            PACKAGE_MANAGER="pacman"
        else
            PACKAGE_MANAGER="unknown"
        fi
    fi

    log_info "包管理器: ${PACKAGE_MANAGER}"
}

check_core_dependencies() {
    log_step "检查核心依赖"

    local missing_deps=()

    # Docker
    if command -v docker &>/dev/null; then
        DOCKER_VERSION=$(docker --version 2>/dev/null | awk '{print $3}' | tr -d ',')
        log_info "Docker: v${DOCKER_VERSION}"

        if ! docker info &>/dev/null 2>&1; then
            log_error "Docker 未运行，请先启动 Docker Desktop 或 Docker 服务"
            show_docker_start_instructions
            return 1
        fi
        log_info "Docker 服务: 运行中 ✓"
    else
        missing_deps+=("docker")
        log_error "未安装 Docker"
        show_docker_install_instructions
    fi

    # Docker Compose
    if docker compose version &>/dev/null 2>&1; then
        COMPOSE_VERSION=$(docker compose version 2>/dev/null | awk '{print $4}' | tr -d ',')
        log_info "Docker Compose (Plugin): v${COMPOSE_VERSION} ✓"
    elif command -v docker-compose &>/dev/null; then
        COMPOSE_VERSION=$(docker-compose --version 2>/dev/null | awk '{print $3}' | tr -d ',')
        log_info "Docker Compose (Standalone): v${COMPOSE_VERSION} ✓"
    else
        missing_deps+=("docker-compose")
        log_error "未安装 Docker Compose"
    fi

    # Git
    if command -v git &>/dev/null; then
        GIT_VERSION=$(git --version 2>/dev/null | awk '{print $3}')
        log_info "Git: v${GIT_VERSION} ✓"
    else
        missing_deps+=("git")
        log_warn "未安装 Git（非必需但推荐用于版本管理）"
    fi

    # Python（可选）
    if command -v python3 &>/dev/null; then
        PYTHON_VERSION=$(python3 --version 2>/dev/null | awk '{print $2}')
        log_info "Python: v${PYTHON_VERSION} ✓"
    elif command -v python &>/dev/null; then
        PYTHON_VERSION=$(python --version 2>/dev/null | awk '{print $2}')
        log_info "Python: v${PYTHON_VERSION} ✓"
    else
        log_info "Python 未安装（SDK 和高级功能需要）"
    fi

    # curl/wget（至少需要一个）
    if command -v curl &>/dev/null || command -v wget &>/dev/null; then
        log_info "网络工具: ✓"
    else
        missing_deps+=("curl或wget")
        log_error "需要安装 curl 或 wget 用于下载依赖"
    fi

    if [[ ${#missing_deps[@]} -gt 0 ]]; then
        log_error "缺少核心依赖: ${missing_deps[*]}"
        return 1
    fi

    return 0
}

show_docker_install_instructions() {
    echo ""
    case "$OS_PLATFORM" in
        linux)
            case "$PACKAGE_MANAGER" in
                apt)
                    echo -e "  ${YELLOW}安装命令:${NC}"
                    echo "    curl -fsSL https://get.docker.com | sh"
                    echo "    sudo usermod -aG docker \$USER"
                    echo "    # 注销并重新登录使组权限生效"
                    ;;
                yum|dnf)
                    echo -e "  ${YELLOW}安装命令:${NC}"
                    echo "    sudo ${PACKAGE_MANAGER} install -y docker docker-compose-plugin"
                    echo "    sudo systemctl enable --now docker"
                    echo "    sudo usermod -aG docker \$USER"
                    ;;
                apk)
                    echo -e "  ${YELLOW}安装命令:${NC}"
                    echo "    sudo apk add docker docker-compose"
                    echo "    rc-update add docker default"
                    echo "    service docker start"
                    ;;
                pacman)
                    echo -e "  ${YELLOW}安装命令:${NC}"
                    echo "    sudo pacman -S docker docker-compose"
                    echo "    sudo systemctl enable --now docker"
                    echo "    sudo usermod -aG docker \$USER"
                    ;;
                brew)
                    echo -e "  ${YELLOW}安装命令:${NC}"
                    echo "    brew install docker docker-compose"
                    echo "    # 启动 Docker Desktop 应用程序"
                    ;;
                *)
                    echo -e "  ${YELLOW}访问:${NC} https://docs.docker.com/get-docker/"
                    ;;
            esac
            ;;
        macos)
            echo -e "  ${YELLOW}安装方法:${NC}"
            echo "    1. 访问 https://www.docker.com/products/docker-desktop/"
            echo "    2. 下载并安装 Docker Desktop for Mac"
            echo "    3. 启动 Docker Desktop 应用程序"
            ;;
        *)
            echo -e "  ${YELLOW}访问:${NC} https://docs.docker.com/get-docker/"
            ;;
    esac
    echo ""
}

show_docker_start_instructions() {
    echo ""
    case "$OS_PLATFORM" in
        linux)
            echo -e "  ${YELLOW}启动命令:${NC}"
            echo "    sudo systemctl start docker          # Systemd 系统"
            echo "    # 或"
            echo "    sudo service docker start           # SysVinit 系统"
            ;;
        macos)
            echo -e "  ${YELLOW}启动方法:${NC}"
            echo "    打开 Docker Desktop 应用程序"
            ;;
        *)
            echo -e "  请手动启动 Docker 服务${NC}"
            ;;
    esac
    echo ""
}

install_dependencies_auto() {
    log_step "自动安装缺失依赖"

    if [[ "$AUTO_MODE" != "true" ]]; then
        if ! confirm_action "是否自动安装缺失的依赖？"; then
            return 0
        fi
    fi

    case "$PACKAGE_MANAGER" in
        apt)
            sudo apt-get update
            sudo apt-get install -y curl wget git python3 python3-pip
            ;;
        yum)
            sudo yum install -y curl wget git python3 python3-pip
            ;;
        dnf)
            sudo dnf install -y curl wget git python3 python3-pip
            ;;
        apk)
            sudo apk add curl wget git python3 py3-pip
            ;;
        brew)
            brew install curl wget git python3
            ;;
        pacman)
            sudo pacman -S --noconfirm curl wget git python3 python-pip
            ;;
        *)
            log_warn "无法自动安装依赖，请手动安装"
            return 1
            ;;
    esac

    log_info "依赖安装完成"
    return 0
}

# =============================================================================
# 项目结构验证
# =============================================================================

validate_project_structure() {
    log_step "验证项目结构"

    local required_files=(
        "docker/Dockerfile.kernel"
        "docker/Dockerfile.daemon"
        "docker/docker-compose.yml"
        ".env.example"
    )

    local optional_files=(
        "docker/docker-compose.prod.yml"
        ".env.production.example"
        "config/agentos.yaml.example"
    )

    local missing_required=()
    local missing_optional=()

    for file in "${required_files[@]}"; do
        if [[ -f "${PROJECT_ROOT}/${file}" ]]; then
            log_info "必需文件: ${file} ✓"
        else
            missing_required+=("${file}")
            log_error "缺失必需文件: ${file}"
        fi
    done

    for file in "${optional_files[@]}"; do
        if [[ -f "${PROJECT_ROOT}/${file}" ]]; then
            log_info "可选文件: ${file} ✓"
        else
            missing_optional+=("${file}")
            log_warn "缺失可选文件: ${file}"
        fi
    done

    if [[ ${#missing_required[@]} -gt 0 ]]; then
        log_error "项目结构不完整，缺少 ${#missing_required[@]} 个必需文件"
        return 1
    fi

    return 0
}

# =============================================================================
# 配置管理
# =============================================================================

setup_environment_config() {
    log_step "生成环境配置"

    mkdir -p "${LOG_DIR}"
    mkdir -p "${CONFIG_DIR}"

    # 复制 .env 文件
    if [[ ! -f "${PROJECT_ROOT}/${ENV_FILE}" ]]; then
        if [[ -f "${PROJECT_ROOT}/.env.production.example" ]]; then
            cp "${PROJECT_ROOT}/.env.production.example" "${PROJECT_ROOT}/${ENV_FILE}"
            log_info "已创建 ${ENV_FILE}（从模板）"
        elif [[ -f "${PROJECT_ROOT}/.env.example" ]]; then
            cp "${PROJECT_ROOT}/.env.example" "${PROJECT_ROOT}/${ENV_FILE}"
            log_info "已创建 ${ENV_FILE}（从 .env.example）"
        else
            generate_env_file
        fi
    else
        log_info "${ENV_FILE} 已存在"
    fi

    # 生成 agentos.yaml 配置
    if [[ ! -f "${CONFIG_DIR}/agentos.yaml" ]] && [[ -f "${CONFIG_DIR}/agentos.yaml.example" ]]; then
        cp "${CONFIG_DIR}/agentos.yaml.example" "${CONFIG_DIR}/agentos.yaml"
        log_info "已创建 config/agentos.yaml（从模板）"
    fi

    # 显示安全配置提醒
    show_security_reminder
}

generate_env_file() {
    cat > "${PROJECT_ROOT}/${ENV_FILE}" << EOF
# AgentOS Environment Configuration
# Generated by install.sh on $(date)

# ===========================================
# 数据库配置
# ===========================================
POSTGRES_HOST=postgres
POSTGRES_PORT=5432
POSTGRES_DB=agentos
POSTGRES_USER=agentos
POSTGRES_PASSWORD=$(generate_random_password 16)

# ===========================================
# Redis 配置
# ===========================================
REDIS_HOST=redis
REDIS_PORT=6379
REDIS_PASSWORD=$(generate_random_password 16)

# ===========================================
# JWT 安全配置
# ===========================================
JWT_SECRET_KEY=$(generate_random_password 32)
JWT_EXPIRATION_HOURS=24

# ===========================================
# API 密钥（请替换为实际密钥）
# ===========================================
API_KEYS=default-key-change-me

# ===========================================
# Gateway 配置
# ===========================================
GATEWAY_HOST=0.0.0.0
GATEWAY_PORT=18789

# ===========================================
# 日志级别
# ===========================================
LOG_LEVEL=INFO

# ===========================================
# 时区配置
# ===========================================
TZ=Asia/Shanghai
EOF

    log_info "已生成默认 ${ENV_FILE}"
}

generate_random_password() {
    local length="${1:-16}"
    if command -v openssl &>/dev/null; then
        openssl rand -base64 "$length" | tr -d '=+/' | head -c "$length"
    else
        LC_ALL=C tr -dc 'A-Za-z0-9' < /dev/urandom | head -c "$length"
    fi
}

show_security_reminder() {
    echo ""
    echo -e "${YELLOW}  ⚠️  安全提醒:${NC}"
    echo "  请务必修改以下敏感配置:"
    echo "    • ${ENV_FILE}:"
    echo "      - POSTGRES_PASSWORD"
    echo "      - REDIS_PASSWORD"
    echo "      - JWT_SECRET_KEY"
    echo "      - API_KEYS"
    echo ""
    echo "  编辑命令:"
    if [[ "$OS_PLATFORM" == "macos" ]]; then
        echo "    nano ${PROJECT_ROOT}/${ENV_FILE}"
        echo "    # 或使用: open -e ${PROJECT_ROOT}/${ENV_FILE}"
    else
        echo "    nano ${PROJECT_ROOT}/${ENV_FILE}"
        echo "    # 或使用: vim ${PROJECT_ROOT}/${ENV_FILE}"
    fi
    echo ""
}

# =============================================================================
# Docker 服务管理
# =============================================================================

start_dev_environment() {
    log_step "启动开发环境"

    cd "${PROJECT_ROOT}/docker"

    log_info "使用配置: docker-compose.yml"
    log_info "启动服务: postgres, redis, gateway, kernel, openlab"

    if docker compose version &>/dev/null 2>&1; then
        docker compose up -d postgres redis gateway kernel openlab
    else
        docker-compose up -d postgres redis gateway kernel openlab
    fi

    if [[ $? -eq 0 ]]; then
        echo ""
        log_info "✅ 开发环境启动成功！"
        echo ""
        show_service_access_info "dev"
        run_health_check
    else
        log_error "开发环境启动失败！"
        collect_error_logs
        return 1
    fi
}

start_prod_environment() {
    log_step "启动生产环境"

    cd "${PROJECT_ROOT}/docker"

    if [[ ! -f "../${ENV_FILE}" ]]; then
        log_error "缺少生产环境配置文件: ${ENV_FILE}"
        return 1
    fi

    log_info "使用配置: docker-compose.prod.yml"
    log_info "环境变量: ../${ENV_FILE}"

    if docker compose version &>/dev/null 2>&1; then
        docker compose --env-file "../${ENV_FILE}" -f docker-compose.prod.yml up -d
    else
        docker-compose --env-file "../${ENV_FILE}" -f docker-compose.prod.yml up -d
    fi

    if [[ $? -eq 0 ]]; then
        echo ""
        log_info "✅ 生产环境启动成功！"
        echo ""
        show_service_access_info "prod"
        run_health_check
    else
        log_error "生产环境启动失败！"
        collect_error_logs
        return 1
    fi
}

stop_environment() {
    log_step "停止所有服务"

    cd "${PROJECT_ROOT}/docker"

    local stopped=false

    if docker compose version &>/dev/null 2>&1; then
        if docker compose ps &>/dev/null 2>&1; then
            docker compose down
            log_info "开发环境已停止 ✓"
            stopped=true
        fi

        if [[ -f docker-compose.prod.yml ]] && docker compose -f docker-compose.prod.yml ps &>/dev/null 2>&1; then
            docker compose -f docker-compose.prod.yml down
            log_info "生产环境已停止 ✓"
            stopped=true
        fi
    else
        if docker-compose ps &>/dev/null 2>&1; then
            docker-compose down
            log_info "开发环境已停止 ✓"
            stopped=true
        fi

        if [[ -f docker-compose.prod.yml ]] && docker-compose -f docker-compose.prod.yml ps &>/dev/null 2>&1; then
            docker-compose -f docker-compose.prod.yml down
            log_info "生产环境已停止 ✓"
            stopped=true
        fi
    fi

    if $stopped; then
        echo ""
        log_info "所有服务已停止"
    else
        log_info "没有正在运行的服务"
    fi
}

show_service_access_info() {
    local mode="$1"

    echo -e "${BOLD}  📋 服务访问地址:${NC}"
    echo ""

    if [[ "$mode" == "dev" ]]; then
        echo "  🌐 Gateway API:     http://localhost:18789"
        echo "  🔧 Kernel IPC:      http://localhost:18080"
        echo "  🧪 OpenLab UI:      http://localhost:3000"
        echo "  📊 Grafana 监控:     http://localhost:3001 (需启用 monitoring profile)"
        echo "  📈 Prometheus:      http://localhost:9091 (需启用 monitoring profile)"
    else
        echo "  📡 Gateway:         :18789 (HTTPS 通过反向代理)"
        echo "  🐳 容器数:          $(docker ps --format '{{.Names}}' 2>/dev/null | grep -c agentos || echo 0)"
    fi

    echo ""
    echo -e "${BOLD}  🔧 常用运维命令:${NC}"
    echo ""

    if [[ "$mode" == "dev" ]]; then
        echo "  查看日志:   docker compose logs -f"
        echo "  重启服务:   docker compose restart"
        echo "  进入容器:   docker compose exec kernel bash"
        echo "  停止服务:   ./install.sh --stop"
    else
        echo "  查看状态:   docker compose -f docker-compose.prod.yml ps"
        echo "  查看日志:   docker compose -f docker-compose.prod.yml logs -f"
        echo "  扩展实例:   docker compose -f docker-compose.prod.yml up -d --scale kernel=3"
        echo "  优雅停止:   docker compose -f docker-compose.prod.yml down --timeout 60"
    fi

    echo ""
}

# =============================================================================
# 健康检查
# =============================================================================

run_health_check() {
    log_step "执行健康检查"

    log_info "等待服务启动 (5秒)..."
    sleep 5

    local services=(
        "gateway:18789:http"
        "postgres:5432:tcp"
        "redis:6379:tcp"
    )

    local healthy_count=0
    local total_count=${#services[@]}

    for service in "${services[@]}"; do
        IFS=':' read -r name port type <<< "$service"

        if [[ "$type" == "http" ]]; then
            if curl -sf --max-time 3 "http://localhost:${port}/api/v1/health" &>/dev/null || \
               curl -sf --max-time 3 "http://localhost:${port}/health" &>/dev/null; then
                log_info "${name}:${port} 健康 ✓"
                ((healthy_count++))
            else
                log_warn "${name}:${port} 未响应（可能仍在启动中）"
            fi
        else
            if command -v nc &>/dev/null && nc -z localhost "${port}" 2>/dev/null; then
                log_info "${name}:${port} 健康 ✓"
                ((healthy_count++))
            elif command -v bash &>/dev/null && echo >/dev/tcp/localhost/${port} 2>/dev/null; then
                log_info "${name}:${port} 健康 ✓"
                ((healthy_count++))
            else
                log_warn "${name}:${port} 未响应（可能仍在启动中）"
            fi
        fi
    done

    echo ""
    local health_percentage=$((healthy_count * 100 / total_count))

    if [[ $healthy_count -eq $total_count ]]; then
        log_info "🎉 所有服务健康检查通过 (${health_percentage}%)"
        return 0
    elif [[ $healthy_count -gt 0 ]]; then
        log_warn "部分服务就绪 (${healthy_count}/${total_count}, ${health_percentage}%)"
        log_info "部分服务可能需要更长时间启动，请稍后使用 --status 查看"
        return 0
    else
        log_error "所有服务未就绪，请查看日志排查问题"
        return 1
    fi
}

# =============================================================================
# 状态显示
# =============================================================================

show_status() {
    log_step "显示系统状态"

    cd "${PROJECT_ROOT}/docker"

    echo ""
    echo -e "${BOLD}  🐳 容器状态:${NC}"
    echo ""

    if docker compose version &>/dev/null 2>&1; then
        docker compose ps 2>/dev/null || log_info "无开发环境容器"
    else
        docker-compose ps 2>/dev/null || log_info "无开发环境容器"
    fi

    if [[ -f docker-compose.prod.yml ]]; then
        echo ""
        if docker compose version &>/dev/null 2>&1; then
            docker compose -f docker-compose.prod.yml ps 2>/dev/null || log_info "无生产环境容器"
        else
            docker-compose -f docker-compose.prod.yml ps 2>/dev/null || log_info "无生产环境容器"
        fi
    fi

    echo ""
    echo -e "${BOLD}  📊 资源使用:${NC}"
    echo ""

    docker stats --no-stream \
        --format "table {{.Name}}\t{{.Status}}\t{{.CPUPerc}}\t{{.MemUsage}}\t{{.NetIO}}" \
        2>/dev/null || log_info "Docker 统计信息不可用"

    echo ""
    echo -e "${BOLD}  📝 最近日志 (最后15行):${NC}"
    echo ""

    if docker compose version &>/dev/null 2>&1; then
        docker compose logs --tail=15 2>/dev/null | tail -15 || true
    else
        docker-compose logs --tail=15 2>/dev/null | tail -15 || true
    fi
}

# =============================================================================
# 日志收集
# =============================================================================

collect_logs() {
    log_step "收集系统日志"

    local log_file="${LOG_DIR}/agentos_deploy_${TIMESTAMP}.log"

    mkdir -p "${LOG_DIR}"

    cd "${PROJECT_ROOT}/docker"

    {
        echo "=========================================="
        echo " AgentOS Deployment Log Collection"
        echo " Timestamp: $(date '+%Y-%m-%d %H:%M:%S %Z')"
        echo " Platform: $(uname -s) $(uname -r) ($(uname -m))"
        echo "=========================================="
        echo ""

        echo "--- System Information ---"
        uname -a
        echo ""

        if command -v docker &>/dev/null; then
            echo "--- Docker Version ---"
            docker --version
            docker compose version 2>/dev/null || docker-compose --version
            echo ""
        fi

        echo "--- Container Logs (Dev) ---"
        if docker compose version &>/dev/null 2>&1; then
            docker compose logs --timestamps 2>/dev/null || true
        else
            docker-compose logs --timestamps 2>/dev/null || true
        fi
        echo ""

        if [[ -f docker-compose.prod.yml ]]; then
            echo "--- Container Logs (Prod) ---"
            if docker compose version &>/dev/null 2>&1; then
                docker compose -f docker-compose.prod.yml logs --timestamps 2>/dev/null || true
            else
                docker-compose -f docker-compose.prod.yml logs --timestamps 2>/dev/null || true
            fi
            echo ""
        fi

        echo "--- Resource Usage ---"
        docker system df 2>/dev/null || true
        echo ""

        echo "--- Disk Usage ---"
        du -sh "${PROJECT_ROOT}"/* 2>/dev/null | head -20 || true
        echo ""

    } > "${log_file}" 2>&1

    log_info "日志已保存到: ${log_file}"
    log_info "文件大小: $(du -h "${log_file}" 2>/dev/null | cut -f1)"

    echo ""
    echo -e "${BOLD}  📄 日志预览 (最后30行):${NC}"
    echo ""
    tail -30 "${log_file}"
}

collect_error_logs() {
    log_step "收集错误日志用于诊断"

    local error_log="${LOG_DIR}/agentos_error_${TIMESTAMP}.log"

    mkdir -p "${LOG_DIR}"

    cd "${PROJECT_ROOT}/docker"

    {
        echo "=========================================="
        echo " AgentOS Error Log - $(date)"
        echo "=========================================="
        echo ""

        echo "--- Failed Containers ---"
        docker ps -a --filter "state=exited" --filter "state=dead" --format "{{.Names}}\t{{.Status}}" 2>/dev/null || true
        echo ""

        echo "--- Recent Error Logs ---"
        if docker compose version &>/dev/null 2>&1; then
            docker compose logs --tail=50 2>&1 | grep -iE "(error|fail|exception|fatal)" || true
        else
            docker-compose logs --tail=50 2>&1 | grep -iE "(error|fail|exception|fatal)" || true
        fi
        echo ""

        echo "--- System Resources ---"
        free -h 2>/dev/null || vm_stat 2>/dev/null || true
        df -h "${PROJECT_ROOT}" 2>/dev/null || true
        echo ""

    } > "${error_log}" 2>&1

    log_error "错误日志已保存到: ${error_log}"
}

# =============================================================================
# 清理功能
# =============================================================================

cleanup_resources() {
    log_step "清理 Docker 资源"

    if ! confirm_action "确定要清理所有 Docker 资源吗？这将删除容器、镜像和网络卷"; then
        log_info "取消清理操作"
        return 0
    fi

    cd "${PROJECT_ROOT}/docker"

    log_info "停止并删除容器..."

    if docker compose version &>/dev/null 2>&1; then
        docker compose down -v --remove-orphans 2>/dev/null || true

        if [[ -f docker-compose.prod.yml ]]; then
            docker compose -f docker-compose.prod.yml down -v --remove-orphans 2>/dev/null || true
        fi
    else
        docker-compose down -v --remove-orphans 2>/dev/null || true

        if [[ -f docker-compose.prod.yml ]]; then
            docker-compose -f docker-compose.prod.yml down -v --remove-orphans 2>/dev/null || true
        fi
    fi

    log_info "清理悬空镜像..."
    docker image prune -f 2>/dev/null || true

    log_info "清理构建缓存..."
    docker builder prune -f 2>/dev/null || true

    log_info "清理未使用的网络..."
    docker network prune -f 2>/dev/null || true

    echo ""
    log_info "✅ 清理完成"

    echo ""
    echo -e "${BOLD}  释放的空间:${NC}"
    docker system df 2>/dev/null || true
}

# =============================================================================
# 用户交互
# =============================================================================

confirm_action() {
    local prompt="${1:-Are you sure?}"

    if [[ "${AUTO_MODE:-false}" == "true" ]]; then
        return 0
    fi

    read -rp "  ${prompt} [y/N]: " response
    case "$response" in
        [yY][eE][sS]|[yY])
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

select_deployment_mode() {
    if [[ "${AUTO_MODE:-false}" == "true" ]]; then
        echo "${DEPLOY_MODE:-dev}"
        return 0
    fi

    echo ""
    echo -e "${BOLD}  请选择部署模式:${NC}"
    echo "    1) 开发环境 (Development) - 适合本地开发和调试"
    echo "    2) 生产环境 (Production) - 适合服务器部署"
    echo ""

    local choice
    read -rp "  选择 [1-2]: " choice

    case "$choice" in
        1)
            echo "dev"
            ;;
        2)
            echo "prod"
            ;;
        *)
            echo "dev"
            ;;
    esac
}

# =============================================================================
# 帮助信息
# =============================================================================

show_help() {
    cat << EOF
${BOLD}${SCRIPT_NAME} V${SCRIPT_VERSION}${NC}

${CYAN}用法:${NC}
  $0 [选项]

${CYAN}选项:${NC}
  --mode MODE        部署模式: dev|prod (默认: dev)
  --target TARGET    部署目标: backend|client|all (默认: all)
  --auto             非交互模式（使用默认配置）
  --check-only       仅检查环境和项目结构
  --status           显示当前运行状态
  --stop             停止所有服务
  --restart          重启所有服务
  --logs             收集并显示日志
  --cleanup          清理 Docker 资源
  --health           执行健康检查
  -h, --help         显示此帮助信息
  -v, --version      显示版本信息

${CYAN}示例:${NC}
  $0                          # 交互式部署（开发环境）
  $0 --mode dev --auto        # 一键部署开发环境
  $0 --mode prod --auto       # 一键部署生产环境
  $0 --check-only             # 仅检查环境
  $0 --status                 # 查看运行状态
  $0 --stop                   # 停止服务
  $0 --logs                   # 收集日志
  $0 --cleanup                # 清理资源
  $0 --health                 # 健康检查

${CYAN}支持的平台:${NC}
  • Linux: Ubuntu/Debian/CentOS/Fedora/Arch/Alpine (内核 >= 5.10)
  • macOS: Monterey (12+) 及更高版本
  • Windows: WSL2 / Git Bash / MSYS2 (推荐使用 install.ps1)

${CYAN}更多信息:${NC}
  项目文档:   ${PROJECT_ROOT}/docs/
  部署指南:   ${PROJECT_ROOT}/agentos/manuals/guides/deployment.md
  架构原则:   ${PROJECT_ROOT}/agentos/manuals/ARCHITECTURAL_PRINCIPLES.md
  问题反馈:   https://github.com/SpharxTeam/AgentOS/issues
  在线文档:   https://docs.agentos.io

${CYAN}环境要求:${NC}
  • Docker >= ${MIN_DOCKER_VERSION}
  • 内存 >= ${MIN_MEMORY_GB_DEV}GB (开发) / ${MIN_MEMORY_GB_PROD}GB (生产)
  • 磁盘 >= ${MIN_DISK_GB}GB 可用空间
  • Python >= ${MIN_PYTHON_VERSION} (可选，用于 SDK)

EOF
}

show_version() {
    echo "${SCRIPT_NAME} V${SCRIPT_VERSION}"
    echo "Platform: $(uname -s) $(uname -m)"
    echo "Support: Linux, macOS, Windows (WSL)"
    echo "Copyright (c) 2026 SPHARX Ltd. All Rights Reserved."
}

deploy_client() {
    log_info "开始部署 AgentOS 桌面客户端..."
    
    local client_dir="${PROJECT_ROOT}/scripts/desktop-client"
    if [[ ! -d "${client_dir}" ]]; then
        log_error "桌面客户端目录不存在: ${client_dir}"
        return 1
    fi
    
    cd "${client_dir}" || {
        log_error "无法进入桌面客户端目录: ${client_dir}"
        return 1
    }
    
    # 检查 Node.js 和 npm
    if ! command -v node &> /dev/null; then
        log_error "Node.js 未安装，请先安装 Node.js >= 18.x"
        return 1
    fi
    
    if ! command -v npm &> /dev/null; then
        log_error "npm 未安装，请先安装 npm"
        return 1
    fi
    
    # 检查 Rust 和 Cargo (Tauri 需要)
    if ! command -v cargo &> /dev/null; then
        log_warning "Rust 未安装，桌面客户端构建需要 Rust 工具链"
        if [[ "${AUTO_MODE}" == "true" ]] || confirm_action "是否尝试安装 Rust？"; then
            log_info "正在安装 Rust 工具链..."
            curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
            source "$HOME/.cargo/env" || true
        else
            log_warning "跳过 Rust 安装，桌面客户端构建可能失败"
        fi
    fi
    
    # 安装依赖
    log_info "安装 Node.js 依赖..."
    if ! npm install; then
        log_error "Node.js 依赖安装失败"
        return 1
    fi
    
    # 生成图标
    log_info "生成应用图标..."
    if [[ -f "src-tauri/icons/generate_icons.py" ]]; then
        cd src-tauri/icons
        if command -v python3 &> /dev/null; then
            python3 generate_icons.py
        elif command -v python &> /dev/null; then
            python generate_icons.py
        else
            log_warning "Python 未安装，跳过图标生成"
        fi
        cd ../..
    fi
    
    # 构建客户端
    log_info "构建桌面客户端..."
    if ! npm run tauri build; then
        log_error "桌面客户端构建失败"
        return 1
    fi
    
    log_success "桌面客户端构建成功！"
    log_info "构建产物位置: ${client_dir}/src-tauri/target/release/bundle/"
    
    # 根据平台提示安装方式
    case "$(uname -s)" in
        Linux*)
            log_info "Linux 安装方式:"
            log_info "  DEB 包: ${client_dir}/src-tauri/target/release/bundle/deb/*.deb"
            log_info "  AppImage: ${client_dir}/src-tauri/target/release/bundle/appimage/*.AppImage"
            ;;
        Darwin*)
            log_info "macOS 安装方式:"
            log_info "  DMG: ${client_dir}/src-tauri/target/release/bundle/dmg/*.dmg"
            ;;
        MINGW*|MSYS*|CYGWIN*)
            log_info "Windows 安装方式:"
            log_info "  MSI: ${client_dir}/src-tauri/target/release/bundle/msi/*.msi"
            log_info "  EXE: ${client_dir}/src-tauri/target/release/bundle/nsis/*.exe"
            ;;
    esac
    
    return 0
}

# =============================================================================
# 主程序
# =============================================================================

main() {
    local mode="dev"
    local target="all"
    local action="deploy"
    AUTO_MODE="false"

    # 解析命令行参数
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --mode)
                mode="$2"
                shift 2
                ;;
            --target)
                target="$2"
                shift 2
                ;;
            --auto)
                AUTO_MODE="true"
                shift
                ;;
            --check-only)
                action="check"
                shift
                ;;
            --status)
                action="status"
                shift
                ;;
            --stop)
                action="stop"
                shift
                ;;
            --restart)
                action="restart"
                shift
                ;;
            --logs)
                action="logs"
                shift
                ;;
            --cleanup)
                action="cleanup"
                shift
                ;;
            --health)
                action="health"
                shift
                ;;
            -h|--help)
                show_help
                exit 0
                ;;
            -v|--version)
                show_version
                exit 0
                ;;
            *)
                log_error "未知选项: $1"
                show_help
                exit 1
                ;;
        esac
    done

    # 切换到项目根目录
    cd "${PROJECT_ROOT}" || {
        log_error "无法进入项目目录: ${PROJECT_ROOT}"
        exit 1
    }

    # 根据操作类型执行
    case "$action" in
        check)
            init_progress 4
            show_progress "平台检测"
            detect_platform

            show_progress "资源检查"
            check_system_resources "$mode"

            show_progress "依赖检测"
            detect_package_manager
            check_core_dependencies

            show_progress "项目验证"
            validate_project_structure

            complete_progress

            if [[ $ERROR_COUNT -eq 0 ]]; then
                exit 0
            else
                exit 1
            fi
            ;;

        deploy)
            # 根据部署目标执行相应操作
            if [[ "$target" == "client" ]]; then
                # 仅部署客户端
                init_progress 1
                show_progress "部署桌面客户端"
                if ! deploy_client; then
                    log_error "桌面客户端部署失败"
                    ERROR_COUNT=$((ERROR_COUNT + 1))
                fi
                complete_progress
                return 0
            fi
            
            # 部署后端服务（backend 或 all）
            init_progress 7

            # 如果是交互模式，让用户选择部署模式
            if [[ "${AUTO_MODE}" != "true" ]]; then
                mode=$(select_deployment_mode)
            fi

            DEPLOY_MODE="$mode"
            log_info "部署模式: ${mode}"

            show_progress "平台与环境检测"
            detect_platform
            check_system_resources "$mode"

            show_progress "依赖检查与安装"
            detect_package_manager

            if ! check_core_dependencies; then
                if [[ "${AUTO_MODE}" == "true" ]]; then
                    install_dependencies_auto
                    check_core_dependencies
                else
                    if confirm_action "是否尝试自动安装缺失的依赖？"; then
                        install_dependencies_auto
                        check_core_dependencies
                    fi
                fi
            fi

            show_progress "项目结构验证"
            validate_project_structure

            show_progress "配置文件生成"
            setup_environment_config

            show_progress "启动服务 (${mode} 模式)"
            case "$mode" in
                dev)
                    start_dev_environment
                    ;;
                prod)
                    start_prod_environment
                    ;;
                *)
                    log_error "未知部署模式: ${mode}"
                    exit 1
                    ;;
            esac

            show_progress "健康检查与验证"
            # 健康检查已在 start_*_environment 中调用

            # 部署桌面客户端（如果目标包含 client）
            if [[ "$target" == "client" || "$target" == "all" ]]; then
                show_progress "部署桌面客户端"
                if ! deploy_client; then
                    log_error "桌面客户端部署失败"
                    ERROR_COUNT=$((ERROR_COUNT + 1))
                fi
            fi

            complete_progress

            if [[ $ERROR_COUNT -eq 0 ]]; then
                echo -e "${GREEN}🎉 部署成功完成！${NC}"
                echo ""
                echo -e "  ${CYAN}下一步操作:${NC}"
                echo "    1. 访问 Gateway API: http://localhost:18789"
                echo "    2. 查看 OpenLab UI: http://localhost:3000"
                echo "    3. 阅读 Quick Start: docs/getting-started.md"
                echo ""
                exit 0
            else
                echo -e "${RED}❌ 部署完成但发现 ${ERROR_COUNT} 个问题${NC}"
                echo ""
                echo -e "  ${YELLOW}建议操作:${NC}"
                echo "    1. 查看错误日志: ${LOG_DIR}/agentos_error_*.log"
                echo "    2. 运行诊断: $0 --health"
                echo "    3. 查看状态: $0 --status"
                echo "    4. 查看文档: agentos/manuals/guides/troubleshooting.md"
                echo ""
                exit 1
            fi
            ;;

        status)
            show_status
            ;;

        stop)
            stop_environment
            ;;

        restart)
            stop_environment
            sleep 3
            case "$mode" in
                dev) start_dev_environment ;;
                prod) start_prod_environment ;;
            esac
            ;;

        logs)
            collect_logs
            ;;

        cleanup)
            cleanup_resources
            ;;

        health)
            init_progress 1
            show_progress "健康检查"
            run_health_check
            complete_progress
            ;;
    esac
}

# =============================================================================
# 入口点
# =============================================================================

main "$@"
