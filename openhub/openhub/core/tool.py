"""
Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

OpenHub Tool Integration Module
==============================

This module provides tool abstraction and integration for OpenHub:
- Tool definition and capability declaration
- Tool registry and discovery
- Tool execution with sandboxing
- Result standardization and error handling

Usage:
    from openhub.core.tool import Tool, ToolRegistry, ToolExecutor

Author: AgentOS Architecture Committee
Version: 1.0.0.6
"""

from __future__ import annotations

import asyncio
import json
import logging
import time
import uuid
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from enum import Enum, auto
from typing import (
    Any,
    Dict,
    List,
    Optional,
    Set,
    TypeVar,
    Generic,
    Awaitable,
    Callable,
)

logger = logging.getLogger(__name__)


class ToolCategory(Enum):
    """Tool category classification."""
    FILE_SYSTEM = auto()
    NETWORK = auto()
    DATABASE = auto()
    CODE_EXECUTION = auto()
    BROWSER = auto()
    API_CLIENT = auto()
    DATA_PROCESSING = auto()
    AI_MODEL = auto()
    VERSION_CONTROL = auto()
    CONTAINER = auto()


class ToolCapability(Enum):
    """Fine-grained tool capabilities."""
    FILE_READ = "file_read"
    FILE_WRITE = "file_write"
    FILE_DELETE = "file_delete"
    FILE_LIST = "file_list"
    HTTP_GET = "http_get"
    HTTP_POST = "http_post"
    SQL_QUERY = "sql_query"
    SQL_EXECUTE = "sql_execute"
    CODE_RUN = "code_run"
    CODE_COMPILE = "code_compile"
    BROWSER_NAVIGATE = "browser_navigate"
    BROWSER_CLICK = "browser_click"
    BROWSER_TYPE = "browser_type"
    API_CALL = "api_call"
    DATA_TRANSFORM = "data_transform"
    MODEL_INFER = "model_infer"
    GIT_CLONE = "git_clone"
    GIT_PUSH = "git_push"
    GIT_PULL = "git_pull"
    DOCKER_RUN = "docker_run"


@dataclass
class ToolMetadata:
    """
    Metadata structure for tool registration.

    Attributes:
        tool_id: Unique tool identifier.
        name: Human-readable tool name.
        version: Tool version string.
        category: Tool category.
        capabilities: Set of fine-grained capabilities.
        description: Tool description.
        input_schema: JSON schema for tool input.
        output_schema: JSON schema for tool output.
        resource_usage: Estimated resource consumption.
        tags: Arbitrary metadata tags.
    """
    tool_id: str
    name: str
    version: str
    category: ToolCategory
    capabilities: Set[ToolCapability]
    description: str
    input_schema: Dict[str, Any]
    output_schema: Dict[str, Any]
    resource_usage: Dict[str, Any] = field(default_factory=dict)
    tags: Dict[str, str] = field(default_factory=dict)


@dataclass
class ToolInput:
    """
    Standardized tool input structure.

    Attributes:
        tool_id: Target tool identifier.
        parameters: Tool-specific parameters.
        context: Execution context (timeout, sandbox, etc.).
        trace_id: Distributed tracing identifier.
    """
    tool_id: str
    parameters: Dict[str, Any]
    context: Dict[str, Any] = field(default_factory=dict)
    trace_id: Optional[str] = None


@dataclass
class ToolOutput:
    """
    Standardized tool output structure.

    Attributes:
        tool_id: Reference to tool that produced output.
        success: Whether execution succeeded.
        result: Tool execution result.
        error: Error message if failed.
        error_code: Standardized error code.
        execution_time_ms: Execution duration.
        metadata: Additional execution metadata.
    """
    tool_id: str
    success: bool
    result: Any = None
    error: Optional[str] = None
    error_code: Optional[str] = None
    execution_time_ms: float = 0.0
    metadata: Dict[str, Any] = field(default_factory=dict)


