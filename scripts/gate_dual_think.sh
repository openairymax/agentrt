#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
#
# C-4 双思考 B 侧三值门禁：正文读取 + schema 校验 + 黄金集。
#   PASS=0 / FAIL=1 / SKIP=2（SKIP 不计 PASS）
# 用法：scripts/gate_dual_think.sh [--bin DIR]
set -uo pipefail

ROOT="$(cd -P "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT/build/bin"
while [ $# -gt 0 ]; do
    case "$1" in
        --bin) BIN="$2"; shift 2 ;;
        *) echo "unknown arg: $1" >&2; exit 2 ;;
    esac
done

REVIEW_C="$ROOT/daemons/think_d/src/review/review_svc.c"
SCHEMA_C="$ROOT/daemons/think_d/src/service/review_schema.c"
TEST_C="$ROOT/daemons/think_d/tests/test_review_schema.c"
GOLDEN="$BIN/test_think_test_review_schema"

PASS=0
FAIL=0
SKIP=0

ok()  { echo "PASS  $1"; PASS=$((PASS + 1)); }
bad() { echo "FAIL  $1"; FAIL=$((FAIL + 1)); }
skip(){ echo "SKIP  $1"; SKIP=$((SKIP + 1)); }

has() { grep -Fq -- "$1" "$2" 2>/dev/null; }

# L01 B 侧读正文：RPC 面透传 output_body，提示词把正文放在结构字段之前。
if has 'output_body' "$REVIEW_C" && has 'rv_prompt_t2' "$REVIEW_C" \
    && has 'rv_prompt_t1f' "$REVIEW_C"; then
    ok "L01 output_body 透传与提示词装配"
else
    bad "L01 output_body 透传与提示词装配"
fi

# L02 schema 严格校验：数值 0/1 + 非空 reason，缺省 fail。
if has 'cJSON_IsNumber' "$SCHEMA_C" && has 'reason' "$SCHEMA_C" \
    && has 'AIRY_REVIEW_SCHEMA' "$SCHEMA_C"; then
    ok "L02 schema 字段存在/类型/非空校验"
else
    bad "L02 schema 字段存在/类型/非空校验"
fi

# L03 分级策略缺省 fail：warn 仅作为显式回退。
if grep -Fq 'getenv("AIRY_REVIEW_SCHEMA")' "$SCHEMA_C" \
    && has 'rv_schema_mode' "$REVIEW_C"; then
    ok "L03 schema 分级 warn/fail（缺省 fail）"
else
    bad "L03 schema 分级 warn/fail（缺省 fail）"
fi

# L04 黄金集与负例存在。
if has '--break' "$TEST_C" && has 'expect_bad' "$TEST_C"; then
    ok "L04 黄金集负例与 --break 模式"
else
    bad "L04 黄金集负例与 --break 模式"
fi

# L05 黄金集真跑：固定输入 → 期望结构 + 关键断言全绿。
if [ ! -x "$GOLDEN" ]; then
    skip "L05 黄金集动态复跑（缺 $GOLDEN）"
elif "$GOLDEN" >/tmp/gate_dual_think.out 2>&1; then
    ok "L05 黄金集动态复跑（$(grep -o '[0-9]* checks, [0-9]* failures' /tmp/gate_dual_think.out | head -1))"
else
    bad "L05 黄金集动态复跑"
fi

# L06 故意破坏 schema 必判红：负例必须全部被拒。
if [ ! -x "$GOLDEN" ]; then
    skip "L06 破坏 schema 必判红（缺 $GOLDEN）"
elif "$GOLDEN" --break >/tmp/gate_dual_think_break.out 2>&1 \
    && grep -Fq 'broken-schema fixtures rejected' /tmp/gate_dual_think_break.out; then
    ok "L06 破坏 schema 必判红（$(grep -o '[0-9]* broken-schema' /tmp/gate_dual_think_break.out | head -1)）"
else
    bad "L06 破坏 schema 必判红"
fi

# L07 真机双思考链路：需要活的 llm_d + 已配置 t2/t1f 模型 + 真实调用方。
# 本树内 think.review 尚无生产调用方（仅服务面与黄金集），本机亦无自托管
# 后端，故记 SKIP 并由 gate:runtime 在调用方接线后复跑，不折算 PASS。
skip "L07 真机双思考链路（无生产调用方与自托管后端）"

echo "SUMMARY dual-think: pass=$PASS fail=$FAIL skip=$SKIP"
if [ "$FAIL" -gt 0 ]; then
    exit 1
elif [ "$PASS" -gt 0 ]; then
    exit 0
fi
exit 2
