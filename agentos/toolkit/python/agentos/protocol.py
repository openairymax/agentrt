# SPDX-FileCopyrightText: 2026 SPHARX Ltd.
# SPDX-License-Identifier: Apache-2.0
"""
AgentOS Python SDK — Protocol Client Module

提供多协议客户端支持，允许通过统一接口与不同协议后端交互：
- JSON-RPC 2.0 (默认)
- MCP (Model Context Protocol) v1.0
- A2A (Agent-to-Agent) v0.3
- OpenAI API 兼容

@since 3.0.0
"""

from __future__ import annotations

import json
import logging
import time
import asyncio
from dataclasses import dataclass, field
from enum import IntEnum
from typing import Any, Awaitable, Callable, Dict, List, Optional, Union

logger = logging.getLogger(__name__)


class ProtocolType(IntEnum):
    """Supported protocol types for AgentOS communication."""
    JSONRPC = 0
    MCP = 1
    A2A = 2
    OPENAI = 3
    AUTO_DETECT = 99


PROTOCOL_NAMES = {
    ProtocolType.JSONRPC: "jsonrpc",
    ProtocolType.MCP: "mcp",
    ProtocolType.A2A: "a2a",
    ProtocolType.OPENAI: "openai",
    ProtocolType.AUTO_DETECT: "auto",
}


@dataclass
class ProtocolConfig:
    """Configuration for a protocol client."""
    protocol_type: ProtocolType = ProtocolType.JSONRPC
    endpoint: str = "http://localhost:18789"
    api_key: Optional[str] = None
    timeout: int = 30
    retry_count: int = 3
    retry_delay: float = 1.0
    enable_streaming: bool = False
    headers: Dict[str, str] = field(default_factory=dict)

    @classmethod
    def from_env(cls) -> "ProtocolConfig":
        """Create configuration from environment variables."""
        import os
        return cls(
            endpoint=os.getenv("AGENTOS_ENDPOINT", cls.endpoint),
            api_key=os.getenv("AGENTOS_API_KEY", cls.api_key),
            timeout=int(os.getenv("AGENTOS_TIMEOUT", str(cls.timeout))),
        )


@dataclass
class ProtocolDetectionResult:
    """Result of automatic protocol detection."""
    detected_type: ProtocolType
    type_name: str
    confidence: float
    is_streaming: bool
    has_binary_payload: bool


@dataclass
class ConnectionTestResult:
    """Result of a connection test to a protocol endpoint."""
    protocol: str
    status: str
    status_code: Optional[int] = None
    latency_ms: Optional[float] = None
    error: Optional[str] = None


