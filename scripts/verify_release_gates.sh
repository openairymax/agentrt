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
#   H  9.9/U-02~4 三份脚本通道白名单同集 + 保留通道文案（bash/ps1/更新器）
#                 + 版本占位合法 + 保留通道声明 manifest（state=reserved）
#                 + 消费端 state 门禁（U-02 通道选择 fail-closed）
#   I  9.10/I-03  架构白名单精确串 + 全仓零 riscv 表述 + 源码构建指引
#   J  9.1/U-01   更新器 detect_pending_release（更新问题：落后指针静默）
#   K  （可选）   发布侧 publish-release.sh 白名单契约（本地可达才断言）
#   L  §4.10 R-5  provider 默认超时 < 网关 LLM 转发背压（连不上网络：精确诊断
#                 不被网关超时吞掉；含重试最坏总耗时上界）
#   M  §4.10 R-6  PDP 双 Schema 兼容 + external 授权集 ⊇ /mcp tools/list 暴露集
#                 （工具不可用：/mcp tools/call 以 external 主体过 PEP，缺任一
#                 工具授权即 fail-closed 返回 -32603）
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

# H4（U-02 收口）：latest/ 必须为保留通道备「显式声明式 manifest」——通道选择
# fail-closed 的权威依据。断言 state/latest/channel 三字段与签名同在；旧行为
# （manifest 缺失 → 客户端 404 猜测）即回归。
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

# H5（U-02 收口）：三消费端均以 state=reserved 显式 fail-closed（不再以空
# latest 含糊归因，也不把选错通道误作「本平台无包」下沉源码构建）。
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

# K2（U-02 收口）：发布链须为 latest/ 缺失通道合成声明式 manifest，否则
# 通道全集不全，客户端 `--channel <ch>` 只能得 404 并退回「网络异常」猜测。
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

# ============================================================
# 组 L · §4.10 R-5 provider 默认超时 < 网关 LLM 转发背压
# ============================================================
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

# ============================================================
# 组 M · §4.10 R-6 工具授权面 ⊇ MCP 暴露面（双 Schema 解析防回归）
# ============================================================
# 现象："各种工具不可用"——MCP 客户端 tools/list 能列出工具，tools/call 一律
# 返回 -32603。根因：同一份 permission_rules.yaml 被两个解析器按不同 Schema
# 读取——daemon_security（daemons/common）读 {agent,tool,effect}，PDP
# （cupolas permission_rule.c）原生读 {agent,action,resource,allow}。后者
# "resource" 回退 "*"、"allow" 回退 false ⇒ 134 条规则全部 fail-closed deny。
# 本组断言：① PDP 解析器保留 ACL Schema 别名（防双 Schema 冲突回归）；
# ② 网关工具执行路径经 PEP；③ 模板 external 授权集覆盖 /mcp 暴露的全部工具。
section "M" "R-6 工具授权面 ⊇ MCP 暴露面（双 Schema 解析防回归）"

PDP_RULE_C="$ROOT/cupolas/src/permission/permission_rule.c"
MCP_BUILTIN="$ROOT/daemons/tool_d/src/service_builtin.c"
GW_BACKEND="$ROOT/gateway/src/biz/gateway_biz_backend.c"

# M1 · PDP 解析器兼容 ACL Schema（resource←tool / allow←effect）
if grep -Fq 'cupolas_permission_rule_resource' "$PDP_RULE_C" \
   && grep -Fq 'cupolas_permission_rule_allow' "$PDP_RULE_C" \
   && grep -Fq 'yaml_get(entry, "tool")' "$PDP_RULE_C" \
   && grep -Fq 'yaml_get(entry, "effect")' "$PDP_RULE_C"; then
    ok "M1 PDP 解析器兼容 ACL Schema（resource←tool / allow←effect），双 Schema 冲突已修"
else
    bad "M1 permission_rule.c 缺 tool→resource / effect→allow 别名（双 Schema 冲突回归，/mcp 工具将被 fail-closed 全拒）"
