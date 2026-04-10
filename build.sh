#!/bin/bash
# AgentOS 跨平台构建脚本 (Linux/macOS)

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

# 检测操作系统
detect_os() {
    case "$(uname -s)" in
        Linux*)     OS=linux;;
        Darwin*)    OS=macos;;
        CYGWIN*|MINGW*|MSYS*) OS=windows;;
        *)          OS=unknown;;
    esac
    echo "$OS"
}

# 检测CPU架构
detect_arch() {
    case "$(uname -m)" in
        x86_64|amd64) ARCH=x64;;
        aarch64|arm64) ARCH=arm64;;
        armv7l) ARCH=armv7;;
        *) ARCH=unknown;;
    esac
    echo "$ARCH"
}

# 主函数
main() {
    print_info "AgentOS 构建脚本 v1.0.0"
    print_info "================================"
    
    # 检测操作系统和架构
    OS=$(detect_os)
    ARCH=$(detect_arch)
    
    print_info "操作系统: $OS"
    print_info "架构: $ARCH"
    
    if [ "$OS" = "windows" ]; then
        print_warn "Windows 环境检测到，请使用 build.ps1 脚本"
        print_info "正在切换到 PowerShell 脚本..."
        if [ -f "./build.ps1" ]; then
            powershell -ExecutionPolicy Bypass -File ./build.ps1 "$@"
            exit $?
        else
            print_error "未找到 build.ps1 脚本"
            exit 1
        fi
    fi
    
    # 构建类型
    BUILD_TYPE="${1:-Release}"
    case "$BUILD_TYPE" in
        Debug|Release|RelWithDebInfo|MinSizeRel)
            print_info "构建类型: $BUILD_TYPE"
            ;;
        *)
            print_error "无效的构建类型: $BUILD_TYPE"
            print_error "可用选项: Debug, Release, RelWithDebInfo, MinSizeRel"
            exit 1
            ;;
    esac
    
    # 构建目录
    BUILD_DIR="build_${OS}_${ARCH}_${BUILD_TYPE}"
    print_info "构建目录: $BUILD_DIR"
    
    # 清理选项
    CLEAN=false
    if [ "${2:-}" = "--clean" ] || [ "${2:-}" = "-c" ]; then
        CLEAN=true
    fi
    
    if [ "$CLEAN" = true ] && [ -d "$BUILD_DIR" ]; then
        print_info "清理构建目录: $BUILD_DIR"
        rm -rf "$BUILD_DIR"
    fi
    
    # 创建构建目录
    if [ ! -d "$BUILD_DIR" ]; then
        mkdir -p "$BUILD_DIR"
    fi
    
    # 进入构建目录
    cd "$BUILD_DIR"
    
    # CMake 配置
    print_info "正在配置 CMake..."
    
    CMAKE_OPTIONS="
        -DCMAKE_BUILD_TYPE=$BUILD_TYPE
        -DBUILD_TESTS=OFF
        -DBUILD_EXAMPLES=ON
        -DBUILD_DOCS=OFF
        -DENABLE_COVERAGE=OFF
    "
    
    # 平台特定选项
    if [ "$OS" = "linux" ]; then
        CMAKE_OPTIONS="$CMAKE_OPTIONS -DENABLE_SANITIZERS=OFF"
    elif [ "$OS" = "macos" ]; then
        CMAKE_OPTIONS="$CMAKE_OPTIONS -DCMAKE_OSX_ARCHITECTURES=$ARCH"
    fi
    
    # 禁用有问题的模块（第一阶段）
    CMAKE_OPTIONS="$CMAKE_OPTIONS
        -DBUILD_MANAGER=OFF
        -DBUILD_DAEMON=OFF
        -DBUILD_OPENLAB=OFF
        -DBUILD_TOOLKIT=OFF
        -DBUILD_HEAPSTORE=OFF
        -DBUILD_GATEWAY=OFF
    "
    
    # 运行 CMake
    cmake .. $CMAKE_OPTIONS
    
    # 构建
    print_info "正在构建..."
    CORES=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    print_info "使用 $CORES 个核心进行构建"
    cmake --build . --config $BUILD_TYPE --parallel $CORES
    
    # 安装（可选）
    if [ "${3:-}" = "--install" ] || [ "${3:-}" = "-i" ]; then
        print_info "正在安装..."
        cmake --install . --config $BUILD_TYPE --prefix ../install
        print_info "安装完成: ../install"
    fi
    
    print_info "构建完成！"
    print_info "构建目录: $(pwd)"
    print_info "构建类型: $BUILD_TYPE"
    
    # 返回项目根目录
    cd ..
}

# 执行主函数
main "$@"