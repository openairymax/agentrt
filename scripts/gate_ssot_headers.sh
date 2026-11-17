#!/usr/bin/env bash
# gate:ci 跨仓 [SC] 头文件一致性（0.1.21 C-15 判据）。
#
# [SC] 共享头文件的物理宿主是 kernel UAPI 目录
# (agent-linux/kernel/include/uapi/linux/airymax)。用户态仓 agentrt 以
# 同一路径镜像（commons/include/linux/airymax），文件逐字节相同——
# 不允许任何改编：契约漂移会直接改掉 sha256。
#
# 唯一例外是本仓自有的 task_desc.h（用户态任务描述，非 [SC] 契约）。
#
# 退出码：0 全部一致；1 存在漂移/缺失；2 环境不满足（缺 kernel 源树）。

set -u

PASS=0
FAIL=0
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
USER_DIR="$ROOT/commons/include/linux/airymax"

# agentrt 自有、非 [SC] 共享的头文件（不参与跨仓比对）。
OWNED_HEADERS="task_desc.h"

SRC="${AIRY_SC_HEADER_SRC:-}"
if [ -z "$SRC" ]; then
    for _cand in \
        "$ROOT/../../agent-linux/kernel/include/uapi/linux/airymax" \
        "$ROOT/../agent-linux/kernel/include/uapi/linux/airymax" \
        "$ROOT/../../../agent-linux/kernel/include/uapi/linux/airymax"; do
        if [ -d "$_cand" ]; then SRC="$_cand"; break; fi
    done
fi

if [ -z "$SRC" ] || [ ! -d "$SRC" ]; then
    printf 'SKIP gate:ci [SC] headers: kernel UAPI dir not found (set AIRY_SC_HEADER_SRC)\n'
    exit 2
fi

printf '[gate:ci] [SC] header sha256: %s <-> %s\n' "$USER_DIR" "$SRC"

for path in "$USER_DIR"/*.h; do
    name="$(basename "$path")"
    case " $OWNED_HEADERS " in
        *" $name "*) continue ;;
    esac
    if [ ! -f "$SRC/$name" ]; then
        printf '  [FAIL] %s: no kernel-side counterpart\n' "$name"
        FAIL=$((FAIL+1))
        continue
    fi
    uh="$(sha256sum "$path" | cut -d' ' -f1)"
    kh="$(sha256sum "$SRC/$name" | cut -d' ' -f1)"
    if [ "$uh" = "$kh" ]; then
        printf '  [PASS] %s %s\n' "$name" "${uh:0:16}"
        PASS=$((PASS+1))
    else
        printf '  [FAIL] %s drift (user %s kernel %s)\n' "$name" "${uh:0:16}" "${kh:0:16}"
        FAIL=$((FAIL+1))
    fi
done

printf '[gate:ci] [SC] headers: %d passed, %d failed\n' "$PASS" "$FAIL"
[ "$FAIL" -eq 0 ]
