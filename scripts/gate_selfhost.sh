#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
#
# 自托管缺陷台账门禁（C-1）：llama.cpp 自托管十项生产级缺陷逐条复核。
#
# 台账来源：0.1.20 r128 整改记录 + 项目记忆 L67。每一项都必须同时满足：
#   1. 机制存在（静态锚点：参数确实从请求面走到线缆/判定面）；
#   2. 行为可证（动态测试：对应单元测试真实跑绿）。
# 动态测试二进制缺失时该项记 SKIP（不计 PASS）；静态锚点缺失直接 FAIL。
#
# 判据：exit 0 = 全项 PASS 且无 FAIL；exit 1 = 有 FAIL；exit 2 = 无 PASS（全 SKIP）。
#
# 用法：scripts/gate_selfhost.sh [--bin <测试二进制目录>]

set -uo pipefail

ROOT="$(cd -P "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN=""
while [ $# -gt 0 ]; do
    case "$1" in
        --bin) BIN="$2"; shift 2 ;;
        *) printf 'gate_selfhost: 未知参数 %s\n' "$1" >&2; exit 2 ;;
    esac
done
[ -n "$BIN" ] || BIN="$ROOT/build/bin"

ENV_C="$ROOT/daemons/providers/src/core/envelope.c"
DISP_C="$ROOT/daemons/llm_d/src/rpc/dispatch.c"
SVC_C="$ROOT/daemons/llm_d/src/rpc/service.c"
YML_C="$ROOT/daemons/llm_d/src/config/service_config_yaml_models.c"
METH_C="$ROOT/daemons/llm_d/src/rpc/methods.c"
SSE_C="$ROOT/daemons/providers/src/core/sse.c"
ADP_C="$ROOT/daemons/providers/src/core/adapter.c"
RESP_C="$ROOT/daemons/llm_d/src/rpc/response.c"
CHAIN_C="$ROOT/daemons/llm_d/src/rpc/service_chain.c"
REL_C="$ROOT/daemons/llm_d/src/rpc/service_reload.c"
EP_C="$ROOT/daemons/providers/src/core/ep_health.c"
RETRY_C="$ROOT/daemons/providers/src/core/retry.c"

PASS=0
FAIL=0
SKIP=0

has() { grep -Fq -- "$1" "$2" 2>/dev/null; }
hasnt() { ! grep -Fq -- "$1" "$2" 2>/dev/null; }

pass() { PASS=$((PASS + 1)); printf 'PASS gate:selfhost %s\n' "$*"; }
fail() { FAIL=$((FAIL + 1)); printf 'FAIL gate:selfhost %s\n' "$*" >&2; }
skip() { SKIP=$((SKIP + 1)); printf 'SKIP gate:selfhost %s\n' "$*"; }

# run_test <id> <binary> <label>：动态腿；二进制不存在记 SKIP。
run_test() {
    local id="$1" bin="$2" label="$3" out
    if [ ! -x "$BIN/$bin" ]; then
        skip "$id $label dynamic=$(basename "$bin") 缺失（--bin $BIN）"
        return 2
    fi
    if out="$("$BIN/$bin" 2>&1)"; then
        pass "$id $label dynamic=$(basename "$bin")"
        return 0
    fi
    fail "$id $label dynamic=$(basename "$bin") 失败（$(printf '%s' "$out" | tail -1)）"
    return 1
}

# ── L01 采样不可复现：seed 必须可从请求面抵达线缆 ──
if has '"seed"' "$DISP_C" && has '"seed"' "$ENV_C"; then
    run_test L01 test_llm_test_envelope "seed=请求面→envelope"
else
    fail "L01 seed 链路断裂（dispatch/envelope 锚点缺失）"
fi

# ── L02 temperature 面：固定 0.2 已废除；0.0 与「未设置」可区分 ──
if has 'temperature_set' "$DISP_C" && has 'temperature_set' "$ENV_C" && hasnt '0.2f' "$ENV_C"; then
    run_test L02 test_llm_test_envelope "temperature=哨兵三态"
else
    fail "L02 temperature 哨兵缺失或仍含固定 0.2"
fi

# ── L03 结构化约束：grammar 受能力位门控并如实下发 ──
if has '"grammar"' "$DISP_C" && has '"grammar"' "$ENV_C"; then
    run_test L03 test_llm_test_envelope "grammar=能力位门控"
else
    fail "L03 grammar 链路断裂"
fi

# ── L04 上下文容量三方一致：声明值进入机制，端点不再硬编码 8192 ──
if has 'svc_tokens_parse(m->context_window)' "$YML_C" &&
    has 'hw.max_context' "$SVC_C" && hasnt 'ep.context_window = 8192;' "$SVC_C"; then
    run_test L04 test_llm_test_config_decl "capacity=context_window→hw_caps→router"
else
    fail "L04 容量声明未进入选路（仍有 8192 硬编码或未派生）"
fi

# ── L05 超时与重试错配：慢失败不重试，重试仅限连接失败 ──
if has 'LLM_RETRY_FAST_FAIL_MS' "$METH_C" && has 'PROVIDER_RETRY_CONNECT_ONLY' "$RETRY_C"; then
    run_test L05 test_llm_test_provider_http_dispatch "retry=慢失败不重试"
else
    fail "L05 重试预算锚点缺失"
fi

# ── L06 流完整性：缺 [DONE] / 缺 finish_reason 必须判失败 ──
if has '[DONE]' "$SSE_C" && has 'finish_reason' "$ADP_C"; then
    run_test L06 test_llm_test_provider_http_dispatch "stream=[DONE]+finish_reason 完整性"
else
    fail "L06 流完整性判定缺失"
fi

# ── L07 截断伪装：finish_reason=length 必须显式暴露 ──
if has 'llm_finish_is_truncated' "$RESP_C"; then
    run_test L07 test_llm_test_finish_reason "truncation=显式暴露"
else
    fail "L07 截断判定缺失"
fi

# ── L08 并发槽位感知：准入闸读 /props.total_slots ──
if has 'total_slots' "$CHAIN_C"; then
    run_test L08 test_llm_test_chain "slots=total_slots 准入闸"
else
    fail "L08 槽位准入闸缺失"
fi

# ── L09 热切换：drain 后 swap，而非仅告警 ──
if has 'drain' "$REL_C" && has 'svc_swap_commit' "$REL_C"; then
    run_test L09 test_llm_test_service_reload "reload=drain+swap"
else
    fail "L09 热切换仍只告警（未见 drain/swap）"
fi

# ── L10 健康探测：深度探测粘滞 + 冷启识别，加载期 fail-closed ──
if has 'deep' "$EP_C" && has 'cold' "$EP_C"; then
    run_test L10 test_llm_test_health "health=deep-sticky+cold"
else
    fail "L10 健康探测状态机缺失"
fi

printf 'SUMMARY gate:selfhost pass=%d fail=%d skip=%d\n' "$PASS" "$FAIL" "$SKIP"
[ "$FAIL" -gt 0 ] && exit 1
[ "$PASS" -gt 0 ] || exit 2
exit 0
