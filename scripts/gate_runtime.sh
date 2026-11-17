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
FIX_PIDS=()
PORT_LLAMA="${AIRY_GATE_PORT_LLAMA:-21435}"
PORT_FIX="${AIRY_GATE_PORT_FIX:-21436}"
PORT_OOM="${AIRY_GATE_PORT_OOM:-21437}"
PORT_OK="${AIRY_GATE_PORT_OK:-21438}"

cleanup() {
    [ -n "$LLM_PID" ] && kill -TERM "$LLM_PID" 2>/dev/null
    [ -n "$LLAMA_PID" ] && kill -KILL "$LLAMA_PID" 2>/dev/null
    rt_fix_stop_all
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
rt_fixture() { # <port> <mode: ok|empty|err500|echo|slow> <file>
    cat >"$SB/fixture.py" <<'PY'
import json, os, re, sys, time
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
            start, end = 0, len(data) - 1
            code = 200
            if rng:
                m = re.match(r"bytes=(\d+)-(\d*)", rng)
                if m:
                    start = int(m.group(1))
                    end = min(int(m.group(2)) if m.group(2) else len(data) - 1, len(data) - 1)
                    code = 206
                    print("RANGE %d-%d %s" % (start, end, self.path), flush=True)
            chunk = data[start:end + 1]
            self.send_response(code)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(len(chunk)))
            if code == 206:
                self.send_header("Content-Range", "bytes %d-%d/%d" % (start, end, len(data)))
            self.end_headers()
            if mode == "slow":
                for off in range(0, len(chunk), 65536):
                    self.wfile.write(chunk[off:off + 65536])
                    self.wfile.flush()
                    time.sleep(0.04)
                return
            self.wfile.write(chunk)
            return
        return self._send(404, b'{"error":"not found"}')

    def do_POST(self):
        n = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(n)
        if mode == "err500":
            return self._send(500, b'{"error":{"message":"gate: injected upstream failure"}}')
        if mode == "echo":
            try:
                msgs = json.loads(raw.decode("utf-8", "replace")).get("messages") or []
                # 守护会追加宿主机时间上下文，回声取最后一条 user 正文。
                text = ""
                for m in msgs:
                    if m.get("role") == "user":
                        text = m.get("content", "")
            except ValueError:
                text = ""
            time.sleep(0.5)
            body = {"id": "gate-echo", "object": "chat.completion", "model": "gate",
                    "choices": [{"index": 0, "finish_reason": "stop",
                                 "message": {"role": "assistant", "content": "echo:" + text}}],
                    "usage": {"prompt_tokens": 3, "completion_tokens": 1, "total_tokens": 4}}
            return self._send(200, json.dumps(body).encode())
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
    python3 "$SB/fixture.py" "$1" "$2" "${3:-}" >"$SB/fixture-$1.log" 2>&1 &
    FIX_PIDS+=("$!")
    rt_wait_http "$1" 20
}

rt_fix_stop_all() {
    local p
    for p in ${FIX_PIDS[@]+"${FIX_PIDS[@]}"}; do
        kill -KILL "$p" 2>/dev/null
        wait "$p" 2>/dev/null
    done
    FIX_PIDS=()
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

rt_size() { stat -c%s "$1" 2>/dev/null || stat -f%z "$1" 2>/dev/null; }

rt_sha() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | awk '{print $1}'
    else
        shasum -a 256 "$1" | awk '{print $1}'
    fi
}

rt_cfg() { # <home> <mode: api|local> <base_url> [model_id]
    local h="$1" mode="$2" base="$3" mid="${4:-$MODEL_ID}"
    cat >"$h/config/model.yaml" <<EOF
models:
  - name: gate-$mode
    mode: $mode
    base_url: $base
    model_id: $mid
    provision: external
EOF
}

rt_cfg_local() { # <home> <base_url> [model_id]
    local h="$1" base="$2" mid="${3:-$MODEL_ID}"
    cat >"$h/config/model.yaml" <<EOF
models:
  - name: gate-local
    mode: local
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
    base_url: $bad
    model_id: gate-bad
    api_key_env: GATE_BAD_KEY
    provision: external
  - name: gate-good
    mode: api
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
        --port "$1" -c 2048 -np 4 --no-webui --reasoning off >"$2" 2>&1 &
    LLAMA_PID=$!
    rt_wait_http "$1" 120
}

rt_llama_stop() {
    [ -n "$LLAMA_PID" ] && kill -KILL "$LLAMA_PID" 2>/dev/null
    wait "$LLAMA_PID" 2>/dev/null
    LLAMA_PID=""
}

