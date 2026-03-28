# Copyright (c) 2026 SPHARX. All Rights Reserved.
# AgentOS 环境验证脚本
# 用于检查开发和运行环境的完整性
# 版本：v1.0.0.5
# 最后更新：2026-03-20

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 计数器
CHECKS_PASSED=0
CHECKS_FAILED=0
CHECKS_WARNING=0

# 打印检查结果
print_check() {
    local name=$1
    local status=$2
    local message=$3

    case $status in
        "PASS")
            echo -e "${GREEN}✓${NC} $name"
            ((CHECKS_PASSED++))
            ;;
        "FAIL")
            echo -e "${RED}✗${NC} $name"
            if [ -n "$message" ]; then
                echo -e "  ${RED}→${NC} $message"
            fi
            ((CHECKS_FAILED++))
            ;;
        "WARN")
            echo -e "${YELLOW}⚠${NC} $name"
            if [ -n "$message" ]; then
                echo -e "  ${YELLOW}→${NC} $message"
            fi
            ((CHECKS_WARNING++))
            ;;
    esac
}

# 检查操作系统
check_os() {
    echo -e "\n${BLUE}检查操作系统...${NC}"

    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        OS_VERSION=$(uname -r)
        print_check "Linux ($OS_VERSION)" "PASS"
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        OS_VERSION=$(sw_vers -productVersion 2>/dev/null || uname -r)
        print_check "macOS ($OS_VERSION)" "PASS"
    elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
        print_check "Windows (WSL/PowerShell)" "PASS"
    else
        print_check "未知操作系统: $OSTYPE" "WARN" "可能不受支持"
    fi
}

# 检查 Git
check_git() {
    echo -e "\n${BLUE}检查 Git...${NC}"

    if command -v git &> /dev/null; then
        GIT_VERSION=$(git --version)
        print_check "Git ($GIT_VERSION)" "PASS"
    else
        print_check "Git" "FAIL" "未找到 Git，请先安装"
    fi
}

# 检查 Python
check_python() {
    echo -e "\n${BLUE}检查 Python...${NC}"

    if command -v python3 &> /dev/null; then
        PYTHON_VERSION=$(python3 --version 2>&1)
        print_check "Python 3 ($PYTHON_VERSION)" "PASS"

        # 检查 Python 版本是否 >= 3.9
        PYTHON_MINOR=$(python3 -c "import sys; print(sys.version_info.minor)")
        if [ "$PYTHON_MINOR" -ge 9 ]; then
            print_check "Python 版本 >= 3.9" "PASS"
        else
            print_check "Python 版本" "FAIL" "需要 Python 3.9+，当前为 3.$PYTHON_MINOR"
        fi
    else
        print_check "Python 3" "FAIL" "未找到 Python 3"
    fi

    # 检查 pip
    if command -v pip3 &> /dev/null || python3 -m pip &> /dev/null; then
        PIP_VERSION=$(pip3 --version 2>&1 || python3 -m pip --version)
        print_check "pip ($PIP_VERSION)" "PASS"
    else
        print_check "pip" "WARN" "建议安装 pip"
    fi
}

# 检查 CMake
check_cmake() {
    echo -e "\n${BLUE}检查 CMake...${NC}"

    if command -v cmake &> /dev/null; then
        CMAKE_VERSION=$(cmake --version | head -n 1)
        print_check "CMake ($CMAKE_VERSION)" "PASS"

        # 检查 CMake 版本是否 >= 3.20
        CMAKE_MINOR=$(cmake --version | head -n 1 | grep -oP '\d+\.\d+' | tail -n 1 | cut -d. -f2)
        if [ "$CMAKE_MINOR" -ge 20 ] || [ "$(cmake --version | head -n 1 | grep -oP '\d+' | head -n 1)" -ge 4 ]; then
            print_check "CMake 版本 >= 3.20" "PASS"
        else
            print_check "CMake 版本" "WARN" "建议升级到 3.20+"
        fi
    else
        print_check "CMake" "WARN" "未找到 CMake，将无法构建 C++ 内核"
    fi
}

# 检查编译器
check_compiler() {
    echo -e "\n${BLUE}检查编译器...${NC}"

    # GCC
    if command -v gcc &> /dev/null; then
        GCC_VERSION=$(gcc --version | head -n 1)
        print_check "GCC ($GCC_VERSION)" "PASS"
    else
        print_check "GCC" "WARN" "未找到 GCC"
    fi

    # Clang
    if command -v clang &> /dev/null; then
        CLANG_VERSION=$(clang --version | head -n 1)
        print_check "Clang ($CLANG_VERSION)" "PASS"
    else
        print_check "Clang" "WARN" "未找到 Clang"
    fi

    # 至少有一个编译器
    if ! command -v gcc &> /dev/null && ! command -v clang &> /dev/null; then
        print_check "C/C++ 编译器" "FAIL" "需要安装 GCC 或 Clang"
    fi
}

