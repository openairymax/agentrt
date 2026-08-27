#!/bin/sh
# ============================================================================
# Airymax AgentRT 一键安装脚本（唯一官方安装入口）
#
# 位置：agentrt 管理仓 scripts/install.sh（v0.1.2 起自伞仓 scripts/ 迁移，
#       构建系统与安装器属 IRON-9 [IND] 完全独立层，随 agentrt 仓独立演进；
#       伞仓 scripts/ 保留兼容重定向入口）。
# 用法：
#   sh <(curl -fsSL https://raw.atomgit.com/openairymax/agentrt/raw/main/scripts/install.sh) --channel stable
#   sh install.sh --prefix "$HOME/.airymaxrt"        # 自定义路径
#   sh install.sh --uninstall                        # 一键卸载
#
# 安装策略（三模式，按可达性自动降级）：
#   模式 A 二进制：AIRY_RELEASE_URL 指向完全体 tarball（含闭源模块预编译产物），
#      下载解压到 $AIRY_HOME，秒级安装、无需工具链（完全体二进制为主）。
#   模式 B 混合构建：管理仓 + 公开子仓源码编译；闭源模块（atoms / memoryrovol）
#      下载预编译包到 $AIRY_HOME/modules/ 后链接（AIRY_ATOMS_PREBUILT_DIR /
#      MEMORYROVOL_PRO_LIB）。本地无闭源源码时自动走此模式。
#   模式 C 全源码构建：本地已持有闭源模块源码（如 airymaxrt-local），直接
#      全量源码编译（AIRY_MODE=source 或检测到本地源码树时）。
#
# 路径体系（与 platform.h AIRY_HOME 完全一致，全产物收敛）：
#   $AIRY_HOME            = ${AIRY_HOME:-$HOME/.airymaxrt}（--prefix 覆盖）
#   $AIRY_HOME/bin  lib  include  config  run  logs  data  tmp  cache
#   $AIRY_HOME/modules    — 闭源预编译模块包（atoms/memoryrovol）
#   $AIRY_HOME/src        — 源码树（构建模式）
#   $AIRY_HOME/build      — out-of-source 构建目录（构建模式）
#   $AIRY_HOME/scripts    — 安装器自托管（install/uninstall 副本）
#
# 环境变量：
#   AIRY_HOME / AIRY_VERSION / AIRY_REPO_URL / AIRY_BUILD_JOBS
#   AIRY_RELEASE_URL / AIRY_NO_BUILD / AIRY_MODE(auto|binary|hybrid|source)
#   AIRY_ATOMS_PREBUILT_URL / AIRY_MEMORYROVOL_PREBUILT_URL（闭源预编译包直链）
# 硬件自适应（2.3.5/2.3.6）：安装即按架构/内存/CPU/加速器裁剪运行画像
#   （full/minimal，固化到 config/profile.env）；AIRY_RELEASE_URL 支持
#   {arch} 占位符按当前架构选择预编译包；airymaxrt monitor 常驻检测
#   外设增强（内存扩容/插卡）后自动恢复被裁剪的功能 daemon。
#
# 参数：
#   --prefix <path>  --mode <auto|binary|hybrid|source>  --bin-dir <path>
#   --profile <full|minimal|auto>  --channel <stable|beta>  --from-file <tarball>
#   --uninstall [--keep-data] [--yes]  --help
#
# 发布通道（2.3.7）：--channel stable|beta 选择官方滚动通道；未指定
# AIRY_RELEASE_URL 时默认拉取官方通道 manifest（GPG 验签 + 本平台制品解析），
# 不再强制源码构建。--from-file <tarball> 支持离线包安装（跳过网络，
# 仅 sha256 + 架构自检）。AIRY_RELEASE_URL 亦支持直接指向 tarball URL
# （{arch} 占位符）或 manifest JSON。官方制品仓库：atomgit.com/openairymax/agentrt。
#
# 安装完成后：固化 install.env（含 AIRY_BIN_LINK）、生成 agentrt-env.sh、
# 软链 airymaxrt 启动器到 PATH（任意路径输入 airymaxrt 即启动），
# 校验 18 个 daemon 全部就位。
#
# 卸载：sh install.sh --uninstall 或 airymaxrt uninstall（停止 daemon +
#       删除 $AIRY_HOME + 移除 PATH 软链；--keep-data 保留记忆数据）。
# ============================================================================

set -u

# ─── 颜色（无 TTY 时禁用） ──────────────────────────────────────────────
if [ -t 1 ]; then
    C_RED='\033[0;31m'; C_GREEN='\033[0;32m'; C_YELLOW='\033[1;33m'; C_CYAN='\033[0;36m'; C_NC='\033[0m'
else
    C_RED=''; C_GREEN=''; C_YELLOW=''; C_CYAN=''; C_NC=''
fi
log_info()  { printf "${C_CYAN}[INFO]${C_NC} %s\n" "$1"; }
log_ok()    { printf "${C_GREEN}[ OK ]${C_NC} %s\n" "$1"; }
log_warn()  { printf "${C_YELLOW}[WARN]${C_NC} %s\n" "$1"; }
log_err()   { printf "${C_RED}[FAIL]${C_NC} %s\n" "$1"; }

# ─── 默认值 ──────────────────────────────────────────────────────────────
AIRY_HOME="${AIRY_HOME:-$HOME/.airymaxrt}"
AIRY_REPO_URL="${AIRY_REPO_URL:-https://atomgit.com/openairymax/airymaxhub.git}"
# 版本 SSoT：优先读取同仓 agentrt/VERSION（源码树内运行），否则回退默认值。
# 注意：curl 管道 / 裸脚本场景无 VERSION 文件可读，默认值只作占位——
# 源码构建路径会以 clone 到的 agentrt/VERSION 为准（见 prepare_source），
# 二进制路径以 manifest/实际包版本为准（install_binary 固化）。
AIRY_VERSION_SPECIFIED=0
if [ -n "${AIRY_VERSION:-}" ]; then
    AIRY_VERSION_SPECIFIED=1
elif [ -f "$(dirname "$0")/../VERSION" ]; then
    AIRY_VERSION="v$(cat "$(dirname "$0")/../VERSION" | tr -d '[:space:]')"
fi
AIRY_VERSION="${AIRY_VERSION:-v0.1.5}"
AIRY_BUILD_JOBS="${AIRY_BUILD_JOBS:-$(nproc 2>/dev/null || echo 4)}"
AIRY_MODE="${AIRY_MODE:-auto}"
BIN_DIR="${BIN_DIR:-$HOME/.local/bin}"
UNINSTALL=0; KEEP_DATA=0; YES=0
# 出厂预装 maths-toolkit（数学计算后端：MCP-Mathematics + sympy-mcp，
# 共享 $AIRY_HOME/venv）。默认开启，安装失败降级警告，不阻断主流程。
WITH_MATHS=1
AIRY_PROFILE="${AIRY_PROFILE:-auto}"
# 发布通道（自更新器/二进制安装共用）：stable | beta。AIRY_RELEASE_URL
# 指向 manifest JSON 时按通道解析本平台制品；指向 tarball 时直用。
AIRY_CHANNEL="${AIRY_CHANNEL:-stable}"
AIRY_FROM_FILE="${AIRY_FROM_FILE:-}"
case "$AIRY_CHANNEL" in stable|beta) ;; *) log_err "非法 --channel: ${AIRY_CHANNEL}（支持 stable|beta）"; exit 1 ;; esac

AIRY_SRC_DIR="${AIRY_HOME}/src/airymaxhub"
MODULES_DIR="${AIRY_HOME}/modules"

# 源码子模块根：兼容两种仓库布局——
#   A) 平铺：$AIRY_SRC_DIR/agentrt、$AIRY_SRC_DIR/ecosystem、…
#   B) 管理仓 submodule：$AIRY_SRC_DIR/agent-workload/agentrt、…/ecosystem、…
# 统一以 $AIRY_SRC_APP 作为 app 源码根，后续引用全部基于该变量。
AIRY_SRC_APP="${AIRY_SRC_DIR}/agent-workload"
if [ ! -d "${AIRY_SRC_APP}/agentrt" ]; then
    AIRY_SRC_APP="${AIRY_SRC_DIR}"
fi

# 18 个 daemon 完整清单（安装后逐一校验，含 think_d/cupolas_d/maths_d；
# 与 agentrt-bootstrap.sh 的分层清单保持一致）
EXPECTED_DAEMONS="monit_d observe_d info_d notify_d sched_d channel_d mem_d
                  llm_d tool_d hook_d plugin_d agent_d a2a_d market_d gateway_d
                  think_d cupolas_d maths_d"