# 内存受限启动：地址空间上限取模型体积之半，权重映射必然失败。
# 返回 llama-server 退出码；124 = 有界等待内未退出（视为异常）。
rt_llama_oom() { # <port> <log> <vkb>
    rt_port_free "$1" || return 99
    (ulimit -v "$3" 2>/dev/null || exit 98
     LD_LIBRARY_PATH="$(dirname "$LLAMA")" exec "$LLAMA" -m "$MODEL" --host 127.0.0.1 \
        --port "$1" -c 2048 -np 4 --no-webui >"$2" 2>&1) &
    LLAMA_PID=$!
    local i=0 rc
    while [ "$i" -lt 600 ]; do
        if ! kill -0 "$LLAMA_PID" 2>/dev/null; then
            wait "$LLAMA_PID" 2>/dev/null
            rc=$?
            LLAMA_PID=""
            return "$rc"
        fi
        sleep 0.1
        i=$((i + 1))
    done
    kill -KILL "$LLAMA_PID" 2>/dev/null
    wait "$LLAMA_PID" 2>/dev/null
    LLAMA_PID=""
    return 124
}

rt_fd_n() { # <pid> → fd 数；无 /proc 平台输出 -1
    [ -d "/proc/$1/fd" ] || { printf -- '-1\n'; return 0; }
    ls "/proc/$1/fd" 2>/dev/null | wc -l | tr -d ' '
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
        rt_fix_stop_all
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

# ── T-1 首次拉模：阶段进度、中断留痕、续传收敛、毒件 fail-closed ──
scn_t1() {
    rt_need_local "T-1" || return 0
    if ! command -v python3 >/dev/null 2>&1; then
        skip "T-1：缺 python3，无法生成制品与夹具"
        return 0
    fi

    local src h pin log out rc dl part ps size sha
    src="$SB/t1"
    mkdir -p "$src"
    python3 - "$src/good.bin" <<'PY'
import sys
buf = bytearray(4 * 1024 * 1024)
for i in range(len(buf)):
    buf[i] = (i * 7 + i // 251) % 256
open(sys.argv[1], "wb").write(buf)
PY
    size="$(rt_size "$src/good.bin")"
    sha="$(rt_sha "$src/good.bin")"
    if [ -z "$size" ] || [ -z "$sha" ]; then
        fail "T-1 制品基准生成失败"
        return 0
    fi

    rt_fixture "$PORT_FIX" slow "$src/good.bin" || {
        fail "T-1 慢速制品夹具未就绪"
        return 0
    }

    # A 段：慢速下载中 SIGTERM，在途 .part 必须保留且不完整。
    h="$(rt_home t1a)"
    pin="$src/pin-a.list"
    printf 'model|gate-model|gate-model.gguf|%s|%s|\nmirror|gate-model.gguf|http://127.0.0.1:%s/artifact/{asset}\n' \
        "$size" "$sha" "$PORT_FIX" >"$pin"
    log="$h/logs/install.log"
    setsid env AIRY_HOME="$h" AIRY_LOCAL_MODEL_PIN="$pin" \
        AIRY_LOCAL_MODEL_CURATED="$src/none.list" \
        "$AIRY_LOCAL" model install gate-model >"$log" 2>&1 &
    dl=$!
    part=""
    for _i in $(seq 1 200); do
        part="$(compgen -G "$h/local/tmp/*.part" | head -1)"
        if [ -n "$part" ] && [ "$(rt_size "$part")" -ge 262144 ]; then break; fi
        sleep 0.1
    done
    if [ -z "$part" ] || [ ! -f "$part" ]; then
        fail "T-1a 未观察到在途 .part"
        kill -TERM -- -"$dl" 2>/dev/null || kill -TERM "$dl" 2>/dev/null
        wait "$dl" 2>/dev/null
        return 0
    fi
    kill -TERM -- -"$dl" 2>/dev/null || {
        kill -TERM "$dl" 2>/dev/null
        pkill -TERM -P "$dl" 2>/dev/null
    }
    wait "$dl" 2>/dev/null

    if [ -f "$part" ]; then
        ps="$(rt_size "$part")"
        pass "T-1a SIGTERM 后保留在途 .part（${ps} 字节）"
        if [ "$ps" -gt 0 ] && [ "$ps" -lt "$size" ]; then
            pass "T-1a 中断点确为不完整件（${ps}/${size}）"
        else
            fail "T-1a 中断件尺寸异常（${ps}/${size}）"
        fi
    else
        ps=0
        fail "T-1a 中断后未保留 .part"
    fi
    if grep -q 'Fetching:' "$log"; then
        pass "T-1a 拉取阶段进度可见"
    else
        fail "T-1a 缺拉取阶段日志"
    fi

    # B 段：同目录重跑，必须从既有偏移续传并收敛为完整件。
    out="$(AIRY_HOME="$h" AIRY_LOCAL_MODEL_PIN="$pin" \
        AIRY_LOCAL_MODEL_CURATED="$src/none.list" \
        "$AIRY_LOCAL" model install gate-model 2>&1)"
    rc=$?
    if [ "$rc" -eq 0 ]; then
        pass "T-1b 重跑收敛成功"
    else
        fail "T-1b 重跑失败（rc=$rc）：$(printf '%.160s' "$out")"
    fi
    if [ "$ps" -gt 0 ] && grep -q "RANGE ${ps}-" "$SB/fixture-$PORT_FIX.log"; then
        pass "T-1b 续传从既有偏移发起（${ps}）"
    else
        fail "T-1b 未见从既有偏移续传"
    fi
    if printf '%s' "$out" | grep -q 'Installed model:'; then
        pass "T-1b 落位阶段进度可见"
    else
        fail "T-1b 缺落位阶段日志"
    fi
    if [ -f "$h/local/models/gate-model.gguf" ] \
       && [ "$(rt_sha "$h/local/models/gate-model.gguf")" = "$sha" ]; then
        pass "T-1b 落位件 sha256 与 pin 一致"
    else
        fail "T-1b 落位件缺失或哈希不符"
    fi

    # C 段：同尺寸异内容即毒件，必须 fail-closed 且不留残件；换正品即恢复。
    rt_fix_stop_all
    python3 - "$src/good.bin" "$src/bad.bin" <<'PY'
import sys
buf = bytearray(open(sys.argv[1], "rb").read())
buf[0] ^= 0xFF
open(sys.argv[2], "wb").write(buf)
PY
    rt_fixture "$PORT_FIX" ok "$src/bad.bin" || {
        fail "T-1c 毒件夹具未就绪"
        return 0
    }
    h="$(rt_home t1b)"
    pin="$src/pin-b.list"
    printf 'model|gate-model|gate-model.gguf|%s|%s|\nmirror|gate-model.gguf|http://127.0.0.1:%s/artifact/{asset}\n' \
        "$size" "$sha" "$PORT_FIX" >"$pin"
    out="$(AIRY_HOME="$h" AIRY_LOCAL_MODEL_PIN="$pin" \
        AIRY_LOCAL_MODEL_CURATED="$src/none.list" \
        "$AIRY_LOCAL" model install gate-model 2>&1)"
    rc=$?
    if [ "$rc" -ne 0 ]; then
        pass "T-1c 毒件拒绝安装（rc=$rc）"
    else
        fail "T-1c 毒件被误接受"
    fi
    case "$out" in
        *poisoned*) pass "T-1c fail-closed 语义明确" ;;
        *) fail "T-1c 缺 fail-closed 说明：$(printf '%.160s' "$out")" ;;
    esac
    if [ ! -f "$h/local/models/gate-model.gguf" ] \
       && [ -z "$(compgen -G "$h/local/tmp/*.part")" ]; then
        pass "T-1c 毒件与残件均未落位"
    else
        fail "T-1c 毒件路径留下残件"
    fi

    rt_fix_stop_all
    for _i in $(seq 1 50); do rt_port_free "$PORT_FIX" && break; sleep 0.1; done
    rt_fixture "$PORT_FIX" ok "$src/good.bin" || {
        fail "T-1c 正品夹具未就绪"
        return 0
    }
    out="$(AIRY_HOME="$h" AIRY_LOCAL_MODEL_PIN="$pin" \
        AIRY_LOCAL_MODEL_CURATED="$src/none.list" \
        "$AIRY_LOCAL" model install gate-model 2>&1)"
    rc=$?
    if [ "$rc" -eq 0 ] && [ -f "$h/local/models/gate-model.gguf" ]; then
        pass "T-1c 换回正品后重试成功"
    else
        fail "T-1c 恢复重试未成功（rc=$rc）"
    fi
    rt_fix_stop_all
}
# ── T-2 断网：失败必须有界且可判读；auto 模式必须切到健康端点 ──
scn_t2() {
    rt_need_llm "T-2" || return 0

    local h out t0 dt
    h="$(rt_home t2a)"
    rt_cfg "$h" api "http://127.0.0.1:9" gate-net
    export GATE_NET_KEY=gate
    rt_start "$h" "$h/logs/llm.log"
    if ! rt_wait_sock "$h" 20; then
        fail "T-2a llm_d 未就绪（见 $h/logs/llm.log）"
        rt_stop_llm
        return 0
    fi

    t0="$(date +%s%N)"
    out="$(rt_rpc "$h/run/llm.sock" complete \
        '{"model":"gate-net","messages":[{"role":"user","content":"ping"}],"max_tokens":32}' 60)"
    dt=$((($(date +%s%N) - t0) / 1000000))
    case "$out" in
        *'"error"'*) pass "T-2a 断网返回明确错误（${dt}ms）" ;;
        *) fail "T-2a 断网未返回错误：$(printf '%.160s' "$out")" ;;
    esac
    if [ "$dt" -lt 60000 ]; then
        pass "T-2a 失败路径有界（${dt}ms < 60000ms）"
    else
        fail "T-2a 失败路径疑似挂起（${dt}ms）"
    fi
    if grep -Eq 'diag=TIMEOUT|HTTP-POST-NORETRY|curl_error|connect' "$h/logs/llm.log"; then
        pass "T-2a 日志给出传输层诊断"
    else
        fail "T-2a 缺传输层诊断日志"
    fi
    out="$(rt_rpc "$h/run/llm.sock" health_check '{}' 10)"
    case "$out" in
        *'"health"'*) pass "T-2a 失败后健康面仍可应答" ;;
        *) fail "T-2a 失败后守护无响应" ;;
    esac
    rt_stop_llm

    # auto 链：坏端点（空 content）→ 好端点（content=ok），切换须留原因。
    h="$(rt_home t2b)"
    if ! rt_fixture "$PORT_FIX" empty "$MODEL"; then
        fail "T-2b 空响应夹具未就绪"
        return 0
    fi
    if ! rt_fixture "$PORT_OK" ok "$MODEL"; then
        fail "T-2b 正常夹具未就绪"
        rt_fix_stop_all
        return 0
    fi
    rt_cfg_chain "$h" "http://127.0.0.1:$PORT_FIX" "http://127.0.0.1:$PORT_OK"
    export GATE_BAD_KEY=gate GATE_GOOD_KEY=gate
    rt_start "$h" "$h/logs/llm.log"
    if ! rt_wait_sock "$h" 20; then
        fail "T-2b llm_d 未就绪（见 $h/logs/llm.log）"
        rt_stop_llm
        rt_fix_stop_all
        return 0
    fi

    out="$(rt_rpc "$h/run/llm.sock" complete \
        '{"model":"gate-bad","messages":[{"role":"user","content":"switch"}],"max_tokens":32}' 90)"
    case "$out" in
        *'"error"'*) fail "T-2b 切换后仍失败：$(printf '%.160s' "$out")" ;;
        *'"ok"'*) pass "T-2b 失败端点后切到健康端点并成功" ;;
        *) fail "T-2b 响应不含预期内容：$(printf '%.160s' "$out")" ;;
    esac
    if grep -q 'CHAIN-SWITCH' "$h/logs/llm.log"; then
        pass "T-2b 切换留 CHAIN-SWITCH 记录"
    else
        fail "T-2b 缺 CHAIN-SWITCH 记录"
    fi
    if grep -q 'reason=transport' "$h/logs/llm.log"; then
        pass "T-2b 切换原因可判读（transport）"
    else
        fail "T-2b 切换原因不可判读"
    fi
    rt_stop_llm
    rt_fix_stop_all
}

# ── T-3 模型缺失：拒绝启用、给出可执行安装指引、通道不崩 ──
scn_t3() {
    rt_need_local "T-3" || return 0

    local h out rc
    h="$(rt_home t3)"
    out="$(AIRY_HOME="$h" "$AIRY_LOCAL" model use gate-absent 2>&1)"
    rc=$?
    if [ "$rc" -ne 0 ]; then
        pass "T-3 缺失模型拒绝启用（rc=$rc）"
    else
        fail "T-3 缺失模型被误判为已安装"
    fi
    case "$out" in
        *"model install gate-absent"*) pass "T-3 输出给出可执行安装指引" ;;
        *) fail "T-3 缺安装指引：$(printf '%.160s' "$out")" ;;
    esac
    if [ "$rc" -lt 128 ]; then
        pass "T-3 缺失路径未崩溃（rc=$rc）"
    else
        fail "T-3 异常终止（rc=$rc）"
    fi
    out="$(AIRY_HOME="$h" "$AIRY_LOCAL" model list 2>&1)"
    rc=$?
    if [ "$rc" -eq 0 ]; then
        pass "T-3 失败后制品通道仍可用（model list rc=0）"
    else
        fail "T-3 失败后制品通道不可用（rc=$rc）：$(printf '%.120s' "$out")"
    fi
}

# ── T-4 并发：8 路各自配对不串扰，fd 不随请求数增长 ──
scn_t4() {
    rt_need_llm "T-4" || return 0
    if ! command -v python3 >/dev/null 2>&1; then
        skip "T-4：缺 python3，无法起回声夹具"
        return 0
    fi
    if ! rt_fixture "$PORT_FIX" echo "$MODEL"; then
        fail "T-4 回声夹具未就绪"
        return 0
    fi

    local h out i bad=0 fd0 fd1 t0 dt tag
    h="$(rt_home t4)"
    rt_cfg "$h" api "http://127.0.0.1:$PORT_FIX" gate-echo
    rt_start "$h" "$h/logs/llm.log"
    if ! rt_wait_sock "$h" 20; then
        fail "T-4 llm_d 未就绪（见 $h/logs/llm.log）"
        rt_stop_llm
        rt_fix_stop_all
        return 0
    fi

    # 预热一次，让常驻连接进入基线，避免首连开合计入增量。
    out="$(rt_rpc "$h/run/llm.sock" complete \
        '{"model":"gate-echo","messages":[{"role":"user","content":"c4-00"}],"max_tokens":16}' 30)"
    fd0="$(rt_fd_n "$LLM_PID")"

    local pids=()
    t0="$(date +%s%N)"
    for i in 1 2 3 4 5 6 7 8; do
        rt_rpc "$h/run/llm.sock" complete \
            "{\"model\":\"gate-echo\",\"messages\":[{\"role\":\"user\",\"content\":\"c4-0$i\"}],\"max_tokens\":16}" 60 \
            >"$SB/t4-$i.out" 2>&1 &
        pids+=("$!")
    done
    for i in ${pids[@]+"${pids[@]}"}; do wait "$i" 2>/dev/null; done
    dt=$((($(date +%s%N) - t0) / 1000000))

    for i in 1 2 3 4 5 6 7 8; do
        tag="$(grep -o 'echo:c4-[0-9][0-9]' "$SB/t4-$i.out" 2>/dev/null | head -1)"
        [ "$tag" = "echo:c4-0$i" ] || bad=$((bad + 1))
    done
    if [ "$bad" -eq 0 ]; then
        pass "T-4 8 并发响应各自配对无串扰"
    else
        fail "T-4 ${bad}/8 并发响应串扰或缺失"
    fi
    if [ "$dt" -lt 3000 ]; then
        pass "T-4 请求并行执行（${dt}ms，串行下界 4000ms）"
    else
        fail "T-4 请求疑似串行（${dt}ms）"
    fi

    sleep 0.5
    fd1="$(rt_fd_n "$LLM_PID")"
    if [ "$fd0" = "-1" ]; then
        skip "T-4 fd 采样不可用（非 /proc 平台）"
    elif [ $((fd1 - fd0)) -le 2 ]; then
        pass "T-4 fd 不随请求数增长（${fd0} → ${fd1}）"
    else
        fail "T-4 fd 泄漏（${fd0} → ${fd1}）"
    fi
    if kill -0 "$LLM_PID" 2>/dev/null; then
        pass "T-4 并发后守护存活"
    else
        fail "T-4 并发后守护退出"
    fi
    rt_stop_llm
    rt_fix_stop_all
}

# ── T-5 内存不足：引擎显式分配失败，守护健康面仍可答 ──
scn_t5() {
    rt_need_llm "T-5" || return 0
    rt_need_llama "T-5" || return 0

    local h lim log rc out t0 dt
    h="$(rt_home t5)"
    log="$h/logs/engine.log"
    rt_cfg "$h" api "http://127.0.0.1:$PORT_OOM" gate-oom
    rt_start "$h" "$h/logs/llm.log"
    if ! rt_wait_sock "$h" 20; then
        fail "T-5 llm_d 未就绪（见 $h/logs/llm.log）"
        rt_stop_llm
        return 0
    fi

    # 上限取权重体积之半：地址空间不足以映射权重，加载必然显式失败。
    lim=$(( $(rt_size "$MODEL") / 2048 ))
    [ "$lim" -ge 65536 ] || lim=65536
    rc=0
    rt_llama_oom "$PORT_OOM" "$log" "$lim" || rc=$?
    if [ "$rc" -eq 98 ]; then
        skip "T-5：本平台不支持 ulimit -v"
        rt_stop_llm
        return 0
    fi
    if [ "$rc" -ne 0 ] && [ "$rc" -ne 124 ] && [ "$rc" -ne 99 ]; then
        pass "T-5 地址空间受限下引擎显式退出（rc=$rc）"
    else
        fail "T-5 引擎未给出显式失败（rc=$rc）"
    fi
    if grep -Eqi 'Cannot allocate memory|out of memory|failed to allocate|ENOMEM|mmap failed' "$log"; then
        pass "T-5 失败语义显式为分配失败"
    else
        fail "T-5 缺显式分配失败日志"
    fi
    if rt_port_free "$PORT_OOM"; then
        pass "T-5 失败后无残留监听（未进入未知态）"
    else
        fail "T-5 引擎失败后端口仍被占用"
    fi

    t0="$(date +%s%N)"
    out="$(rt_rpc "$h/run/llm.sock" complete \
        '{"model":"gate-oom","messages":[{"role":"user","content":"oom"}],"max_tokens":16}' 60)"
    dt=$((($(date +%s%N) - t0) / 1000000))
    case "$out" in
        *'"error"'*) pass "T-5 引擎不可用时请求返回明确错误（${dt}ms）" ;;
        *) fail "T-5 未返回明确错误：$(printf '%.160s' "$out")" ;;
    esac
    if [ "$dt" -lt 60000 ]; then
        pass "T-5 失败路径有界（${dt}ms < 60000ms）"
    else
        fail "T-5 失败路径疑似挂起（${dt}ms）"
    fi
    out="$(rt_rpc "$h/run/llm.sock" health_check '{}' 10)"
    case "$out" in
        *'"health"'*) pass "T-5 OOM 后守护健康面仍可应答" ;;
        *) fail "T-5 OOM 后守护健康面无响应" ;;
    esac
    if kill -0 "$LLM_PID" 2>/dev/null; then
        pass "T-5 守护进程存活未进入未知态"
    else
        fail "T-5 守护进程已退出"
    fi
    rt_stop_llm
}

rt_wait_port_free() { # <port> <秒>
    local i=0
    while [ "$i" -lt $(($2 * 10)) ]; do
        rt_port_free "$1" && return 0
        sleep 0.1
        i=$((i + 1))
    done
    return 1
}

# 请求四元组（D-7/Q19）：输出 "total ok fail timeout"，不可读时输出全 -1。
rt_req_tuple() { # <sock>
    rt_rpc "$1" get_stats '{}' 10 | python3 -c '
import json, sys
try:
    d = json.load(sys.stdin)
except ValueError:
    print(-1, -1, -1, -1)
    raise SystemExit
r = d.get("result")
if isinstance(r, str):
    try:
        r = json.loads(r)
    except ValueError:
        r = None
q = (r or {}).get("requests") or {}
print(int(q.get("total", -1)), int(q.get("ok", -1)),
      int(q.get("fail", -1)), int(q.get("timeout", -1)))
'
}

# 计费账目合计（prompt + completion）：无账目文件输出 -1。
rt_usage_tokens() { # <home>
    local f="$1/data/agentrt/llm_usage.json"
    [ -f "$f" ] || { printf -- '-1\n'; return 0; }
    python3 - "$f" <<'PY'
import json, sys
try:
    d = json.load(open(sys.argv[1], encoding="utf-8"))
except (OSError, ValueError):
    print(-1)
    raise SystemExit
tot = 0
for m in d.get("models") or []:
    tot += int(m.get("prompt_tokens", 0)) + int(m.get("completion_tokens", 0))
print(tot)
PY
}

# ── T-6 OOM/强杀后恢复：引擎可重建，请求自动恢复 ──
scn_t6() {
    rt_need_llm "T-6" || return 0
    rt_need_llama "T-6" || return 0

    local h out
    h="$(rt_home t6)"
    if ! rt_llama "$PORT_LLAMA" "$h/logs/engine.log"; then
        fail "T-6 引擎首启未就绪（见 $h/logs/engine.log）"
        rt_llama_stop
        return 0
    fi
    rt_cfg_local "$h" "http://127.0.0.1:$PORT_LLAMA/v1"
    rt_start "$h" "$h/logs/llm.log"
    if ! rt_wait_sock "$h" 20; then
        fail "T-6 llm_d 未就绪（见 $h/logs/llm.log）"
        rt_stop_llm
        rt_llama_stop
        return 0
    fi

    out="$(rt_rpc "$h/run/llm.sock" complete \
        "{\"model\":\"$MODEL_ID\",\"messages\":[{\"role\":\"user\",\"content\":\"gate t6\"}],\"max_tokens\":24}" 120)"
    if printf '%s' "$out" | grep -Eq '"content":".+"'; then
        pass "T-6 基线请求成功（引擎在线）"
    else
        fail "T-6 基线请求失败：$(printf '%.160s' "$out")"
    fi

    rt_llama_stop
    if rt_wait_port_free "$PORT_LLAMA" 10; then
        pass "T-6 引擎被强杀后端口释放"
    else
        fail "T-6 引擎强杀后端口仍占用"
    fi
    out="$(rt_rpc "$h/run/llm.sock" complete \
        "{\"model\":\"$MODEL_ID\",\"messages\":[{\"role\":\"user\",\"content\":\"gate t6 down\"}],\"max_tokens\":24}" 60)"
    case "$out" in
        *'"error"'*) pass "T-6 引擎缺失时请求返回明确错误" ;;
        *) fail "T-6 引擎缺失时未返回明确错误：$(printf '%.160s' "$out")" ;;
    esac

    if rt_llama "$PORT_LLAMA" "$h/logs/engine2.log"; then
        pass "T-6 引擎同端口重建成功"
    else
        fail "T-6 引擎同端口重建失败"
    fi
    out="$(rt_rpc "$h/run/llm.sock" complete \
        "{\"model\":\"$MODEL_ID\",\"messages\":[{\"role\":\"user\",\"content\":\"gate t6 up\"}],\"max_tokens\":24}" 120)"
    if printf '%s' "$out" | grep -Eq '"content":".+"'; then
        pass "T-6 引擎重建后请求自动恢复（未重启守护）"
    else
        fail "T-6 引擎重建后请求未恢复：$(printf '%.160s' "$out")"
    fi
    out="$(rt_rpc "$h/run/llm.sock" health_check '{}' 10)"
    case "$out" in
        *'"health"'*) pass "T-6 恢复后守护健康面可答" ;;
        *) fail "T-6 恢复后守护健康面无响应" ;;
    esac
    if kill -0 "$LLM_PID" 2>/dev/null; then
        pass "T-6 守护跨引擎重建存活"
    else
        fail "T-6 守护已退出"
    fi
    rt_stop_llm
    rt_llama_stop
}

# ── T-7 重启恢复：配置与计费账目跨重启保留，请求仍通 ──
scn_t7() {
    rt_need_llm "T-7" || return 0
    if ! command -v python3 >/dev/null 2>&1; then
        skip "T-7：缺 python3，无法起回声夹具"
        return 0
    fi
    if ! rt_fixture "$PORT_FIX" echo "$MODEL"; then
        fail "T-7 回声夹具未就绪"
        return 0
    fi

    local h out i cfg_sha t1 t2
    h="$(rt_home t7)"
    rt_cfg "$h" api "http://127.0.0.1:$PORT_FIX" gate-echo
    rt_start "$h" "$h/logs/llm.log"
    if ! rt_wait_sock "$h" 20; then
        fail "T-7 llm_d 首启未就绪（见 $h/logs/llm.log）"
        rt_stop_llm
        rt_fix_stop_all
        return 0
    fi

    for i in 1 2 3; do
        out="$(rt_rpc "$h/run/llm.sock" complete \
            "{\"model\":\"gate-echo\",\"messages\":[{\"role\":\"user\",\"content\":\"t7-$i\"}],\"max_tokens\":16}" 60)"
        printf '%s' "$out" | grep -q "echo:t7-$i" || fail "T-7 首启第 ${i} 次请求失败"
    done
    # D-7（Q19）：四元组自洽且分母非零；两次连续采集差值可解释。
    local tup a b
    tup="$(rt_req_tuple "$h/run/llm.sock")"
    set -- $tup
    if [ "$1" -gt 0 ] && [ $(( $2 + $3 + $4 )) -eq "$1" ]; then
        pass "T-7 成功率四元组自洽（total=$1 ok=$2 fail=$3 timeout=$4）"
    else
        fail "T-7 四元组不自洽（total=$1 ok=$2 fail=$3 timeout=$4）"
    fi
    a="$tup"
    sleep 1
    b="$(rt_req_tuple "$h/run/llm.sock")"
    if [ "$a" = "$b" ]; then
        pass "T-7 连续两次采集差值 0（期间无新请求）"
    else
        fail "T-7 采集差值不可解释（$a → $b）"
    fi
    cfg_sha="$(rt_sha "$h/config/model.yaml")"
    rt_stop_llm

    t1="$(rt_usage_tokens "$h")"
    if [ -f "$h/data/agentrt/llm_usage.json" ] && [ "$t1" = "12" ]; then
        pass "T-7 停机后计费账目落盘（3 次 × 4 = ${t1}）"
    else
        fail "T-7 计费账目落盘异常（合计 ${t1}）"
    fi

    rt_start "$h" "$h/logs/llm2.log"
    if ! rt_wait_sock "$h" 20; then
        fail "T-7 llm_d 重启未就绪（见 $h/logs/llm2.log）"
        rt_stop_llm
        rt_fix_stop_all
        return 0
    fi
    if [ "$(rt_sha "$h/config/model.yaml")" = "$cfg_sha" ]; then
        pass "T-7 配置跨重启逐字节不变"
    else
        fail "T-7 配置在重启中被改写"
    fi
    if [ "$(rt_usage_tokens "$h")" = "$t1" ]; then
        pass "T-7 重启后账目保留（${t1}）"
    else
        fail "T-7 重启后账目丢失（$t1 → $(rt_usage_tokens "$h")）"
    fi

    out="$(rt_rpc "$h/run/llm.sock" complete \
        '{"model":"gate-echo","messages":[{"role":"user","content":"t7-after"}],"max_tokens":16}' 60)"
    if printf '%s' "$out" | grep -q 'echo:t7-after'; then
        pass "T-7 重启后请求仍通"
    else
        fail "T-7 重启后请求失败：$(printf '%.160s' "$out")"
    fi
    # 计数器随进程重建：重启后仅本次请求计入，四元组仍须自洽。
    tup="$(rt_req_tuple "$h/run/llm.sock")"
    set -- $tup
    if [ "$1" = 1 ] && [ "$2" = 1 ] && [ $(( $2 + $3 + $4 )) -eq "$1" ]; then
        pass "T-7 重启后四元组按新进程计（total=1 ok=1）"
    else
        fail "T-7 重启后四元组异常（total=$1 ok=$2 fail=$3 timeout=$4）"
    fi
    rt_stop_llm
    t2="$(rt_usage_tokens "$h")"
    if [ "$t2" = "16" ]; then
        pass "T-7 重启后账目继续累加（${t1} → ${t2}）"
    else
        fail "T-7 重启后账目累加异常（${t1} → ${t2}）"
    fi
    rt_fix_stop_all
}

# ── T-8 日志可判读：启动/就绪/请求/失败四段均有可检索字段 ──
scn_t8() {
    rt_need_llm "T-8" || return 0
    rt_need_llama "T-8" || return 0

    local h log out
    h="$(rt_home t8)"
    log="$h/logs/llm.log"
    if ! rt_llama "$PORT_LLAMA" "$h/logs/engine.log"; then
        fail "T-8 引擎未就绪（见 $h/logs/engine.log）"
        rt_llama_stop
        return 0
    fi
    rt_cfg_local "$h" "http://127.0.0.1:$PORT_LLAMA/v1"
    rt_start "$h" "$log"
    if ! rt_wait_sock "$h" 20; then
        fail "T-8 llm_d 未就绪（见 $log）"
        rt_stop_llm
        rt_llama_stop
        return 0
    fi

    out="$(rt_rpc "$h/run/llm.sock" complete \
        "{\"model\":\"$MODEL_ID\",\"messages\":[{\"role\":\"user\",\"content\":\"gate t8\"}],\"max_tokens\":24}" 120)"
    rt_llama_stop
    out="$(rt_rpc "$h/run/llm.sock" complete \
        "{\"model\":\"$MODEL_ID\",\"messages\":[{\"role\":\"user\",\"content\":\"gate t8 down\"}],\"max_tokens\":24}" 60)"
    out="$(rt_rpc "$h/run/llm.sock" health_check '{}' 10)"

    grep -q 'C-L02: SVC: CREATE-OK' "$log" \
        && pass "T-8 启动段可判读（SVC CREATE-OK）" \
        || fail "T-8 缺启动段结构化日志"
    grep -Eq "Listening on .*llm\.sock|C-L08: REGISTER service='llm_d'.*endpoint=" "$log" \
        && pass "T-8 就绪段可判读（监听地址与注册端点）" \
        || fail "T-8 缺就绪段结构化日志"
    if grep -Eq 'C-L02: [A-Z]+: COMPLETE-START model=' "$log" \
        && grep -Eq 'C-L02: [A-Z]+: COMPLETE-OK model=' "$log"; then
        pass "T-8 请求段可判读（COMPLETE-START/OK）"
    else
        fail "T-8 缺请求段结构化日志"
    fi
    grep -Eq 'C-L02: [A-Z]+: COMPLETE-FAIL model=.*DIAGNOSIS=' "$log" \
        && pass "T-8 失败段可判读（COMPLETE-FAIL 带诊断码）" \
        || fail "T-8 缺失败段结构化日志"
    if grep -q 'NO-KEY env=' "$log"; then
        fail "T-8 本地模式误报 NO-KEY 引导"
    else
        pass "T-8 本地模式无误导性 NO-KEY 引导"
    fi
    rt_stop_llm
}

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
