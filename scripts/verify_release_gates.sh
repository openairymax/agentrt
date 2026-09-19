#!/usr/bin/env bash

set -u

PASS=0
FAIL=0
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

ok()   { PASS=$((PASS+1)); printf '  [PASS] %s\n' "$1"; }
bad()  { FAIL=$((FAIL+1)); printf '  [FAIL] %s\n' "$1"; }
skip() { printf '  [SKIP] %s\n' "$1"; }
section() { printf '\n[组%s] %s\n' "$1" "$2"; }

INSTALL="$ROOT/scripts/install.sh"
INSTALL_PS1="$ROOT/scripts/install.ps1"
LATEST_RT="$ROOT/latest/airymaxrt"
CLI="$ROOT/tools/airy_cli"

SDK_AIRYMAXRT="${AIRY_GATE_SDK_AIRYMAXRT:-}"
SDK_EXPLICIT=0
[ -n "$SDK_AIRYMAXRT" ] && SDK_EXPLICIT=1
if [ "$SDK_EXPLICIT" -eq 0 ]; then
    for _cand in "$ROOT/../sdk/tui/scripts/airymaxrt" \
                 "$ROOT/agent-workload/sdk/tui/scripts/airymaxrt"; do
        if [ -f "$_cand" ]; then SDK_AIRYMAXRT="$_cand"; break; fi
    done
fi

sdk_ready() {
    if [ "$SDK_EXPLICIT" -eq 1 ] && [ ! -f "$SDK_AIRYMAXRT" ]; then
        return 2
    fi
    [ -f "$SDK_AIRYMAXRT" ] && return 0
    return 1
}

SDK_CONSOLE="${AIRY_GATE_CONSOLE_DIR:-}"
if [ -z "$SDK_CONSOLE" ]; then
    for _cand in "$ROOT/../sdk/console" "$ROOT/agent-workload/sdk/console"; do
        if [ -d "$_cand" ]; then SDK_CONSOLE="$_cand"; break; fi
    done
fi

extract_fn() { # <file> <fnname> <outfile>
    sed -n "/^$2() {/,/^}$/p" "$1" > "$3"
}

_missing=0
for _f in "$INSTALL" "$INSTALL_PS1" "$LATEST_RT" \
          "$CLI/src/chat/cli_chat.c" \
          "$CLI/src/tui/cli_tui_internal.h" \
          "$CLI/src/tui/tui_history.c" \
          "$CLI/src/cmd/cli_gw.c" \
          "$CLI/include/cli_internal.h" \
          "$CLI/src/chat/cli_chat_usage.c" \
          "$CLI/src/chat/cli_chat_history.c"; do
    if [ ! -f "$_f" ]; then
        printf '  [FAIL] 结构性文件缺失: %s\n' "$_f"
        _missing=1
    fi
done
if [ "$_missing" -eq 1 ]; then
    printf '\n门禁汇总: PASS=%d FAIL>0（结构性文件缺失，中止逐条检查）\n' "$PASS"
    exit 1
fi

section "A" "9.2/I-01 install.sh 前向兼容（安装/启动问题：函数先定义后调用）"

if bash -n "$INSTALL" 2>/dev/null; then
    ok "A1 bash -n 语法零错"
else
    bad "A1 install.sh 存在语法错误"
fi

_err="$TMP/help.err"
if AIRY_INSTALLER_BOOTSTRAPPED=1 bash "$INSTALL" --help >/dev/null 2>"$_err" \
   && ! grep -q 'command not found' "$_err"; then
    ok "A2 --help 快速路径 rc=0 且 stderr 零 command not found（AIRY_INSTALLER_BOOTSTRAPPED=1 跳过 self_bootstrap，零网络）"
else
    bad "A2 --help 运行失败或 stderr 出现 command not found（前向兼容破裂）"
fi

if awk '
    /^[A-Za-z_][A-Za-z0-9_]*\(\) \{/ { lastdef = NR }
    /^main "\$@"$/ { mainline = NR }
    END { exit !(mainline > 0 && lastdef > 0 && lastdef < mainline) }
' "$INSTALL"; then
    ok "A3 全部顶层函数定义行号 < main 调用行（curl 管道 bash 直跑形态）"
else
    bad "A3 存在函数定义晚于 main 调用行（直跑会 command not found）"
fi

section "B" "9.3/I-02① 架构检测 + 平台键/标记判定 + 体积渲染三副本逐字节一致（install.sh / latest / sdk）"

for _fn in _uspace_bits _loader_exists _arch_warn_unknown detect_arch assess_hardware detect_accel \
           plat_markers arch_markers_ok human_size plat_name plat_legacy_name; do
    extract_fn "$INSTALL"   "$_fn" "$TMP/a_$_fn"
    extract_fn "$LATEST_RT" "$_fn" "$TMP/b_$_fn"

    case "$(sdk_ready; echo $?)" in
        0)
            extract_fn "$SDK_AIRYMAXRT" "$_fn" "$TMP/c_$_fn"
            if [ "$_fn" = "_arch_warn_unknown" ]; then
                _w=0
                for _g in "$TMP/a_$_fn" "$TMP/b_$_fn" "$TMP/c_$_fn"; do
                    grep -q '用户空间位宽未知' "$_g" || _w=1
                done
                if [ "$_w" -eq 0 ]; then
                    ok "B($_fn) 三副本检测语义一致（呈现设施各异：printf>&2 / log warn，SSoT=文案）"
                else
                    bad "B($_fn) 告警文案漂移"
                fi
            elif [ "$_fn" = "assess_hardware" ]; then
                _w=0
                for _g in "$TMP/a_$_fn" "$TMP/b_$_fn" "$TMP/c_$_fn"; do
                    grep -q 'hw.memsize' "$_g" && grep -q '拒绝静默降级' "$_g" || _w=1
                done
                if [ "$_w" -eq 0 ] && cmp -s "$TMP/a_$_fn" "$TMP/b_$_fn" && cmp -s "$TMP/a_$_fn" "$TMP/c_$_fn"; then
                    ok "B($_fn) 三副本逐字节一致且含 sysctl 回退+显式失败守卫（macOS 探测 SSoT）"
                else
                    bad "B($_fn) 副本漂移或缺失 macOS 探测守卫（sysctl 回退/显式失败）"
                fi
            elif cmp -s "$TMP/a_$_fn" "$TMP/b_$_fn" && cmp -s "$TMP/a_$_fn" "$TMP/c_$_fn"; then
                ok "B($_fn) 三副本提取 cmp 逐字节一致"
            else
                bad "B($_fn) 三副本存在漂移（install.sh / latest/airymaxrt / sdk airymaxrt）"
            fi
            ;;
        1)
            if cmp -s "$TMP/a_$_fn" "$TMP/b_$_fn"; then
                skip "B($_fn) 仓内两副本一致（sdk 未检出，跨仓比对跳过）"
            else
                bad "B($_fn) 仓内两副本漂移（install.sh / latest/airymaxrt）"
            fi
            ;;
        *)
            bad "B($_fn) AIRY_GATE_SDK_AIRYMAXRT 显式指定但文件缺失: $SDK_AIRYMAXRT"
            ;;
    esac
done

section "B2" "P0 安装根 SSoT（安装/重装/更新定位同一实例；跨根候选零残留）"

_ghost=""
for _f in "$INSTALL" "$LATEST_RT" \
          "$ROOT/commons/platform/src/platform_paths.c"; do
    [ -f "$_f" ] || continue
    if sed -e 's/^[[:space:]]*#.*$//' -e 's/^[[:space:]]*\*.*$//' "$_f" \
       | grep -Eq '\.airymaxrt/config/install\.env|\.local/share/airymaxrt'; then
        _ghost="$_ghost $_f"
    fi
done
if [ -z "$_ghost" ]; then
    ok "B2-0 跨根候选零残留（install.sh / latest + commons/platform_paths.c）"
else
    bad "B2-0 跨根候选残留（幽灵根劫持命门）:${_ghost}"
fi

_anchor_missing=""
for _f in "$INSTALL" "$LATEST_RT"; do
    grep -q '}/\.\./config/install\.env' "$_f" || _anchor_missing="$_anchor_missing $_f"
done
case "$(sdk_ready; echo $?)" in
    0) grep -q '}/\.\./config/install\.env' "$SDK_AIRYMAXRT" \
           || _anchor_missing="$_anchor_missing $SDK_AIRYMAXRT" ;;
esac
if [ -z "$_anchor_missing" ]; then
    ok "B2-6 自锚定读取存在（install.sh 内嵌启动器 / latest / sdk）"
else
    bad "B2-6 自锚定读取缺失（解析链退化为兜底根，将误建幽灵根）:${_anchor_missing}"
fi

cat > "$TMP/homelib.sh" <<'EOS'
log_warn() { printf '[WARN] %s\n' "$1" >&2; }
EOS
extract_fn "$INSTALL" resolve_link_chain    "$TMP/fn_rlc"
extract_fn "$INSTALL" is_install_home       "$TMP/fn_iih"
extract_fn "$INSTALL" discover_install_home "$TMP/fn_dih"
cat "$TMP/homelib.sh" "$TMP/fn_rlc" "$TMP/fn_iih" "$TMP/fn_dih" \
    > "$TMP/homelib_full.sh"

_CORE_PATH="/usr/bin:/bin"
_mkroot() { # <dir> [version] —— 造一个含 install.env + bin/airy_cli 的安装根
    mkdir -p "$1/bin" "$1/config"
    printf 'AIRY_HOME=%s\nAIRY_VERSION=%s\n' "$1" "${2:-v0.1.16}" \
        > "$1/config/install.env"
    : > "$1/bin/airy_cli"; chmod +x "$1/bin/airy_cli"
    : > "$1/bin/airymaxrt"; chmod +x "$1/bin/airymaxrt"
}
_DIH_SCRIPT='. "$DIHLIB"; discover_install_home'

mkdir -p "$TMP/b2/home"
_mkroot "$TMP/b2/root1"
_out="$(env -i HOME="$TMP/b2/home" PATH="$_CORE_PATH" \
        DIHLIB="$TMP/homelib_full.sh" AIRY_HOME="$TMP/b2/root1" \
        sh -c "$_DIH_SCRIPT" plain.sh 2>/dev/null)"
if [ "$_out" = "$TMP/b2/root1" ]; then
    ok "B2-1 AIRY_HOME 指向既有根 → 复用该根"
else
    bad "B2-1 AIRY_HOME 有效根未被复用（out=${_out:-<空>}）"
fi

_err="$TMP/b2/err2"
_out="$(env -i HOME="$TMP/b2/home" PATH="$_CORE_PATH" \
        DIHLIB="$TMP/homelib_full.sh" AIRY_HOME="$TMP/b2/nonexistent" \
        sh -c "$_DIH_SCRIPT" plain.sh 2>"$_err")"
if [ -z "$_out" ] && grep -q '已忽略环境变量' "$_err"; then
    ok "B2-2 AIRY_HOME 指向无效目录 → 忽略 + 告警（不落幽灵根）"
else
    bad "B2-2 残留环境变量未拦截（out=${_out:-<空>}）"
fi

_mkroot "$TMP/b2/root3"
mkdir -p "$TMP/b2/link3" "$TMP/b2/other3"
ln -s "$TMP/b2/root3/bin/airymaxrt" "$TMP/b2/link3/airymaxrt"
_out="$(env -i HOME="$TMP/b2/home" PATH="$TMP/b2/link3:$_CORE_PATH" \
        DIHLIB="$TMP/homelib_full.sh" \
        sh -c "$_DIH_SCRIPT" "$TMP/b2/other3/tool.sh" 2>/dev/null)"
