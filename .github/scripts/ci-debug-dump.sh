#!/usr/bin/env bash
#
# ci-debug-dump.sh — 失败步骤日志直传 ci-debug issue
#
# 背景：GitHub Actions 完整运行日志（build/ctest stdout）需登录态才能下载，
# 匿名 API 只能读到每 job ≤10 条 annotation。本项目以 GitHub 为 mac/windows
# 编译测试场、经 MCP/curl（无本地 PAT）排障，需要可匿名读取的失败明细。
# 故失败时把日志提炼段写入公共 repo 的 ci-debug issue（issue/comment 对
# 匿名 API 开放），供外部拉取定位根因。
#
# 卫生策略：每次调用先关闭此前 open 的 ci-debug issue（至多保留一个），
# 全绿后自然不再产生。
#
# 用法: ci-debug-dump.sh "<标题后缀>" "<日志文件>"
# 依赖：gh CLI（GitHub 官方 runner 预装）+ GH_TOKEN（PAT，org repo 写权限）
set -uo pipefail

TITLE_SUFFIX="${1:?usage: ci-debug-dump.sh <title-suffix> <logfile>}"
LOGFILE="${2:?usage: ci-debug-dump.sh <title-suffix> <logfile>}"
REPO="${GITHUB_REPOSITORY:-openairymax/agentrt}"
LABEL="ci-debug"

if [ -z "${GH_TOKEN:-}" ]; then
    echo "ci-debug-dump: skip (no GH_TOKEN)"
    exit 0
fi
if [ ! -s "$LOGFILE" ]; then
    echo "ci-debug-dump: skip (log empty/missing: $LOGFILE)"
    exit 0
fi

# 关闭历史 open 的 ci-debug issue，避免堆积
if gh issue list --repo "$REPO" --label "$LABEL" --state open \
        --json number -q '.[].number' >/tmp/ci-debug-open.txt 2>/dev/null; then
    while read -r n; do
        [ -n "$n" ] || continue
        gh issue close "$n" --repo "$REPO" \
            --comment "superseded by run $GITHUB_RUN_ID" >/dev/null 2>&1 || true
    done < /tmp/ci-debug-open.txt
fi

# 提炼日志：优先从 "The following tests FAILED" 段起取；否则取文件尾部。
# 压缩到 ~40KB，确保进得去 issue body。
python3 - "$LOGFILE" > /tmp/ci-debug-body.txt <<'PY' || true
import sys
fn = sys.argv[1]
try:
    txt = open(fn, encoding="utf-8", errors="replace").read()
except Exception:
    sys.exit(0)
marker = "The following tests FAILED"
i = txt.find(marker)
if i >= 0:
    body = txt[i:]
else:
    body = txt
# 去掉 CR、折叠超长行后截尾
body = body.replace("\r", "")
lines = body.splitlines(keepends=True)
out = []
for ln in lines:
    out.append(ln if len(ln) <= 400 else ln[:400] + "…\n")
body = "".join(out)
print(body[-40000:])
PY

# 标签可能不存在：gh issue create 引用不存在标签会失败，先兜底创建
#（已存在时 gh 报错，|| true 忽略即可）
gh label create "$LABEL" --repo "$REPO" >/dev/null 2>&1 || true

BODY="Job: ${GITHUB_JOB}
Step: ${TITLE_SUFFIX}
Run: ${GITHUB_RUN_ID}
Log: ${LOGFILE}

\`\`\`
$(cat /tmp/ci-debug-body.txt)
\`\`\`"

gh issue create --repo "$REPO" \
    --title "ci-debug ${TITLE_SUFFIX} (run ${GITHUB_RUN_ID})" \
    --label "$LABEL" \
    --body "$BODY" >/tmp/ci-debug-url.txt 2>&1 || {
        echo "ci-debug-dump: issue create failed"
        tail -2 /tmp/ci-debug-url.txt || true
        exit 0
    }
echo "ci-debug-dump: issue -> $(cat /tmp/ci-debug-url.txt)"
