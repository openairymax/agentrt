#!/bin/bash
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved. "From data intelligence emerges".
# AgentOS Docker 快速入门脚本
# 一键完成环境检查、镜像构建和服务启动

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# Logo
print_logo() {
    echo -e "${CYAN}"
    cat << "EOF"
   ____          _        __  __                                                   _   
  / __ \        | |      |  \/  |                                                 | |  
 | |  | |_ __   __| | __ _| \  / | __ _ _ __   __ _  ___ _ __ ___   ___  _ __  __ _| |_ 
 | |  | | '_ \ / _` |/ _` | |\/| |/ _` | '_ \ / _` |/ _ \ '_ ` _ \ / _ \| '_ \/ _` | __|
 | |__| | | | | (_| | (_| | |  | | (_| | | | | (_| |  __/ | | | | | (_) | | | | (_| | |_ 
  \____/|_| |_|\__,_|\__,_|_|  |_|\__,_|_| |_|\__, |\___|_| |_| |_|\___/|_| |_|\__,_|\__|
                                               __/ |                                      
                                              |___/                                       
EOF
    echo -e "${NC}"
}

# 打印步骤
print_step() {
    echo -e "${BLUE}[步骤]${NC} $1"
}

# 打印成功
print_success() {
    echo -e "${GREEN}[成功]${NC} $1"
}

# 打印警告
print_warning() {
    echo -e "${YELLOW}[警告]${NC} $1"
}

# 打印错误
print_error() {
    echo -e "${RED}[错误]${NC} $1"
}

# 检查 Docker
check_docker() {
    print_step "检查 Docker 环境..."
    
    if ! command -v docker &> /dev/null; then
        print_error "Docker 未安装！"
        echo "请先安装 Docker: https://docs.docker.com/get-docker/"
        exit 1
    fi
    
    if ! docker info &> /dev/null; then
        print_error "Docker 未运行！"
        echo "请启动 Docker 服务"
        exit 1
    fi
    
    print_success "Docker 已安装：$(docker --version)"
}

# 检查 Docker Compose
check_docker_compose() {
    print_step "检查 Docker Compose..."
    
    if ! command -v docker-compose &> /dev/null; then
        print_warning "docker-compose 未安装，尝试使用 'docker compose'"
        export DOCKER_COMPOSE="docker compose"
    else
        export DOCKER_COMPOSE="docker-compose"
    fi
    
    print_success "Docker Compose 就绪"
}

# 检查端口占用
check_ports() {
    print_step "检查端口占用情况..."
    
    local ports=(8080 8081 8082 8083 8084)
    local occupied=()
    
    for port in "${ports[@]}"; do
        if command -v lsof &> /dev/null; then
            if lsof -i :$port &> /dev/null; then
                occupied+=($port)
            fi
        elif command -v netstat &> /dev/null; then
            if netstat -tuln | grep -q ":$port "; then
                occupied+=($port)
            fi
        fi
    done
    
    if [ ${#occupied[@]} -gt 0 ]; then
        print_warning "以下端口被占用：${occupied[*]}"
        echo "请确保这些端口可用，或修改 docker-compose.yml 中的端口映射"
        read -p "是否继续？(y/N): " confirm
        if [[ ! $confirm =~ ^[Yy]$ ]]; then
            exit 1
        fi
    else
        print_success "所有端口可用"
    fi
}

# 创建 .env 文件
create_env_file() {
    print_step "配置环境变量..."
    
    if [ -f ".env" ]; then
        print_warning ".env 文件已存在，跳过创建"
        return
    fi
    
    cp .env.example .env
    print_success "已创建 .env 文件"
    
    echo ""
    echo "请编辑 .env 文件配置以下必要参数:"
    echo "  - OPENAI_API_KEY (或其他 LLM API 密钥)"
    echo "  - POSTGRES_PASSWORD (数据库密码)"
    echo ""
    
    read -p "是否现在编辑 .env 文件？(y/N): " edit
    if [[ $edit =~ ^[Yy]$ ]]; then
        if command -v nano &> /dev/null; then
            nano .env
        elif command -v vim &> /dev/null; then
            vim .env
        else
            notepad .env 2>/dev/null || echo "请手动编辑 .env 文件"
        fi
    fi
}

# 构建镜像
build_images() {
    print_step "构建 Docker 镜像..."
    
    echo "选择构建类型:"
    echo "  1) 生产版本 (推荐，镜像更小)"
    echo "  2) 开发版本 (包含调试工具)"
    read -p "请选择 [1-2]: " choice
    
    local build_type="release"
    if [[ $choice == "2" ]]; then
        build_type="dev"
        print_warning "开发版本镜像较大，包含调试工具"
    fi
    
    print_info "开始构建镜像 (${build_type})..."
    
    # 使用构建脚本
    chmod +x build.sh
    ./build.sh all ${build_type}
    
    print_success "镜像构建完成"
}

# 启动服务
start_services() {
    print_step "启动 AgentOS 服务..."
    
    $DOCKER_COMPOSE up -d
    
    print_success "服务已启动"
}

# 等待服务就绪
wait_for_services() {
    print_step "等待服务就绪..."
    
    local max_attempts=30
    local attempt=0
    
    echo "检查内核服务..."
    while [ $attempt -lt $max_attempts ]; do
        if docker ps --format '{{.Names}}' | grep -q "agentos-kernel"; then
            print_success "内核服务已启动"
            break
        fi
        sleep 2
        attempt=$((attempt + 1))
        echo -n "."
    done
    
    if [ $attempt -eq $max_attempts ]; then
        print_warning "内核服务启动超时，请查看日志"
    fi
    
    echo ""
    echo "检查服务层..."
    sleep 5
    
    if docker ps --format '{{.Names}}' | grep -q "agentos-services"; then
        print_success "服务层已启动"
    else
        print_warning "服务层未启动，请查看日志"
    fi
}

# 显示访问信息
show_access_info() {
    echo ""
    print_success "AgentOS 部署完成！"
    echo ""
    echo "=========================================="
    echo "  服务访问信息"
    echo "=========================================="
    echo ""
    echo "📡 服务端口:"
    echo "  LLM 服务：     http://localhost:8080"
    echo "  工具服务：   http://localhost:8081"
    echo "  市场服务：   http://localhost:8082"
    echo "  调度服务：   http://localhost:8083"
    echo "  监控服务：   http://localhost:8084"
    echo ""
    echo "🔍 监控面板:"
    echo "  Jaeger UI:   http://localhost:16686"
    echo "  Prometheus:  http://localhost:8888/metrics"
    echo ""
    echo "📝 常用命令:"
    echo "  查看日志：docker-compose logs -f"
    echo "  查看状态：docker-compose ps"
    echo "  停止服务：docker-compose down"
    echo "  进入容器：docker-compose exec agentos-kernel bash"
    echo ""
    echo "💡 提示:"
    echo "  使用 'make help' 查看更多可用命令"
    echo ""
    echo "=========================================="
    echo ""
}

# 主函数
main() {
    print_logo
    
    echo "欢迎使用 AgentOS Docker 快速入门向导"
    echo ""
    
    # 环境检查
    check_docker
    check_docker_compose
    check_ports
    
    echo ""
    
    # 配置和构建
    create_env_file
    build_images
    start_services
    wait_for_services
    
    # 显示访问信息
    show_access_info
    
    print_success "部署完成！祝您使用愉快！"
}

# 捕获中断信号
trap 'echo ""; print_warning "操作被中断"; exit 1' INT TERM

# 执行主函数
main "$@"
