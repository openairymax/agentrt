#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
# ============================================================================
# 完全体包 finalize（0.1.11，Linux 腿共享）：运行时 .so 收集 + fail-closed 校验。
#
# 为什么必须此脚本（v0.1.10 社区事故 + 2026-09-05 多轮实证）：
#   - release.yml 容器 job 的 run step 默认 shell 是 dash；lib-package.sh 是
#     bash 函数库，dash 直接 source 报 "Syntax error: (" unexpected（exit 2）。
#     本脚本 shebang bash、由 run step 显式 `bash ...` 调用，规避 dash。
#   - 收集必须发生在「持有自编译产物」的环境：container job 在容器内收；
#     docker run 腿（arm-64/x86-32/riscv/arm-32）在构建容器脚本内收（stage
#     挂载回 host），host 无 /usr/local 自编译库，收集必 not found。
#   - 依赖解析：自编译 deps 装 /usr/local/lib，先注入 LD_LIBRARY_PATH，再
#     pkg_runtime_libs（bin ELF 依赖并集 → lib/），touch .collected 使
#     pkg_verify_deps 以包内 lib/ 前置校验，缺任一依赖 fail-closed 中止。
#   - .collected 不入包（finalize 末尾清理）。
#
# 用法：
#   bash finalize-linux-libs.sh <out_dir> [tools_repo_dir=..] [readelf] [sysroot]
#     out_dir     收集目标（包内顶层目录 或 STAGE_DIR，须含 bin/ 与 lib/）
#     tools_repo  含 scripts/ci/release/lib-package.sh 的仓目录（默认 ..）
#     readelf     交叉腿必填：目标架构 readelf（宿主 ldd 无法分析异架构 ELF）
#     sysroot     交叉腿必填：目标 sysroot（.so 来源，系统核心库不入包）
#   交叉腿（riscv64 等）必须同时给 readelf/sysroot：走 pkg_runtime_libs_cross
#   + pkg_verify_deps_cross；否则走容器内 ldd 路径。
# ============================================================================
set -euo pipefail

OUT="${1:?out_dir（含 bin/ lib/ 的包目录或 STAGE_DIR）}"
TOOLS="${2:-..}"
READELF="${3:-}"
SYSROOT="${4:-}"

if { [ -n "$READELF" ] && [ -z "$SYSROOT" ]; } || \
   { [ -z "$READELF" ] && [ -n "$SYSROOT" ]; }; then
    echo "[finalize] readelf/sysroot 须成对给出（交叉收集需两者）" >&2
    exit 1
fi

if [ ! -d "$OUT/bin" ] || [ ! -d "$OUT/lib" ]; then
    echo "[finalize] 缺 $OUT/bin 或 $OUT/lib，中止" >&2
    exit 1
fi
if [ ! -f "$TOOLS/scripts/ci/release/lib-package.sh" ]; then
    echo "[finalize] 找不到 $TOOLS/scripts/ci/release/lib-package.sh" >&2
    exit 1
fi

# 自编译 deps（cjson/ssl/curl/websockets…）在 /usr/local/lib；容器或 runner 的
# ld.so.cache 未含该路径时 ldd 报 not found，显式注入保证收集与校验可达。
# 交叉腿的自编译 deps 在 <sysroot>/usr/lib/<multiarch>，由 readelf+sysroot
# 路径直达，不依赖宿主 ld.so 解析，故不注入。
export LD_LIBRARY_PATH="/usr/local/lib:${LD_LIBRARY_PATH:-}"

. "$TOOLS/scripts/ci/release/lib-package.sh"

if [ -n "$READELF" ]; then
    pkg_runtime_libs_cross "$OUT" "$SYSROOT" "$READELF"
else
    pkg_runtime_libs "$OUT"
fi
# fail-closed：收集后 lib/ 必须存在运行时库（本仓 daemon 全动态链接）。
# 0.1.11-rc.2 实证：file(1) 在工具链镜像中缺失时，收集/校验的 ELF 守卫
# 曾整体空转——0 个 .so 仍假阳性 "collected + verified" 出包，自编译
# OpenSSL 3.x/cjson 未入包致目标机无法运行。此断言把同类回归转硬失败。
if [ -z "$(ls -A "$OUT/lib" 2>/dev/null | grep -v '^\.' || true)" ]; then
    echo "[finalize] lib/ 无运行时库：收集空转（ELF 判定或 ldd 异常），中止" >&2
    exit 1
fi
if [ -n "$READELF" ]; then
    if ! pkg_verify_deps_cross "$OUT" "$READELF"; then
        echo "[finalize] 交叉产物存在未解析依赖，中止" >&2
        exit 1
    fi
else
    touch "$OUT/lib/.collected"
    pkg_verify_deps "$OUT"
    rm -f "$OUT/lib/.collected"
fi

echo "[finalize] runtime libs collected + verified ($OUT)"
