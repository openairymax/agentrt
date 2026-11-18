#!/bin/bash
# SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
# ---------------------------------------------------------------------------
# L-8 端到端任务闭环回归：故障注入面（测试代码，不是产品功能演示）。
#
# 五类故障，逐类断言「指定终态 + 证据可判读」，禁止把 SKIP 当通过：
#   schema   工具参数 schema 违例 → failed，失败原因含字段级信息
#   unknown  模型调用不存在/目录外工具 → failed（非 SUCCESS）
#   netbreak 运行中断网（叠加 500 与空正文）→ paused/failed 且可重试续跑
#   deny     策略拒绝 → failed，failure.reason=denied
#   restart  守护重启 → paused，续跑后标记只执行一次（无重复执行）
# 判据的公共部分（task_id/plan/jobs/verifier/预算/board/replay）在
# lib_tasks.sh:chk_case 统一执行；本文件只加各类故障的专属断言。
#
# 用法: case_faults.sh <schema|unknown|netbreak|deny|restart>
# 退出码: 0 全绿；1 有 FAIL；10 环境不可用。
# ---------------------------------------------------------------------------
set -u
DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=./lib_tasks.sh
. "$DIR/lib_tasks.sh"

CASE=${1:?usage: case_faults.sh <schema|unknown|netbreak|deny|restart>}
mkdir -p "$EV" "$LOGS"
need || { echo "ENV: 前置缺失（BIN/CLI/mock/python3/curl）"; exit 10; }
up || { echo "ENV: 守护栈未就绪（见 $LOGS）"; exit 10; }
trap 'down' EXIT

msg() { pyf "$1" 'd.get("error",{}).get("message","")'; }
fd() { pyf "$1" "d.get('error',{}).get('data',{}).get('$2','')"; }
rcase() { # rcase 建初始快照并提交
    board "$EV/before-$1.json"
    date +%s%3N
}
tui_same() { # tui_same <case> <cli-tid> <criteria> <goal> <want> <board-want> <verdict> <extra>
    # 双端同源：同一 submit 内容以 client=tui 再发一次，两端 task_id 各一，
    # 事件类别、终态与失败归因必须同构（client 标签不得改变裁决）。
    local c=$1 cli=$2 cj=$3 goal=$4 ws=$5 wb=$6 vd=$7 extra=$8 t0 tui a b
    board "$EV/before-$c-tui.json"
    t0=$(date +%s%3N)
    submit "$EV/$c-tui.json" "$goal（tui 端）" "$cj" "$extra"
    chk_case "$c-tui" "$EV/$c-tui.json" "$ws" "$wb" "$vd" "$EV/before-$c-tui.json" "$t0"
    tui=$L8_LAST_TID
    ck "$([ -n "$cli" ] && [ -n "$tui" ] && [ "$cli" != "$tui" ] && echo 1 || echo 0)" \
        "$c/tui: 与 cli 各得独立 task_id（$cli / $tui）"
    a=$(pyf "$EV/replay-$cli.json" '",".join(sorted({e.get("category","") for e in d.get("result",{}).get("events",[])}))')
    b=$(pyf "$EV/replay-$tui.json" '",".join(sorted({e.get("category","") for e in d.get("result",{}).get("events",[])}))')
    ck "$([ -n "$a" ] && [ "$a" = "$b" ] && echo 1 || echo 0)" \
        "$c/tui: 事件类别同构（${a:-<none>}）"
    a=$(pyf "$EV/board-$c.json" "next((e.get('state','') for e in d.get('result',{}).get('entries',[]) if e.get('execution_id')=='$cli'), '')")
    b=$(pyf "$EV/board-$c-tui.json" "next((e.get('state','') for e in d.get('result',{}).get('entries',[]) if e.get('execution_id')=='$tui'), '')")
    ck "$([ "$a" = "$b" ] && [ -n "$a" ] && echo 1 || echo 0)" \
        "$c/tui: 终态同构（cli=$a tui=$b）"
    a=$(pyf "$EV/$c.json" 'str(d.get("error",{}).get("data",{}).get("reason",""))')
    b=$(pyf "$EV/$c-tui.json" 'str(d.get("error",{}).get("data",{}).get("reason",""))')
    ck "$([ "$a" = "$b" ] && echo 1 || echo 0)" \
        "$c/tui: 失败归因同构（$a）"
}

