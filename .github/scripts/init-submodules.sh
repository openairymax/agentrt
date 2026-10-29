#!/usr/bin/env bash
#
# init-submodules.sh — agentrt 仓 CI 用依赖布局准备
#
# agentrt 现为 workflow 宿主仓（发布平面），本脚本取代原伞仓同名脚本。
# 语义差异：
#   原伞仓：在伞仓工作树初始化 umbrella 子模块（agent-workload/tools），
#           agentrt 只是 agent-workload 的嵌套子模块，需递归两层。
#   现 agentrt 仓：agentrt 即工作树根，其 .gitmodules 直接列出 6 个叶子
#           （atoms/commons/daemons/gateway/heapstore/protocols），
#           在根上执行 submodule update 一次即可（§254a 起 cupolas 挂点
#           迁出至 products 装配仓，不在本树列出）。
#
# 私有子仓（atoms 在 GitHub 为 private）：GH_TOKEN（org PAT）写入 ~/.netrc，
# 避免匿名 404（与伞仓方案一致，见原 init-submodules.sh）。
#
# 用法（保持伞仓 workflow 的调用形式不变，脚本内部按参数重映射）：
#   bash .github/scripts/init-submodules.sh agent-workload
#        → 仅初始化 6 个叶子子模块（build-test / codegen-check 用）。
#   bash .github/scripts/init-submodules.sh agent-workload tools
#        → 初始化叶子 + 补齐 release 打包所需的 sibling 数据到历史相对路径：
#          tools/（发布/签名脚本、bootstrap、config 模板）、
#          agent-workload/{sdk,ecosystem,products}（TUI 源、Python 运行时、
#          manager 配置、cupolas 安全穹顶源）。
#          —— sdk/ecosystem/tools 在 agentrt 树外（分属独立仓），以镜像期数据
#          补齐，使打包/发布脚本的路径引用无需改动。
#
# 快照同源（0.1.20）：装配输入 sdk/products 不再跟随 main tip，而是解析
# lf-gate-fixture 的 *_sha default（全仓唯一字面量），使「门禁验证的快照」
# 与「发行装配的快照」必然同一。tip 克隆时上游一推进即分叉：门禁判绿来自
# pin、二进制产自 tip，两边的结论互不背书，且发布内容不可复现。解析或校验
# 失败即 fail-closed，严禁回退 tip 克隆（静默回退正是分叉的成因）。
# 仍跟 tip 的两腿及理由：tools 是发布/签名脚本与 airy_depgraph 的宿主，钉旧
# 会使发布工具链落后于脚本修正；ecosystem 是策略载荷（manager 配置、运行时
# 策略、sanitizer suppressions），按机制/策略分离可随生态推进，不入机制核
# 判据面。

set -uo pipefail

export GIT_TERMINAL_PROMPT=0

FIXTURE=.github/actions/lf-gate-fixture/action.yml

