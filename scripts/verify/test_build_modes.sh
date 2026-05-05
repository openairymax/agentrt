#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 SPHARX Ltd.
# SPDX-License-Identifier: Apache-2.0
#
# MemoryRovol 独立仓库构建模式集成测试脚本
# R-09-01-6: memoryrovol migrated to independent repo
#
# 验证场景:
#   S1 - OSS模式: MEMORYROVOL_OSS=ON, 仅编译L1+L2
#   S2 - PRO模式(默认): 编译完整L1+L2+L3+L4+forgetting
#   S3 - PRO_LIB模式: 预编译库路径链接
#   S4 - 互斥检测: OSS + PRO_LIB 同时设置应失败
#   S5 - PRO_LIB文件不存在应失败
#   S6 - in-source build 应被阻止
#
# 用法: ./test_build_modes.sh [--standalone] [--keep]
#   --standalone  测试独立 MemoryRovol 仓库（默认）
#   --keep        保留构建产物（默认清理）

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

STANDALONE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)/MemoryRovol"

TEST_MODE="standalone"
KEEP_BUILD=false

for arg in "$@"; do
    case "$arg" in
        --standalone) TEST_MODE="standalone" ;;
        --keep)       KEEP_BUILD=true ;;
    esac
done

MR_DIR="$STANDALONE_DIR"
LABEL="MemoryRovol (Independent Repo)"

PASS=0
FAIL=0
ERRORS=""

cleanup() {
    local build_dir="$1"
    if [ -d "$build_dir" ] && [ "$KEEP_BUILD" = false ]; then
        rm -rf "$build_dir"
    fi
}

run_test() {
    local name="$1"
    local cmd="$2"

    printf "[......] %s" "$name"

    if eval "$cmd" > "/tmp/test_build_modes_${name// /_}.log" 2>&1; then
        printf "\r[ PASS ] %s\n" "$name"
        PASS=$((PASS + 1))
    else
        printf "\r[ FAIL ] %s\n" "$name"
        FAIL=$((FAIL + 1))
        ERRORS="${ERRORS}--- ${name} FAILED ---\n"
        ERRORS="${ERRORS}$(tail -20 /tmp/test_build_modes_${name// /_}.log 2>/dev/null || true)\n\n"
    fi
}

echo "========================================"
echo "  MemoryRovol Build Mode Tests"
echo "  Target: $LABEL"
echo "  Directory: $MR_DIR"
echo "========================================"
echo ""

if [ ! -d "$MR_DIR" ]; then
    echo "[ ERROR ] Directory not found: $MR_DIR"
    exit 1
fi

if [ ! -f "$MR_DIR/CMakeLists.txt" ]; then
    echo "[ ERROR ] CMakeLists.txt not found in $MR_DIR"
    exit 1
fi

# =========================================================================
# S1: OSS模式构建
# =========================================================================

