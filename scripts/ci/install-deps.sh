#!/usr/bin/env bash
# AgentOS 统一依赖安装脚本
# 支持平台: Linux (Ubuntu/Debian), macOS, Windows (MSYS2/Git Bash)
# 特性: 重试机制、错误处理、版本验证、缓存感知

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
MAX_RETRIES=3
RETRY_DELAY=5

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info()  { echo -e "${BLUE}[INFO]${NC}  $*"; }
log_ok()    { echo -e "${GREEN}[OK]${NC}    $*"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
log_error() { echo -e "${RED}[ERROR]${NC} $*" >&2; }

# 带重试的执行函数
retry_cmd() {
    local cmd="$@"
    local attempt=1
    
    while [ $attempt -le $MAX_RETRIES ]; do
        log_info "执行: $cmd (尝试 $attempt/$MAX_RETRIES)"
        
        if eval "$cmd"; then
            log_ok "命令成功: $cmd"
            return 0
        fi
        
        if [ $attempt -lt $MAX_RETRIES ]; then
            log_warn "命令失败，${RETRY_DELAY}秒后重试..."
            sleep $RETRY_DELAY
            RETRY_DELAY=$((RETRY_DELAY * 2))
        fi
        
        attempt=$((attempt + 1))
    done
    
    log_error "命令最终失败（已重试$MAX_RETRIES次）: $cmd"
    return 1
}

# 检测操作系统
detect_os() {
    case "$(uname -s)" in
        Linux*)  echo "linux" ;;
        Darwin*) echo "macos" ;;
        MINGW*|MSYS*|CYGWIN*) echo "windows" ;;
        *)       echo "unknown" ;;
    esac
}

OS=$(detect_os)
log_info "检测到操作系统: $OS"

# ==================== Linux 依赖安装 ====================
install_linux_deps() {
    log_info "=== 安装 Linux 系统依赖 ==="
    
    # 更新包列表（带重试）
    retry_cmd "sudo apt-get update -qq"
    
    # 核心构建工具
    retry_cmd "sudo apt-get install -y --no-install-recommends \
        build-essential cmake gcc g++ pkg-config git wget curl \
        libcurl4-openssl-dev libyaml-dev libcjson-dev libssl-dev \
        libmicrohttpd-dev libwebsockets-dev \
        valgrind gcovr cppcheck clang-format \
        python3 python3-pip python3-venv"
    
    # 可选依赖（失败不阻塞）
    sudo apt-get install -y --no-install-recommends \
        libsqlite3-dev libevent-dev 2>/dev/null || \
        log_warn "可选依赖安装跳过（不影响核心功能）"
    
    # 验证关键依赖
    verify_linux_deps
}

verify_linux_deps() {
    log_info "=== 验证 Linux 依赖 ==="
    
    local missing=0
    
    for cmd in cmake gcc pkg-config; do
        if command -v "$cmd" &>/dev/null; then
            local ver=$("$cmd" --version | head -1)
            log_ok "$cmd: $ver"
        else
            log_error "缺少: $cmd"
            missing=$((missing + 1))
        fi
    done
    
    for pkg in libcurl libyaml cjson openssl; do
        if dpkg -l | grep -q "ii.*${pkg}"; then
            log_ok "$pkg: 已安装"
        else
            log_error "缺少包: $pkg"
            missing=$((missing + 1))
        fi
    done
    
    if [ $missing -gt 0 ]; then
        log_error "缺少 $missing 个依赖，构建可能失败"
        return 1
    fi
    
    log_ok "所有核心依赖验证通过"
    return 0
}

# ==================== tiktoken 存根创建 ====================
create_tiktoken_stub() {
    log_info "=== 创建 tiktoken 存根 ==="
    
    local stub_dir="/usr/local/lib/pkgconfig"
    local stub_file="${stub_dir}/tiktoken.pc"
    
    # 检查是否已有存根
    if [ -f "$stub_file" ]; then
        log_ok "tiktoken 存根已存在"
        return 0
    fi
    
    # 创建目录
    sudo mkdir -p "$stub_dir"
    
    # 创建 .pc 文件
    cat << 'PC_EOF' | sudo tee "$stub_file" > /dev/null
prefix=/usr/local
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: tiktoken
Description: Tokenizer library (CI stub for AgentOS)
Version: 0.5.2
URL: https://github.com/openai/tiktoken
Libs: -L${libdir} -ltiktoken
Cflags: -I${includedir}
PC_EOF
    
    # 创建空桩库
    local tmp_src=$(mktemp /tmp/tiktoken_stub_XXXX.c)
    local tmp_obj=$(mktemp /tmp/tiktoken_stub_XXXX.o)
    
    echo 'void tiktoken_init(void) {}' > "$tmp_src"
    if command -v gcc &>/dev/null; then
        gcc -c "$tmp_src" -o "$tmp_obj" 2>/dev/null || true
        ar rcs /usr/local/lib/libtiktoken.a "$tmp_obj" 2>/dev/null || true
    fi
    
    rm -f "$tmp_src" "$tmp_obj"
    
    log_ok "tiktoken 存根创建完成 ($stub_file)"
}

