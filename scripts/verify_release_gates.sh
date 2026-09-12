#!/usr/bin/env bash
# scripts/verify_release_gates.sh — WS-9 社区六类问题修复发布门禁（0.1.15 方案 §4.9 / 步骤 9.8）
# @owner: team-C
#
# 断言 9.1~9.11 各步骤的「可复现判据」在仓内保持成立，防止已修复的社区
# 六类问题（更新/安装/启动/稳定/长对话/长任务）回归。由 ctest 以
# release_gates 用例驱动（tests/CMakeLists.txt，挂 WS-6 CI required）。
#
# 判据分组（编号对应方案 §4.9 步骤表）：
#   A  9.2/I-01   install.sh 前向兼容（安装问题：函数先定义后调用）
#   B  9.3/I-02① detect_arch 等四函数三副本逐字节一致（install.sh/latest/sdk）
#   C  9.3/I-02② detect_arch 20 例平台矩阵仿真（含 aarch64 32 位陷阱）
#   D  9.4/C-01   airy_cli 聊天路径单一实现（cli_chat.c，无 stream 遗留）
#   E  9.6/S-01   TUI 会话历史环形裁剪（长对话问题）
#   F  9.7/S-02   长任务可取消 + 超时可诊断
#   G  9.11/S-04  reasoning 截断 + 日志轮转（长任务问题）
#   H  9.9/U-02~4 三份脚本通道白名单同集 + 保留通道文案 + 版本占位合法
#   I  9.10/I-03  架构白名单精确串 + 全仓零 riscv 表述 + 源码构建指引
#   J  9.1/U-01   更新器 detect_pending_release（更新问题：落后指针静默）
#   K  （可选）   发布侧 publish-release.sh 白名单契约（本地可达才断言）
#
# 跨仓判据（B/J/部分 H/I）依赖 sdk 仓 airymaxrt。探测顺序：
#   1. AIRY_GATE_SDK_AIRYMAXRT 显式指定（CI 取料 step 用；指定但缺失 → FAIL）
#   2. $ROOT/../sdk/tui/scripts/airymaxrt（hub 本地布局，与 agentrt 并列）
#   3. $ROOT/agent-workload/sdk/tui/scripts/airymaxrt（CI 取料布局）
# 自动探测失败 → 跨仓组 SKIP（本地开发常态）；CI 侧由取料 step fail-closed
# 兜底（clone 失败即红，门禁不静默降级）。
#
# 兼容性：macOS job 亦跑 ctest，本脚本必须 bash-3.2 兼容
# （无关联数组/mapfile/${var,,}/lastpipe；计数 while 用重定向非管道）。

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

# ---------- 路径 ----------
INSTALL="$ROOT/scripts/install.sh"
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

# 跨仓判据前置：rc 0=可执行 / 1=应 SKIP（本地无 sdk）/ 2=应 FAIL（显式指定但缺失）
sdk_ready() {
    if [ "$SDK_EXPLICIT" -eq 1 ] && [ ! -f "$SDK_AIRYMAXRT" ]; then
        return 2
    fi
    [ -f "$SDK_AIRYMAXRT" ] && return 0
    return 1
}

# 提取顶格函数定义全文（函数体内均缩进，顶格 } 仅出现在函数结束）
extract_fn() { # <file> <fnname> <outfile>
    sed -n "/^$2() {/,/^}$/p" "$1" > "$3"
}

# ---------- 结构性文件存在性前置（缺失即红，不逐条刷屏）----------
_missing=0
for _f in "$INSTALL" "$LATEST_RT" \
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

# ============================================================
# 组 A · 9.2/I-01 install.sh 前向兼容
# ============================================================
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

# ============================================================
# 组 B · 9.3/I-02① detect_arch 四函数三副本逐字节一致
# ============================================================
section "B" "9.3/I-02① 架构检测四函数三副本逐字节一致（install.sh / latest / sdk）"

for _fn in _uspace_bits _loader_exists _arch_warn_unknown detect_arch; do
    extract_fn "$INSTALL"   "$_fn" "$TMP/a_$_fn"
    extract_fn "$LATEST_RT" "$_fn" "$TMP/b_$_fn"

    case "$(sdk_ready; echo $?)" in
        0)
            extract_fn "$SDK_AIRYMAXRT" "$_fn" "$TMP/c_$_fn"
            if [ "$_fn" = "_arch_warn_unknown" ]; then
                # 呈现层函数：宿主日志设施各异（install.sh/latest=printf>&2+C_YELLOW，
                # sdk=log warn），SSoT 边界=触发条件与文案，不做逐字节 cmp
                _w=0
                for _g in "$TMP/a_$_fn" "$TMP/b_$_fn" "$TMP/c_$_fn"; do
                    grep -q '用户空间位宽未知' "$_g" || _w=1
                done
                if [ "$_w" -eq 0 ]; then
                    ok "B($_fn) 三副本检测语义一致（呈现设施各异：printf>&2 / log warn，SSoT=文案）"
                else
                    bad "B($_fn) 告警文案漂移"
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