# 检查关键依赖库
check_libraries() {
    echo -e "\n${BLUE}检查关键依赖库...${NC}"

    # OpenSSL
    if command -v openssl &> /dev/null; then
        OPENSSL_VERSION=$(openssl version)
        print_check "OpenSSL ($OPENSSL_VERSION)" "PASS"
    else
        print_check "OpenSSL" "WARN" "未找到 OpenSSL"
    fi

    # 检查 Python 包
    if command -v python3 &> /dev/null; then
        print_info "检查 Python 包..."

        # NumPy
        if python3 -c "import numpy" 2>/dev/null; then
            NP_VERSION=$(python3 -c "import numpy; print(numpy.__version__)")
            print_check "NumPy ($NP_VERSION)" "PASS"
        else
            print_check "NumPy" "WARN" "未安装，运行 quickstart.sh 会自动安装"
        fi

        # PyYAML
        if python3 -c "import yaml" 2>/dev/null; then
            print_check "PyYAML" "PASS"
        else
            print_check "PyYAML" "WARN" "未安装"
        fi
    fi
}

# 检查项目结构
check_project_structure() {
    echo -e "\n${BLUE}检查项目结构...${NC}"

    # 检查关键目录
    DIRS=("atoms" "daemon" "cupolas" "toolkit" "manager" "scripts" "paper" "tests")
    for dir in "${DIRS[@]}"; do
        if [ -d "$dir" ]; then
            print_check "目录：$dir" "PASS"
        else
            print_check "目录：$dir" "FAIL" "缺少关键目录"
        fi
    done

    # 检查关键文件
    FILES=("README.md" "LICENSE" "pyproject.toml" "Makefile")
    for file in "${FILES[@]}"; do
        if [ -f "$file" ]; then
            print_check "文件：$file" "PASS"
        else
            print_check "文件：$file" "FAIL" "缺少关键文件"
        fi
    done
}

# 检查环境变量
check_environment() {
    echo -e "\n${BLUE}检查环境变量...${NC}"

    # 检查 .env 文件
    if [ -f .env ]; then
        print_check ".env 配置文件" "PASS"
    else
        print_check ".env 配置文件" "WARN" "不存在，请从 .env.example 复制"
    fi

    # 检查必要的环境变量（如果已加载）
    if [ -n "$AGENTOS_CONFIG" ]; then
        print_check "AGENTOS_CONFIG 环境变量" "PASS"
    else
        print_check "AGENTOS_CONFIG 环境变量" "WARN" "未设置（可选）"
    fi
}

# 检查磁盘空间
check_disk_space() {
    echo -e "\n${BLUE}检查磁盘空间...${NC}"

    # 获取可用空间（MB）
    if command -v df &> /dev/null; then
        AVAILABLE_SPACE=$(df -m . | awk 'NR==2 {print $4}')
        if [ "$AVAILABLE_SPACE" -gt 5120 ]; then
            print_check "磁盘空间 (${AVAILABLE_SPACE}MB 可用)" "PASS"
        elif [ "$AVAILABLE_SPACE" -gt 1024 ]; then
            print_check "磁盘空间 (${AVAILABLE_SPACE}MB 可用)" "WARN" "建议释放更多空间"
        else
            print_check "磁盘空间 (${AVAILABLE_SPACE}MB 可用)" "FAIL" "空间不足，需要至少 1GB"
        fi
    else
        print_check "磁盘空间" "WARN" "无法检查"
    fi
}

# 显示总结
show_summary() {
    echo -e "\n${BLUE}=========================================="
    echo "  验证总结"
    echo "==========================================${NC}"
    echo ""
    echo -e "${GREEN}通过：$CHECKS_PASSED${NC}"
    echo -e "${YELLOW}警告：$CHECKS_WARNING${NC}"
    echo -e "${RED}失败：$CHECKS_FAILED${NC}"
    echo ""

    if [ $CHECKS_FAILED -gt 0 ]; then
        echo -e "${RED}✗ 环境验证失败${NC}"
        echo "请修复上述错误后重新运行此脚本"
        echo ""
        echo "常见问题解决："
        echo "  - 安装 Git: https://git-scm.com/downloads"
        echo "  - 安装 Python 3.9+: https://www.python.org/downloads/"
        echo "  - 安装 CMake: https://cmake.org/download/"
        echo "  - 安装 GCC: sudo apt-get install build-essential (Ubuntu)"
        echo ""
        exit 1
    elif [ $CHECKS_WARNING -gt 0 ]; then
        echo -e "${YELLOW}⚠ 环境验证通过（有警告）${NC}"
        echo "可以继续，但部分功能可能受限"
        echo ""
        exit 0
    else
        echo -e "${GREEN}✓ 环境验证完全通过！${NC}"
        echo ""
        echo "您现在可以："
        echo "  1. 运行 ./quickstart.sh 快速体验"
        echo "  2. 运行 make build 构建项目"
        echo "  3. 查看 manuals/ 中的文档"
        echo ""
        exit 0
    fi
}

# 辅助函数：打印信息
print_info() {
    echo -e "${BLUE}→${NC} $1"
}

# 主函数
main() {
    echo ""
    echo "=========================================="
    echo "  AgentOS 环境验证工具"
    echo "  版本：v1.0.0.5"
    echo "=========================================="

    # 执行所有检查
    check_os
    check_git
    check_python
    check_cmake
    check_compiler
    check_libraries
    check_project_structure
    check_environment
    check_disk_space

    # 显示总结
    show_summary
}

# 执行主函数
main "$@"
