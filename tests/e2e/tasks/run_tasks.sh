#!/bin/bash
# SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
# =============================================================================
# run_tasks.sh — L-8「端到端任务闭环回归」入口
#
# 用法：
#   run_tasks.sh --profile ci [--shard 1|2|3]   # 五类故障注入，按 shard 轮转
#   run_tasks.sh --profile runtime             # 全量七例（含真实包安装）
#
# 排程：
#   * ci 档：schema / unknown / netbreak / deny / restart 五例按 shard 轮转，
#     每个 shard 跑约 1/3（2/2/1），CI 档不允许 SKIP（SKIP 计为失败）；
#   * runtime 档：五类故障注入 + firefox / tencent-meeting 两条真实安装基线
#     （安装按包管理器适配器走 dnf 离线源或 dpkg 私有根，缺前置显式 SKIP）。
#
# 退出码：0 全绿（runtime 允许显式 SKIP，但计入 SKIP 计数）；
#         1 存在失败（含 CI 档出现 SKIP）；
#         10 环境不可用（打印 gate:runtime 与手工步骤）。
#
# 证据：每个用例在 $AIRY_HOME/cases/<case>/evidence 落原始 JSON；
#       汇总写 $AIRY_HOME/evidence/summary.txt，TUI 静态门禁写 td-gate.log。
# =============================================================================
set -u
DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=./lib_tasks.sh
. "$DIR/lib_tasks.sh"

PROFILE=""
SHARD=1

usage() {
    echo "usage: run_tasks.sh --profile <ci|runtime> [--shard <1|2|3>]"
}

while [ $# -gt 0 ]; do
    case "$1" in
        --profile) PROFILE=${2:?missing profile}; shift 2 ;;
        --shard) SHARD=${2:?missing shard}; shift 2 ;;
        -h | --help) usage; exit 0 ;;
        *) echo "unknown argument: $1" >&2; usage; exit 2 ;;
    esac
done
case "$PROFILE" in
    ci | runtime) ;;
    *) usage; exit 2 ;;
esac
case "$SHARD" in
    1 | 2 | 3) ;;
    *) echo "shard must be 1|2|3" >&2; exit 2 ;;
esac

need || {
    echo "gate:runtime — 环境不可用，L-8 无法执行。"
    cat <<'EOF'
手工步骤（gate:runtime）：
  1) cmake -S . -B /tmp/rt21 && cmake --build /tmp/rt21 -j"$(nproc)"
  2) 确认 /tmp/rt21/bin/{cupolas_d,notify_d,llm_d,think_d,tool_d,mem_d,sched_d,agent_d,gateway_d}
     与 /tmp/rt21/tools/airy_cli/airy_cli 存在；
  3) 可经 AIRY_L8_BIN / AIRY_L8_CLI 覆盖构建产物路径；
  4) 重跑: bash tests/e2e/tasks/run_tasks.sh --profile runtime
EOF
    exit 10
}

FAULTS=(schema unknown netbreak deny restart)
INSTS=(firefox tencent-meeting)
SEL=()
if [ "$PROFILE" = ci ]; then
    for i in "${!FAULTS[@]}"; do
        [ $((i % 3)) -eq $((SHARD - 1)) ] && SEL+=("${FAULTS[$i]}")
    done
else
    SEL=("${FAULTS[@]}" "${INSTS[@]}")
fi

script_of() { # script_of <case>：安装基线走 case_install.sh，其余走 case_faults.sh
    case "$1" in
        firefox | tencent-meeting) echo case_install.sh ;;
        *) echo case_faults.sh ;;
    esac
}

TP=0
TF=0
TS=0
BAD=0
ENV_RC=0
LINES=()

run_one() { # run_one <case>：子进程独立沙箱，证据与日志各自落盘
    local c=$1 scr h log rc line p f s
    scr=$(script_of "$c")
    h="$AIRY_HOME/cases/$c"
    log="$LOGS/$c.log"
    mkdir -p "$h"
    AIRY_HOME="$h" bash "$DIR/$scr" "$c" >"$log" 2>&1
    rc=$?
    line=$(grep -o 'L-8 summary:.*' "$log" | tail -1)
    printf '== %-16s rc=%-2s %s\n' "$c" "$rc" "${line:-<no summary>}"
    LINES+=("$c rc=$rc ${line:-<no summary>}")
    p=$(printf '%s' "$line" | sed -n 's/.*PASS=\([0-9]*\).*/\1/p')
    f=$(printf '%s' "$line" | sed -n 's/.*FAIL=\([0-9]*\).*/\1/p')
    s=$(printf '%s' "$line" | sed -n 's/.*SKIP=\([0-9]*\).*/\1/p')
    TP=$((TP + ${p:-0}))
    TF=$((TF + ${f:-0}))
    TS=$((TS + ${s:-0}))
    [ "$rc" -eq 10 ] && ENV_RC=10
    [ "$rc" -ne 0 ] && BAD=$((BAD + 1))
}

echo "L-8 run: profile=$PROFILE shard=$SHARD cases=${SEL[*]}"
td_gate
for c in "${SEL[@]}"; do
    run_one "$c"
done

if [ "$PROFILE" = ci ] && [ "$TS" -gt 0 ]; then
    echo "CI: 故障注入档不允许 SKIP（SKIP=$TS 计为失败）"
    BAD=$((BAD + 1))
fi

echo "L-8 run_tasks: profile=$PROFILE shard=$SHARD cases=${#SEL[@]} PASS=$TP FAIL=$TF SKIP=$TS"
{
    echo "profile=$PROFILE shard=$SHARD cases=${SEL[*]}"
    printf '%s\n' "${LINES[@]}"
    echo "PASS=$TP FAIL=$TF SKIP=$TS"
} >"$EV/summary.txt"
echo "evidence: $AIRY_HOME/evidence（各用例: $AIRY_HOME/cases/<case>/evidence）"

[ "$ENV_RC" -ne 0 ] && {
    echo "gate:runtime — 有用例因环境不可用中止（rc=10），见上方各用例日志；"
    echo "修复构建/前置后重跑: bash tests/e2e/tasks/run_tasks.sh --profile runtime"
    exit 10
}
[ "$BAD" -ne 0 ] && exit 1
exit 0
