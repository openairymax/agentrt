#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
#
# agentrt-tui（Rust TUI）共享构建入口，release.yml 各腿复用。
#
# 用法（在 agentrt checkout 根执行）：
#   bash .github/scripts/build-tui.sh <tui_src_dir> <out_bin_dir>
#     例：bash .github/scripts/build-tui.sh agent-workload/sdk/tui "$STAGE_DIR/bin"
#
# 契约：
#   - tui 经共享协议客户端 agentrt-rs 通信，wire 常量由 agentrt-rs 的
#     build.rs 从唯一 C 头 commons/include/airy_run_stream.h 生成，故
#     tui 源码的同级目录必须存在 sdk-rust（path 依赖 ../sdk-rust）；
#   - 版本 SSoT 为 agentrt/VERSION，经 AIRY_RT_VERSION_FILE 显式注入；
#   - CARGO_TARGET_DIR 指向 runner temp，源码树不落构建产物；
#   - 失败契约：AIRY_TUI_FAIL_HARD=1（门禁产品腿）时任一缺件即中止，
#     默认 0 保持降级（exit 0），仅供非产品载体（如 canary 腿）。
set -u

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

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

export AIRY_RT_VERSION_FILE="${ROOT}/VERSION"
[ -f "$AIRY_RT_VERSION_FILE" ] || { [ "$FAIL_HARD" = 1 ] && _fail "VERSION 文件缺失: $AIRY_RT_VERSION_FILE"; }

RS_SRC="$(cd "$(dirname "$SRC")" && pwd)/sdk-rust"
[ -f "${RS_SRC}/Cargo.toml" ] || _fail "agentrt-rs 源码目录缺失: ${RS_SRC}（tui 依赖 ../sdk-rust）"

if [ ! -f commons/include/airy_run_stream.h ]; then
    _fail "commons/include/airy_run_stream.h 缺失（工作树异常）"
fi
export AGENTRT_RS_RUN_STREAM_H="$(pwd)/commons/include/airy_run_stream.h"

export PATH="${HOME}/.cargo/bin:${PATH}"

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

# 发布完整性门禁：fail-hard 腿断言运行时库 FFI 分支字符串不存在——
# 禁止 TUI 直连 daemon/运行时库的发布侧机器化验证，与 tui build.rs
# 白名单空集（fail-closed panic）构成构建期 + 发布期双保险。
# 判据用 FFI 分支专属运行时字符串（release 已 strip，符号级判据恒假阴性）。
if [ "$FAIL_HARD" = "1" ]; then
    if command -v strings >/dev/null 2>&1; then
        if strings "$OUT/agentrt-tui" 2>/dev/null | grep -q 'ime: dict loaded:'; then
            _fail "agentrt-tui 残留运行时库 FFI 分支（违反 gateway-only 铁律）——发布完整性门禁"
        fi
    fi
fi

echo "[OK] agentrt-tui -> ${OUT}/agentrt-tui"
