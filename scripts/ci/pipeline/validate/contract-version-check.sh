#!/usr/bin/env bash
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# contract-version-check.sh — 跨团队合约版本变更检测
# P0.14.5: CI 中配置合约版本变更检测
#
# 功能：
#   1. 检测 contracts/ 下合约文件是否被修改
#   2. 提取合约版本号常量（CONTRACT_*_VERSION）
#   3. 与基线版本对比，检测版本是否被 bump
#   4. 合约内容变更但版本未 bump → CI 失败
#   5. 版本已 bump → 通知受影响团队
#
# 用法：
#   bash scripts/ci/pipeline/validate/contract-version-check.sh [--baseline-branch <branch>]
#   bash scripts/ci/pipeline/validate/contract-version-check.sh --baseline-snapshot <snapshot_file>

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"
CONTRACTS_DIR="${PROJECT_ROOT}/contracts"
BASELINE_BRANCH="main"
BASELINE_SNAPSHOT=""
CI_MODE=false

# 颜色
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
BLUE='\033[0;34m'; NC='\033[0m'

log_info()  { echo -e "${BLUE}[CONTRACT]${NC}   $*"; }
log_ok()    { echo -e "${GREEN}[CONTRACT-OK]${NC}  $*"; }
log_warn()  { echo -e "${YELLOW}[CONTRACT-WARN]${NC} $*"; }
log_error() { echo -e "${RED}[CONTRACT-ERR]${NC}  $*" >&2; }

###############################################################################
# 参数解析
###############################################################################
while [[ $# -gt 0 ]]; do
    case "$1" in
        --baseline-branch)
            BASELINE_BRANCH="$2"
            shift 2
            ;;
        --baseline-snapshot)
            BASELINE_SNAPSHOT="$2"
            shift 2
            ;;
        --ci)
            CI_MODE=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --baseline-branch <branch>   Baseline branch for comparison (default: main)"
            echo "  --baseline-snapshot <file>   Use a saved snapshot file instead of git"
            echo "  --ci                          CI mode: stricter checks, exit 1 on violation"
            echo ""
            echo "Description:"
            echo "  Detects contract file changes and validates version bumps."
            echo "  If a contract file's content changed but the version constant"
            echo "  was not bumped, the check fails."
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            exit 1
            ;;
    esac
done

###############################################################################
# 合约文件定义
###############################################################################
declare -A CONTRACT_TEAMS=(
    ["contract_A_B.h"]="team-A,team-B"
    ["contract_A_C.h"]="team-A,team-C"
    ["contract_B_C.h"]="team-B,team-C"
)

declare -A CONTRACT_VERSION_MACRO=(
    ["contract_A_B.h"]="CONTRACT_A_B_VERSION"
    ["contract_A_C.h"]="CONTRACT_A_C_VERSION"
    ["contract_B_C.h"]="CONTRACT_B_C_VERSION"
)

###############################################################################
# 版本号提取函数
###############################################################################
extract_version_hex() {
    local file="$1"
    local macro="$2"
    grep -oP "#define\s+${macro}\s+0x[0-9a-fA-F]+" "$file" 2>/dev/null | \
        grep -oP "0x[0-9a-fA-F]+" || echo "0x00000000"
}

hex_to_semver() {
    local hex="$1"
    hex="${hex#0x}"
    hex="${hex#0X}"
    # 格式: 0x00MAJORMINORPATCH
    local major=$(( (0x${hex} >> 24) & 0xFF ))
    local minor=$(( (0x${hex} >> 16) & 0xFF ))
    local patch=$(( (0x${hex}) & 0xFFFF ))
    echo "${major}.${minor}.${patch}"
}

###############################################################################
# 主检测逻辑
###############################################################################
VERSION_MISMATCH=0
CHANGED_CONTRACTS=()

log_info "=== AgentRT Contract Version Change Detection ==="
log_info "Contracts dir: ${CONTRACTS_DIR}"
log_info "Baseline ref:  ${BASELINE_BRANCH}"
echo ""

