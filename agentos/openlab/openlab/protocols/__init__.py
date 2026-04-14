# SPDX-FileCopyrightText: 2026 SPHARX Ltd.
# SPDX-License-Identifier: Apache-2.0
"""
AgentOS openlab.protocols — Protocol Integration Bindings

将 AgentOS 协议系统集成到 OpenLab 应用框架中，提供：
- ProtocolSessionManager — 协议会话管理器
- ProtocolAgentAdapter — 协议感知的智能体适配器
- ProtocolToolBridge — 协议工具桥接（MCP工具→AgentOS Skill）
- UnifiedProtocolRunner — 统一协议运行器

@since 2.0.0
"""

from __future__ import annotations

import asyncio
import json
import logging
import time
import uuid
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from enum import IntEnum
from typing import Any, Callable, Dict, List, Optional, Type, Union

logger = logging.getLogger(__name__)

# Re-export from agentos.protocol for convenience
try:
    from agentos.protocol import (
        ProtocolClient,
        ProtocolConfig,
        ProtocolType,
        ProtocolDetectionResult,
        ConnectionTestResult,
    )
    PROTOCOL_SDK_AVAILABLE = True
except ImportError:
    ProtocolClient = None
    ProtocolConfig = None
    ProtocolType = None
    PROTOCOL_SDK_AVAILABLE = False
    logger.warning("agentos.protocol SDK not available, using standalone mode")


# ============================================================================
# Enums & Data Classes
# ============================================================================

class SessionState(IntEnum):
    """Protocol session lifecycle states."""
    ACTIVE = 0
    PAUSED = 1
    CLOSING = 2
    CLOSED = 3
    ERROR = 4


class ToolBindingMode(IntEnum):
    """How MCP/A2A tools map to AgentOS skills."""
    DIRECT_PROXY = 0       # Pass-through to original protocol
    SANDBOXED_WRAP = 1     # Wrap in AgentOS sandbox
    CONVERTED_NATIVE = 2   # Convert to native AgentOS skill format


@dataclass
class ProtocolSessionConfig:
    """Configuration for a protocol-aware session."""
    session_id: str = field(default_factory=lambda: f"proto_sess_{uuid.uuid4().hex[:12]}")
    primary_protocol: ProtocolType = (ProtocolType.JSONRPC if ProtocolType else None)
    fallback_protocols: List[ProtocolType] = field(default_factory=list)
    auto_detect: bool = True
    tool_binding_mode: ToolBindingMode = ToolBindingMode.SANDBOXED_WRAP
    enable_streaming: bool = False
    max_concurrent_requests: int = 10
    request_timeout: int = 60
    trace_id: str = field(default_factory=lambda: uuid.uuid4().hex)


@dataclass
class BoundToolInfo:
    """Information about a tool bound from an external protocol."""
    tool_id: str
    name: str
    description: str
    source_protocol: str
    source_method: str
    input_schema: Dict[str, Any]
    output_schema: Dict[str, Any]
    binding_mode: ToolBindingMode
    is_streaming: bool
    permission_required: bool = False
    created_at: float = field(default_factory=time.time)


@dataclass
class ProtocolRequestContext:
    """Context carried through the protocol processing pipeline."""
    session_id: str
    trace_id: str
    source_protocol: str
    target_protocol: str
    method: str
    params: Dict[str, Any]
    metadata: Dict[str, Any] = field(default_factory=dict)
    timestamp: float = field(default_factory=time.time)
    parent_span_id: Optional[str] = None


@dataclass
class ProtocolResponse:
    """Standardized response from any protocol operation."""
    success: bool
    data: Optional[Dict[str, Any]] = None
    error_code: Optional[int] = None
    error_message: Optional[str] = None
    duration_ms: float = 0.0
    protocol: str = ""
    transformed: bool = False
    metrics: Dict[str, Any] = field(default_factory=dict)


# ============================================================================
// Abstract Base: Protocol Handler
// ============================================================================

class ProtocolHandler(ABC):
    """Base class for protocol-specific message handlers."""

    @abstractmethod
    async def handle_request(self, ctx: ProtocolRequestContext) -> ProtocolResponse:
        """Process a protocol request and return a standardized response."""
        ...

    @abstractmethod
    def supported_methods(self) -> List[str]:
        """Return list of method names this handler can process."""
        ...

    @property
    @abstractmethod
    def protocol_name(self) -> str:
        """Human-readable protocol name."""
        ...


# ============================================================================
// Protocol Session Manager
// ============================================================================