case_schema() { # 参数 schema 违例：失败必须带字段级原因
    mset mode schema
    mset command "printf l8-probe"
    local t0 cli
    t0=$(rcase schema)
    submit "$EV/schema.json" "安装 firefox 并验证（schema 故障注入）" "$(crit "test -f /nope")"
    chk_case schema "$EV/schema.json" failed failed fail "$EV/before-schema.json" "$t0"
    cli=$L8_LAST_TID
    ck "$(pyf "$EV/schema.json" 'int("command(string, required)" in d.get("error",{}).get("message",""))')" \
        "schema: 失败原因含字段级信息（command 为必填 string）"
    ck "$(pyf "$EV/schema.json" 'int(d.get("error",{}).get("data",{}).get("reason")=="tool_fuse")')" \
        "schema: failure.reason=tool_fuse"
    ck "$(pyf "$EV/schema.json" 'int((d.get("result") or {}).get("status")!="succeeded")')" \
        "schema: 未出现 succeeded"
    tui_same schema "$cli" "$(crit "test -f /nope")" "安装 firefox 并验证（schema 故障注入）" \
        failed failed fail '{"client":"tui"}'
}

case_unknown() { # 目录外工具：不执行、不算成功、最终 failed
    mset mode unknown
    mset command "printf l8-probe"
    local t0 cli
    t0=$(rcase unknown)
    submit "$EV/unknown.json" "安装 firefox 并验证（目录外工具故障注入）" "$(crit "test -f /nope")"
    chk_case unknown "$EV/unknown.json" failed failed fail "$EV/before-unknown.json" "$t0"
    cli=$L8_LAST_TID
    ck "$(pyf "$EV/unknown.json" 'int("not available in this run" in d.get("error",{}).get("message",""))')" \
        "unknown: 原因说明工具不在本轮目录内"
    ck "$(pyf "$EV/jobs-unknown.json" "int(sum(1 for j in d.get('result',{}).get('jobs',[]) if j.get('tool_id')=='airyl8_no_such_tool')==0)")" \
        "unknown: 目录外工具未被真实执行（jobs 无该 tool_id）"
    ck "$(pyf "$EV/unknown.json" 'int(d.get("error",{}).get("data",{}).get("reason")=="tool_fuse")')" \
        "unknown: failure.reason=tool_fuse"
    tui_same unknown "$cli" "$(crit "test -f /nope")" "安装 firefox 并验证（目录外工具故障注入）" \
        failed failed fail '{"client":"tui"}'
}

case_deny() { # 策略拒绝：高危调用未获逐次授权 → 工具被拒 → failed/approval_denied
    # shell_run 属固有高危（tool_audit_risk）；不携带 allow_dangerous 时 D-9
    # 逐次审批 fail-closed 拒绝，工具以 EPERM 失败，三轮熔断后归因 approval_denied。
    mset mode deny
    mset command "printf l8-deny"
    local t0 cli
    t0=$(rcase deny)
    submit "$EV/deny.json" "安装 firefox 并验证（策略拒绝注入）" "$(crit "test -f /nope")" \
        '{"allow_dangerous": false}'
    chk_case deny "$EV/deny.json" failed failed fail "$EV/before-deny.json" "$t0"
    cli=$L8_LAST_TID
    ck "$(pyf "$EV/deny.json" 'int(d.get("error",{}).get("data",{}).get("reason")=="approval_denied")')" \
        "deny: failure.reason=approval_denied"
    ck "$(pyf "$EV/deny.json" 'int(d.get("error",{}).get("data",{}).get("origin")=="tool")')" \
        "deny: failure.origin=tool（拒绝发生在工具审批面）"
    ck "$(pyf "$EV/jobs-deny.json" "int(any(j.get('tool_id')=='shell_run' and j.get('state')=='failed' and 'requires approval' in (j.get('error') or '') for j in d.get('result',{}).get('jobs',[]) if j.get('created_ms',0)>=$t0))")" \
        "deny: 被拒的 shell_run 作业留痕且带审批拒绝原因"
    ck "$(pyf "$EV/replay-$L8_LAST_TID.json" 'int(any("requires approval" in str(e.get("content",{})) for e in d.get("result",{}).get("events",[])))')" \
        "deny: 事件流保留拒绝原因（denied_reason）"
    tui_same deny "$cli" "$(crit "test -f /nope")" "安装 firefox 并验证（策略拒绝注入）" \
        failed failed fail '{"allow_dangerous": false, "client": "tui"}'
}

