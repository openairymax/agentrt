#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
#
# 0 字节日志门禁（A-5 gate:runtime）：干净 AIRY_HOME 首启后，
# 整个 sandbox 内 `find -type f -size 0` 命中数必须为 0。
#
# 判据（run 子命令）：
#   1. 静默 daemon（/bin/sleep）被真实派生（supervisor 日志有 spawn 行），
#      且不得留下 logs/<name>.out；
#   2. 有输出 daemon（/bin/echo gate-output）的 .out 存在、非空、内容匹配；
#   3. sandbox 内 0 字节普通文件数为 0（白名单为空——出现即审计）。
#
# selftest 子命令是反例注入：同一场景加 AIRY_PREOPEN_OUT=1（旧预开行为），
# 必须检出静默 daemon 的 0 字节 .out；检不出说明门禁本身失效（fail-closed）。
#
# 用法：scripts/gate_no_empty_out.sh [run|selftest] [--bin <supervisor_d>] [--keep]

set -uo pipefail

ROOT="$(cd -P "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODE="run"
BIN=""
KEEP=0
while [ $# -gt 0 ]; do
    case "$1" in
        run|selftest) MODE="$1"; shift ;;
        --bin) BIN="$2"; shift 2 ;;
        --keep) KEEP=1; shift ;;
        *) printf 'gate_no_empty_out: 未知参数 %s\n' "$1" >&2; exit 2 ;;
    esac
done

if [ -z "$BIN" ]; then
    for _c in "$ROOT/build/bin/supervisor_d" "$ROOT/bin/supervisor_d" \
              "$ROOT/build/daemons/supervisor_d/supervisor_d"; do
        [ -x "$_c" ] && BIN="$_c" && break
    done
fi
if [ ! -x "$BIN" ]; then
    printf 'SKIP gate:no-empty-out 缺 supervisor_d（--bin 指定构建产物）\n' >&2
    exit 2
fi

SB="$(mktemp -d "${TMPDIR:-/tmp}/airy-emptyout.XXXXXX")"
cleanup() {
    if [ "$KEEP" = "1" ]; then
        printf 'gate_no_empty_out: 保留沙箱 %s\n' "$SB" >&2
    else
        rm -rf "$SB"
    fi
}
trap cleanup EXIT INT TERM

fail() { printf 'FAIL gate:no-empty-out %s\n' "$*" >&2; exit 1; }
ok() { printf 'PASS gate:no-empty-out %s\n' "$*"; }

HOME_DIR="$SB/home"
mkdir -p "$HOME_DIR/bin" "$HOME_DIR/config" "$HOME_DIR/run" "$HOME_DIR/logs"
# 声明：静默者留活口（/bin/sleep 30），出声者写一行即退（CORE 退避复活无碍）
cat >"$HOME_DIR/config/profile.env" <<'EOF'
AIRY_PROFILE=gate
AIRYRT_LAUNCH_CORE="gate_silent_d gate_echo_d"
AIRYRT_LAUNCH_BIN_gate_silent_d=/bin/sleep
AIRYRT_LAUNCH_ARGS_gate_silent_d=30
AIRYRT_LAUNCH_BIN_gate_echo_d=/bin/echo
AIRYRT_LAUNCH_ARGS_gate_echo_d=gate-output
EOF

export AIRY_HOME="$HOME_DIR"
export AIRY_RUNTIME_DIR="$SB/run"
export AIRYRT_SUP_TICK_MS=200
export AIRYRT_SUP_BACKOFF_BASE_MS=200
export AIRYRT_ADMIT_CMD="/nonexistent/airy-rt-cap"
if [ "$MODE" = "selftest" ]; then
    export AIRY_PREOPEN_OUT=1
else
    unset AIRY_PREOPEN_OUT
fi

"$BIN" >"$SB/sup.log" 2>&1 &
SUP_PID=$!

wait_for() { # wait_for <描述> <秒> <命令...>
    _wf_desc="$1"
    _wf_secs="$2"
    shift 2
    _wf_i=0
    while [ "$_wf_i" -lt $((_wf_secs * 20)) ]; do
        "$@" >/dev/null 2>&1 && return 0
        kill -0 "$SUP_PID" 2>/dev/null || return 1
        sleep 0.05
        _wf_i=$((_wf_i + 1))
    done
    printf 'gate_no_empty_out: 等待超时: %s\n' "$_wf_desc" >&2
    return 1
}

has_spawn() { grep -q "spawn $1 pid=" "$SB/sup.log"; }
echo_ready() {
    [ -s "$HOME_DIR/logs/gate_echo_d.out" ] &&
        grep -q 'gate-output' "$HOME_DIR/logs/gate_echo_d.out"
}
silent_file() { [ -e "$HOME_DIR/logs/gate_silent_d.out" ]; }

wait_for "supervisor 派生 gate_silent_d" 5 has_spawn gate_silent_d ||
    fail "静默 daemon 未被派生（无法证明判据有效）"
wait_for "gate_echo_d 输出落盘" 5 echo_ready ||
    fail "有输出 daemon 的 .out 未在 5s 内出现非空内容"

kill -TERM "$SUP_PID" 2>/dev/null || true
wait "$SUP_PID" 2>/dev/null || true

if [ "$MODE" = "selftest" ]; then
    # 反例：旧预开行为下静默 daemon 必须留下 0 字节 .out
    silent_file || fail "反例未被检出：AIRY_PREOPEN_OUT=1 下也无 .out（门禁判据失效）"
    [ -s "$HOME_DIR/logs/gate_silent_d.out" ] &&
        fail "反例 .out 非空（预期 0 字节）"
    ok "selftest 反例已判红（预开 .out=0 字节可检出）"
    exit 0
fi

silent_file && fail "静默 daemon 留下 logs/gate_silent_d.out（应为不存在）"
grep -q 'gate-output' "$HOME_DIR/logs/gate_echo_d.out" ||
    fail "有输出 daemon 的 .out 内容不符"

EMPTY_LIST="$(find "$HOME_DIR" -type f -size 0 -print 2>/dev/null)"
if [ -n "$EMPTY_LIST" ]; then
    printf '%s\n' "$EMPTY_LIST" | sed 's/^/  /' >&2
    fail "干净 AIRY_HOME 内仍有 0 字节文件（白名单为空）"
fi

ok "silent=无文件 echo=非空 empty=0"
