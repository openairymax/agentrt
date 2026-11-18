#!/bin/bash
# SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
# ---------------------------------------------------------------------------
# L-8 离线安装源夹具：现场构建真实可安装的小包（不联网、不污染系统）。
#
# 用法: build_offline_repo.sh <outdir>
# 产物:
#   <outdir>/deb/airyl8-firefox_1.0_all.deb
#   <outdir>/deb/airyl8-tencent-meeting_1.0_all.deb
#   <outdir>/apt/Packages            （本地 .deb 仓库索引，供 apt 形态适配）
#   <outdir>/rpm/*.rpm + repodata    （仅当本机有 rpmbuild/createrepo_c）
#
# 退出码: 0 成功；3 缺少 dpkg-deb（必须显式 SKIP，不得当作通过）。
# ---------------------------------------------------------------------------
set -u
unset LD_LIBRARY_PATH LD_PRELOAD 2>/dev/null || true

OUT=${1:?usage: build_offline_repo.sh <outdir>}
DEB="$OUT/deb"
APT="$OUT/apt"
RPM="$OUT/rpm"
mkdir -p "$DEB" "$APT" "$RPM" || exit 3

if ! command -v dpkg-deb >/dev/null 2>&1; then
    echo "SKIP: dpkg-deb 不可用，无法构建离线 .deb 夹具包" >&2
    exit 3
fi

# build_pkg <pkg> <version> <brand-file-content>
build_pkg() {
    local pkg=$1 ver=$2 brand=$3 root
    root=$(mktemp -d "$OUT/.build.XXXXXX") || return 1
    mkdir -p "$root/DEBIAN" "$root/opt/airyl8/$pkg"
    printf '%s\n' "$brand" > "$root/opt/airyl8/$pkg/version"
    cat > "$root/DEBIAN/control" <<EOF
Package: $pkg
Version: $ver
Section: misc
Priority: optional
Architecture: all
Maintainer: AirymaxRT L-8 <l8@test.invalid>
Description: AirymaxRT L-8 offline install fixture ($pkg)
 Real installable payload used by the end-to-end task loop; no network needed.
EOF
    env -u LD_LIBRARY_PATH dpkg-deb --root-owner-group -b "$root" \
        "$DEB/${pkg}_${ver}_all.deb" >/dev/null 2>&1
    local rc=$?
    rm -rf "$root"
    return $rc
}

rc=0
build_pkg airyl8-firefox 1.0 "AirymaxRT firefox fixture 1.0" || rc=1
build_pkg airyl8-tencent-meeting 1.0 "AirymaxRT tencent-meeting fixture 1.0" || rc=1

# 本地 apt 形态索引：让「离线源」在 deb 侧也有仓库语义（apt 默认不启用）。
( cd "$DEB" && env -u LD_LIBRARY_PATH dpkg-scanpackages . /dev/null 2>/dev/null \
    | gzip -c > "$APT/Packages.gz" ) || true

# rpm/dnf 形态：285H 运行时如具备 rpmbuild/createrepo_c，现场构建同构小包。
if command -v rpmbuild >/dev/null 2>&1; then
    top=$(mktemp -d "$OUT/.rpm.XXXXXX") || exit $rc
    for p in airyl8-firefox airyl8-tencent-meeting; do
        mkdir -p "$top/SPECS" "$top/SOURCES"
        printf 'l8 payload\n' > "$top/SOURCES/$p.txt"
        cat > "$top/SPECS/$p.spec" <<EOF
Name: $p
Version: 1.0
Release: 1
Summary: AirymaxRT L-8 offline fixture
License: AGPL-3.0-or-later
BuildArch: noarch
%description
AirymaxRT L-8 offline install fixture.
%install
mkdir -p %{buildroot}/opt/airyl8/$p
cp %{_sourcedir}/$p.txt %{buildroot}/opt/airyl8/$p/version
%files
/opt/airyl8/$p/version
EOF
        env -u LD_LIBRARY_PATH rpmbuild --define "_topdir $top" -bb \
            "$top/SPECS/$p.spec" >/dev/null 2>&1 && \
            cp "$top"/RPMS/noarch/*.rpm "$RPM/" 2>/dev/null
    done
    rm -rf "$top"
    command -v createrepo_c >/dev/null 2>&1 && \
        env -u LD_LIBRARY_PATH createrepo_c "$RPM" >/dev/null 2>&1 || true
fi

[ $rc -eq 0 ] || { echo "FAIL: 离线夹具包构建失败" >&2; exit 1; }
echo "OK: offline fixtures at $OUT"