class ProtocolClient:
    """
    Unified multi-protocol client for AgentOS.

    Provides a single interface for communicating via JSON-RPC, MCP, A2A,
    or OpenAI-compatible endpoints. Automatically handles message format
    transformation between protocols.

    Usage::

        client = ProtocolClient(ProtocolConfig(protocol_type=ProtocolType.AUTO_DETECT))
        result = await client.send_request("skill.execute", {"name": "echo"})
    """

    def __init__(self, config: Optional[ProtocolConfig] = None):
        self._config = config or ProtocolConfig()
        self._session = None
        self._stats = {"requests_sent": 0, "transformations": 0}

    @property
    def config(self) -> ProtocolConfig:
        return self._config

    async def detect_protocol(self) -> ProtocolDetectionResult:
        """
        Auto-detect the appropriate protocol by querying the gateway.

        Returns:
            ProtocolDetectionResult with detected type and confidence score.
        """
        import aiohttp

        try:
            async with aiohttp.ClientSession() as session:
                url = f"{self._config.endpoint}/api/v1/protocols"
                headers = self._build_headers()
                async with session.get(url, headers=headers, timeout=aiohttp.ClientTimeout(total=10)) as resp:
                    content_type = resp.content_type or ""
                    body = await resp.text()

                    confidence = 50.0
                    detected = ProtocolType.JSONRPC
                    is_streaming = "event-stream" in content_type
                    has_binary = False

                    if "json" in content_type:
                        confidence += 20.0
                        try:
                            data = json.loads(body) if body else {}
                            body_str = json.dumps(data) if isinstance(data, dict) else body or ""

                            if '"tools/call"' in body_str or '"method":"tools/list"' in body_str:
                                detected = ProtocolType.MCP
                                confidence += 30.0
                            elif '"task/delegate"' in body_str or '"agent/discover"' in body_str:
                                detected = ProtocolType.A2A
                                confidence += 30.0
                            elif ('"model"' in body_str and '"choices"' in body_str):
                                detected = ProtocolType.OPENAI
                                confidence += 25.0
                            elif '"jsonrpc"' in body_str:
                                detected = ProtocolType.JSONRPC
                                confidence += 40.0
                        except json.JSONDecodeError:
                            pass

                    return ProtocolDetectionResult(
                        detected_type=detected,
                        type_name=PROTOCOL_NAMES.get(detected, "unknown"),
                        confidence=min(confidence, 100.0),
                        is_streaming=is_streaming,
                        has_binary_payload=has_binary,
                    )
        except Exception as e:
            logger.warning(f"Protocol detection failed: {e}")
            return ProtocolDetectionResult(
                detected_type=ProtocolType.JSONRPC,
                type_name="jsonrpc",
                confidence=50.0,
                is_streaming=False,
                has_binary_payload=False,
            )

    async def send_request(self, method: str,
                           params: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        """
        Send a unified request through the configured protocol.

        Args:
            method: The RPC method name (e.g., "skill.execute")
            params: Method parameters as a dictionary

        Returns:
            Parsed JSON response as a dictionary

        Raises:
            SessionError: On network/protocol errors
            AgentOSError: On server-side errors
        """
        import aiohttp
        from .session import SessionError

        params = params or {}
        self._stats["requests_sent"] += 1

        payload = self._build_request_payload(method, params)
        url_path = self._get_url_path()

        url = f"{self._config.endpoint}{url_path}"
        headers = self._build_headers()

        last_error = None
        delay = self._config.retry_delay

        for attempt in range(self._config.retry_count + 1):
            try:
                async with aiohttp.ClientSession() as session:
                    async with session.post(
                        url,
                        data=json.dumps(payload),
                        headers=headers,
                        timeout=aiohttp.ClientTimeout(total=self._config.timeout),
                    ) as resp:

                        body = await resp.text()
                        if resp.status >= 400:
                            raise SessionError(
                                f"Protocol HTTP {resp.status}: {body[:200]}"
                            )

                        response_data = json.loads(body) if body else {}

                        if self._config.protocol_type not in (ProtocolType.JSONRPC, ProtocolType.AUTO_DETECT):
                            self._stats["transformations"] += 1

                        return response_data

            except (aiohttp.ClientError, asyncio.TimeoutError) as e:
                last_error = e
                if attempt < self._config.retry_count:
                    await asyncio.sleep(delay)
                    delay *= 2

        raise SessionError(f"All retries exhausted: {last_error}")

    async def stream_request(self, method: str,
                             params: Optional[Dict[str, Any]] = None,
                             on_chunk: Callable[[bytes], Awaitable[None]] = None) -> None:
        """
        Send a streaming request and deliver chunks via callback.

        Args:
            method: The RPC method name
            params: Method parameters
            on_chunk: Async callback receiving each chunk's raw bytes
        """
        import aiohttp

        if not self._config.enable_streaming:
            result = await self.send_request(method, params)
            if on_chunk:
                await on_chunk(json.dumps(result).encode())
            return

        params = params or {}
        payload = self._build_request_payload(method, params)

        url = f"{self._config.endpoint}/rpc"
        headers = self._build_headers()
        headers["Accept"] = "text/event-stream"

        async with aiohttp.ClientSession() as session:
            async with session.post(
                url, data=json.dumps(payload), headers=headers,
                timeout=aiohttp.ClientTimeout(total=self._config.timeout * 3),
            ) as resp:
                async for chunk in resp.content:
                    if chunk and on_chunk:
                        await on_chunk(chunk)

    async def list_protocols(self) -> List[str]:
        """Query available protocols from the gateway."""
        import aiohttp

        async with aiohttp.ClientSession() as session:
            url = f"{self._config.endpoint}/api/v1/protocols"
            async with session.get(url, headers=self._build_headers(),
                                   timeout=aiohttp.ClientTimeout(total=5)) as resp:
                data = await resp.json()
                return data.get("protocols", [])

    async def test_connection(self, protocol_name: str) -> ConnectionTestResult:
        """
        Test connectivity to a specific protocol endpoint.

        Returns:
            ConnectionTestResult with latency and status information.
        """
        import aiohttp

        url = f"{self._config.endpoint}/api/v1/protocols/{protocol_name}/test"
        start = time.monotonic()

        try:
            async with aiohttp.ClientSession() as session:
                async with session.get(url, headers=self._build_headers(),
                                       timeout=aiohttp.ClientTimeout(total=10)) as resp:
                    latency = (time.monotonic() - start) * 1000
                    body = await resp.text()
                    return ConnectionTestResult(
                        protocol=protocol_name,
                        status="ok" if resp.status < 400 else "error",
                        status_code=resp.status,
                        latency_ms=round(latency, 2),
                    )
        except Exception as e:
            latency = (time.monotonic() - start) * 1000
            return ConnectionTestResult(
                protocol=protocol_name,
                status="error",
                latency_ms=round(latency, 2),
                error=str(e),
            )

    async def get_capabilities(self, protocol_name: str) -> Dict[str, Any]:
        """Get capabilities of a specific protocol adapter."""
        import aiohttp

        url = f"{self._config.endpoint}/api/v1/protocols/{protocol_name}/capabilities"
        async with aiohttp.ClientSession() as session:
            async with session.get(url, headers=self._build_headers(),
                                   timeout=aiohttp.ClientTimeout(total=5)) as resp:
                return await resp.json()

    def get_stats(self) -> Dict[str, int]:
        """Return internal statistics about this client instance."""
        return dict(self._stats)

    # ==========================================================================
    # Internal helpers
    # ==========================================================================

    def _build_headers(self) -> Dict[str, str]:
        headers = {
            "Content-Type": "application/json",
            "Accept": "application/json",
            **self._config.headers,
        }
        if self._config.api_key:
            headers["Authorization"] = f"Bearer {self._config.api_key}"
        return headers

    def _get_url_path(self) -> str:
        paths = {
            ProtocolType.OPENAI: "/v1/chat/completions",
            ProtocolType.MCP: "/api/v1/invoke",
            ProtocolType.A2A: "/api/v1/invoke",
            ProtocolType.JSONRPC: "/rpc",
            ProtocolType.AUTO_DETECT: "/rpc",
        }
        return paths.get(self._config.protocol_type, "/rpc")

    def _build_request_payload(self, method: str,
                               params: Dict[str, Any]) -> Dict[str, Any]:

        if self._config.protocol_type == ProtocolType.OPENAI:
            local_params = dict(params)
            messages = local_params.pop("messages", [])
            return {
                "model": local_params.pop("model", "gpt-4o"),
                "messages": messages,
                "temperature": local_params.pop("temperature", 0.7),
                "max_tokens": local_params.pop("max_tokens", 2048),
                "stream": self._config.enable_streaming,
            }

        elif self._config.protocol_type == ProtocolType.MCP:
            mcp_method = method.split(".")[-1] if "." in method else method
            return {
                "protocol": "mcp",
                "version": "1.0",
                "method": mcp_method,
                "params": params,
            }

        elif self._config.protocol_type == ProtocolType.A2A:
            mapping = {"agent.list": "agent/discover", "task.create": "task/delegate"}
            a2a_method = mapping.get(method, method)
            return {
                "protocol": "a2a",
                "version": "0.3",
                "method": a2a_method,
                "params": params,
            }

        return {
            "jsonrpc": "2.0",
            "id": f"req_{int(time.time() * 1000)}_{id(params)}",
            "method": method,
            "params": params,
        }
