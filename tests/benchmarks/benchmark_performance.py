#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 SPHARX Ltd.
# SPDX-License-Identifier: GPL-3.0-or-later
"""
AgentOS Performance Benchmark Suite

Benchmarks:
  1. JSON-RPC request throughput
  2. Protocol translation latency
  3. IPC service bus message throughput
  4. Service discovery lookup latency
  5. Circuit breaker state transition latency
  6. Metrics export latency
  7. Configuration read/write throughput

Usage:
    python benchmark_performance.py [--gateway URL] [--iterations N] [--json]
"""

import sys
import os
import json
import time
import statistics
import argparse
from typing import List, Dict

GATEWAY_URL = os.environ.get("AGENTOS_GATEWAY_URL", "http://localhost:18789")


def http_post(url_path: str, data: Dict, timeout: int = 10):
    import urllib.request
    import urllib.error
    url = f"{GATEWAY_URL}{url_path}"
    body = json.dumps(data).encode()
    headers = {"Content-Type": "application/json"}
    req = urllib.request.Request(url, data=body, headers=headers, method="POST")
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            return resp.status, json.loads(resp.read().decode())
    except Exception as e:
        return 0, {"error": str(e)}


def jsonrpc(method: str, params: Dict = None):
    return http_post("/jsonrpc", {
        "jsonrpc": "2.0", "id": "1", "method": method, "params": params or {}
    })


def run_benchmark(name: str, func, iterations: int = 100) -> Dict:
    latencies = []
    errors = 0

    for _ in range(iterations):
        try:
            start = time.perf_counter()
            result = func()
            latency = (time.perf_counter() - start) * 1000

            if isinstance(result, tuple) and result[0] > 0 and result[0] < 500:
                latencies.append(latency)
            else:
                errors += 1
        except Exception:
            errors += 1

    if not latencies:
        return {
            "name": name,
            "iterations": iterations,
            "errors": errors,
            "status": "FAILED",
        }

    latencies.sort()
    return {
        "name": name,
        "iterations": iterations,
        "errors": errors,
        "status": "OK" if errors < iterations // 2 else "DEGRADED",
        "latency_ms": {
            "min": round(min(latencies), 3),
            "max": round(max(latencies), 3),
            "mean": round(statistics.mean(latencies), 3),
            "median": round(statistics.median(latencies), 3),
            "p95": round(latencies[int(len(latencies) * 0.95)], 3),
            "p99": round(latencies[int(len(latencies) * 0.99)], 3),
            "stdev": round(statistics.stdev(latencies), 3) if len(latencies) > 1 else 0,
        },
        "throughput_rps": round(iterations / (sum(latencies) / 1000), 1) if sum(latencies) > 0 else 0,
    }


def main():
    parser = argparse.ArgumentParser(description="AgentOS Performance Benchmarks")
    parser.add_argument("--gateway", default=GATEWAY_URL)
    parser.add_argument("--iterations", "-n", type=int, default=100)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    global GATEWAY_URL
    GATEWAY_URL = args.gateway

    print(f"\n{'=' * 60}")
    print(f"  AgentOS Performance Benchmark Suite")
    print(f"  Gateway: {GATEWAY_URL}")
    print(f"  Iterations: {args.iterations}")
    print(f"{'=' * 60}")

    benchmarks = [
        ("JSON-RPC service.list", lambda: jsonrpc("service.list")),
        ("JSON-RPC agent.list", lambda: jsonrpc("agent.list")),
        ("JSON-RPC task.list", lambda: jsonrpc("task.list")),
        ("Protocol translate MCP", lambda: jsonrpc("protocol.translate", {"protocol": "mcp", "method": "tools/list"})),
        ("Protocol translate A2A", lambda: jsonrpc("protocol.translate", {"protocol": "a2a", "method": "agent/discover"})),
        ("Service discovery", lambda: jsonrpc("service.discovery.discover", {"service_name": "gateway"})),
        ("Config get", lambda: jsonrpc("config.get", {"key": "agentos.version"})),
        ("Health check", lambda: http_post("/jsonrpc", {"jsonrpc": "2.0", "id": "1", "method": "health.check"})),
    ]

    results = []
    for name, func in benchmarks:
        print(f"\n  Running: {name}...")
        result = run_benchmark(name, func, args.iterations)
        results.append(result)

        if result["status"] != "FAILED":
            lat = result["latency_ms"]
            print(f"    Mean: {lat['mean']:.1f}ms  P95: {lat['p95']:.1f}ms  "
                  f"P99: {lat['p99']:.1f}ms  TPS: {result['throughput_rps']:.0f}")
        else:
            print(f"    FAILED ({result['errors']} errors)")

    print(f"\n{'=' * 60}")
    print(f"  Benchmark Summary")
    print(f"{'=' * 60}")

    ok_count = sum(1 for r in results if r["status"] == "OK")
    degraded = sum(1 for r in results if r["status"] == "DEGRADED")
    failed = sum(1 for r in results if r["status"] == "FAILED")

    print(f"  OK: {ok_count}  Degraded: {degraded}  Failed: {failed}")

    if args.json:
        report = {
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%S"),
            "gateway_url": GATEWAY_URL,
            "iterations": args.iterations,
            "benchmarks": results,
        }
        print(json.dumps(report, indent=2, ensure_ascii=False))

    return 1 if failed > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