# ============================================================
# 组 C · 9.3/I-02② detect_arch 20 例平台矩阵仿真
# ============================================================
section "C" "9.3/I-02② detect_arch 20 例平台矩阵仿真（stub 注入 uname/bits/loader）"

# 用仓内真实实现组装被测库（零硬拷贝，实现漂移即暴露）
extract_fn "$INSTALL" _uspace_bits       "$TMP/fn_bits"
extract_fn "$INSTALL" _loader_exists     "$TMP/fn_loader"
extract_fn "$INSTALL" _arch_warn_unknown "$TMP/fn_warn"
extract_fn "$INSTALL" detect_arch        "$TMP/fn_detect"
cat "$TMP/fn_bits" "$TMP/fn_loader" "$TMP/fn_warn" "$TMP/fn_detect" > "$TMP/archlib.sh"
# 同名 stub 后定义覆盖先定义：_uspace_bits/_loader_exists 由环境变量供值；
# uname 走 PATH 前置 stub；_arch_warn_unknown 的 stderr 告警丢弃保 stdout 纯净。
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

# 表：uname -m|_uspace_bits|loader 存在名|期望 detect_arch 输出。
# 空字段统一用 "-" 占位（bash read 末变量吃剩余整段，连续分隔符会错位），
# read 后还原为空串。
arch_fail=0
arch_n=0
# while 用重定向（非管道）防 bash-3.2 子 shell 丢计数
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

# ============================================================
# 组 D · 9.4/C-01 聊天路径单一实现
# ============================================================
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

# ============================================================
# 组 E · 9.6/S-01 TUI 会话历史环形裁剪（长对话）
# ============================================================
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

# ============================================================
# 组 F · 9.7/S-02 长任务可取消 + 超时可诊断
# ============================================================
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

# ============================================================
# 组 G · 9.11/S-04 reasoning 截断 + 日志轮转（长任务）
# ============================================================
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

# ============================================================
# 组 H · 9.9/U-02~4 通道白名单三副本同集 + 占位合法
# ============================================================
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
if [ -f "$SDK_AIRYMAXRT" ]; then
    grep -Fq 'beta 为保留通道' "$SDK_AIRYMAXRT" || _h2=0
fi
if [ "$_h2" -eq 1 ]; then
    ok "H2 beta 缺席时保留通道明确文案在位（U-03：可操作提示）"
else
    bad "H2 保留通道文案缺失（beta 403 时用户无指引）"
fi

if grep -Eq '^AIRY_VERSION="\$\{AIRY_VERSION:-v[0-9]+\.[0-9]+\.[0-9]+\}"$' "$INSTALL"; then
    ok "H3 版本占位格式合法（U-04：curl 管道形态兜底可解析；占位超前指针属 bump 窗口常态不断言相等）"
else
    bad "H3 版本占位行缺失或格式漂移"
fi

# ============================================================
# 组 I · 9.10/I-03 架构白名单 + 全仓零 riscv 表述
# ============================================================
section "I" "9.10/I-03 架构白名单精确串 + 全仓零 riscv 表述 + 源码构建指引"

if grep -Fq 'SUPPORTED_ARCHS="x86_64 aarch64 i686 armv7l"' "$INSTALL"; then
    ok "I1 SUPPORTED_ARCHS 白名单精确串在位"
else
    bad "I1 SUPPORTED_ARCHS 白名单漂移"
fi

_i2=0
for _mf in "$ROOT/latest/manifest.stable.json" "$ROOT/latest/manifest.rc.json"; do
    if [ -f "$_mf" ] && grep -riq 'riscv' "$_mf"; then
        _i2=1
    fi
done
if [ "$_i2" -eq 0 ]; then
    ok "I2 latest/ 双 manifest 零 riscv（不宣发未支持架构）"
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

# ============================================================
# 组 J · 9.1/U-01 更新器落后指针静默修复
# ============================================================
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

# ============================================================
# 组 K · 发布侧白名单契约（可选，本地可达才断言）
# ============================================================
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

# ============================================================
# 汇总
# ============================================================
printf '\n门禁汇总: PASS=%d FAIL=%d\n' "$PASS" "$FAIL"
if [ "$FAIL" -gt 0 ]; then
    printf '  [GATE] WS-9 社区六类问题修复发布门禁未通过（方案 §4.9 / 9.8）\n'
    exit 1
fi
printf '  [GATE] WS-9 发布门禁全绿（9.1~9.11 判据）\n'
exit 0
