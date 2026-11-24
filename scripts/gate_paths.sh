#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
#
# 路径一致性门禁（A-6-07）：启动器、CLI、doctor 三者必须取同一套路径。
#
# 判据四条，缺一即失败：
#   1. 形状：`airy_cli paths --json` 单行、十键齐全、值非空；
#   2. 派生：显式 AIRY_HOME 下 log/config/run 落在机制规则位置
#      （data/agentrt/logs、config、run），证明非脚本自拼；
#   3. 同源：doctor 报告的日志根与 paths 快照的 log 逐字相等（差集为空）；
#   4. 静态：启动器保留 `_pget log` 取值链与同款兜底，不得回退硬编码。
#
# 用法：scripts/gate_paths.sh [--cli <airy_cli>]

set -uo pipefail

ROOT="$(cd -P "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CLI="${AIRY_CLI_BIN:-$ROOT/build/bin/airy_cli}"
while [ $# -gt 0 ]; do
    case "$1" in
        --cli) CLI="$2"; shift 2 ;;
        *) printf 'gate_paths: 未知参数 %s\n' "$1" >&2; exit 2 ;;
    esac
done

for _c in "$CLI" "$ROOT/build/tools/airy_cli/airy_cli" "$ROOT/tools/airy_cli/airy_cli"; do
    [ -x "$_c" ] && CLI="$_c" && break
done
[ -x "$CLI" ] || { printf 'gate_paths: 缺 airy_cli（%s）\n' "$CLI" >&2; exit 2; }
# 绝对化：下方把 CLI 以符号链接放进沙箱 bin/，相对路径的链接目标会按链接
# 所在目录解析而悬空，doctor 随即看不到 CLI、退化为兜底路径，判据误红。
CLI="$(cd -P "$(dirname "$CLI")" && pwd)/$(basename "$CLI")"
[ -x "$CLI" ] || { printf 'gate_paths: airy_cli 不可执行（%s）\n' "$CLI" >&2; exit 2; }

LAUNCH="$ROOT/latest/airymaxrt"
[ -f "$LAUNCH" ] || { printf 'gate_paths: 缺启动器 %s\n' "$LAUNCH" >&2; exit 2; }

SB="$(mktemp -d "${TMPDIR:-/tmp}/airy-paths.XXXXXX")"
trap 'rm -rf "$SB"' EXIT INT TERM

fail() { printf 'FAIL gate:paths %s\n' "$*" >&2; exit 1; }
ok() { printf 'PASS gate:paths %s\n' "$*"; }
jget() { printf '%s' "$JS" | sed -n "s/.*\"$1\":\"\([^\"]*\)\".*/\1/p"; }

_BASE="$SB/home"
mkdir -p "$_BASE/bin" "$_BASE/config" "$_BASE/run"
ln -sf "$CLI" "$_BASE/bin/airy_cli"
JS="$(AIRY_HOME="$_BASE" "$CLI" paths --json 2>/dev/null)" || fail "paths --json 执行失败"
[ "$(printf '%s' "$JS" | wc -l | tr -d ' ')" = "0" ] && [ -n "$JS" ] ||
    fail "paths --json 非单行输出（形态退化，shell 取值会断裂）"

for _k in home bin lib run log config data tmp cache workspace; do
    _v="$(jget "$_k")"
    [ -n "$_v" ] || fail "paths 缺键或空值: $_k"
done

_WANT_LOG="$_BASE/data/agentrt/logs"
[ "$(jget log)" = "$_WANT_LOG" ] || fail "log 偏离机制规则: $(jget log) != $_WANT_LOG"
[ "$(jget config)" = "$_BASE/config" ] || fail "config 偏离机制规则: $(jget config)"
[ "$(jget run)" = "$_BASE/run" ] || fail "run 偏离机制规则: $(jget run)"

_OUT="$(AIRY_HOME="$_BASE" bash "$LAUNCH" doctor 2>&1 || true)"
_ESC="$(printf '\033')"
_DLOG="$(printf '%s\n' "$_OUT" | sed "s/${_ESC}\[[0-9;]*m//g" |
    sed -n 's/.*airy_cli paths log=\([^ ]*\).*/\1/p' | head -1)"
if [ -z "$_DLOG" ]; then
    printf '%s\n' "$_OUT" | sed "s/${_ESC}\[[0-9;]*m//g" | tail -n 20 | sed 's/^/  doctor| /' >&2
    fail "doctor 未自证路径来源（路径来源行缺失）"
fi
[ "$_DLOG" = "$_WANT_LOG" ] || fail "doctor 日志根与 paths 快照不一致: $_DLOG != $_WANT_LOG"

grep -q 'LOG_DIR="\$(_pget log)"' "$LAUNCH" || fail "启动器丢失 _pget log 取值链"
grep -q 'LOG_DIR="${LOG_DIR:-$AIRY_HOME/data/agentrt/logs}"' "$LAUNCH" ||
    fail "启动器兜底规则与机制不一致"
grep -q 'RUNTIME_DIR="${RUNTIME_DIR:-${AIRY_RUNTIME_DIR:-$AIRY_HOME/run}}"' "$LAUNCH" ||
    fail "启动器运行根未尊重 AIRY_RUNTIME_DIR 契约"

ok "json10=ok derive=$_WANT_LOG doctor=同源 static=同源"
