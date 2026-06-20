#!/usr/bin/env bash
# ──────────────────────────────────────────────────────────
# AgentRT 5-Minute QuickStart Script
# Version: 0.1.1
# ──────────────────────────────────────────────────────────
set -euo pipefail

BOLD='\033[1m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
RED='\033[0;31m'
NC='\033[0m'

info()  { echo -e "${CYAN}[INFO]${NC} $*"; }
ok()    { echo -e "${GREEN}[OK]${NC} $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $*"; }
fail()  { echo -e "${RED}[FAIL]${NC} $*"; exit 1; }

AGENTRT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
EXAMPLE_NAME="${1:-hello-agent}"
TARGET_DIR="${2:-./my-agent-project}"

echo ""
echo -e "${BOLD}═══════════════════════════════════════════════${NC}"
echo -e "${BOLD}   AgentRT v0.1.1 — 5-Minute QuickStart${NC}"
echo -e "${BOLD}═══════════════════════════════════════════════${NC}"
echo ""

# ── Step 1: 检查环境 ─────────────────────────────────────
info "Step 1/5: 检查运行环境..."

check_cmd() {
    if command -v "$1" &>/dev/null; then
        ok "$1 已安装"
        return 0
    else
        warn "$1 未安装"
        return 1
    fi
}

ENV_OK=true
check_cmd python3 || ENV_OK=false
check_cmd cargo   || ENV_OK=false
check_cmd git     || ENV_OK=false

if [ "$ENV_OK" = false ]; then
    warn "部分依赖缺失，示例项目仍可创建，但运行可能需要安装缺失依赖"
fi

echo ""

# ── Step 2: 选择示例项目 ─────────────────────────────────
info "Step 2/5: 选择示例项目: ${BOLD}${EXAMPLE_NAME}${NC}"

EXAMPLES_DIR="${AGENTRT_DIR}/examples"
if [ ! -d "${EXAMPLES_DIR}/${EXAMPLE_NAME}" ]; then
    fail "示例项目 '${EXAMPLE_NAME}' 不存在。可用项目:"
    ls -1 "${EXAMPLES_DIR}/" 2>/dev/null | sed 's/^/  - /'
fi

ok "找到示例项目: ${EXAMPLE_NAME}"
echo ""

# ── Step 3: 创建项目 ─────────────────────────────────────
info "Step 3/5: 创建项目目录: ${TARGET_DIR}"

if [ -d "${TARGET_DIR}" ]; then
    warn "目录 ${TARGET_DIR} 已存在，跳过创建"
else
    mkdir -p "${TARGET_DIR}"
    ok "目录已创建"
fi

# 复制示例项目文件
cp -r "${EXAMPLES_DIR}/${EXAMPLE_NAME}/"* "${TARGET_DIR}/" 2>/dev/null || true
ok "示例文件已复制到 ${TARGET_DIR}"
echo ""

# ── Step 4: 初始化配置 ───────────────────────────────────
info "Step 4/5: 初始化项目配置..."

# 如果没有 config.yaml，创建一个默认的
if [ ! -f "${TARGET_DIR}/config.yaml" ]; then
    cat > "${TARGET_DIR}/config.yaml" << 'YAML'
# AgentRT 项目配置
gateway:
  url: "http://localhost:8080"
  timeout_ms: 30000

llm:
  default_model: "gpt-4"
  temperature: 0.7
  max_tokens: 4096

memory:
  enabled: true
  layers:
    L1: { type: "working",   capacity: 8192  }
    L2: { type: "session",   capacity: 32768 }
    L3: { type: "long_term", capacity: 1048576 }
    L4: { type: "rule",      capacity: 1024  }

hooks:
  enabled: true

logging:
  level: "info"
  format: "json"
YAML
    ok "默认配置已生成"
else
    ok "配置文件已存在"
fi

# 创建 agents 目录（如果不存在）
mkdir -p "${TARGET_DIR}/agents"

# 如果没有 agent 定义，创建一个默认的
if [ ! -f "${TARGET_DIR}/agents/main.agent.yaml" ] && [ -z "$(ls -A "${TARGET_DIR}/agents/" 2>/dev/null)" ]; then
    cat > "${TARGET_DIR}/agents/main.agent.yaml" << 'YAML'
name: "my-agent"
version: "0.1.0"
model: "gpt-4"

system_prompt: |
  你是一个智能助手，由 AgentRT 驱动。
  请用简洁、准确的方式回答用户的问题。

tools: []
skills: []
hooks: []
YAML
    ok "默认 Agent 定义已生成"
fi

echo ""

# ── Step 5: 验证与提示 ───────────────────────────────────
info "Step 5/5: 验证项目结构..."

FILE_COUNT=$(find "${TARGET_DIR}" -type f | wc -l)
ok "项目包含 ${FILE_COUNT} 个文件"

echo ""
echo -e "${BOLD}═══════════════════════════════════════════════${NC}"
echo -e "${GREEN}${BOLD}   QuickStart 完成！${NC}"
echo -e "${BOLD}═══════════════════════════════════════════════${NC}"
echo ""
echo -e "项目目录: ${CYAN}${TARGET_DIR}${NC}"
echo ""
echo -e "${BOLD}下一步:${NC}"
echo ""
echo -e "  1. 查看项目结构:"
echo -e "     ${CYAN}cd ${TARGET_DIR} && find . -type f${NC}"
echo ""
echo -e "  2. 阅读 README:"
echo -e "     ${CYAN}cat README.md${NC}"
echo ""
echo -e "  3. 启动 Gateway (需要先构建 AgentRT):"
echo -e "     ${CYAN}cd ${AGENTRT_DIR} && make run-gateway${NC}"
echo ""
echo -e "  4. 运行 Agent:"
echo -e "     ${CYAN}agentrt run --agent-file agents/main.agent.yaml${NC}"
echo ""
echo -e "  5. 尝试其他示例:"
echo -e "     ${CYAN}./scripts/ops/bin/quickstart.sh weather-agent ./my-weather-agent${NC}"
echo ""
echo -e "${BOLD}可用示例项目:${NC}"
for d in "${EXAMPLES_DIR}"/*/; do
    name=$(basename "$d")
    echo -e "  - ${CYAN}${name}${NC}"
done
echo ""