# ─── 参数解析 ────────────────────────────────────────────────────────────
while [ $# -gt 0 ]; do
    case "$1" in
        --prefix)    AIRY_HOME="$2"; shift 2 ;;
        --mode)      AIRY_MODE="$2"; shift 2 ;;
        --bin-dir)   BIN_DIR="$2"; shift 2 ;;
        --profile)   AIRY_PROFILE="$2"; shift 2 ;;
        --channel)   AIRY_CHANNEL="$2"; shift 2 ;;
        --from-file) AIRY_FROM_FILE="$2"; shift 2 ;;
        --uninstall) UNINSTALL=1; shift ;;
        --keep-data) KEEP_DATA=1; shift ;;
        --yes)       YES=1; shift ;;
        --with-maths)    WITH_MATHS=1; shift ;;
        --without-maths) WITH_MATHS=0; shift ;;
        --help|-h)   sed -n '2,52p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) log_err "未知参数: $1（--help 查看用法）"; exit 1 ;;
    esac
done

case "$AIRY_MODE" in auto|binary|hybrid|source) ;; *) log_err "非法 --mode: ${AIRY_MODE}"; exit 1 ;; esac
case "$AIRY_PROFILE" in auto|full|minimal) ;; *) log_err "非法 --profile: ${AIRY_PROFILE}（支持 full|minimal|auto）"; exit 1 ;; esac

# ─── 工具链检测 ──────────────────────────────────────────────────────────
require_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        log_err "缺少必要工具: $1"
        case "$1" in
            git)   log_warn "请安装 git（如 Debian/Ubuntu: sudo apt install git）" ;;
            cmake) log_warn "请安装 cmake ≥3.20（如: sudo apt install cmake）" ;;
            gcc|cc|clang) log_warn "请安装 C 编译器（如: sudo apt install build-essential）" ;;
            curl)  log_warn "请安装 curl" ;;
        esac
        exit 1
    fi
}

check_toolchain() {
    require_cmd curl
    if [ "${AIRY_NO_BUILD:-}" != "1" ]; then
        require_cmd git
        require_cmd cmake
        require_cmd make
        if ! command -v gcc >/dev/null 2>&1 && ! command -v clang >/dev/null 2>&1 && ! command -v cc >/dev/null 2>&1; then
            log_err "未找到 C 编译器（gcc/clang/cc）"; exit 1
        fi
        for lib in libcurl sqlite3; do
            pkg-config --exists "$lib" 2>/dev/null || \
                log_warn "未检测到 ${lib} 开发库，部分功能受限（建议安装 lib${lib}-dev）"
        done
    fi
}

# ─── 创建 AIRY_HOME 目录骨架 ───────────────────────────────────────────
init_home() {
    mkdir -p "${AIRY_HOME}"/bin "${AIRY_HOME}"/lib "${AIRY_HOME}"/include \
             "${AIRY_HOME}"/share "${AIRY_HOME}"/run \
             "${AIRY_HOME}"/config "${AIRY_HOME}"/data \
             "${AIRY_HOME}"/tmp \
             "${AIRY_HOME}"/data/agentrt/logs "${AIRY_HOME}"/data/agentrt/tmp \
             "${AIRY_HOME}"/data/agentrt/cache "${AIRY_HOME}"/data/agentrt/workspaces \
             "${AIRY_HOME}"/modules "${AIRY_HOME}"/scripts
    chmod 700 "${AIRY_HOME}/config" 2>/dev/null || true
    log_ok "AIRY_HOME 就绪: ${AIRY_HOME}"
}

# ─── 停止运行中的 daemon ────────────────────────────────────────────────
stop_daemons() {
    local bin="$1"
    [ -d "$bin" ] || return 0
    for d in ${EXPECTED_DAEMONS}; do
        [ -x "${bin}/${d}" ] && pkill -f "${bin}/${d}" >/dev/null 2>&1
    done
    sleep 1
}

# ─── 一键卸载 ────────────────────────────────────────────────────────────
do_uninstall() {
    local home="$1" keep_data="$2" yes="$3" env_file link size ans
    env_file="${home}/config/install.env"
    if [ -f "$env_file" ]; then
        home="$(sed -n 's/^AIRY_HOME=//p' "$env_file" | head -1)"
        [ -n "$home" ] || home="$1"
    fi
    if [ ! -d "$home" ]; then
        log_warn "未检测到安装（$home 不存在），无需卸载"
        return 0
    fi
    link="$(sed -n 's/^AIRY_BIN_LINK=//p' "$env_file" 2>/dev/null | head -1)"
    [ -n "$link" ] || link="${BIN_DIR}/airymaxrt"
    size="$(du -sh "$home" 2>/dev/null | cut -f1)"
    log_warn "将卸载 AirymaxRT：${home}（${size}）"
    if [ "$yes" != "1" ]; then
        printf "${C_YELLOW}确认卸载？[y/N] ${C_NC}"
        read -r ans || true
        case "$ans" in y|Y|yes|YES) ;; *) log_info "已取消卸载"; return 0 ;; esac
    fi
    stop_daemons "$home/bin"
    if [ "$keep_data" = "1" ] && [ -d "$home/data" ]; then
        rm -rf "$home"
        mkdir -p "$home/data"
        log_ok "已删除 ${home}（保留 data/ 记忆数据）"
    else
        rm -rf "$home"
        log_ok "已删除 ${home}"
    fi
    if [ -L "$link" ]; then
        rm -f "$link"
        log_ok "已移除启动器软链 ${link}"
    fi
    # 移除自动追加的 PATH 引导行（install.env 记录 rc 路径，带标记范围删除）
    rc_path="$(sed -n 's/^AIRY_PATH_RC=//p' "$env_file" 2>/dev/null | head -1)"
    if [ -n "$rc_path" ] && [ -f "$rc_path" ]; then
        if sed '\|# >>> AgentRT PATH bootstrap <<<|,\|# <<< AgentRT PATH bootstrap <<<|d' \
            "$rc_path" > "$rc_path.airy_tmp" 2>/dev/null && mv "$rc_path.airy_tmp" "$rc_path"; then
            log_ok "已从 ${rc_path} 移除 AgentRT PATH 引导行"
        else
            rm -f "$rc_path.airy_tmp" 2>/dev/null
        fi
    fi
    log_ok "卸载完成"
}

# ─── 方式 A：完全体二进制 tarball（优先） ───────────────────────────────
# 硬件自适应（2.3.5）：预编译包按架构分发——AIRY_RELEASE_URL 支持 {arch}
# 占位符（自动替换为当前架构，如 .../agentrt-v0.1.3-linux-{arch}.tar.gz）；
# 架构不在预编译支持清单时告警并回退源码构建，避免跨架构运行错乱。
# 2.3.7 发布通道：AIRY_RELEASE_URL 亦支持 manifest JSON（.../manifest.stable.json）
# ——下载后 GPG 验签（内置公钥）+ 按当前平台解析制品 url/sha256；本地离线包
# 可直接传 tarball 路径（--from-file / AIRY_FROM_FILE），仅做 sha256 + 架构自检。

# 官方发布 GPG 公钥（manifest 权威签名；与 tools/scripts/ci/release/keys/agentrt.asc
# 及 sdk/tui/scripts/airymaxrt AIRY_GPG_PUBKEY 同源，指纹见 keys/agentrt.fingerprint）
AIRY_GPG_PUBKEY='-----BEGIN PGP PUBLIC KEY BLOCK-----

mDMEao7uahYJKwYBBAHaRw8BAQdAk8Ou1tA2EfX5xZT4ET79YJESeqINPyFF86MK
cpPAQDO0NEFnZW50UlQgUmVsZWFzZSBTaWduaW5nIDxyZWxlYXNlQGFnZW50cnQu
YWlyeW1heC5pbz6IkwQTFgoAOxYhBIbDf3xc3cxA57s+YuQ19/HMJP+EBQJqju5q
AhsDBQsJCAcCAiICBhUKCQgLAgQWAgMBAh4HAheAAAoJEOQ19/HMJP+EepEBANYY
xAN1mQL4gulwMvH3xjiL6aEVm1PFjus33MXJrDmKAQDEck2sowTfLa1WneqUY93D
QpegwKdM5Y9YiANOL8FODQ==
=EPz8
-----END PGP PUBLIC KEY BLOCK-----'

# GPG 验签（独立 homedir 隔离用户 keyring；签名缺失拒绝——防供应链攻击
# 的 fail-closed 约束。公钥优先级：发布源 latest/keys/ 在线拉取（支持轮换）
# → 本地 $AIRY_HOME/keys/ → 内嵌公钥（首次引导兜底））。
verify_gpg_sig() { # <file> <sig.asc>
    [ -s "$2" ] || { log_warn "缺少签名文件（fail-closed），拒绝安装"; return 1; }
    local gnupg="${AIRY_HOME}/tmp/gnupg-install" keyf
    mkdir -p "$gnupg" && chmod 700 "$gnupg"
    keyf="$gnupg/agentrt.asc"
    if [ -z "${AIRY_NO_NETWORK:-}" ]; then
        curl -fsSL --max-time 30 -o "$keyf" \
            "https://raw.atomgit.com/${AIRY_RELEASE_OWNER:-openairymax/agentrt}/raw/main/latest/keys/agentrt.asc" >/dev/null 2>&1 || true
    fi
    if [ ! -s "$keyf" ] && [ -f "${AIRY_HOME}/keys/agentrt.asc" ]; then
        cp -f "${AIRY_HOME}/keys/agentrt.asc" "$keyf" 2>/dev/null || true
    fi
    [ -s "$keyf" ] || printf '%s\n' "$AIRY_GPG_PUBKEY" > "$keyf"
    gpg --batch --quiet --no-tty --homedir "$gnupg" --import "$keyf" >/dev/null 2>&1 || return 1
    gpg --batch --no-tty --homedir "$gnupg" --verify "$2" "$1" >/dev/null 2>&1
}

