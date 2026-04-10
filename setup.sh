#!/bin/bash
# AgentOS 一键安装设置脚本 (Linux/macOS)

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 检查命令是否存在
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# 检测操作系统
detect_os() {
    case "$(uname -s)" in
        Linux*)     OS=linux;;
        Darwin*)    OS=macos;;
        *)          OS=unknown;;
    esac
    echo "$OS"
}

# 安装系统依赖 (Linux)
install_system_deps_linux() {
    print_info "正在安装系统依赖 (Linux)..."
    
    if command_exists apt-get; then
        # Debian/Ubuntu
        sudo apt-get update
        sudo apt-get install -y \
            git cmake build-essential \
            pkg-config libssl-dev \
            libsqlite3-dev libcurl4-openssl-dev \
            libyaml-dev libcjson-dev \
            libmicrohttpd-dev libwebsockets-dev \
            python3 python3-pip
    elif command_exists yum; then
        # RHEL/CentOS
        sudo yum install -y \
            git cmake make gcc gcc-c++ \
            pkgconfig openssl-devel \
            sqlite-devel libcurl-devel \
            libyaml-devel cjson-devel \
            libmicrohttpd-devel libwebsockets-devel \
            python3 python3-pip
    elif command_exists dnf; then
        # Fedora
        sudo dnf install -y \
            git cmake make gcc gcc-c++ \
            pkgconfig openssl-devel \
            sqlite-devel libcurl-devel \
            libyaml-devel cjson-devel \
            libmicrohttpd-devel libwebsockets-devel \
            python3 python3-pip
    elif command_exists pacman; then
        # Arch Linux
        sudo pacman -S --noconfirm \
            git cmake base-devel \
            pkg-config openssl \
            sqlite curl \
            yaml cjson \
            libmicrohttpd libwebsockets \
            python python-pip
    else
        print_warn "无法识别的包管理器，请手动安装依赖"
        return 1
    fi
}

# 安装系统依赖 (macOS)
install_system_deps_macos() {
    print_info "正在安装系统依赖 (macOS)..."
    
    if ! command_exists brew; then
        print_info "正在安装 Homebrew..."
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    fi
    
    brew update
    brew install \
        git cmake pkg-config \
        openssl sqlite curl \
        yaml-cpp cjson \
        libmicrohttpd libwebsockets \
        python3
    
    # 设置 OpenSSL 路径
    export PKG_CONFIG_PATH="/usr/local/opt/openssl@3/lib/pkgconfig:$PKG_CONFIG_PATH"
}

# 安装 vcpkg
install_vcpkg() {
    print_info "正在安装 vcpkg..."
    
    if [ -d "vcpkg" ]; then
        print_info "vcpkg 已存在，跳过安装"
        return 0
    fi
    
    git clone https://github.com/Microsoft/vcpkg.git
    cd vcpkg
    ./bootstrap-vcpkg.sh
    
    # 添加到环境变量
    export VCPKG_ROOT="$(pwd)"
    export PATH="$VCPKG_ROOT:$PATH"
    
    cd ..
    
    print_info "vcpkg 安装完成"
}

# 安装 vcpkg 依赖
install_vcpkg_deps() {
    print_info "正在安装 vcpkg 依赖..."
    
    if [ ! -f "vcpkg.json" ]; then
        print_error "vcpkg.json 不存在"
        return 1
    fi
    
    if ! command_exists vcpkg; then
        print_error "vcpkg 未找到"
        return 1
    fi
    
    cd vcpkg
    ./vcpkg install ../vcpkg.json
    cd ..
    
    print_info "vcpkg 依赖安装完成"
}

# 主函数
main() {
    print_info "AgentOS 一键安装设置脚本 v1.0.0"
    print_info "======================================"
    
    OS=$(detect_os)
    print_info "操作系统: $OS"
    
    if [ "$OS" = "unknown" ]; then
        print_error "不支持的操作系统"
        exit 1
    fi
    
    # 安装系统依赖
    read -p "是否安装系统依赖？(y/n): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        if [ "$OS" = "linux" ]; then
            install_system_deps_linux
        elif [ "$OS" = "macos" ]; then
            install_system_deps_macos
        fi
    fi
    
    # 安装 vcpkg
    read -p "是否安装 vcpkg？(y/n): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        install_vcpkg
    fi
    
    # 安装 vcpkg 依赖
    if [ -d "vcpkg" ]; then
        read -p "是否安装 vcpkg 依赖？(y/n): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            install_vcpkg_deps
        fi
    fi
    
    # 构建项目
    read -p "是否构建项目？(y/n): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        if [ -f "./build.sh" ]; then
            chmod +x ./build.sh
            ./build.sh Release --clean
        else
            print_error "未找到 build.sh 脚本"
            exit 1
        fi
    fi
    
    print_info "设置完成！"
    print_info "======================================"
    print_info "下一步："
    print_info "1. 运行 ./build.sh 进行构建"
    print_info "2. 查看 README.md 获取使用说明"
    print_info "3. 运行示例程序: ./build_*/bin/agentos_example"
    
    if [ -d "vcpkg" ]; then
        print_info "vcpkg 已安装，设置环境变量:"
        print_info "  export VCPKG_ROOT=\"$(pwd)/vcpkg\""
        print_info "  export PATH=\"\$VCPKG_ROOT:\$PATH\""
    fi
}

# 执行主函数
main "$@"