#!/bin/sh

set -u

if [ -t 1 ] && [ -z "${NO_COLOR:-}" ]; then
    C_RED="$(printf '\033[0;31m')"; C_GREEN="$(printf '\033[0;32m')"
    C_YELLOW="$(printf '\033[1;33m')"; C_CYAN="$(printf '\033[0;36m')"
    C_NC="$(printf '\033[0m')"
else
    C_RED=''; C_GREEN=''; C_YELLOW=''; C_CYAN=''; C_NC=''
fi
log_info()  { printf '%s[INFO]%s %s\n' "$C_CYAN" "$C_NC" "$1"; }
log_ok()    { printf '%s[YES]%s %s\n' "$C_GREEN" "$C_NC" "$1"; }
log_warn()  { printf '%s[WARN]%s %s\n' "$C_YELLOW" "$C_NC" "$1"; }
log_err()   { printf '%s[NO]%s %s\n' "$C_RED" "$C_NC" "$1"; }

if [ -t 2 ]; then
    CURL_FLAG="--progress-bar -S"
    HAS_TTY=1
else
    CURL_FLAG="-s"
    HAS_TTY=0
fi

spinner() {
    local _sp_pid="$1" _sp_label="$2" _sp_i=0 _sp_c='|'
    while kill -0 "$_sp_pid" 2>/dev/null; do
        case "$_sp_i" in
            0) _sp_c='|' ;; 1) _sp_c='/' ;; 2) _sp_c='-' ;; 3) _sp_c='\' ;;
        esac
        printf '\r\033[K[%s] %s' "$_sp_c" "$_sp_label" >&2
        _sp_i=$(( (_sp_i + 1) % 4 ))
        sleep 0.1
    done
    printf '\r\033[K' >&2
}

run_spin() {
    local _rs_label="$1"
    shift
    if [ "$HAS_TTY" = "1" ]; then
        "$@" >/dev/null 2>&1 &
        local _rs_pid=$!
        spinner "$_rs_pid" "$_rs_label"
        wait "$_rs_pid"
        return $?
    fi
    "$@" >/dev/null 2>&1
}

resolve_link_chain() {
    local _p="$1" _d _t _i=0
    while [ -L "$_p" ] && [ "$_i" -lt 16 ]; do
        _d="$(cd -P "$(dirname "$_p")" 2>/dev/null && pwd || dirname "$_p")"
        _t="$(readlink "$_p" 2>/dev/null || true)"
        [ -n "$_t" ] || break
        case "$_t" in
            /*) _p="$_t" ;;
            *)  _p="${_d}/${_t}" ;;
        esac
        _i=$((_i + 1))
    done
    printf '%s' "$_p"
}

is_install_home() {
    [ -n "$1" ] || return 1
    [ -f "$1/config/install.env" ] || [ -x "$1/bin/airy_cli" ]
}

discover_install_home() {
    local _h="${AIRY_HOME:-}" _link _real
    if [ -n "$_h" ]; then
        if is_install_home "$_h"; then printf '%s' "$_h"; return 0; fi
        log_warn "已忽略环境变量 AIRY_HOME=${_h}（非既有安装根，疑似终端残留）" >&2
    fi
    _link="$(command -v airymaxrt 2>/dev/null || true)"
    if [ -n "$_link" ] && command -v readlink >/dev/null 2>&1; then
        _real="$(cd -P "$(dirname "$(resolve_link_chain "$_link")")" 2>/dev/null && pwd || true)"
        if [ -n "$_real" ]; then
            _h="$(dirname "$_real")"
            if is_install_home "$_h"; then printf '%s' "$_h"; return 0; fi
        fi
    fi
    case "$0" in
        */*)
            _real="$(cd -P "$(dirname "$0")" 2>/dev/null && pwd || true)"
            if [ -n "$_real" ] && [ -f "${_real}/../config/install.env" ]; then
                _h="$(sed -n 's/^AIRY_HOME=//p' "${_real}/../config/install.env" 2>/dev/null | head -1)"
                if is_install_home "$_h" && [ "${_real}" = "$(cd -P "${_h}/bin" 2>/dev/null && pwd || true)" ]; then
                    printf '%s' "$_h"; return 0
                fi
            fi
            ;;
    esac
    return 1
}

AIRY_HOME="$(discover_install_home || true)"
if [ -n "$AIRY_HOME" ]; then
    log_info "复用既有安装根: ${AIRY_HOME}（并列新实例请加 --prefix <path>）"
else
    AIRY_HOME="${HOME}/.airymaxrt"
fi
AIRY_REPO_URL="${AIRY_REPO_URL:-https://atomgit.com/openairymax/airymaxhub.git}"
AIRY_VERSION_SPECIFIED=0
if [ -n "${AIRY_VERSION:-}" ]; then
    AIRY_VERSION_SPECIFIED=1
elif [ -f "$(dirname "$0")/../VERSION" ]; then
    AIRY_VERSION="v$(cat "$(dirname "$0")/../VERSION" | tr -d '[:space:]')"
fi
AIRY_VERSION="${AIRY_VERSION:-v0.1.18}"
AIRY_BUILD_JOBS="${AIRY_BUILD_JOBS:-$(nproc 2>/dev/null || echo 4)}"
AIRY_MODE="${AIRY_MODE:-auto}"
BIN_DIR="${BIN_DIR:-$HOME/.local/bin}"
UNINSTALL=0; REINSTALL=0; KEEP_DATA=0; YES=0
WITH_MATHS=1
AIRY_PROFILE="${AIRY_PROFILE:-auto}"
AIRY_CHANNEL="${AIRY_CHANNEL:-stable}"
AIRY_FROM_FILE="${AIRY_FROM_FILE:-}"
case "$AIRY_CHANNEL" in stable|rc|beta) ;; *) log_err "非法 --channel: ${AIRY_CHANNEL}（支持 stable|rc|beta）"; exit 1 ;; esac

AIRY_RELEASE_OWNER="${AIRY_RELEASE_OWNER:-openairymax/agentrt}"
AIRY_RELEASE_BASE="${AIRY_RELEASE_BASE:-https://atomgit.com/${AIRY_RELEASE_OWNER}/releases/download}"

syscurl() {
    local _ldp="" _seg _rest="${LD_LIBRARY_PATH:-}"
    while [ -n "$_rest" ]; do
        _seg="${_rest%%:*}"
        [ "$_seg" = "${AIRY_HOME}/lib" ] || _ldp="${_ldp:+$_ldp:}$_seg"
        [ "$_seg" = "$_rest" ] && _rest="" || _rest="${_rest#*:}"
    done
    if [ -n "$_ldp" ]; then
        env LD_LIBRARY_PATH="$_ldp" curl "$@"
    else
        env -u LD_LIBRARY_PATH curl "$@"
    fi
}

sha256_file() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" 2>/dev/null | awk '{print $1}'
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$1" 2>/dev/null | awk '{print $1}'
    else
        printf ''
    fi
}
sha256_stdin() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum 2>/dev/null | awk '{print $1}'
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 2>/dev/null | awk '{print $1}'
    else
        printf ''
    fi
}

env_set() {
    local _k="${1%%=*}" _f="${AIRY_HOME}/config/install.env"
    mkdir -p "$(dirname "$_f")" 2>/dev/null || true
    [ -f "$_f" ] || : > "$_f"
    if grep -v "^${_k}=" "$_f" > "$_f.tmp" 2>/dev/null; then
        mv "$_f.tmp" "$_f"
    else
        rm -f "$_f.tmp"
    fi
    echo "$1" >> "$_f"
}

AIRY_SRC_DIR="${AIRY_HOME}/src/airymaxhub"
MODULES_DIR="${AIRY_HOME}/modules"

AIRY_SRC_APP="${AIRY_SRC_DIR}/agent-workload"
if [ ! -d "${AIRY_SRC_APP}/agentrt" ]; then
    AIRY_SRC_APP="${AIRY_SRC_DIR}"
fi

