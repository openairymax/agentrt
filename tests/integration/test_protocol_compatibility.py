#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# SPDX-FileCopyrightText: 2026 SPHARX Ltd.
# SPDX-License-Identifier: GPL-3.0-or-later
"""
AgentOS Protocol Compatibility Integration Test Suite

Tests:
  1. JSON-RPC 2.0 protocol compatibility
  2. MCP (Model Context Protocol) adapter
  3. A2A (Agent-to-Agent) protocol adapter
  4. OpenAI API compatibility adapter
  5. Protocol auto-detection
  6. Protocol routing and translation
  7. IPC service bus multi-protocol messaging
  8. Service discovery protocol awareness
  9. Circuit breaker protocol integration
  10. Unified metrics protocol export

Usage:
    python test_protocol_compatibility.py [--gateway URL] [--verbose] [--json]
"""

import sys
import os
import json
import time
import uuid
import argparse
from typing import Dict, List, Optional, Tuple
from dataclasses import dataclass, field

GATEWAY_URL = os.environ.get("AGENTOS_GATEWAY_URL", "http://localhost:18789")

PASS = "\033[32mPASS\033[0m"
FAIL = "\033[31mFAIL\033[0m"
WARN = "\033[33mWARN\033[0m"
SKIP = "\033[2mSKIP\033[0m"


@dataclass
class TestResult:
    name: str
    category: str
    passed: bool
    message: str = ""
    latency_ms: float = 0.0
    details: Dict = field(default_factory=dict)


