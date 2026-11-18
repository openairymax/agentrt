#!/bin/bash
# SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
# ---------------------------------------------------------------------------
# L-8 端到端任务闭环回归：真实安装基线（runtime 档，含真实包安装）。
#
# 用例：
#   firefox         安装 firefox 夹具包并校验（cli/tui 双端同源各一次）
#   tencent-meeting 安装腾讯会议夹具包并校验（同上）
#
# 包管理器适配器（安装动作只经任务链 shell_run 真实执行，测试脚本不代劳）：
#   * 有 dnf+rpm（285H runtime）：fixtures 现场构建 noarch rpm + repodata
#     作离线 dnf 源，shell_run 内以 --installroot 私有根安装，
#     verifier 用 `rpm --root <root> -q <pkg>`；
#   * 无 dnf（本机 Ubuntu 26.04）：本地离线 .deb 仓库夹具（dpkg-deb 现场
#     构建真实小包），shell_run 内以私有 DPKG_ADMINDIR/instdir 安装，
#     verifier 用 `dpkg -s <pkg>`；不触碰宿主 /var/lib/dpkg 与 /usr；
#   * 两者皆缺：显式 SKIP 并写明原因（runtime 允许 SKIP，CI 档不含本用例）。
#
# 双端同源：同一 submit 内容分别以 client=cli / client=tui 各发一次，两端
# task_id 各一、事件类别与终态同构。TUI 不得直发 agent.run：以
# scripts/verify_release_gates.sh 的 TD1 静态门禁（TUI 零 agent.run）作静态
# 证据落盘（$EV/td-gate.log），理由见 README.md。
#
# 用法: case_install.sh <firefox|tencent-meeting>
# 退出码: 0 全绿（SKIP 亦为 0，但独立计数）；1 有 FAIL；10 环境不可用。
# ---------------------------------------------------------------------------
set -u
DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=./lib_tasks.sh
. "$DIR/lib_tasks.sh"

CASE=${1:?usage: case_install.sh <firefox|tencent-meeting>}
need || { echo "ENV: 前置缺失（BIN/CLI/mock/python3/curl）"; exit 10; }

PM=none
ADM="$WS/d/a" # dpkg 私有 admindir（本机形态）
ROOT="$WS/d/r" # 私有 instdir / dnf installroot
GOAL=""

skip_env() { # skip_env <原因>：缺前置必须显式 SKIP，不得当作通过
    echo "SKIP: $1"
    skipc "安装基线 $CASE：$1"
    summary
    exit 0
}

pm_init() {
    if command -v dnf >/dev/null 2>&1 && command -v rpm >/dev/null 2>&1; then
        PM=dnf
        ROOT="$WS/rpmroot"
        return 0
    fi
    if command -v dpkg >/dev/null 2>&1 && command -v dpkg-deb >/dev/null 2>&1; then
        PM=dpkg
        return 0
    fi
    return 1
}

repo_build() { # 离线源现场构建（deb 或 rpm），返回夹具脚本退出码
    bash "$FX/build_offline_repo.sh" "$PKG" >"$EV/repo-build.log" 2>&1
}

prep() { # 私有安装根与 dpkg 状态区（仅目录准备，不调用包管理器）
    mkdir -p "$ROOT" || return 1
    if [ "$PM" = dpkg ]; then
        mkdir -p "$ADM/updates" "$ADM/info" && : >"$ADM/status" || return 1
    fi
    return 0
}

inst_cmd() { # inst_cmd <deb 文件名>：交由任务链 shell_run 执行的安装命令
    if [ "$PM" = dnf ]; then
        printf "dnf -y --disablerepo='*' --repofrompath=l8,file://%s/rpm --nogpgcheck --installroot=%s install %s" \
            "$PKG" "$ROOT" "$1"
    else
        printf 'DPKG_ADMINDIR=%s dpkg --force-not-root --instdir=%s --admindir=%s -i %s/deb/%s' \
            "$ADM" "$ROOT" "$ADM" "$PKG" "$1"
    fi
}

ver_cmd() { # ver_cmd <包名>：verifier 判据命令（同样经工具链执行）
    if [ "$PM" = dnf ]; then
        printf 'rpm --root=%s -q %s' "$ROOT" "$1"
    else
        printf 'DPKG_ADMINDIR=%s dpkg -s %s | grep -q "^Status: install ok installed"' "$ADM" "$1"
    fi
}