daemon_list() {
    local bin="${1:-${AIRY_HOME}/bin}" d
    [ -d "$bin" ] || return 0
    for d in "${bin}"/*_d; do
        [ -f "$d" ] && basename "$d"
    done
}

installer_self_bootstrap() {
    [ "${AIRY_INSTALLER_BOOTSTRAPPED:-0}" = "1" ] && return 0
    [ -f "$0" ] || return 0
    command -v curl >/dev/null 2>&1 || return 0
    local remote tmp_inst _ls _rs
    remote="$(mktemp 2>/dev/null || echo "${TMPDIR:-/tmp}/airymaxrt-installer.remote.$$")"
    syscurl -fsSL --max-time 30 -o "$remote" \
        "${AIRY_RELEASE_BASE}/latest/install.sh" || { rm -f "$remote"; return 0; }
    [ -s "$remote" ] || { rm -f "$remote"; return 0; }
    _rs="$(sha256_file "$remote")"
    _ls="$(sha256_file "$0")"
    if [ -z "$_rs" ] || [ "$_rs" = "$_ls" ]; then
        rm -f "$remote"
        return 0
    fi
    log_info "检测到发布面安装器更新，切换到最新版执行…"
    export AIRY_INSTALLER_BOOTSTRAPPED=1
    tmp_inst="$(mktemp 2>/dev/null || echo "${TMPDIR:-/tmp}/airymaxrt-installer.$$")"
    if cp -f "$remote" "$tmp_inst" && chmod 755 "$tmp_inst"; then
        rm -f "$remote"
        exec "$tmp_inst" "$@"
    fi
    rm -f "$remote" "$tmp_inst"
    return 0
}

installer_self_bootstrap "$@"

show_usage() {
    cat <<'USAGE_EOF'
AirymaxRT 安装器

用法:
  一键安装:  curl -fsSL "https://atomgit.com/openairymax/agentrt/releases/download/latest/install.sh" | bash
  本地执行:  sh install.sh [参数]
  更新:      airymaxrt update [--check] [--channel stable|rc|beta] [--rollback]
  重装:      airymaxrt update --reinstall  或  sh install.sh --reinstall
  卸载:      sh install.sh --uninstall [--keep-data]

参数:
  --prefix DIR       安装根目录（默认复用既有实例，否则 ~/.airymaxrt）
  --mode MODE        安装模式: auto|binary|hybrid|source（默认 auto）
  --bin-dir DIR      可执行文件链接目录（默认 ~/.local/bin）
  --profile PROFILE  功能档位: full|minimal|auto（默认 auto）
  --channel CHANNEL  发布通道: stable|rc|beta（默认 stable）
  --from-file FILE   从本地制品文件安装
  --reinstall        强制重装
  --uninstall        卸载
  --keep-data        卸载时保留用户数据
  --with-maths       安装数学计算模块（默认开启）
  --without-maths    跳过数学计算模块
  --yes              跳过交互确认
  -h, --help         显示本帮助
USAGE_EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --prefix)    AIRY_HOME="$2"; shift 2 ;;
        --mode)      AIRY_MODE="$2"; shift 2 ;;
        --bin-dir)   BIN_DIR="$2"; shift 2 ;;
        --profile)   AIRY_PROFILE="$2"; shift 2 ;;
        --channel)   AIRY_CHANNEL="$2"; shift 2 ;;
        --from-file) AIRY_FROM_FILE="$2"; shift 2 ;;
        --uninstall) UNINSTALL=1; shift ;;
        --reinstall) REINSTALL=1; shift ;;
        --keep-data) KEEP_DATA=1; shift ;;
        --yes)       YES=1; shift ;;
        --with-maths)    WITH_MATHS=1; shift ;;
        --without-maths) WITH_MATHS=0; shift ;;
        --help|-h)   show_usage; exit 0 ;;
        *) log_err "未知参数: $1（--help 查看用法）"; exit 1 ;;
    esac
done

case "$AIRY_MODE" in auto|binary|hybrid|source) ;; *) log_err "非法 --mode: ${AIRY_MODE}"; exit 1 ;; esac
case "$AIRY_PROFILE" in auto|full|minimal) ;; *) log_err "非法 --profile: ${AIRY_PROFILE}（支持 full|minimal|auto）"; exit 1 ;; esac

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

detect_existing_install() {
    local env_file="${AIRY_HOME}/config/install.env" ver link resolved
    if [ -f "$env_file" ]; then
        ver="$(sed -n 's/^AIRY_VERSION=//p' "$env_file" 2>/dev/null | head -1)"
        log_info "检测到既有安装: ${AIRY_HOME}（${ver:-版本未知}）→ 覆盖安装；config/ 与 secrets.env 保留"
        return 0
    fi
    link="$(command -v airymaxrt 2>/dev/null || true)"
    [ -n "$link" ] || return 0
    command -v readlink >/dev/null 2>&1 || return 0
    resolved="$(resolve_link_chain "$link")"
    [ -n "$resolved" ] || return 0
    case "$resolved" in
        "${AIRY_HOME}/bin/airymaxrt") return 0 ;;
    esac
    log_warn "PATH 中的 airymaxrt 指向其它实例: ${link} → ${resolved}"
    log_info "  本次显式安装到 ${AIRY_HOME}，两套实例互不影响；若只想重装既有实例，"
    log_info "  去掉 --prefix/AIRY_HOME 后重跑即可（默认自动复用 PATH 中的安装根）"
    return 0
}

stop_daemons() {
    local bin="$1" found=0 d
    [ -d "$bin" ] || return 1
    for d in $(daemon_list "$bin"); do
        if [ -x "${bin}/${d}" ]; then
            if pkill -f "${bin}/${d}" >/dev/null 2>&1; then
                found=1
            fi
            if [ -d /proc ] && command -v readlink >/dev/null 2>&1; then
                for _pid in /proc/[0-9]*; do
                    _exe="$(readlink "${_pid}/exe" 2>/dev/null || true)"
                    case "$_exe" in
                        "${bin}/${d}"|*"/${d}")
                            kill "${_pid#/proc/}" 2>/dev/null && found=1 || true ;;
                    esac
                done
            fi
        fi
    done
    [ "$found" = "1" ] && sleep 1
    return $(( found == 0 ))
}

remove_path_bootstrap() {
    local rc rc_path="${1:-}" _tmp
    for rc in "$rc_path" "$HOME/.bashrc" "$HOME/.zshrc" "$HOME/.profile" "$HOME/.config/fish/config.fish"; do
        [ -n "$rc" ] && [ -f "$rc" ] || continue
        grep -q '# >>> AgentRT PATH bootstrap <<<' "$rc" 2>/dev/null || continue
        _tmp="$rc.airy_uninstall_tmp"
        if sed '\|# >>> AgentRT PATH bootstrap <<<|,\|# <<< AgentRT PATH bootstrap <<<|d' "$rc" > "$_tmp" 2>/dev/null \
            && mv "$_tmp" "$rc"; then
            log_ok "已从 ${rc} 移除 AgentRT PATH 引导行"
        else
            rm -f "$_tmp" 2>/dev/null
            log_warn "无法自动清理 ${rc} 的 AgentRT PATH 引导行，请手动删除标记区间"
        fi
    done
}

do_uninstall() {
    local home="$1" keep_data="$2" yes="$3" env_file link size ans rc_path
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
    rc_path="$(sed -n 's/^AIRY_PATH_RC=//p' "$env_file" 2>/dev/null | head -1)"
    size="$(du -sh "$home" 2>/dev/null | cut -f1)"
    log_warn "将卸载 AirymaxRT：${home}（${size}）"
    if [ "$yes" != "1" ]; then
        printf "${C_YELLOW}确认卸载？[y/N] ${C_NC}"
        read -r ans || true
        case "$ans" in y|Y|yes|YES) ;; *) log_info "已取消卸载"; return 0 ;; esac
    fi
    stop_daemons "$home/bin"
    if command -v pgrep >/dev/null 2>&1; then
        for _mp in $(pgrep -f "airymaxrt monitor" 2>/dev/null || true); do
            if tr '\0' '\n' < "/proc/${_mp}/environ" 2>/dev/null | grep -q "^AIRY_HOME=${home}$"; then
                kill "$_mp" 2>/dev/null || true
            fi
        done
    fi
    if [ "$keep_data" = "1" ] && [ -d "$home/data" ]; then
        rm -rf "$home"
        mkdir -p "$home/data"
        log_ok "已删除 ${home}（保留 data/ 记忆数据）"
    else
        rm -rf "$home"
        log_ok "已删除 ${home}"
    fi
    if [ -L "$link" ] || [ -e "$link" ]; then
        rm -f "$link"
        log_ok "已移除启动器 ${link}"
    fi
    remove_path_bootstrap "$rc_path"
    log_ok "卸载完成"
}


AIRY_GPG_PUBKEY='-----BEGIN PGP PUBLIC KEY BLOCK-----

mDMEao7uahYJKwYBBAHaRw8BAQdAk8Ou1tA2EfX5xZT4ET79YJESeqINPyFF86MK
cpPAQDO0NEFnZW50UlQgUmVsZWFzZSBTaWduaW5nIDxyZWxlYXNlQGFnZW50cnQu
YWlyeW1heC5pbz6IkwQTFgoAOxYhBIbDf3xc3cxA57s+YuQ19/HMJP+EBQJqju5q
AhsDBQsJCAcCAiICBhUKCQgLAgQWAgMBAh4HAheAAAoJEOQ19/HMJP+EepEBANYY
xAN1mQL4gulwMvH3xjiL6aEVm1PFjus33MXJrDmKAQDEck2sowTfLa1WneqUY93D
QpegwKdM5Y9YiANOL8FODQ==
=EPz8
-----END PGP PUBLIC KEY BLOCK-----'

fetch_release_asset() {
    local tag="${3:-latest}" tmp
    tmp="${AIRY_HOME}/tmp/asset.$$"
    mkdir -p "$(dirname "$tmp")" 2>/dev/null
    syscurl -fsSL --max-time 60 -o "$tmp" "${AIRY_RELEASE_BASE}/${tag}/${1}" || { rm -f "$tmp"; return 1; }
    mv -f "$tmp" "$2" || { rm -f "$tmp"; return 1; }
    [ -s "$2" ]
}

verify_gpg_sig() {
    [ -s "$2" ] || { log_warn "缺少签名文件（fail-closed），拒绝安装"; return 1; }
    local gnupg="${AIRY_HOME}/tmp/gnupg-install" keyf
    mkdir -p "$gnupg" && chmod 700 "$gnupg"
    keyf="$gnupg/agentrt.asc"
    if [ -z "${AIRY_NO_NETWORK:-}" ]; then
        fetch_release_asset "agentrt.asc" "$keyf" >/dev/null 2>&1 || true
    fi
    if [ ! -s "$keyf" ] && [ -f "${AIRY_HOME}/keys/agentrt.asc" ]; then
        cp -f "${AIRY_HOME}/keys/agentrt.asc" "$keyf" 2>/dev/null || true
    fi
    [ -s "$keyf" ] || printf '%s\n' "$AIRY_GPG_PUBKEY" > "$keyf"
    gpg --batch --quiet --no-tty --homedir "$gnupg" --import "$keyf" >/dev/null 2>&1 || return 1
    gpg --batch --no-tty --homedir "$gnupg" --verify "$2" "$1" >/dev/null 2>&1
}

parse_manifest() {
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
    sed -n "/\"$2\"[[:space:]]*:[[:space:]]*{/,/^[[:space:]]*}/{
        s/.*\"$3\"[[:space:]]*:[[:space:]]*\"\([^\"]*\)\".*/\1/p
        s/.*\"$3\"[[:space:]]*:[[:space:]]*\([0-9][0-9]*\).*/\1/p
    }" "$1" | head -1
}

