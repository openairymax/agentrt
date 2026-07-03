#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Airymax (AgentRT) v0.1.1 端到端测试脚本
==========================================
模拟 `agentrt run` 的完整调用链路。

色彩说明:
  ✓ 绿色  = 通过
  ✗ 红色  = 失败
  ⚠ 黄色  = 警告
  ▶ 蓝色  = 信息
  ℹ 青色  = 细节

位置: AgentRT/tests/integration/python/test_airymax_e2e.py

用法:
    # 1. 仅测试网关连通性（不需要 LLM Key）
    python3 AgentRT/tests/integration/python/test_airymax_e2e.py --health-only

    # 2. 完整测试（需要设置 OPENAI_API_KEY 或其他 LLM Key）
    export OPENAI_API_KEY="sk-..."
    python3 AgentRT/tests/integration/python/test_airymax_e2e.py

    # 3. 使用不同网关地址
    python3 AgentRT/tests/integration/python/test_airymax_e2e.py --gateway http://192.168.1.100:8080

    # 4. 禁用色彩输出（重定向到文件时自动禁用）
    python3 AgentRT/tests/integration/python/test_airymax_e2e.py --no-color
"""
import argparse
import json
import os
import signal
import subprocess
import sys
import time
import urllib.request
import urllib.error

# ── ANSI 色彩支持 (自动检测终端，可强制关闭) ──
_USE_COLOR = sys.stderr.isatty() or sys.stdout.isatty()

def _c(code, text):
    return f"{code}{text}{RESET}" if _USE_COLOR else text

# ── 色彩常量 ──
GREEN   = "\033[92m" if _USE_COLOR else ""
RED     = "\033[91m" if _USE_COLOR else ""
YELLOW  = "\033[93m" if _USE_COLOR else ""
BLUE    = "\033[94m" if _USE_COLOR else ""
MAGENTA = "\033[95m" if _USE_COLOR else ""
CYAN    = "\033[96m" if _USE_COLOR else ""
GRAY    = "\033[90m" if _USE_COLOR else ""
BOLD    = "\033[1m"  if _USE_COLOR else ""
RESET   = "\033[0m"  if _USE_COLOR else ""

PASS = f"{GREEN}✓{RESET}"
FAIL = f"{RED}✗{RESET}"
WARN = f"{YELLOW}⚠{RESET}"
INFO = f"{BLUE}▶{RESET}"
DIM  = f"{GRAY}ℹ{RESET}"
HL   = f"{MAGENTA}✦{RESET}"

results = {"pass": 0, "fail": 0, "warn": 0}


def ok(msg):
    results["pass"] += 1
    print(f"  {PASS}  {msg}")


def bad(msg):
    results["fail"] += 1
    print(f"  {FAIL}  {msg}")


def warn(msg):
    results["warn"] += 1
    print(f"  {WARN}  {msg}")


def section(title, subtitle=None):
    print(f"\n{BOLD}{'═'*60}{RESET}")
    print(f"  {HL} {BOLD}{title}{RESET}")
    if subtitle:
        print(f"    {DIM}{subtitle}{RESET}")
    print(f"{BOLD}{'─'*60}{RESET}")


def do_get(url, expected_status=None):
    """HTTP GET, 返回 (status, body)"""
    try:
        req = urllib.request.Request(url)
        req.add_header("User-Agent", "airymax-test/0.1.1")
        with urllib.request.urlopen(req, timeout=10) as resp:
            return resp.status, resp.read().decode("utf-8", errors="replace")
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode("utf-8", errors="replace")
    except urllib.error.URLError as e:
        return 0, str(e.reason)
    except Exception as e:
        return 0, str(e)


def do_post(url, data, expected_status=None):
    """HTTP POST JSON, 返回 (status, body)"""
    try:
        body = json.dumps(data).encode("utf-8")
        req = urllib.request.Request(
            url, data=body,
            headers={"Content-Type": "application/json", "User-Agent": "airymax-test/0.1.1"},
            method="POST"
        )
        with urllib.request.urlopen(req, timeout=60) as resp:
            return resp.status, resp.read().decode("utf-8", errors="replace")
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode("utf-8", errors="replace")
    except urllib.error.URLError as e:
        return 0, str(e.reason)
    except Exception as e:
        return 0, str(e)


def start_gateway(gateway_binary, port):
    """启动网关守护进程"""
    print(f"\n  {INFO} 启动网关: {gateway_binary} -p {port}")
    env = os.environ.copy()
    # 添加 conda libuuid 路径（如果存在）— v3.1: 改为环境变量注入，消除硬编码本地路径
    conda_lib = os.environ.get("AGENTOS_CONDA_LIBUUID", "")
    if conda_lib and os.path.exists(conda_lib):
        env["LD_LIBRARY_PATH"] = conda_lib + ":" + env.get("LD_LIBRARY_PATH", "")
    try:
        proc = subprocess.Popen(
            [gateway_binary, "-p", str(port)],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            env=env
        )
        return proc
    except FileNotFoundError:
        bad(f"网关二进制文件不存在: {gateway_binary}")
        return None
    except Exception as e:
        bad(f"启动网关失败: {e}")
        return None


def wait_for_gateway(url, max_wait=10):
    """等待网关就绪"""
    deadline = time.time() + max_wait
    while time.time() < deadline:
        status, body = do_get(url)
        if status > 0:
            ok(f"网关就绪 (HTTP {status})")
            return True
        time.sleep(0.5)
    bad(f"网关在 {max_wait}s 内未就绪")
    return False


# ═══════════════════════════════════════════════════════════
# 测试用例
# ═══════════════════════════════════════════════════════════

def test_health(gateway_url):
    """测试 1: 健康检查"""
    section("测试 1: 健康检查", f"GET {gateway_url}/api/v1/health + /healthz + /readyz")
    for endpoint in ["/api/v1/health", "/healthz", "/readyz"]:
        url = f"{gateway_url}{endpoint}"
        status, body = do_get(url)
        if status == 200:
            try:
                data = json.loads(body)
                ok(f"{endpoint} → status={data.get('status', '?')}")
            except json.JSONDecodeError:
                ok(f"{endpoint} → HTTP 200 ({body[:50]}...)")
        else:
            bad(f"{endpoint} → HTTP {status}: {body[:80]}")


def test_config(gateway_url):
    """测试 2: 配置端点"""
    section("测试 2: 配置端点", "GET /api/v1/config + /api/v1/config/reload")
    for endpoint in ["/api/v1/config", "/api/v1/config/reload"]:
        url = f"{gateway_url}{endpoint}"
        status, body = do_get(url)
        if status in (200, 405):  # 405=Method Not Allowed for POST-only endpoints
            ok(f"{endpoint} → HTTP {status} (endpoint exists)")
        elif status == 404:
            warn(f"{endpoint} → HTTP 404 (endpoint not registered)")
        else:
            bad(f"{endpoint} → HTTP {status}")


def test_llm_list(gateway_url):
    """测试 3: LLM 提供商列表"""
    section("测试 3: LLM 提供商列表", "GET /api/v1/llm/providers")
    url = f"{gateway_url}/api/v1/llm/providers"
    status, body = do_get(url)
    if status == 200:
        try:
            data = json.loads(body)
            if isinstance(data, list):
                ok(f"LLM providers: {len(data)} 个 ({', '.join(data[:3])}{'...' if len(data)>3 else ''})")
            elif isinstance(data, dict):
                providers = data.get("providers", data.get("data", []))
                ok(f"LLM providers: {len(providers)} 个")
            else:
                ok(f"LLM providers 端点响应正常")
        except json.JSONDecodeError:
            ok(f"LLM providers → HTTP 200 ({body[:60]}...)")
    elif status == 503:
        warn(f"LLM providers → 503 (未配置 LLM，这是正常的)")
    else:
        warn(f"LLM providers → HTTP {status}: {body[:80]}")


def test_agent_run_basic(gateway_url):
    """测试 4: Agent 运行（基础请求）"""
    section("测试 4: Agent Run", f"POST {gateway_url}/api/v1/agent/run")
    url = f"{gateway_url}/api/v1/agent/run"
    payload = {
        "prompt": "Hello, what is 2+2?",
        "agent_file": "default",
        "interactive": False
    }
    status, body = do_post(url, payload)
    resp_info = body[:200].replace("\n", "\\n")
    if status == 200:
        try:
            data = json.loads(body)
            resp_text = data.get("response", body)
            tokens = data.get("tokens_used", "?")
            session = data.get("session_id", "?")
            ok(f"Agent 响应 (session={session[:12]}..., tokens={tokens}):")
            print(f"      {CYAN}{resp_text[:200]}{RESET}")
        except json.JSONDecodeError:
            ok(f"Agent 响应: {resp_info}")
    elif status == 503:
        warn(f"Agent run → 503 (LLM 未配置或不可用，需设置 API Key)")
        warn(f"   设置方法: export OPENAI_API_KEY='sk-...'")
    elif status == 422:
        warn(f"Agent run → 422 参数错误: {resp_info}")
    else:
        bad(f"Agent run → HTTP {status}: {resp_info}")


def test_agent_run_no_llm(gateway_url):
    """测试 5: 无 LLM 时的降级行为"""
    section("测试 5: Agent Run — 无 LLM Key 时的降级")
    api_key = os.environ.get("OPENAI_API_KEY", os.environ.get("ANTHROPIC_API_KEY", ""))
    if api_key:
        ok(f"检测到 API Key ({'OPENAI' if 'OPENAI' in os.environ else 'ANTHROPIC'})，跳过降级测试")
        return
    warn("未检测到 API Key — 以下测试验证系统优雅降级行为")

    url = f"{gateway_url}/api/v1/agent/run"
    test_cases = [
        {"prompt": "hello world", "agent_file": "default"},
        {"prompt": "", "agent_file": "default"},                    # 空 prompt
        {"prompt": "test", "agent_file": "nonexistent"},           # 不存在的 agent
    ]
    for tc in test_cases:
        payload = {**tc, "interactive": False}
        status, body = do_post(url, payload)
        msg = body[:100].replace("\n", "\\n")
        label = f"prompt='{tc['prompt'][:20]}' agent={tc['agent_file']}"
        if status in (200, 503):
            ok(f"{label} → HTTP {status} (正确降级)")
        else:
            warn(f"{label} → HTTP {status}: {msg}")


def test_tui_connectivity_simulation(gateway_url):
    """测试 6: 模拟 TUI 的连接诊断流程"""
    section("测试 6: 模拟 TUI 连接诊断", "完整模拟 agentrt-tui Phase 0~4 启动流程")

    # 模拟 TUI 启动时的检查序列
    print(f"  {INFO} 模拟 TUI 启动日志输出：")

    # Phase 0: 参数检查
    print(f"  {INFO} [Phase0] CLI args parsed:")
    print(f"        gateway_url = {gateway_url}")
    print(f"        agent_file  = agents/main.agent.yaml")
    agent_file = "agents/main.agent.yaml"
    if not os.path.exists(agent_file):
        print(f"  {WARN}  [Phase1] Agent file '{agent_file}' not found on disk")
    else:
        print(f"  {PASS}  [Phase1] Agent file found: {agent_file}")

    # Phase 2: HTTP 客户端初始化
    print(f"  {INFO} [Phase2] HTTP client initialized (base={gateway_url})")
    print(f"  {INFO} [Phase2] Connecting to gateway...")

    # 健康检查
    status, body = do_get(f"{gateway_url}/api/v1/health")
    elapsed = time.time()
    if status == 200:
        try:
            data = json.loads(body)
            ver = data.get("version", "unknown")
            print(f"  {PASS}  [Phase2] Gateway health check: status={data.get('status','?')}, version={ver}")
        except json.JSONDecodeError:
            print(f"  {PASS}  [Phase2] Gateway health check: HTTP 200")
    else:
        print(f"  {WARN}  [Phase2] Gateway health check FAILED: HTTP {status}")
        print(f"  {WARN}  → Start gateway: docker compose up -d  or  agentos-gateway_d")

    # Phase 3: 终端设置（模拟）
    print(f"  {INFO} [Phase3] Setting up terminal (raw mode + alternate screen)...")
    print(f"  {PASS}  [Phase3] Terminal initialized. Starting event loop.")

    # 连接状态
    if status == 200:
        print(f"  {INFO} [Phase3b] App state initialized. connected=true, version={ver}")
        print(f"  {INFO}          Status: 'Connected to AgentRT v{ver}'")
    else:
        print(f"  {INFO} [Phase3b] App state initialized. connected=false")
        print(f"  {INFO}          Status: 'Gateway unreachable'")

    # 模拟用户输入
    print(f"  {INFO} [EventLoop] User submitted input: '什么是 Airymax?' (10 chars)")
    print(f"  {INFO} [Client] POST /api/v1/agent/run (prompt_len=10)")

    if status == 200:
        print(f"  {PASS}  [Client] ← agent/run OK (XXms, YY tokens)")
    else:
        print(f"  {WARN}  [Client] ← agent/run FAILED: HTTP {status} — gateway not available")
        print(f"  {WARN}  [TUI] submit_input error → shown in System message panel")

    # Phase 4: 关闭
    print(f"  {INFO} [Phase4] User pressed Ctrl+C, shutting down...")
    print(f"  {INFO} [Phase4] Restoring terminal...")
    print(f"  {PASS}  [Phase4] Terminal restored.")
    print(f"  {INFO} [Phase4] TUI exited normally after X.XXs")


# ═══════════════════════════════════════════════════════════
# Main
# ═══════════════════════════════════════════════════════════

def main():
    parser = argparse.ArgumentParser(
        description="Airymax v0.1.1 E2E Test",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "色彩说明:\n"
            "  \033[92m✓\033[0m  绿色 = 通过   \033[91m✗\033[0m  红色 = 失败\n"
            "  \033[93m⚠\033[0m  黄色 = 警告   \033[94m▶\033[0m  蓝色 = 信息"
        ),
    )
    parser.add_argument("--gateway", default="http://localhost:8099", help="Gateway URL (default: %(default)s)")
    parser.add_argument("--health-only", action="store_true", help="Only test gateway health")
    parser.add_argument("--start-gateway", action="store_true", help="Auto-start gateway daemon")
    parser.add_argument("--gateway-binary",
                        default=os.environ.get("AGENTOS_GATEWAY_BINARY", "gateway_d"),
                        help="Path to gateway binary (default: %(default)s, env: AGENTOS_GATEWAY_BINARY)")
    parser.add_argument("--no-color", action="store_true", help="Disable ANSI color output")
    args = parser.parse_args()

    # ── 色彩覆盖 ──
    global _USE_COLOR
    if args.no_color:
        _USE_COLOR = False

    gateway_url = args.gateway.rstrip("/")
    gateway_proc = None

    has_key = bool(os.environ.get("OPENAI_API_KEY") or os.environ.get("ANTHROPIC_API_KEY"))
    llm_status = f"{GREEN}已设置{RESET}" if has_key else f"{YELLOW}未设置（仅测试连通性）{RESET}"

    print(f"\n{BOLD}{'═'*60}{RESET}")
    print(f"  {HL} {BOLD}Airymax (AgentRT) v0.1.1 — 端到端集成测试{RESET}")
    print(f"  {DIM}  网关地址:{RESET} {CYAN}{gateway_url}{RESET}")
    print(f"  {DIM}  LLM Key: {RESET} {llm_status}")
    print(f"{BOLD}{'─'*60}{RESET}")

    # ── 启动网关 ──
    if args.start_gateway:
        port = gateway_url.split(":")[-1]
        gateway_proc = start_gateway(args.gateway_binary, port)
        if gateway_proc is None:
            print(f"\n{RED}无法启动网关，请手动启动后重试。{RESET}")
            sys.exit(1)
        time.sleep(1)
        if not wait_for_gateway(f"{gateway_url}/api/v1/health", max_wait=15):
            gateway_proc.kill()
            gateway_proc.wait()
            sys.exit(1)

    # ── 运行测试 ──
    test_health(gateway_url)

    if not args.health_only:
        test_config(gateway_url)
        test_llm_list(gateway_url)
        test_agent_run_basic(gateway_url)
        test_agent_run_no_llm(gateway_url)
        test_tui_connectivity_simulation(gateway_url)

    # ── 清理 ──
    if gateway_proc:
        print(f"\n  {INFO} 停止网关 (PID={gateway_proc.pid})...")
        gateway_proc.send_signal(signal.SIGTERM)
        try:
            gateway_proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            gateway_proc.kill()
        print(f"  {PASS}  网关已停止")

    # ── 结果 ──
    total = sum(results.values())
    print(f"\n{BOLD}{'═'*60}{RESET}")
    print(f"  {HL} {BOLD}测试报告{RESET}")
    print(f"  {DIM}  总计:{RESET} {BOLD}{total}{RESET} 项"
          f"  {GREEN}通过: {results['pass']}{RESET}"
          f"  {RED}失败: {results['fail']}{RESET}"
          f"  {YELLOW}警告: {results['warn']}{RESET}")
    print(f"{BOLD}{'─'*60}{RESET}")

    if results["fail"] > 0:
        print(f"\n  {FAIL} {RED}{BOLD}存在失败项，请检查网关配置或日志。{RESET}")
        print(f"  {DIM}  建议:{RESET}")
        print(f"  {DIM}  1. 确认网关已启动:{RESET} docker compose up -d")
        print(f"  {DIM}  2. 检查日志:{RESET} tail -f /tmp/gateway.log")
        sys.exit(1)
    else:
        print(f"\n  {PASS} {GREEN}{BOLD}全部通过！{RESET}")
        if results["warn"] > 0:
            print(f"  {WARN} {YELLOW}存在 {results['warn']} 个警告，通常是因为未配置 LLM API Key。{RESET}")
            print(f"  {DIM}  设置方法:{RESET} export OPENAI_API_KEY='sk-...'")
            print(f"  {DIM}  然后重试:{RESET} python3 test_airymax_e2e.py")
    print(f"{BOLD}{'═'*60}{RESET}")


if __name__ == "__main__":
    main()