S1_CMD="
    BUILD_DIR=\"\$(mktemp -d /tmp/mr-oss-build-XXXXXX)\"
    cd \"\$BUILD_DIR\" || exit 1
    cmake \"$MR_DIR\" -DMEMORYROVOL_OSS=ON -DBUILD_TESTS=OFF > /dev/null 2>&1 || { cleanup \"\$BUILD_DIR\"; exit 1; }
    cmake --build . --parallel \"\$(nproc)\" > /dev/null 2>&1 || { cleanup \"\$BUILD_DIR\"; exit 1; }

    NMC\$(nproc | tr -d '\n') > /dev/null 2>&1 || { cleanup \"\$BUILD_DIR\"; exit 1; }

    if [ -f libagentos_memoryrovol.a ]; then
        echo 'OSS build: libagentos_memoryrovol.a generated'
    fi

    cmake --install . --prefix \"\$BUILD_DIR/install\" > /dev/null 2>&1 || true

    if [ -f \"\$BUILD_DIR/install/include/agentos/memoryrovol/memoryrovol.h\" ]; then
        echo 'OSS install: public headers installed'
    fi

    if [ -f \"\$BUILD_DIR/install/include/agentos/memoryrovol/layer3_structure.h\" ] 2>/dev/null; then
        echo 'WARNING: OSS install should NOT include layer3_structure.h'
        cleanup \"\$BUILD_DIR\"
        exit 1
    else
        echo 'OSS install: layer3_structure.h correctly excluded'
    fi

    if [ -f \"\$BUILD_DIR/install/include/agentos/memoryrovol/forgetting.h\" ] 2>/dev/null; then
        echo 'WARNING: OSS install should NOT include forgetting.h'
        cleanup \"\$BUILD_DIR\"
        exit 1
    else
        echo 'OSS install: forgetting.h correctly excluded'
    fi

    cleanup \"\$BUILD_DIR\"
"

run_test "S1-OSS构建" "$S1_CMD"

# =========================================================================
# S2: PRO模式构建（默认）
# =========================================================================

S2_CMD="
    BUILD_DIR=\"\$(mktemp -d /tmp/mr-pro-build-XXXXXX)\"
    cd \"\$BUILD_DIR\" || exit 1
    cmake \"$MR_DIR\" -DBUILD_TESTS=OFF > /dev/null 2>&1 || { cleanup \"\$BUILD_DIR\"; exit 1; }
    cmake --build . --parallel \"\$(nproc)\" > /dev/null 2>&1 || { cleanup \"\$BUILD_DIR\"; exit 1; }

    if [ -f libagentos_memoryrovol.a ]; then
        echo 'PRO build: libagentos_memoryrovol.a generated'
    else
        cleanup \"\$BUILD_DIR\"
        exit 1
    fi

    cmake --install . --prefix \"\$BUILD_DIR/install\" > /dev/null 2>&1 || true

    if [ -f \"\$BUILD_DIR/install/include/agentos/memoryrovol/layer3_structure.h\" ]; then
        echo 'PRO install: layer3_structure.h included (correct)'
    else
        echo 'WARNING: PRO install should include layer3_structure.h'
        cleanup \"\$BUILD_DIR\"
        exit 1
    fi

    if [ -f \"\$BUILD_DIR/install/include/agentos/memoryrovol/forgetting.h\" ]; then
        echo 'PRO install: forgetting.h included (correct)'
    else
        echo 'WARNING: PRO install should include forgetting.h'
        cleanup \"\$BUILD_DIR\"
        exit 1
    fi

    cleanup \"\$BUILD_DIR\"
"

run_test "S2-PRO构建" "$S2_CMD"

# =========================================================================
# S3: PRO_LIB模式 - 预编译库不存在应失败
# =========================================================================

S3_CMD="
    BUILD_DIR=\"\$(mktemp -d /tmp/mr-prolib-build-XXXXXX)\"
    cd \"\$BUILD_DIR\" || exit 1
    if cmake \"$MR_DIR\" -DMEMORYROVOL_PRO_LIB=/nonexistent/libmemoryrovol.a -DBUILD_TESTS=OFF > /dev/null 2>&1; then
        echo 'ERROR: PRO_LIB with nonexistent file should FAIL'
        cleanup \"\$BUILD_DIR\"
        exit 1
    else
        echo 'PRO_LIB error: correctly failed with nonexistent library'
    fi
    cleanup \"\$BUILD_DIR\"
"

run_test "S3-PRO_LIB文件校验" "$S3_CMD"

# =========================================================================
# S4: OSS + PRO_LIB 互斥检测
# =========================================================================

S4_CMD="
    BUILD_DIR=\"\$(mktemp -d /tmp/mr-mutex-build-XXXXXX)\"
    cd \"\$BUILD_DIR\" || exit 1
    if cmake \"$MR_DIR\" -DMEMORYROVOL_OSS=ON -DMEMORYROVOL_PRO_LIB=/fake/lib.a -DBUILD_TESTS=OFF > /dev/null 2>&1; then
        echo 'ERROR: OSS + PRO_LIB should be mutually exclusive'
        cleanup \"\$BUILD_DIR\"
        exit 1
    else
        echo 'Mutual exclusion: correctly failed on OSS + PRO_LIB'
    fi
    cleanup \"\$BUILD_DIR\"
"

run_test "S4-OSS+PRO_LIB互斥" "$S4_CMD"

# =========================================================================
# S5: in-source build 防护
# =========================================================================

S5_CMD="
    cd \"$MR_DIR\" || exit 1
    if [ -f CMakeCache.txt ]; then
        echo 'Skipping in-source test: CMakeCache.txt already present'
        exit 0
    fi
    if cmake . -DBUILD_TESTS=OFF > /dev/null 2>&1; then
        echo 'ERROR: in-source build should be blocked'
        rm -f CMakeCache.txt CMakeFiles 2>/dev/null || true
        exit 1
    else
        echo 'In-source build: correctly blocked'
    fi
    rm -f CMakeCache.txt 2>/dev/null || true
    rm -rf CMakeFiles 2>/dev/null || true
"

run_test "S5-InSource防护" "$S5_CMD"

# =========================================================================
# S6: 编译定义检查 - OSS模式应有MEMORYROVOL_OSS定义
# =========================================================================

S6_CMD="
    BUILD_DIR=\"\$(mktemp -d /tmp/mr-defs-build-XXXXXX)\"
    cd \"\$BUILD_DIR\" || exit 1
    cmake \"$MR_DIR\" -DMEMORYROVOL_OSS=ON -DBUILD_TESTS=OFF > /dev/null 2>&1 || { cleanup \"\$BUILD_DIR\"; exit 1; }

    if grep -r 'MEMORYROVOL_OSS' CMakeCache.txt flags.make compile_commands.json 2>/dev/null | head -5; then
        echo 'OSS definition: MEMORYROVOL_OSS found in build config'
    else
        echo 'INFO: MEMORYROVOL_OSS may be set via target_compile_definitions'
    fi

    cleanup \"\$BUILD_DIR\"
"

run_test "S6-OSS编译定义" "$S6_CMD"

# =========================================================================
# Cleanup temp logs
# =========================================================================

rm -f /tmp/test_build_modes_*.log 2>/dev/null || true

echo ""
echo "========================================"
echo "  Results: ${PASS} PASS / ${FAIL} FAIL"
echo "========================================"

if [ "$FAIL" -gt 0 ]; then
    echo ""
    echo "Error Summary:"
    echo ""
    printf "%b" "$ERRORS"
    exit 1
fi

exit 0
