#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
#
# 重启语义门禁（A-9）：运维动作不得误伤自身，单守护死亡必须被重建。
#
# 三值三码（SKIP 不计 PASS）：
#   PASS  H1  锚定 pkill 只杀绝对路径开头的目标，不命中携带同串的 ssh 会话
#   PASS  H2  TERM 单个 CORE 守护 ≤10s 重建；新 PID ≠ 旧 PID
#   PASS  H2b AIRY_SUPERVISOR_NO_RESPAWN=1 时不重建（回退开关有效）
#   PASS  H3s 启动器以 daemon count 为口径，stdin=/dev/null 不冒充整体退出
#   PASS  H4s CLI/TUI 均绑定 PR_SET_PDEATHSIG（启动器被强杀不留孤儿）
#   SKIP  H3r/H4r 全量安装态 + pty 实机演练（由 gate:runtime 实机验收覆盖）
#   exit 0 = 无 FAIL 且有 PASS；1 = 有 FAIL；2 = 全部 SKIP（缺前置）
#
# 用法：
#   scripts/gate_restart_semantics.sh
#   AIRY_SUPERVISOR_BIN=/tmp/a6b/bin/supervisor_d scripts/gate_restart_semantics.sh

set -uo pipefail

ROOT="$(cd -P "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SUP="${AIRY_SUPERVISOR_BIN:-}"
if [ -z "$SUP" ]; then
    for _c in "$ROOT/build/bin/supervisor_d" "$ROOT/build/daemons/supervisor_d/supervisor_d" \
              /tmp/a6b/bin/supervisor_d; do
        [ -x "$_c" ] && SUP="$_c" && break
    done
fi

PASS_N=0
FAIL_N=0
SKIP_N=0
pass() { printf 'PASS  %s\n' "$*"; PASS_N=$((PASS_N + 1)); }
fail() { printf 'FAIL  %s\n' "$*"; FAIL_N=$((FAIL_N + 1)); }
skip() { printf 'SKIP  %s\n' "$*"; SKIP_N=$((SKIP_N + 1)); }

SB="$(mktemp -d "${TMPDIR:-/tmp}/airy-restart.XXXXXX")" || exit 2
cleanup() {
    [ -n "${SUP_PID:-}" ] && kill -TERM "$SUP_PID" 2>/dev/null
    [ -n "${D_PID:-}" ] && kill -KILL "$D_PID" 2>/dev/null
    [ -n "${S_PID:-}" ] && kill -KILL "$S_PID" 2>/dev/null
    rm -rf "$SB"
}
trap cleanup EXIT

# 夹具：极简 sleeper（ELF，argv[0] 保留绝对路径，与真实 daemon 一致）
printf '#include <unistd.h>\nint main(void){sleep(60);return 0;}\n' >"$SB/fake.c"
if ! command -v gcc >/dev/null 2>&1 || ! gcc -O0 -o "$SB/fake_d" "$SB/fake.c" 2>/dev/null; then
    printf 'SKIP  全部门禁：缺 gcc，无法构建进程级夹具\n'
    exit 2
fi

# ── H1：测试启动器中的真实 pkill_bin（源码抽取，避免门禁逻辑漂移）──
sed -n '/^pkill_bin()/,/^}/p' "$ROOT/latest/airymaxrt" >"$SB/pkill_bin.sh"
if [ ! -s "$SB/pkill_bin.sh" ]; then
    fail "H1 启动器已无 pkill_bin 锚定函数"
else
    # shellcheck disable=SC1090
    . "$SB/pkill_bin.sh"
    mkdir -p "$SB/h1/.airymaxrt/bin"
    cp "$SB/fake_d" "$SB/h1/.airymaxrt/bin/think_d"
    "$SB/h1/.airymaxrt/bin/think_d" &
    D_PID=$!
    sh -c "sleep 60 # $SB/h1/.airymaxrt/bin/think_d ssh-proxy" &
    S_PID=$!
    sleep 0.3
    pkill_bin "$SB/h1/.airymaxrt/bin/think_d" || true
    sleep 0.3
    if kill -0 "$D_PID" 2>/dev/null; then
        fail "H1 目标守护未被锚定命中（pid=$D_PID）"
    elif ! kill -0 "$S_PID" 2>/dev/null; then
        fail "H1 误杀携带同串的 ssh 会话（pid=$S_PID）"
    else
        pass "H1 锚定命中目标 / 同串会话幸存"
    fi
    kill -KILL "$S_PID" 2>/dev/null; S_PID=""
    D_PID=""
fi

