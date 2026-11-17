#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
#
# 自托管运行态门禁（C-6）：以真实进程验证 T-1~T-8 八项运行态判据。
# 每个场景独立起停、独立判定，任一项缺证据即由汇总行点名，不以整体 PASS 掩盖。
#
# 场景契约（方案 §18.1 C-6 场景表）：
#   T-1 首次拉模：有阶段进度、可中断、可续传、失败可重试
#   T-2 断网：明确错误而非挂起；配置 routing.mode=auto 时切换成功且留原因日志
#   T-3 模型缺失：给出安装指引，不崩溃、不进入未知态
#   T-4 并发：N 并发互不串扰，llm_d 文件描述符不随请求数增长
#   T-5 内存不足：引擎报显式分配失败，守护仍可应答健康面
#   T-6 OOM/强杀后恢复：引擎可重建，请求自动恢复
#   T-7 重启恢复：配置与计费账目跨重启保留，请求仍通
#   T-8 日志可判读：启动/就绪/请求/失败四段均有可检索结构化字段
#
# 三值三码（SKIP 不计 PASS）：
#   exit 0 = 有 PASS 且无 FAIL；1 = 有 FAIL；2 = 全部 SKIP（缺前置）
#
# 用法：
#   scripts/gate_runtime.sh                    # 跑全部可用场景
#   scripts/gate_runtime.sh --stage T-2,T-4    # 只跑指定场景
#   scripts/gate_runtime.sh --self-test        # 只校验夹具与参数契约
#   scripts/gate_runtime.sh --list
#   AIRY_GATE_BIN=/tmp/a6b/bin scripts/gate_runtime.sh
#   AIRY_GATE_LLAMA=... AIRY_GATE_MODEL=... scripts/gate_runtime.sh

set -uo pipefail

ROOT="$(cd -P "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SB="$(mktemp -d "${TMPDIR:-/tmp}/airy-runtime.XXXXXX")" || exit 2

PASS_N=0
FAIL_N=0
SKIP_N=0
KEEP="${AIRY_GATE_KEEP:-0}"
STAGES="T-1 T-2 T-3 T-4 T-5 T-6 T-7 T-8"
SEL=""
MODE="run"

pass() { printf 'PASS  %s\n' "$*"; PASS_N=$((PASS_N + 1)); }
fail() { printf 'FAIL  %s\n' "$*"; FAIL_N=$((FAIL_N + 1)); }
skip() { printf 'SKIP  %s\n' "$*"; SKIP_N=$((SKIP_N + 1)); }

LLAMA_PID=""
LLM_PID=""
FIX_PID=""
PORT_LLAMA="${AIRY_GATE_PORT_LLAMA:-21435}"
PORT_FIX="${AIRY_GATE_PORT_FIX:-21436}"
PORT_OOM="${AIRY_GATE_PORT_OOM:-21437}"

cleanup() {
    [ -n "$LLM_PID" ] && kill -TERM "$LLM_PID" 2>/dev/null
    [ -n "$LLAMA_PID" ] && kill -KILL "$LLAMA_PID" 2>/dev/null
    [ -n "$FIX_PID" ] && kill -KILL "$FIX_PID" 2>/dev/null
    [ "$KEEP" = "1" ] && printf 'NOTE  夹具保留于 %s\n' "$SB"
    [ "$KEEP" = "1" ] || rm -rf "$SB"
}
trap cleanup EXIT

usage() { sed -n '5,30p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; }

while [ $# -gt 0 ]; do
    case "$1" in
        --bin) AIRY_GATE_BIN="$2"; shift 2 ;;
        --llama) AIRY_GATE_LLAMA="$2"; shift 2 ;;
        --model) AIRY_GATE_MODEL="$2"; shift 2 ;;
        --stage) SEL="$2"; shift 2 ;;
        --keep) KEEP=1; shift ;;
        --self-test) MODE="self"; shift ;;
        --list) MODE="list"; shift ;;
        -h|--help) usage; exit 0 ;;
        *) printf 'FAIL  未知参数 %s\n' "$1"; exit 2 ;;
    esac