fi

# M2 · 网关工具执行路径接入 PEP（gw_acl_check_tool）
if grep -Fq 'gw_acl_check_tool' "$GW_BACKEND"; then
    ok "M2 网关工具执行路径经 PEP 判定（gw_acl_check_tool）"
else
    bad "M2 gateway 工具执行路径未接入 PEP（gw_acl_check_tool 缺失，权限判定旁路）"
fi

# M3 · /mcp 暴露工具集（tool_d service_builtin.c 的 .id 为 SSoT）
if [ ! -f "$MCP_BUILTIN" ]; then
    bad "M3 缺失 MCP 工具 SSoT: $MCP_BUILTIN"
else
    _mcp_tools="$TMP/mcp_tools.txt"
    sed -n 's/^[[:space:]]*\.id = "\([^"]*\)".*/\1/p' "$MCP_BUILTIN" > "$_mcp_tools"
    _mcp_n="$(wc -l < "$_mcp_tools" | tr -d ' ')"

    # 模板探测顺序：显式指定 → CI 取料（_tools）→ hub 本地布局（tools 并列）
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

        # M4 · external 保有网关高敏能力授权（T-11b agent.run 收口）
        _capmiss=0
        for _cap in cap:agent.run cap:agent.control; do
            grep -qxF "$_cap" "$_ext_tools" \
                || { bad "M4 external 缺高敏能力授权: $_cap"; _capmiss=1; }
        done
        [ "$_capmiss" -eq 0 ] \
            && ok "M4 external 保有 cap:agent.run / cap:agent.control（T-11b 网关高敏入口授权）"
    fi
fi

# ============================================================
# 组 N · 0.1.16 B3 南向 A-IPC 客户端面归一（AF_UNIX 白名单）
# ============================================================
# 现象：gateway 四处手搓 UDS 客户端（sse_tool/sse_stream/sse_run_stream/
# pep_cache）绕过统一客户端面，第二套传输实现与 A-IPC 漂移，L2 灰度
# 永远覆盖不了这些路径（0.1.16 A-IPC 架构收口设计 §4.3 门禁 N2）。
# 本组断言：AF_UNIX socket 创建在 gateway/src 内仅允许出现在统一
# 南向客户端面 gateway_aipc_client.c；手搓点删除后不得无声再生。
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

# ============================================================
# 组 P · 0.1.16 B4 构建收口（corekern include 面 + 客户端零内核机制）
# ============================================================
# 依据：0.1.16 A-IPC 架构收口设计 §5.3（2026-09-14 裁决③）。
# 第 2 句铁律「内核机制只被 daemon 服务面访问」的构建面判据：
#   P1 白名单断言：airy_cli 允许集不含 airy_atoms 微核心聚合；
#   P2 全局面断言：根 CMakeLists 全局 include 不含 atoms/corekern/include；
#   P3 CLI include 面断言：tools/airy_cli 构建与源树均无 corekern 引用；
#   P4 CLI 产物断言：nm 扫描零内核机制符号（airy_init/airy_shutdown/
#      airy_oom_*/airy_persist_*），有即 fail-closed。
section "P" "B4 构建收口（corekern include 面 + CLI 零内核机制）"

_whitelist="$ROOT/link-whitelist.txt"

# P1 · 白名单机器断言：客户端目标禁链 airy_atoms
_p1_line="$(grep -E '^airy_cli:' "$_whitelist" || true)"
if [ -z "$_p1_line" ]; then
    bad "P1 link-whitelist.txt 缺 airy_cli 登记（白名单被削）"
elif echo "$_p1_line" | grep -q 'airy_atoms'; then
    bad "P1 airy_cli 允许集含 airy_atoms（CLI 进程内微核心违规再生）"
else
    ok "P1 airy_cli 允许集不含 airy_atoms（进程内微核心收回固化）"
fi