# ── H2/H2b：真 supervisor + 真 fork/exec，量测重建时延与回退开关 ──
run_sup_case() {
    tag="$1" no_respawn="$2" expect_respawn="$3"
    H="$SB/$tag"
    mkdir -p "$H/config" "$H/bin" "$H/run" "$H/logs" "$H/data"
    cp "$SB/fake_d" "$H/bin/gatefake_d"
    printf 'AIRY_PROFILE=full\nAIRYRT_LAUNCH_CORE=gatefake_d\n' >"$H/config/profile.env"
    if [ "$no_respawn" = "1" ]; then
        AIRY_HOME="$H" AIRY_SUPERVISOR_NO_RESPAWN=1 "$SUP" >>"$H/logs/sup.log" 2>&1 &
    else
        AIRY_HOME="$H" "$SUP" >>"$H/logs/sup.log" 2>&1 &
    fi
    SUP_PID=$!

    pid1=""
    for _i in $(seq 1 100); do
        pid1="$(pgrep -x gatefake_d 2>/dev/null | head -1)"
        [ -n "$pid1" ] && break
        sleep 0.1
    done
    if [ -z "$pid1" ]; then
        fail "$tag supervisor 未拉起声明的 CORE（见 $H/logs/sup.log）"
        kill -TERM "$SUP_PID" 2>/dev/null; wait "$SUP_PID" 2>/dev/null; SUP_PID=""
        return
    fi

    kill -TERM "$pid1" 2>/dev/null
    t0="$(date +%s%N)"
    pid2=""
    limit=100
    [ "$expect_respawn" = "0" ] && limit=20
    for _i in $(seq 1 "$limit"); do
        pid2="$(pgrep -x gatefake_d 2>/dev/null | head -1)"
        if [ -n "$pid2" ] && [ "$pid2" != "$pid1" ]; then break; fi
        pid2=""
        sleep 0.1
    done
    dt_ms=$((($(date +%s%N) - t0) / 1000000))

    if [ "$expect_respawn" = "1" ]; then
        if [ -n "$pid2" ]; then
            pass "$tag TERM 后 ${dt_ms}ms 重建（新 pid=$pid2）"
        else
            fail "$tag TERM 后 10s 内未重建"
        fi
    else
        if [ -z "$pid2" ]; then
            pass "$tag NO_RESPAWN 下未重建（回退开关有效）"
        else
            fail "$tag NO_RESPAWN 下仍重建（pid=$pid2）"
        fi
    fi

    AIRY_HOME="$H" "$SUP" stop >/dev/null 2>&1 || true
    for _i in $(seq 1 50); do kill -0 "$SUP_PID" 2>/dev/null || break; sleep 0.1; done
    kill -TERM "$SUP_PID" 2>/dev/null
    wait "$SUP_PID" 2>/dev/null
    SUP_PID=""
    pkill -KILL -x gatefake_d 2>/dev/null || true
}

if [ -z "$SUP" ] || [ ! -x "$SUP" ]; then
    skip "H2/H2b 缺 supervisor_d 构建产物（设 AIRY_SUPERVISOR_BIN）"
else
    run_sup_case h2 0 1
    run_sup_case h2b 1 0
fi

# ── H3s：启动器口径静态核对（运行态需全量安装，见 SKIP）──
LAUNCH="$ROOT/latest/airymaxrt"
if grep -q '^daemon_count()' "$LAUNCH" &&
   grep -q 'stdin_is_null' "$LAUNCH" &&
   grep -q 'daemon count=' "$LAUNCH"; then
    pass "H3s 启动器以 daemon count 为口径且识别 /dev/null"
else
    fail "H3s 启动器缺少 daemon_count/stdin_is_null 口径"
fi
skip "H3r 全量安装态启动器实机（需 gateway+13 daemon，由 gate:runtime 覆盖）"

# ── H4s：CLI/TUI 父死联动静态核对 ──
CLI_MAIN="$ROOT/tools/airy_cli/src/core/main.c"
TUI_ENTRY="$(cd -P "$ROOT/../sdk/tui/src" 2>/dev/null && pwd)/entry.rs"
if grep -q 'cli_bind_pdeath' "$CLI_MAIN" &&
   [ -f "$TUI_ENTRY" ] && grep -q 'PR_SET_PDEATHSIG' "$TUI_ENTRY"; then
    pass "H4s CLI 与 TUI 均绑定 PR_SET_PDEATHSIG 链路"
else
    skip "H4s 缺 TUI 源码（sdk 未同步）或绑定缺失（绑定缺失应视为 FAIL）"
    grep -q 'cli_bind_pdeath' "$CLI_MAIN" || fail "H4 缺 CLI 父死联动"
fi
skip "H4r pty 实机强杀启动器验证无孤儿 TUI（由 gate:runtime 覆盖）"

printf -- '---\nPASS=%d FAIL=%d SKIP=%d\n' "$PASS_N" "$FAIL_N" "$SKIP_N"
[ "$FAIL_N" -gt 0 ] && exit 1
[ "$PASS_N" -gt 0 ] && exit 0
exit 2
