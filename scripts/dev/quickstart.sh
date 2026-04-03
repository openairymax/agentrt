# Copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
# AgentOS 一键快速启动脚本
# 用于快速体验 AgentOS 核心功能 
# 版本：v1.0.0.5
# 最后更新：2026-03-20

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 打印带颜色的消息
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 检查 Python 版本
check_python() {
    print_info "检查 Python 版本..."
    if command -v python3 &> /dev/null; then
        PYTHON_VERSION=$(python3 --version)
        print_success "找到 Python: $PYTHON_VERSION"
    else
        print_error "未找到 Python 3，请先安装 Python 3.9+"
        exit 1
    fi
}

# 检查 CMake
check_cmake() {
    print_info "检查 CMake..."
    if command -v cmake &> /dev/null; then
        CMAKE_VERSION=$(cmake --version | head -n 1)
        print_success "找到 CMake: $CMAKE_VERSION"
    else
        print_warning "未找到 CMake，将跳过 C++ 内核构建"
    fi
}

# 检查 Git
check_git() {
    print_info "检查 Git..."
    if command -v git &> /dev/null; then
        GIT_VERSION=$(git --version)
        print_success "找到 Git: $GIT_VERSION"
    else
        print_error "未找到 Git，请先安装 Git"
        exit 1
    fi
}

# 初始化环境
init_environment() {
    print_info "初始化环境..."

    # 复制环境变量文件
    if [ ! -f .env ]; then
        print_info "复制环境变量模板..."
        cp .env.example .env
        print_success "环境变量已配置"
    else
        print_info ".env 文件已存在，跳过"
    fi

    # 运行配置初始化脚本
    if [ -f scripts/init_config.py ]; then
        print_info "运行配置初始化脚本..."
        python3 scripts/init_config.py
        print_success "配置初始化完成"
    fi
}

# 安装依赖
install_dependencies() {
    print_info "安装 Python 依赖..."

    # 检查是否使用 Poetry
    if command -v poetry &> /dev/null; then
        print_info "使用 Poetry 安装依赖..."
        poetry install
    else
        print_info "使用 pip 安装依赖..."
        python3 -m pip install -e . || python3 -m pip install numpy scipy pandas pyyaml requests
    fi

    print_success "依赖安装完成"
}

# 构建内核（可选）
build_kernel() {
    if command -v cmake &> /dev/null; then
        print_info "构建 C++ 内核..."

        # 创建构建目录
        mkdir -p build
        cd build

        # 配置 CMake
        cmake ../atoms \
            -DCMAKE_BUILD_TYPE=Release \
            -DBUILD_TESTS=OFF \
            -DENABLE_TRACING=OFF

        # 编译
        cmake --build . --parallel $(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

        cd ..
        print_success "内核构建完成"
    else
        print_warning "跳过 C++ 内核构建（CMake 未安装）"
    fi
}

# 运行示例
run_example() {
    print_info "运行示例程序..."

    # 检查是否有示例脚本
    if [ -f scripts/run_example.sh ]; then
        bash scripts/run_example.sh
    else
        print_info "运行 Python 示例..."
        # 这里可以添加简单的 Python 示例代码
        python3 -c "
print('AgentOS 快速体验')
print('=' * 40)
print('✓ 环境检查通过')
print('✓ 依赖安装完成')
print('✓ 配置初始化成功')
print('')
print('AgentOS 已准备就绪!')
print('详细文档请访问：https://docs.spharx.cn/agentos')
"
    fi

    print_success "示例运行完成"
}

# 显示下一步指引
show_next_steps() {
    echo ""
    print_success "🎉 AgentOS 快速体验完成!"
    echo ""
    echo "=========================================="
    echo "下一步:"
    echo "=========================================="
    echo ""
    echo "1. 查看文档:"
    echo "   - 快速入门：manuals/guides/getting_started.md"
    echo "   - 架构说明：manuals/architecture/"
    echo "   - API 文档：manuals/api/"
    echo ""
    echo "2. 开始开发:"
    echo "   - 阅读 CONTRIBUTING.md 了解贡献流程"
    echo "   - 查看 tests/ 目录了解测试示例"
    echo ""
    echo "3. 常用命令:"
    echo "   - make build     : 构建项目"
    echo "   - make test      : 运行测试"
    echo "   - make docs      : 生成文档"
    echo "   - make clean     : 清理构建产物"
    echo ""
    echo "4. 获取支持:"
    echo "   - Gitee Issues: https://gitee.com/spharx/agentos/issues"
    echo "   - GitHub Issues: https://github.com/SpharxTeam/AgentOS/issues"
    echo "   - 官方邮箱：lidecheng@spharx.cn, wangliren@spharx.cn"
    echo ""
    echo "=========================================="
}

# 主函数
main() {
    echo ""
    echo "=========================================="
    echo "  AgentOS 快速启动脚本"
    echo "  From data intelligence emerges"
    echo "=========================================="
    echo ""

    # 检查依赖
    check_git
    check_python
    check_cmake

    # 初始化和安装
    init_environment
    install_dependencies

    # 可选的内核构建
    if command -v cmake &> /dev/null; then
        build_kernel
    fi

    # 运行示例
    run_example

    # 显示下一步指引
    show_next_steps

    echo ""
    print_success "准备就绪！开始构建您的智能体应用吧！"
    echo ""
    echo "=========================================="
    echo "© 2026 SPHARX Ltd. 保留所有权利。"
    echo "=========================================="
}

# 执行主函数
main "$@"