dual() { # dual <包名>：双端同源各提交一次并断言事件类别/终态同构
    local pkg=$1 t0 a b cli tui
    board "$EV/before-cli.json"
    t0=$(date +%s%3N)
    submit "$EV/cli.json" "$GOAL" "$(crit "$(ver_cmd "$pkg")")" '{"client":"cli"}'
    chk_case cli "$EV/cli.json" completed completed pass "$EV/before-cli.json" "$t0"
    cli=$L8_LAST_TID
    board "$EV/before-tui.json"
    t0=$(date +%s%3N)
    submit "$EV/tui.json" "$GOAL" "$(crit "$(ver_cmd "$pkg")")" '{"client":"tui"}'
    chk_case tui "$EV/tui.json" completed completed pass "$EV/before-tui.json" "$t0"
    tui=$L8_LAST_TID
    ck "$([ -n "$cli" ] && [ -n "$tui" ] && [ "$cli" != "$tui" ] && echo 1 || echo 0)" \
        "双端同源：cli/tui 各得 task_id 且互不相同（$cli / $tui）"
    a=$(pyf "$EV/replay-$cli.json" '",".join(sorted({e.get("category","") for e in d.get("result",{}).get("events",[])}))')
    b=$(pyf "$EV/replay-$tui.json" '",".join(sorted({e.get("category","") for e in d.get("result",{}).get("events",[])}))')
    ck "$([ -n "$a" ] && [ "$a" = "$b" ] && echo 1 || echo 0)" \
        "双端同源：事件类别同构（${a:-<none>}）"
    a=$(pyf "$EV/board-cli.json" "next((e.get('state','') for e in d.get('result',{}).get('entries',[]) if e.get('execution_id')=='$cli'), '')")
    b=$(pyf "$EV/board-tui.json" "next((e.get('state','') for e in d.get('result',{}).get('entries',[]) if e.get('execution_id')=='$tui'), '')")
    ck "$([ "$a" = "$b" ] && [ "$a" = completed ] && echo 1 || echo 0)" \
        "双端同源：终态同构（cli=$a tui=$b）"
}

real_ck() { # real_ck <包名>：安装确由任务链执行（载荷落盘 + 审计可见包管理器）
    local pkg=$1 art="$ROOT/opt/airyl8/$1/version"
    ck "$([ -f "$art" ] && echo 1 || echo 0)" "真实安装：包载荷落在私有根（$art）"
    rpc tool.jobs '{}' "$EV/jobs-install.json" >/dev/null
    ck "$(pyf "$EV/jobs-install.json" "int(any(j.get('tool_id')=='shell_run' and j.get('state')=='done' and '$PM' in ((j.get('params_sum') or '') + (j.get('output_sum') or '')) for j in d.get('result',{}).get('jobs',[])))")" \
        "真实安装：shell_run 审计可见 $PM 命令（非测试脚本代劳）"
}

case_main() { # case_main <展示名> <包名> <deb 文件名>
    local disp=$1 pkg=$2 file=$3 rc
    repo_build
    rc=$?
    [ "$rc" -eq 3 ] && skip_env "离线夹具包构建缺少 dpkg-deb/rpmbuild"
    if [ "$rc" -ne 0 ]; then
        ck 0 "离线夹具包构建失败（rc=$rc，见 $EV/repo-build.log）"
        return
    fi
    prep || {
        ck 0 "私有安装根初始化失败（$ROOT）"
        return
    }
    mset mode install
    mset command "$(inst_cmd "$file")"
    mup || {
        ck 0 "mock LLM 重启失败（见 $LOGS/mock.log）"
        return
    }
    GOAL="安装 $disp 并验证（$CASE 基线，$PM 适配）"
    dual "$pkg"
    real_ck "$pkg"
    td_gate
}

pm_init || skip_env "本机既无 dnf/rpm 也无 dpkg/dpkg-deb，安装基线无法执行"
up || {
    echo "ENV: 守护栈未就绪（见 $LOGS）"
    exit 10
}
trap 'down' EXIT

case "$CASE" in
    firefox) case_main firefox airyl8-firefox airyl8-firefox_1.0_all.deb ;;
    tencent-meeting) case_main tencent-meeting airyl8-tencent-meeting airyl8-tencent-meeting_1.0_all.deb ;;
    *)
        echo "unknown case: $CASE" >&2
        exit 2
        ;;
esac

summary
