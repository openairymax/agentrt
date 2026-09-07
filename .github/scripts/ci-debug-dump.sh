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
# 不依赖 gh CLI/curl；Windows runner 的 Git Bash 路径语义与原生 python
# 不一致（D:\... 路径、/tmp 映射），故整段提炼+API 全在 python 内完成，
# 日志路径经环境变量传递，杜绝跨 shell 路径漂移。
set -uo pipefail

TITLE_SUFFIX="${1:?usage: ci-debug-dump.sh <title-suffix> <logfile>}"
LOGFILE="${2:?usage: ci-debug-dump.sh <title-suffix> <logfile>}"

if [ -z "${GH_TOKEN:-}" ]; then
    echo "ci-debug-dump: skip (no GH_TOKEN)"
    exit 0
fi
# 注意：不在 bash 层用 [ -s ] 探测日志（Windows runner 的 Git Bash 无法
# 解析 D:\... 原生路径，会误判文件缺失），日志读取/缺失判断全在 python 内。

PY="python3"
command -v python3 >/dev/null 2>&1 || PY="python"

REPO="${GITHUB_REPOSITORY:-openairymax/agentrt}" \
LABEL="ci-debug" \
LOGFILE="$LOGFILE" \
TITLE_SUFFIX="$TITLE_SUFFIX" \
RUN_ID="${GITHUB_RUN_ID:-?}" \
JOB_NAME="${GITHUB_JOB:-?}" \
"$PY" - <<'PY'
import json
import os
import re
import sys
import urllib.error
import urllib.parse
import urllib.request

repo = os.environ["REPO"]
label = os.environ["LABEL"]
tok = os.environ["GH_TOKEN"]
logfile = os.environ["LOGFILE"]
suffix = os.environ["TITLE_SUFFIX"]
run_id = os.environ["RUN_ID"]
job = os.environ["JOB_NAME"]
api = f"https://api.github.com/repos/{repo}"
headers = {
    "Authorization": f"Bearer {tok}",
    "Accept": "application/vnd.github+json",
    "User-Agent": "ci-debug-dump",
}

# 1) 提炼日志：取文件尾部整段（output-on-failure 的失败详情在 FAILED
#    汇总之前，不能只从标记处起取）。压缩到 ~55KB 进 issue body。
#    ctest 类失败（TITLE_SUFFIX 含 "ctest"）日志被 INFO 洪流淹没，改走
#    "信号行摘要 + 原始尾部" 双段：只保留 Test # / PASS|FAIL / 断言 /
#    崩溃码 / ERROR 等关键行，再附原始尾部供上下文核对。
try:
    txt = open(logfile, encoding="utf-8", errors="replace").read()
except Exception as e:
    print(f"ci-debug-dump: cannot read log {logfile}: {e}")
    sys.exit(0)
txt = txt.replace("\r", "")
lines = txt.splitlines(keepends=True)
trunc = lambda s: s if len(s) <= 400 else s[:400] + "\n"

def build_body():
    if "ctest" not in suffix:
        # 非 ctest（编译日志等）：整段取尾部（原语义不变）
        return "".join(trunc(ln) for ln in lines)[-55000:]
    sig = re.compile(
        r"(Test\s+#|Passed|\*\*\*Failed|\bPASS\b|\bFAIL\b|\[FAIL\]|\[error\]|"
        r"assertion|Assertion|assert\b|0xc[0-9a-fA-F]{6}|Access violation|"
        r"stack overflow|STATUS_|terminate|CRITICAL|\[WARN\]|exception|"
        r"exit code|Error:|error [A-Z]|LNK\d|failed test names)",
        re.IGNORECASE,
    )
    hits = [ln for ln in lines if sig.search(ln)]
    digest = "".join(trunc(ln) for ln in hits)
    if len(digest) > 43000:
        digest = digest[-43000:]
    tail = "".join(lines)[-11000:]
    head = ("ctest 信号行摘要（行数 %d，原始 %d 行，截尾详见 RAW TAIL）\n```\n"
            % (len(hits), len(lines)))
    return head + digest + "\n```\n\n==== RAW TAIL (last 11KB) ====\n```\n" + tail + "\n```"

body_log = build_body()

body = f"""Job: {job}
Step: {suffix}
Run: {run_id}
Log: {logfile}

```
{body_log}
```"""


def req(method, url, payload=None):
    data = json.dumps(payload).encode() if payload is not None else None
    r = urllib.request.Request(url, data=data, method=method, headers=headers)
    try:
        with urllib.request.urlopen(r, timeout=60) as resp:
            return resp.status, json.loads(resp.read().decode() or "null")
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode()[:500]


# 2) issue 卫生：同一 run 内不同失败步骤各自留档（不再互相覆盖——
#    0.1.10 前 windows build 错误曾被子步骤 ctest rerun 顶掉，无法取证）；
#    关闭更早 run 的旧 issue。同 run 同步骤重复调用 → 追加 comment。
q = urllib.parse.quote(f"repo:{repo} is:issue state:open label:{label}")
_, data = req("GET", f"https://api.github.com/search/issues?q={q}&per_page=50")
run_tag = f"run {run_id}"
dup = None
for item in (data or {}).get("items", []):
    if run_tag in item["title"]:
        if suffix in item["title"]:
            dup = item
        continue  # 同一 run 的其他失败步骤：保留独立 issue
    req("PATCH", f"{api}/issues/{item['number']}",
        {"state": "closed", "state_reason": "not_planned",
         "body": (item.get("body") or "") + f"\n\nsuperseded by {run_tag}"})

if dup is not None:
    req("POST", f"{api}/issues/{dup['number']}/comments",
        {"body": f"**{suffix} 再次失败（重跑/重复上报）**\n\n{body}"})
    print(f"ci-debug-dump: comment -> {dup['html_url']}")
    sys.exit(0)

# 3) 创建新 issue
title = f"ci-debug {suffix} (run {run_id})"
st, created = req("POST", f"{api}/issues", {"title": title, "body": body})
if st not in (200, 201):
    print(f"ci-debug-dump: issue create failed st={st}: {created}")
    sys.exit(0)

# 4) 补标签（先建 issue 再打 label，规避 label 不存在的 422）
req("POST", f"{api}/issues/{created['number']}/labels", {"labels": [label]})
print(f"ci-debug-dump: issue -> {created['html_url']}")
PY