class Tool(ABC):
    """
    Abstract base class for all OpenHub tools.

    Tools are the atomic execution units that agents can invoke
    to perform specific operations. Each tool:

    - Declares its capabilities via ToolCapability flags
    - Defines input/output schemas for type safety
    - Executes within a sandboxed context
    - Returns standardized ToolOutput objects

    Subclasses must implement:
        - _do_validate_input(): Validate input parameters
        - _do_execute(): Core tool logic
        - _do_cleanup(): Resource cleanup

    Example:
        class FileReadTool(Tool):
            CAPABILITIES = {ToolCapability.FILE_READ}
            INPUT_SCHEMA = {
                "type": "object",
                "properties": {"path": {"type": "string"}},
                "required": ["path"]
            }

            async def _do_validate_input(self, params: Dict) -> None:
                path = Path(params["path"])
                if not path.exists():
                    raise ValueError(f"File not found: {path}")

            async def _do_execute(self, params: Dict) -> Any:
                return Path(params["path"]).read_text()
    """

    CAPABILITIES: Set[ToolCapability] = set()
    INPUT_SCHEMA: Dict[str, Any] = {"type": "object", "properties": {}}
    OUTPUT_SCHEMA: Dict[str, Any] = {"type": "object", "properties": {}}

    def __init__(
        self,
        tool_id: Optional[str] = None,
        name: Optional[str] = None,
        config: Optional[Dict[str, Any]] = None,
    ) -> None:
        """
        Initialize a new Tool instance.

        Args:
            tool_id: Unique identifier (auto-generated if not provided).
            name: Human-readable name.
            config: Tool-specific configuration.
        """
        self.tool_id = tool_id or str(uuid.uuid4())
        self.name = name or self.__class__.__name__
        self.config = config or {}

        self._metadata = ToolMetadata(
            tool_id=self.tool_id,
            name=self.name,
            version=self.config.get("version", "1.0.0"),
            category=self._get_category(),
            capabilities=self.CAPABILITIES,
            description=self.__doc__ or "",
            input_schema=self.INPUT_SCHEMA,
            output_schema=self.OUTPUT_SCHEMA,
        )

        self._initialized = False
        self._stats = {
            "executions": 0,
            "successes": 0,
            "failures": 0,
            "total_execution_time_ms": 0,
        }

        logger.info(
            "Tool instance created",
            extra={
                "tool_id": self.tool_id,
                "name": self.name,
                "capabilities": [c.value for c in self.CAPABILITIES],
            },
        )

    @property
    def metadata(self) -> ToolMetadata:
        """Get tool metadata."""
        return self._metadata

    def validate_input(self, parameters: Dict[str, Any]) -> None:
        """
        Validate input parameters against schema.

        Args:
            parameters: Input parameters to validate.

        Raises:
            ValueError: If validation fails.
            jsonschema.ValidationError: If schema validation fails.
        """
        import jsonschema

        try:
            jsonschema.validate(instance=parameters, schema=self.INPUT_SCHEMA)
            self._do_validate_input(parameters)
        except jsonschema.ValidationError as e:
            logger.warning(
                f"Tool {self.tool_id} input validation failed",
                extra={"error": str(e)},
            )
            raise

    async def execute(
        self, parameters: Dict[str, Any], context: Optional[Dict[str, Any]] = None
    ) -> ToolOutput:
        """
        Execute the tool with given parameters.

        Args:
            parameters: Tool input parameters.
            context: Execution context (timeout, sandbox, etc.).

        Returns:
            ToolOutput with execution result.

        Raises:
            RuntimeError: If tool not initialized.
        """
        if not self._initialized:
            await self.initialize()

        start_time = time.time()

        logger.info(
            "Tool execution started",
            extra={
                "tool_id": self.tool_id,
                "name": self.name,
                "trace_id": context.get("trace_id") if context else None,
            },
        )

        try:
            self.validate_input(parameters)

            result = await self._do_execute(parameters, context or {})

            execution_time_ms = (time.time() - start_time) * 1000

            self._stats["executions"] += 1
            self._stats["successes"] += 1
            self._stats["total_execution_time_ms"] += execution_time_ms

            logger.info(
                "Tool execution succeeded",
                extra={
                    "tool_id": self.tool_id,
                    "execution_time_ms": execution_time_ms,
                },
            )

            return ToolOutput(
                tool_id=self.tool_id,
                success=True,
                result=result,
                execution_time_ms=execution_time_ms,
            )

        except Exception as e:
            execution_time_ms = (time.time() - start_time) * 1000

            self._stats["executions"] += 1
            self._stats["failures"] += 1
            self._stats["total_execution_time_ms"] += execution_time_ms

            logger.error(
                "Tool execution failed",
                extra={
                    "tool_id": self.tool_id,
                    "error": str(e),
                    "execution_time_ms": execution_time_ms,
                },
            )

            return ToolOutput(
                tool_id=self.tool_id,
                success=False,
                error=str(e),
                error_code=self._get_error_code(e),
                execution_time_ms=execution_time_ms,
            )

        finally:
            try:
                await self._do_cleanup()
            except Exception as e:
                logger.warning(
                    f"Tool cleanup failed: {e}",
                    extra={"tool_id": self.tool_id},
                )

    async def initialize(self) -> None:
        """
        Initialize the tool (async setup).

        Override this for async initialization needs.
        """
        self._initialized = True

    @abstractmethod
    def _do_validate_input(self, parameters: Dict[str, Any]) -> None:
        """
        Subclass-specific input validation.

        Args:
            parameters: Validated input parameters.

        Raises:
            ValueError: If validation fails.
        """
        pass

    @abstractmethod
    async def _do_execute(
        self, parameters: Dict[str, Any], context: Dict[str, Any]
    ) -> Any:
        """
        Core tool execution logic.

        Args:
            parameters: Validated input parameters.
            context: Execution context.

        Returns:
            Tool execution result.

        Raises:
            Exception: Any execution error.
        """
        pass

    async def _do_cleanup(self) -> None:
        """
        Subclass-specific cleanup logic.

        Called after each execution for resource cleanup.
        """
        pass

    def _get_category(self) -> ToolCategory:
        """Get tool category (override for custom categories)."""
        return ToolCategory.FILE_SYSTEM

    def _get_error_code(self, error: Exception) -> str:
        """Map exception to standardized error code."""
        error_mapping = {
            FileNotFoundError: "TOOL_FILE_NOT_FOUND",
            PermissionError: "TOOL_PERMISSION_DENIED",
            TimeoutError: "TOOL_TIMEOUT",
            ValueError: "TOOL_INVALID_INPUT",
        }
        for exc_type, code in error_mapping.items():
            if isinstance(error, exc_type):
                return code
        return "TOOL_EXECUTION_ERROR"

    def get_stats(self) -> Dict[str, Any]:
        """Get tool execution statistics."""
        return self._stats.copy()


