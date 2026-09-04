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
# 依赖：bash + python3/python + GH_TOKEN（org repo 写权限）。
# 不依赖 gh CLI/curl：Windows runner 的 Git Bash 下 gh 不在 PATH（实测
# 1a39447dc 轮 windows 侧建 issue 静默失败），故全部走 python urllib。
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

# Windows runner 的 Git Bash 只有 python.exe（无 python3），统一探测解释器
PY="python3"
command -v python3 >/dev/null 2>&1 || PY="python"

# 提炼日志：取文件尾部整段（output-on-failure 的失败详情在 FAILED 汇总
# 之前，不能只从标记处起取）。压缩到 ~55KB 进 issue body。
"$PY" - "$LOGFILE" > /tmp/ci-debug-body.txt <<'PY' || true
import sys
fn = sys.argv[1]
try:
    txt = open(fn, encoding="utf-8", errors="replace").read()
except Exception:
    sys.exit(0)
txt = txt.replace("\r", "")
out = [ln if len(ln) <= 400 else ln[:400] + "\n" for ln in txt.splitlines(keepends=True)]
print("".join(out)[-55000:])
PY

# 组装 issue 元信息，交给 python 经 urllib 调 GitHub API
{
    echo "title<<<ci-debug ${TITLE_SUFFIX} (run ${GITHUB_RUN_ID})"
    echo "body<<<Job: ${GITHUB_JOB}"
    echo "Step: ${TITLE_SUFFIX}"
    echo "Run: ${GITHUB_RUN_ID}"
    echo "Log: ${LOGFILE}"
    echo ""
    echo '```'
    cat /tmp/ci-debug-body.txt
    echo '```'
} > /tmp/ci-debug-meta.txt

REPO="$REPO" LABEL="$LABEL" "$PY" - <<'PY'
import json
import os
import urllib.request
import urllib.parse

repo = os.environ["REPO"]
label = os.environ["LABEL"]
tok = os.environ["GH_TOKEN"]
api = f"https://api.github.com/repos/{repo}"
headers = {
    "Authorization": f"Bearer {tok}",
    "Accept": "application/vnd.github+json",
    "User-Agent": "ci-debug-dump",
}

def req(method, url, payload=None):
    data = json.dumps(payload).encode() if payload is not None else None
    r = urllib.request.Request(url, data=data, method=method, headers=headers)
    try:
        with urllib.request.urlopen(r, timeout=60) as resp:
            return resp.status, json.loads(resp.read().decode() or "null")
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode()[:500]

# 1) 关闭历史 open 的 ci-debug issue（保持至多一个 open）
q = urllib.parse.quote(f"repo:{repo} is:issue state:open label:{label}")
_, data = req("GET", f"https://api.github.com/search/issues?q={q}&per_page=50")
for item in (data or {}).get("items", []):
    req("PATCH", f"{api}/issues/{item['number']}",
        {"state": "closed", "state_reason": "not_planned",
         "body": (item.get("body") or "") + f"\n\nsuperseded by run {os.environ.get('GITHUB_RUN_ID','?')}"})

# 2) 创建新 issue
meta = open("/tmp/ci-debug-meta.txt", encoding="utf-8", errors="replace").read()
title = meta.split("title<<<", 1)[1].split("body<<<", 1)[0].strip()
body = meta.split("body<<<", 1)[1]
st, created = req("POST", f"{api}/issues",
                  {"title": title, "body": body.strip()})
if st not in (200, 201):
    print(f"ci-debug-dump: issue create failed st={st}: {created}")
    sys.exit(0)

# 3) 补标签（先建 issue 再打 label，规避 label 不存在的 422）
req("POST", f"{api}/issues/{created['number']}/labels", {"labels": [label]})
print(f"ci-debug-dump: issue -> {created['html_url']}")
PY
