#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
#
# 配置热重载门禁（A-6-06）：把「SIGHUP 生效」从口头承诺变成可判定事实。
#
# 判据三条，缺一即失败：
#   1. 重载不换进程：SIGHUP 前后 pid 不变（热重载不是重启）；
#   2. 重载真执行：日志出现新的 `reload done gen=<n>`，且 gen 严格递增；
#   3. 重载真生效：生效载体（provider 数）随配置变化，而非只打印日志。
#
# 用法：
#   scripts/gate_reload.sh [--bin-dir <dir>]
# 默认二进制目录：$AIRY_BIN_DIR，否则 <repo>/build/bin。
# 仅限 POSIX（SIGHUP 语义）；Windows 由 CI 的 linux 作业承担，不在本脚本伪装。

set -uo pipefail

ROOT="$(cd -P "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${AIRY_BIN_DIR:-$ROOT/build/bin}"
while [ $# -gt 0 ]; do
    case "$1" in
        --bin-dir) BIN="$2"; shift 2 ;;
        *) printf 'gate_reload: 未知参数 %s\n' "$1" >&2; exit 2 ;;
    esac
done

LLM="$BIN/llm_d"
[ -x "$LLM" ] || { printf 'gate_reload: 缺 llm_d（%s）\n' "$LLM" >&2; exit 2; }

SB="$(mktemp -d "${TMPDIR:-/tmp}/airy-reload.XXXXXX")"
PID=""
cleanup() {
    [ -n "$PID" ] && kill "$PID" 2>/dev/null
    wait "$PID" 2>/dev/null
    rm -rf "$SB"
}
trap cleanup EXIT INT TERM

_HOME="$SB/home"
mkdir -p "$_HOME/config" "$_HOME/run" "$_HOME/data"
CFG="$_HOME/config/model.yaml"
OUT="$SB/llm.out"

cfg_write() {
    {
        printf 'models:\n'
        printf '  - name: Probe1\n    mode: api\n    api_format: chat\n'
        printf '    base_url: http://127.0.0.1:9\n    model_id: gate-model-1\n'
        if [ "$1" -ge 2 ]; then
            printf '  - name: Probe2\n    mode: api\n    api_format: chat\n'
            printf '    base_url: http://127.0.0.1:9\n    model_id: gate-model-2\n'
        fi
    } >"$CFG"
}

wait_line() { # <pattern> <timeout_s>
    local i=0
    while [ "$i" -lt "$(($2 * 10))" ]; do
        grep -q "$1" "$OUT" 2>/dev/null && return 0
        kill -0 "$PID" 2>/dev/null || return 1
        sleep 0.1
        i=$((i + 1))
    done
    return 1
}

gen_of() { grep -o 'reload done gen=[0-9]*' "$OUT" 2>/dev/null | sed 's/.*=//' | tail -1; }
cnt() { grep -c 'reload done gen=' "$OUT" 2>/dev/null || true; }
wait_cnt() { # <min_count> <timeout_s>
    local i=0 n
    while [ "$i" -lt "$(($2 * 10))" ]; do
        n="$(cnt)"
        case "${n:-}" in ''|*[!0-9]*) n=0 ;; esac
        [ "$n" -ge "$1" ] && return 0
        kill -0 "$PID" 2>/dev/null || return 1
        sleep 0.1
        i=$((i + 1))
    done
    return 1
}

fail() { printf 'FAIL gate:reload %s\n' "$*" >&2; exit 1; }

cfg_write 1
AIRY_HOME="$_HOME" "$LLM" --manager "$CFG" >"$OUT" 2>&1 &
PID=$!

wait_line 'LLM service started' 15 || fail "llm_d 未在 15s 内就绪（pid=$PID）"
_OLD_PID="$PID"

kill -HUP "$PID" || fail "SIGHUP 发送失败"
wait_cnt 1 15 || fail "SIGHUP 后无 reload done（热重载未执行）"
kill -0 "$PID" 2>/dev/null || fail "重载过程中进程退出（pid 变化即失败）"
[ "$PID" = "$_OLD_PID" ] || fail "pid 由 $_OLD_PID 变为 $PID（重载退化成重启）"
_G1="$(gen_of)"

cfg_write 2
kill -HUP "$PID" || fail "第二次 SIGHUP 发送失败"
wait_cnt 2 15 || fail "第二次重载未执行"
_G2="$(gen_of)"
[ "${_G2:-0}" -gt "${_G1:-0}" ] || fail "gen 未递增（${_G1:-?} → ${_G2:-?}）"
grep -q "reload done gen=${_G2} .*providers=2" "$OUT" ||
    fail "gen=${_G2} 但 provider 数未变为 2（重载未生效，仅打印日志）"

printf 'PASS gate:reload pid=%s gen=%s..%s providers=1..2\n' "$_OLD_PID" "$_G1" "$_G2"
