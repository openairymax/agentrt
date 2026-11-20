#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
#
# check-version-drift.sh — 断言安装脚本内嵌的兜底版本与 VERSION 一致
#
# 安装器在「未指定版本且同目录无 VERSION 文件」时使用内嵌兜底版本：
# 单文件分发（curl 下载即用）不携带 VERSION，兜底值就是用户实际装到
# 的版本。主分支推进 VERSION 却漏改兜底值时，单文件安装会静默装回旧
# 版——不报错、不留痕，只在用户侧表现为「装完还是上一版」。判据前移
# 到合并前：两处兜底字面量必须与 VERSION 逐字相等，否则 fail-closed。
#
# 用法: check-version-drift.sh [repo-root]
# 退出: 0 一致 / 1 漂移或字面量缺失 / 2 用法或环境错误
set -uo pipefail

ROOT=$(CDPATH= cd -- "${1:-$(dirname -- "$0")/../..}" && pwd)
VER_FILE="$ROOT/VERSION"
SH_INSTALLER="$ROOT/scripts/install.sh"
PS_INSTALLER="$ROOT/scripts/install.ps1"

die() {
    echo "::error::$*" >&2
    exit 2
}

[ -f "$VER_FILE" ] || die "缺少版本 SSoT: $VER_FILE"
[ -f "$SH_INSTALLER" ] || die "缺少安装器: $SH_INSTALLER"
[ -f "$PS_INSTALLER" ] || die "缺少安装器: $PS_INSTALLER"

version=$(tr -d '[:space:]' < "$VER_FILE")
[ -n "$version" ] || die "VERSION 为空"
want="v$version"

# install.sh 兜底: AIRY_VERSION="${AIRY_VERSION:-vX.Y.Z}"
sh_fallback=$(sed -n 's/.*AIRY_VERSION="\${AIRY_VERSION:-\(v[0-9][0-9.]*\)}".*/\1/p' \
    "$SH_INSTALLER" | head -n 1)
# install.ps1 兜底: else { "vX.Y.Z" }
ps_fallback=$(sed -n 's/.*else { "\(v[0-9][0-9.]*\)" }.*/\1/p' \
    "$PS_INSTALLER" | head -n 1)

rc=0
check() {
    local file="$1" got="$2"

    if [ -z "$got" ]; then
        echo "::error::$file 未找到兜底版本文面量（应为 $want）" >&2
        rc=1
    elif [ "$got" != "$want" ]; then
        echo "::error::$file 兜底版本 $got 与 VERSION 漂移（应为 $want）" >&2
        rc=1
    else
        echo "ok: $file 兜底版本 $got 与 VERSION 一致"
    fi
}

check "scripts/install.sh" "$sh_fallback"
check "scripts/install.ps1" "$ps_fallback"
exit $rc
