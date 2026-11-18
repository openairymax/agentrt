#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
#
# gate:ci A-4 测试隔离门禁（§17.1 A-4 / P-F③④ + P-I②）
#
# 背景：测试此前直接写真实 $HOME（存量污染：~/.airymaxrt/data/agentrt 下
# logs/state/cache/tui/hall 等由 daemons 与 CLI 用例落盘；~/.airymaxrt-signing
# 与 ~/.config/airymax-release.env 由发布签名工具落盘），门禁只有「正向跑通」
# 判据，把门禁改坏也不会判红。
#
# 本脚本提供三件事：
#   1. 隔离跑：把 HOME / XDG_* / TMPDIR 重定向到沙箱目录后执行被测命令，
#      跑完比对真实 $HOME 的快照（路径集合 + size + mtime），有新增/变化
#      即判红。不用 inode 比对——overlayfs 与容器层会复用/漂移 inode。
#   2. 两腿独立：tests 腿（除 cli_ 前缀的全部用例）与 tools 腿（^cli_）
#      各写一份 verdict；verify 子命令要求两腿都存在且 PASS，缺一腿判红。
#   3. 反例注入：selftest 用探针故意写真实 $HOME，门禁必须判红；探针未被
#      拦下则说明门禁自身失效，selftest 判 FAIL。
#
# 回退：AIRY_GATE_SELFTEST=0 可跳过反例注入（仅排障），CI 中禁止关闭。
#
# 用法：
#   gate_home_isolate.sh run --leg tests|tools [--verdict-dir DIR] -- CMD...
#   gate_home_isolate.sh selftest [--verdict-dir DIR]
#   gate_home_isolate.sh verify --verdict-dir DIR
#
# 退出码：0 通过；1 门禁判红；2 用法错误。

set -u

A4_LEGS="tests tools"
A4_SNAP_OPTS="-mindepth 1"
# 白名单（相对真实 $HOME 的 glob）：只允许「与测试无关、会被系统/用户会话
# 并发改动」的易变文件。测试自身写出的文件一律不得入白——必须隔离或改用例，
# 否则门禁失明。白名单即门禁的一部分，增删须随本脚本一起评审。
A4_WHITE_PATTERNS=(
    '.bash_history'
    '.Xauthority'
    '.sudo_as_admin_successful'
    # CI 自带树：runner 在 $HOME 下自写诊断日志与检出/构建目录（GitHub
    # hosted runner 的 HOME 即 /home/runner），与用例无关且并发变动。
    # 白名单只放行宿主自带树，$HOME 下的应用/配置路径仍逐条判红。
    'actions-runner/*'
    'work/*'
)

a4_usage() {
    sed -n '23,28p' "$0"
    exit 2
}

# 真实 HOME：CI 里即 runner 的 HOME；测试机可用 REAL_HOME 覆盖。
a4_real_home() {
    printf '%s\n' "${REAL_HOME:-$HOME}"
}

a4_default_vdir() {
    printf '%s\n' "${RUNNER_TEMP:-/tmp}/airy-gate-a4"
}

# 快照：路径集合 + size + mtime。符号链接不跟随（find 缺省 -P），避免逃逸
# 到 HOME 外。GNU find 有 -printf 时单进程直出；BSD/macOS 缺该谓词则回退
# 逐文件 stat（开发机 HOME 上百万条目时回退路径慢到门禁不可用，CI 小 HOME
# 不受影响——两条路径产出的字段语义一致，只是取数方式不同）。
a4_snap() {
    local home="$1" out="$2"
    if find "$home" -maxdepth 0 -printf '' >/dev/null 2>&1; then
        find "$home" $A4_SNAP_OPTS -printf '%y\t%s\t%T@\t%p\n' 2>/dev/null |
            LC_ALL=C sort >"$out"
        return 0
    fi
    find "$home" $A4_SNAP_OPTS -exec bash -c '
        for p in "$@"; do
            if stat -c "%s %Y" "$p" >/dev/null 2>&1; then
                st=$(stat -c "%s %Y" "$p")
            else
                st=$(stat -f "%z %m" "$p" 2>/dev/null) || continue
            fi
            sz=${st%% *}; mt=${st##* }; ty=f
            [ -d "$p" ] && ty=d
            printf "%s\t%s\t%s\t%s\n" "$ty" "$sz" "$mt" "$p"
        done
    ' _ {} + 2>/dev/null | LC_ALL=C sort >"$out"
}

# 白名单：相对 HOME 的 glob，命中的新增/变化不判红。
a4_skip() {
    local rel="$1" pat
    for pat in "${A4_WHITE_PATTERNS[@]}"; do
        # shellcheck disable=SC2254
        case "$rel" in $pat) return 0 ;; esac
    done
    return 1
}