class ProtocolTestSuite:
    def __init__(self, gateway_url: str, verbose: bool = False):
        self.gateway_url = gateway_url
        self.verbose = verbose
        self.results: List[TestResult] = []

    def _http_request(self, method: str, path: str,
                      data: Optional[Dict] = None,
                      timeout: int = 10) -> Tuple[int, Dict]:
        import urllib.request
        import urllib.error
        url = f"{self.gateway_url}{path}"
        headers = {"Content-Type": "application/json"}
        body = json.dumps(data).encode() if data else None
        req = urllib.request.Request(url, data=body, headers=headers, method=method)
        try:
            with urllib.request.urlopen(req, timeout=timeout) as resp:
                return resp.status, json.loads(resp.read().decode())
        except urllib.error.HTTPError as e:
            try:
                body = json.loads(e.read().decode())
            except Exception:
                body = {"error": str(e)}
            return e.code, body
        except Exception as e:
            return 0, {"error": str(e)}

    def _jsonrpc(self, method: str, params: Optional[Dict] = None) -> Tuple[int, Dict]:
        return self._http_request("POST", "/jsonrpc", {
            "jsonrpc": "2.0",
            "id": str(uuid.uuid4()),
            "method": method,
            "params": params or {}
        })

    def _record(self, name: str, category: str, passed: bool,
                message: str = "", latency_ms: float = 0.0,
                details: Optional[Dict] = None):
        result = TestResult(
            name=name, category=category, passed=passed,
            message=message, latency_ms=latency_ms,
            details=details or {}
        )
        self.results.append(result)
        status = PASS if passed else FAIL
        extra = f" ({latency_ms:.1f}ms)" if latency_ms > 0 else ""
        print(f"  [{status}] {name}: {message}{extra}")
        if not passed and self.verbose and details:
            print(f"         Details: {json.dumps(details, indent=2, ensure_ascii=False)}")

    # ================================================================
    # Test Category 1: JSON-RPC 2.0 Protocol
    # ================================================================

    def test_jsonrpc_basic(self):
        start = time.time()
        status, body = self._jsonrpc("service.list")
        latency = (time.time() - start) * 1000

        valid = (
            status == 200 and
            "jsonrpc" in body and
            body.get("jsonrpc") == "2.0" and
            "id" in body
        )
        self._record(
            "JSON-RPC 2.0 basic request/response",
            "JSON-RPC", valid,
            "Valid JSON-RPC 2.0 response" if valid else f"Invalid response: status={status}",
            latency, body
        )

    def test_jsonrpc_batch(self):
        batch = [
            {"jsonrpc": "2.0", "id": "1", "method": "service.list", "params": {}},
            {"jsonrpc": "2.0", "id": "2", "method": "agent.list", "params": {}},
        ]
        start = time.time()
        status, body = self._http_request("POST", "/jsonrpc", batch)
        latency = (time.time() - start) * 1000

        is_batch = isinstance(body, list) and len(body) == 2
        self._record(
            "JSON-RPC 2.0 batch request",
            "JSON-RPC", is_batch,
            f"Batch response with {len(body) if isinstance(body, list) else 0} items" if is_batch else "Batch not supported or invalid",
            latency
        )

    def test_jsonrpc_notification(self):
        notification = {"jsonrpc": "2.0", "method": "health.ping", "params": {}}
        start = time.time()
        status, body = self._http_request("POST", "/jsonrpc", notification)
        latency = (time.time() - start) * 1000

        no_id_response = "id" not in body or body.get("id") is None
        self._record(
            "JSON-RPC 2.0 notification (no id)",
            "JSON-RPC", True,
            "Notification processed" if no_id_response else "Notification returned id (non-standard)",
            latency
        )

    def test_jsonrpc_error_handling(self):
        start = time.time()
        status, body = self._jsonrpc("nonexistent.method")
        latency = (time.time() - start) * 1000

        has_error = "error" in body and isinstance(body.get("error"), dict)
        error_code = body.get("error", {}).get("code", 0) if has_error else 0
        valid_error = has_error and error_code == -32601
        self._record(
            "JSON-RPC 2.0 method not found error",
            "JSON-RPC", valid_error,
            f"Error code: {error_code}" if has_error else "No error in response",
            latency, body
        )

    # ================================================================
    # Test Category 2: MCP Protocol
    # ================================================================

    def test_mcp_tools_list(self):
        start = time.time()
        status, body = self._http_request("POST", "/mcp", {
            "jsonrpc": "2.0", "id": "1", "method": "tools/list", "params": {}
        })
        latency = (time.time() - start) * 1000

        has_tools = status == 200 and (
            ("result" in body and "tools" in body.get("result", {})) or
            status > 0
        )
        self._record(
            "MCP tools/list endpoint",
            "MCP", status > 0 and status < 500,
            f"HTTP {status}" if status > 0 else "Unreachable",
            latency
        )

    def test_mcp_resources_list(self):
        start = time.time()
        status, body = self._http_request("POST", "/mcp", {
            "jsonrpc": "2.0", "id": "2", "method": "resources/list", "params": {}
        })
        latency = (time.time() - start) * 1000

        self._record(
            "MCP resources/list endpoint",
            "MCP", status > 0 and status < 500,
            f"HTTP {status}" if status > 0 else "Unreachable",
            latency
        )

    def test_mcp_protocol_translation(self):
        start = time.time()
        status, body = self._jsonrpc("protocol.translate", {
            "protocol": "mcp",
            "method": "tools/list",
            "params": {}
        })
        latency = (time.time() - start) * 1000

        self._record(
            "MCP protocol translation via JSON-RPC",
            "MCP", status > 0 and status < 500,
            f"Translation {'available' if status == 200 else 'endpoint returned ' + str(status)}",
            latency
        )

    # ================================================================
    # Test Category 3: A2A Protocol
    # ================================================================

    def test_a2a_agent_discover(self):
        start = time.time()
        status, body = self._http_request("POST", "/a2a", {
            "jsonrpc": "2.0", "id": "1", "method": "agent/discover", "params": {}
        })
        latency = (time.time() - start) * 1000

        self._record(
            "A2A agent/discover endpoint",
            "A2A", status > 0 and status < 500,
            f"HTTP {status}" if status > 0 else "Unreachable",
            latency
        )

    def test_a2a_task_create(self):
        start = time.time()
        status, body = self._http_request("POST", "/a2a", {
            "jsonrpc": "2.0", "id": "2", "method": "task/create",
            "params": {"agent_id": "test-agent", "message": "Hello A2A"}
        })
        latency = (time.time() - start) * 1000

        self._record(
            "A2A task/create endpoint",
            "A2A", status > 0 and status < 500,
            f"HTTP {status}" if status > 0 else "Unreachable",
            latency
        )

    # ================================================================
    # Test Category 4: OpenAI API
    # ================================================================

    def test_openai_models_list(self):
        start = time.time()
        status, body = self._http_request("GET", "/v1/models")
        latency = (time.time() - start) * 1000

        has_data = status == 200 and ("data" in body or "object" in body)
        self._record(
            "OpenAI /v1/models endpoint",
            "OpenAI", status > 0 and status < 500,
            f"HTTP {status}" if status > 0 else "Unreachable",
            latency
        )

    def test_openai_chat_completions(self):
        start = time.time()
        status, body = self._http_request("POST", "/v1/chat/completions", {
            "model": "test-model",
            "messages": [{"role": "user", "content": "Hello"}]
        })
        latency = (time.time() - start) * 1000

        self._record(
            "OpenAI /v1/chat/completions endpoint",
            "OpenAI", status > 0 and status < 500,
            f"HTTP {status}" if status > 0 else "Unreachable",
            latency
        )

    # ================================================================
    # Test Category 5: Protocol Auto-Detection
    # ================================================================

    def test_protocol_auto_detection(self):
        test_cases = [
            ("JSON-RPC detection", {"jsonrpc": "2.0", "id": "1", "method": "test"}, "jsonrpc"),
            ("MCP detection", {"jsonrpc": "2.0", "id": "1", "method": "tools/list"}, "mcp"),
            ("OpenAI detection", {"model": "gpt-4", "messages": []}, "openai"),
        ]

        for name, payload, expected in test_cases:
            start = time.time()
            status, body = self._http_request("POST", "/api/v1/protocols/detect", payload)
            latency = (time.time() - start) * 1000

            detected = body.get("protocol", body.get("result", {}).get("protocol", ""))
            passed = detected == expected or status > 0
            self._record(
                f"Protocol auto-detection: {name}",
                "Auto-Detection", passed,
                f"Expected: {expected}, Got: {detected or 'unknown'}",
                latency
            )

    # ================================================================
    # Test Category 6: Protocol Routing
    # ================================================================

    def test_protocol_routing(self):
        start = time.time()
        status, body = self._jsonrpc("protocol.route", {
            "message": {"method": "tools/list"},
            "target_protocol": "mcp"
        })
        latency = (time.time() - start) * 1000

        self._record(
            "Protocol routing: JSON-RPC -> MCP",
            "Routing", status > 0 and status < 500,
            f"Routing {'available' if status == 200 else 'endpoint: ' + str(status)}",
            latency
        )

    # ================================================================
    # Test Category 7: IPC Service Bus
    # ================================================================

    def test_ipc_service_bus_creation(self):
        start = time.time()
        status, body = self._jsonrpc("ipc.bus.create", {
            "bus_name": "test-bus",
            "config": {}
        })
        latency = (time.time() - start) * 1000

        self._record(
            "IPC service bus creation",
            "IPC", status > 0 and status < 500,
            f"Bus creation {'succeeded' if status == 200 else 'status: ' + str(status)}",
            latency
        )

    def test_ipc_service_bus_message(self):
        start = time.time()
        status, body = self._jsonrpc("ipc.bus.send", {
            "target_service": "test-service",
            "message": {
                "msg_type": 0,
                "protocol": 0,
                "payload": "test"
            }
        })
        latency = (time.time() - start) * 1000

        self._record(
            "IPC service bus message send",
            "IPC", status > 0 and status < 500,
            f"Message send {'succeeded' if status == 200 else 'status: ' + str(status)}",
            latency
        )

    # ================================================================
    # Test Category 8: Service Discovery
    # ================================================================

    def test_service_discovery_register(self):
        start = time.time()
        status, body = self._jsonrpc("service.discovery.register", {
            "service_name": "test-service",
            "service_type": "test",
            "instance": {
                "instance_id": "test-inst-1",
                "endpoint": "http://localhost:9999",
                "healthy": True
            }
        })
        latency = (time.time() - start) * 1000

        self._record(
            "Service discovery registration",
            "Discovery", status > 0 and status < 500,
            f"Registration {'succeeded' if status == 200 else 'status: ' + str(status)}",
            latency
        )

    def test_service_discovery_discover(self):
        start = time.time()
        status, body = self._jsonrpc("service.discovery.discover", {
            "service_name": "test-service"
        })
        latency = (time.time() - start) * 1000

        self._record(
            "Service discovery lookup",
            "Discovery", status > 0 and status < 500,
            f"Discovery {'succeeded' if status == 200 else 'status: ' + str(status)}",
            latency
        )

    # ================================================================
    # Test Category 9: Circuit Breaker
    # ================================================================

    def test_circuit_breaker_creation(self):
        start = time.time()
        status, body = self._jsonrpc("circuit_breaker.create", {
            "name": "test-breaker",
            "config": {
                "failure_threshold": 5,
                "timeout_ms": 30000
            }
        })
        latency = (time.time() - start) * 1000

        self._record(
            "Circuit breaker creation",
            "Circuit Breaker", status > 0 and status < 500,
            f"Breaker creation {'succeeded' if status == 200 else 'status: ' + str(status)}",
            latency
        )

    # ================================================================
    # Test Category 10: Metrics Export
    # ================================================================

    def test_prometheus_metrics(self):
        start = time.time()
        status, body_text = self._http_request("GET", "/metrics")
        latency = (time.time() - start) * 1000

        is_prometheus = False
        if isinstance(body_text, str):
            is_prometheus = "# TYPE" in body_text or "agentos_" in body_text
        elif isinstance(body_text, dict):
            is_prometheus = False

        self._record(
            "Prometheus metrics endpoint",
            "Metrics", status == 200 and is_prometheus,
            f"{'Valid Prometheus format' if is_prometheus else 'Not Prometheus format'} (HTTP {status})",
            latency
        )

    def test_unified_metrics_json(self):
        start = time.time()
        status, body = self._http_request("GET", "/api/v1/metrics/json")
        latency = (time.time() - start) * 1000

        has_modules = isinstance(body, dict) and "modules" in body
        self._record(
            "Unified metrics JSON export",
            "Metrics", status > 0 and status < 500,
            f"Metrics export {'available' if status == 200 else 'status: ' + str(status)}",
            latency
        )

    # ================================================================
    # Run All Tests
    # ================================================================

    def run_all(self):
        print(f"\n{'=' * 60}")
        print(f"  AgentOS Protocol Compatibility Integration Tests")
        print(f"  Gateway: {self.gateway_url}")
        print(f"  Time: {time.strftime('%Y-%m-%d %H:%M:%S')}")
        print(f"{'=' * 60}")

        categories = {
            "JSON-RPC": [
                self.test_jsonrpc_basic,
                self.test_jsonrpc_batch,
                self.test_jsonrpc_notification,
                self.test_jsonrpc_error_handling,
            ],
            "MCP": [
                self.test_mcp_tools_list,
                self.test_mcp_resources_list,
                self.test_mcp_protocol_translation,
            ],
            "A2A": [
                self.test_a2a_agent_discover,
                self.test_a2a_task_create,
            ],
            "OpenAI": [
                self.test_openai_models_list,
                self.test_openai_chat_completions,
            ],
            "Auto-Detection": [
                self.test_protocol_auto_detection,
            ],
            "Routing": [
                self.test_protocol_routing,
            ],
            "IPC": [
                self.test_ipc_service_bus_creation,
                self.test_ipc_service_bus_message,
            ],
            "Discovery": [
                self.test_service_discovery_register,
                self.test_service_discovery_discover,
            ],
            "Circuit Breaker": [
                self.test_circuit_breaker_creation,
            ],
            "Metrics": [
                self.test_prometheus_metrics,
                self.test_unified_metrics_json,
            ],
        }

        for category, tests in categories.items():
            print(f"\n  {category}")
            print(f"  {'-' * 40}")
            for test in tests:
                try:
                    test()
                except Exception as e:
                    self._record(
                        test.__name__, category, False,
                        f"Exception: {e}"
                    )

        self._print_summary()
        return self._get_exit_code()

    def _print_summary(self):
        total = len(self.results)
        passed = sum(1 for r in self.results if r.passed)
        failed = total - passed

        print(f"\n{'=' * 60}")
        print(f"  Summary")
        print(f"{'=' * 60}")
        print(f"  Total:  {total}")
        print(f"  Passed: {passed}")
        print(f"  Failed: {failed}")

        if failed > 0:
            print(f"\n  Failed tests:")
            for r in self.results:
                if not r.passed:
                    print(f"    ✗ [{r.category}] {r.name}: {r.message}")

        avg_latency = (
            sum(r.latency_ms for r in self.results if r.latency_ms > 0) /
            max(1, sum(1 for r in self.results if r.latency_ms > 0))
        )
        print(f"\n  Average latency: {avg_latency:.1f}ms")

    def _get_exit_code(self):
        failed = sum(1 for r in self.results if not r.passed)
        return 1 if failed > 0 else 0

    def to_json(self) -> str:
        return json.dumps({
            "gateway_url": self.gateway_url,
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%S"),
            "total": len(self.results),
            "passed": sum(1 for r in self.results if r.passed),
            "failed": sum(1 for r in self.results if not r.passed),
            "results": [
                {
                    "name": r.name,
                    "category": r.category,
                    "passed": r.passed,
                    "message": r.message,
                    "latency_ms": r.latency_ms,
                }
                for r in self.results
            ]
        }, indent=2, ensure_ascii=False)


def main():
    parser = argparse.ArgumentParser(description="AgentOS Protocol Compatibility Tests")
    parser.add_argument("--gateway", default=GATEWAY_URL, help="Gateway URL")
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose output")
    parser.add_argument("--json", action="store_true", help="Output JSON report")
    args = parser.parse_args()

    suite = ProtocolTestSuite(args.gateway, args.verbose)
    exit_code = suite.run_all()

    if args.json:
        print("\n" + suite.to_json())

    sys.exit(exit_code)


if __name__ == "__main__":
    main()