manifest_state() {
    if command -v python3 >/dev/null 2>&1; then
        python3 - "$1" <<'PYEOF'
import json, sys
try:
    print(json.load(open(sys.argv[1])).get("state", "active"))
except Exception:
    print("active")
PYEOF
        return 0
    fi
    sed -n 's/.*"state"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' "$1" | head -1
}

install_binary() {
    local url="$1" arch plat expect_sha="" expect_size="" legacy="" tarball="" local_src=0 _dl_flags=""
    arch="$(detect_arch)"
    if [ "$(uname -s 2>/dev/null)" = "Darwin" ]; then
        plat="macos-$(plat_name "$(uname -m 2>/dev/null)")"
    else
        plat="linux-$(plat_name "${arch}")"
    fi
    log_info "运行平台: ${plat}（架构 ${arch}）"
    if [ -z "${AIRY_FROM_FILE:-}" ]; then
        case "$arch" in
            riscv64|riscv32|riscv)
                log_err "检测到 RISC-V（${arch}）：官方暂无预编译制品（CI 仅 canary 构建）"
                log_err "请源码构建：AIRY_MODE=source bash install.sh（需 RISC-V 工具链 + 依赖库）"
                return 1
                ;;
        esac
        case " ${SUPPORTED_ARCHS} " in
            *" ${arch} "*) ;;
            *)
                log_err "检测到架构 ${arch}，不在官方预编译发布清单（${SUPPORTED_ARCHS}）内"
                log_err "请源码构建：AIRY_MODE=source bash install.sh（需 C 工具链 + 依赖库）"
                return 1
                ;;
        esac
    fi

    if [ "${url##*.}" = "json" ]; then
        local man="${AIRY_HOME}/tmp/manifest.json" man_asc="${AIRY_HOME}/tmp/manifest.json.asc"
        syscurl -fsSL --max-time 60 -o "$man" "$url" || {
            log_err "manifest 拉取失败：${url}"
            log_err "可能是网络/服务异常；也可能该通道暂无制品——当前可用：stable（生产）/ rc（候选），beta 为保留通道"
            return 2
        }
        syscurl -fsSL --max-time 60 -o "$man_asc" "${url}.asc" >/dev/null 2>&1 || true
        verify_gpg_sig "$man" "$man_asc" || { log_err "manifest 验签失败（GPG），拒绝安装——请确认网络环境未被劫持后重试"; return 2; }
        [ -s "$man_asc" ] && log_ok "manifest 验签通过（GPG）"
        if [ "$(manifest_state "$man")" = "reserved" ]; then
            log_err "通道 ${AIRY_CHANNEL} 为保留通道（state=reserved），官方尚未发布任何制品"
            log_err "当前可用：stable（生产）/ rc（候选）——切换：AIRY_CHANNEL=stable bash install.sh"
            return 2
        fi
        url="$(parse_manifest "$man" "$plat" url)"
        expect_sha="$(parse_manifest "$man" "$plat" sha256)"
        expect_size="$(parse_manifest "$man" "$plat" size)"
        if [ -z "$url" ]; then
            for legacy in $(plat_legacy_name "$plat"); do
                [ -n "$legacy" ] || continue
                url="$(parse_manifest "$man" "$legacy" url)"
                expect_sha="$(parse_manifest "$man" "$legacy" sha256)"
                expect_size="$(parse_manifest "$man" "$legacy" size)"
                [ -n "$url" ] && { log_info "平台键 ${plat} 未命中，已用兼容命名 ${legacy}"; break; }
            done
        fi
        [ -n "$url" ] || { log_warn "manifest 无 ${plat} 制品，回退源码构建"; return 1; }
        local _fname _fver _os
        _fname="$(basename "${url%%\?*}")"
        _fver=""
        for _os in linux macos win; do
            _fver="$(printf '%s' "$_fname" | sed -n "s/^agentrt-v\\(.*\\)-${_os}-.*/\\1/p")"
            [ -n "$_fver" ] && break
        done
        if [ -n "$_fver" ]; then
            log_info "目标版本: v${_fver}（通道 ${AIRY_CHANNEL}）"
        else
            log_info "目标制品: ${_fname}（通道 ${AIRY_CHANNEL}）"
        fi
    elif [ -f "$url" ]; then
        log_info "使用本地离线包: $url"
        tarball="$url"
        local_src=1
    else
        url="$(printf '%s' "$url" | sed "s/{arch}/${arch}/g")"
    fi

    if [ -z "$tarball" ] && [ -n "$url" ]; then
        tarball="${AIRY_HOME}/tmp/$(basename "${url%%\?*}")"
    fi

    rm -rf "${AIRY_HOME}"/tmp/agentrt-*/ 2>/dev/null || true

    if [ -z "$expect_sha" ] && [ -f "${tarball}.sha256" ]; then
        expect_sha="$(cut -d' ' -f1 "${tarball}.sha256" 2>/dev/null)"
    fi
    _retry_download=0
    while :; do
        if [ ! -f "$tarball" ]; then
            if [ "$local_src" = "1" ]; then
                log_err "离线包不存在或已被移除: ${tarball}，请重新指定 --from-file 路径"
                return 2
            fi
            _dl_flags="-fsSL"
            [ "$HAS_TTY" = "1" ] && _dl_flags="-fL --progress-bar"
            if [ -n "$expect_size" ]; then
                log_info "下载完全体二进制包（$(human_size "$expect_size")）: ${url}"
            else
                log_info "下载完全体二进制包: ${url}"
            fi
            if ! syscurl $_dl_flags --max-time 600 -o "${tarball}" "${url}"; then
                rm -f "${tarball}"
                if [ "$_retry_download" -lt 1 ]; then
                    log_warn "release 下载失败，重试一次（网络抖动兜底）…"
                    _retry_download=$((_retry_download+1)); continue
                fi
                log_err "release 下载失败（已重试）。请检查网络后重新运行一键安装："
                log_err "  curl -fsSL \"${AIRY_RELEASE_BASE}/latest/install.sh\" | bash"
                return 2
            fi
            log_ok "下载完成: $(human_size "$(wc -c < "${tarball}" 2>/dev/null | tr -d ' ')")"
        fi
        if [ -n "$expect_sha" ]; then
            _actual_sha="$(sha256_file "$tarball")"
            if [ "$_actual_sha" != "$expect_sha" ]; then
                if [ "$local_src" = "1" ]; then
                    log_err "sha256 校验失败：离线包与校验值不一致，拒绝安装"
                    log_err "  - 请重新下载安装包，或核对离线包与其 .sha256 是否匹配。"
                    return 2
                fi
                rm -f "$tarball"
                if [ "$_retry_download" -lt 2 ]; then
                    log_warn "本地缓存与官方校验值不符（可能是修复重传或残留旧包），重新下载…"
                    _retry_download=$((_retry_download+1)); continue
                fi
                log_err "sha256 校验失败：下载内容与官方 manifest 不一致，拒绝安装"
                log_err "  - 已自动重试仍失败，多为 CDN 缓存陈旧或网络中间层篡改。"
                log_err "  - 请稍候重试；或 --from-file 使用手动下载并核对 sha256 的离线包。"
                return 2
            fi
        fi
        break
    done
    if [ -n "$expect_sha" ]; then
        log_ok "sha256 校验通过"
    fi
    AIRY_ARTIFACT_SHA256="$(sha256_file "$tarball")"
    if ! _marker="$(arch_markers_ok "${tarball}" "${arch}")"; then
        log_err "二进制包架构与当前主机（${arch}）不匹配（包内标记 $(echo $_marker | tr ' ' '/')），拒绝安装"
        log_err "  期望标记之一: $(plat_markers "${arch}")"
        log_err "  制品: ${tarball}"
        [ "$local_src" = "1" ] || rm -f "${tarball}"
        return 2
    fi
    log_ok "二进制包架构校验通过（${arch}）"
    tar -xzf "${tarball}" -C "${AIRY_HOME}/tmp" || { log_err "release 包解压失败（tar），制品可能损坏"; [ "$local_src" = "1" ] || rm -f "$tarball"; return 2; }
    local extracted
    extracted="$(find "${AIRY_HOME}/tmp" -maxdepth 1 -type d -name 'agentrt-*' | head -1)"
    [ -n "$extracted" ] || { log_err "release 包结构异常（缺 agentrt-* 顶层目录），制品不完整"; return 2; }
    EXPECTED_DAEMONS="$(daemon_list "${extracted}/bin")"
    local rel_id rel_dir d
    rel_id="$(basename "$extracted" | sed 's/^agentrt-//')"
    rel_dir="${AIRY_HOME}/releases/${rel_id}"
    if [ -z "$EXPECTED_DAEMONS" ]; then
        log_err "release 包缺失 daemon 二进制（bin/*_d 为空，制品不完整）"
        return 2
    fi
    # B10 版本化布局：存量旧布局（真实目录）先迁移为首个版本目录
    if [ -d "${AIRY_HOME}/bin" ] && [ ! -L "${AIRY_HOME}/bin" ]; then
        local _mig _cur_tmp
        _mig="${AIRY_HOME}/releases/migrate-$(date +%Y%m%d%H%M%S)"
        mkdir -p "${AIRY_HOME}/releases" "$_mig" || { log_err "releases/ 目录创建失败"; return 2; }
        for d in bin lib include share; do
            [ -d "${AIRY_HOME}/$d" ] && cp -rf "${AIRY_HOME}/$d" "$_mig/" 2>/dev/null || true
        done
        if [ ! -e "$_mig/bin" ] || [ ! -e "$_mig/lib" ]; then
            log_err "存量安装迁移失败（复制不完整），中止以免破坏现有安装"
            rm -rf "$_mig"; return 2
        fi
        printf 'AIRY_VERSION=%s\n' "${AIRY_VERSION:-unknown}" > "$_mig/release.env"
        : > "$_mig/.complete"
        # -n: dest 为目录符号链接时直接替换链接本身（mv 会移入目录内部）
        if ! ln -sfn "$_mig" "${AIRY_HOME}/current"; then
            rm -rf "$_mig"
            log_err "存量迁移提交失败（current 切换被拒），现有安装未受影响"
            return 2
        fi
        for d in bin lib include share; do
            [ -d "${AIRY_HOME}/$d" ] || continue
            if ! mv "${AIRY_HOME}/$d" "${AIRY_HOME}/$d.mig.bak" 2>/dev/null; then
                log_err "存量迁移失败（$d 重命名被拒），现有安装未受影响"
                rm -rf "$_mig"; return 2
            fi
            if ! ln -s "current/$d" "${AIRY_HOME}/$d" 2>/dev/null; then
                mv "${AIRY_HOME}/$d.mig.bak" "${AIRY_HOME}/$d" 2>/dev/null || true
                log_err "存量迁移失败（$d 符号链接建立被拒），现有安装未受影响"
                rm -rf "$_mig"; return 2
            fi
            rm -rf "${AIRY_HOME}/$d.mig.bak"
        done
        rm -rf "${AIRY_HOME}/.rollback" 2>/dev/null || true
        log_ok "存量安装已迁移至版本化目录（releases/$(basename "$_mig")）"
    fi
    if [ -e "${AIRY_HOME}/current" ] && [ ! -L "${AIRY_HOME}/current" ]; then
        log_err "布局异常: ${AIRY_HOME}/current 为真实目录（预期符号链接），请先清理后重装"
        return 2
    fi
    mkdir -p "${AIRY_HOME}/releases"
    rm -rf "$rel_dir"; mkdir -p "$rel_dir"
    cp -rf "${extracted}/bin" "${rel_dir}/bin" 2>/dev/null || true
    local _binok=1 _d2
    for _d2 in ${EXPECTED_DAEMONS}; do
        [ -x "${rel_dir}/bin/${_d2}" ] || { _binok=0; log_err "bin/ 部署失败，缺失: ${_d2}（检查磁盘/权限）"; break; }
    done
    if [ "$_binok" != "1" ]; then
        rm -rf "$rel_dir"
        return 2
    fi
    lib_has() {
        ls "$1"/*.so* >/dev/null 2>&1 || ls "$1"/*.dylib* >/dev/null 2>&1
    }
    if [ -d "${extracted}/lib" ] && lib_has "${extracted}/lib"; then
        cp -rf "${extracted}/lib" "${rel_dir}/lib" 2>/dev/null || true
        if ! lib_has "${rel_dir}/lib"; then
            log_err "lib/ 部署失败（.so/.dylib 未就位），二进制将无法启动"
            rm -rf "$rel_dir"
            return 2
        fi
    fi
    [ -d "${extracted}/include" ] && cp -rf "${extracted}/include" "${rel_dir}/include" 2>/dev/null || true
    [ -d "${extracted}/share" ] && cp -rf "${extracted}/share" "${rel_dir}/share" 2>/dev/null || true
    if [ -d "${extracted}/config" ]; then
        cp -f "${extracted}"/config/* "${AIRY_HOME}/config/" 2>/dev/null || true
    fi
    if [ -f "${extracted}/keys/agentrt.asc" ]; then
        mkdir -p "${AIRY_HOME}/keys"
        cp -f "${extracted}/keys/agentrt.asc" "${AIRY_HOME}/keys/" 2>/dev/null || true
    fi
    if [ -d "${extracted}/modules/maths-toolkit" ]; then
        mkdir -p "${AIRY_HOME}/modules"
        cp -rf "${extracted}/modules/maths-toolkit" "${AIRY_HOME}/modules/" 2>/dev/null || true
    fi
    {
        printf 'AIRY_VERSION=v%s\n' "$rel_id"
        [ -n "${AIRY_ARTIFACT_SHA256:-}" ] && printf 'AIRY_ARTIFACT_SHA256=%s\n' "$AIRY_ARTIFACT_SHA256"
        printf 'AIRY_RELEASE_ID=%s\n' "$rel_id"
    } > "${rel_dir}/release.env"
    : > "${rel_dir}/.complete"
    for d in bin lib include share; do
        if [ -e "${AIRY_HOME}/$d" ] && [ ! -L "${AIRY_HOME}/$d" ]; then
            log_err "布局异常: ${AIRY_HOME}/$d 为真实目录（预期符号链接），请先清理后重装"
            rm -rf "$rel_dir"
            return 2
        fi
        [ -L "${AIRY_HOME}/$d" ] || ln -s "current/$d" "${AIRY_HOME}/$d" 2>/dev/null || true
    done
    # -n: dest 为目录符号链接时直接替换链接本身（mv 会移入目录内部）
    if ! ln -sfn "$rel_dir" "${AIRY_HOME}/current"; then
        rm -rf "$rel_dir"
        log_err "版本提交失败（current 切换被拒），现有安装未受影响"
        return 2
    fi
    rm -rf "${extracted}"
    AIRY_VERSION="v${rel_id}"
    log_ok "完全体二进制包安装完成（${AIRY_VERSION}，版本化布局）"
    return 0
}

fetch_prebuilt_module() {
    local name="$1" url="$2" mod_dir="$3" dest tarball
    dest="${MODULES_DIR}/${mod_dir}"
    tarball="${AIRY_HOME}/tmp/${mod_dir}.tar.gz"
    [ -n "$url" ] || { log_warn "未配置 ${name} 预编译包 URL，跳过"; return 1; }
    if [ -d "$dest" ]; then log_ok "${name} 预编译模块已就位"; return 0; fi
    log_info "下载闭源预编译模块 ${name}…"
    syscurl -fL ${CURL_FLAG} --max-time 600 -o "$tarball" "$url" || { log_warn "${name} 下载失败"; return 1; }
    mkdir -p "$dest"
    tar -xzf "$tarball" -C "$dest" || { log_warn "${name} 解压失败"; return 1; }
    log_ok "${name} 预编译模块就位: ${dest}"
    return 0
}

prepare_source() {
    if [ ! -d "${AIRY_SRC_DIR}/.git" ]; then
        log_info "git 拉取 airymaxhub（${AIRY_REPO_URL}）…"
        mkdir -p "$(dirname "${AIRY_SRC_DIR}")"
        if [ "$AIRY_VERSION_SPECIFIED" = "1" ]; then
            git clone --depth 1 -b "${AIRY_VERSION}" "${AIRY_REPO_URL}" "${AIRY_SRC_DIR}" \
                || { log_err "git 拉取失败（若子仓私有，请配置 AIRY_RELEASE_URL 走二进制模式）"; exit 1; }
        else
            git clone --depth 1 "${AIRY_REPO_URL}" "${AIRY_SRC_DIR}" \
                || { log_err "git 拉取失败（若子仓私有，请配置 AIRY_RELEASE_URL 走二进制模式）"; exit 1; }
        fi
        git -C "${AIRY_SRC_DIR}" submodule update --init --recursive --depth 1 2>/dev/null || \
            log_warn "部分子仓拉取受限（闭源模块将由预编译包补齐）"
    else
        log_info "airymaxhub 源码已存在，复用本地源码树"
        git -C "${AIRY_SRC_DIR}" fetch --all --tags --depth 1 >/dev/null 2>&1 || true
    fi

    if [ -d "${AIRY_SRC_APP}/agentrt" ]; then :; else AIRY_SRC_APP="${AIRY_SRC_DIR}"; fi
    local real_ver
    real_ver="$(cat "${AIRY_SRC_APP}/agentrt/VERSION" 2>/dev/null | tr -d '[:space:]')"
    if [ -n "$real_ver" ]; then
        AIRY_VERSION="v${real_ver}"
        log_ok "源码版本（SSoT）: ${AIRY_VERSION}"
    fi
    log_ok "源码就绪: ${AIRY_SRC_DIR}"
}

build_and_install() {
    local build_dir="${AIRY_HOME}/build"
    local cmake_args="-DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -DENABLE_SANITIZERS=OFF -DCMAKE_INSTALL_PREFIX=${AIRY_HOME}"

    if [ -d "${MODULES_DIR}/atoms" ]; then
        cmake_args="${cmake_args} -DAIRY_ATOMS_PREBUILT_DIR=${MODULES_DIR}/atoms"
    fi
    if [ -f "${MODULES_DIR}/memoryrovol/libagentrt_memoryrovol.a" ]; then
        cmake_args="${cmake_args} -DMEMORYROVOL_PRO_LIB=${MODULES_DIR}/memoryrovol/libagentrt_memoryrovol.a"
    fi

    log_info "cmake 配置（${cmake_args}）…"
    cmake -S "${AIRY_SRC_APP}/agentrt" -B "${build_dir}" ${cmake_args} \
        || { log_err "cmake 配置失败"; exit 1; }
    log_info "构建（-j${AIRY_BUILD_JOBS}）…"
    cmake --build "${build_dir}" -j"${AIRY_BUILD_JOBS}" || { log_err "构建失败"; exit 1; }
    log_info "安装到 ${AIRY_HOME}…"
    cmake --install "${build_dir}" || { log_err "安装失败（cmake --install）"; exit 1; }
    if [ -d "${build_dir}/bin" ]; then
        cp -f "${build_dir}"/bin/* "${AIRY_HOME}/bin/" 2>/dev/null || true
    fi
    local _d2 _binok=1
    for _d2 in $(daemon_list); do
        [ -x "${AIRY_HOME}/bin/${_d2}" ] || { _binok=0; log_err "构建产物缺失 daemon: ${_d2}"; break; }
    done
    [ "$_binok" = "1" ] || { log_err "源码构建安装不完整，请检查磁盘空间与权限后重试"; exit 1; }
    log_ok "源码构建安装完成"
}

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

install_maths_toolkit() {
    if [ "$WITH_MATHS" != "1" ]; then
        log_info "已跳过 maths-toolkit（--without-maths）"
        return 0
    fi
    local toolkit=""
    if [ -f "${AIRY_SRC_APP}/ecosystem/markets/tools/maths-toolkit/install.sh" ]; then
        toolkit="${AIRY_SRC_APP}/ecosystem/markets/tools/maths-toolkit/install.sh"
    elif [ -f "${AIRY_HOME}/modules/maths-toolkit/install.sh" ]; then
        toolkit="${AIRY_HOME}/modules/maths-toolkit/install.sh"
    fi
    if [ -z "$toolkit" ]; then
        log_warn "maths-toolkit 安装器不存在（源码与二进制模式均未携带），跳过数学后端预装"
        return 0
    fi
    if ! command -v python3 >/dev/null 2>&1; then
        log_warn "未找到 python3，跳过 maths-toolkit（maths_d 纯 C 快速路径可用）"
        return 0
    fi
    log_info "出厂预装数学计算后端（包内离线 wheel 优先 + 在线自动更新，失败降级纯 C 快速路径）…"
    local run_sh="sh"
    command -v bash >/dev/null 2>&1 && run_sh="bash"
    if run_spin "预装数学计算后端（maths-toolkit：离线 wheel 优先，失败降级纯 C 快速路径）…" \
        "$run_sh" "$toolkit" --airy-home "${AIRY_HOME}"; then
        if [ -x "${AIRY_HOME}/venv/bin/python3" ] && \
           "${AIRY_HOME}/venv/bin/python3" -c "import sympy" >/dev/null 2>&1; then
            log_ok "maths-toolkit 安装完成（maths_d 符号计算后端已就绪）"
        else
            log_warn "maths-toolkit 安装器返回成功但后端不可用（venv/sympy 缺失），已降级：maths_d 纯 C 快速路径可用；"
            log_warn "包内已内置离线 wheel，可重试: sh ${toolkit} --airy-home ${AIRY_HOME}"
        fi
    else
        log_warn "maths-toolkit 安装失败（离线 wheel 与在线源均不可用），已降级：maths_d 纯 C 快速路径可用；"
        log_warn "包内已内置离线 wheel，可重试: sh ${toolkit} --airy-home ${AIRY_HOME}"
    fi
}

build_tui() {
    [ -d "${AIRY_SRC_APP}/sdk/tui" ] || return 0
    export AIRY_HOME
    if ! command -v cargo >/dev/null 2>&1 && [ -x "$HOME/.cargo/bin/cargo" ]; then
        export PATH="$HOME/.cargo/bin:$PATH"
    fi
    command -v cargo >/dev/null 2>&1 || { log_warn "cargo 不可用，跳过 agentrt-tui"; return 0; }
    export CARGO_TARGET_DIR="${AIRY_HOME}/target"
    log_info "构建 agentrt-tui（Rust TUI，产物 → ${CARGO_TARGET_DIR}）…"
    ( cd "${AIRY_SRC_APP}/sdk/tui" && cargo build --release ) 2>/dev/null || { log_warn "TUI 构建失败，跳过"; return 0; }
    [ -f "${CARGO_TARGET_DIR}/release/agentrt-tui" ] && \
        cp -f "${CARGO_TARGET_DIR}/release/agentrt-tui" "${AIRY_HOME}/bin/agentrt-tui"
    log_ok "agentrt-tui 部署完成"
}

init_secrets() {
    local secrets="${AIRY_HOME}/config/secrets.env"
    if [ ! -f "${secrets}" ]; then
        local template="${AIRY_SRC_DIR}/tools/scripts/ops/templates/secrets.env.example"
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

path_rc_file() {
    case "$(basename "${SHELL:-/bin/sh}")" in
        zsh) echo "$HOME/.zshrc" ;;
        fish) echo "$HOME/.config/fish/config.fish" ;;
        sh|dash|ash) echo "$HOME/.profile" ;;
        *) echo "$HOME/.bashrc" ;;
    esac
}

path_bootstrap() {
    local rc
    rc="$(path_rc_file)"
    if [ -f "$rc" ] && grep -q '# >>> AgentRT PATH bootstrap <<<' "$rc" 2>/dev/null; then
        log_info "PATH 引导: $rc 已包含 AgentRT PATH 行（幂等跳过）"
        env_set "AIRY_PATH_RC=$rc"
        env_set "AIRY_PATH_APPENDED=yes"
        return 0
    fi
    local line
    if [ "$(basename "$rc")" = "config.fish" ]; then
        line="set -gx PATH \"${BIN_DIR}\" \$PATH"
    else
        line="export PATH=\"${BIN_DIR}:\$PATH\""
    fi
    if { printf '\n# >>> AgentRT PATH bootstrap <<<\n%s\n# <<< AgentRT PATH bootstrap <<<\n' "$line" ; } >> "$rc" 2>/dev/null; then
        log_ok "PATH 引导: 已自动追加 ${BIN_DIR} 到 ${rc}（新开终端生效，或执行 source \"$rc\"）"
        env_set "AIRY_PATH_RC=$rc"
        env_set "AIRY_PATH_APPENDED=yes"
        return 0
    fi
    log_warn "PATH 引导: 无法写入 ${rc}（自动追加失败），请手动执行:"
    log_warn "  echo '${line}' >> \"$rc\" && source \"$rc\""
    env_set "AIRY_PATH_APPENDED=no"
    return 1
}

_uspace_bits() {
    local _b
    _b="$(getconf LONG_BIT 2>/dev/null)"
    case "$_b" in 32|64) echo "$_b"; return ;; esac
    if [ -r /proc/self/exe ] && command -v od >/dev/null 2>&1; then
        _b="$(od -An -j4 -N1 -t x1 /proc/self/exe 2>/dev/null | tr -d ' \n')"
        case "$_b" in 01) echo 32; return ;; 02) echo 64; return ;; esac
    fi
    if command -v dpkg >/dev/null 2>&1; then
        _b="$(dpkg --print-architecture 2>/dev/null)"
        case "$_b" in
            amd64|arm64|riscv64) echo 64; return ;;
            armhf|armel|i386)    echo 32; return ;;
        esac
    fi
    echo ""
}
_loader_exists() {
    local _n="$1" _p
    for _p in "/lib/$_n" "/lib64/$_n" "/usr/lib/$_n" \
              /lib/*-linux-gnu*/"$_n" /usr/lib/*-linux-gnu*/"$_n"; do
        [ -f "$_p" ] && return 0
    done
    return 1
}
_arch_warn_unknown() {
    printf '%s[WARN]%s %s\n' "$C_YELLOW" "$C_NC" \
        "架构判定: 用户空间位宽未知（getconf/ELF/dpkg 均不可用），按 $1 处理" >&2
}
detect_arch() {
    local _m _bits
    _m="$(uname -m 2>/dev/null)"
    _bits="$(_uspace_bits)"
    case "$_m" in
        x86_64|amd64)
            if [ "$_bits" = "32" ]; then
                echo "i686"
            elif [ "$_bits" = "64" ] || _loader_exists ld-linux-x86-64.so.2; then
                echo "x86_64"
            elif _loader_exists ld-linux.so.2; then
                echo "i686"
            else
                _arch_warn_unknown "$_m"
                echo "i686"
            fi
            ;;
        i386|i486|i586|i686|x86) echo "i686" ;;
        aarch64|arm64)
            if [ "$_bits" = "32" ]; then
                echo "armv7l"
            elif [ "$_bits" = "64" ] || _loader_exists ld-linux-aarch64.so.1; then
                echo "aarch64"
            elif _loader_exists ld-linux-armhf.so.3; then
                echo "armv7l"
            else
                _arch_warn_unknown "$_m"
                echo "aarch64"
            fi
            ;;
        armv7l|armv6l|armhf) echo "armv7l" ;;
        riscv64)
            if [ "$_bits" = "32" ]; then
                echo "riscv32"
            elif [ "$_bits" = "64" ] || _loader_exists ld-linux-riscv64-lp64d.so.1; then
                echo "riscv64"
            else
                _arch_warn_unknown "$_m"
                echo "riscv32"
            fi
            ;;
        riscv32|riscv) echo "riscv32" ;;
        *)                echo "unknown" ;;
    esac
}
SUPPORTED_ARCHS="x86_64 aarch64 i686 armv7l"