# manifest 字段提取（python3 优先，回退 sed 单行提取）
parse_manifest() { # <manifest> <platform> <field:url|sha256>
    if command -v python3 >/dev/null 2>&1; then
        python3 - "$1" "$2" "$3" <<'PYEOF'
import json, sys
try:
    m = json.load(open(sys.argv[1]))
    rel = m.get("releases", {}).get(m.get("latest", ""), {})
    print(rel.get("artifacts", {}).get(sys.argv[2], {}).get(sys.argv[3], ""))
except Exception:
    pass
PYEOF
        return 0
    fi
    sed -n "s/.*\"$3\"[[:space:]]*:[[:space:]]*\"\([^\"]*\)\".*/\1/p" "$1" | head -1
}

install_binary() {
    local url="$1" arch expect_sha=""
    local tarball="${AIRY_HOME}/tmp/agentrt-${AIRY_VERSION}.tar.gz"
    arch="$(detect_arch)"
    log_info "硬件架构: ${arch}（预编译支持: ${SUPPORTED_ARCHS}）"
    case " ${SUPPORTED_ARCHS} " in
        *" ${arch} "*) ;;
        *) log_warn "架构 ${arch} 无预编译包（支持: ${SUPPORTED_ARCHS}），回退源码构建"
           return 1 ;;
    esac

    # 来源解析：a) manifest JSON（通道）→ GPG 验签 + 解析本平台制品；
    #          b) 本地 tarball（--from-file / AIRY_FROM_FILE）→ 直用；
    #          c) 远程 tarball URL（{arch} 占位符）→ 下载，相邻 .sha256 自动校验
    if [ "${url##*.}" = "json" ]; then
        local man="${AIRY_HOME}/tmp/manifest.json" man_asc="${AIRY_HOME}/tmp/manifest.json.asc" plat
        curl -fsSL --max-time 60 -o "$man" "$url" || { log_warn "manifest 下载失败，回退源码构建"; return 1; }
        curl -fsSL --max-time 60 -o "$man_asc" "${url}.asc" >/dev/null 2>&1 || true
        verify_gpg_sig "$man" "$man_asc" || { log_warn "manifest 验签失败（GPG），拒绝安装"; return 1; }
        [ -s "$man_asc" ] && log_ok "manifest 验签通过（GPG）"
        plat="linux-${arch}"
        [ "$(uname -s 2>/dev/null)" = "Darwin" ] && plat="macos-$(uname -m 2>/dev/null)"
        url="$(parse_manifest "$man" "$plat" url)"
        expect_sha="$(parse_manifest "$man" "$plat" sha256)"
        [ -n "$url" ] || { log_warn "manifest 无 ${plat} 制品，回退源码构建"; return 1; }
        log_info "通道 ${AIRY_CHANNEL} 最新制品（${plat}）: $(basename "${url%%\?*}")"
    elif [ -f "$url" ]; then
        log_info "使用本地离线包: $url"
        tarball="$url"
    else
        # URL {arch} 占位符替换（POSIX sed，兼容 sh）
        url="$(printf '%s' "$url" | sed "s/{arch}/${arch}/g")"
    fi

    # 下载（仅远程来源）
    if [ ! -f "$tarball" ]; then
        log_info "下载完全体二进制包: ${url}"
        if ! curl -fsSL --max-time 600 -o "${tarball}" "${url}"; then
            log_warn "release 下载失败，回退源码构建"
            rm -f "${tarball}"
            return 1
        fi
    fi
    # sha256 校验（manifest 期望值，或相邻 .sha256 文件）
    if [ -z "$expect_sha" ] && [ -f "${tarball}.sha256" ]; then
        expect_sha="$(cut -d' ' -f1 "${tarball}.sha256" 2>/dev/null)"
    fi
    if [ -n "$expect_sha" ]; then
        if printf '%s  %s\n' "$expect_sha" "$tarball" | sha256sum -c - >/dev/null 2>&1; then
            log_ok "sha256 校验通过"
        else
            log_err "sha256 校验失败，拒绝安装"
            rm -f "$tarball"
            return 1
        fi
    fi
    # 包内架构自校验：tarball 根含 platform-<arch> 标识文件时交叉校验，
    # 防止下载到异架构包后静默安装（跨架构 daemon 启动即崩溃）。
    if tar -tzf "${tarball}" 2>/dev/null | grep -q "platform-${arch}"; then
        log_ok "二进制包架构校验通过（${arch}）"
    elif tar -tzf "${tarball}" 2>/dev/null | grep -qE 'platform-(x86_64|aarch64|armv7l|riscv64)'; then
        log_err "二进制包架构与当前主机（${arch}）不匹配，拒绝安装"
        rm -f "${tarball}"
        return 1
    fi
    tar -xzf "${tarball}" -C "${AIRY_HOME}/tmp"
    local extracted
    extracted="$(find "${AIRY_HOME}/tmp" -maxdepth 1 -type d -name 'agentrt-*' | head -1)"
    [ -n "$extracted" ] || { log_warn "release 包结构异常，回退源码构建"; return 1; }
    cp -f "${extracted}"/bin/* "${AIRY_HOME}/bin/" 2>/dev/null || true
    cp -f "${extracted}"/lib/* "${AIRY_HOME}/lib/" 2>/dev/null || true
    cp -rf "${extracted}"/include/* "${AIRY_HOME}/include/" 2>/dev/null || true
    # LICENSE/README（share/licenses|share/doc）随包分发，满足许可证随二进制分发要求
    cp -rf "${extracted}"/share/* "${AIRY_HOME}/share/" 2>/dev/null || true
    # 二进制包内置配置（secrets.env.example / agentrt.yaml / model.yaml）拷入 config/
    if [ -d "${extracted}/config" ]; then
        cp -f "${extracted}"/config/* "${AIRY_HOME}/config/" 2>/dev/null || true
    fi
    # 签名公钥随包同步（自更新器/下次安装复用）
    if [ -f "${extracted}/keys/agentrt.asc" ]; then
        mkdir -p "${AIRY_HOME}/keys"
        cp -f "${extracted}/keys/agentrt.asc" "${AIRY_HOME}/keys/" 2>/dev/null || true
    fi
    # 以实际安装包版本固化（manifest 通道可能高于默认 AIRY_VERSION）
    local ver_num
    ver_num="$(basename "$extracted" | sed 's/^agentrt-//')"
    [ -n "$ver_num" ] && AIRY_VERSION="v${ver_num}"
    log_ok "完全体二进制包安装完成（v${ver_num:-${AIRY_VERSION}}）"
    return 0
}

# ─── 闭源预编译模块下载（模式 B） ───────────────────────────────────────
fetch_prebuilt_module() {
    # fetch_prebuilt_module <name> <url> <解压后目录名>
    local name="$1" url="$2" dirname="$3" dest="${MODULES_DIR}/${dirname}"
    [ -n "$url" ] || { log_warn "未配置 ${name} 预编译包 URL，跳过"; return 1; }
    if [ -d "$dest" ]; then log_ok "${name} 预编译模块已就位"; return 0; fi
    log_info "下载闭源预编译模块 ${name}…"
    local tarball="${AIRY_HOME}/tmp/${dirname}.tar.gz"
    curl -fsSL --max-time 600 -o "$tarball" "$url" || { log_warn "${name} 下载失败"; return 1; }
    mkdir -p "$dest"
    tar -xzf "$tarball" -C "$dest" || { log_warn "${name} 解压失败"; return 1; }
    log_ok "${name} 预编译模块就位: ${dest}"
    return 0
}

# ─── 源码获取（模式 B/C） ───────────────────────────────────────────────
prepare_source() {
    if [ ! -d "${AIRY_SRC_DIR}/.git" ]; then
        log_info "git 拉取 airymaxhub（${AIRY_REPO_URL}）…"
        mkdir -p "$(dirname "${AIRY_SRC_DIR}")"
        # 版本来源二选一：
        #   - 用户显式指定 AIRY_VERSION → 固定 tag 精确安装（可复现/回滚）；
        #   - 未指定（curl 管道等无 VERSION 场景）→ clone 默认分支，随后从
        #     agentrt/VERSION 读取真实版本（SSoT 单一来源），杜绝 install.sh
        #     内置默认版本与当前发布漂移导致 clone 到不存在/过期 tag。
        if [ "$AIRY_VERSION_SPECIFIED" = "1" ]; then
            git clone --depth 1 -b "${AIRY_VERSION}" "${AIRY_REPO_URL}" "${AIRY_SRC_DIR}" \
                || { log_err "git 拉取失败（若子仓私有，请配置 AIRY_RELEASE_URL 走二进制模式）"; exit 1; }
        else
            git clone --depth 1 "${AIRY_REPO_URL}" "${AIRY_SRC_DIR}" \
                || { log_err "git 拉取失败（若子仓私有，请配置 AIRY_RELEASE_URL 走二进制模式）"; exit 1; }
        fi
        # --recursive：agentrt 的 7 个核心子仓（atoms/commons/daemons/gateway/
        # cupolas/protocols/heapstore）与 sdk/ecosystem 子仓均为公开仓，必须
        # 一并拉取，否则模式 C 源码构建缺核心源码必然失败。闭源子仓
        # （closed-docs / closed-dev-build / memoryrovol 标 update=none）自动跳过。
        git -C "${AIRY_SRC_DIR}" submodule update --init --recursive --depth 1 2>/dev/null || \
            log_warn "部分子仓拉取受限（闭源模块将由预编译包补齐）"
    else
        log_info "airymaxhub 源码已存在，复用本地源码树"
        git -C "${AIRY_SRC_DIR}" fetch --all --tags --depth 1 >/dev/null 2>&1 || true
    fi

    # 源码版本 SSoT：以 agentrt/VERSION 为权威（重探测布局：管理仓 submodule
    # 或平铺两种布局）。
    if [ -d "${AIRY_SRC_APP}/agentrt" ]; then :; else AIRY_SRC_APP="${AIRY_SRC_DIR}"; fi
    local real_ver
    real_ver="$(cat "${AIRY_SRC_APP}/agentrt/VERSION" 2>/dev/null | tr -d '[:space:]')"
    if [ -n "$real_ver" ]; then
        AIRY_VERSION="v${real_ver}"
        log_ok "源码版本（SSoT）: ${AIRY_VERSION}"
    fi
    log_ok "源码就绪: ${AIRY_SRC_DIR}"
}

# ─── 构建（模式 B/C 共用） ──────────────────────────────────────────────
build_and_install() {
    local build_dir="${AIRY_HOME}/build"
    local cmake_args="-DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -DENABLE_SANITIZERS=OFF -DCMAKE_INSTALL_PREFIX=${AIRY_HOME}"

    # 闭源模块预编译路径注入（模式 B）
    if [ -d "${MODULES_DIR}/atoms" ]; then
        cmake_args="${cmake_args} -DAIRY_ATOMS_PREBUILT_DIR=${MODULES_DIR}/atoms"
    fi
    # 预编译库文件名对齐真实归档名（target agentrt_memoryrovol →
    # libagentrt_memoryrovol.a），与 install.ps1 及 products/memoryrovol 一致
    if [ -f "${MODULES_DIR}/memoryrovol/libagentrt_memoryrovol.a" ]; then
        cmake_args="${cmake_args} -DMEMORYROVOL_PRO_LIB=${MODULES_DIR}/memoryrovol/libagentrt_memoryrovol.a"
    fi

    log_info "cmake 配置（${cmake_args}）…"
    cmake -S "${AIRY_SRC_APP}/agentrt" -B "${build_dir}" ${cmake_args} \
        || { log_err "cmake 配置失败"; exit 1; }
    log_info "构建（-j${AIRY_BUILD_JOBS}）…"
    cmake --build "${build_dir}" -j"${AIRY_BUILD_JOBS}" || { log_err "构建失败"; exit 1; }
    log_info "安装到 ${AIRY_HOME}…"
    cmake --install "${build_dir}" || true
    if [ -d "${build_dir}/bin" ]; then
        cp -f "${build_dir}"/bin/* "${AIRY_HOME}/bin/" 2>/dev/null || true
    fi
    log_ok "源码构建安装完成"
}

# ─── Python 依赖安装 → lib/ ────────────────────────────────────────────
install_python_deps() {
    log_info "安装 Python 依赖到 ${AIRY_HOME}/lib …"
    local pkg
    for pkg in airymax_agents airymax_agents_rs orchestration; do
        [ -d "${AIRY_SRC_APP}/ecosystem/agents/${pkg}" ] || { log_warn "跳过: ecosystem/agents/${pkg}"; continue; }
        rsync -a --exclude tests --exclude __pycache__ --exclude .git --exclude examples \
            "${AIRY_SRC_APP}/ecosystem/agents/${pkg}" "${AIRY_HOME}/lib/" 2>/dev/null \
            || cp -r "${AIRY_SRC_APP}/ecosystem/agents/${pkg}" "${AIRY_HOME}/lib/"
    done
    if [ -d "${AIRY_SRC_APP}/sdk/sdk-python/agentrt" ]; then
        rsync -a --exclude tests --exclude __pycache__ --exclude .git \
            "${AIRY_SRC_APP}/sdk/sdk-python/agentrt" "${AIRY_HOME}/lib/" 2>/dev/null \
            || cp -r "${AIRY_SRC_APP}/sdk/sdk-python/agentrt" "${AIRY_HOME}/lib/"
    fi
    if command -v python3 >/dev/null 2>&1; then
        if PYTHONPATH="${AIRY_HOME}/lib" python3 -c "import agentrt, airymax_agents, orchestration" 2>/dev/null; then
            log_ok "Python 依赖可导入 (agentrt/airymax_agents/orchestration)"
        else
            log_warn "lib/ 导入校验失败（检查源码包结构）"
        fi
    fi
}

# ─── 出厂预装 maths-toolkit（数学计算后端） ─────────────────────────────
# MCP-Mathematics + sympy-mcp 组合，共享 $AIRY_HOME/venv，默认不装
# einsteinpy。python3 缺失或安装失败时降级警告（maths_d 纯 C 快速路径
# 仍可用），不阻断 agentrt 主流程。--without-maths 可跳过。
install_maths_toolkit() {
    if [ "$WITH_MATHS" != "1" ]; then
        log_info "已跳过 maths-toolkit（--without-maths）"
        return 0
    fi
    local toolkit="${AIRY_SRC_APP}/ecosystem/markets/tools/maths-toolkit/install.sh"
    if [ ! -f "$toolkit" ]; then
        log_warn "maths-toolkit 安装器不存在（${toolkit}），跳过数学后端预装"
        return 0
    fi
    if ! command -v python3 >/dev/null 2>&1; then
        log_warn "未找到 python3，跳过 maths-toolkit（maths_d 纯 C 快速路径可用）"
        return 0
    fi
    log_info "出厂预装数学计算后端（MCP-Mathematics + sympy-mcp，共享 venv，清华源）…"
    if sh "$toolkit" --airy-home "${AIRY_HOME}" >/dev/null 2>&1; then
        log_ok "maths-toolkit 安装完成（maths_d 符号计算后端已就绪）"
    else
        log_warn "maths-toolkit 安装失败（网络/依赖问题），已降级：maths_d 纯 C 快速路径可用；"
        log_warn "可稍后手动安装: sh ${toolkit} --airy-home ${AIRY_HOME}"
    fi
}

# ─── MemoryRovol OSS 库构建（TUI 独立链接用，源码模式） ───────────────
# TUI 是独立 Rust 二进制，无法链接 PRO 库（依赖 agentrt 运行时符号）；
# 本地持有 memoryrovol 源码（模式 C）时以 OSS 模式（L1+L2）编译并部署为
# $AIRY_HOME/lib/libagentrt_memoryrovol_oss.a，TUI build.rs 才会选中它
#（2.6：本地源码构建 memoryrovol 全功能开启）。无源码则跳过，
# TUI 降级 JsonlMemory（真实可用后备）。
build_mr_oss() {
    local mr_src="${AIRY_SRC_APP}/products/memoryrovol"
    [ -d "$mr_src" ] || { log_warn "products/memoryrovol 源码缺失，跳过 OSS 库构建（TUI 降级 JsonlMemory）"; return 0; }
    local mr_build="${AIRY_HOME}/build-mr-oss"
    log_info "构建 MemoryRovol OSS 库（L1+L2，TUI 独立链接）…"
    cmake -S "$mr_src" -B "$mr_build" \
        -DCMAKE_BUILD_TYPE=Release -DMEMORYROVOL_OSS=ON -DBUILD_TESTS=OFF \
        || { log_warn "memoryrovol OSS cmake 配置失败，TUI 降级 JsonlMemory"; return 0; }
    cmake --build "$mr_build" -j"${AIRY_BUILD_JOBS}" 2>&1 | tail -5 \
        || { log_warn "memoryrovol OSS 构建失败，TUI 降级 JsonlMemory"; return 0; }
    local oss_lib="$mr_build/src/libagentrt_memoryrovol.a"
    if [ -f "$oss_lib" ]; then
        cp -f "$oss_lib" "${AIRY_HOME}/lib/libagentrt_memoryrovol_oss.a"
        log_ok "MemoryRovol OSS 库就位: ${AIRY_HOME}/lib/libagentrt_memoryrovol_oss.a"
    else
        log_warn "OSS 库产物缺失（${oss_lib}），TUI 降级 JsonlMemory"
    fi
}

# ─── Rust TUI 构建（源码模式附带） ─────────────────────────────────────
build_tui() {
    [ -d "${AIRY_SRC_APP}/sdk/tui" ] || return 0
    # AIRY_HOME 需 export 给 cargo 子进程：TUI build.rs 据此定位
    # $AIRY_HOME/lib/libagentrt_memoryrovol.a（TUI memoryrovol 全功能链接）。
    export AIRY_HOME
    if ! command -v cargo >/dev/null 2>&1 && [ -x "$HOME/.cargo/bin/cargo" ]; then
        export PATH="$HOME/.cargo/bin:$PATH"
    fi
    command -v cargo >/dev/null 2>&1 || { log_warn "cargo 不可用，跳过 agentrt-tui"; return 0; }
    # 构建产物收敛（铁律 4.7）：CARGO_TARGET_DIR 重定向到 $AIRY_HOME/target，
    # 禁止 cargo 在源码树 sdk/tui/target 落盘（曾有 1.7G 泄漏）。
    export CARGO_TARGET_DIR="${AIRY_HOME}/target"
    log_info "构建 agentrt-tui（Rust TUI，产物 → ${CARGO_TARGET_DIR}）…"
    ( cd "${AIRY_SRC_APP}/sdk/tui" && cargo build --release ) 2>/dev/null || { log_warn "TUI 构建失败，跳过"; return 0; }
    [ -f "${CARGO_TARGET_DIR}/release/agentrt-tui" ] && \
        cp -f "${CARGO_TARGET_DIR}/release/agentrt-tui" "${AIRY_HOME}/bin/agentrt-tui"
    log_ok "agentrt-tui 部署完成"
}

# ─── CLI 兼容入口：Rust TUI 缺失时用 C airy_cli 提供 agentrt-tui ──────
# airymaxrt 启动器通过 TUI 可执行文件进入交互界面；当 Rust TUI 未构建
# （cargo 缺失 / 构建失败 / 二进制包未含）时，以 C 实现的 airy_cli 作为
# agentrt-tui 兼容入口，保证 `airymaxrt` 始终可用。包装脚本 source
# install-pinned 的 agentrt-env.sh，导出 AIRY_HOME 等，使 CLI 能连接已
# 安装的 daemon（不受调用 shell 的环境变量影响）。
ensure_cli_entry() {
    [ -x "${AIRY_HOME}/bin/agentrt-tui" ] && return 0
    if [ -x "${AIRY_HOME}/bin/airy_cli" ]; then
        cat > "${AIRY_HOME}/bin/agentrt-tui" <<'TUIEOF'
#!/bin/sh
# SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
# agentrt-tui compat entry for the C airy_cli (used when the Rust TUI is
# not built). Sources the install-pinned environment so the CLI reaches
# the installed daemons regardless of the calling shell.
_DIR="$(cd -P "$(dirname "$0")" && pwd)"
# shellcheck disable=SC1091
. "$_DIR/agentrt-env.sh"
exec "$_DIR/airy_cli" "$@"
TUIEOF
        chmod 755 "${AIRY_HOME}/bin/agentrt-tui"
        log_ok "agentrt-tui 使用 C airy_cli 兼容入口（Rust TUI 未构建）"
    else
        log_warn "agentrt-tui 与 airy_cli 均缺失"
    fi
}

# ─── secrets.env 模板 ──────────────────────────────────────────────────
init_secrets() {
    local secrets="${AIRY_HOME}/config/secrets.env"
    if [ ! -f "${secrets}" ]; then
        local template="${AIRY_SRC_DIR}/tools/scripts/ops/templates/secrets.env.example"
        # 二进制模式无源码树：回退到随包分发的 config/secrets.env.example
        [ -f "${template}" ] || template="${AIRY_HOME}/config/secrets.env.example"
        if [ -f "${template}" ]; then
            cp "${template}" "${secrets}"
            chmod 600 "${secrets}"
            log_warn "已生成 ${secrets}，请填写 LLM API key"
        else
            log_warn "未找到 secrets.env 模板，跳过"
        fi
    else
        log_ok "secrets.env 已存在，跳过"
    fi
    [ -f "${AIRY_SRC_APP}/ecosystem/manager/configs/agentrt.yaml" ] && \
        cp -f "${AIRY_SRC_APP}/ecosystem/manager/configs/agentrt.yaml" "${AIRY_HOME}/config/" 2>/dev/null || true
    [ -f "${AIRY_SRC_APP}/ecosystem/manager/model/model.yaml" ] && \
        cp -f "${AIRY_SRC_APP}/ecosystem/manager/model/model.yaml" "${AIRY_HOME}/config/" 2>/dev/null || true
    # 工具级权限规则（fail-closed：缺文件时 tool_d/agent_d 拒绝全部工具调用）。
    # 模板授予 coding_v1 标准编码工具集；生产部署应按最小权限裁剪。
    # 权威路径 $AIRY_HOME/config/cupolas/permission_rules.yaml
    # （daemon_cupolas_bootstrap.c 启动时读取，缺 cupolas 才回退
    #  $AIRY_HOME/config/permission_rules.yaml 兼容旧部署）。
    # 注意：模板是 SSoT，必须每次覆盖（不跳过已存在文件），否则模板演进
    # （如新增工具授权 fs_delete）无法随重装生效，造成运行时 ACL 陈旧。
    local rules_tpl="${AIRY_SRC_DIR}/tools/scripts/ops/templates/permission_rules.yaml"
    [ -f "${rules_tpl}" ] || rules_tpl="${AIRY_HOME}/config/permission_rules.yaml"
    if [ -f "${rules_tpl}" ]; then
        mkdir -p "${AIRY_HOME}/config/cupolas"
        cp -f "${rules_tpl}" "${AIRY_HOME}/config/cupolas/permission_rules.yaml"
        chmod 600 "${AIRY_HOME}/config/cupolas/permission_rules.yaml"
        log_ok "已部署工具权限规则 ${AIRY_HOME}/config/cupolas/permission_rules.yaml"
    else
        log_warn "未找到 permission_rules.yaml 模板，工具调用将 fail-closed 拒绝"
    fi
}

# ─── PATH 自动引导（2.3.2.6 增强，2026-08-24） ────────────────────────
# 根因：默认 BIN_DIR=~/.local/bin 在多数新系统不在 PATH——安装器此前只
# 提示"请手动 export"，用户不执行就永远 command not found（只能完整路径
# 启动）。彻底解决：安装时自动幂等追加 BIN_DIR 到当前 shell 的 rc 文件
# （带标记行，可卸载移除），并固化 rc 路径到 install.env 供卸载与
# airymaxrt 启动器自愈使用；rc 不可写时回退提示。
path_rc_file() {
    case "$(basename "${SHELL:-/bin/sh}")" in
        zsh) echo "$HOME/.zshrc" ;;
        fish) echo "$HOME/.config/fish/config.fish" ;;
        sh|dash|ash) echo "$HOME/.profile" ;;
        *) echo "$HOME/.bashrc" ;;
    esac
}

# 返回 0 已引导（rc 含标记行或成功追加）；1 追加失败（已提示手动命令）。
path_bootstrap() {
    local rc
    rc="$(path_rc_file)"
    if [ -f "$rc" ] && grep -q '# >>> AgentRT PATH bootstrap <<<' "$rc" 2>/dev/null; then
        log_info "PATH 引导: $rc 已包含 AgentRT PATH 行（幂等跳过）"
        echo "AIRY_PATH_RC=$rc" >> "${AIRY_HOME}/config/install.env"
        echo "AIRY_PATH_APPENDED=yes" >> "${AIRY_HOME}/config/install.env"
        return 0
    fi
    local line
    if [ "$(basename "$rc")" = "config.fish" ]; then
        line="set -gx PATH \"${BIN_DIR}\" \$PATH"
    else
        line="export PATH=\"${BIN_DIR}:\$PATH\""
    fi
    if { printf '\n# >>> AgentRT PATH bootstrap <<<\n%s\n# <<< AgentRT PATH bootstrap <<<\n' "$line" ; } >> "$rc" 2>/dev/null; then
        log_ok "PATH 引导: 已自动追加 ${BIN_DIR} 到 $rc（新开终端生效，或执行 source \"$rc\"）"
        echo "AIRY_PATH_RC=$rc" >> "${AIRY_HOME}/config/install.env"
        echo "AIRY_PATH_APPENDED=yes" >> "${AIRY_HOME}/config/install.env"
        return 0
    fi
    log_warn "PATH 引导: 无法写入 $rc（自动追加失败），请手动执行:"
    log_warn "  echo '${line}' >> \"$rc\" && source \"$rc\""
    echo "AIRY_PATH_APPENDED=no" >> "${AIRY_HOME}/config/install.env"
    return 1
}

# ─── 硬件评估与画像固化（2.3.5/2.3.6 硬件自适应裁剪） ──────────────
# 与 airymaxrt 启动器 assess_hardware/detect_accel/detect_arch 同口径
# （SSoT 单一判据，见 sdk/tui/scripts/airymaxrt）：
#   minimal：MemTotal < 2.5GiB 或 MemAvailable < 1.5GiB 或 CPU 核数 < 3
#     （端侧/低配设备：树莓派 4B 2GB 等，启动器仅拉起 llm/think/agent/tool
#     核心 daemon，其余能力 daemon 裁剪，gateway 自动降级，避免 OOM）
#   full：资源充足（大型服务器/个人电脑）
# 加速器探测（nvidia-smi / rocm-smi / /dev/dri）记录到画像——为本地推理
# 能力判定预留依据；airymaxrt monitor 在检测到外设增强（插卡/扩容）时
# 自动恢复被裁剪 daemon（见 sdk/tui/scripts/airymaxrt）。
# 架构检测（uname -m 归一化）：二进制模式按架构选择预编译包
# （AIRY_RELEASE_URL 支持 {arch} 占位符），并固化到画像供后续校验。
detect_arch() {
    case "$(uname -m 2>/dev/null)" in
        x86_64|amd64)     echo "x86_64" ;;
        aarch64|arm64)    echo "aarch64" ;;
        armv7l|armv6l|armhf) echo "armv7l" ;;
        riscv64)          echo "riscv64" ;;
        *)                echo "unknown" ;;
    esac
}
# 预编译包支持的架构清单（binary 模式校验；其余架构回退源码构建）
# 与 CI release.yml build-riscv64 job（agentrt-<v>-linux-riscv64.tar.gz）
# 及 sdk/tui/scripts/airymaxrt detect_arch 同口径。
SUPPORTED_ARCHS="x86_64 aarch64 armv7l riscv64"

detect_accel() {
    if command -v nvidia-smi >/dev/null 2>&1 && nvidia-smi -L >/dev/null 2>&1; then
        echo "nvidia:$(nvidia-smi -L 2>/dev/null | wc -l)"
    elif command -v rocm-smi >/dev/null 2>&1; then
        echo "rocm"
    elif [ -d /dev/dri ] && ls /dev/dri/renderD* >/dev/null 2>&1; then
        echo "dri"
    else
        echo "none"
    fi
}

assess_hardware() {
    local mem_kib mem_avail_kib nproc_val accel profile
    # 跨平台内存探测：Linux 读 /proc/meminfo；macOS/BSD 无 /proc，回退
    # sysctl（hw.memsize 单位字节 → KiB）。此前仅 /proc 导致 macOS 恒判
    # minimal（非致命但画像错误，2026-08-25 修复）。
    if [ -r /proc/meminfo ]; then
        mem_kib="$(awk '/^MemTotal:/{print $2}' /proc/meminfo 2>/dev/null || echo 0)"
        mem_avail_kib="$(awk '/^MemAvailable:/{print $2}' /proc/meminfo 2>/dev/null || echo "${mem_kib:-0}")"
    elif command -v sysctl >/dev/null 2>&1; then
        mem_kib="$(( $(sysctl -n hw.memsize 2>/dev/null || echo 0) / 1024 ))"
        mem_avail_kib="$mem_kib"
    else
        mem_kib=0
        mem_avail_kib=0
    fi
    # 跨平台 CPU 核数：nproc（GNU）→ sysctl hw.ncpu（macOS/BSD）→ 1
    if command -v nproc >/dev/null 2>&1; then
        nproc_val="$(nproc 2>/dev/null || echo 1)"
    elif command -v sysctl >/dev/null 2>&1; then
        nproc_val="$(sysctl -n hw.ncpu 2>/dev/null || echo 1)"
    else
        nproc_val=1
    fi
    accel="$(detect_accel)"
    if [ -n "$mem_kib" ] && [ "$mem_kib" -gt 0 ] && \
       [ "$mem_kib" -ge $((2560 * 1024)) ] && \
       [ "$mem_avail_kib" -ge $((1536 * 1024)) ] && \
       [ "$nproc_val" -ge 3 ]; then
        profile="full"
    else
        profile="minimal"
    fi
    printf '%s|%s|%s|%s|%s' "$profile" "${mem_kib:-0}" "${mem_avail_kib:-0}" "$nproc_val" "$accel"
}

# 固化运行画像到 $AIRY_HOME/config/profile.env（airymaxrt 启动器启动时
# 优先读取，见 sdk/tui/scripts/airymaxrt PROFILE_ENV）。install.env 保持
# 只读（安装信息），画像允许被 `airymaxrt profile` / monitor 跨会话调整。
persist_profile() {
    local hw hw_profile mem total avail cores accel
    hw="$(assess_hardware)"
    hw_profile="${hw%%|*}"
    mem="${hw#*|}"
    total="${mem%%|*}"; mem="${mem#*|}"
    avail="${mem%%|*}"; mem="${mem#*|}"
    cores="${mem%%|*}"
    accel="${mem#*|}"
    # auto 画像以硬件评估为准；显式 --profile 尊重用户选择
    if [ "$AIRY_PROFILE" != "auto" ]; then
        hw_profile="$AIRY_PROFILE"
    fi
    mkdir -p "${AIRY_HOME}/config"
    {
        echo "# AgentRT 运行画像（由 install.sh 生成，airymaxrt 启动器读取）"
        echo "AIRY_PROFILE=${hw_profile}"
        echo "AIRY_HW_ARCH=$(detect_arch)"
        echo "AIRY_HW_MEM_TOTAL_KIB=${total}"
        echo "AIRY_HW_MEM_AVAIL_KIB=${avail}"
        echo "AIRY_HW_CPU_CORES=${cores}"
        echo "AIRY_HW_ACCEL=${accel}"
    } > "${AIRY_HOME}/config/profile.env"
    chmod 600 "${AIRY_HOME}/config/profile.env" 2>/dev/null || true
    log_ok "运行画像已固化: ${hw_profile}（${AIRY_HW_ARCH:-$(detect_arch)} · 内存 ${total}KiB/可用 ${avail}KiB · CPU ${cores} 核 · 加速器 ${accel}）"
    log_info "  硬件变化后（内存扩容/插入显卡）执行 'airymaxrt profile' 重评估，"
    log_info "  或 'airymaxrt monitor --daemon' 后台常驻自动恢复被裁剪功能"
}

# ─── 固化安装位置 + 生成运行环境 + 启动器软链 ──────────────────────────
finalize_install() {
    # 生成 vault 主密钥口令（AES-256-GCM 凭据加密）。随机强口令，缺失回退链：
    # openssl rand → od /dev/urandom（POSIX：BSD od 亦支持 -N，替代 GNU
    # head -c）→ 时间戳+urandom 哈希 cksum（POSIX，替代 Linux 特有 sha256sum）。
    local vault_password
    vault_password=$(openssl rand -hex 32 2>/dev/null \
        || { od -An -tx1 -N32 /dev/urandom 2>/dev/null | tr -d ' \n'; })
    if [ -z "${vault_password}" ]; then
        vault_password="$( { date '+%s%N'; od -An -tx1 -N64 /dev/urandom 2>/dev/null; } | cksum | cut -c1-64 )"
    fi
    {
        echo "# AgentRT 安装信息（由 install.sh 生成，勿手改）"
        echo "AIRY_HOME=${AIRY_HOME}"
        echo "AIRY_VERSION=${AIRY_VERSION}"
        echo "AIRY_CHANNEL=${AIRY_CHANNEL}"
        echo "AIRY_BIN_LINK=${BIN_DIR}/airymaxrt"
        echo "INSTALLED_AT=$(date -Is 2>/dev/null || date '+%Y-%m-%dT%H:%M:%S%z')"
        echo "AIRY_VAULT_PASSWORD=${vault_password}"
    } > "${AIRY_HOME}/config/install.env"
    chmod 600 "${AIRY_HOME}/config/install.env"

    # 引号 heredoc（<<'EOF'）：dash + set -u 下 \${...} 在无引号 heredoc 中会被
    # 错误展开（"AIRY_CONFIG_DIR: parameter not set" 中止写入，agentrt-env.sh
    # 变空文件——2026-08-25 实测复现）。引号 heredoc 杜绝一切展开，AIRY_HOME
    # 实际值用占位符 __AIRY_HOME__ + sed 注入。
    cat > "${AIRY_HOME}/bin/agentrt-env.sh" <<'AIRY_ENV_EOF'
#!/bin/sh
# AgentRT 运行环境（由 install.sh 生成，source 使用）
# AIRY_HOME 以调用方显式设置优先（隔离测试/多实例/--prefix 自定义安装），
# 缺省回退安装时固化值；无条件 export 会覆盖显式设置，导致 daemon 群与
# 调用方等待探测的 socket 目录分叉（历史故障：socket 未就绪 + 第二实例
# 抢占生产 socket）。
AIRY_HOME="${AIRY_HOME:-__AIRY_HOME__}"
export AIRY_HOME
export AIRY_RUNTIME_DIR="${AIRY_RUNTIME_DIR:-$AIRY_HOME/run}"
# 运行时数据全量统一于 $AIRY_HOME/data/agentrt（2026-08-25）：日志/缓存/
# 临时/持久化工作区均收敛其下，顶层仅保留分发物、用户配置与易失 run/。
export AIRY_LOG_DIR="${AIRY_LOG_DIR:-$AIRY_HOME/data/agentrt/logs}"
export AIRY_CONFIG_DIR="${AIRY_CONFIG_DIR:-$AIRY_HOME/config}"
export AIRY_BIN_DIR="${AIRY_BIN_DIR:-$AIRY_HOME/bin}"
export AIRY_LIB_DIR="${AIRY_LIB_DIR:-$AIRY_HOME/lib}"
export AIRY_DATA_DIR="${AIRY_DATA_DIR:-$AIRY_HOME/data}"
export AIRY_CACHE_DIR="${AIRY_CACHE_DIR:-$AIRY_HOME/data/agentrt/cache}"
export AIRY_TMP_DIR="${AIRY_TMP_DIR:-$AIRY_HOME/data/agentrt/tmp}"
export AIRY_WORKSPACE_DIR="${AIRY_WORKSPACE_DIR:-$AIRY_HOME/data/agentrt/workspaces}"
# Python 字节码缓存收敛：editable 安装的包源码位于源码区，PYTHONPYCACHEPREFIX
# 将所有 __pycache__ 重定向到 $AIRY_HOME/data/agentrt/cache/pycache，禁止落盘源码区。
export PYTHONPYCACHEPREFIX="${PYTHONPYCACHEPREFIX:-$AIRY_HOME/data/agentrt/cache/pycache}"
# Agent 工具 ACL：默认不设（fail-closed，以 $AIRY_CONFIG_DIR/permission_rules.yaml
# 为唯一权威源，按角色最小权限授权）。高级部署可显式覆盖收紧，如
# AIRY_AGENT_ACL="coding_v1=fs_read,fs_glob"。
export AIRY_AGENT_ACL="${AIRY_AGENT_ACL:-}"
export PATH="${AIRY_HOME}/bin:$PATH"
AIRY_ENV_EOF
    sed "s|__AIRY_HOME__|${AIRY_HOME}|g" "${AIRY_HOME}/bin/agentrt-env.sh" > "${AIRY_HOME}/bin/agentrt-env.sh.tmp" && \
        mv "${AIRY_HOME}/bin/agentrt-env.sh.tmp" "${AIRY_HOME}/bin/agentrt-env.sh"
    chmod 700 "${AIRY_HOME}/bin/agentrt-env.sh"

    # 启动器软链：任意路径输入 airymaxrt 即启动（读 install.env 定位运行时根）
    if [ -f "${AIRY_SRC_APP}/sdk/tui/scripts/airymaxrt" ]; then
        cp -f "${AIRY_SRC_APP}/sdk/tui/scripts/airymaxrt" "${AIRY_HOME}/bin/airymaxrt"
        chmod 755 "${AIRY_HOME}/bin/airymaxrt"
    elif [ -f "${AIRY_HOME}/bin/agentrt-tui" ]; then
        # 二进制模式无源码：生成轻量启动器。
        # 从启动器自身位置推导 AIRY_HOME（含软链解析，POSIX 兼容），
        # 使 BIN_DIR 软链在未 export AIRY_HOME 的任意 shell 中也可用。
        cat > "${AIRY_HOME}/bin/airymaxrt" <<EOF
#!/bin/sh
_SELF="\$0"
while [ -L "\$_SELF" ]; do
    _LINK="\$(readlink "\$_SELF")"
    case "\$_LINK" in
        /*) _SELF="\$_LINK" ;;
        *)  _SELF="\$(dirname "\$_SELF")/\$_LINK" ;;
    esac
done
_DIR="\$(cd -P "\$(dirname "\$_SELF")" && pwd)"
AIRY_HOME="\${AIRY_HOME:-\$(sed -n 's/^AIRY_HOME=//p' "\${_DIR}/../config/install.env" 2>/dev/null | head -1)}"
AIRY_HOME="\${AIRY_HOME:-\${_DIR}/..}"
export AIRY_HOME
exec "\$AIRY_HOME/bin/agentrt-tui" "\$@"
EOF
        chmod 755 "${AIRY_HOME}/bin/airymaxrt"
    fi
    if [ -x "${AIRY_HOME}/bin/airymaxrt" ]; then
        mkdir -p "${BIN_DIR}"
        ln -sf "${AIRY_HOME}/bin/airymaxrt" "${BIN_DIR}/airymaxrt"
        log_ok "启动器软链: ${BIN_DIR}/airymaxrt → ${AIRY_HOME}/bin/airymaxrt"
    fi

    # daemon 启动编排脚本（bootstrap）：部署到 bin/（systemd 与手动启动引用）
    if [ -f "${AIRY_SRC_DIR}/tools/scripts/ops/bin/agentrt-bootstrap.sh" ]; then
        cp -f "${AIRY_SRC_DIR}/tools/scripts/ops/bin/agentrt-bootstrap.sh" "${AIRY_HOME}/bin/agentrt-bootstrap.sh"
        chmod 755 "${AIRY_HOME}/bin/agentrt-bootstrap.sh"
        log_ok "agentrt-bootstrap.sh 已部署到 bin/"
    elif [ -f "${AIRY_HOME}/bin/agentrt-bootstrap.sh" ]; then
        log_ok "agentrt-bootstrap.sh 已存在（二进制模式自带）"
    else
        log_warn "agentrt-bootstrap.sh 未部署（源码缺失且二进制未含）"
    fi

    # 安装器自托管（供离线卸载）
    cp -f "$0" "${AIRY_HOME}/scripts/install.sh" 2>/dev/null || true
    log_ok "安装位置已固化: install.env + agentrt-env.sh + 启动器"

    # ── PATH 引导检查（2.3.2.6，与 build.sh 同构）────────────────────────
    # 二进制安装最常见的失败模式：`airymaxrt: command not found`——BIN_DIR
    # 不在用户 PATH 中且安装器未提示。安装即引导：BIN_DIR 不在 PATH 时给出
    # 可复制的修复命令，并把结果写入 install.env（airymaxrt status/doctor
    # 可据此提示，避免"装上了却找不到命令"的困惑）。
    # 2026-08-24 强化：
    #   - 写入 AIRY_BIN_LINK（doctor/status 据此给出准确的修复路径，此前
    #     该键从未写入，--prefix 自定义安装时诊断提示回退到错误的默认路径）；
    #   - PATH 检测按段遍历（对含空格/glob 字符的 BIN_DIR 健壮，此前
    #     case glob 匹配在这些路径下会误判）；
    #   - 永久生效提示按当前 shell 选择 rc 文件（bash/zsh/fish/posix）。
    echo "AIRY_BIN_LINK=${BIN_DIR}/airymaxrt" >> "${AIRY_HOME}/config/install.env"
    _PATH_OK=0
    _P_SEG="$PATH"
    while [ -n "$_P_SEG" ]; do
        _seg="${_P_SEG%%:*}"
        if [ "$_seg" = "$BIN_DIR" ]; then
            _PATH_OK=1
            break
        fi
        if [ "$_seg" = "$_P_SEG" ]; then
            _P_SEG=""
        else
            _P_SEG="${_P_SEG#*:}"
        fi
    done
    if [ "$_PATH_OK" = "1" ]; then
        log_ok "PATH 引导: ${BIN_DIR} 已在 PATH 中，可直接输入 airymaxrt"
        echo "AIRY_BIN_DIR_IN_PATH=yes" >> "${AIRY_HOME}/config/install.env"
    else
        log_warn "PATH 引导: ${BIN_DIR} 不在 PATH 中——当前 shell 输入 airymaxrt 会报 command not found"
        path_bootstrap
        echo "AIRY_BIN_DIR_IN_PATH=no" >> "${AIRY_HOME}/config/install.env"
    fi
}

# ─── 17 daemon 完整性校验 ──────────────────────────────────────────────
# 参数 strict：二进制模式下缺 daemon 视为安装失败（exit 1），
# 避免「残缺安装却显示成功」；源码模式保留 warn。
verify_daemons() {
    local missing="" strict="$1"
    for d in ${EXPECTED_DAEMONS}; do
        [ -x "${AIRY_HOME}/bin/${d}" ] || missing="${missing} ${d}"
    done
    if [ -n "$missing" ]; then
        if [ "$strict" = "strict" ]; then
            log_err "daemon 校验失败，缺失:${missing}（二进制包不完整，请检查 release 制品）"
            exit 1
        fi
        log_warn "daemon 校验未全通过，缺失:${missing}（可能为二进制包未含全部组件）"
    else
        log_ok "18 个 daemon 全部就位"
    fi
}

# ─── 安装后自检：版本一致性 + 更新通道提示 ──────────────────────────────
post_install_selfcheck() {
    local ver_installed=""
    if [ -f "${AIRY_HOME}/config/install.env" ]; then
        ver_installed="$(sed -n 's/^AIRY_VERSION=//p' "${AIRY_HOME}/config/install.env" 2>/dev/null | head -1)"
    fi
    log_ok "已安装版本: ${ver_installed:-v?}（通道: ${AIRY_CHANNEL}）"
    if [ "${AIRY_CHANNEL}" = "beta" ]; then
        log_warn "beta 通道发布更频繁；正式环境建议 'airymaxrt update --channel stable' 切回"
    fi
    log_info "更新检查: airymaxrt update --check    升级: airymaxrt update"
}

# ─── 版本信息 ──────────────────────────────────────────────────────────
print_banner() {
    cat <<EOF
${C_CYAN}
  ┌─────────────────────────────────────────────────────┐
  │         Airymax Agent Platform Engineering          │
  │=====================================================│
  │     Runtime · Frame · SpuerAgent · All-in-one       │
  │=====================================================│
  │ "Agents, To the open air. To OpenAirymax. To hope." │
  └─────────────────────────────────────────────────────┘
${C_NC}
EOF
}

print_summary() {
    cat <<EOF

安装位置:   ${AIRY_HOME}
可执行文件: ${AIRY_HOME}/bin/
配置文件:   ${AIRY_HOME}/config/
安装固化:   ${AIRY_HOME}/config/install.env
运行环境:   . ${AIRY_HOME}/bin/agentrt-env.sh
启动器:     ${BIN_DIR}/airymaxrt（任意路径输入 airymaxrt 即启动）

快速开始:
  1. 配置 LLM 提供方（API key）:
     ${AIRY_HOME}/config/secrets.env
  2. 启动交互界面（自动拉起 gateway/llm daemon）:
     airymaxrt
  3. 查看运行时状态:
     airymaxrt status
  4. 一键卸载（--keep-data 保留记忆数据）:
     airymaxrt uninstall   或   ${AIRY_HOME}/scripts/install.sh --uninstall
EOF
}

# ─── PATH 引导（2026-08-22 初版；2026-08-23 2.1.2.6 优化）──────────────
# 历史教训：其他设备安装后用户直接输入 airymaxrt 报 command not found——
# ${BIN_DIR}（默认 $HOME/.local/bin）未加入 PATH，而摘要宣称"任意路径输入
# airymaxrt 即启动"造成误导。安装收尾时必须实测 PATH 并在缺失时给出
# 精确、可复制的引导（临时/持久/完整路径三选一）。
# 2.1.2.6 优化：
#   - 持久生效给出"一键命令"（写 rc + 立即 export 合并，无需重启会话）
#   - 检测全部存在的 shell rc（.bashrc/.zshrc/.profile），逐项给出
#   - PATH 已就绪时输出确认，避免静默
print_path_guidance() {
    _found=0
    _ifs="$IFS"
    IFS=:
    for _p in $PATH; do
        [ -n "$_p" ] || _p="."
        if [ "$_p" = "$BIN_DIR" ]; then _found=1; break; fi
    done
    IFS="$_ifs"

    # 安装完整性兜底：BIN_DIR 下无启动器时提前告警（避免引导后仍 404）
    if [ ! -x "$BIN_DIR/airymaxrt" ]; then
        log_warn "${BIN_DIR}/airymaxrt 不存在——启动器安装可能未完成。"
        echo "  请先检查上方安装日志，或使用完整路径排查："
        echo "    ls -l \"${BIN_DIR}/airymaxrt\""
    fi

    if [ "$_found" -eq 1 ]; then
        log_ok "PATH 已包含 ${BIN_DIR}，可直接输入 airymaxrt 启动。"
        return 0
    fi

    log_warn "${BIN_DIR} 不在当前 PATH 中，无法直接输入 airymaxrt 启动。"
    echo "  请按需选择以下任一种方式："
    echo ""
    echo "    1) 临时生效（仅当前终端，立即可用）:"
    echo "       export PATH=\"${BIN_DIR}:\$PATH\""
    echo ""
    echo "    2) 持久生效（推荐，一键命令——写入配置并立即生效）:"
    _shown_rc=0
    for _rc in "$HOME/.bashrc" "$HOME/.zshrc" "$HOME/.profile"; do
        if [ -f "$_rc" ]; then
            _rc_basename="${_rc##*/}"
            echo "       # ${_rc_basename}"
            echo "       echo 'export PATH=\"${BIN_DIR}:\$PATH\"' >> \"$_rc\" && export PATH=\"${BIN_DIR}:\$PATH\""
            _shown_rc=1
        fi
    done
    if [ "$_shown_rc" -eq 0 ]; then
        echo "       # 未检测到 .bashrc/.zshrc/.profile，请手动执行后任选其一追加:"
        echo "       echo 'export PATH=\"${BIN_DIR}:\$PATH\"' >> \"\$HOME/.bashrc\""
    fi
    echo ""
    echo "    3) 不改 PATH，改用完整路径启动:"
    echo "       ${BIN_DIR}/airymaxrt"
    echo ""
    echo "  提示：方式 2 对新开的终端永久生效；本终端立即执行方式 1 或方式 2 中的 export 即可先用。"
}

