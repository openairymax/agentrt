#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
# =============================================================================
# lib_tasks.sh — L-8「端到端任务闭环回归」公共库
#
# 职责：mktemp -d 沙箱（$AIRY_HOME）内的配置渲染、九守护进程栈启停、mock LLM
# 控制、gateway JSON-RPC 封装、断言/证据落盘、包管理器适配器与用例通用核对。
#
# 约定：
#   * 所有运行期写入限定在 $AIRY_HOME；dpkg 使用私有 admindir/instdir 根，
#     不触碰宿主 /var/lib/dpkg 与 /usr；
#   * 断言失败不中断（累计计数），由 run_tasks.sh 汇总退出码；
#   * 每个用例的原始 JSON 证据落在 $AIRY_HOME/evidence/。
# =============================================================================
set -u
# 继承宿主 trae 运行时的 libselinux 会让 dpkg/find 报版本告警并改变行为。
unset LD_LIBRARY_PATH LD_PRELOAD 2>/dev/null || true

L8_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
L8_ROOT="$(cd "$L8_DIR/../../.." && pwd)"
FX="$L8_DIR/fixtures"
BIN="${AIRY_L8_BIN:-/tmp/rt21/bin}"
CLI="${AIRY_L8_CLI:-/tmp/rt21/tools/airy_cli/airy_cli}"
# 构建产物路径在入口即解析为绝对路径：守护进程在 $AIRY_HOME 内启动（sone 里的
# cd 是刻意的），相对路径会在 cd 后指向错误位置——CI 传入的 ../build/bin 曾因此
# 让九个守护进程全部 exec 失败、用例退化为 rc=10。此处的归一化对调用方的 cwd
# 敏感（相对路径按调用目录解释），之后无论再 cd 到哪里都稳定。
abspath() {
    case "$1" in
        /*) printf '%s' "$1" ;;
        *) printf '%s/%s' "$PWD" "$1" ;;
    esac
}
BIN="$(abspath "$BIN")"
CLI="$(abspath "$CLI")"
AIRY_HOME="${AIRY_HOME:-$(mktemp -d /tmp/airy-l8.XXXXXX)}"
export AIRY_HOME
GW_PORT="${AIRY_L8_GW_PORT:-18080}"
MOCK_PORT="${AIRY_L8_MOCK_PORT:-18135}"
GW="http://127.0.0.1:$GW_PORT"
EV="$AIRY_HOME/evidence"
LOGS="$AIRY_HOME/logs"
PKG="$AIRY_HOME/pkgrepo"
WS="$AIRY_HOME/ws"
CKPT="$AIRY_HOME/data/agentrt/state/agent_turn_ckpt.json"
mkdir -p "$EV" "$LOGS" "$PKG" "$WS" "$AIRY_HOME/run" "$AIRY_HOME/mock" "$AIRY_HOME/data"
: > "$EV/checks.log"

PASS=0
FAIL=0
SKIP=0
FAILED=()

log() { printf '%s\n' "$*"; }

ck() { # ck <0|1> <label>
    local st=FAIL
    [ "${1:-0}" = "1" ] && st=PASS
    if [ "$st" = PASS ]; then PASS=$((PASS + 1)); else FAIL=$((FAIL + 1)); FAILED+=("$2"); fi
    printf '  [%s] %s\n' "$st" "$2" | tee -a "$EV/checks.log"
}

skipc() { # 单条断言级 SKIP（不允许用 SKIP 冒充通过）
    SKIP=$((SKIP + 1))
    printf '  [SKIP] %s\n' "$1" | tee -a "$EV/checks.log"
}

pyf() { # pyf <json-file> <python-expr> [default]
    python3 - "$1" "$2" "${3-}" <<'PY'
import json, sys
default = sys.argv[3] if len(sys.argv) > 3 else ""
try:
    with open(sys.argv[1], encoding="utf-8") as f:
        d = json.load(f)
except Exception:
    print(default)
    raise SystemExit
try:
    v = eval(sys.argv[2], {"d": d, "len": len, "str": str, "int": int, "bool": bool,
                           "sum": sum, "all": all, "any": any, "isinstance": isinstance,
                           "json": json, "next": next, "sorted": sorted, "set": set})
except Exception:
    v = default
if isinstance(v, bool):
    print(1 if v else 0)
elif v is None:
    print("")
else:
    print(v)
PY
}

need() { # 环境前置检查：缺失返回 10（run_tasks 映射为 gate:runtime）
    local missing="" p
    for p in "$BIN/cupolas_d" "$BIN/tool_d" "$BIN/agent_d" "$BIN/gateway_d" \
             "$CLI" "$FX/mock_llm.py"; do
        [ -e "$p" ] || missing="$missing $p"
    done
    for p in python3 curl; do
        command -v "$p" >/dev/null 2>&1 || missing="$missing $p"
    done
    if [ -n "$missing" ]; then
        log "环境不可用，缺少:$missing"
        return 10
    fi
    return 0
}

rpc() { # rpc <method> [params-json] [out-file]
    local m="$1" p="${2:-{\}}" out="${3:-$EV/rpc.last.json}" body
    body=$(printf '{"jsonrpc":"2.0","method":"%s","params":%s,"id":1}' "$m" "$p")
    curl -sS -m "${AIRY_L8_RPC_TIMEOUT:-200}" -X POST -H 'Content-Type: application/json' \
        --data-binary "$body" "$GW" >"$out" 2>"$out.err" || true
    printf '%s' "$out"
}

wport() { # wport <port> [tries]
    local p="$1" n="${2:-60}" i=0
    while [ "$i" -lt "$n" ]; do
        if (exec 3<>"/dev/tcp/127.0.0.1/$p") 2>/dev/null; then exec 3>&- 3<&-; return 0; fi
        sleep 0.25
        i=$((i + 1))
    done
    return 1
}

cfg_gen() {
    mkdir -p "$AIRY_HOME/config/cupolas" "$AIRY_HOME/data" "$AIRY_HOME/run" \
             "$AIRY_HOME/mock" "$PKG" "$WS"
    sed "s/__PORT__/$MOCK_PORT/g" "$FX/model.yaml.in" >"$AIRY_HOME/config/model.yaml"
    printf 'MOCK_API_KEY=l8-mock-key\n' >"$AIRY_HOME/config/secrets.env"
    cp "$FX/permission_rules.yaml" "$AIRY_HOME/config/cupolas/permission_rules.yaml"
}

mset() { printf '%s' "$2" >"$AIRY_HOME/mock/$1"; }

mup() {
    mdown
    # 端口必须真正空闲后再起新 mock：残留实例会静默接管请求，让故障注入失真。
    local i=0
    while (echo >/dev/tcp/127.0.0.1/"$MOCK_PORT") 2>/dev/null; do
        sleep 0.1
        i=$((i + 1))
        [ "$i" -ge 60 ] && return 1
    done
    (cd "$AIRY_HOME" && exec env MOCK_STATE="$AIRY_HOME/mock" MOCK_PORT="$MOCK_PORT" \
        python3 "$FX/mock_llm.py") >>"$LOGS/mock.log" 2>&1 &
    echo $! >"$AIRY_HOME/run/mock.pid"
    wport "$MOCK_PORT" 60 || return 1
}

mdown() {
    local pid
    if [ -f "$AIRY_HOME/run/mock.pid" ]; then
        pid=$(cat "$AIRY_HOME/run/mock.pid" 2>/dev/null || true)
        [ -n "$pid" ] && kill "$pid" 2>/dev/null || true
        rm -f "$AIRY_HOME/run/mock.pid"
    fi
    # 清理历次运行的残留 mock（同端口不同 AIRY_HOME），避免旧状态目录接管请求。
    pkill -f "$FX/mock_llm.py" 2>/dev/null || true
}

sone() { # sone <daemon> [args...]
    local n="$1"
    shift
    # exec 让后台作业直接被守护进程接管，pid 文件才是真实 pid（否则只记 wrapper，
    # SIGKILL 会留下孤儿进程继续持有 socket 与在飞任务）。
    (cd "$AIRY_HOME" && exec env -u LD_LIBRARY_PATH -u LD_PRELOAD "$BIN/$n" "$@") \
        >>"$LOGS/$n.log" 2>&1 &
    echo $! >"$AIRY_HOME/run/$n.pid"
}

down() {
    local n pid
    mdown
    for n in gateway_d agent_d sched_d mem_d tool_d think_d llm_d notify_d cupolas_d; do
        [ -f "$AIRY_HOME/run/$n.pid" ] || continue
        pid=$(cat "$AIRY_HOME/run/$n.pid" 2>/dev/null || true)
        [ -n "$pid" ] && kill "$pid" 2>/dev/null || true
    done
    sleep 0.4
    for n in gateway_d agent_d sched_d mem_d tool_d think_d llm_d notify_d cupolas_d; do
        [ -f "$AIRY_HOME/run/$n.pid" ] || continue
        pid=$(cat "$AIRY_HOME/run/$n.pid" 2>/dev/null || true)
        [ -n "$pid" ] && kill -9 "$pid" 2>/dev/null || true
        rm -f "$AIRY_HOME/run/$n.pid"
    done
}

up() {
    cfg_gen
    export AIRY_HOME MOCK_API_KEY=l8-mock-key AIRY_THINK_ENABLED=0
    export AIRY_TOOL_SANDBOX_MODE="${AIRY_TOOL_SANDBOX_MODE:-workspace}"
    export AIRY_TOOL_SANDBOX_WORKSPACE="${AIRY_TOOL_SANDBOX_WORKSPACE:-$WS}"
    export AIRY_TOOL_SANDBOX_REQUIRE_LANDLOCK="${AIRY_TOOL_SANDBOX_REQUIRE_LANDLOCK:-0}"
    export AIRY_TOOL_SANDBOX_NET="${AIRY_TOOL_SANDBOX_NET:-1}"
    export AIRY_PROV_BRK_OPEN_MS="${AIRY_PROV_BRK_OPEN_MS:-1000}"
    mup || { log "mock LLM 启动失败"; return 1; }
    local d
    for d in cupolas_d notify_d llm_d think_d tool_d mem_d sched_d agent_d; do
        sone "$d"
    done
    sleep 1
    sone gateway_d -h 127.0.0.1 -p "$GW_PORT"
    wport "$GW_PORT" 120 || { log "gateway 端口未就绪"; return 1; }
    local i=0
    while [ "$i" -lt 80 ]; do
        rpc tool.jobs '{}' "$EV/boot-jobs.json" >/dev/null
        if [ "$(pyf "$EV/boot-jobs.json" 'int(isinstance(d.get("result"), dict))')" = "1" ]; then
            rpc agent.turns '{}' "$EV/boot-turns.json" >/dev/null
            [ "$(pyf "$EV/boot-turns.json" 'int(isinstance(d.get("result"), dict))')" = "1" ] && return 0
        fi
        sleep 0.25
        i=$((i + 1))
    done
    log "守护进程 RPC 未就绪（见 $LOGS）"
    return 1
}

rstart() { # 模拟守护重启：SIGKILL agent_d 后重新拉起并等待 RPC 可用
    local pid i=0
    pid=$(cat "$AIRY_HOME/run/agent_d.pid" 2>/dev/null || true)
    [ -n "${pid:-}" ] && kill -9 "$pid" 2>/dev/null || true
    while [ -n "${pid:-}" ] && kill -0 "$pid" 2>/dev/null; do
        sleep 0.1
        i=$((i + 1))
        [ "$i" -ge 50 ] && break
    done
    rm -f "$AIRY_HOME/run/agent.sock" # 旧实例残留监听文件不得被新实例继承
    i=0
    sone agent_d
    while [ "$i" -lt 60 ]; do
        rpc agent.turns '{}' "$EV/turns.json" >/dev/null
        [ "$(pyf "$EV/turns.json" 'int(isinstance(d.get("result"), dict))')" = "1" ] && return 0
        sleep 0.25
        i=$((i + 1))
    done
    return 1
}

crit() { # crit <cmd> [cmd...] -> success_criteria JSON
    python3 -c 'import json,sys;print(json.dumps([{"type":"cmd","cmd":a} for a in sys.argv[1:]]))' "$@"
}

submit() { # submit <out> <goal> <criteria-json> [extra-json]
    local out="$1" goal="$2" cj="$3" ex="${4:-}"
    python3 - "$out" "$goal" "$cj" "$ex" <<'PY'
import json, sys
out, goal, cj, ex = sys.argv[1:5]
p = {"goal": goal, "mode": "task", "success_criteria": json.loads(cj),
     "gccp_confirmed": True, "client": "cli", "model": "mock-1",
     "budget": {"rounds": 6, "tokens": 200000}, "rounds": 6,
     "allow_dangerous": True}
if ex:
    p.update(json.loads(ex))
open(out + ".req", "w", encoding="utf-8").write(json.dumps(p, ensure_ascii=False))
PY
    rpc task.submit "$(cat "$out.req")" "$out" >/dev/null
}

board() { rpc hall.board '{}' "${1:-$EV/board.json}" >/dev/null; }

turn_tok() { # turn_tok <state>：从检查点只读出口取该状态的续跑令牌
    rpc agent.turns '{}' "$EV/turns.json" >/dev/null
    pyf "$EV/turns.json" "next((t.get('resume_token','') for t in reversed(d.get('result',{}).get('turns',[])) if t.get('state')=='$1'), '')"
}

turn_state() { # turn_state：最新检查点条目状态（无条目为空）
    rpc agent.turns '{}' "$EV/turns.json" >/dev/null
    pyf "$EV/turns.json" "next((t.get('state','') for t in reversed(d.get('result',{}).get('turns',[]))), '')"
}

submit_resume() { # submit_resume <out> <token> <criteria-json> <goal>
    local out="$1" tok="$2" cj="$3" goal="$4"
    python3 - "$out" "$tok" "$cj" "$goal" <<'PY'
import json, sys
out, tok, cj, goal = sys.argv[1:5]
p = {"goal": goal, "resume_token": tok, "mode": "task",
     "success_criteria": json.loads(cj),
     "gccp_confirmed": True, "client": "cli", "model": "mock-1",
     "budget": {"rounds": 6, "tokens": 200000}, "rounds": 6,
     "allow_dangerous": True}
open(out + ".req", "w", encoding="utf-8").write(json.dumps(p, ensure_ascii=False))
PY
    rpc task.submit "$(cat "$out.req")" "$out" >/dev/null
}

replay() { # replay <task-id> -> evidence path
    local f="$EV/replay-$1.json"
    rpc hall.replay "{\"task_id\":\"$1\",\"execution_id\":\"$1\"}" "$f" >/dev/null
    printf '%s' "$f"
}

jshow() { (cd "$AIRY_HOME" && AIRY_GATEWAY_URL="$GW" timeout 40 "$CLI" -p "/jobs show $1" 2>&1); }

tidnew() { # 依据 board 差集取本次新增 task_id（失败帧不带 task_id）
    # $1 必须是提交前已落盘的 board 快照；此处只取当前板，不能覆盖快照。
    board "$EV/board.now.json"
    python3 - "$1" "$EV/board.now.json" <<'PY'
import json, sys
def ids(p):
    try:
        with open(p, encoding="utf-8") as f:
            d = json.load(f)
    except Exception:
        return []
    r = d.get("result") or {}
    return [e.get("execution_id", "") for e in r.get("entries", []) if e.get("execution_id", "").startswith("task-")]
before = set(ids(sys.argv[1]))
now = ids(sys.argv[2])
new = [x for x in now if x and x not in before]
print(new[-1] if new else "")
PY
}

evsave() { cp "$1" "$EV/$2" 2>/dev/null || true; }

chk_case() { # <case> <resp> <want-status> <want-board> <verdict> <board-before> <t0-ms>
    local name="$1" resp="$2" want="$3" bwant="$4" verdict="$5" before="$6" t0="$7"
    local tid st plan rf pu ru rl tok vt vok jf shell doneN jid jout bst evc
    evsave "$resp" "resp-$name.json"
    tid=$(pyf "$resp" 'd.get("result",{}).get("task_id","")')
    [ -n "$tid" ] || tid=$(tidnew "$before")
    if printf '%s' "$tid" | grep -Eq '^task-[0-9]+-[0-9]+$'; then
        ck 1 "task_id 存在且格式合法: $tid"
    else
        ck 0 "task_id 存在且格式合法: '${tid:-<empty>}'"
    fi

    rf=$(replay "$tid")
    L8_LAST_TID="$tid" # 供用例在 chk_case 之后追加断言（replay 证据按 task_id 落盘）
    plan=$(pyf "$resp" 'd.get("result",{}).get("plan_id","")')
    if [ -z "$plan" ]; then
        plan=$(pyf "$rf" 'next((e.get("content",{}).get("plan_id","") for e in d.get("result",{}).get("events",[]) if e.get("content",{}).get("plan_id")), "")')
    fi
    pu=$(pyf "$rf" 'sum(1 for e in d.get("result",{}).get("events",[]) if "plan_unavailable" in json.dumps(e, ensure_ascii=False))')
    if printf '%s' "$plan" | grep -Eq '^plan-[0-9]+-[0-9]+$' || [ "${pu:-0}" -ge 1 ]; then
        ck 1 "plan_id 合法或显式 plan_unavailable（plan=${plan:-none}, pu=$pu）"
    else
        ck 0 "plan_id 合法或显式 plan_unavailable（plan=${plan:-none}, pu=$pu）"
    fi

    st=$(pyf "$resp" 'd.get("result",{}).get("status","")')
    if [ -z "$st" ]; then
        board "$EV/board-$name.json"
        st=$(pyf "$EV/board-$name.json" "next((e.get('state','') for e in d.get('result',{}).get('entries',[]) if e.get('execution_id')=='$tid'), '')")
    fi
    if [ "$want" = "any" ] || [ "$st" = "$want" ]; then
        ck 1 "终态符合预期: $st"
    else
        ck 0 "终态符合预期: want=$want got=${st:-<empty>}"
    fi

    rpc tool.jobs '{}' "$EV/jobs-$name.json" >/dev/null
    shell=$(pyf "$EV/jobs-$name.json" "sum(1 for j in d.get('result',{}).get('jobs',[]) if j.get('tool_id')=='shell_run' and j.get('created_ms',0)>=$t0)")
    if [ "${shell:-0}" -ge 1 ]; then
        ck 1 "≥1 次真实工具调用（jobs shell_run=$shell）"
    else
        ck 0 "≥1 次真实工具调用（jobs shell_run=${shell:-0}）"
    fi
    doneN=$(pyf "$EV/jobs-$name.json" "sum(1 for j in d.get('result',{}).get('jobs',[]) if j.get('state')=='done' and j.get('created_ms',0)>=$t0)")
    if [ "$want" = "completed" ]; then
        [ "${doneN:-0}" -ge 1 ] && ck 1 "jobs 中存在 done 终态作业（$doneN）" || ck 0 "jobs 中存在 done 终态作业（${doneN:-0}）"
    fi

    jid=$(pyf "$EV/jobs-$name.json" "next((j.get('job_id') for j in d.get('result',{}).get('jobs',[]) if j.get('tool_id')=='shell_run' and j.get('created_ms',0)>=$t0), '')")
    if [ -n "$jid" ]; then
        jout=$(jshow "$jid")
        printf '%s\n' "$jout" >"$EV/jobs-show-$name.txt"
        # /jobs show 非 TTY 模式不打印横幅里的 job_id，故以「工具名 + 终态 +
        # 退出/错误列」判定时间线可还原（这些列只来自 job 账本本身）。
        if printf '%s' "$jout" | grep -q "shell_run" &&
            printf '%s' "$jout" | grep -Eqw "done|failed" &&
            printf '%s' "$jout" | grep -Eq "exit|err"; then
            ck 1 "/jobs show 可还原时间线（$jid）"
        else
            ck 0 "/jobs show 可还原时间线（$jid）"
        fi
    else
        ck 0 "/jobs show 可还原时间线（无 job_id）"
    fi

    vt=$(pyf "$resp" 'd.get("result",{}).get("verification",{}).get("total",0)')
    vok=$(pyf "$resp" 'int(bool(d.get("result",{}).get("verification",{}).get("ok")))')
    case "$verdict" in
        pass)
            ck "$vok" "verifier 判通过（ok=$vok, total=$vt）"
            ck "$(pyf "$resp" 'int(bool(d.get("result",{}).get("verification",{}).get("items")) and all(i.get("pass") is True for i in d.get("result",{}).get("verification",{}).get("items",[])))')" "verifier 逐条结果为 pass"
            ck "$(pyf "$resp" 'int(all(k in i for k in ("type","expect","actual","pass") for i in d.get("result",{}).get("verification",{}).get("items",[])))')" "verifier 逐条含 type/expect/actual/pass"
            ;;
        fail)
            ck "$(pyf "$resp" 'int(d.get("result",{}).get("verification",{}).get("ok") is not True)')" "verifier 未判通过（ok=$vok, total=$vt）"
            ;;
    esac
    if [ "$st" = "completed" ]; then
        ck "$vok" "completed 必须 verifier.ok（拒收 succeeded-but-unverified）"
    fi

    ru=$(pyf "$resp" 'd.get("result",{}).get("rounds_used",-1)')
    rl=$(pyf "$resp" 'd.get("result",{}).get("rounds_limit",-1)')
    if [ "$ru" = "-1" ] || [ -z "$ru" ]; then
        ru=$(pyf "$resp" 'd.get("error",{}).get("data",{}).get("budget_used",0)')
        rl=$(pyf "$resp" 'd.get("error",{}).get("data",{}).get("budget_total",0)')
    fi
    if [ "${ru:-0}" -le "${rl:-0}" ] && [ "${rl:-0}" -ge 1 ]; then
        ck 1 "轮次预算不越界（$ru/$rl）"
    else
        ck 0 "轮次预算不越界（${ru:-?}/${rl:-?}）"
    fi
    tok=$(pyf "$resp" 'd.get("result",{}).get("usage",{}).get("total",0)')
    if [ "${tok:-0}" -le 200000 ]; then
        ck 1 "token 预算不越界（$tok<=200000）"
    else
        ck 0 "token 预算不越界（$tok>200000）"
    fi

    board "$EV/board-$name.json"
    bst=$(pyf "$EV/board-$name.json" "next((e.get('state','') for e in d.get('result',{}).get('entries',[]) if e.get('execution_id')=='$tid'), '')")
    if [ -n "$bst" ] && { [ "$bwant" = "any" ] || [ "$bst" = "$bwant" ]; }; then
        ck 1 "hall.board 出现该 task_id 且状态为 $bst"
    else
        ck 0 "hall.board 出现该 task_id 且状态为 $bwant（实际: ${bst:-<missing>}）"
    fi

    evc=$(pyf "$rf" '",".join(sorted({e.get("category","") for e in d.get("result",{}).get("events",[])}))')
    local c
    for c in progress result issue; do
        if printf '%s' "$evc" | grep -qw "$c"; then
            ck 1 "hall.replay 含 $c 事件"
        else
            ck 0 "hall.replay 含 $c 事件（实际: ${evc:-<none>}）"
        fi
    done
}

td_gate() { # TUI 执行面静态门禁（判据①：零 agent.run），证据落 $EV/td-gate.log
    # TUI 位置随布局而变：门禁取料（lf-gate-fixture）把它物化在仓内
    # agent-workload/sdk/tui，本地伞仓则与仓同级（../sdk/tui）。只认同级
    # 会让 CI 侧取到空树、TD1 以「环境不可用」判红——候选集合与
    # verify_release_gates.sh 的 TUI 解析同源。
    local tui gate="$L8_ROOT/scripts/verify_release_gates.sh"
    tui="$L8_ROOT/agent-workload/sdk/tui"
    [ -d "$tui" ] || tui="$L8_ROOT/../sdk/tui"
    if [ ! -d "$tui/src" ] || [ ! -f "$gate" ]; then
        skipc "TUI 源树或门禁脚本缺失，TD1 静态证据不可得（$tui）"
        return
    fi
    (cd "$L8_ROOT" && env -u LD_LIBRARY_PATH -u LD_PRELOAD AIRY_GATE_TUI_SRC="$tui" \
        timeout 300 bash "$gate") >"$EV/td-gate.log" 2>&1 || true
    if grep -q '\[PASS\] TD1 ' "$EV/td-gate.log"; then
        ck 1 "TUI 静态门禁 TD1：零 agent.run（verify_release_gates.sh）"
    else
        ck 0 "TUI 静态门禁 TD1：未通过或未取证（见 $EV/td-gate.log）"
    fi
}

summary() { # 汇总检查项：0 无失败；1 存在失败（SKIP 由 run_tasks 按档位处理）
    echo "L-8 summary: PASS=$PASS FAIL=$FAIL SKIP=$SKIP total=$((PASS + FAIL + SKIP))"
    [ "$FAIL" -eq 0 ]
}