plat_name() {
    case "$1" in
        x86_64)  echo "x86-64" ;;
        i686)    echo "x86-32" ;;
        aarch64) echo "arm-64" ;;
        armv7l)  echo "arm-32" ;;
        riscv64) echo "riscv-64" ;;
        riscv32) echo "riscv-32" ;;
        *)       echo "$1" ;;
    esac
}
plat_legacy_name() {
    case "$1" in
        linux-x86-64|macos-x86-64|win-x86-64) printf '%s\n' "${1%-x86-64}-x64" "${1%-x86-64}-x86_64" ;;
        linux-x86-32|macos-x86-32|win-x86-32) printf '%s\n' "${1%-x86-32}-x86" "${1%-x86-32}-i686" ;;
        linux-arm-64|macos-arm-64|win-arm-64) printf '%s\n' "${1%-arm-64}-arm64" "${1%-arm-64}-aarch64" ;;
        linux-arm-32|macos-arm-32|win-arm-32) printf '%s\n' "${1%-arm-32}-arm32" "${1%-arm-32}-armv7l" ;;
        linux-riscv-64|win-riscv-64)          echo "${1%-riscv-64}-riscv64" ;;
        linux-riscv-32|win-riscv-32)          echo "${1%-riscv-32}-riscv32" ;;
        *)                                    echo "" ;;
    esac
}
plat_markers() {
    case "$1" in
        x86_64)  printf 'platform-x86-64 platform-x64 platform-x86_64 platform-amd64' ;;
        i686)    printf 'platform-x86-32 platform-x86 platform-i686 platform-i386' ;;
        aarch64) printf 'platform-arm-64 platform-arm64 platform-aarch64' ;;
        armv7l)  printf 'platform-arm-32 platform-arm32 platform-armv7l' ;;
        riscv64) printf 'platform-riscv-64 platform-riscv64' ;;
        riscv32) printf 'platform-riscv-32 platform-riscv32' ;;
        *)       echo "" ;;
    esac
}