class ToolRegistry:
    """
    Central registry for tool discovery and management.

    Features:
    - Tool registration and unregistration
    - Capability-based discovery
    - Category-based listing
    - Version management

    Example:
        registry = ToolRegistry()

        await registry.register(my_tool.metadata)

        tools = registry.find_by_capability(ToolCapability.FILE_READ)

        tool = registry.get("tool-uuid-123")
    """

    def __init__(self) -> None:
        """Initialize an empty registry."""
        self._tools: Dict[str, ToolMetadata] = {}
        self._capability_index: Dict[ToolCapability, Set[str]] = {
            cap: set() for cap in ToolCapability
        }
        self._category_index: Dict[ToolCategory, Set[str]] = {}
        self._lock = asyncio.Lock()

        logger.info("ToolRegistry initialized")

    async def register(self, tool: Tool) -> None:
        """
        Register a tool in the catalog.

        Args:
            tool: Tool instance to register.

        Raises:
            ValueError: If tool_id already exists.
        """
        async with self._lock:
            if tool.tool_id in self._tools:
                raise ValueError(f"Tool {tool.tool_id} already registered")

            metadata = tool.metadata
            self._tools[tool.tool_id] = metadata

            for cap in metadata.capabilities:
                self._capability_index[cap].add(tool.tool_id)

            if metadata.category not in self._category_index:
                self._category_index[metadata.category] = set()
            self._category_index[metadata.category].add(tool.tool_id)

            logger.info(
                "Tool registered",
                extra={
                    "tool_id": tool.tool_id,
                    "name": metadata.name,
                    "category": metadata.category.name,
                },
            )

    async def unregister(self, tool_id: str) -> None:
        """
        Unregister a tool from the catalog.

        Args:
            tool_id: ID of tool to remove.

        Raises:
            KeyError: If tool_id not found.
        """
        async with self._lock:
            metadata = self._tools.pop(tool_id, None)
            if metadata is None:
                raise KeyError(f"Tool {tool_id} not found")

            for cap in metadata.capabilities:
                self._capability_index[cap].discard(tool_id)

            self._category_index.get(metadata.category, set()).discard(tool_id)

            logger.info(f"Tool {tool_id} unregistered")

    async def get(self, tool_id: str) -> Optional[ToolMetadata]:
        """
        Get tool metadata by ID.

        Args:
            tool_id: Tool identifier.

        Returns:
            ToolMetadata if found, None otherwise.
        """
        async with self._lock:
            return self._tools.get(tool_id)

    async def find_by_capability(
        self, capability: ToolCapability
    ) -> List[ToolMetadata]:
        """
        Find all tools with a given capability.

        Args:
            capability: Required capability.

        Returns:
            List of matching tool metadata.
        """
        async with self._lock:
            tool_ids = self._capability_index.get(capability, set())
            return [self._tools[tid] for tid in tool_ids if tid in self._tools]

    async def find_by_category(
        self, category: ToolCategory
    ) -> List[ToolMetadata]:
        """
        Find all tools in a given category.

        Args:
            category: Required category.

        Returns:
            List of matching tool metadata.
        """
        async with self._lock:
            tool_ids = self._category_index.get(category, set())
            return [self._tools[tid] for tid in tool_ids if tid in self._tools]

    async def find_by_capabilities(
        self, capabilities: Set[ToolCapability], match_all: bool = False
    ) -> List[ToolMetadata]:
        """
        Find tools matching specified capabilities.

        Args:
            capabilities: Set of required capabilities.
            match_all: If True, tool must have ALL capabilities.
                      If False, tool must have AT LEAST ONE.

        Returns:
            List of matching tool metadata.
        """
        async with self._lock:
            results = []
            for tool_id, metadata in self._tools.items():
                if match_all:
                    if capabilities.issubset(metadata.capabilities):
                        results.append(metadata)
                else:
                    if capabilities.intersection(metadata.capabilities):
                        results.append(metadata)
            return results

    async def list_all(self) -> List[ToolMetadata]:
        """List all registered tool metadata."""
        async with self._lock:
            return list(self._tools.values())

    async def get_stats(self) -> Dict[str, Any]:
        """Get registry statistics."""
        async with self._lock:
            return {
                "total_tools": len(self._tools),
                "by_capability": {
                    cap.value: len(tools)
                    for cap, tools in self._capability_index.items()
                    if tools
                },
                "by_category": {
                    cat.name: len(tools)
                    for cat, tools in self._category_index.items()
                    if tools
                },
            }


