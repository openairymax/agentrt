#!/usr/bin/env bash
# ──────────────────────────────────────────────────────────
# AgentRT Installer Script (Linux/macOS)
# Version: 0.1.1
# Usage: curl -fsSL https://raw.githubusercontent.com/spharx/agentrt/main/scripts/get-agentrt.sh | bash
# ──────────────────────────────────────────────────────────
set -euo pipefail

BOLD='\033[1m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
RED='\033[0;31m'
NC='\033[0m'

info()  { echo -e "${CYAN}[INFO]${NC} $*"; }
ok()    { echo -e "${GREEN}[OK]${NC} $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $*"; }
fail()  { echo -e "${RED}[FAIL]${NC} $*"; exit 1; }

VERSION="${AGENTRT_VERSION:-0.1.1}"
INSTALL_DIR="${AGENTRT_INSTALL_DIR:-$HOME/.agentrt}"
BIN_DIR="${AGENTRT_BIN_DIR:-$HOME/.local/bin}"
REPO_URL="https://github.com/spharx/agentrt"

echo ""
echo -e "${BOLD}═══════════════════════════════════════════════${NC}"
echo -e "${BOLD}   AgentRT v${VERSION} Installer${NC}"
echo -e "${BOLD}═══════════════════════════════════════════════${NC}"
echo ""

# ── Step 1: 检测平台 ─────────────────────────────────────
info "Step 1/6: 检测平台..."

OS="$(uname -s | tr '[:upper:]' '[:lower:]')"
ARCH="$(uname -m)"

case "${OS}" in
    linux)  PLATFORM="linux" ;;
    darwin) PLATFORM="darwin" ;;
    *)      fail "不支持的操作系统: ${OS}" ;;
esac

case "${ARCH}" in
    x86_64|amd64) ARCH="amd64" ;;
    aarch64|arm64) ARCH="arm64" ;;
    *)             fail "不支持的架构: ${ARCH}" ;;
esac

ok "平台: ${PLATFORM}-${ARCH}"
echo ""

# ── Step 2: 检查依赖 ─────────────────────────────────────
info "Step 2/6: 检查依赖..."

MISSING_DEPS=()

check_dep() {
    if command -v "$1" &>/dev/null; then
        ok "$1 已安装"
    else
        warn "$1 未安装"
        MISSING_DEPS+=("$1")
    fi
}

check_dep curl
check_dep git
check_dep gcc
check_dep cmake
check_dep cargo