arch_markers_ok() {
    local _t="$1" _a="$2" _ms _m _p _bad=""
    _ms="$(tar -tzf "$_t" 2>/dev/null | awk -F/ '{print $NF}' \
        | grep -E '^platform-[A-Za-z0-9_-]+$' | sort -u || true)"
    [ -n "$_ms" ] || return 0
    for _m in $_ms; do
        for _p in $(plat_markers "$_a"); do
            if [ "$_m" = "$_p" ]; then return 0; fi
        done
        _bad="$_bad $_m"
    done
    printf '%s' "${_bad# }"
    return 1
}

human_size() {
    local _b="${1:-}"
    case "$_b" in ''|*[!0-9]*) echo "未知"; return 0 ;; esac
    if [ "$_b" -ge 1048576 ]; then
        echo "$((_b / 1048576)).$((_b % 1048576 / 104858))MiB"
    elif [ "$_b" -ge 1024 ]; then
        echo "$((_b / 1024))KiB"
    else
        echo "${_b}B"
    fi
}

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
    local mem_kib="" mem_avail_kib="" nproc_val="" accel profile raw
    if [ -r /proc/meminfo ]; then
        mem_kib="$(awk '/^MemTotal:/{print $2}' /proc/meminfo 2>/dev/null || true)"
        mem_avail_kib="$(awk '/^MemAvailable:/{print $2}' /proc/meminfo 2>/dev/null || true)"
    elif command -v sysctl >/dev/null 2>&1; then
        raw="$(sysctl -n hw.memsize 2>/dev/null || true)"
        case "${raw:-0}" in ''|*[!0-9]*) raw=0 ;; esac
        mem_kib=$((raw / 1024))
        mem_avail_kib="$mem_kib"
    fi
    case "${mem_kib:-}" in ''|*[!0-9]*) mem_kib="" ;; esac
    case "${mem_avail_kib:-}" in ''|*[!0-9]*) mem_avail_kib="$mem_kib" ;; esac
    if command -v nproc >/dev/null 2>&1; then
        nproc_val="$(nproc 2>/dev/null || true)"
    elif command -v sysctl >/dev/null 2>&1; then
        nproc_val="$(sysctl -n hw.ncpu 2>/dev/null || true)"
    fi
    case "${nproc_val:-}" in ''|*[!0-9]*) nproc_val="" ;; esac
    if [ -z "$mem_kib" ] || [ "$mem_kib" -eq 0 ] || [ -z "$nproc_val" ]; then
        printf 'assess_hardware: 硬件探测失败（内存/核数不可得），拒绝静默降级为 minimal\n' >&2
        return 1
    fi
    accel="$(detect_accel)"
    if [ "$mem_kib" -ge $((2560 * 1024)) ] && \
       [ "$mem_avail_kib" -ge $((1536 * 1024)) ] && \
       [ "$nproc_val" -ge 3 ]; then
        profile="full"
    else
        profile="minimal"
    fi
    printf '%s|%s|%s|%s|%s' "$profile" "$mem_kib" "${mem_avail_kib:-0}" "$nproc_val" "$accel"
}