class ProtocolSessionManager:
    """
    Manages multi-protocol sessions for OpenLab agents.

    Each session maintains connections to one or more protocol backends,
    handles automatic failover between protocols, and provides unified
    tool discovery and execution across all registered protocols.

    Usage::

        mgr = ProtocolSessionManager(ProtocolSessionConfig())
        await mgr.initialize()
        tools = await mgr.discover_tools()
        result = await mgr.execute_tool("file_reader", {"path": "/tmp/test.txt"})
        await mgr.close()
    """

    def __init__(self, config: Optional[ProtocolSessionConfig] = None):
        self._config = config or ProtocolSessionConfig()
        self._state = SessionState.ACTIVE
        self._clients: Dict[ProtocolType, Any] = {}
        self._bound_tools: Dict[str, BoundToolInfo] = {}
        self._handlers: List[ProtocolHandler] = []
        self._request_queue: asyncio.Queue = asyncio.Queue(maxsize=100)
        self._stats = {
            "requests_processed": 0,
            "tools_bound": 0,
            "protocol_switches": 0,
            "errors": 0,
            "avg_latency_ms": 0.0,
        }

    @property
    def config(self) -> ProtocolSessionConfig:
        return self._config

    @property
    def state(self) -> SessionState:
        return self._state

    @property
    def stats(self) -> Dict[str, Any]:
        return dict(self._stats)

    async def initialize(self) -> None:
        """Initialize the session manager and connect to protocol backends."""
        if PROTOCOL_SDK_AVAILABLE and ProtocolClient:
            for proto_type in [self._config.primary_protocol] + self._config.fallback_protocols:
                try:
                    client = ProtocolClient(ProtocolConfig(
                        protocol_type=proto_type,
                        endpoint="http://localhost:18789",
                    ))
                    if hasattr(client, 'detect_protocol'):
                        detection = await client.detect_protocol()
                        logger.info(f"Protocol {detection.type_name} detected "
                                   f"(confidence={detection.confidence:.1f}%)")
                    self._clients[proto_type] = client
                    logger.info(f"Initialized {proto_type} client for session {self._config.session_id}")
                except Exception as e:
                    logger.warning(f"Failed to initialize {proto_type}: {e}")
        else:
            logger.info("Running in standalone mode (no SDK protocol client)")

        self._state = SessionState.ACTIVE

    async def discover_tools(self) -> List[BoundToolInfo]:
        """
        Discover available tools from all connected protocols.

        Tools from different protocols are unified into BoundToolInfo objects
        with consistent naming and schema.
        """
        tools: List[BoundToolInfo] = []
        discovered_count = 0

        for proto_type, client in self._clients.items():
            try:
                if hasattr(client, 'list_protocols'):
                    protocols = await client.list_protocols()
                    logger.debug(f"Protocols via {proto_type}: {protocols}")

                if hasattr(client, 'get_capabilities') and isinstance(proto_type, (int, ProtocolType)):
                    proto_name = str(proto_type).lower() if isinstance(proto_type, int) else proto_type.value
                    caps = await client.get_capabilities(proto_name)
                    methods = caps.get('supportedMethods', caps.get('supported_methods', []))

                    for method_name in methods:
                        tool_info = BoundToolInfo(
                            tool_id=f"{proto_name}_{method_name}",
                            name=method_name,
                            description=f"Tool from {proto_name}",
                            source_protocol=proto_name,
                            source_method=method_name,
                            input_schema={},
                            output_schema={},
                            binding_mode=self._config.tool_binding_mode,
                            is_streaming=caps.get('streamingSupported', False),
                        )
                        tools.append(tool_info)
                        self._bound_tools[tool_info.tool_id] = tool_info
                        discovered_count += 1

            except Exception as e:
                logger.warning(f"Tool discovery failed for {proto_type}: {e}")
                self._stats["errors"] += 1

        self._stats["tools_bound"] = len(tools)
        logger.info(f"Discovered {discovered_count} tools across {len(self._clients)} protocols")
        return tools

    async def execute_tool(self, tool_id: str, params: Dict[str, Any]) -> ProtocolResponse:
        """
        Execute a bound tool through the appropriate protocol.

        Handles protocol selection, request transformation, response normalization,
        and error recovery.
        """
        start_time = time.monotonic()

        if self._state != SessionState.ACTIVE:
            return ProtocolResponse(
                success=False,
                error_code=4001,
                error_message=f"Session not active (state={self._state.name})",
            )

        tool = self._bound_tools.get(tool_id)
        if not tool:
            return ProtocolResponse(
                success=False,
                error_code=4004,
                error_message=f"Tool '{tool_id}' not found",
            )

        ctx = ProtocolRequestContext(
            session_id=self._config.session_id,
            trace_id=self._config.trace_id,
            source_protocol="openlab",
            target_protocol=tool.source_protocol,
            method=tool.source_method,
            params=params,
        )

        for handler in self._handlers:
            if tool.method in handler.supported_methods():
                try:
                    result = await handler.handle_request(ctx)
                    result.duration_ms = (time.monotonic() - start_time) * 1000
                    result.protocol = tool.source_protocol
                    self._update_latency_stats(result.duration_ms)
                    self._stats["requests_processed"] += 1
                    return result
                except Exception as e:
                    logger.error(f"Handler error for {tool.method}: {e}")

        client = self._clients.get(self._get_protocol_enum(tool.source_protocol))
        if client and hasattr(client, 'send_request'):
            try:
                raw_result = await client.send_request(tool.method, params)
                duration_ms = (time.monotonic() - start_time) * 1000

                response = ProtocolResponse(
                    success=True,
                    data=raw_result if isinstance(raw_result, dict) else {"result": raw_result},
                    duration_ms=duration_ms,
                    protocol=tool.source_protocol,
                    transformed=True,
                )
                self._update_latency_stats(duration_ms)
                self._stats["requests_processed"] += 1
                return response
            except Exception as e:
                logger.error(f"Direct protocol call failed: {e}")
                self._stats["errors"] += 1

        duration_ms = (time.monotonic() - start_time) * 1000
        self._stats["errors"] += 1
        return ProtocolResponse(
            success=False,
            error_code=5001,
            error_message=f"No handler found for tool '{tool_id}'",
            duration_ms=duration_ms,
        )

    async def close(self) -> None:
        """Gracefully close the session and release resources."""
        self._state = SessionState.CLOSING

        for proto_type, client in self._clients.items():
            try:
                if hasattr(client, 'close'):
                    await client.close()
                elif hasattr(client, '__aexit__'):
                    await client.__aexit__(None, None, None)
                logger.debug(f"Closed {proto_type} client")
            except Exception as e:
                logger.warning(f"Error closing {proto_type}: {e}")

        self._clients.clear()
        self._bound_tools.clear()
        self._state = SessionState.CLOSED
        logger.info(f"Session {self._config.session_id} closed")

    def register_handler(self, handler: ProtocolHandler) -> None:
        """Register a custom protocol handler."""
        self._handlers.append(handler)

    def get_tool(self, tool_id: str) -> Optional[BoundToolInfo]:
        """Get information about a bound tool by ID."""
        return self._bound_tools.get(tool_id)

    def list_tools(self) -> List[BoundToolInfo]:
        """List all currently bound tools."""
        return list(self._bound_tools.values())

    # ==================================================================
    # Internal helpers
    # ==================================================================

    def _get_protocol_enum(self, name: str) -> ProtocolType:
        mapping = {
            "jsonrpc": ProtocolType.JSONRPC if ProtocolType else 0,
            "mcp": ProtocolType.MCP if ProtocolType else 1,
            "a2a": ProtocolType.A2A if ProtocolType else 2,
            "openai": ProtocolType.OPENAI if ProtocolType else 3,
        }
        return mapping.get(name.lower(), ProtocolType.AUTO_DETECT if ProtocolType else 0)

    def _update_latency_stats(self, latency_ms: float) -> None:
        current = self._stats.get("avg_latency_ms", 0.0)
        count = self._stats.get("requests_processed", 1)
        self._stats["avg_latency_ms"] = (current * (count - 1) + latency_ms) / count


