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
# 依赖: gh CLI、GH_TOKEN/GITHUB_TOKEN（contents: read）、GITHUB_REPOSITORY
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
    if gh api "repos/$owner/$name/commits/$sha" --jq .sha >/dev/null 2>&1; then
        echo "ok   $path $owner/$name $sha"
    else
        echo "::error::$path pin $sha is not published to $owner/$name"
        fail=1
    fi
done < <(git -C "$BASE" ls-tree HEAD | awk '$2 == "commit"')

exit "$fail"
