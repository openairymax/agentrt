#!/usr/bin/env bash
#
# assert-submodule-pins.sh — 断言发行 commit 的全部子模块指针已发布到远端
#
# 背景（0.1.21）：发行平面的子模块检出按 gitlink 精确取 SHA，不跟随分支
# tip。上游仓本地新增、尚未 push 到 GitHub 的指针提交，会让整条构建矩阵在
# checkout 阶段失败并报 "not our ref" / "Unable to find"，日志里看不到任何
# 编译错误，定位成本全落在「哪个子模块的哪个 SHA 没推」上。判据前移到门禁
# 绿之后、装配之前：逐个 gitlink 用 GitHub API 确认 commit 可达，不可达即
# 打印 <path> <repo> <sha> 并 fail-closed。
#
# 用法: assert-submodule-pins.sh [base-dir]
# 依赖: gh CLI、GH_TOKEN（跨仓 contents: read，闭源子仓需要）、GITHUB_REPOSITORY
set -uo pipefail

BASE="${1:-.}"
REPO="${GITHUB_REPOSITORY:?GITHUB_REPOSITORY missing}"
TOKEN="${GH_TOKEN:-${GITHUB_TOKEN:-}}"

die() {
    echo "::error::$*"
    exit 1
}

[ -n "$TOKEN" ] || die "GH_TOKEN/GITHUB_TOKEN missing"
command -v gh >/dev/null 2>&1 || die "gh CLI not available"
export GH_TOKEN="$TOKEN"

fail=0
ERR=""
# 提交面查询带有限重试：404 是终局（真的没有），限流/5xx/网络抖动不是。
# 本判据坐在发行闸口上，一次瞬时失败会连坐整条装配腿，日志里只剩
# 「未发布」的假象。
pinned() {
    local api="repos/$1/$2/commits/$3" out i

    for i in 1 2 3; do
        if out=$(gh api "$api" --jq .sha 2>&1 >/dev/null); then
            return 0
        fi
        case "$out" in
            *404*) break ;;
        esac
        sleep 5
    done
    ERR="${out%%$'\n'*}"
    return 1
}

while read -r _mode type sha path; do
    [ "$type" = "commit" ] || continue
    url=$(git config -f "$BASE/.gitmodules" --get "submodule.$path.url") || {
        echo "::error::$path has no .gitmodules url"
        fail=1
        continue
    }
    case "$url" in
        ../*)
            owner="${REPO%%/*}"
            name="${url#../}"
            ;;
        git@github.com:*)
            spec="${url#git@github.com:}"
            owner="${spec%%/*}"
            name="${spec#*/}"
            ;;
        https://github.com/*)
            spec="${url#https://github.com/}"
            owner="${spec%%/*}"
            name="${spec#*/}"
            ;;
        *)
            echo "::warning::$path uses non-GitHub url '$url', skipped"
            continue
            ;;
    esac
    name="${name%.git}"
    if pinned "$owner" "$name" "$sha"; then
        echo "ok   $path $owner/$name $sha"
    else
        # 不可达有两种来源：指针真没推，或 token 读不到该仓（闭源子仓 +
        # 作用域仅限本仓的 GITHUB_TOKEN）。把 API 原文一并打印，避免把
        # 「无读权」误报成「未发布」（run 44312572826 实证）。
        echo "::error::$path pin $sha is not reachable in $owner/$name: $ERR"
        fail=1
    fi
done < <(git -C "$BASE" ls-tree HEAD | awk '$2 == "commit"')

exit "$fail"