# ==================== macOS 依赖安装 ====================
install_macos_deps() {
    log_info "=== 安装 macOS 系统依赖 ==="
    
    # 确保 Homebrew 可用
    if ! command -v brew &>/dev/null; then
        log_error "Homebrew 未安装，请先安装 Homebrew"
        return 1
    fi
    
    # 更新 Homebrew
    retry_cmd "brew update"
    
    # 安装核心依赖
    retry_cmd "brew install cmake curl openssl@3 yaml-cpp cjson \
        libmicrohttpd libwebsockets sqlite \
        pkg-config wget valgrind gcovr cppcheck clang-format"
    
    # 链接 OpenSSL
    brew link --force openssl@3 2>/dev/null || true
    
    # Python
    retry_cmd "brew install python@3.11"
    
    # 创建 tiktoken 存根
    create_tiktoken_stub_macos
    
    verify_macos_deps
}

create_tiktoken_stub_macos() {
    log_info "=== 创建 macOS tiktoken 存根 ==="
    
    local stub_dir="/usr/local/lib/pkgconfig"
    local stub_file="${stub_dir}/tiktoken.pc"
    
    if [ -f "$stub_file" ]; then
        log_ok "tiktoken 存根已存在"
        return 0
    fi
    
    sudo mkdir -p "$stub_dir"
    
    cat << 'PC_EOF' | sudo tee "$stub_file" > /dev/null
prefix=/usr/local
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: tiktoken
Description: Tokenizer library (CI stub for AgentOS)
Version: 0.5.2
Libs: -L${libdir} -ltiktoken
Cflags: -I${includedir}
PC_EOF
    
    echo 'void tiktoken_init(void) {}' > /tmp/tiktoken_stub.c
    cc -c /tmp/tiktoken_stub.c -o /tmp/tiktoken_stub.o 2>/dev/null || true
    sudo ar rcs /usr/local/lib/libtiktoken.a /tmp/tiktoken_stub.o 2>/dev/null || true
    rm -f /tmp/tiktoken_stub.c /tmp/tiktoken_stub.o
    
    log_ok "macOS tiktoken 存根创建完成"
}

verify_macos_deps() {
    log_info "=== 验证 macOS 依赖 ==="
    
    for cmd in cmake gcc pkg-config brew; do
        if command -v "$cmd" &>/dev/null; then
            log_ok "$cmd: $(command -v "$cmd")"
        else
            log_error "缺少: $cmd"
        fi
    done
    
    log_ok "macOS 依赖检查完成"
}

# ==================== Windows 依赖说明 ====================
install_windows_deps() {
    log_info "=== Windows 依赖安装说明 ==="
    log_info "Windows 使用 vcpkg 管理依赖，请确保:"
    log_info "  1. vcpkg.json 已存在于项目根目录"
    log_info "  2. CI 中通过 vcpkg 工作流自动安装"
    log_info ""
    log_info "所需 vcpkg 包:"
    log_info "  - curl:x64-windows-static"
    log_info "  - cjson:x64-windows-static"
    log_info "  - yaml-cpp:x64-windows-static"
    log_info "  - openssl:x64-windows-static"
    log_info "  - libmicrohttpd:x64-windows-static"
    log_info "  - libwebsockets:x64-windows-static"
    log_info "  - sqlite3:x64-windows-static"
}

# ==================== 主流程 ====================
main() {
    log_info "========================================="
    log_info "AgentOS 依赖安装脚本"
    log_info "项目路径: ${PROJECT_ROOT}"
    log_info "操作系统: ${OS}"
    log_info "========================================="
    
    case "$OS" in
        linux)
            install_linux_deps
            create_tiktoken_stub
            ;;
        macos)
            install_macos_deps
            ;;
        windows)
            install_windows_deps
            ;;
        *)
            log_error "不支持的操作系统: $OS"
            exit 1
            ;;
    esac
    
    EXIT_CODE=$?
    
    if [ $EXIT_CODE -eq 0 ]; then
        log_info "========================================="
        log_ok "依赖安装全部完成！"
        log_info "========================================="
    else
        log_error "========================================="
        log_error "依赖安装存在问题（退出码: $EXIT_CODE）"
        log_info "========================================="
    fi
    
    exit $EXIT_CODE
}

# 执行主流程
main "$@"
