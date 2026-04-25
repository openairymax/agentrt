"""
Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

openlab - Agent Operating System
=================================

openlab is a production-grade multi-agent orchestration framework.

Core Modules:
    - core: Agent, Task, Tool, Storage management
    - agents: Pre-built agent implementations
    - contrib: Contributed agents, skills, and strategies

Example:
    import asyncio
    from openlab.core.agent import AgentManager, AgentRegistry
    from openlab.agents.architect import ArchitectAgent

    async def main():
        registry = AgentRegistry()
        manager = AgentManager(registry)

        agent = await manager.create_agent(
            agent_class=ArchitectAgent,
            agent_id="architect-001",
            manager={"verbose": True},
        )

        result = await agent.execute(context, input_data)
        print(result)

        await manager.shutdown()

    asyncio.run(main())

Author: Spharx AgentOS Team
Version: 1.0.0.6
"""

from openlab.core import (
    Agent,
    AgentCapability,
    AgentContext,
    AgentRegistry,
    AgentStatus,
    TaskResult,
    Message,
    TaskStatus,
    TaskCategory,
    TaskDefinition,
    TaskState,
    ExecutionPlan,
    TaskScheduler,
    Tool,
    ToolCapability,
    ToolCategory,
    ToolContext,
    ToolResult,
    ToolRegistry,
    ToolExecutor,
    Storage,
    StorageType,
    DataCategory,
    StorageRecord,
    QueryResult,
    MemoryStorage,
    SQLiteStorage,
)

from openlab.agents.architect import ArchitectAgent, ArchitectConfig

__version__ = "1.0.0.6"

__all__ = [
    # Version
    "__version__",
    # Core - Agent
    "Agent",
    "AgentCapability",
    "AgentContext",
    "AgentManager",
    "AgentMetadata",
    "AgentRegistry",
    "AgentStatus",
    "TaskResult",
    # Core - Task
    "Task",
    "TaskDefinition",
    "TaskEvent",
    "TaskHandler",
    "TaskPriority",
    "TaskScheduler",
    "TaskState",
    "TaskStatus",
    # Core - Tool
    "Tool",
    "ToolCapability",
    "ToolCategory",
    "ToolExecutor",
    "ToolInput",
    "ToolMetadata",
    "ToolOutput",
    "ToolRegistry",
    # Core - Storage
    "InMemoryStorage",
    "Query",
    "QueryCondition",
    "QueryOperator",
    "SQLiteStorage",
    "Storage",
    "StorageBackend",
    "StorageRecord",
    "StorageStats",
    # Agents
    "ArchitectAgent",
    "architect_demo",
]