done

# ---- 二进制与模型探测：显式优先，其后按常见安装/出树布局回退 ----
LLM_D="${AIRY_GATE_BIN:-}"
[ -n "$LLM_D" ] && [ -d "$LLM_D" ] && LLM_D="$LLM_D/llm_d"
if [ -z "$LLM_D" ] || [ ! -x "$LLM_D" ]; then
    for _c in "$ROOT/build/bin/llm_d" "$ROOT/build/daemons/llm_d/llm_d" \
              /tmp/a6b/bin/llm_d "$HOME/.airymaxrt/current/bin/llm_d"; do
        [ -x "$_c" ] && LLM_D="$_c" && break
    done
fi

LLAMA="${AIRY_GATE_LLAMA:-}"
if [ -z "$LLAMA" ] || [ ! -x "$LLAMA" ]; then
    for _c in "$HOME/.airymaxrt/local/llamacpp/bin/llama-server" \
              /usr/local/bin/llama-server /usr/bin/llama-server; do
        [ -x "$_c" ] && LLAMA="$_c" && break
    done
fi

MODEL="${AIRY_GATE_MODEL:-}"
if [ -z "$MODEL" ] || [ ! -f "$MODEL" ]; then
    for _c in "$HOME"/.airymaxrt/local/models/*.gguf; do
        [ -f "$_c" ] && MODEL="$_c" && break
    done
fi

MODEL_ID="${AIRY_GATE_MODEL_ID:-$(basename "${MODEL:-model.gguf}" .gguf)}"
AIRY_LOCAL="${AIRY_LOCAL_BIN:-$ROOT/../../tools/scripts/ops/bin/airy-local}"

# ---- 夹具：OpenAI 兼容端 + 支持 Range 的制品服务 ----
rt_fixture() { # <port> <mode: ok|empty|err500> <file>
    cat >"$SB/fixture.py" <<'PY'
import json, os, re, sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

mode = sys.argv[2]
path = sys.argv[3] if len(sys.argv) > 3 else ""

class H(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *a):
        pass

    def _send(self, code, body=b"", ctype="application/json"):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == "/health":
            return self._send(200, b'{"status":"ok"}')
        if self.path.startswith("/artifact") and path:
            data = open(path, "rb").read()
            rng = self.headers.get("Range")
            if rng:
                m = re.match(r"bytes=(\d+)-(\d*)", rng)
                if m:
                    a = int(m.group(1))
                    b = int(m.group(2)) if m.group(2) else len(data) - 1
                    b = min(b, len(data) - 1)
                    chunk = data[a:b + 1]
                    self.send_response(206)
                    self.send_header("Content-Range", "bytes %d-%d/%d" % (a, b, len(data)))
                    self.send_header("Content-Length", str(len(chunk)))
                    self.end_headers()
                    self.wfile.write(chunk)
                    return
            return self._send(200, data, "application/octet-stream")
        return self._send(404, b'{"error":"not found"}')

    def do_POST(self):
        n = int(self.headers.get("Content-Length", "0"))
        _ = self.rfile.read(n)
        if mode == "err500":
            return self._send(500, b'{"error":{"message":"gate: injected upstream failure"}}')
        if mode == "empty":
            body = {"id": "gate-empty", "object": "chat.completion", "model": "gate",
                    "choices": [{"index": 0, "finish_reason": "length",
                                 "message": {"role": "assistant", "content": "",
                                             "reasoning_content": "gate: reasoning only"}}],
                    "usage": {"prompt_tokens": 3, "completion_tokens": 2, "total_tokens": 5}}
        else:
            body = {"id": "gate-ok", "object": "chat.completion", "model": "gate",
                    "choices": [{"index": 0, "finish_reason": "stop",
                                 "message": {"role": "assistant", "content": "ok"}}],
                    "usage": {"prompt_tokens": 3, "completion_tokens": 1, "total_tokens": 4}}
        return self._send(200, json.dumps(body).encode())

ThreadingHTTPServer(("127.0.0.1", int(sys.argv[1])), H).serve_forever()
PY
    python3 "$SB/fixture.py" "$1" "$2" "${3:-}" >"$SB/fixture.log" 2>&1 &
    FIX_PID=$!
    rt_wait_http "$1" 20
}

rt_wait_http() { # <port> <秒>
    local i=0
    while [ "$i" -lt "$2" ]; do
        curl -fsS -m 1 "http://127.0.0.1:$1/health" >/dev/null 2>&1 && return 0
        sleep 0.2
        i=$((i + 1))
    done
    return 1
}

rt_port_free() { # <port>
    ! (exec 3<>"/dev/tcp/127.0.0.1/$1") 2>/dev/null
}

rt_home() { # <tag>
    local h="$SB/$1"
    mkdir -p "$h/config" "$h/data" "$h/logs" "$h/run" "$h/local/models" "$h/local/tmp"
    printf '%s\n' "$h"
}

rt_cfg() { # <home> <mode: api|local> <base_url> [model_id]
    local h="$1" mode="$2" base="$3" mid="${4:-$MODEL_ID}"
    cat >"$h/config/model.yaml" <<EOF
models:
  - name: gate-$mode
    mode: $mode
    api_format: $mode
    base_url: $base
    model_id: $mid
    provision: external
EOF
}

rt_cfg_chain() { # <home> <bad_base> <good_base>
    local h="$1" bad="$2" good="$3"
    cat >"$h/config/model.yaml" <<EOF
routing:
  mode: auto
  chain_depth: 3
  deadline_sec: 30
  on: [transport, auth, rate_limit, not_ready]
models:
  - name: gate-bad
    mode: api
    api_format: api
    base_url: $bad
    model_id: gate-bad
    api_key_env: GATE_BAD_KEY
    provision: external
  - name: gate-good
    mode: api
    api_format: api
    base_url: $good
    model_id: gate-good
    api_key_env: GATE_GOOD_KEY
    provision: external
EOF
}

rt_start() { # <home> <log> → LLM_PID
    AIRY_HOME="$1" "$LLM_D" --manager "$1/config/model.yaml" >"$2" 2>&1 &
    LLM_PID=$!
}

rt_wait_sock() { # <home> <秒>
    local sock="$1/run/llm.sock" i=0
    while [ "$i" -lt "$2" ]; do
        [ -S "$sock" ] && return 0
        kill -0 "${LLM_PID:-0}" 2>/dev/null || return 1
        sleep 0.2
        i=$((i + 1))
    done
    return 1
}

rt_stop_llm() {
    [ -n "$LLM_PID" ] && kill -TERM "$LLM_PID" 2>/dev/null
    wait "$LLM_PID" 2>/dev/null
    LLM_PID=""
}

rt_rpc() { # <sock> <method> <json> [秒] → stdout
    SOCK="$1" METHOD="$2" PARAMS="$3" TMO="${4:-120}" python3 - <<'PY'
import json, os, socket, sys, time
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.settimeout(float(os.environ["TMO"]))
try:
    s.connect(os.environ["SOCK"])
except OSError as e:
    print(json.dumps({"transport_error": str(e)}))
    sys.exit(3)
req = {"jsonrpc": "2.0", "id": 1, "method": os.environ["METHOD"],
       "params": json.loads(os.environ["PARAMS"] or "{}")}
s.sendall(json.dumps(req).encode())
buf = b""
end = time.time() + float(os.environ["TMO"])
while time.time() < end:
    try:
        c = s.recv(65536)
    except socket.timeout:
        break
    if not c:
        break
    buf += c
    try:
        json.loads(buf)
        break
    except ValueError:
        s.settimeout(0.5)
print(buf.decode("utf-8", "replace"))
s.close()
PY
}

rt_llama() { # <port> <log>
    rt_port_free "$1" || return 1
    LD_LIBRARY_PATH="$(dirname "$LLAMA")" "$LLAMA" -m "$MODEL" --host 127.0.0.1 \
        --port "$1" -c 2048 -np 4 --no-webui >"$2" 2>&1 &
    LLAMA_PID=$!
    rt_wait_http "$1" 120
}

rt_llama_stop() {
    [ -n "$LLAMA_PID" ] && kill -KILL "$LLAMA_PID" 2>/dev/null
    wait "$LLAMA_PID" 2>/dev/null
    LLAMA_PID=""
}

rt_need_llm() {
    [ -x "${LLM_D:-}" ] || { skip "$1：缺 llm_d 二进制"; return 1; }
    return 0
}

rt_need_llama() {
    [ -n "${LLAMA:-}" ] && [ -x "$LLAMA" ] && [ -f "${MODEL:-}" ] && return 0
    skip "$1：缺 llama-server 或 gguf 模型"; return 1
}

rt_need_local() {
    [ -x "$AIRY_LOCAL" ] || { skip "$1：缺 airy-local"; return 1; }
    return 0
}

rt_sel() {
    [ -z "$SEL" ] && return 0
    case ",$SEL," in *",$1,"*) return 0 ;; esac
    return 1
}

rt_self_test() {
    printf '== 契约自检 ==\n'
    if [ -x "$AIRY_LOCAL" ]; then pass "S0 制品通道可用 $AIRY_LOCAL"
    else skip "S0 缺 airy-local（T-1 将跳过）"; fi
    [ -n "${LLM_D:-}" ] && [ -x "$LLM_D" ] && pass "S0 llm_d=$LLM_D" \
        || skip "S0 缺 llm_d（T-2~T-8 将跳过）"
    rt_need_llama "S0" && pass "S0 llama-server=$LLAMA model=$(basename "$MODEL")"
    if rt_fixture "$PORT_FIX" ok "$MODEL"; then
        local got
        got="$(curl -fsS -m 3 -X POST "http://127.0.0.1:$PORT_FIX/v1/chat/completions" \
               -H 'Content-Type: application/json' -d '{}' 2>/dev/null)"
        case "$got" in *gate-ok*) pass "S0 夹具 ok 模式可用" ;; *) fail "S0 夹具 ok 模式异常" ;; esac
        kill -KILL "$FIX_PID" 2>/dev/null; wait "$FIX_PID" 2>/dev/null; FIX_PID=""
    fi
}

rt_list() {
    printf 'T-1 首次拉模（阶段进度/中断/续传/重试）\n'
    printf 'T-2 断网（明确错误/切换与原因日志）\n'
    printf 'T-3 模型缺失（安装指引/不崩溃）\n'
    printf 'T-4 并发（互不串扰/句柄不增长）\n'
    printf 'T-5 内存不足（显式失败/守护可答）\n'
    printf 'T-6 OOM 后恢复（引擎重建/请求恢复）\n'
    printf 'T-7 重启恢复（配置与账目保留）\n'
    printf 'T-8 日志可判读（四段结构化字段）\n'
}

# 场景实现按卡片递进补齐；未补齐的场景以 SKIP 保留契约位置。
scn_t1() { skip "T-1 场景待 C-6-03 落地"; }
scn_t2() { skip "T-2 场景待 C-6-02 落地"; }
scn_t3() { skip "T-3 场景待 C-6-02 落地"; }
scn_t4() { skip "T-4 场景待 C-6-04 落地"; }
scn_t5() { skip "T-5 场景待 C-6-04 落地"; }
scn_t6() { skip "T-6 场景待 C-6-05 落地"; }
scn_t7() { skip "T-7 场景待 C-6-05 落地"; }
scn_t8() { skip "T-8 场景待 C-6-05 落地"; }

case "$MODE" in
    list) rt_list; exit 0 ;;
    self) rt_self_test ;;
    run)
        for _s in $STAGES; do
            rt_sel "$_s" || continue
            printf -- '---- %s ----\n' "$_s"
            "scn_$(printf '%s' "$_s" | tr -d '-' | tr 'A-Z' 'a-z')"
        done
        ;;
esac

printf -- '---- 汇总 ----\nPASS=%d FAIL=%d SKIP=%d\n' "$PASS_N" "$FAIL_N" "$SKIP_N"
[ "$FAIL_N" -gt 0 ] && exit 1
[ "$PASS_N" -eq 0 ] && exit 2
exit 0