# P2 · 全局面断言：corekern include 不得进入根 CMakeLists 全局注入
_p2_hits="$(grep -n 'atoms/corekern/include' "$ROOT/CMakeLists.txt" \
    | grep -v 'AIRY_COREKERN_INCLUDE_DIR\|corekern 头不再全局注入' || true)"
if [ -z "$_p2_hits" ]; then
    ok "P2 根 CMakeLists 全局面无 atoms/corekern/include 注入（B4 收口保持）"
else
    bad "P2 根 CMakeLists 全局面重现 corekern include 注入: $_p2_hits"
fi

# P3 · CLI include 面断言：源树零 corekern 头引用 + 构建文件零 corekern 路径
# 注：仅断言「真实引用」（#include 指令 / 非注释构建路径），B2 说明性注释不算违规。
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

# P4 · CLI 产物断言：零内核机制符号（airy_time_* 等无状态时钟符号豁免，
# 见 link-whitelist.txt 0.1.16 B4 头注）
#
# 产物定位：由 ctest 注入 AIRY_GATE_BUILD_DIR=<CMAKE_BINARY_DIR>（tests/
# CMakeLists.txt 的 release_gates ENVIRONMENT），于其中探测 airy_cli 两种
# 常见落点。语义对齐脚本既有 fail-closed 约定：
#   · AIRY_GATE_BUILD_DIR 显式指定但产物缺失 → FAIL（构建契约被破坏）；
#   · 未指定（本地手工直接跑脚本常态）→ SKIP，CI 侧由 ctest 注入兜底。
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

# ============================================================
# 组 Q · 0.1.16 B6 gateway 零编排（退役 SSE 编排死代码物理移除）
# ============================================================
# 依据：0.1.16 A-IPC 架构收口设计 §7（2026-09-14 改判，采甲）。
# 现象：gateway 的 /api/v1/chat/stream 自 0.1.13（B11 清零）起恒返
# 410 Gone，其后工具循环状态机（含 EXEC_TOOLS 相位）为不可达死代码，
# 与铁律第 3 句「gateway 翻译、daemon 干活」相悖。B6 将其物理移除。
# 本组断言：死模块零残留、编排常量零残留、退役路由未登记，且 live
# 端点（run_stream/hall_watch）与共享助手仍在——防无声再生。
section "Q" "B6 gateway 零编排（退役 SSE 编排死代码物理移除）"

# Q1 · 死模块与单测零残留
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

# Q2 · 编排常量与相位零残留（说明性注释豁免，同 P3 口径）
_q2_hits="$(grep -rnE 'GW_SSE_MAX_TOOL_LOOPS|GW_SSE_TOOL_LIMIT_MSG|GW_SSE_TEXT_CHUNK|GW_SSE_SUMMARY_MAX|GW_SSE_TOOL_FEEDBACK_MAX|gw_sse_phase_t|EXEC_TOOLS' \
    "$ROOT/gateway/src" --include='*.c' --include='*.h' 2>/dev/null \
    | grep -vE ':[0-9]+:[[:space:]]*(\*|/\*|//)' || true)"
if [ -z "$_q2_hits" ]; then
    ok "Q2 gateway/src 零编排常量与相位（EXEC_TOOLS 等未再生）"
else
    bad "Q2 gateway/src 重现编排常量/相位: $(echo "$_q2_hits" | tr '\n' ' ')"
fi

# Q3 · 退役路由未登记（宏与字面量双重断言）
_q3_hits="$(grep -rnE 'GW_SSE_CHAT_PATH|"/api/v1/chat/stream"' \
    "$ROOT/gateway/src" --include='*.c' --include='*.h' 2>/dev/null || true)"
if [ -z "$_q3_hits" ]; then
    ok "Q3 退役路由 /api/v1/chat/stream 未登记（宏与字面量均零残留）"
else
    bad "Q3 退役路由 /api/v1/chat/stream 再生: $(echo "$_q3_hits" | tr '\n' ' ')"
fi

# Q4 · live 端点与共享助手仍在
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