persist_profile() {
    local hw hw_profile mem total avail cores accel
    if ! hw="$(assess_hardware)"; then
        if [ "$AIRY_PROFILE" != "auto" ]; then
            log_warn "硬件探测失败，按显式画像 ${AIRY_PROFILE} 继续（硬件快照置 0）"
            hw="${AIRY_PROFILE}|0|0|1|none"
        else
            log_err "硬件探测失败，无法自动评估画像；请用 --profile full|minimal 显式指定后重试"
            exit 1
        fi
    fi
    hw_profile="${hw%%|*}"
    mem="${hw#*|}"
    total="${mem%%|*}"; mem="${mem#*|}"
    avail="${mem%%|*}"; mem="${mem#*|}"
    cores="${mem%%|*}"
    accel="${mem#*|}"
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
    log_ok "运行画像已固化: ${hw_profile}（详情见 config/profile.env）"
    log_info "  硬件变化后执行 'airymaxrt profile' 重评估，或 'airymaxrt monitor --daemon' 自动恢复被裁剪功能"
}

finalize_install() {
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
        echo "AIRY_ARTIFACT_SHA256=${AIRY_ARTIFACT_SHA256:-}"
        echo "AIRY_CHANNEL=${AIRY_CHANNEL}"
        echo "AIRY_BIN_LINK=${BIN_DIR}/airymaxrt"
        echo "INSTALLED_AT=$(date -Is 2>/dev/null || date '+%Y-%m-%dT%H:%M:%S%z')"
        echo "AIRY_VAULT_PASSWORD=${vault_password}"
    } > "${AIRY_HOME}/config/install.env"
    chmod 600 "${AIRY_HOME}/config/install.env"

    cat > "${AIRY_HOME}/bin/agentrt-env.sh" <<'AIRY_ENV_EOF'
