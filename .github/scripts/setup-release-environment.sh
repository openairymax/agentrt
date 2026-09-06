#!/usr/bin/env bash
# setup-release-environment.sh — release environment 全量配置固化（0.1.12 H3）
#
# 背景（docs-closed/agentrt/0.1.12-架构改进方案.md P17/P18）：
#   - deployment_branch_policy 为 custom 模式但列表为空时，publish job 的
#     deployment 永不创建、卡 waiting（v0.1.10 正式 tag 曾卡死）；
#   - branch 型 policy 只匹配分支不匹配 tag：tag 触发 run 的 deployment ref
#     是 tag 名，必须另登记 tag 型 policy（undocumented type:"tag" 能力）；
#   - PUT environment 整体更新在 custom_branch_policies 与 protected_branches
#     同值时返回 422，故 policy 登记一律走独立 POST 端点。
#
# 本脚本幂等：重复运行不产生副作用；终验 fail-closed（缺 policy 即非零退出），
# 可直接作为 release 全链回归的前置步骤或 CI 步骤内嵌。
#
# 用法：GH_TOKEN=<token> ./setup-release-environment.sh [owner/repo] [env-name]
#   默认 owner/repo=openairymax/agentrt，env-name=release
set -euo pipefail

OWNER_REPO="${1:-openairymax/agentrt}"
ENV_NAME="${2:-release}"
API="https://api.github.com"
: "${GH_TOKEN:?GH_TOKEN required}"

env_get() {
    curl -skL -H "Authorization: token ${GH_TOKEN}" \
        -H "Accept: application/vnd.github+json" "$@"
}

# ---- 1. environment 存在且 policy 模式为 custom（protected_branches=false）----
env_get -X PUT "${API}/repos/${OWNER_REPO}/environments/${ENV_NAME}" \
    -d '{"wait_timer":0,"reviewers":[],"deployment_branch_policy":{"protected_branches":false,"custom_branch_policies":true}}' \
    -o /dev/null -w 'environment PUT -> HTTP %{http_code}\n'

# ---- 2. 幂等登记 policies：branch main + tag v* ----
# GET 返回包装结构 {"total_count":N,"branch_policies":[...]}（非裸数组）
list_policies() {
    env_get "${API}/repos/${OWNER_REPO}/environments/${ENV_NAME}/deployment-branch-policies"
}

policy_names() {
    list_policies | python3 -c "
import json, sys
print('\n'.join(sorted({p['name'] for p in json.load(sys.stdin)['branch_policies']})))
"
}

for spec in 'main|branch' 'v*|tag'; do
    name="${spec%%|*}"
    type="${spec##*|}"
    if policy_names | grep -Fxq -- "${name}"; then
        echo "policy exists: ${name} (${type})"
        continue
    fi
    code=$(env_get -o /dev/null -w '%{http_code}' -X POST \
        "${API}/repos/${OWNER_REPO}/environments/${ENV_NAME}/deployment-branch-policies" \
        -d "{\"name\":\"${name}\",\"type\":\"${type}\"}")
    if [ "$code" = "422" ]; then
        # 并发重复登记容忍：复验在册即放行，仍缺则报错
        if policy_names | grep -Fxq -- "${name}"; then
            echo "policy exists (concurrent add): ${name} (${type})"
            continue
        fi
    fi
    if [ "$code" != "200" ] && [ "$code" != "201" ]; then
        echo "::error::POST policy '${name}' (${type}) -> HTTP ${code}"
        exit 1
    fi
    echo "policy added: ${name} (${type})"
done

# ---- 3. 终验 fail-closed：main 与 v* 必须同时在册 ----
MISSING=$(policy_names | python3 -c "
import sys
names = {line.strip() for line in sys.stdin if line.strip()}
missing = {'main', 'v*'} - names
print(','.join(sorted(missing)))
")
if [ -n "${MISSING}" ]; then
    echo "::error::deployment-branch-policies missing: ${MISSING}"
    exit 1
fi
echo "release environment OK: branch=main, tag=v* (custom policy)"