# 比对快照，输出未白名单的差异行（新增或 size/mtime 变化）。
a4_diff() {
    local home="$1" before="$2" after="$3" out="$4"
    : >"$out"
    # comm -13：after 独有；comm -23 中同路径变属性的行也按「路径」合并。
    comm -13 "$before" "$after" >"$out.raw"
    while IFS= read -r line; do
        local path rel
        path=${line##*$'\t'}
        rel=${path#"$home"/}
        a4_skip "$rel" || printf '%s\n' "$line" >>"$out"
    done <"$out.raw"
    rm -f "$out.raw"
}

a4_verdict_dir() {
    local vdir="$1"
    mkdir -p "$vdir" || return 1
    printf '%s\n' "$vdir"
}

a4_write_verdict() {
    local vdir="$1" leg="$2" verdict="$3" detail="$4"
    printf '%s\n' "$verdict" >"$vdir/leg-$leg.verdict"
    printf '%s\n' "$detail" >>"$vdir/leg-$leg.verdict"
}

a4_run() {
    local leg="" vdir="" sandbox rc
    while [ $# -gt 0 ]; do
        case "$1" in
            --leg) leg="$2"; shift 2 ;;
            --verdict-dir) vdir="$2"; shift 2 ;;
            --) shift; break ;;
            *) echo "gate_home_isolate: 未知参数 $1" >&2; a4_usage ;;
        esac
    done
    [ -n "$leg" ] && [ $# -gt 0 ] || a4_usage
    case " $A4_LEGS " in *" $leg "*) ;; *) echo "gate_home_isolate: 未知腿 $leg" >&2; a4_usage ;; esac
    [ -n "$vdir" ] || vdir=$(a4_default_vdir)
    vdir=$(a4_verdict_dir "$vdir") || exit 1

    local home
    home=$(a4_real_home)
    sandbox="$vdir/sandbox-$leg"
    rm -rf "$sandbox"
    mkdir -p "$sandbox/home" "$sandbox/tmp" "$sandbox/xdg/config" \
        "$sandbox/xdg/cache" "$sandbox/xdg/data" "$sandbox/xdg/state" \
        "$sandbox/xdg/runtime" || exit 1
    chmod 700 "$sandbox/xdg/runtime" 2>/dev/null || true

    a4_snap "$home" "$vdir/snap-$leg.before"

    echo "::group::A-4 $leg 腿隔离执行（HOME=$sandbox/home）"
    (
        export HOME="$sandbox/home"
        export XDG_CONFIG_HOME="$sandbox/xdg/config"
        export XDG_CACHE_HOME="$sandbox/xdg/cache"
        export XDG_DATA_HOME="$sandbox/xdg/data"
        export XDG_STATE_HOME="$sandbox/xdg/state"
        export XDG_RUNTIME_DIR="$sandbox/xdg/runtime"
        export TMPDIR="$sandbox/tmp"
        export TMP="$sandbox/tmp" TEMP="$sandbox/tmp"
        "$@"
    )
    rc=$?
    echo "::endgroup::"

    a4_snap "$home" "$vdir/snap-$leg.after"
    local offenders="$vdir/offenders-$leg.txt"
    a4_diff "$home" "$vdir/snap-$leg.before" "$vdir/snap-$leg.after" "$offenders"

    if [ -s "$offenders" ]; then
        echo "::error::A-4 $leg 腿写入了真实 HOME（$home）："
        head -40 "$offenders" | sed 's/^/::error::  /'
        a4_write_verdict "$vdir" "$leg" FAIL "真实 HOME 新增/变化 $(( $(wc -l <"$offenders") )) 项"
        return 1
    fi
    if [ $rc -ne 0 ]; then
        echo "::error::A-4 $leg 腿用例自身失败 rc=$rc"
        a4_write_verdict "$vdir" "$leg" FAIL "用例失败 rc=$rc"
        return 1
    fi
    a4_write_verdict "$vdir" "$leg" PASS "隔离执行且真实 HOME 零新增"
    echo "A-4 $leg 腿：PASS（真实 HOME 零新增）"
    return 0
}