if [ "$_out" = "$TMP/b2/root3" ]; then
    ok "B2-3 PATH 符号链命中既有根 → 复用（跨前缀重装不再另建一套）"
else
    bad "B2-3 PATH 符号链未复用（out=${_out:-<空>}）"
fi

_mkroot "$TMP/b2/home/.airymaxrt"
_out="$(env -i HOME="$TMP/b2/home" PATH="$_CORE_PATH" \
        DIHLIB="$TMP/homelib_full.sh" \
        sh -c "$_DIH_SCRIPT" tool 2>/dev/null)"
if [ -z "$_out" ]; then
    ok "B2-4 \$HOME/.airymaxrt 存在也不被跨根候选命中（幽灵根劫持已断）"
else
    bad "B2-4 仍被跨根候选命中: $_out"
fi

_mkroot "$TMP/b2/root5"
_out="$(env -i HOME="$TMP/b2/home" PATH="$_CORE_PATH" \
        DIHLIB="$TMP/homelib_full.sh" \
        sh -c "$_DIH_SCRIPT" "$TMP/b2/root5/bin/agentrt-bootstrap.sh" 2>/dev/null)"
if [ "$_out" = "$TMP/b2/root5" ]; then
    ok "B2-5 安装副本 bin/ 内启动 → 自锚定到副本根"
else
    bad "B2-5 自锚定失败（out=${_out:-<空>}）"
fi

section "C" "9.3/I-02② detect_arch 20 例平台矩阵仿真（stub 注入 uname/bits/loader）"

extract_fn "$INSTALL" _uspace_bits       "$TMP/fn_bits"
extract_fn "$INSTALL" _loader_exists     "$TMP/fn_loader"
extract_fn "$INSTALL" _arch_warn_unknown "$TMP/fn_warn"
extract_fn "$INSTALL" detect_arch        "$TMP/fn_detect"
cat "$TMP/fn_bits" "$TMP/fn_loader" "$TMP/fn_warn" "$TMP/fn_detect" > "$TMP/archlib.sh"
cat >> "$TMP/archlib.sh" <<'GA_STUB'
_uspace_bits()   { printf '%s' "${GA_BITS:-}"; }
_loader_exists() { [ "${GA_LOADER:-}" = "$1" ]; }
GA_STUB
mkdir -p "$TMP/bin"
printf '#!/bin/sh\nprintf "%%s\\n" "${GA_UNAME:-}"\n' > "$TMP/bin/uname"
chmod +x "$TMP/bin/uname"

arch_one() { # <machine> <bits> <loader> <expected>
    got=$(GA_UNAME="$1" GA_BITS="$2" GA_LOADER="$3" \
          PATH="$TMP/bin:$PATH" \
          bash -c ". '$TMP/archlib.sh'; detect_arch" 2>/dev/null)
    [ "$got" = "$4" ]
}

arch_fail=0
arch_n=0
while IFS='|' read -r _m _b _l _e; do
    [ -z "$_m" ] && continue
    [ "$_b" = "-" ] && _b=""
    [ "$_l" = "-" ] && _l=""
    arch_n=$((arch_n+1))
    if arch_one "$_m" "$_b" "$_l" "$_e"; then :; else
        arch_fail=$((arch_fail+1))
        printf '    [arch] %s bits=%s loader=%s: got <%s> want <%s>\n' \
            "$_m" "$_b" "$_l" "${got:-}" "$_e"
    fi
done <<'GA_CASES'
x86_64|64|-|x86_64
x86_64|32|-|i686
x86_64|-|ld-linux-x86-64.so.2|x86_64
x86_64|-|ld-linux.so.2|i686
x86_64|-|-|i686
amd64|64|-|x86_64
i386|64|-|i686
i486|64|-|i686
i586|64|-|i686
x86|64|-|i686
aarch64|64|-|aarch64
aarch64|32|-|armv7l
aarch64|-|ld-linux-aarch64.so.1|aarch64
aarch64|-|ld-linux-armhf.so.3|armv7l
aarch64|-|-|aarch64
arm64|64|-|aarch64
armv7l|64|-|armv7l
armv6l|64|-|armv7l
armhf|64|-|armv7l
s390x|64|-|unknown
GA_CASES

if [ "$arch_fail" -eq 0 ]; then
    ok "C detect_arch 20 例平台矩阵仿真全通过（含 aarch64 32 位 → armv7l 陷阱、unknown 保守告警）"
else
    bad "C detect_arch 平台矩阵 $arch_fail/$arch_n 例失配（见上方明细）"
fi

section "D" "9.4/C-01 airy_cli 聊天路径单一实现（稳定问题：stream 遗留死码清除）"

CHAT="$CLI/src/chat/cli_chat.c"
if grep -q 'cli_gw_call(' "$CHAT" && grep -q '"llm.complete"' "$CHAT"; then
    ok "D1 cli_chat.c 直连 gw（cli_gw_call + llm.complete）"
else
    bad "D1 cli_chat.c 未走 cli_gw_call/llm.complete 单一实现"
fi

if [ ! -f "$CLI/src/chat/cli_chat_stream.c" ]; then
    ok "D2 cli_chat_stream.c 已删除"
else
    bad "D2 cli_chat_stream.c 复活（应删除，防双实现漂移）"
fi

if grep -rIqE 'cli_chat_stream_round|cli_chat_stream_cb|cli_chat_reasoning_cb|cli_stream_norm|cli_gw_stream|cli_gw_line_cb' "$CLI"; then
    bad "D3 死符号仍被引用（cli_chat_stream_round/stream_cb/reasoning_cb/stream_norm/gw_stream/gw_line_cb）"
else
    ok "D3 六个 stream 遗留死符号全仓零引用"
fi

if grep -q 'llm_svc_adapter_create' "$CHAT"; then
    bad "D4 cli_chat.c 仍引用 llm_svc_adapter_create（旧 adapter 路径残留）"
else
    ok "D4 cli_chat.c 零 llm_svc_adapter_create（旧 adapter 解耦完成）"
fi

section "E" "9.6/S-01 TUI 会话历史环形裁剪（长对话问题：无界增长拖垮 TUI）"

if grep -Fq '#define TUI_HIST_MAX 1024' "$CLI/src/tui/cli_tui_internal.h"; then
    ok "E1 TUI_HIST_MAX=1024 上限常量在位"
else
    bad "E1 TUI_HIST_MAX 常量缺失或数值漂移"
fi

if grep -Fq 'if (t->hist.count >= TUI_HIST_MAX) {' "$CLI/src/tui/tui_history.c"; then
    ok "E2 历史超限裁剪分支在位（丢最老行环形窗口）"
else
    bad "E2 历史超限裁剪分支缺失（无界增长回归）"
fi

section "F" "9.7/S-02 长任务可取消 + 超时可诊断（稳定问题：Ctrl+C 假死/超时黑箱）"

GW="$CLI/src/cmd/cli_gw.c"
if grep -Fq '#define CLI_GW_EXCH_CANCELED' "$GW" \
   && grep -Fq '#define CLI_GW_EXCH_TIMEOUT' "$GW"; then
    ok "F1 交换状态码 CANCELED/TIMEOUT 常量在位"
else
    bad "F1 CLI_GW_EXCH_CANCELED/TIMEOUT 常量缺失"
fi

_gw_cancel=$(grep -c 'if (g_cli_cancel)' "$GW" || true)
if [ "${_gw_cancel:-0}" -ge 2 ]; then
    ok "F2 交换双腿取消检查在位（$_gw_cancel 处 if (g_cli_cancel)）"
else
    bad "F2 取消检查不足双腿（$_gw_cancel 处，应 ≥2）"
fi

if grep -q 'AIRY_ERR_CANCELED' "$GW"; then
    ok "F3 取消映射 AIRY_ERR_CANCELED 在位（诊断可区分取消/超时）"
else
    bad "F3 AIRY_ERR_CANCELED 映射缺失"
fi

section "G" "9.11/S-04 reasoning 无界累计封顶 + 日志轮转（长任务问题：内存/磁盘无界）"

if grep -Fq 'CLI_CHAT_REASONING_MAX_BYTES' "$CLI/include/cli_internal.h" \
   && grep -Fq 'AIRY_REASONING_LOG_MAX_BYTES' "$CLI/include/cli_internal.h"; then
    ok "G1 截断/轮转双常量在位（cli_internal.h）"
else
    bad "G1 CLI_CHAT_REASONING_MAX_BYTES / AIRY_REASONING_LOG_MAX_BYTES 常量缺失"
fi

if grep -q 'g_chat_reasoning_truncated' "$CLI/src/chat/cli_chat_usage.c"; then
    ok "G2 单回合截断标记在位（cli_chat_usage.c）"
else
    bad "G2 g_chat_reasoning_truncated 截断逻辑缺失"
fi

if grep -q 'cli_reasoning_log_rotate' "$CLI/src/chat/cli_chat_history.c" \
   && grep -q 'remove(' "$CLI/src/chat/cli_chat_history.c"; then
    ok "G3 日志轮转 cli_reasoning_log_rotate + remove 旧份在位"
else
    bad "G3 思考链日志轮转缺失（磁盘无界回归）"
fi

section "H" "9.9/U-02~4 通道白名单三副本同集 + 保留通道文案 + 版本占位格式合法"

case "$(sdk_ready; echo $?)" in
    0)
        _h_ok=1
        for _f in "$INSTALL" "$LATEST_RT" "$SDK_AIRYMAXRT"; do
            grep -Fq 'in stable|rc|beta)' "$_f" || _h_ok=0
        done
        if [ "$_h_ok" -eq 1 ]; then
            ok "H1 三份脚本通道白名单 case 同集（stable|rc|beta，beta 为保留通道）"
        else
            bad "H1 三份脚本通道白名单不同集"
        fi
        ;;
    1)
        skip "H1 sdk 未检出，仅断言仓内两副本"
        _h_ok=1
        for _f in "$INSTALL" "$LATEST_RT"; do
            grep -Fq 'in stable|rc|beta)' "$_f" || _h_ok=0
        done
        if [ "$_h_ok" -eq 1 ]; then
            ok "H1(仓内) install.sh/latest 白名单 case 同集"
        else
            bad "H1(仓内) install.sh/latest 白名单不同集"
        fi
        ;;
    *)
        bad "H1 AIRY_GATE_SDK_AIRYMAXRT 显式指定但文件缺失: $SDK_AIRYMAXRT"
        ;;
esac

_h2=1
grep -Fq 'beta 为保留通道' "$INSTALL" || _h2=0
grep -Fq 'beta 为保留通道' "$INSTALL_PS1" || _h2=0
if [ -f "$SDK_AIRYMAXRT" ]; then
    grep -Fq 'beta 为保留通道' "$SDK_AIRYMAXRT" || _h2=0
fi
if [ "$_h2" -eq 1 ]; then
    ok "H2 beta 缺席时保留通道明确文案在位（U-03：可操作提示；bash/ps1/更新器三副本）"
else
    bad "H2 保留通道文案缺失（beta 403 时用户无指引）"
fi

if grep -Eq '^AIRY_VERSION="\$\{AIRY_VERSION:-v[0-9]+\.[0-9]+\.[0-9]+\}"$' "$INSTALL"; then
    ok "H3 版本占位格式合法（U-04：curl 管道形态兜底可解析；占位超前指针属 bump 窗口常态不断言相等）"
else
    bad "H3 版本占位行缺失或格式漂移"
fi

