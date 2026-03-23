"""
Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

OpenHub Agent Management Module
================================

This module provides core Agent lifecycle management, including:
- Agent registration and discovery
- Agent state management
- Capability-based routing
- Resource allocation

Architecture:
    AgentRegistry -> AgentManager -> Agent instances
    Each agent operates within a secure Workbench (virtual sandbox).

Usage:
    from openhub.core.agent import Agent, AgentRegistry, AgentManager

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
from pathlib import Path
from typing import (
    Any,
    Callable,
    Dict,
    List,
    Optional,
    Set,
    TypeVar,
    Generic,
    Awaitable,
    Union,
)

logger = logging.getLogger(__name__)


class AgentStatus(Enum):
    """Agent operational status states."""
    CREATED = auto()
    INITIALIZING = auto()
    IDLE = auto()
    BUSY = auto()
    SUSPENDED = auto()
    TERMINATING = auto()
    TERMINATED = auto()
    ERROR = auto()


class AgentCapability(Enum):
    """Standard capability types that agents can declare."""
    CODE_GENERATION = "code_generation"
    CODE_REVIEW = "code_review"
    ARCHITECTURE_DESIGN = "architecture_design"
    DATA_ANALYSIS = "data_analysis"
    DOCUMENTATION = "documentation"
    TEST_GENERATION = "test_generation"
    DEPLOYMENT = "deployment"
    MONITORING = "monitoring"
    SECURITY_AUDIT = "security_audit"
    API_INTEGRATION = "api_integration"
    DATABASE_DESIGN = "database_design"
    FRONTEND_DEV = "frontend_development"
    BACKEND_DEV = "backend_development"
    DEVOPS = "devops_automation"
    RESEARCH = "research_analysis"


@dataclass
class AgentMetadata:
    """
    Metadata structure for Agent registration and discovery.

    Attributes:
        agent_id: Unique identifier for the agent instance.
        agent_type: Semantic type name (e.g., 'architect', 'backend').
        version: Agent implementation version string.
        capabilities: Set of capability flags this agent supports.
        description: Human-readable description of agent purpose.
        config_schema: JSON schema for agent configuration.
        resource_requirements: Dictionary of resource constraints.
        tags: Arbitrary metadata tags for discovery.
    """
    agent_id: str
    agent_type: str
    version: str
    capabilities: Set[AgentCapability]
    description: str
    config_schema: Dict[str, Any]
    resource_requirements: Dict[str, Any] = field(default_factory=dict)
    tags: Dict[str, str] = field(default_factory=dict)
    created_at: float = field(default_factory=time.time)
    updated_at: float = field(default_factory=time.time)


@dataclass
class AgentContext:
    """
    Execution context passed to agents during task processing.

    Attributes:
        task_id: Unique identifier of the task being processed.
        trace_id: Distributed tracing identifier for observability.
        session_id: User session identifier.
        workbench_id: Secure sandbox identifier.
        timeout_seconds: Maximum execution time allowed.
        metadata: Additional context-specific data.
    """
    task_id: str
    trace_id: str
    session_id: str
    workbench_id: Optional[str] = None
    timeout_seconds: int = 300
    metadata: Dict[str, Any] = field(default_factory=dict)


@dataclass
class TaskResult:
    """
    Standardized result structure returned by agent processing.

    Attributes:
        success: Whether task completed successfully.
        output: Task output data (agent-specific format).
        error: Error message if success is False.
        error_code: Standardized error code.
        metrics: Execution metrics (duration, tokens, etc.).
    """
    success: bool
    output: Any = None
    error: Optional[str] = None
    error_code: Optional[str] = None
    metrics: Dict[str, Any] = field(default_factory=dict)


class Agent(ABC):
    """
    Abstract base class for all OpenHub Agents.

    Agents are the primary execution units in OpenHub. Each agent:
    - Declares its capabilities via AgentCapability flags
    - Processes tasks within a secure Workbench context
    - Returns standardized TaskResult objects
    - Reports health and metrics for observability

    Subclasses must implement:
        - _do_initialize(): Async initialization logic
        - _do_execute(): Core task processing logic
        - _do_shutdown(): Cleanup logic

    Example:
        class MyAgent(Agent):
            CAPABILITIES = {AgentCapability.CODE_GENERATION}

            async def _do_initialize(self, config: Dict[str, Any]) -> None:
                self.model = load_model(config["model_path"])

            async def _do_execute(self, context: AgentContext,
                                  input_data: Any) -> TaskResult:
                result = await self.model.generate(input_data["prompt"])
                return TaskResult(success=True, output={"text": result})

        agent = MyAgent(agent_id="my-agent", config={...})
        await agent.initialize()
        result = await agent.execute(context, {"prompt": "Hello"})
    """

    # Class-level capability declaration (override in subclasses)
    CAPABILITIES: Set[AgentCapability] = set()

    def __init__(
        self,
        agent_id: str,
        agent_type: str,
        config: Optional[Dict[str, Any]] = None,
        workbench_id: Optional[str] = None,
    ) -> None:
        """
        Initialize a new Agent instance.

        Args:
            agent_id: Unique identifier for this agent instance.
            agent_type: Semantic type (e.g., 'architect', 'backend').
            config: Agent-specific configuration dictionary.
            workbench_id: Optional workbench sandbox identifier.
        """
        self.agent_id = agent_id
        self.agent_type = agent_type
        self.config = config or {}
        self.workbench_id = workbench_id

        self._status = AgentStatus.CREATED
        self._metadata = AgentMetadata(
            agent_id=agent_id,
            agent_type=agent_type,
            version=self.config.get("version", "1.0.0"),
            capabilities=self.CAPABILITIES,
            description=self.__doc__ or "",
            config_schema=self._get_config_schema(),
        )
        self._stats = {
            "tasks_processed": 0,
            "tasks_succeeded": 0,
            "tasks_failed": 0,
            "total_execution_time_ms": 0,
        }

        logger.info(
            "Agent instance created",
            extra={
                "agent_id": agent_id,
                "agent_type": agent_type,
                "capabilities": [c.value for c in self.CAPABILITIES],
            },
        )

    @property
    def status(self) -> AgentStatus:
        """Get current agent status."""
        return self._status

    @property
    def metadata(self) -> AgentMetadata:
        """Get agent metadata."""
        return self._metadata

    @property
    def stats(self) -> Dict[str, Any]:
        """Get agent statistics."""
        return self._stats.copy()

    async def initialize(self) -> None:
        """
        Initialize the agent asynchronously.

        This method must be called before execute().

        Raises:
            RuntimeError: If agent is not in CREATED or ERROR state.
            Exception: Agent-specific initialization errors.
        """
        if self._status not in (AgentStatus.CREATED, AgentStatus.ERROR):
            raise RuntimeError(
                f"Agent {self.agent_id} cannot initialize from status {self._status}"
            )

        self._status = AgentStatus.INITIALIZING
        logger.info(f"Initializing agent {self.agent_id}")

        try:
            await self._do_initialize(self.config)
            self._status = AgentStatus.IDLE
            logger.info(f"Agent {self.agent_id} initialized successfully")
        except Exception as e:
            self._status = AgentStatus.ERROR
            logger.error(f"Agent {self.agent_id} initialization failed: {e}")
            raise

    async def execute(
        self, context: AgentContext, input_data: Any
    ) -> TaskResult:
        """
        Execute a task with the given input.

        Args:
            context: Execution context with task metadata.
            input_data: Task input data (format defined by agent type).

        Returns:
            TaskResult with success status and output/error.

        Raises:
            RuntimeError: If agent is not initialized.
        """
        if self._status not in (AgentStatus.IDLE, AgentStatus.BUSY):
            raise RuntimeError(
                f"Agent {self.agent_id} cannot execute from status {self._status}"
            )

        self._status = AgentStatus.BUSY
        start_time = time.time()

        logger.info(
            "Agent executing task",
            extra={
                "agent_id": self.agent_id,
                "task_id": context.task_id,
                "trace_id": context.trace_id,
            },
        )

        try:
            result = await self._do_execute(context, input_data)

            execution_time_ms = (time.time() - start_time) * 1000
            result.metrics["execution_time_ms"] = execution_time_ms
            result.metrics["agent_id"] = self.agent_id

            self._stats["tasks_processed"] += 1
            self._stats["tasks_succeeded"] += 1
            self._stats["total_execution_time_ms"] += execution_time_ms

            logger.info(
                "Agent task completed",
                extra={
                    "agent_id": self.agent_id,
                    "task_id": context.task_id,
                    "execution_time_ms": execution_time_ms,
                    "success": result.success,
                },
            )

            return result

        except Exception as e:
            execution_time_ms = (time.time() - start_time) * 1000

            self._stats["tasks_processed"] += 1
            self._stats["tasks_failed"] += 1
            self._stats["total_execution_time_ms"] += execution_time_ms

            logger.error(
                "Agent task failed",
                extra={
                    "agent_id": self.agent_id,
                    "task_id": context.task_id,
                    "error": str(e),
                    "execution_time_ms": execution_time_ms,
                },
            )

            return TaskResult(
                success=False,
                error=str(e),
                error_code="AGENT_EXECUTION_ERROR",
                metrics={"execution_time_ms": execution_time_ms},
            )

        finally:
            if self._status == AgentStatus.BUSY:
                self._status = AgentStatus.IDLE

    async def shutdown(self) -> None:
        """
        Gracefully shutdown the agent.

        Raises:
            RuntimeError: If agent is in TERMINATED state.
        """
        if self._status == AgentStatus.TERMINATED:
            raise RuntimeError(f"Agent {self.agent_id} already terminated")

        self._status = AgentStatus.TERMINATING
        logger.info(f"Shutting down agent {self.agent_id}")

        try:
            await self._do_shutdown()
            self._status = AgentStatus.TERMINATED
            logger.info(f"Agent {self.agent_id} shutdown complete")
        except Exception as e:
            self._status = AgentStatus.ERROR
            logger.error(f"Agent {self.agent_id} shutdown failed: {e}")
            raise

    def get_health(self) -> Dict[str, Any]:
        """
        Get agent health status for observability.

        Returns:
            Dictionary containing health metrics.
        """
        avg_execution_time = (
            self._stats["total_execution_time_ms"] / self._stats["tasks_processed"]
            if self._stats["tasks_processed"] > 0
            else 0
        )

        return {
            "agent_id": self.agent_id,
            "status": self._status.name,
            "uptime_seconds": time.time() - self.metadata.created_at,
            "tasks": {
                "processed": self._stats["tasks_processed"],
                "success_rate": (
                    self._stats["tasks_succeeded"] / self._stats["tasks_processed"]
                    if self._stats["tasks_processed"] > 0
                    else 0
                ),
                "avg_execution_time_ms": avg_execution_time,
            },
            "capabilities": [c.value for c in self.CAPABILITIES],
        }

    @abstractmethod
    async def _do_initialize(self, config: Dict[str, Any]) -> None:
        """
        Subclass-specific initialization logic.

        Args:
            config: Agent configuration dictionary.

        Raises:
            Exception: Any initialization error.
        """
        pass

    @abstractmethod
    async def _do_execute(
        self, context: AgentContext, input_data: Any
    ) -> TaskResult:
        """
        Subclass-specific task execution logic.

        Args:
            context: Execution context.
            input_data: Task input data.

        Returns:
            TaskResult with execution outcome.

        Raises:
            Exception: Any execution error.
        """
        pass

    @abstractmethod
    async def _do_shutdown(self) -> None:
        """
        Subclass-specific shutdown logic.

        Raises:
            Exception: Any cleanup error.
        """
        pass

    def _get_config_schema(self) -> Dict[str, Any]:
        """
        Get JSON schema for agent configuration.

        Returns:
            JSON schema dictionary (default empty schema).
        """
        return {
            "type": "object",
            "properties": {},
            "required": [],
        }


T = TypeVar("T", bound=Agent)


class AgentRegistry:
    """
    Central registry for agent metadata and discovery.

    The registry maintains a catalog of available agents
    and their declared capabilities for routing decisions.

    Example:
        registry = AgentRegistry()

        # Register an agent
        await registry.register(my_agent.metadata)

        # Find agents by capability
        agents = registry.find_by_capability(AgentCapability.CODE_GENERATION)

        # Get agent metadata
        metadata = registry.get("agent-001")
    """

    def __init__(self) -> None:
        """Initialize an empty registry."""
        self._agents: Dict[str, AgentMetadata] = {}
        self._capability_index: Dict[AgentCapability, Set[str]] = {
            cap: set() for cap in AgentCapability
        }
        self._type_index: Dict[str, Set[str]] = {}
        self._lock = asyncio.Lock()

        logger.info("AgentRegistry initialized")

    async def register(self, metadata: AgentMetadata) -> None:
        """
        Register an agent's metadata in the catalog.

        Args:
            metadata: Agent metadata to register.

        Raises:
            ValueError: If agent_id already exists.
        """
        async with self._lock:
            if metadata.agent_id in self._agents:
                raise ValueError(
                    f"Agent {metadata.agent_id} already registered"
                )

            self._agents[metadata.agent_id] = metadata

            for cap in metadata.capabilities:
                self._capability_index[cap].add(metadata.agent_id)

            if metadata.agent_type not in self._type_index:
                self._type_index[metadata.agent_type] = set()
            self._type_index[metadata.agent_type].add(metadata.agent_id)

            logger.info(
                "Agent registered",
                extra={
                    "agent_id": metadata.agent_id,
                    "type": metadata.agent_type,
                    "capabilities": [c.value for c in metadata.capabilities],
                },
            )

    async def unregister(self, agent_id: str) -> None:
        """
        Unregister an agent from the catalog.

        Args:
            agent_id: ID of agent to remove.

        Raises:
            KeyError: If agent_id not found.
        """
        async with self._lock:
            metadata = self._agents.pop(agent_id, None)
            if metadata is None:
                raise KeyError(f"Agent {agent_id} not found")

            for cap in metadata.capabilities:
                self._capability_index[cap].discard(agent_id)

            self._type_index.get(metadata.agent_type, set()).discard(agent_id)

            logger.info(f"Agent {agent_id} unregistered")

    async def get(self, agent_id: str) -> Optional[AgentMetadata]:
        """
        Get agent metadata by ID.

        Args:
            agent_id: Agent identifier.

        Returns:
            AgentMetadata if found, None otherwise.
        """
        async with self._lock:
            return self._agents.get(agent_id)

    async def find_by_capability(
        self, capability: AgentCapability
    ) -> List[AgentMetadata]:
        """
        Find all agents with a given capability.

        Args:
            capability: Required capability.

        Returns:
            List of matching agent metadata.
        """
        async with self._lock:
            agent_ids = self._capability_index.get(capability, set())
            return [self._agents[aid] for aid in agent_ids if aid in self._agents]

    async def find_by_type(self, agent_type: str) -> List[AgentMetadata]:
        """
        Find all agents of a given type.

        Args:
            agent_type: Required agent type.

        Returns:
            List of matching agent metadata.
        """
        async with self._lock:
            agent_ids = self._type_index.get(agent_type, set())
            return [self._agents[aid] for aid in agent_ids if aid in self._agents]

    async def list_all(self) -> List[AgentMetadata]:
        """
        List all registered agent metadata.

        Returns:
            List of all registered agents.
        """
        async with self._lock:
            return list(self._agents.values())

    async def get_stats(self) -> Dict[str, Any]:
        """
        Get registry statistics.

        Returns:
            Dictionary with catalog statistics.
        """
        async with self._lock:
            return {
                "total_agents": len(self._agents),
                "by_capability": {
                    cap.value: len(agents)
                    for cap, agents in self._capability_index.items()
                    if agents
                },
                "by_type": {
                    atype: len(agents)
                    for atype, agents in self._type_index.items()
                    if agents
                },
            }


class AgentManager:
    """
    High-level manager for Agent lifecycle and execution.

    AgentManager provides:
    - Agent instance creation and lifecycle
    - Instance pooling and resource limits
    - Automatic health monitoring
    - Graceful shutdown coordination

    Example:
        manager = AgentManager(registry, max_instances=10)

        # Create agent instance
        agent = await manager.create_agent(
            agent_class=MyAgent,
            agent_id="agent-001",
            config={"setting": "value"},
        )

        # Execute task
        result = await agent.execute(context, input_data)

        # Cleanup
        await manager.shutdown()
    """

    def __init__(
        self,
        registry: AgentRegistry,
        max_instances: int = 100,
        shutdown_timeout: float = 30.0,
    ) -> None:
        """
        Initialize the agent manager.

        Args:
            registry: AgentRegistry instance for metadata catalog.
            max_instances: Maximum concurrent agent instances.
            shutdown_timeout: Timeout for graceful shutdown (seconds).
        """
        self.registry = registry
        self.max_instances = max_instances
        self.shutdown_timeout = shutdown_timeout

        self._instances: Dict[str, Agent] = {}
        self._lock = asyncio.Lock()
        self._shutdown = False

        logger.info(
            "AgentManager initialized",
            extra={"max_instances": max_instances},
        )

    async def create_agent(
        self,
        agent_class: type[T],
        agent_id: str,
        config: Optional[Dict[str, Any]] = None,
        workbench_id: Optional[str] = None,
    ) -> T:
        """
        Create and initialize a new agent instance.

        Args:
            agent_class: Agent subclass to instantiate.
            agent_id: Unique identifier for this instance.
            config: Agent configuration dictionary.
            workbench_id: Optional workbench sandbox ID.

        Returns:
            Initialized agent instance.

        Raises:
            RuntimeError: If manager is shutting down.
            ValueError: If max instances reached.
        """
        if self._shutdown:
            raise RuntimeError("AgentManager is shutting down")

        async with self._lock:
            if agent_id in self._instances:
                raise ValueError(f"Agent {agent_id} already exists")

            if len(self._instances) >= self.max_instances:
                raise ValueError(
                    f"Max agent instances ({self.max_instances}) reached"
                )

            agent = agent_class(
                agent_id=agent_id,
                agent_type=agent_class.__name__.lower(),
                config=config,
                workbench_id=workbench_id,
            )

            self._instances[agent_id] = agent

        await agent.initialize()
        await self.registry.register(agent.metadata)

        logger.info(f"Agent {agent_id} created and registered")
        return agent

    async def get_agent(self, agent_id: str) -> Optional[Agent]:
        """
        Get a running agent instance.

        Args:
            agent_id: Agent identifier.

        Returns:
            Agent instance if running, None otherwise.
        """
        async with self._lock:
            return self._instances.get(agent_id)

    async def shutdown_agent(self, agent_id: str) -> None:
        """
        Gracefully shutdown a specific agent.

        Args:
            agent_id: Agent to shutdown.

        Raises:
            KeyError: If agent not found.
        """
        async with self._lock:
            agent = self._instances.pop(agent_id, None)
            if agent is None:
                raise KeyError(f"Agent {agent_id} not found")

        try:
            await asyncio.wait_for(
                agent.shutdown(),
                timeout=self.shutdown_timeout,
            )
            await self.registry.unregister(agent_id)
        except asyncio.TimeoutError:
            logger.warning(
                f"Agent {agent_id} shutdown timed out"
            )

    async def shutdown(self) -> None:
        """
        Gracefully shutdown all agents and the manager.

        This method:
        1. Marks manager as shutting down
        2. Shuts down all agent instances concurrently
        3. Waits for completion with timeout
        """
        if self._shutdown:
            return

        self._shutdown = True
        logger.info("AgentManager shutting down")

        async with self._lock:
            agent_ids = list(self._instances.keys())

        if agent_ids:
            shutdown_tasks = [
                self.shutdown_agent(aid) for aid in agent_ids
            ]
            await asyncio.wait(
                shutdown_tasks,
                timeout=self.shutdown_timeout,
            )

        logger.info("AgentManager shutdown complete")

    async def get_health_report(self) -> Dict[str, Any]:
        """
        Get health report for all agents.

        Returns:
            Dictionary with health status of all agents.
        """
        async with self._lock:
            instance_health = {
                aid: agent.get_health()
                for aid, agent in self._instances.items()
            }

        return {
            "manager_status": "shutting_down" if self._shutdown else "running",
            "total_instances": len(self._instances),
            "max_instances": self.max_instances,
            "agents": instance_health,
        }