a4_probe() {
    # 反例注入：故意写真实 $HOME；必须被 run 的比对拦下。
    # 入参是快照用的真实 HOME——不能在这里读 $HOME，沙箱已把它改掉。
    local home="$1"
    printf 'airy gate selftest probe\n' >"$home/.airy_gate_selftest_probe"
    echo "probe wrote $home/.airy_gate_selftest_probe"
}

a4_selftest() {
    local vdir=""
    while [ $# -gt 0 ]; do
        case "$1" in
            --verdict-dir) vdir="$2"; shift 2 ;;
            *) a4_usage ;;
        esac
    done
    [ -n "$vdir" ] || vdir=$(a4_default_vdir)
    vdir=$(a4_verdict_dir "$vdir") || exit 1

    if [ "${AIRY_GATE_SELFTEST:-1}" = "0" ]; then
        echo "A-4 selftest：SKIP（AIRY_GATE_SELFTEST=0，仅排障；CI 中禁止关闭）"
        printf 'SKIP\n' >"$vdir/selftest.verdict"
        return 0
    fi

    local home
    home=$(a4_real_home)
    rm -f "$home/.airy_gate_selftest_probe"

    # 快照 → 探针 → 比对：探针只写真实 $HOME，隔离沙箱内的写不算污染。
    local sandbox="$vdir/sandbox-selftest"
    rm -rf "$sandbox"
    mkdir -p "$sandbox/home" "$sandbox/tmp" "$sandbox/xdg/config" \
        "$sandbox/xdg/cache" "$sandbox/xdg/data" "$sandbox/xdg/state" \
        "$sandbox/xdg/runtime" || exit 1
    a4_snap "$home" "$vdir/snap-selftest.before"
    (
        export HOME="$sandbox/home" TMPDIR="$sandbox/tmp"
        export XDG_CONFIG_HOME="$sandbox/xdg/config"
        export XDG_CACHE_HOME="$sandbox/xdg/cache"
        export XDG_DATA_HOME="$sandbox/xdg/data"
        export XDG_STATE_HOME="$sandbox/xdg/state"
        export XDG_RUNTIME_DIR="$sandbox/xdg/runtime"
        a4_probe "$home"
    ) >"$vdir/selftest.log" 2>&1
    a4_snap "$home" "$vdir/snap-selftest.after"
    a4_diff "$home" "$vdir/snap-selftest.before" "$vdir/snap-selftest.after" \
        "$vdir/offenders-selftest.txt"

    rm -f "$home/.airy_gate_selftest_probe"
    if [ -s "$vdir/offenders-selftest.txt" ]; then
        echo "A-4 selftest：PASS（探针被拦下，门禁有效）"
        printf 'PASS\n探针被拦下\n' >"$vdir/selftest.verdict"
        return 0
    fi
    echo "::error::A-4 selftest：FAIL——探针写入真实 HOME 未被拦下，门禁自身失效"
    printf 'FAIL\n探针未被拦下\n' >"$vdir/selftest.verdict"
    return 1
}

a4_verify() {
    local vdir=""
    while [ $# -gt 0 ]; do
        case "$1" in
            --verdict-dir) vdir="$2"; shift 2 ;;
            *) a4_usage ;;
        esac
    done
    [ -n "$vdir" ] || vdir=$(a4_default_vdir)

    local fail=0 leg f
    for leg in $A4_LEGS; do
        f="$vdir/leg-$leg.verdict"
        if [ ! -f "$f" ]; then
            echo "::error::A-4 缺 $leg 腿结论（$f 不存在）"
            fail=1
            continue
        fi
        if [ "$(head -1 "$f")" != "PASS" ]; then
            echo "::error::A-4 $leg 腿判红：$(head -1 "$f")"
            fail=1
        else
            echo "A-4 $leg 腿：PASS"
        fi
    done
    f="$vdir/selftest.verdict"
    if [ -f "$f" ]; then
        if [ "$(head -1 "$f")" = "FAIL" ]; then
            echo "::error::A-4 反例注入判红：门禁自身失效"
            fail=1
        else
            echo "A-4 反例注入：$(head -1 "$f")"
        fi
    elif [ "${AIRY_GATE_SELFTEST:-1}" != "0" ]; then
        echo "::error::A-4 缺反例注入结论（selftest 未执行）"
        fail=1
    fi
    return $fail
}

cmd="${1:-}"
[ $# -gt 0 ] && shift
case "$cmd" in
    run) a4_run "$@" ;;
    selftest) a4_selftest "$@" ;;
    verify) a4_verify "$@" ;;
    *) a4_usage ;;
esac