# 获取变更的合约文件
if [[ -n "${BASELINE_SNAPSHOT}" ]]; then
    # 使用快照文件比对
    log_info "Comparing against snapshot: ${BASELINE_SNAPSHOT}"
    if [[ ! -f "${BASELINE_SNAPSHOT}" ]]; then
        log_error "Snapshot file not found: ${BASELINE_SNAPSHOT}"
        exit 1
    fi
    CHANGED_FILES=$(diff <(sort "${BASELINE_SNAPSHOT}") \
        <(find "${CONTRACTS_DIR}" -name "contract_*.h" -type f | sort) 2>/dev/null || true)
else
    # 使用 git diff 比对
    if git rev-parse --verify "${BASELINE_BRANCH}" >/dev/null 2>&1; then
        CHANGED_FILES=$(git diff --name-only "${BASELINE_BRANCH}" HEAD -- \
            "contracts/contract_*.h" 2>/dev/null || true)
    elif git rev-parse --verify "origin/${BASELINE_BRANCH}" >/dev/null 2>&1; then
        CHANGED_FILES=$(git diff --name-only "origin/${BASELINE_BRANCH}" HEAD -- \
            "contracts/contract_*.h" 2>/dev/null || true)
    else
        log_warn "Cannot find baseline branch '${BASELINE_BRANCH}', checking all contracts"
        CHANGED_FILES=$(find "${CONTRACTS_DIR}" -name "contract_*.h" -type f 2>/dev/null || true)
    fi
fi

if [[ -z "${CHANGED_FILES}" ]]; then
    log_ok "No contract files changed. Skipping version check."
    exit 0
fi

echo ""
log_info "Changed contract files:"
echo "${CHANGED_FILES}" | while read -r f; do
    [[ -n "$f" ]] && echo "  - $f"
done
echo ""

# 逐文件检查
for contract_file in contract_A_B.h contract_A_C.h contract_B_C.h; do
    filepath="${CONTRACTS_DIR}/${contract_file}"

    if [[ ! -f "$filepath" ]]; then
        log_warn "Contract file not found: ${contract_file} (skipped)"
        continue
    fi

    # 检查此文件是否在变更列表中
    if ! echo "${CHANGED_FILES}" | grep -q "${contract_file}"; then
        log_ok "${contract_file}: not changed"
        continue
    fi

    CHANGED_CONTRACTS+=("${contract_file}")

    macro="${CONTRACT_VERSION_MACRO[${contract_file}]}"
    current_hex=$(extract_version_hex "${filepath}" "${macro}")
    current_ver=$(hex_to_semver "${current_hex}")

    # 获取基线版本
    baseline_hex="0x00000000"
    if [[ -n "${BASELINE_SNAPSHOT}" ]]; then
        baseline_hex=$(grep "^${contract_file}:" "${BASELINE_SNAPSHOT}" 2>/dev/null | \
            cut -d: -f2 || echo "0x00000000")
    elif git rev-parse --verify "${BASELINE_BRANCH}" >/dev/null 2>&1; then
        baseline_content=$(git show "${BASELINE_BRANCH}:${filepath}" 2>/dev/null || true)
        if [[ -n "${baseline_content}" ]]; then
            baseline_hex=$(echo "${baseline_content}" | \
                grep -oP "#define\s+${macro}\s+0x[0-9a-fA-F]+" | \
                grep -oP "0x[0-9a-fA-F]+" || echo "0x00000000")
        fi
    elif git rev-parse --verify "origin/${BASELINE_BRANCH}" >/dev/null 2>&1; then
        baseline_content=$(git show "origin/${BASELINE_BRANCH}:${filepath}" 2>/dev/null || true)
        if [[ -n "${baseline_content}" ]]; then
            baseline_hex=$(echo "${baseline_content}" | \
                grep -oP "#define\s+${macro}\s+0x[0-9a-fA-F]+" | \
                grep -oP "0x[0-9a-fA-F]+" || echo "0x00000000")
        fi
    fi

    baseline_ver=$(hex_to_semver "${baseline_hex}")

    # 检查版本是否被 bump
    if [[ "${current_hex}" == "${baseline_hex}" ]]; then
        # 版本未变更 —— 如果文件内容确实变了（不只是版本号），则报错
        log_error "${contract_file}: VERSION NOT BUMPED!"
        log_error "  Current:  ${current_ver} (0x${current_hex})"
        log_error "  Baseline: ${baseline_ver} (0x${baseline_hex})"
        log_error "  Teams affected: ${CONTRACT_TEAMS[${contract_file}]}"
        log_error "  ACTION: Bump ${macro} before modifying this contract."
        log_error "  Example: #define ${macro} 0x00010001  /* v1.0.1 */"
        echo ""
        ((VERSION_MISMATCH++)) || true
    else
        # 版本已变更
        log_ok "${contract_file}: VERSION BUMPED"
        log_info "  ${baseline_ver} → ${current_ver}"
        log_info "  Notify teams: ${CONTRACT_TEAMS[${contract_file}]}"
        echo ""

        # CI 模式下输出 GitHub Actions 通知
        if [[ "${CI_MODE}" == "true" ]] && [[ -n "${GITHUB_ACTIONS:-}" ]]; then
            echo "::warning title=Contract Version Changed::${contract_file} version bumped: ${baseline_ver} → ${current_ver}. Affected teams: ${CONTRACT_TEAMS[${contract_file}]}"
        fi
    fi
