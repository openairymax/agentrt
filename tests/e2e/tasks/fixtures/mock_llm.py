#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
"""L-8 端到端任务闭环回归：确定性 mock LLM（OpenAI chat/completions 兼容）。

设计要点：
  * 由 $MOCK_STATE 目录下的文本文件驱动运行期切换，测试脚本改文件即可注入；
  * 故障模式先发一次无害探针 shell_run，保证 jobs 审计中仍有真实工具调用，
    再在后续轮次注入故障，避免"故障用例没有工具调用"的假绿；
  * 工具调用以 SSE delta.tool_calls 下发，驱动 agent_d -> tool_d 的真实执行。

模式（$MOCK_STATE/mode）：
  text    正常终答（默认）
  install 首轮下发 $MOCK_STATE/command，之后终答
  tool    同 install（别名）
  schema  首轮探针，之后下发缺 command 的非法参数（schema 违例）
  unknown 首轮探针，之后下发不存在的工具名
  deny    始终下发 shell_run（配合 allow_dangerous=false 触发审批拒绝）
  fail500 首轮探针，之后返回 HTTP 500
  empty   首轮探针，之后返回空正文
  hang    首轮下发 command，工具完成后挂起 hang_after_tool 秒再终答
"""

import json
import os
import sys
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

STATE = os.environ.get("MOCK_STATE", "/tmp/mock-state")
PORT = int(os.environ.get("MOCK_PORT", "18135"))
PROBE = "printf l8-probe"
USAGE = {"prompt_tokens": 12, "completion_tokens": 6, "total_tokens": 18}


def _read(key, default=""):
    try:
        with open(os.path.join(STATE, key), encoding="utf-8") as f:
            return f.read().strip() or default
    except OSError:
        return default


def _ntools(messages):
    return sum(1 for m in messages if m.get("role") == "tool")


def _chunk(delta, finish=None):
    body = {"id": "chatcmpl-l8", "object": "chat.completion.chunk",
            "model": "mock-1", "choices": [{"index": 0, "delta": delta,
                                            "finish_reason": finish}]}
    return "data: " + json.dumps(body, ensure_ascii=False) + "\n\n"


def _call(cmd, name="shell_run", args=None):
    return ("tool", name, args if args is not None else {"command": cmd})


def _decide(messages):
    mode = _read("mode", "text")
    n = _ntools(messages)
    cmd = _read("command", "printf l8-tool")
    final = _read("final", "已完成")
    if mode == "fail500":
        return ("500",) if n >= 1 else _call(PROBE)
    if mode == "empty":
        return ("empty",) if n >= 1 else _call(PROBE)
    if mode == "schema":
        if n < 1:
            return _call(PROBE)
        # 每轮参数带 nonce：相同失败调用会被 run_repeat_blocked 拦截且不计
        # 连败，只有变化参数才能让三次真实失败触发熔断。
        return _call("", args={"cmd": "%s-%d" % (cmd, n)})
    if mode == "unknown":
        if n < 1:
            return _call(PROBE)
        return _call("", name="airyl8_no_such_tool", args={"command": "printf x-%d" % n})
    if mode == "deny":
        return _call("%s-%d" % (cmd, n))
    if mode == "hang":
        if n == 0:
            return _call(cmd)
        wait = _read("hang_after_tool", "0")
        if wait and float(wait) > 0:
            time.sleep(float(wait))
        return ("text", final)
    if mode in ("tool", "install"):
        if n == 0:
            return _call(cmd)
        return ("text", final)
    return ("text", final)


def _sse_text(text):
    out = _chunk({"role": "assistant", "content": text})
    final = {"id": "chatcmpl-l8", "object": "chat.completion.chunk", "model": "mock-1",
             "choices": [{"index": 0, "delta": {}, "finish_reason": "stop"}],
             "usage": USAGE}
    out += "data: " + json.dumps(final) + "\n\n"
    return out + "data: [DONE]\n\n"


def _sse_tool(name, args):
    fn = {"name": name, "arguments": json.dumps(args, ensure_ascii=False)}
    delta = {"role": "assistant", "content": None,
             "tool_calls": [{"index": 0, "id": "call_l8_1", "type": "function",
                             "function": fn}]}
    out = _chunk(delta)
    final = {"id": "chatcmpl-l8", "object": "chat.completion.chunk", "model": "mock-1",
             "choices": [{"index": 0, "delta": {}, "finish_reason": "tool_calls"}],
             "usage": USAGE}
    out += "data: " + json.dumps(final) + "\n\n"
    return out + "data: [DONE]\n\n"


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *args):  # 静默访问日志，测试日志只留错误
        pass

    def _send(self, code, body, ctype="application/json"):
        raw = body.encode("utf-8") if isinstance(body, str) else body
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)

    def do_GET(self):
        if self.path.rstrip("/").endswith("/models"):
            data = {"object": "list", "data": [{"id": "mock-1", "object": "model"}]}
            self._send(200, json.dumps(data))
            return
        self._send(404, json.dumps({"error": "not found"}))

    def do_POST(self):
        n = int(self.headers.get("Content-Length", "0") or "0")
        try:
            req = json.loads(self.rfile.read(n) or b"{}")
        except ValueError:
            self._send(400, json.dumps({"error": "bad json"}))
            return
        verdict = _decide(req.get("messages") or [])
        if verdict[0] == "500":
            self._send(500, json.dumps({"error": {"message": "mock upstream failure"}}))
            return
        if verdict[0] == "empty":
            self._send(200, _sse_text(""), "text/event-stream")
            return
        if verdict[0] == "tool":
            self._send(200, _sse_tool(verdict[1], verdict[2]), "text/event-stream")
            return
        self._send(200, _sse_text(verdict[1]), "text/event-stream")


def main():
    os.makedirs(STATE, exist_ok=True)
    srv = ThreadingHTTPServer(("127.0.0.1", PORT), Handler)
    sys.stderr.write("mock_llm listening on %d state=%s\n" % (PORT, STATE))
    srv.serve_forever()


if __name__ == "__main__":
    main()
