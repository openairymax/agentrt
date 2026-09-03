#!/usr/bin/env bash
#
# init-submodules.sh — agentrt 仓 CI 用依赖布局准备
#
# agentrt 现为 workflow 宿主仓（发布平面），本脚本取代原伞仓同名脚本。
# 语义差异：
#   原伞仓：在伞仓工作树初始化 umbrella 子模块（agent-workload/tools），
#           agentrt 只是 agent-workload 的嵌套子模块，需递归两层。
#   现 agentrt 仓：agentrt 即工作树根，其 .gitmodules 直接列出 7 个叶子
#           （atoms/commons/cupolas/daemons/gateway/heapstore/protocols），
#           在根上执行 submodule update 一次即可。
#
# 私有子仓（atoms 在 GitHub 为 private）：GH_TOKEN（org PAT）写入 ~/.netrc，
# 避免匿名 404（与伞仓方案一致，见原 init-submodules.sh）。
#
# 用法（保持伞仓 workflow 的调用形式不变，脚本内部按参数重映射）：
#   bash .github/scripts/init-submodules.sh agent-workload
#        → 仅初始化 7 个叶子子模块（build-test / codegen-check 用）。
#   bash .github/scripts/init-submodules.sh agent-workload tools
#        → 初始化叶子 + 克隆 release 打包所需的 sibling 数据到历史相对路径：
#          tools/（发布/签名脚本、bootstrap、config 模板）、
#          agent-workload/{sdk,ecosystem}（TUI 源、Python 运行时、manager 配置）。
#          —— sdk/ecosystem/tools 在 agentrt 树外（分属独立仓），以镜像期数据
#          补齐，使打包/发布脚本的路径引用无需改动。

set -uo pipefail

export GIT_TERMINAL_PROMPT=0

# PAT 认证走 ~/.netrc（git https 原生凭据机制），不改写 URL。
if [ -n "${GH_TOKEN:-}" ]; then
  printf 'machine github.com\nlogin x-access-token\npassword %s\n' \
    "$GH_TOKEN" > "$HOME/.netrc"
  chmod 600 "$HOME/.netrc"
  echo "auth: ~/.netrc written for github.com (GH_TOKEN)"
else
  echo "auth: GH_TOKEN not set, private submodule (atoms) may fail"
fi

# 1) 叶子子模块（agentrt 直接子模块，SHA 由 agentrt 树 gitlink 钉定）
git submodule update --init --recursive
rc=$?

# 2) sibling 数据（仅当参数含 tools 时按需补齐，即 release 链）
want_layout=false
for a in "$@"; do
  [ "$a" = "tools" ] && want_layout=true
done

clone_sibling() {
  local name="$1" dir="$2"
  if [ -e "$dir" ]; then
    echo "layout: $dir already present"
    return 0
  fi
  if git clone -q --depth 1 "https://github.com/openairymax/${name}.git" "$dir" 2>/dev/null; then
    echo "layout: cloned github.com/openairymax/${name}.git -> $dir"
    return 0
  fi
  rm -rf "$dir"
  if [ -n "${GH_TOKEN:-}" ]; then
    if git clone -q --depth 1 \
        "https://x-access-token:${GH_TOKEN}@github.com/openairymax/${name}.git" "$dir" 2>/dev/null; then
      echo "layout: cloned (token) github.com/openairymax/${name}.git -> $dir"
      return 0
    fi
    rm -rf "$dir"
  fi
  echo "::warning::layout: clone ${name} failed — 打包完整度可能降级（pack 步骤多为 || true 容错）"
  return 1
}

if [ "$want_layout" = true ]; then
  echo "layout: cloning release sibling repos (_tools/sdk/ecosystem)"
  # 注意：tools 克隆到 _tools/ —— agentrt 自带顶层 tools/（codegen 等），
  # 若同名会跳过克隆导致 release 脚本缺失（linux job 实证）。
  clone_sibling tools _tools
  mkdir -p agent-workload
  clone_sibling sdk agent-workload/sdk
  clone_sibling ecosystem agent-workload/ecosystem
fi

echo "--- submodule status ---"
git submodule status --recursive | sed 's/^/  /' | head -80
exit "$rc"