done

###############################################################################
# 生成合约版本快照（供后续 CI 使用）
###############################################################################
SNAPSHOT_FILE="${PROJECT_ROOT}/ci-artifacts/contract-versions.txt"
mkdir -p "$(dirname "${SNAPSHOT_FILE}")"

echo "# AgentRT Contract Versions Snapshot" > "${SNAPSHOT_FILE}"
echo "# Generated: $(date -u +"%Y-%m-%dT%H:%M:%SZ")" >> "${SNAPSHOT_FILE}"

for contract_file in contract_A_B.h contract_A_C.h contract_B_C.h; do
    filepath="${CONTRACTS_DIR}/${contract_file}"
    if [[ -f "$filepath" ]]; then
        macro="${CONTRACT_VERSION_MACRO[${contract_file}]}"
        hex=$(extract_version_hex "${filepath}" "${macro}")
        ver=$(hex_to_semver "${hex}")
        echo "${contract_file}:${hex}:${ver}" >> "${SNAPSHOT_FILE}"
    fi
done

log_info "Contract version snapshot saved to: ${SNAPSHOT_FILE}"

###############################################################################
# 报告小结
###############################################################################
echo ""
echo "=== Contract Version Check Summary ==="
echo "Contracts changed:  ${#CHANGED_CONTRACTS[@]}"
echo "Version mismatches: ${VERSION_MISMATCH}"

if [[ "${#CHANGED_CONTRACTS[@]}" -gt 0 ]]; then
    echo ""
    echo "Changed contracts:"
    for c in "${CHANGED_CONTRACTS[@]}"; do
        echo "  - ${c} (teams: ${CONTRACT_TEAMS[${c}]})"
    done
fi

echo ""

if [[ "${VERSION_MISMATCH}" -gt 0 ]]; then
    log_error "${VERSION_MISMATCH} contract(s) changed without version bump!"
    log_error "Policy: Any change to a contract file MUST bump its version constant."

    if [[ "${CI_MODE}" == "true" ]]; then
        log_error "CI MODE: Failing the build."
        exit 1
    else
        log_warn "Local mode: reporting only (use --ci to fail on violation)."
        exit 0
    fi
else
    log_ok "All changed contracts have valid version bumps."
    exit 0
fi