#!/bin/sh
AIRY_HOME="${AIRY_HOME:-__AIRY_HOME__}"
export AIRY_HOME
export AIRY_RUNTIME_DIR="$AIRY_HOME/run"
export AIRY_LOG_DIR="$AIRY_HOME/data/agentrt/logs"
export AIRY_CONFIG_DIR="$AIRY_HOME/config"
export AIRY_BIN_DIR="$AIRY_HOME/bin"
export AIRY_LIB_DIR="$AIRY_HOME/lib"
export AIRY_DATA_DIR="$AIRY_HOME/data"
export AIRY_CACHE_DIR="$AIRY_HOME/data/agentrt/cache"
export AIRY_TMP_DIR="$AIRY_HOME/data/agentrt/tmp"
export AIRY_WORKSPACE_DIR="$AIRY_HOME/data/agentrt/workspaces"
export PYTHONPYCACHEPREFIX="${PYTHONPYCACHEPREFIX:-$AIRY_HOME/data/agentrt/cache/pycache}"
export AIRY_AGENT_ACL="${AIRY_AGENT_ACL:-}"
export PATH="${AIRY_HOME}/bin:$PATH"
export LD_LIBRARY_PATH="${AIRY_HOME}/lib:${LD_LIBRARY_PATH:-}"
AIRY_ENV_EOF
    sed "s|__AIRY_HOME__|${AIRY_HOME}|g" "${AIRY_HOME}/bin/agentrt-env.sh" > "${AIRY_HOME}/bin/agentrt-env.sh.tmp" && \
        mv "${AIRY_HOME}/bin/agentrt-env.sh.tmp" "${AIRY_HOME}/bin/agentrt-env.sh"
    chmod 700 "${AIRY_HOME}/bin/agentrt-env.sh"

    if [ -f "${AIRY_SRC_APP}/agentrt/latest/airymaxrt" ]; then
        cp -f "${AIRY_SRC_APP}/agentrt/latest/airymaxrt" "${AIRY_HOME}/bin/airymaxrt"
        chmod 755 "${AIRY_HOME}/bin/airymaxrt"
    elif [ -f "${AIRY_SRC_APP}/sdk/tui/scripts/airymaxrt" ]; then
        cp -f "${AIRY_SRC_APP}/sdk/tui/scripts/airymaxrt" "${AIRY_HOME}/bin/airymaxrt"
        chmod 755 "${AIRY_HOME}/bin/airymaxrt"
    else
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
_AH=""
if [ -f "\${_DIR}/../config/install.env" ]; then
    _AH="\$(sed -n 's/^AIRY_HOME=//p' "\${_DIR}/../config/install.env" 2>/dev/null | head -1)"
    [ -x "\$_AH/bin/airy_cli" ] || _AH=""
fi
if [ -z "\$_AH" ]; then
    _AH="\${AIRY_HOME:-}"
    [ -n "\$_AH" ] && [ -x "\$_AH/bin/airy_cli" ] || _AH=""
fi
[ -n "\$_AH" ] || _AH="\$HOME/.airymaxrt"
AIRY_HOME="\$_AH"
export AIRY_HOME
syscurl() {
    _sc_ldp=""
    _sc_rest="\${LD_LIBRARY_PATH:-}"
    _sc_seg=""
    while [ -n "\$_sc_rest" ]; do
        _sc_seg="\${_sc_rest%%:*}"
        [ "\$_sc_seg" = "\${AIRY_HOME}/lib" ] || _sc_ldp="\${_sc_ldp:+\$_sc_ldp:}\$_sc_seg"
        [ "\$_sc_seg" = "\$_sc_rest" ] && _sc_rest="" || _sc_rest="\${_sc_rest#*:}"
    done
    if [ -n "\$_sc_ldp" ]; then
        env LD_LIBRARY_PATH="\$_sc_ldp" curl "\$@"
    else
        env -u LD_LIBRARY_PATH curl "\$@"
    fi
}
if [ -f "\${AIRY_HOME}/bin/agentrt-env.sh" ]; then
    . "\${AIRY_HOME}/bin/agentrt-env.sh"
    AIRY_HOME="\$_AH"
fi
case ":\${LD_LIBRARY_PATH:-}:" in
    *":\${AIRY_HOME}/lib:"*) ;;
    *) export LD_LIBRARY_PATH="\${AIRY_HOME}/lib:\${LD_LIBRARY_PATH:-}" ;;
esac
_BINLINK="\$(sed -n 's/^AIRY_BIN_LINK=//p' "\${AIRY_HOME}/config/install.env" 2>/dev/null | head -1)"
_BINLINK="\${_BINLINK:-\$HOME/.local/bin/airymaxrt}"
_BINDIR="\${_BINLINK%/airymaxrt}"
_INPATH=0
_P="\$PATH"
while [ -n "\$_P" ]; do
    _SEG="\${_P%%:*}"
    [ "\$_SEG" = "\$_BINDIR" ] && _INPATH=1
    [ "\$_SEG" = "\$_P" ] && _P="" || _P="\${_P#*:}"
    [ "\$_INPATH" = "1" ] && break
done
if [ "\$_INPATH" != "1" ] && [ -n "\$_BINDIR" ]; then
    case "\$(basename "\${SHELL:-/bin/sh}")" in
        zsh) _RC="\$HOME/.zshrc" ;;
        fish) _RC="\$HOME/.config/fish/config.fish" ;;
        sh|dash|ash) _RC="\$HOME/.profile" ;;
        *) _RC="\$HOME/.bashrc" ;;
    esac
    if ! grep -q '# >>> AgentRT PATH bootstrap <<<' "\$_RC" 2>/dev/null; then
        if [ "\$(basename "\$_RC")" = "config.fish" ]; then
            _LINE=\$(printf 'set -gx PATH "%s" \$PATH' "\$_BINDIR")
        else
            _LINE=\$(printf 'export PATH="%s:\$PATH"' "\$_BINDIR")
        fi
        printf '\n# >>> AgentRT PATH bootstrap <<<\n%s\n# <<< AgentRT PATH bootstrap <<<\n' "\$_LINE" >> "\$_RC" 2>/dev/null \
            && echo "airymaxrt: 已自动将 \${_BINDIR} 追加到 \${_RC}（新开终端生效，或 source \"\$_RC\"）" >&2
    fi
fi
case "\$1" in
    start|cli|profile|monitor|status|doctor|logs|uninstall|update|reinstall)
        _FULL="\$AIRY_HOME/bin/airymaxrt-full"
        if [ "\$1" != "update" ] && [ -s "\$_FULL" ]; then
            exec bash "\$_FULL" "\$@"
        fi
        _TMP="\$AIRY_HOME/tmp/airymaxrt-full.\$\$"
        mkdir -p "\$AIRY_HOME/tmp" || exit 1
        syscurl -fsSL --max-time 60 -o "\$_TMP" \
            "${AIRY_RELEASE_BASE}/latest/airymaxrt" || {
            echo "airymaxrt \$1: 完整启动器下载失败（发布面 releases/download 不可达）" >&2
            rm -f "\$_TMP"; exit 1
        }
        if [ ! -s "\$_TMP" ]; then
            echo "airymaxrt \$1: 完整启动器下载为空（发布面附件缺失）" >&2
            rm -f "\$_TMP"; exit 1
        fi
        mv -f "\$_TMP" "\$_FULL" || { rm -f "\$_TMP"; exit 1; }
        chmod 755 "\$_FULL"
        exec bash "\$_FULL" "\$@"
        ;;
    "")
        _FULL="\$AIRY_HOME/bin/airymaxrt-full"
        _GWP="\$(sed -n '1s/[^0-9]//gp' "\$AIRY_HOME/run/gateway.port" 2>/dev/null | head -1)"
        _GWP="\${_GWP:-8080}"
        if [ -s "\$_FULL" ]; then
            exec bash "\$_FULL"
        fi
        if [ -x "\$AIRY_HOME/bin/agentrt-bootstrap.sh" ]; then
            _GW_READY=0
            if command -v curl >/dev/null 2>&1; then
                syscurl -fsS --max-time 1 -X POST "http://127.0.0.1:\$_GWP/" \
                    -H 'Content-Type: application/json' \
                    -d '{"jsonrpc":"2.0","id":1,"method":"ping","params":{}}' \
                    >/dev/null 2>&1 && _GW_READY=1
            fi
            if [ "\$_GW_READY" != "1" ]; then
                echo "airymaxrt: gateway 离线，拉起守护进程群…" >&2
                "\$AIRY_HOME/bin/agentrt-bootstrap.sh" start >/dev/null 2>&1 || true
            fi
        fi
        ;;
esac
if [ ! -x "\$AIRY_HOME/bin/airy_cli" ]; then
    echo "airymaxrt: airy_cli 缺失（\$AIRY_HOME/bin/airy_cli），请重新安装" >&2
    exit 1
fi
if [ -t 0 ] && [ -t 1 ]; then
    exec "\$AIRY_HOME/bin/airy_cli" --tui
