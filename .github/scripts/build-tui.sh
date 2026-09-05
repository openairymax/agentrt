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
#   - 失败降级契约保留：工具链缺省/网络失败 warn 返回 0，不阻断发布；
#     但 header 就绪后构建若成功，TUI 必须随包（完全体组件完整性）。
#
# 用法（在 agentrt checkout 根执行）：
#   bash .github/scripts/build-tui.sh <tui_src_dir> <out_bin_dir>
#     例：bash .github/scripts/build-tui.sh agent-workload/sdk/tui "$STAGE_DIR/bin"
# ============================================================================
set -u

SRC="${1:?tui 源码目录}"
OUT="${2:?目标 bin 目录}"

# build.rs 依赖的 airy_run_stream.h（agentrt 仓 commons/include/）
if [ ! -f commons/include/airy_run_stream.h ]; then
    echo "warn: commons/include/airy_run_stream.h 缺失（工作树异常），agentrt-tui 跳过"
    exit 0
fi
export AGENTRT_TUI_RUN_STREAM_H="$(pwd)/commons/include/airy_run_stream.h"

export PATH="${HOME}/.cargo/bin:${PATH}"

# cargo 缺失兜底（SSoT，0.1.11 P1）：x86-64 腿切换预构建工具链镜像后不再有
# 独立 rustup 安装 step，各腿（host/容器）统一在此兜底；失败降级契约同上。
if ! command -v cargo >/dev/null 2>&1; then
    echo "[tui] cargo 不在 PATH，安装 rustup（minimal）..."
    if ! curl --proto '=https' --tlsv1.2 -sSf --retry 3 \
         https://sh.rustup.rs | sh -s -- -y --profile minimal; then
        echo "warn: rustup 安装失败，agentrt-tui 跳过（降级，不阻断发布）"
        exit 0
    fi
    export PATH="${HOME}/.cargo/bin:${PATH}"
fi

export CARGO_TARGET_DIR="${CARGO_TARGET_DIR:-${RUNNER_TEMP:-/tmp}/tui-target}"

if ! (cd "$SRC" && cargo build --release); then
    echo "warn: agentrt-tui 构建失败（降级，不阻断发布）"
    exit 0
fi
mkdir -p "$OUT"
if ! cp -f "${CARGO_TARGET_DIR}/release/agentrt-tui" "$OUT/"; then
    echo "warn: agentrt-tui 产物缺失（降级）"
    exit 0
fi
echo "[OK] agentrt-tui -> ${OUT}/agentrt-tui"