# ============================================================================
// Built-in Handlers
// ============================================================================

class JSONRPCHandler(ProtocolHandler):
    """Handler for JSON-RPC 2.0 protocol messages."""

    def __init__(self, endpoint: str = "http://localhost:18789"):
        self._endpoint = endpoint

    @property
    def protocol_name(self) -> str:
        return "jsonrpc"

    def supported_methods(self) -> List[str]:
        return [
            "agent.list", "agent.create", "agent.destroy",
            "skill.list", "skill.execute", "skill.install",
            "task.submit", "task.query", "task.cancel",
            "memory.write", "memory.search", "memory.evolve",
            "session.create", "session.close",
        ]

    async def handle_request(self, ctx: ProtocolRequestContext) -> ProtocolResponse:
        import aiohttp

        url = f"{self._endpoint}/rpc"
        payload = {
            "jsonrpc": "2.0",
            "id": f"{ctx.trace_id}_{int(time.time())}",
            "method": ctx.method,
            "params": ctx.params,
        }

        try:
            async with aiohttp.ClientSession() as session:
                async with session.post(url, json=payload,
                                         timeout=aiohttp.ClientTimeout(total=30)) as resp:
                    body = await resp.json()
                    return ProtocolResponse(
                        success=True,
                        data=body,
                        protocol="jsonrpc",
                    )
        except Exception as e:
            return ProtocolResponse(
                success=False,
                error_code=5003,
                error_message=str(e),
                protocol="jsonrpc",
            )


class MCPHandler(ProtocolHandler):
    """Handler for Model Context Protocol (MCP) messages."""

    def __init__(self, endpoint: str = "http://localhost:18789"):
        self._endpoint = endpoint

    @property
    def protocol_name(self) -> str:
        return "mcp"

    def supported_methods(self) -> List[str]:
        return ["tools/list", "tools/call", "resources/list", "resources/read"]

    async def handle_request(self, ctx: ProtocolRequestContext) -> ProtocolResponse:
        import aiohttp

        url = f"{self._endpoint}/api/v1/invoke"
        payload = {
            "protocol": "mcp",
            "version": "1.0",
            "method": ctx.method,
            "params": ctx.params,
        }

        try:
            async with aiohttp.ClientSession() as session:
                async with session.post(url, json=payload,
                                         timeout=aiohttp.ClientTimeout(total=30)) as resp:
                    body = await resp.json()
                    return ProtocolResponse(
                        success=True,
                        data=body,
                        protocol="mcp",
                        transformed=True,
                    )
        except Exception as e:
            return ProtocolResponse(
                success=False,
                error_code=5003,
                error_message=str(e),
                protocol="mcp",
            )