BETA_DECL="$ROOT/latest/manifest.beta.json"
_h4=1
[ -s "$BETA_DECL" ] || _h4=0
[ "$_h4" -eq 1 ] && { grep -Fq '"channel": "beta"' "$BETA_DECL" || _h4=0; }
[ "$_h4" -eq 1 ] && { grep -Fq '"state": "reserved"' "$BETA_DECL" || _h4=0; }
[ "$_h4" -eq 1 ] && { grep -Fq '"latest": ""' "$BETA_DECL" || _h4=0; }
[ "$_h4" -eq 1 ] && { [ -s "$BETA_DECL.asc" ] || _h4=0; }
if [ "$_h4" -eq 1 ]; then
    ok "H4 保留通道声明 manifest 在位（beta state=reserved + 空指针 + 已签名）"
else
    bad "H4 latest/manifest.beta.json 缺失/字段漂移/未签名（通道选择将退回 404 猜测）"
fi

_h5=1
grep -Fq 'state=reserved' "$INSTALL"     || _h5=0
grep -Fq 'state=reserved' "$INSTALL_PS1" || _h5=0
if [ -f "$SDK_AIRYMAXRT" ]; then
    grep -Fq 'state=reserved' "$SDK_AIRYMAXRT" || _h5=0
fi
if [ "$_h5" -eq 1 ]; then
    ok "H5 消费端保留通道 fail-closed 门禁在位（install.sh / install.ps1 / 更新器）"
else
    bad "H5 消费端缺 state=reserved 门禁（保留通道将被静默源码构建）"
fi

section "I" "9.10/I-03 架构白名单精确串 + 全仓零 riscv 表述 + 源码构建指引"

if grep -Fq 'SUPPORTED_ARCHS="x86_64 aarch64 i686 armv7l"' "$INSTALL"; then
    ok "I1 SUPPORTED_ARCHS 白名单精确串在位"
else
    bad "I1 SUPPORTED_ARCHS 白名单漂移"
fi

_i2=0
for _mf in "$ROOT/latest/manifest.stable.json" "$ROOT/latest/manifest.rc.json" \
           "$ROOT/latest/manifest.beta.json"; do
    if [ -f "$_mf" ] && grep -riq 'riscv' "$_mf"; then
        _i2=1
    fi
done
if [ "$_i2" -eq 0 ]; then
    ok "I2 latest/ 三 manifest 零 riscv（不宣发未支持架构）"
else
    bad "I2 latest/ manifest 出现 riscv 表述"
fi

_i3=0
for _rd in "$ROOT/README.md" "$ROOT/README_zh.md"; do
    if [ -f "$_rd" ] && grep -iq 'riscv' "$_rd"; then
        _i3=1
    fi
done
if [ "$_i3" -eq 0 ]; then
    ok "I3 README 双语零 riscv 承诺"
else
    bad "I3 README 出现 riscv 支持表述"
fi

case "$(sdk_ready; echo $?)" in
    0)
        if grep -Fq 'AIRY_MODE=source' "$SDK_AIRYMAXRT"; then
            ok "I4 sdk 更新器 riscv → AIRY_MODE=source 源码构建指引在位"
        else
            bad "I4 sdk 更新器缺 riscv 源码构建指引"
        fi
        ;;
    1) skip "I4 sdk 未检出，riscv 源码指引跳过" ;;
    *) bad "I4 AIRY_GATE_SDK_AIRYMAXRT 显式指定但文件缺失: $SDK_AIRYMAXRT" ;;
esac

section "J" "9.1/U-01 更新器 detect_pending_release（更新问题：manifest 指针落后于发布）"

case "$(sdk_ready; echo $?)" in
    0)
        if grep -Fq 'detect_pending_release() {' "$SDK_AIRYMAXRT" \
           && grep -Fq 'detect_pending_release "$latest"' "$SDK_AIRYMAXRT" \
           && grep -q 'manifest 指针尚未更新' "$SDK_AIRYMAXRT"; then
            ok "J1 detect_pending_release 定义 + 接入 + 落后指针告警文案全链在位"
        else
            bad "J1 更新器落后指针检测链破裂（定义/接入/文案至少一项缺失）"
        fi
        ;;
    1) skip "J1 sdk 未检出，更新器判据跳过" ;;
    *) bad "J1 AIRY_GATE_SDK_AIRYMAXRT 显式指定但文件缺失: $SDK_AIRYMAXRT" ;;
esac

section "K" "（可选）发布侧 publish-release.sh 白名单契约"

PUBLISH="$ROOT/../../tools/scripts/ci/release/publish-release.sh"
if [ -f "$PUBLISH" ]; then
    if grep -Fq '"linux-x86-64"' "$PUBLISH" && grep -Fq '"linux-arm-64"' "$PUBLISH"; then
        ok "K1 发布侧 target 归一映射含 linux-x86-64/linux-arm-64（与安装侧白名单契约对齐）"
    else
        bad "K1 发布侧归一映射缺 linux 主干 target（安装/发布契约破裂）"
    fi
else
    skip "K1 hub tools 仓未检出，发布侧契约跳过（CI 不取料，属预期）"
fi

if [ -f "$PUBLISH" ]; then
    if grep -Fq 'emit_channel_declarations() {' "$PUBLISH" \
       && grep -Fq 'emit_channel_declarations "$LATEST_DIR" "$CHANNEL"' "$PUBLISH" \
       && grep -Fq '"state": "reserved"' "$PUBLISH" \
       && grep -Fq '"state": "active"' "$PUBLISH"; then
        ok "K2 发布侧保留通道声明链在位（emit_channel_declarations + state=active/reserved）"
    else
        bad "K2 发布侧保留通道声明链破裂（latest/ 通道 manifest 不全，客户端退回 404 猜测）"
    fi
    if grep -Fq 'SKIP_SIGN' "$PUBLISH" && grep -Fq '跳过保留通道声明 manifest' "$PUBLISH"; then
        ok "K3 无有效签名时不假签声明（SKIP_SIGN/SKIP_GPG 宁缺不假签）"
    else
        bad "K3 声明 manifest 缺签名保护（假签名将被客户端误读为签名篡改）"
    fi
else
    skip "K2/K3 hub tools 仓未检出，发布侧契约跳过（CI 不取料，属预期）"
fi

section "L" "R-5 provider 默认超时 < 网关 LLM 转发背压（连不上网络：精确诊断先于网关返回）"

PROVIDER_C="$ROOT/daemons/llm_d/src/providers/provider.c"
LLM_METHODS="$ROOT/daemons/llm_d/src/llm_daemon_methods.c"
GW_INTERNAL="$ROOT/gateway/src/biz/gateway_biz_internal.h"

_gate_define() { # <file> <macro>  → 打印宏字面量（无则空）
    sed -n "s/^#define $2[[:space:]]*\([0-9][0-9.]*\).*/\1/p" "$1" | head -1
}
_prov_to="$(_gate_define "$PROVIDER_C" PROVIDER_DEFAULT_TIMEOUT_SEC)"
_gw_ms="$(_gate_define "$GW_INTERNAL" GW_LLM_DEFAULT_TIMEOUT_MS)"
_retries="$(_gate_define "$LLM_METHODS" LLM_MAX_RETRIES)"
_fast_ms="$(_gate_define "$LLM_METHODS" LLM_RETRY_FAST_FAIL_MS)"

if [ -z "$_prov_to" ] || [ -z "$_gw_ms" ]; then
    bad "L1 无法解析 PROVIDER_DEFAULT_TIMEOUT_SEC / GW_LLM_DEFAULT_TIMEOUT_MS（宏被删改）"
elif awk -v t="$_prov_to" -v g="$_gw_ms" 'BEGIN { exit !(t * 1000 < g) }'; then
    ok "L1 单次 provider 超时 ${_prov_to}s < 网关 LLM 背压 ${_gw_ms}ms（单次失败诊断先于网关返回）"
else
    bad "L1 单次 provider 超时 ${_prov_to}s 未小于网关背压 ${_gw_ms}ms（诊断会被网关超时吞掉）"
fi

if grep -Fq ': 120.0' "$PROVIDER_C"; then
    bad "L2 provider 默认超时仍写死 120.0s（超过网关背压，超时倒挂回归）"
else
    ok "L2 provider 默认超时无 120.0s 硬编码残留（统一走 PROVIDER_DEFAULT_TIMEOUT_SEC）"
fi

if [ -n "$_prov_to" ] && [ -n "$_gw_ms" ] && [ -n "$_retries" ] && [ -n "$_fast_ms" ] \
   && grep -Fq 'LLM_RETRY_FAST_FAIL_MS' "$LLM_METHODS"; then
    if awk -v t="$_prov_to" -v r="$_retries" -v f="$_fast_ms" -v g="$_gw_ms" \
         'BEGIN { exit !(r * f + t * 1000 < g) }'; then
        ok "L3 重试最坏总耗时 ${_retries}×${_fast_ms}ms+${_prov_to}s < 网关背压 ${_gw_ms}ms"
    else
        bad "L3 重试最坏总耗时越过网关背压（${_retries}×${_fast_ms}ms+${_prov_to}s）"
    fi
else
    bad "L3 缺 LLM_MAX_RETRIES / LLM_RETRY_FAST_FAIL_MS 或其使用点（重试预算护栏缺失）"
fi

section "M" "R-6 工具授权面 ⊇ MCP 暴露面（双 Schema 解析防回归）"

PDP_RULE_C="$ROOT/cupolas/src/permission/permission_rule.c"
MCP_BUILTIN="$ROOT/daemons/tool_d/src/service_builtin.c"
GW_BACKEND="$ROOT/gateway/src/biz/gateway_biz_backend.c"

if grep -Fq 'cupolas_permission_rule_resource' "$PDP_RULE_C" \
   && grep -Fq 'cupolas_permission_rule_allow' "$PDP_RULE_C" \
   && grep -Fq 'yaml_get(entry, "tool")' "$PDP_RULE_C" \
   && grep -Fq 'yaml_get(entry, "effect")' "$PDP_RULE_C"; then
    ok "M1 PDP 解析器兼容 ACL Schema（resource←tool / allow←effect），双 Schema 冲突已修"
else
    bad "M1 permission_rule.c 缺 tool→resource / effect→allow 别名（双 Schema 冲突回归，/mcp 工具将被 fail-closed 全拒）"
fi

if grep -Fq 'gw_acl_check_tool' "$GW_BACKEND"; then
    ok "M2 网关工具执行路径经 PEP 判定（gw_acl_check_tool）"
else
    bad "M2 gateway 工具执行路径未接入 PEP（gw_acl_check_tool 缺失，权限判定旁路）"
fi

if [ ! -f "$MCP_BUILTIN" ]; then
    bad "M3 缺失 MCP 工具 SSoT: $MCP_BUILTIN"