# ─── 主流程 ────────────────────────────────────────────────────────────
main() {
    print_banner
    log_info "Airymax AgentRT 安装程序"
    log_info "AIRY_HOME = ${AIRY_HOME} | 模式 = ${AIRY_MODE}"

    if [ "$UNINSTALL" = "1" ]; then
        do_uninstall "$AIRY_HOME" "$KEEP_DATA" "$YES"
        exit $?
    fi

    init_home

    local installed=1
    # 发布来源解析：--from-file 离线包 > AIRY_RELEASE_URL 显式 URL >
    # 官方通道 manifest（默认，stable/beta 由 --channel 决定；--mode source 除外）。
    # manifest 路径走 GPG 验签 + 平台解析；失败自动降级源码构建。
    local release_url="${AIRY_RELEASE_URL:-}"
    if [ -z "$release_url" ] && [ "${AIRY_MODE:-auto}" != "source" ]; then
        release_url="https://raw.atomgit.com/openairymax/agentrt/raw/main/latest/manifest.${AIRY_CHANNEL}.json"
    fi
    if [ -n "$AIRY_FROM_FILE" ]; then
        install_binary "$AIRY_FROM_FILE" && installed=0
    elif [ "$AIRY_MODE" = "binary" ] || { [ "$AIRY_MODE" = "auto" ] && [ -n "$release_url" ]; }; then
        install_binary "$release_url" && installed=0
    fi

    if [ "$installed" -ne 0 ]; then
        log_info "进入源码构建模式（${AIRY_MODE}）"
        # 工具链仅在源码构建路径要求（二进制模式无需 git/cmake/gcc）
        check_toolchain
        prepare_source

        # 模式 B：无闭源源码 → 下载预编译模块包（URL 未配置时跳过，闭源功能受限）
        if [ ! -d "${AIRY_SRC_APP}/agentrt/atoms" ] && [ "$AIRY_MODE" != "source" ]; then
            fetch_prebuilt_module "atoms" "${AIRY_ATOMS_PREBUILT_URL:-}" "atoms" || \
                log_warn "atoms 预编译包不可用；如需完整功能请配置 AIRY_ATOMS_PREBUILT_URL 或使用本地源码"
            fetch_prebuilt_module "memoryrovol" "${AIRY_MEMORYROVOL_PREBUILT_URL:-}" "memoryrovol" || \
                log_warn "memoryrovol 预编译包不可用（无授权将自动降级 OSS/builtin）"
        elif [ "$AIRY_MODE" = "source" ] && [ -d "${AIRY_SRC_APP}/agentrt/atoms" ]; then
            log_ok "模式 C：本地闭源源码（atoms/memoryrovol）全量构建"
        fi

        if [ "${AIRY_NO_BUILD:-}" != "1" ]; then
            build_and_install
            install_python_deps
            build_mr_oss
            build_tui
        fi
    fi

    # 启动器兼容入口与 secrets 在两种模式（二进制/源码）下均需生成：
    # 二进制模式依赖 airy_cli 生成 agentrt-tui 兼容入口，否则无任何启动命令
    ensure_cli_entry
    init_secrets

    # 硬件评估与运行画像固化（2.3.5/2.3.6）：安装即按硬件裁剪——
    # 二进制包同样适用（无论安装模式，自动识别硬件、固化画像；
    # airymaxrt 启动器按画像拉起 daemon 集，monitor 监控外设增强自动恢复）。
    persist_profile

    # 出厂预装 maths-toolkit（数学计算后端，默认开启，可 --without-maths 跳过）
    install_maths_toolkit

    finalize_install
    if [ "$installed" -eq 0 ]; then
        verify_daemons strict
    else
        verify_daemons
    fi
    post_install_selfcheck
    print_summary
    print_path_guidance
    log_ok "安装完成"
}

main "$@"