# 需要物化的子模块路径清单（跳过 update=none 的可选项，如 products 的
# memoryrovol 商业件——门禁数据集不取，发布腿亦不校验）。
sub_paths() {
  git -C "$1" config --file .gitmodules \
    --get-regexp '^submodule\..*\.path$' 2>/dev/null |
    while read -r key path; do
      name=${key#submodule.}; name=${name%.path}
      [ "$(git -C "$1" config --file .gitmodules \
             "submodule.$name.update" 2>/dev/null)" = "none" ] && continue
      printf '%s\n' "$path"
    done
}

# 登记子模块的工作树必须真正 populate——「已初始化」不等于「有内容」：
# 克隆成功而 checkout 中止时目录为空而状态仍显示就绪（2026-10-29
# run 40880211714 实证：protocols 空树，CMake 静默跳过，13 门连锁红）。
# 故按内容判据把故障挡在 configure 之前，不以「后续门禁红」延迟暴露。
empty_paths() {
  local dir="$1" p out=""
  for p in $(sub_paths "$dir"); do
    [ -n "$(ls -A "$dir/$p" 2>/dev/null)" ] || out="$out $dir/$p"
  done
  printf '%s\n' "${out# }"
}

# 解析 lf-gate-fixture 内某 input 的 default 作为唯一快照来源（SSoT）。
fixture_pin() {
  local v
  v=$(awk -v k="$1" '
        $0 ~ ("^  " k ":") { f = 1; next }
        f && /^  [^ ]/     { exit }
        f && /^    default:/ { sub(/^ *default: */, ""); print; exit }
      ' "$FIXTURE" 2>/dev/null | tr -d "\"' ")
  case "$v" in
    '' | *[!0-9a-f]*) return 1 ;;
  esac
  [ "${#v}" -ge 40 ] || return 1
  printf '%s\n' "$v"
}

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
#
# 竞态容错：agentrt main 推上 GitHub 的瞬间，sync-mirror workflow 才刚开始
# 从 atomgit 镜像叶子仓（agentrt 排在叶子之后同步，见 sync-mirror.sh 注释）。
# 若本步先于镜像完成执行，gitlink 钉定的叶子 SHA 尚不在 GitHub（"remote
# error: ... not found"），submodule update 立即失败（exit 128，19:05 run
# 实证：build-test/codegen 三作业同一秒全挂）。叶子仓往往在数分钟内补齐，
# 故对失败做有限重试（每次重跑幂等），而非一次失败即红。
init_rc=1
for attempt in 1 2 3 4 5 6; do
  # sync 重算 .git/modules 内的 URL，避免镜像切换后沿用旧地址；
  # --force 保证重试时重新 checkout（首次 checkout 中止会留下空工作树，
  # 而 git 视其为已初始化，幂等重跑不会再动它）。
  git submodule sync --recursive >/dev/null 2>&1 || true
  git submodule update --init --recursive --force
  init_rc=$?
  if [ "$init_rc" -eq 0 ]; then
    break
  fi
  echo "warn: submodule update failed (rc=$init_rc), retry $attempt/6 in 30s"
  sleep 30
done
rc=$init_rc

# 1b) fail-closed：叶子工作树必须真正 populate（判据见 empty_paths）
missing=$(empty_paths .)
if [ -n "$missing" ]; then
  echo "::error::submodule worktree empty:$missing"
  rc=1
fi

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
  # 0.1.11 完全体组件回归修复：sdk 仓的 tui/sdk-python 等为嵌套子模块
  #（../tui.git 相对 URL），浅克隆不带 --recurse-submodules 时这些目录为空，
  # cargo build 报 "could not find Cargo.toml"、Python 运行时缺失（v0.1.10
  # 实证：agentrt-tui 缺失 + lib/ 无 sdk-python）。--shallow-submodules 保持
  # 各嵌套子模块浅拉，避免全量历史。
  if git clone -q --depth 1 --recurse-submodules --shallow-submodules \
      "https://github.com/openairymax/${name}.git" "$dir" 2>/dev/null; then
    echo "layout: cloned github.com/openairymax/${name}.git (recursive) -> $dir"
    return 0
  fi
  rm -rf "$dir"
  if [ -n "${GH_TOKEN:-}" ]; then
    if git clone -q --depth 1 --recurse-submodules --shallow-submodules \
        "https://x-access-token:${GH_TOKEN}@github.com/openairymax/${name}.git" "$dir" 2>/dev/null; then
      echo "layout: cloned (token, recursive) github.com/openairymax/${name}.git -> $dir"
      return 0
    fi
    rm -rf "$dir"
  fi
  echo "::warning::layout: clone ${name} failed — 打包完整度可能降级（pack 步骤多为 || true 容错）"
  return 1
}

# 按 SHA 物化装配输入（与 lf-gate-fixture 同一取料语义）：--no-checkout 建壳、
# fetch 指定 SHA 再 checkout，得到 detached 锁定快照；嵌套 gitlink（sdk→tui、
# products→cupolas）随该 commit 记录的 gitlink 检出。末三行是决定性断言——
# HEAD 必须等于 pin、需物化的子模块不得空树，否则发行装配的快照与门禁验证的
# 快照不同一，本脚本宁红不降级。认证复用脚本顶部写入的 ~/.netrc。
pin_sibling() {
  local name="$1" dir="$2" sha="$3" miss
  if [ -e "$dir" ]; then
    echo "layout: $dir already present"
    return 0
  fi
  git clone -q --no-checkout --depth 1 \
    "https://github.com/openairymax/${name}.git" "$dir" || return 1
  git -C "$dir" fetch -q --depth 1 origin "$sha" || return 1
  git -C "$dir" checkout -q "$sha" || return 1
  [ "$(git -C "$dir" rev-parse HEAD)" = "$sha" ] || return 1
  git -C "$dir" submodule update --init --recursive --depth 1 || return 1
  miss=$(empty_paths "$dir")
  if [ -n "$miss" ]; then
    echo "::error::pin: submodule worktree empty:$miss"
    return 1
  fi
  echo "layout: pinned ${name}@$(printf '%.7s' "$sha") -> $dir"
}

if [ "$want_layout" = true ]; then
  echo "layout: materializing release sibling data (_tools/ecosystem + pinned sdk/products)"
  # 注意：tools 克隆到 _tools/ —— agentrt 自带顶层 tools/（codegen 等），
  # 若同名会跳过克隆导致 release 脚本缺失（linux job 实证）。
  clone_sibling tools _tools
  mkdir -p agent-workload
  clone_sibling ecosystem agent-workload/ecosystem
  # 装配输入按 pin 物化（§快照同源）：pin 解析自 lf-gate-fixture，
  # 解析或检出失败即置 rc=1，严禁回退 tip 克隆。
  for _leg in sdk products; do
    _pin=$(fixture_pin "${_leg}_sha") || {
      echo "::error::layout: cannot resolve ${_leg}_sha from $FIXTURE"
      rc=1
      continue
    }
    pin_sibling "$_leg" "agent-workload/$_leg" "$_pin" || {
      echo "::error::layout: materialize ${_leg}@$_pin failed"
      rc=1
    }
  done
fi

echo "--- submodule status ---"
git submodule status --recursive | sed 's/^/  /' | head -80
exit "$rc"