fi
exec "\$AIRY_HOME/bin/airy_cli" -p
EOF
        chmod 755 "${AIRY_HOME}/bin/airymaxrt"
    fi
    if [ -x "${AIRY_HOME}/bin/airymaxrt" ]; then
        mkdir -p "${BIN_DIR}"
        ln -sf "${AIRY_HOME}/bin/airymaxrt" "${BIN_DIR}/airymaxrt"
        log_ok "启动器软链: ${BIN_DIR}/airymaxrt → ${AIRY_HOME}/bin/airymaxrt"
    fi

    if [ -f "${AIRY_SRC_DIR}/tools/scripts/ops/bin/agentrt-bootstrap.sh" ]; then
        cp -f "${AIRY_SRC_DIR}/tools/scripts/ops/bin/agentrt-bootstrap.sh" "${AIRY_HOME}/bin/agentrt-bootstrap.sh"
        chmod 755 "${AIRY_HOME}/bin/agentrt-bootstrap.sh"
        log_ok "agentrt-bootstrap.sh 已部署到 bin/"
    elif [ -f "${AIRY_HOME}/bin/agentrt-bootstrap.sh" ]; then
        log_ok "agentrt-bootstrap.sh 已存在（二进制模式自带）"
    else
        log_warn "agentrt-bootstrap.sh 未部署（源码缺失且二进制未含）"
    fi

    mkdir -p "${AIRY_HOME}/scripts"
    if [ -f "$0" ] && [ -r "$0" ]; then
        cp -f "$0" "${AIRY_HOME}/scripts/install.sh"
    else
        fetch_release_asset "install.sh" "${AIRY_HOME}/scripts/install.sh" >/dev/null 2>&1 || true
        if [ ! -s "${AIRY_HOME}/scripts/install.sh" ]; then
            log_warn "安装器自托管失败（管道执行且发布面附件拉取失败）；airymaxrt uninstall 将提示在线兜底"
        fi
    fi
    chmod 755 "${AIRY_HOME}/scripts/install.sh" 2>/dev/null || true
    log_ok "安装位置已固化: install.env + agentrt-env.sh + 启动器"

    env_set "AIRY_BIN_LINK=${BIN_DIR}/airymaxrt"
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
        env_set "AIRY_BIN_DIR_IN_PATH=yes"
    else
        log_warn "PATH 引导: ${BIN_DIR} 不在 PATH 中——当前 shell 输入 airymaxrt 会报 command not found"
        path_bootstrap
        env_set "AIRY_BIN_DIR_IN_PATH=no"
    fi
}

verify_daemons() {
    local missing="" strict="$1" dl d n=0
    dl="$(daemon_list)"
    if [ -z "$dl" ]; then
        if [ "$strict" = "strict" ]; then
            log_err "daemon 校验失败：${AIRY_HOME}/bin 下无 *_d 二进制（制品不完整）"
            exit 1
        fi
        log_warn "daemon 校验未全通过：${AIRY_HOME}/bin 下无 *_d 二进制"
        return 1
    fi
    for d in ${dl}; do
        n=$((n + 1))
        [ -x "${AIRY_HOME}/bin/${d}" ] || missing="${missing} ${d}"
    done
    if [ -n "$missing" ]; then
        if [ "$strict" = "strict" ]; then
            log_err "daemon 校验失败，缺失:${missing}（二进制包不完整，请检查 release 制品）"
            exit 1
        fi
        log_warn "daemon 校验未全通过，缺失:${missing}（可能为二进制包未含全部组件）"
    else
        log_ok "${n} 个 daemon 全部就位"
    fi
}

post_install_selfcheck() {
    local ver_installed=""
    if [ -f "${AIRY_HOME}/config/install.env" ]; then
        ver_installed="$(sed -n 's/^AIRY_VERSION=//p' "${AIRY_HOME}/config/install.env" 2>/dev/null | head -1)"
    fi
    log_ok "已安装版本: ${ver_installed:-v?}（通道: ${AIRY_CHANNEL}）"
    if [ "${AIRY_CHANNEL}" != "stable" ]; then
        log_warn "${AIRY_CHANNEL} 通道为非生产通道；正式环境建议 'airymaxrt update --channel stable' 切回"
    fi
    log_info "更新检查: airymaxrt update --check    升级: airymaxrt update"

    local _missing=0 _b
    for _b in "${AIRY_HOME}"/bin/*; do
        [ -f "$_b" ] || continue
        if head -c4 "$_b" 2>/dev/null | od -An -tx1 | grep -q "7f 45 4c 46"; then
            if command -v ldd >/dev/null 2>&1 && ldd "$_b" 2>/dev/null | grep -q "not found"; then
                log_warn "$(basename "$_b") 存在未解析动态库依赖:"
                ldd "$_b" 2>/dev/null | grep "not found" | sed 's/^/    /'
                _missing=$((_missing+1))
            fi
        fi
    done
    if [ "$_missing" -gt 0 ]; then
        log_warn "检测到 ${_missing} 个二进制缺少运行时库。"
        log_warn "若 libcjson.so.1 等随包 .so 未生效，请确认 AIRY_LIB_PATH 已含 ${AIRY_HOME}/lib，或重装本版本。"
    else
        log_ok "运行时依赖校验通过（全部二进制动态库解析正常）"
    fi
}

print_banner() {
    cat <<EOF
${C_CYAN}
  ┌─────────────────────────────────────────────┐
  │  Airymax AgentRT · Agent Runtime Platform   │
  └─────────────────────────────────────────────┘
${C_NC}
EOF
}

stage() {
    printf '%s\n  ── [%s/%s] %s ───────────────────────────%s\n' "$C_CYAN" "$1" "$2" "$3" "$C_NC"
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

print_path_guidance() {
    _found=0
    _ifs="$IFS"
    IFS=:
    for _p in $PATH; do
        [ -n "$_p" ] || _p="."
        if [ "$_p" = "$BIN_DIR" ]; then _found=1; break; fi
    done
    IFS="$_ifs"

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

main() {
    print_banner
    log_info "Airymax AgentRT 安装程序"
    log_info "AIRY_HOME = ${AIRY_HOME} | 模式 = ${AIRY_MODE}"

    if [ "$UNINSTALL" = "1" ]; then
        do_uninstall "$AIRY_HOME" "$KEEP_DATA" "$YES"
        exit $?
    fi

    stage 1 5 "准备安装环境"
    init_home
    detect_existing_install

    if [ "$REINSTALL" = "1" ]; then
        log_warn "重装模式：清除本地包缓存并停止旧 daemon，强制下载最新版本…"
        rm -f "${AIRY_HOME}"/tmp/agentrt-*.tar.gz 2>/dev/null || true
        stop_daemons "$AIRY_HOME/bin"
    fi

    local installed=1 _bin_rc=0
    stage 2 5 "获取运行时"
    local release_url="${AIRY_RELEASE_URL:-}"
    if [ -z "$release_url" ] && [ "${AIRY_MODE:-auto}" != "source" ]; then
        release_url="${AIRY_RELEASE_BASE}/latest/manifest.${AIRY_CHANNEL}.json"
    fi
    if [ -n "$AIRY_FROM_FILE" ]; then
        install_binary "$AIRY_FROM_FILE"; _bin_rc=$?
        [ "$_bin_rc" = "0" ] && installed=0
    elif [ "$AIRY_MODE" = "binary" ] || { [ "$AIRY_MODE" = "auto" ] && [ -n "$release_url" ]; }; then
        install_binary "$release_url"; _bin_rc=$?
        [ "$_bin_rc" = "0" ] && installed=0
    fi

    if [ "$_bin_rc" = "2" ]; then
        log_err "二进制安装失败（详见上方错误）。已停止，未进入源码构建。"
        exit 1
    fi
    if [ "$_bin_rc" = "1" ] && [ "$AIRY_MODE" = "binary" ]; then
        log_err "官方未发布当前平台的预编译制品，且你指定 --mode binary（禁止源码构建）。"
        log_err "可改 --mode auto 或 hybrid 自动源码构建，或 AIRY_MODE=source 显式源码构建。"
        exit 1
    fi

    if [ "$installed" -ne 0 ]; then
        if [ "$AIRY_MODE" = "auto" ]; then
            log_warn "当前平台暂无官方预编译制品（${AIRY_MODE}），转为源码构建…"
        else
            log_info "进入源码构建模式（${AIRY_MODE}）"
        fi
        check_toolchain
        prepare_source

        if [ ! -d "${AIRY_SRC_APP}/agentrt/atoms" ] && [ "$AIRY_MODE" != "source" ]; then
            fetch_prebuilt_module "atoms" \
                "${AIRY_ATOMS_PREBUILT_URL:-https://atomgit.com/openairymax/agentrt/releases/download/${AIRY_VERSION}/airy-atoms-prebuilt-${AIRY_VERSION}-linux-$(detect_arch).tar.gz}" \
                "atoms" || \
                log_warn "atoms 预编译包不可用；如需完整功能请配置 AIRY_ATOMS_PREBUILT_URL 或使用本地源码"
            fetch_prebuilt_module "memoryrovol" "${AIRY_MEMORYROVOL_PREBUILT_URL:-}" "memoryrovol" || \
                log_warn "memoryrovol 预编译包不可用（无授权将自动降级 OSS/builtin）"
        elif [ "$AIRY_MODE" = "source" ] && [ -d "${AIRY_SRC_APP}/agentrt/atoms" ]; then
            log_ok "模式 C：本地闭源源码（atoms/memory/memoryrovol）全量构建"
        fi

        if [ "${AIRY_NO_BUILD:-}" != "1" ]; then
            build_and_install
            install_python_deps
            build_tui
        fi
    fi

    stage 3 5 "部署组件与运行配置"
    init_secrets

    persist_profile

    if stop_daemons "$AIRY_HOME/bin"; then
        log_ok "已自动停止旧版本 daemon（新版本已就位，运行 airymaxrt 启动）"
    fi

    stage 4 5 "预装数学计算后端"
    install_maths_toolkit

    stage 5 5 "校验与完成"
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