if [ ${#MISSING_DEPS[@]} -gt 0 ]; then
    warn "缺失依赖: ${MISSING_DEPS[*]}"
    info "尝试安装缺失依赖..."

    if command -v apt-get &>/dev/null; then
        sudo apt-get update -qq
        sudo apt-get install -y -qq "${MISSING_DEPS[@]}" 2>/dev/null || true
    elif command -v brew &>/dev/null; then
        brew install "${MISSING_DEPS[@]}" 2>/dev/null || true
    elif command -v dnf &>/dev/null; then
        sudo dnf install -y "${MISSING_DEPS[@]}" 2>/dev/null || true
    fi
fi
echo ""

# ── Step 3: 下载源码 ─────────────────────────────────────
info "Step 3/6: 下载 AgentRT 源码..."

if [ -d "${INSTALL_DIR}/AgentRT" ]; then
    info "已有源码目录，执行 git pull..."
    cd "${INSTALL_DIR}/AgentRT"
    git pull --ff-only 2>/dev/null || warn "git pull 失败，使用现有代码"
else
    mkdir -p "${INSTALL_DIR}"
    cd "${INSTALL_DIR}"
    git clone --depth 1 --branch "v${VERSION}" "${REPO_URL}.git" AgentRT 2>/dev/null || \
    git clone --depth 1 "${REPO_URL}.git" AgentRT 2>/dev/null || \
    warn "git clone 失败，请手动下载"
fi

ok "源码已就绪: ${INSTALL_DIR}/AgentRT"
echo ""

# ── Step 4: 构建 ─────────────────────────────────────────
info "Step 4/6: 构建 AgentRT..."

cd "${INSTALL_DIR}/AgentRT"

# 构建 C 核心
if [ -f "CMakeLists.txt" ]; then
    info "构建 C 核心引擎..."
    mkdir -p build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release 2>/dev/null || warn "CMake 配置失败"
    make -j"$(nproc 2>/dev/null || echo 4)" 2>/dev/null || warn "C 构建失败"
    cd ..
fi

# 构建 CLI
if [ -f "sdk/cli/Cargo.toml" ]; then
    info "构建 CLI 工具..."
    cd sdk/cli
    cargo build --release 2>/dev/null || warn "CLI 构建失败"
    cd ../..
fi

# 构建 TUI
if [ -f "sdk/tui/Cargo.toml" ]; then
    info "构建 TUI 工具..."
    cd sdk/tui
    cargo build --release 2>/dev/null || warn "TUI 构建失败"
    cd ../..
fi

ok "构建完成"
echo ""

# ── Step 5: 安装 ─────────────────────────────────────────
info "Step 5/6: 安装到系统..."

mkdir -p "${BIN_DIR}"

# 安装 CLI
if [ -f "sdk/cli/target/release/agentrt" ]; then
    cp sdk/cli/target/release/agentrt "${BIN_DIR}/agentrt"
    chmod +x "${BIN_DIR}/agentrt"
    ok "CLI 已安装: ${BIN_DIR}/agentrt"
fi

# 安装 TUI
if [ -f "sdk/tui/target/release/agentrt-tui" ]; then
    cp sdk/tui/target/release/agentrt-tui "${BIN_DIR}/agentrt-tui"
    chmod +x "${BIN_DIR}/agentrt-tui"
    ok "TUI 已安装: ${BIN_DIR}/agentrt-tui"
fi

# 确保 BIN_DIR 在 PATH 中
if [[ ":${PATH}:" != *":${BIN_DIR}:"* ]]; then
    info "将 ${BIN_DIR} 添加到 PATH..."
    SHELL_RC="${HOME}/.bashrc"
    if [ -f "${HOME}/.zshrc" ]; then
        SHELL_RC="${HOME}/.zshrc"
    fi
    echo "" >> "${SHELL_RC}"
    echo "# AgentRT" >> "${SHELL_RC}"
    echo "export PATH=\"${BIN_DIR}:\$PATH\"" >> "${SHELL_RC}"
    warn "请运行 'source ${SHELL_RC}' 或重新打开终端以更新 PATH"
fi

echo ""

# ── Step 6: 验证 ─────────────────────────────────────────
info "Step 6/6: 验证安装..."

if command -v agentrt &>/dev/null; then
    ok "agentrt 命令可用"
    agentrt --version 2>/dev/null || true
else
    warn "agentrt 命令尚未在 PATH 中，请重新打开终端"
fi

echo ""
echo -e "${BOLD}═══════════════════════════════════════════════${NC}"
echo -e "${GREEN}${BOLD}   安装完成！${NC}"
echo -e "${BOLD}═══════════════════════════════════════════════${NC}"
echo ""
echo -e "${BOLD}快速开始:${NC}"
echo ""
echo -e "  1. 创建第一个 Agent:"
echo -e "     ${CYAN}agentrt init my-first-agent${NC}"
echo ""
echo -e "  2. 运行 Agent:"
echo -e "     ${CYAN}cd my-first-agent && agentrt run \"你好，世界！\"${NC}"
echo ""
echo -e "  3. 或使用 QuickStart 脚本:"
echo -e "     ${CYAN}${INSTALL_DIR}/AgentRT/scripts/quickstart.sh${NC}"
echo ""
echo -e "  4. 查看文档:"
echo -e "     ${CYAN}agentrt --help${NC}"
echo ""
echo -e "${BOLD}更多信息:${NC}"
echo -e "  源码: ${CYAN}${REPO_URL}${NC}"
echo -e "  版本: ${CYAN}v${VERSION}${NC}"
echo -e "  安装目录: ${CYAN}${INSTALL_DIR}${NC}"
echo ""