case_net() { # 断网：工具已执行后杀掉 provider → 可重试续跑
    mset mode hang # 工具完成后挂起，留出真实断网窗口
    mset command "printf l8-net"
    mset hang_after_tool 8
    local t0 bg i=0
    t0=$(rcase netbreak)
    submit "$EV/net1.json" "安装 firefox 并验证（断网注入）" "$(crit "test -f /nope")" &
    bg=$!
    while [ "$i" -lt 120 ]; do
        [ "$(turn_state)" = "running" ] && break
        sleep 0.2
        i=$((i + 1))
    done
    rpc tool.jobs '{}' "$EV/jobs-net1.json" >/dev/null # 断网前刷新审计（工具已执行）
    ck "$(pyf "$EV/jobs-net1.json" "int(sum(1 for j in d.get('result',{}).get('jobs',[]) if j.get('tool_id')=='shell_run' and j.get('created_ms',0)>=$t0)>=1)")" \
        "netbreak: 断网前已完成 ≥1 次真实工具调用"
    mdown # 真正的网络断：provider 连接被切断
    wait "$bg"
    # paused 在 API 面表现为可重试失败帧 + 板面 running；终态断言走裁决字段。
    chk_case net1 "$EV/net1.json" any running fail "$EV/before-netbreak.json" "$t0"
    ck "$(pyf "$EV/net1.json" 'int(str(d.get("error",{}).get("data",{}).get("reason","")).startswith("provider"))')" \
        "netbreak: failure.reason=provider_unreachable"
    ck "$(pyf "$EV/net1.json" 'int(d.get("error",{}).get("data",{}).get("retryable") is True)')" \
        "netbreak: failure.retryable=true（可再提交续跑）"
    turn_state >/dev/null
    ck "$(pyf "$EV/turns.json" 'int(any(t.get("resume_token") for t in d.get("result",{}).get("turns",[])))')" \
        "netbreak: 断网后检查点仍保留续跑令牌"
    # 叠加 provider 500 与空正文：均须 retryable 且不得判成功
    local m
    for m in fail500 empty; do
        mset mode "$m"
        mup || { ck 0 "netbreak: mock（$m）重启失败"; return; }
        submit "$EV/net-$m.json" "安装 firefox 并验证（$m 注入）" "$(crit "test -f /nope")"
        ck "$(pyf "$EV/net-$m.json" 'int((d.get("result") or {}).get("status")!="succeeded")')" \
            "netbreak/$m: 未判成功"
        ck "$(pyf "$EV/net-$m.json" 'int(d.get("error",{}).get("data",{}).get("retryable") is True)')" \
            "netbreak/$m: retryable=true"
    done
    mset mode tool
    mset command "echo net-ok >> $AIRY_HOME/net-marker"
    mset hang_after_tool 0
    mup || { ck 0 "netbreak: mock 恢复失败"; return; }
    t0=$(rcase net2)
    submit "$EV/net2.json" "安装 firefox 并验证（断网恢复后续跑）" "$(crit "test -f $AIRY_HOME/net-marker")"
    chk_case net2 "$EV/net2.json" completed completed pass "$EV/before-net2.json" "$t0"
}

case_restart() { # 守护重启：running→paused，凭令牌续跑且不重复执行
    mset mode hang
    mset hang_after_tool 8
    mset command "echo r1 >> $AIRY_HOME/rst-marker"
    local t0 bg i=0 tok
    t0=$(rcase restart)
    submit "$EV/rs0.json" "安装 firefox 并验证（守护重启注入）" "$(crit "test -f $AIRY_HOME/rst-marker")" &
    bg=$!
    while [ "$i" -lt 120 ]; do
        [ "$(turn_state)" = "running" ] && break
        sleep 0.2
        i=$((i + 1))
    done
    rstart || { ck 0 "restart: agent_d 重拉失败"; return; }
    wait "$bg"
    turn_state >/dev/null # 重拉后刷新检查点快照再断言
    ck "$(pyf "$EV/turns.json" 'int(any(t.get("state")=="paused" for t in d.get("result",{}).get("turns",[])))')" \
        "restart: boot 将 running 迁移为 paused"
    tok=$(turn_tok paused)
    ck "$([ -n "$tok" ] && echo 1 || echo 0)" "restart: 取得续跑令牌（非空）"
    [ -n "$tok" ] || return
    mset mode text
    mset hang_after_tool 0
    # 续跑沿用本次闭环起点 t0：真实工具调用发生在被中断那一轮，jobs 审计按用例起点取。
    board "$EV/before-rs1.json"
    submit_resume "$EV/rs1.json" "$tok" "$(crit "test -f $AIRY_HOME/rst-marker" 'test "$(wc -l < '"$AIRY_HOME"'/rst-marker)" -eq 1')" \
        "安装 firefox 并验证（守护重启注入）"
    chk_case rs1 "$EV/rs1.json" completed completed pass "$EV/before-rs1.json" "$t0"
    ck "$([ "$(wc -l <"$AIRY_HOME/rst-marker" 2>/dev/null || echo 0)" -eq 1 ] && echo 1 || echo 0)" \
        "restart: 续跑后标记仅 1 行（无重复执行）"
}

case "$CASE" in
    schema) case_schema ;;
    unknown) case_unknown ;;
    deny) case_deny ;;
    netbreak | net) case_net ;;
    restart) case_restart ;;
    *)
        echo "unknown case: $CASE" >&2
        exit 2
        ;;
esac

summary