class ToolExecutor:
    """
    Tool execution engine with pooling and resource management.

    Features:
    - Tool instance pooling
    - Concurrency control
    - Execution timeout
    - Error handling and retry

    Example:
        executor = ToolExecutor(registry, max_concurrent=10)

        output = await executor.execute(
            tool_id="file-read-tool",
            parameters={"path": "/tmp/data.txt"},
            context={"timeout": 30},
        )
    """

    def __init__(
        self,
        registry: ToolRegistry,
        max_concurrent: int = 50,
        default_timeout: float = 60.0,
    ) -> None:
        """
        Initialize the tool executor.

        Args:
            registry: ToolRegistry for tool discovery.
            max_concurrent: Maximum concurrent executions.
            default_timeout: Default execution timeout.
        """
        self.registry = registry
        self.max_concurrent = max_concurrent
        self.default_timeout = default_timeout

        self._tool_instances: Dict[str, Tool] = {}
        self._tool_classes: Dict[str, type] = {}
        self._semaphore: Optional[asyncio.Semaphore] = None
        self._lock = asyncio.Lock()
        self._shutdown = False

        logger.info(
            "ToolExecutor initialized",
            extra={"max_concurrent": max_concurrent},
        )

    def register_tool_class(self, tool_class: type, tool_id: str) -> None:
        """
        Register a tool class for lazy instantiation.

        Args:
            tool_class: Tool subclass to register.
            tool_id: Unique tool identifier.
        """
        self._tool_classes[tool_id] = tool_class
        logger.info(
            "Tool class registered",
            extra={"tool_id": tool_id, "class": tool_class.__name__},
        )

    async def get_tool(self, tool_id: str) -> Optional[Tool]:
        """
        Get or create a tool instance.

        Args:
            tool_id: Tool identifier.

        Returns:
            Tool instance if found, None otherwise.
        """
        if tool_id in self._tool_instances:
            return self._tool_instances[tool_id]

        tool_class = self._tool_classes.get(tool_id)
        if tool_class is None:
            metadata = await self.registry.get(tool_id)
            if metadata is None:
                return None

        tool = tool_class(tool_id=tool_id)
        await tool.initialize()
        self._tool_instances[tool_id] = tool

        return tool

    async def execute(
        self,
        tool_id: str,
        parameters: Dict[str, Any],
        context: Optional[Dict[str, Any]] = None,
    ) -> ToolOutput:
        """
        Execute a tool by ID.

        Args:
            tool_id: Target tool identifier.
            parameters: Tool input parameters.
            context: Execution context.

        Returns:
            ToolOutput with execution result.

        Raises:
            KeyError: If tool not found.
            RuntimeError: If executor is shutting down.
        """
        if self._shutdown:
            raise RuntimeError("ToolExecutor is shutting down")

        if self._semaphore is None:
            self._semaphore = asyncio.Semaphore(self.max_concurrent)

        tool = await self.get_tool(tool_id)
        if tool is None:
            raise KeyError(f"Tool {tool_id} not found")

        timeout = context.get("timeout", self.default_timeout) if context else self.default_timeout

        async with self._semaphore:
            try:
                exec_context = context or {}
                exec_context["timeout"] = timeout

                result = await asyncio.wait_for(
                    tool.execute(parameters, exec_context),
                    timeout=timeout,
                )

                return result

            except asyncio.TimeoutError:
                logger.warning(
                    f"Tool {tool_id} execution timed out",
                    extra={"timeout": timeout},
                )
                return ToolOutput(
                    tool_id=tool_id,
                    success=False,
                    error=f"Execution timed out after {timeout}s",
                    error_code="TOOL_TIMEOUT",
                )

    async def execute_batch(
        self,
        requests: List[Dict[str, Any]],
    ) -> List[ToolOutput]:
        """
        Execute multiple tools concurrently.

        Args:
            requests: List of execution requests,
                     each with 'tool_id', 'parameters', optional 'context'.

        Returns:
            List of ToolOutput results in same order as requests.
        """
        tasks = [
            self.execute(
                tool_id=req["tool_id"],
                parameters=req["parameters"],
                context=req.get("context"),
            )
            for req in requests
        ]

        results = await asyncio.gather(*tasks, return_exceptions=True)

        outputs = []
        for i, result in enumerate(results):
            if isinstance(result, Exception):
                tool_id = requests[i].get("tool_id", "unknown")
                outputs.append(ToolOutput(
                    tool_id=tool_id,
                    success=False,
                    error=str(result),
                    error_code="TOOL_EXECUTION_ERROR",
                ))
            else:
                outputs.append(result)

        return outputs

    async def shutdown(self) -> None:
        """
        Gracefully shutdown the executor.

        Cleans up all tool instances.
        """
        if self._shutdown:
            return

        self._shutdown = True

        async with self._lock:
            for tool_id, tool in self._tool_instances.items():
                try:
                    await asyncio.wait_for(
                        tool.initialize(),
                        timeout=5.0,
                    )
                except Exception as e:
                    logger.warning(
                        f"Tool {tool_id} cleanup failed: {e}"
                    )

        logger.info("ToolExecutor shutdown complete")

    async def get_stats(self) -> Dict[str, Any]:
        """Get executor statistics."""
        async with self._lock:
            tool_stats = {
                tid: tool.get_stats()
                for tid, tool in self._tool_instances.items()
            }

        return {
            "total_registered": len(self._tool_classes),
            "total_instances": len(self._tool_instances),
            "max_concurrent": self.max_concurrent,
            "tools": tool_stats,
        }