else
    _mcp_tools="$TMP/mcp_tools.txt"
    sed -n 's/^[[:space:]]*\.id = "\([^"]*\)".*/\1/p' "$MCP_BUILTIN" > "$_mcp_tools"
    _mcp_n="$(wc -l < "$_mcp_tools" | tr -d ' ')"

    PERM_TMPL=""
    _tools_root="${AIRY_GATE_TOOLS_ROOT:-}"
    for _cand in "${_tools_root:+$_tools_root/scripts/ops/templates/permission_rules.yaml}" \
                 "$ROOT/_tools/scripts/ops/templates/permission_rules.yaml" \
                 "$ROOT/../../tools/scripts/ops/templates/permission_rules.yaml"; do
        if [ -n "$_cand" ] && [ -f "$_cand" ]; then PERM_TMPL="$_cand"; break; fi
    done

    if [ "$_mcp_n" -lt 15 ]; then
        bad "M3 /mcp 暴露工具数 $_mcp_n < 15（内置工具集被删减）"
    elif [ -z "$PERM_TMPL" ]; then
        if [ -n "$_tools_root" ]; then
            bad "M3 AIRY_GATE_TOOLS_ROOT 显式指定但模板缺失（$AIRY_GATE_TOOLS_ROOT）"
        else
            skip "M3 未取到 tools 仓 permission_rules.yaml（本地无 tools 仓）；CI 侧由取料 step fail-closed 兜底"
        fi
    else
        _ext_tools="$TMP/ext_tools.txt"
        awk '
            $1 == "-" && $2 == "agent:" { a = $3; gsub(/"/, "", a); next }
            $1 == "tool:" { t = $2; gsub(/"/, "", t); if (a == "external") print t }
        ' "$PERM_TMPL" > "$_ext_tools"
        _miss=0
        while IFS= read -r _t; do
            [ -n "$_t" ] || continue
            grep -qxF "$_t" "$_ext_tools" \
                || { bad "M3 external 缺工具授权: $_t（/mcp tools/call 将 -32603）"; _miss=1; }
        done < "$_mcp_tools"
        if [ "$_miss" -eq 0 ]; then
            ok "M3 external 授权集 ⊇ /mcp tools/list 暴露集（$_mcp_n 个工具全覆盖）"
        fi

        _capmiss=0
        for _cap in cap:agent.run cap:agent.control; do
            grep -qxF "$_cap" "$_ext_tools" \
                || { bad "M4 external 缺高敏能力授权: $_cap"; _capmiss=1; }
        done
        [ "$_capmiss" -eq 0 ] \
            && ok "M4 external 保有 cap:agent.run / cap:agent.control（T-11b 网关高敏入口授权）"
    fi
fi

section "N" "B3 南向 A-IPC 客户端面归一（gateway/src AF_UNIX 白名单）"

_aipc_face="$ROOT/gateway/src/biz/gateway_aipc_client.c"
if [ ! -f "$_aipc_face" ]; then
    bad "N1 缺失南向统一客户端面: gateway/src/biz/gateway_aipc_client.c（客户端面被删除）"
else
    _aipc_offenders="$(grep -rl 'socket(AF_UNIX' "$ROOT/gateway/src" --include='*.c' \
        | grep -v 'gateway_aipc_client.c' || true)"
    if [ -z "$_aipc_offenders" ]; then
        ok "N2 AF_UNIX socket 创建仅存在于统一客户端面 gateway_aipc_client.c（四处手搓点未再生）"
    else
        bad "N2 gateway/src 出现统一客户端面之外的手搓 AF_UNIX socket: $(echo "$_aipc_offenders" | tr '\n' ' ')"
    fi
    for _sym in gw_aipc_call gw_aipc_stream gw_aipc_subscribe; do
        grep -Fq "$_sym" "$_aipc_face" \
            || bad "N3 统一客户端面缺入口 $_sym（客户端面 API 面被削）"
    done
    grep -Fq 'gw_aipc_call' "$ROOT/gateway/src/biz/gateway_biz_forward.c" \
        && ok "N3 gw_svc_call 转调统一客户端面（gw_aipc_call）" \
        || bad "N3 gw_svc_call 未转调 gw_aipc_call（第二套传输实现回归）"
fi

section "P" "B4 构建收口（corekern include 面 + CLI 零内核机制）"

_whitelist="$ROOT/link-whitelist.txt"

_p1_line="$(grep -E '^airy_cli:' "$_whitelist" || true)"
if [ -z "$_p1_line" ]; then
    bad "P1 link-whitelist.txt 缺 airy_cli 登记（白名单被削）"
elif echo "$_p1_line" | grep -q 'airy_atoms'; then
    bad "P1 airy_cli 允许集含 airy_atoms（CLI 进程内微核心违规再生）"
else
    ok "P1 airy_cli 允许集不含 airy_atoms（进程内微核心收回固化）"
fi

_p2_hits="$(grep -n 'atoms/corekern/include' "$ROOT/CMakeLists.txt" \
    | grep -v 'AIRY_COREKERN_INCLUDE_DIR\|corekern 头不再全局注入' || true)"
if [ -z "$_p2_hits" ]; then
    ok "P2 根 CMakeLists 全局面无 atoms/corekern/include 注入（B4 收口保持）"
else
    bad "P2 根 CMakeLists 全局面重现 corekern include 注入: $_p2_hits"
fi

_p3_hdr="$(grep -rnE '^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"]' \
    "$ROOT/tools/airy_cli/src" "$ROOT/tools/airy_cli/include" \
    --include='*.c' --include='*.h' 2>/dev/null \
    | grep -E '[<"](airy_rt|task|mem|ipc|airy_time|export|error)\.h[>"]|corekern/' || true)"
_p3_cmake="$(grep -n 'corekern/include' "$ROOT/tools/airy_cli/CMakeLists.txt" \
    | grep -vE '^[0-9]+:[[:space:]]*#' || true)"
if [ -z "$_p3_hdr" ] && [ -z "$_p3_cmake" ]; then
    ok "P3 CLI 源树与构建文件零 corekern 引用（include 面收口保持）"
else
    [ -n "$_p3_hdr" ] && bad "P3 CLI 源树出现 corekern 头引用: $(echo "$_p3_hdr" | tr '\n' ' ')"
    [ -n "$_p3_cmake" ] && bad "P3 CLI 构建文件出现 corekern 路径: $_p3_cmake"
fi

_cli_bin=""
if [ -n "${AIRY_GATE_BUILD_DIR:-}" ]; then
    for _cand in \
        "$AIRY_GATE_BUILD_DIR/tools/airy_cli/airy_cli" \
        "$AIRY_GATE_BUILD_DIR/bin/airy_cli"; do
        if [ -f "$_cand" ] && [ -x "$_cand" ]; then _cli_bin="$_cand"; break; fi
    done
fi
if [ -z "$_cli_bin" ]; then
    if [ -n "${AIRY_GATE_BUILD_DIR:-}" ]; then
        bad "P4 构建树指定但 airy_cli 产物缺失: $AIRY_GATE_BUILD_DIR（构建契约破坏）"
    else
        skip "P4 未收到 AIRY_GATE_BUILD_DIR，跳过产物断言（ctest 注入兜底）"
    fi
else
    _cli_syms="$(nm "$_cli_bin" 2>/dev/null | awk '{print $3}' \
        | grep -E '^(airy_init|airy_shutdown)$|^airy_oom_|^airy_persist_' || true)"
    if [ -z "$_cli_syms" ]; then
        ok "P4 airy_cli 产物零内核机制符号（$(basename "$_cli_bin")）"
    else
        bad "P4 airy_cli 产物出现内核机制符号: $(echo "$_cli_syms" | tr '\n' ' ')"
    fi
fi

section "Q" "B6 gateway 零编排（退役 SSE 编排死代码物理移除）"

_q1_dead=""
for _f in \
    "$ROOT/gateway/src/gateway/gateway_sse_tool.c" \
    "$ROOT/gateway/src/gateway/gateway_sse_frame.c" \
    "$ROOT/gateway/src/gateway/gateway_sse_stream.c" \
    "$ROOT/gateway/src/gateway/gateway_sse_memory.c" \
    "$ROOT/gateway/tests/test_sse_stream.c" \
    "$ROOT/gateway/tests/test_sse_utf8.c"; do
    [ -e "$_f" ] && _q1_dead="$_q1_dead $(basename "$_f")"
done
if [ -z "$_q1_dead" ]; then
    ok "Q1 退役 SSE 编排死模块与单测零残留"
else
    bad "Q1 退役 SSE 编排死模块再生:$_q1_dead"
fi

_q2_hits="$(grep -rnE 'GW_SSE_MAX_TOOL_LOOPS|GW_SSE_TOOL_LIMIT_MSG|GW_SSE_TEXT_CHUNK|GW_SSE_SUMMARY_MAX|GW_SSE_TOOL_FEEDBACK_MAX|gw_sse_phase_t|EXEC_TOOLS' \
    "$ROOT/gateway/src" --include='*.c' --include='*.h' 2>/dev/null \
    | grep -vE ':[0-9]+:[[:space:]]*(\*|/\*|//)' || true)"
if [ -z "$_q2_hits" ]; then
    ok "Q2 gateway/src 零编排常量与相位（EXEC_TOOLS 等未再生）"
else
    bad "Q2 gateway/src 重现编排常量/相位: $(echo "$_q2_hits" | tr '\n' ' ')"
fi

_q3_hits="$(grep -rnE 'GW_SSE_CHAT_PATH|"/api/v1/chat/stream"' \
    "$ROOT/gateway/src" --include='*.c' --include='*.h' 2>/dev/null || true)"
if [ -z "$_q3_hits" ]; then
    ok "Q3 退役路由 /api/v1/chat/stream 未登记（宏与字面量均零残留）"
else
    bad "Q3 退役路由 /api/v1/chat/stream 再生: $(echo "$_q3_hits" | tr '\n' ' ')"
fi

_q4_missing=""
for _sym in gw_sse_send_json_error handle_run_stream_sse handle_hall_watch_sse; do
    grep -rq "$_sym" "$ROOT/gateway/src" --include='*.c' --include='*.h' 2>/dev/null \
        || _q4_missing="$_q4_missing $_sym"
done
if [ -z "$_q4_missing" ]; then
    ok "Q4 live 端点与共享助手仍在（run_stream/hall_watch/gw_sse_send_json_error）"
else
    bad "Q4 live 端点或共享助手缺失:$_q4_missing"
fi

section "R" "T4b 制品 platform-* 标记判定语义（跨版本更新误拒根因）+ 下载体积渲染"

extract_fn "$INSTALL" plat_markers    "$TMP/r_plat"
extract_fn "$INSTALL" arch_markers_ok "$TMP/r_amok"
extract_fn "$INSTALL" human_size      "$TMP/r_hsize"
cat "$TMP/r_plat" "$TMP/r_amok" "$TMP/r_hsize" > "$TMP/r_lib.sh"

mkdir -p "$TMP/r_cases"
r_mk() { # <name> <marker-path>...
    _n="$1"; shift
    _d="$TMP/r_build-$_n"
    rm -rf "$_d"; mkdir -p "$_d/agentrt-9.9.9/bin"
    : > "$_d/agentrt-9.9.9/bin/airymaxrt"
    for _p in "$@"; do
        mkdir -p "$_d/$(dirname "$_p")"; : > "$_d/$_p"
    done
    tar -czf "$TMP/r_cases/$_n.tar.gz" -C "$_d" . 2>/dev/null
    rm -rf "$_d"
}
r_mk root_x86      platform-x86-64
r_mk subdir_x86    agentrt-9.9.9/platform-x86-64
r_mk libname_arm   lib/libplatform-arm-64.so
r_mk wrong_arm     platform-arm-64
r_mk mixed         platform-arm-64 platform-x86-64
r_mk gen2_x64      platform-x64
r_mk riscv         platform-riscv-64
r_mk none

r_fail=0
r_n=0
r_chk() { # <case> <expect_rc> <arch> <expect_out>
    r_n=$((r_n+1))
    _rc=0
    _out="$(bash -c ". '$TMP/r_lib.sh'; arch_markers_ok '$TMP/r_cases/$1.tar.gz' '$3'")" || _rc=$?
    if [ "$_rc" = "$2" ] && [ "$_out" = "$4" ]; then :; else
        r_fail=$((r_fail+1))
        printf '    [marker] %s arch=%s: rc=%s want=%s out=<%s> want_out=<%s>\n' \
            "$1" "$3" "$_rc" "$2" "$_out" "$4"
    fi
}
r_chk root_x86    0 x86_64 ""
r_chk subdir_x86  0 x86_64 ""
r_chk mixed       0 x86_64 ""
r_chk gen2_x64    0 x86_64 ""
r_chk wrong_arm   0 aarch64 ""
r_chk libname_arm 0 x86_64 ""
r_chk none        0 x86_64 ""
r_chk wrong_arm   1 x86_64 "platform-arm-64"
r_chk riscv       1 x86_64 "platform-riscv-64"
r_chk root_x86    1 aarch64 "platform-x86-64"
r_chk mixed       1 riscv64 "platform-arm-64 platform-x86-64"
r_chk root_x86    1 mips "platform-x86-64"

r_hs_fail=0
r_hs() { # <bytes> <expect>
    _g="$(bash -c ". '$TMP/r_lib.sh'; human_size '$1'")"
    if [ "$_g" = "$2" ]; then :; else
        r_hs_fail=$((r_hs_fail+1))
        printf '    [size] human_size(%s)=<%s> want=<%s>\n' "$1" "$_g" "$2"
    fi
}
r_hs ""           未知
r_hs abc          未知
r_hs 0            0B
r_hs 512          512B
r_hs 1024         1KiB
r_hs 4096         4KiB
r_hs 1048576      1.0MiB
r_hs 37709685     35.9MiB

if [ "$r_fail" -eq 0 ] && [ "$r_hs_fail" -eq 0 ]; then
    ok "R T4b 标记判定 $r_n 例 + 体积渲染 8 例全通过（库名子串误判/子目录标记/任一命中/异架构拒绝/fail-closed）"
else
    bad "R T4b 标记判定失配 $r_fail/$r_n 例、体积渲染失配 $r_hs_fail/8 例（见上方明细）"
fi

section "S" "R1-c 执行体角色词汇表 SSoT（字面量零外泄 + 权威点唯一）"

# S1 豁免清单：SSoT 本体与结构性非角色用法（YAML 配置节键 / A2A 协议任务类型 / 报告 JSON 键）。
for _a in \
    commons/utils/cognition/agent_vocab.h \
    commons/utils/cognition/agent_vocab.c \
    daemons/common/include/agent_vocab.h \
    atoms/coreloopthree/include/hall_store.h \
    atoms/coreloopthree/src/config/yaml_loader_parse.c \
    daemons/gateway_d/src/main.c \
    tools/airy_cli/src/cmd/cli_review.c; do
    printf '%s:\n' "$ROOT/$_a"
done > "$TMP/s_allow"

_s_hits="$(grep -rnE '"(product_manager|architect|backend|frontend|devops|security|tester|coding|data_engineer|reviewer|analyst)"' \
    "$ROOT/atoms" "$ROOT/daemons" "$ROOT/gateway" "$ROOT/tools" \
    --include='*.c' --include='*.h' 2>/dev/null \
    | grep -vE ':[0-9]+:[[:space:]]*(\*|/\*|//)' \
    | grep -v '/tests/' \
    | grep -vFf "$TMP/s_allow" || true)"
if [ -z "$_s_hits" ]; then
    ok "S1 角色字面量零外泄（仅 SSoT 与结构性非角色用法保留）"
else
    bad "S1 角色字面量外泄: $(echo "$_s_hits" | tr '\n' ' ')"
fi

_s2=""
[ -f "$ROOT/commons/utils/cognition/agent_vocab.c" ] || _s2="$_s2 权威实现缺失"
[ -e "$ROOT/daemons/common/src/svc/agent_vocab.c" ] && _s2="$_s2 daemons 侧重复实现"
grep -q 'agent_vocab_canonical' "$ROOT/commons/utils/cognition/agent_vocab.c" 2>/dev/null \
    || _s2="$_s2 归一化符号缺失"
grep -q 'agent_vocab_canonical' "$ROOT/atoms/coreloopthree/src/work_hall/work_hall_agent.c" 2>/dev/null \
    || _s2="$_s2 归一化边界缺失"
grep -q 'agent_vocab.h' "$ROOT/daemons/common/include/agent_vocab.h" 2>/dev/null \
    || _s2="$_s2 兼容头重导出缺失"
grep -q 'agent_vocab_resolve' "$ROOT/commons/utils/cognition/agent_vocab.c" 2>/dev/null \
    || _s2="$_s2 严格解析符号缺失"
grep -q 'agent_vocab_resolve' "$ROOT/atoms/coreloopthree/src/work_hall/work_hall_auth.c" 2>/dev/null \
    || _s2="$_s2 权限判定未用严格解析（fail-open 风险）"
if [ -z "$_s2" ]; then
    ok "S2 SSoT 权威点唯一（commons 实现 + daemons 重导出 + 归一化边界单点 + 权限判定严格解析）"
else
    bad "S2 SSoT 结构异常:$_s2"
fi

section "T" "§8 交付防复发判据（DR-1 能力接线 / DR-4 失败可判读 / DR-5 码值契约 / DR-6 运行态门禁 / 判据6 命令面唯一）"

REG="$ROOT/gateway/src/biz/gateway_cap_registry.c"
ERRS="$ROOT/commons/utils/error/error_codes.h"
ERRN="$ROOT/commons/utils/error/handler.c"

_t1_n=0
_t1_bad=""
for _cap in $(grep -rhoE 'cli_gw_call\("[a-z_]+\.[a-z_0-9]+"' "$CLI/src" 2>/dev/null \
              | sed 's/.*("//' | tr -d '"' | sort -u); do
    _t1_n=$((_t1_n+1))
    grep -q "{\"$_cap\"," "$REG" 2>/dev/null || _t1_bad="$_t1_bad $_cap"
done
if [ "$_t1_n" -gt 0 ] && [ -z "$_t1_bad" ]; then
    ok "T1 DR-1 用户面能力串全部登记于网关能力表（$_t1_n 项）"
else
    bad "T1 DR-1 用户面能力串未登记:${_t1_bad:- 无用户面能力串}"
fi

# EXC-T7(T2) 例外登记：CLI ⇄ TUI 的 JSON Lines 事件流位于「TUI 是 CLI 的渲染层」同层内部边界，
# 不构成跨层调用，A-IPC 铁律不适用于该边界；T6/T7 判据集据此取证，禁止按“A-IPC 全链统一”误判为违规。
_t2_n=0
_t2_bad=""
for _cap in llm.complete think.process think.lang_process think.lang_postprocess \
            think.lang_stats mem.get_stats mem.write mem.search \
            agent.spawn agent.invoke agent.health_check \
            sched.plan sched.absorb sched.dag_submit sched.dag_status \
            sched.dag_list sched.dag_cancel tool.execute; do
    _t2_n=$((_t2_n+1))
    _line="$(grep -m1 "{\"$_cap\"," "$REG" 2>/dev/null)"
    if [ -z "$_line" ]; then _t2_bad="$_t2_bad $_cap(无网关登记)"; continue; fi
    _ns="$(printf '%s' "$_line" | sed -E 's/.*\{"[^"]*", *"([^"]*)", *"([^"]*)".*/\1/')"
    _m="$(printf '%s' "$_line" | sed -E 's/.*\{"[^"]*", *"([^"]*)", *"([^"]*)".*/\2/')"
    grep -rq "\"$_m\"" "$ROOT/daemons/${_ns}_d/src" 2>/dev/null \
        || _t2_bad="$_t2_bad $_cap(服务面未注册 $_m)"
    if ! grep -rq "cli_gw_call(\"$_cap\"" "$CLI/src" 2>/dev/null; then
        if [ "$_m" != "get_stats" ] || \
           ! grep -rq '%s\.get_stats' "$CLI/src" 2>/dev/null; then
            _t2_bad="$_t2_bad $_cap(无用户面调用方)"
        fi
    fi
done
if [ -z "$_t2_bad" ]; then
    ok "T2 DR-1 关键能力三件套齐备（$_t2_n 项：网关登记 + 服务面注册 + 用户面调用）"
else
    bad "T2 DR-1 能力断链:$_t2_bad"
fi

_t3_bad=""
[ "$(grep -rl 'gw_syscall_error_response' "$ROOT/gateway/src" 2>/dev/null | wc -l)" -ge 2 ] \
    || _t3_bad="$_t3_bad 网关统一错误响应面缺失"
grep -rq 'cli_err_desc' "$CLI/src" 2>/dev/null || _t3_bad="$_t3_bad CLI 错误描述面缺失"
grep -rq 'g_cli_gw_err' "$CLI/src" 2>/dev/null || _t3_bad="$_t3_bad CLI 失败原因透传缺失"
grep -q 'airy_err_code_name' "$ERRN" 2>/dev/null || _t3_bad="$_t3_bad 错误码名称面缺失"
_sl_src="$CLI/src/cmd/cli_gw.c"
if [ -f "$_sl_src" ]; then
    _sl_bad="$(awk '
        /^[a-zA-Z_].*cli_gw_call\(/ { _f=1; _n=0; next }
        _f && /^}/ { _f=0; next }
        _f {
            _n++; _b[_n % 16]=$0;
            if ($0 ~ /return -1;/) {
                _hit=0;
                for (_i=1; _i<=15; _i++) {
                    if (_n-_i >= 1 && _b[(_n-_i) % 16] ~ /(g_cli_gw_err|cli_gw_err_set)/) { _hit=1; break }
                }
                if (!_hit) printf " 静默return-1@%d行", NR;
            }
        }
    ' "$_sl_src")"
    if [ -n "$_sl_bad" ]; then
        _t3_bad="$_t3_bad cli_gw_call$_sl_bad"
    fi
fi
if [ -z "$_t3_bad" ]; then
    ok "T3 DR-4 失败可判读（统一错误响应 + CLI 描述面 + 原因透传 + 名称面）"
else
    bad "T3 DR-4 失败可判读结构异常:$_t3_bad"
fi

_t4_n=0
_t4_bad=""
for _code in $(grep -rhoE 'AIRY_ERR_[A-Z0-9_]+' "$CLI/src" 2>/dev/null | sort -u); do
    _t4_n=$((_t4_n+1))
    grep -qE "^#define $_code\b" "$ERRS" 2>/dev/null \
        || { _t4_bad="$_t4_bad $_code(未登记契约)"; continue; }
    if ! grep -qE "\{ *$_code," "$ERRN" 2>/dev/null && \
       ! grep -rqE "case $_code:" "$CLI/src" 2>/dev/null; then
        _t4_bad="$_t4_bad $_code(无可读名称)"
    fi
done
if [ "$_t4_n" -gt 0 ] && [ -z "$_t4_bad" ]; then
    ok "T4 DR-5 错误码契约完整（$_t4_n 项：契约登记 + 可读名称）"
else
    bad "T4 DR-5 错误码契约缺口:${_t4_bad:- 无错误码引用}"
fi

# EXC-T7(T5) 例外登记：门禁自检口径承认 CLI ⇄ TUI 同层内部边界为例外（见 T7 判据），
# 该边界内的 JSON Lines 事件流不计入 A-IPC 跨层调用面，T7 据此维护例外清单。
_t5_bad=""
grep -q 'verify_release_gates.sh' "$ROOT/tests/CMakeLists.txt" 2>/dev/null \
    || _t5_bad="$_t5_bad 门禁未注册到 ctest"
for _g in A S T; do
    grep -qE "^section \"$_g\" " "$0" 2>/dev/null || _t5_bad="$_t5_bad 分组 $_g 缺失"
done
if [ -z "$_t5_bad" ]; then
    ok "T5 DR-6 运行态门禁（ctest 注册 + A/S/T 分组齐备）"
else
    bad "T5 DR-6 运行态门禁异常:$_t5_bad"
fi

CMDL="$CLI/src/core/airy_cli_cmdline.c"
HELP_SRC="$CLI/src/cmd/cli_cmds.c"
COMP_SRC="$CLI/src/tui/tui_complete.c"
FE_SRC="$CLI/src/core/airy_cli_frontend.c"
MAIN_SRC="$CLI/src/core/main.c"

_t6_bad=""
_t6_n=0
if [ -f "$CMDL" ]; then
    _t6_n="$(awk '/^const cli_command_t CLI_COMMANDS\[\] = \{/,/^\};/' "$CMDL" \
             | grep -cE '^[[:space:]]*\{ *"/')"
    _t6_impl="$(grep -rhoE '^int cmd_[a-z_0-9]+\(' "$CLI/src" 2>/dev/null \
                | sed 's/^int //;s/(//' | sort -u | wc -l)"
    if [ "$_t6_n" -eq 0 ]; then
        _t6_bad="$_t6_bad 命令面契约源 CLI_COMMANDS 为空"
    elif [ "$_t6_n" -ne "$_t6_impl" ]; then
        _t6_bad="$_t6_bad 表项($_t6_n)≠cmd_*实现($_t6_impl)：存在第二命令面或孤立实现"
    fi
    for _fn in $(awk '/^const cli_command_t CLI_COMMANDS\[\] = \{/,/^\};/' "$CMDL" \
                 | grep -oE 'cmd_[a-z_0-9]+' | sort -u); do
        grep -rqE "^int $_fn\(" "$CLI/src" 2>/dev/null \
            || _t6_bad="$_t6_bad 表项函数未定义:$_fn"
    done
    grep -qE '#define CLI_COMMANDS_COUNT \(sizeof\(CLI_COMMANDS\) / sizeof\(CLI_COMMANDS\[0\]\)\)' "$CMDL" \
        || _t6_bad="$_t6_bad 命令计数非 sizeof 派生（存在漂移风险）"
else
    _t6_bad="$_t6_bad 命令面契约源缺失:$CMDL"
fi
grep -q 'cli_commands_count' "$HELP_SRC" 2>/dev/null || _t6_bad="$_t6_bad /help 未遍历 SSoT"
grep -q 'CLI_COMMANDS' "$COMP_SRC" 2>/dev/null || _t6_bad="$_t6_bad Tab 补全未遍历 SSoT"
grep -q 'cli_run_tui_frontend' "$MAIN_SRC" 2>/dev/null || _t6_bad="$_t6_bad /tui 未收敛至唯一实现"
if [ -n "$SDK_CONSOLE" ]; then
    grep -qE '^\[\[bin\]\]' "$SDK_CONSOLE/Cargo.toml" 2>/dev/null \
        && _t6_bad="$_t6_bad 第二用户面复活（console 含 [[bin]]）"
    [ -f "$SDK_CONSOLE/src/main.rs" ] \
        && _t6_bad="$_t6_bad 第二用户面复活（console 含 src/main.rs）"
else
    skip "T6 判据6 console 仓未检出，第二用户面防复活降级"
fi
if [ -z "$_t6_bad" ]; then
    ok "T6 判据6 用户命令面唯一（CLI_COMMANDS $_t6_n 项 = cmd_* 实现；/help 与补全共用 SSoT；console 零可执行面）"
else
    bad "T6 判据6 命令面一致性异常:$_t6_bad"
fi

_t7_bad=""
grep -q 'EXC-T7(T2)' "$0" 2>/dev/null || _t7_bad="$_t7_bad T2 取证口径未登记例外"
grep -q 'EXC-T7(T5)' "$0" 2>/dev/null || _t7_bad="$_t7_bad T5 门禁自检未登记例外"
if [ -f "$FE_SRC" ]; then
    if grep -qE 'aipc|airy_ipc|AIPC' "$FE_SRC" 2>/dev/null; then
        _t7_bad="$_t7_bad 同层内部边界误用 A-IPC"
    fi
    grep -q 'execve' "$FE_SRC" 2>/dev/null \
        || _t7_bad="$_t7_bad 渲染层同层派生子进程形态缺失"
else
    _t7_bad="$_t7_bad 渲染层入口实现缺失:$FE_SRC"
fi
if [ -z "$_t7_bad" ]; then
    ok "T7 §0.1 通信铁律例外登记（CLI ⇄ TUI 同层内部边界，T2/T5 判据集双点登记 + 实现侧零 A-IPC 依赖）"
else
    bad "T7 通信铁律例外登记异常:$_t7_bad"
fi

_t8_bad=""
if [ -f "$INSTALL" ]; then
    grep -q 'ensure_cli_entry' "$INSTALL" 2>/dev/null \
        && _t8_bad="$_t8_bad install.sh 垫片生成器复活"
    grep -qE 'exec "\$_DIR/airy_cli"' "$INSTALL" 2>/dev/null \
        && _t8_bad="$_t8_bad install.sh 以垫片冒充 agentrt-tui"
    if ! grep -qE 'exec "\\\$AIRY_HOME/bin/airy_cli" --tui' "$INSTALL" 2>/dev/null; then
        _t8_bad="$_t8_bad install.sh 启动器未以 airy_cli 为唯一前端"
    fi
else
    _t8_bad="$_t8_bad install.sh 缺失"
fi
_t8_seen=0
for _f in "$LATEST_RT" "$SDK_AIRYMAXRT"; do
    [ -f "$_f" ] || continue
    _t8_seen=$((_t8_seen+1))
    if grep -qE '自动续接|将使用 airy_cli 作为前端|TUI_BIN|AIRYRT_FORCE_CLI|terminal_capable_tui' "$_f" 2>/dev/null; then
        _t8_bad="$_t8_bad 启动器接力逻辑残留:${_f##*/agent-workload/}"
    fi
done
[ "$_t8_seen" -eq 0 ] && _t8_bad="$_t8_bad 启动器副本全不可达"
if [ -z "$_t8_bad" ]; then
    ok "T8 垫片与接力禁止（install.sh 无垫片 + $_t8_seen 份启动器零接力残留）"
else
    bad "T8 垫片或接力残留:$_t8_bad"
fi

section "U" "B1 会话上下文轮次边界（上下文串轮防复发：历史门控 + 边界标记 + 轮次标注）"

_u_missing=""
_CTX="$ROOT/../sdk/tui/src/app/context.rs"
_TASK="$ROOT/../sdk/tui/src/app/task.rs"
_MEM="$ROOT/../sdk/tui/src/memory.rs"
_LOOP="$ROOT/daemons/agent_d/src/agent_run_loop.c"
_ENG="$ROOT/daemons/agent_d/src/agent_run_engine.c"
for _f in "$_CTX" "$_TASK" "$_MEM" "$_LOOP" "$_ENG"; do
    [ -f "$_f" ] || _u_missing="$_u_missing $(basename "$_f")"
done
if [ -n "$_u_missing" ]; then
    bad "U0 B1 结构性文件缺失:$_u_missing"
else
    _u1_bad=""
    grep -q 'enum HistoryPolicy' "$_CTX" || _u1_bad="$_u1_bad 无 HistoryPolicy"
    grep -q 'Gated' "$_CTX" || _u1_bad="$_u1_bad 无 Gated 策略"
    grep -q 'fn needs_anaphora_history' "$_CTX" || _u1_bad="$_u1_bad 无指代检测"
    grep -qE 'chars\(\)\.count\(\) <= 8|len\(\) <= 8' "$_CTX" \
        && _u1_bad="$_u1_bad 纯长度判据回潮（短≠指代，误注入复活串轮）"
    if [ -z "$_u1_bad" ]; then
        ok "U1 Gated 历史门控存在且无纯长度判据（V1.1 机制）"
    else
        bad "U1 历史门控退化:$_u1_bad"
    fi

    if grep -q 'HIST_PREFIX' "$_CTX" && grep -q 'HIST_SUFFIX' "$_CTX" \
        && grep -q '【轮次边界】' "$_CTX"; then
        ok "U2 历史包裹标记与轮次边界声明存在（V1.3 机制）"
    else
        bad "U2 边界标记或轮次声明缺失:$_CTX"
    fi

    if grep -q '【轮次边界】' "$_LOOP" && grep -q 'cJSON_InsertItemInArray' "$_LOOP" \
        && grep -q '【轮次边界】' "$_ENG"; then
        ok "U3 agent_d 轮次约束兜底双侧存在（主对话头插 + GCCP plan 追加）"
    else
        bad "U3 agent_d 轮次约束兜底缺失"
    fi

    if grep -q 'fn mem_push_tagged' "$_TASK" && grep -q 'turn:' "$_MEM"; then
        ok "U4 记忆写入-召回轮次标注链路存在（turn:N）"
    else
        bad "U4 记忆轮次标注链路断裂"
    fi
fi

section "V" "B3 控制面/用户面隔离（[MODE:] 协议与内部标识符零泄漏：V3.1~V3.4 机制防回潮）"

_v_missing=""
_MODE="$ROOT/../sdk/tui/src/app/mode.rs"
_POLL="$ROOT/../sdk/tui/src/app/poll.rs"
_DISP="$ROOT/../sdk/tui/src/app/dispatch.rs"
_TSK="$ROOT/../sdk/tui/src/app/task.rs"
for _f in "$_MODE" "$_POLL" "$_DISP" "$_TSK"; do
    [ -f "$_f" ] || _v_missing="$_v_missing $(basename "$_f")"
done
if [ -n "$_v_missing" ]; then
    bad "V0 B3 结构性文件缺失:$_v_missing"
else
    if grep -q 'pub struct StreamSanitizer' "$_MODE" \
        && grep -q 'stream_sanitizer\.feed' "$_POLL"; then
        ok "V1 流式中间态净化在位（V3.1：SSE 增量过 StreamSanitizer）"
    else
        bad "V1 流式净化缺失或未接线（poll.rs 直通上屏回潮）"
    fi

    if grep -q 'protocol_buf' "$_MODE" && grep -q '协议段已剥离' "$_TSK"; then
        ok "V2 协议段诊断通道在位（V3.2：剥出文本入 F3 而非删除）"
    else
        bad "V2 协议诊断通道断裂（剥出即丢弃，V3.2 退化）"
    fi

    if grep -q 'pub fn sanitize_reply' "$_MODE" \
        && grep -q 'fn strip_mode_segments' "$_MODE"; then
        ok "V3 非流式结构化解析在位（V3.3：前导元话语随标记剥离）"
    else
        bad "V3 结构化解析缺失（仅剥标记、保留元话语回潮）"
    fi

    if grep -q 'TOOL_ACTIONS' "$_DISP" && grep -q '未知工具（已隐藏）' "$_DISP" \
        && ! grep -rq 'tool\.to_string()' "$ROOT/../sdk/tui/src"; then
        ok "V4 标识符白名单兜底在位（V3.4：未登记名零透传）"
    else
        bad "V4 白名单兜底缺失或 tool.to_string() 裸透传回潮"
    fi
fi

_v2_engine="$ROOT/daemons/agent_d/src/agent_run_engine.c"
_v2_task="$ROOT/../sdk/tui/src/app/task.rs"
_v2_block="$ROOT/../sdk/tui/src/panels/chat/block.rs"
_v2_mem="$ROOT/../sdk/tui/src/memory.rs"
_v2_sess="$ROOT/../sdk/tui/src/app/session.rs"
_v2_keys="$ROOT/../sdk/tui/src/keys.rs"
_v2_panel="$ROOT/../sdk/tui/src/app/panel.rs"
section "V2" "B4 思维链原文泄漏（默认不上屏 / 不落长期记忆 / 恢复不回灌 V4.1~V4.3 防回潮）"

if grep -qF '思考链不再随 result 直出' "$_v2_engine" \
    && ! grep -qF 'cJSON_AddStringToObject(result, "reasoning"' "$_v2_engine" \
    && grep -qF '0.1.18 B4 起默认不上屏、不落长期记忆' "$_v2_task" \
    && grep -qF '0.1.18 B4：思考链不再生成 System 消息' "$_v2_block"; then
    ok "V2.1 V4.1 思考链默认不上屏（result 零直出 + 不生成 System 消息，含≤6行短链）"
else
    bad "V2.1 V4.1 思考链默认上屏回潮（result 直出或 System 消息渲染路径复活）"
fi

if grep -qF 'fn push(&mut self, role: &str, content: &str, tags: &str)' "$_v2_mem" \
    && grep -qF '不再提供携带思考链' "$_v2_mem" \
    && grep -qF 'fn push_never_records_reasoning()' "$_v2_mem" \
    && grep -qF '恢复会话不得回灌思考链原文' "$_v2_sess" \
    && ! grep -q 'rec\.reasoning' "$_v2_sess"; then
    ok "V2.2 V4.2 思考链不落长期记忆且恢复不回灌（写入面无 reasoning 入参 + 恢复仅取 content）"
else
    bad "V2.2 V4.2 思考链落长期记忆或恢复回灌回潮（memory.rs 写入面 / session.rs 恢复面）"
fi

if grep -qF '打开思考链独立视图（Alt+E）' "$_v2_panel" \
    && grep -qF 'Alt+E：打开思考链独立视图' "$_v2_keys"; then
    ok "V2.3 V4.3 Alt+E 独立视图在位（能力未丢失：按需取用唯一数据源）"
else
    bad "V2.3 V4.3 思考链独立视图缺失（Alt+E 能力回退）"
fi

section "W" "B11 TUI 滚动/鼠标/焦点交互（滚动契约 SSoT 与判据 V11.1~V11.3 机制防回潮）"

_w_chat="$ROOT/../sdk/tui/src/panels/chat/mod.rs"
_w_ctrl="$ROOT/../sdk/tui/src/app/control.rs"
_w_main="$ROOT/../sdk/tui/src/main.rs"
_w_keys="$ROOT/../sdk/tui/src/keys.rs"
_w_ui="$ROOT/../sdk/tui/src/ui.rs"
_w_tests="$ROOT/../sdk/tui/src/app/tests.rs"
_w_missing=""
for _f in "$_w_chat" "$_w_ctrl" "$_w_main" "$_w_ui" "$_w_tests"; do
    [ -f "$_f" ] || _w_missing="$_w_missing $(basename "$_f")"
done
if [ -n "$_w_missing" ]; then
    bad "W0 B11 结构性文件缺失:$_w_missing"
else
    if grep -q 'app\.page_step = viewport' "$_w_chat" \
        && grep -q 'app\.chat_scroll_max = frame\.total' "$_w_chat"; then
        ok "W1 滚动契约渲染回写在位（V11.2：page_step=视口高度，resize 自动跟随）"
    else
        bad "W1 滚动契约回写断裂（渲染→控制面 SSoT 单向流退化）"
    fi

    if grep -q 'min(self\.chat_scroll_max)' "$_w_ctrl" \
        && grep -q 'saturating_sub(self\.page_step)' "$_w_ctrl" \
        && ! grep -q 'self\.scroll_offset = self\.scroll_offset\.saturating_add(10)' "$_w_ctrl"; then
        ok "W2 控制面钳位翻页在位（V11.3：无滚动量 no-op；固定常量翻页已废除）"
    else
        bad "W2 控制面滚动契约退化（钳位缺失或固定常量翻页回潮）"
    fi

    if grep -q 'let mut mouse_on = app\.mouse_capture' "$_w_main" \
        && grep -q 'app\.mouse_capture != mouse_on' "$_w_main" \
        && grep -q 'mouse_capture: false' "$ROOT/../sdk/tui/src/app/mod.rs"; then
        ok "W3 鼠标捕获唯一执行点在位（V11.1：默认关，Ctrl+M 会话级差分切换）"
    else
        bad "W3 鼠标捕获执行点散落或默认态回潮"
    fi

    if grep -q 'MouseEventKind::ScrollUp' "$_w_main" \
        && grep -q 'wheel_lines(shift, ctrl)' "$_w_main"; then
        ok "W4 滚轮事件分派在位（V11.1：修饰键语义 Shift 翻页/Ctrl 单行）"
    else
        bad "W4 滚轮事件分派缺失（捕获开启后滚轮无响应）"
    fi

    if grep -q 'pub fn render_focus' "$_w_chat" \
        && grep -q 'ActivePanel::Focus => panels::chat::render_focus' "$_w_ui" \
        && grep -q 'KeyCode::Char(.f.) | KeyCode::Char(.F.)' "$_w_keys"; then
        ok "W5 焦点视图路由与键位在位（Alt+F 全屏只读 + Esc 返回）"
    else
        bad "W5 焦点视图路由或键位断裂"
    fi

    if grep -q 'fn b11_scroll_clamps_to_chat_scroll_max' "$_w_tests" \
        && grep -q 'fn b11_wheel_lines_follows_modifiers' "$_w_tests" \
        && grep -q 'fn b11_focus_open_snapshots_last_reply' "$_w_tests" \
        && grep -q 'fn b11_hint_covers_position_states' "$_w_chat"; then
        ok "W6 B11 单元测试防回潮在位（钳位/滚轮/焦点/位置指示契约）"
    else
        bad "W6 B11 单元测试缺失（契约回归无守卫）"
    fi
fi

_lg="$ROOT/atoms/coreloopthree/src/lang_gateway"
section "X" "B6 语言网关校准与路由（校准异步化/容量 SSoT/阈值同源 V6.1~V6.3 机制防回潮）"

_lg_missing=""
for _f in "$_lg/canonical.h" "$_lg/lang_router.c" "$_lg/calibrator.c" \
          "$_lg/lang_gateway.c" \
          "$ROOT/atoms/coreloopthree/tests/unit/test_lang_gateway.c"; do
    [ -f "$_f" ] || _lg_missing="$_lg_missing $(basename "$_f")"
done
if [ -n "$_lg_missing" ]; then
    bad "X0 B6 结构性文件缺失:$_lg_missing"
else
    if grep -q 'void cal_worker_post' "$_lg/canonical.h" \
        && grep -q '懒启动' "$_lg/calibrator.c" \
        && grep -q 'cal_worker_stop(gw)' "$_lg/lang_gateway.c"; then
        ok "X1 校准后台 worker 异步化在位（V6.1：懒启动+pending 合并，首轮不阻塞请求路径）"
    else
        bad "X1 校准回潮为请求路径同步执行（V6.1 退化）"
    fi

    if grep -q '_Static_assert' "$_lg/canonical.h" \
        && grep -q 'AIRY_LANG_PROFILE_MAX \* sizeof' "$_lg/canonical.h" \
        && grep -q '>= AIRY_LANG_PROFILE_MAX' "$_lg/calibrator.c"; then
        ok "X2 画像容量 SSoT 统一在位（V6.2：_Static_assert 绑定 + 越界 fail-fast 拒收）"
    else
        bad "X2 画像容量绑定断裂（容量宏与结构体数组失去一致性约束）"
    fi

    if grep -q 'profile->ratio_high > 0.0 ? profile->ratio_high' "$_lg/lang_router.c" \
        && grep -q 'AIRY_LANG_RATIO_HIGH_DEFAULT' "$_lg/canonical.h" \
        && ! grep -Eq 'ratio (>|\?) 1\.2|1\.2 .*→ en' "$_lg/lang_router.c"; then
        ok "X3 路由阈值画像化在位（V6.3：改画像即改阈值，SSoT 宏回退）"
    else
        bad "X3 路由阈值硬编码回潮（V6.3 同源同值断裂）"
    fi

    if grep -q '"version", 2' "$_lg/calibrator.c" \
        && grep -q 'test_route_threshold_from_profile' \
            "$ROOT/atoms/coreloopthree/tests/unit/test_lang_gateway.c"; then
        ok "X4 阈值持久化与测试守卫在位（version 2 契约 + V6.3 单元测试）"
    else
        bad "X4 阈值持久化契约或测试守卫缺失"
    fi
fi

_tui="$ROOT/../sdk/tui/src/app"
_ad="$ROOT/daemons/agent_d"
section "Y" "B2 首字延迟与零反馈（流式契约/零反馈/打字机解耦/分段耗时 V2.1~V2.4 机制防回潮）"

if grep -q 'daemon_rpc_call_stream(llm_sock, "complete_stream"' "$_ad/src/agent_run_loop.c" \
    && grep -q 'RUN_DELTA_STACK' "$_ad/src/agent_run_loop.c" \
    && grep -q 'AIRY_MALLOC(dlen + 1)' "$_ad/src/agent_run_loop.c" \
    && ! grep -q 'daemon_rpc_call(llm_sock, "complete"' "$_ad/src/agent_run_loop.c"; then
    ok "Y1 流式契约在位（V2.1/V2.2：complete_stream 真实增量，超限堆扩容不截断不丢内容）"
else
    bad "Y1 流式契约回潮（退化为非流式 complete 或定长截断）"
fi

if grep -q 'pub(super) fn begin_busy' "$_tui/control.rs" \
    && grep -q '已受理' "$_tui/control.rs"; then
    ok "Y2 零反馈消除在位（V2.3：begin_busy 唯一入口，首 busy 帧即「已受理 · N.Ns」）"
else
    bad "Y2 零反馈回潮（请求发出到首字节之间无可见状态）"
fi

if grep -q 'fn typewriter_enabled' "$_tui/mod.rs" \
    && grep -q 'AIRY_TUI_TYPEWRITER' "$_tui/mod.rs" \
    && grep -q 'if !self.typewriter' "$_tui/poll.rs" \
    && grep -q 'self.streaming_reveal = total' "$_tui/poll.rs"; then
    ok "Y3 打字机解耦在位（V2.4：默认关、关闭当拍追平全文、动画不门控落定）"
else
    bad "Y3 打字机耦合回潮（默认开启或落定被 reveal 进度门控）"
fi

if grep -q 'AIRY_RS_K_THINK_MS' "$_ad/src/agent_run_engine.c" \
    && grep -q 'AIRY_RS_K_LLM_MS' "$_ad/src/agent_run_engine.c" \
    && grep -q 'AIRY_RS_K_TOOL_MS' "$_ad/src/agent_run_engine.c"; then
    ok "Y4 分段耗时落盘在位（think/llm/tool 三段度量，V2.1 不达标可分段归因）"
else
    bad "Y4 分段耗时缺失（首字延迟不可归因）"
fi

if grep -q '900B payload not truncated' "$_ad/tests/test_run_loop.c" \
    && grep -q 'fn typewriter_default_off_and_env_gated' "$_tui/tests.rs" \
    && grep -q 'fn begin_busy_marks_request_start' "$_tui/tests.rs"; then
    ok "Y5 B2 测试守卫在位（V2.2 截断回归 + V2.3/V2.4 Rust 断言）"
else
    bad "Y5 B2 测试守卫缺失"
fi

_zrr="$ROOT/daemons/sched_d/src/roadmap_rpc.c"
_zcr="$ROOT/gateway/src/biz/gateway_cap_registry.c"
section "Z" "B8 架构文档名实一致（V8.3 roadmap_status 实例状态名防回潮）"

if grep -q 'method_dispatcher_register(d, "roadmap_status"' "$_zrr" \
    && grep -q '"sched.roadmap_status"' "$_zcr" \
    && grep -q '`roadmap_status`' "$ROOT/daemons/sched_d/README.md" \
    && grep -q 'roadmap_status' "$ROOT/daemons/gateway_d/README.md" \
    && ! grep -rq --exclude-dir=.git --include='*.c' --include='*.h' --include='*.rs' --include='*.md' 'roadmap_stats' "$ROOT"; then
    ok "Z1 roadmap_status 名实一致在位（V8.3：仅就绪态与服务标识，全树旧名零残留）"
else
    bad "Z1 roadmap_stats 名实不符回潮（handler/注册表/README 不同步或旧名残留）"
fi

_wc="$ROOT/daemons/mem_d/src/engine/compress.c"
_wh="$ROOT/daemons/mem_d/include/compress.h"
_wch="$ROOT/daemons/mem_d/src/handlers/cache_handlers.c"
_wsr="$ROOT/daemons/llm_d/src/service_request.c"
_wlh="$ROOT/daemons/mem_d/src/handlers/ledger_handlers.c"
section "AA" "B5 压缩/缓存安全门禁（V5.1~V5.3 fail-closed 防回潮）"

if grep -q '\.l2_enabled = 0, /\* 默认关' "$_wc" \
    && grep -q '\.gate = { \.grayscale_enabled = 0, \.acr = -1\.0, \.ttft_ms = -1\.0 }' "$_wc" \
    && grep -q 'COMPRESS_GATE_MIN_ACR' "$_wh" \
    && grep -q 'COMPRESS_GATE_MAX_TTFT_MS' "$_wh"; then
    ok "AA1 V5.1 L2 默认关 + 门禁默认全关 fail-closed（未过门禁不得生效）"
else
    bad "AA1 V5.1 L2/门禁默认值回潮为开（compress.c/compress.h）"
fi

if grep -q 'mem_cache_admit' "$_wch" \
    && grep -q '!cacheable' "$_wsr" \
    && grep -q 'manager->cacheable' "$_wsr"; then
    ok "AA2 V5.2 缓存双门在位（调用方 cacheable 声明 + mem_d 敏感面 admit 拒绝）"
else
    bad "AA2 V5.2 缓存敏感面双门缺失（cache_handlers/service_request）"
fi

if grep -q 'gate->acr < COMPRESS_GATE_MIN_ACR' "$_wc" \
    && grep -q 'gate->ttft_ms > COMPRESS_GATE_MAX_TTFT_MS' "$_wc" \
    && grep -q '"acr"' "$_wlh" \
    && grep -q '"ttft_ms"' "$_wlh"; then
    ok "AA3 V5.3 gate 数值语义在位（阈值判定 + acr/ttft JSON 数值暴露，非桩值）"
else
    bad "AA3 V5.3 gate 阈值判定或数值暴露链缺失（compress.c/ledger_handlers.c）"
fi

_wp="$ROOT/atoms/coreloopthree/src/work_hall/work_hall_persist.c"
_whh="$ROOT/atoms/coreloopthree/include/work_hall.h"
_whc="$ROOT/atoms/coreloopthree/src/work_hall/work_hall.c"
_wha="$ROOT/atoms/coreloopthree/src/work_hall/work_hall_agent.c"
section "AB" "B7 工作大厅超时与重启恢复（V7.1~V7.3 语义防回潮）"

if grep -q 'strcmp(en->state, "skipped") == 0' "$_wp" \
    && grep -q 'snprintf(en->state, sizeof(en->state), "failed")' "$_wp" \
    && ! grep -q 'normalize them to canceled' "$_wp"; then
    ok "AB1 V7.2 非终态中断恢复为 failed（不静默归一化 canceled，skipped 计入终态）"
else
    bad "AB1 V7.2 非终态恢复语义回潮（work_hall_persist.c 归一化为 canceled）"
fi

if grep -q 'cJSON_AddStringToObject(o, "input_json"' "$_wp" \
    && grep -q 'cJSON_AddNumberToObject(o, "redispatch_count"' "$_wp" \
    && grep -q 'en->input_json = AIRY_STRDUP(s)' "$_wp" \
    && grep -q 'en->redispatch_count = (int32_t)it->valuedouble' "$_wp"; then
    ok "AB2 V7.2 恢复字段持久化（save 输入副本/预算 + load 恢复保真）"
else
    bad "AB2 V7.2 恢复字段持久化缺失（input_json/redispatch_count 未落盘或未恢复）"
fi

if grep -q 'hall->redispatch_max > 0' "$_wp" \
    && grep -q 'en->redispatch_count < hall->redispatch_max' "$_wp"; then
    ok "AB3 V7.2 预算内重派重新布防（redispatch_at 按当前时基重算）"
else
    bad "AB3 V7.2 重启后重派未重新布防（work_hall_persist.c）"
fi

if grep -q 'wh_json_language(input_json' "$_wha" \
    && grep -q '请求输入面（input_json 顶层）优先于节点声明面' "$_wha"; then
    ok "AB4 V7.3 language 取自请求（请求输入面优先于节点声明面）"
else
    bad "AB4 V7.3 language 请求面优先语义缺失（work_hall_agent.c）"
fi

if grep -q 'AIRY_WORK_HALL_DEFAULT_TIMEOUT_MS 300000' "$_whh" \
    && grep -q 'AIRY_WORK_HALL_TIMEOUT_MS' "$_whh" \
    && grep -q 'getenv("AIRY_WORK_HALL_TIMEOUT_MS")' "$_whc"; then
    ok "AB5 V7.1 超时 SSoT（默认 300000 + AIRY_WORK_HALL_TIMEOUT_MS 覆盖）"
else
    bad "AB5 V7.1 超时默认值或 env 覆盖链缺失（work_hall.h/work_hall.c）"
fi

section "AC" "B10 update 事务化与互斥（版本目录+current 原子切换 / mkdir 互斥 / intent 幂等重放 V10.1~V10.4 防回潮）"

case "$(sdk_ready; echo $?)" in
    0)
        _b10="$SDK_AIRYMAXRT"
        if grep -qF 'TMPD="$AIRY_HOME/releases/.staging.$$"' "$_b10" \
            && grep -qF 'ln -sfn "$target" "$AIRY_HOME/current"' "$_b10" \
            && grep -qF 'mv "$extracted" "$rel"' "$_b10" \
            && grep -q '永不就地改写已发布版本目录' "$_b10"; then
            ok "AC1 V10.1 事务化消除空窗（staging 临时目录 + rename 就位 + current 符号链接原子切换）"
        else
            bad "AC1 V10.1 update 回潮为就地覆盖（staging/rename/current 切换链缺失）"
        fi

        if grep -qF 'mkdir "$lk" 2>/dev/null' "$_b10" \
            && grep -q '另一更新进程持锁运行中' "$_b10" \
            && grep -qF 'upd_lock_rel() {' "$_b10" \
            && grep -qF 'upd_lock_acq || return 1' "$_b10"; then
            ok "AC2 V10.2 互斥锁在位（mkdir 独占 + 并发显式拒绝提示 + 全路径释放）"
        else
            bad "AC2 V10.2 update 互斥缺失（并发 update 互踩风险）"
        fi

        if grep -q 'sha256 校验失败，拒绝安装' "$_b10" \
            && grep -q '缺少 sha256 期望值，拒绝安装（防供应链绕过）' "$_b10" \
            && grep -q '制品自检失败（bin/ 关键组件缺失）' "$_b10" \
            && grep -q '制品自检失败（lib/${_py} python 运行时缺失）' "$_b10"; then
            ok "AC3 V10.3 校验/自检失败显式报错且保留上一可用版本（含失败原因）"
        else
            bad "AC3 V10.3 校验/自检失败静默回滚或吞错（用户不可见原因）"
        fi

        if grep -qF 'upd_intent_set() {' "$_b10" \
            && grep -qF 'upd_intent_clear() {' "$_b10" \
            && grep -qF 'upd_boot_recover() {' "$_b10" \
            && grep -q '在途更新恢复未完成（保留 intent 待下次重试）' "$_b10"; then
            ok "AC4 V10.4 intent 幂等重放+启动自恢复（中断更新可续、不二义）"
        else
            bad "AC4 V10.4 更新 intent 幂等重放/启动自恢复缺失（中断更新不可续）"
        fi

        if grep -qF 'rm -rf "${AIRY_HOME}/releases/$from_id"' "$_b10" \
            && grep -q '无可用回滚目标（releases/ 仅含当前版本）' "$_b10" \
            && grep -q '管理命令（update 等）不 source 旧制品 env/secrets 脚本' "$_b10"; then
            ok "AC5 V10.4 回滚终态移除被放弃版本（二次回滚显式拒绝）+ 管理命令免注入损坏脚本"
        else
            bad "AC5 V10.4 回滚乒乓回切或损坏脚本阻断更新自救通道"
        fi
        ;;
    1)
        skip "AC B10 事务化断言（sdk airymaxrt 未检出，跳过）"
        ;;
    *)
        bad "AC B10 AIRY_GATE_SDK_AIRYMAXRT 显式指定但文件缺失: $SDK_AIRYMAXRT"
        ;;
esac

section "AD" "B9 macOS 平台探测收敛与干净机发布门禁（V9.1~V9.4 防回潮）"

case "$(sdk_ready; echo $?)" in
    0)
        _b9="$SDK_AIRYMAXRT"
        if grep -qF '拒绝静默降级为 minimal' "$_b9" \
            && grep -qF '硬件探测失败，无法自动评估画像；请显式设置 AIRYRT_PROFILE=full|minimal 后重试' "$_b9"; then
            ok "AD1 V9.1/V9.2 探测失败显式失败（不伪装成 minimal 降级 → 不裁剪 daemon）"
        else
            bad "AD1 V9.1 探测失败静默降级回潮（macOS 恒判 minimal 裁剪 daemon）"
        fi

        if grep -qF 'if command -v ldd >/dev/null 2>&1; then' "$_b9" \
            && grep -qF '运行库: 平台无 ldd，跳过动态库解析检查' "$_b9"; then
            ok "AD2 V9.3 ldd 调用有守卫（非 Linux 平台不误报运行库缺失）"
        else
            bad "AD2 V9.3 无守卫调用 ldd 误报（macOS doctor 假缺失）"
        fi
        ;;
    1)
        skip "AD B9 平台探测断言（sdk airymaxrt 未检出，跳过）"
        ;;
    *)
        bad "AD B9 AIRY_GATE_SDK_AIRYMAXRT 显式指定但文件缺失: $SDK_AIRYMAXRT"
        ;;
esac

_mch="$ROOT/.github/workflows/macos-clean-host.yml"
_mchk="$ROOT/.github/scripts/verify-macos-clean-host.sh"
if [ -f "$_mch" ] && [ -f "$_mchk" ] \
    && grep -qF 'macos-latest' "$_mch" \
    && grep -qF 'macos-15-intel' "$_mch" \
    && grep -qF 'gateway online' "$_mchk" \
    && grep -qF '要求 N==M 且 M>0' "$_mchk"; then
    ok "AD3 V9.4 macOS 干净机发布门禁在位（arm-64/x86-64 双腿 + gateway online N==M 出证）"
else
    bad "AD3 V9.4 macOS 干净机发布门禁缺失（V9.4 三平台 smoke 未纳入）"
fi

printf '\n门禁汇总: PASS=%d FAIL=%d\n' "$PASS" "$FAIL"
if [ "$FAIL" -gt 0 ]; then
    printf '  [GATE] WS-9 社区六类问题修复发布门禁未通过（方案 §4.9 / 9.8）\n'
    exit 1
fi
printf '  [GATE] WS-9 发布门禁全绿（9.1~9.11 判据）\n'
exit 0
