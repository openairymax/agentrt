#!/usr/bin/env bash
#
# assert-ci-green.sh — 断言门禁工作流在指定 commit 上判绿（快照同源）
#
# 背景（0.1.20）：release 平面原先对目标 commit 从未经过门禁就聚合发行物，
# 于是「门禁验证的树」与「发行装配的树」可以完全不同一——上游一推进即分叉，
# 而发行物不可复现。装配输入的 pin 已由 init-submodules.sh 收敛到
# lf-gate-fixture 的单点声明，本脚本补齐另一半：被发布的 commit 本身必须是
# 门禁实际跑过并判绿的 commit。
#
# 判据（fail-closed，无跳过通道）：GATES 内每个工作流都必须在 head_sha 上
# 有一条 push 事件运行，且其终态 conclusion 为 success。据此：
#   - 无运行 → 红。两条成因都真：该 commit 非某次 push 的 tip（一次 push 只
#     为 tip 建运行），或纯文档变更被 paths-ignore 跳过。发行须改打门禁真正
#     跑过的 commit——"跳过判据发布"与"补跑一次 dispatch"都会让绿结论与被
#     发布的内容脱钩，正是本判据要消除的形态。
#   - 有运行但未终态 → 轮询等待至 deadline。tag 常紧随 main push 打出，
#     此时门禁仍在跑；等待使「打 tag」无需人工择时。
#   - 终态非 success（含 cancelled）→ 红。同一 sha 的运行被更新的 push 顶掉
#     而取消，说明该 sha 已非受检前沿，不得据此发行。
#
# 用法: assert-ci-green.sh <sha> [deadline_min] [workflow.yml ...]
#   deadline_min 默认 240；workflow.yml 缺省为 build-test.yml quality-gate.yml
#   （本仓门禁平面的权威清单，严禁在 workflow 内复制）。
# 依赖: gh CLI + GH_TOKEN/GITHUB_TOKEN（需 actions:read）+ GITHUB_REPOSITORY。

set -uo pipefail

SHA="${1:?usage: assert-ci-green.sh <sha> [deadline_min] [workflow.yml ...]}"
DEADLINE_MIN="${2:-240}"
GATES=("${@:3}")
if [ "${#GATES[@]}" -eq 0 ]; then
  GATES=(build-test.yml quality-gate.yml)
fi
REPO="${GITHUB_REPOSITORY:?GITHUB_REPOSITORY missing}"
TOKEN="${GH_TOKEN:-${GITHUB_TOKEN:-}}"

die() {
  echo "::error::$*"
  exit 1
}

[ "${#SHA}" -ge 40 ] || die "sha '$SHA' is not a full commit id"
command -v gh >/dev/null 2>&1 || die "gh CLI not available on this runner"
[ -n "$TOKEN" ] || die "GH_TOKEN/GITHUB_TOKEN missing (need actions:read)"

# 该 sha 上某门禁工作流的最新 push 运行状态，输出 "<status> <conclusion>"，
# 无运行输出 "absent -"。event/head_sha 交由 API 侧过滤，避免把 PR 合入
# 运行（head_sha 为 PR 头）误当对本 commit 的判定。
gate_state() {
  gh api "repos/$REPO/actions/workflows/$1/runs" \
    -X GET -f head_sha="$SHA" -f event='push' -f per_page=100 \
    --jq '.workflow_runs | if length == 0 then "absent -"
           else (.[0].status + " " + (.[0].conclusion // "-")) end'
}

# 单轮全量裁决：0=全绿 1=判红（含无运行/状态异常） 2=仍有运行在途。
check_once() {
  local wf st all_done=1
  for wf in "${GATES[@]}"; do
    st=$(gate_state "$wf") || { echo "::error::cannot query $wf at $SHA"; return 1; }
    case "$st" in
      'completed success') echo "gate: $wf green @ $(printf '%.7s' "$SHA")" ;;
      'absent -')
        echo "::error::no push run of $wf at $SHA — gate never verified it"
        echo "  release the commit the gates actually ran on (tag a push tip"
        echo "  that is not docs-only under paths-ignore)"
        return 1
        ;;
      'in_progress -' | 'queued -' | 'requested -') all_done=0 ;;
      *)
        echo "::error::$wf at $SHA is not green: '$st'"
        return 1
        ;;
    esac
  done
  [ "$all_done" = 1 ] && return 0
  return 2
}

deadline=$(( $(date +%s) + DEADLINE_MIN * 60 ))
while :; do
  check_once
  rc=$?
  [ "$rc" = 0 ] && { echo "ci-green: all gates green at $SHA"; exit 0; }
  [ "$rc" = 1 ] && exit 1
  [ "$(date +%s)" -ge "$deadline" ] &&
    die "gates not conclusive within ${DEADLINE_MIN}min at $SHA"
  echo "ci-green: gates still running, next poll in 60s"
  sleep 60
done
