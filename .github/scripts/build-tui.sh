#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
# ============================================================================
# agentrt-tui（Rust TUI）共享构建入口（0.1.11，release.yml 各腿复用 SSoT）。
#
# 背景（v0.1.10 社区事故）：
#   - 早期各腿内联 cargo build，缺 AGENTRT_TUI_RUN_STREAM_H 时 build.rs 按伞仓
#     布局默认相对路径（../../agentrt/commons/...）找不到 airy_run_stream.h，
#     日志实证 "run_stream schema header not found"，warn 降级后包内无 TUI。
#   - 本脚本把 header 路径显式注入（本 checkout 直布局 commons/include/），
#     CARGO_TARGET_DIR 指向 runner temp（禁止源码树落盘，铁律 4.7）。
#   - 失败契约：AIRY_TUI_FAIL_HARD=1（门禁产品腿）时任一缺件即中止——TUI 自
#     0.1.13 #11 起属发布必检（publish REQUIRED_SUBTREE 断言 bin/agentrt-tui），
#     缺件即产品缺件，早失败省整条 rc run（U11 实证：arm-32 TUI 曾静默缺失）；
#     默认 0 保持降级（exit 0），仅供 riscv canary 等非产品载体。
#
# 用法（在 agentrt checkout 根执行）：
#   bash .github/scripts/build-tui.sh <tui_src_dir> <out_bin_dir>
#     例：bash .github/scripts/build-tui.sh agent-workload/sdk/tui "$STAGE_DIR/bin"
# ============================================================================
set -u

# B1（2026-09-07）：fail-hard 开关见头部契约注释。
FAIL_HARD="${AIRY_TUI_FAIL_HARD:-0}"
_fail() {
    if [ "$FAIL_HARD" = "1" ]; then
        echo "::error::agentrt-tui: $1（bin/agentrt-tui 为发布完整性必需）"
        exit 1
    fi
    echo "warn: agentrt-tui: $1（降级，不阻断发布）"
    exit 0
}

SRC="${1:?tui 源码目录}"
OUT="${2:?目标 bin 目录}"

# agentrt checkout 根（本脚本位于 <root>/.github/scripts/）
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

# 版本 SSoT 注入（0.1.14 R3 修复）：CI 兄弟仓布局下 build.rs 的伞仓相对
# 路径 ../../agentrt/VERSION 不存在，曾致 TUI 横幅回落 Cargo 版本（实机
# 实证 "TUI v0.1.9" vs 系统 0.1.13）。
export AIRY_RT_VERSION_FILE="${ROOT}/VERSION"
[ -f "$AIRY_RT_VERSION_FILE" ] || { [ "$FAIL_HARD" = 1 ] && _fail "VERSION 文件缺失: $AIRY_RT_VERSION_FILE"; }

# IME 链接库注入（0.1.14 R4 修复）：CI 构建目录在源码树外（BAN-33），
# build.rs 四候选路径全部落空 → 发布 TUI 从未链接 airy_ime（内置输入法
# 实机全灭的根因）。按腿构建目录定位 libairy_common.a 并导出
# AIRY_COMMON_LIB（build.rs 首选候选）。
if [ -z "${AIRY_COMMON_LIB:-}" ]; then
    for cand in \
        "${AIRY_COMMON_LIB:-}" \
        "${BUILD_DIR:+$ROOT/${BUILD_DIR#/}/commons/libairy_common.a}" \
        "${BUILD_DIR:+${BUILD_DIR#/}}/commons/libairy_common.a" \
        "$ROOT/../build/commons/libairy_common.a"; do
        [ -n "$cand" ] && [ -f "$cand" ] && export AIRY_COMMON_LIB="$cand" && break
    done
fi
if [ -z "${AIRY_COMMON_LIB:-}" ]; then
    if [ "$FAIL_HARD" = "1" ]; then
        _fail "libairy_common.a 未定位（AIRY_COMMON_LIB/BUILD_DIR 均未生效）——发布 TUI 必须携带内置输入法"
    fi
    echo "warn: agentrt-tui: libairy_common.a 未定位，IME 将降级禁用"
fi

# build.rs 依赖的 airy_run_stream.h（agentrt 仓 commons/include/）
if [ ! -f commons/include/airy_run_stream.h ]; then
    _fail "commons/include/airy_run_stream.h 缺失（工作树异常）"
fi
export AGENTRT_TUI_RUN_STREAM_H="$(pwd)/commons/include/airy_run_stream.h"

export PATH="${HOME}/.cargo/bin:${PATH}"

# cargo 缺失兜底（SSoT，0.1.11 P1）：x86-64 腿切换预构建工具链镜像后不再有
# 独立 rustup 安装 step，各腿（host/容器）统一在此兜底。
if ! command -v cargo >/dev/null 2>&1; then
    echo "[tui] cargo 不在 PATH，安装 rustup（minimal）..."
    if ! curl --proto '=https' --tlsv1.2 -sSf --retry 3 \
         https://sh.rustup.rs | sh -s -- -y --profile minimal; then
        _fail "rustup 安装失败"
    fi
    export PATH="${HOME}/.cargo/bin:${PATH}"
fi

export CARGO_TARGET_DIR="${CARGO_TARGET_DIR:-${RUNNER_TEMP:-/tmp}/tui-target}"

if ! (cd "$SRC" && cargo build --release); then
    _fail "cargo build --release 失败"
fi
mkdir -p "$OUT"
if ! cp -f "${CARGO_TARGET_DIR}/release/agentrt-tui" "$OUT/"; then
    _fail "构建产物 agentrt-tui 缺失"
fi

# 发布完整性门禁（0.1.14 R4）：fail-hard 腿断言 IME 符号真实链接——
# 防止"构建绿但输入法静默缺失"再次流入发布物（v0.1.13 实机实证）。
if [ "$FAIL_HARD" = "1" ]; then
    if command -v nm >/dev/null 2>&1; then
        if ! nm "$OUT/agentrt-tui" 2>/dev/null | grep -q 'airy_ime_load'; then
            _fail "agentrt-tui 未链接 airy_ime（内置输入法缺失）——发布完整性门禁"
        fi
    fi
fi

echo "[OK] agentrt-tui -> ${OUT}/agentrt-tui"
