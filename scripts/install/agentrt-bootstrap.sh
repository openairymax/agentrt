# AgentRT Bootstrap Wrapper
# 委托给 scripts/ops/bin/ 中的正式版本
DIR="$(cd "$(dirname "$0")" && pwd)"
exec "${DIR}/../ops/bin/agentrt-bootstrap.sh" "$@"
