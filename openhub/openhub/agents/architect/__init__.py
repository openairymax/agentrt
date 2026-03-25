"""
Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

Architect Agent Package
=====================

This package provides the Architect Agent implementation for OpenHub.

Usage:
    from openhub.agents.architect.agent import ArchitectAgent, demo

    async def main():
        agent = ArchitectAgent(agent_id="my-architect")
        await agent.initialize()
        result = await agent.execute(context, input_data)
        await agent.shutdown()

Author: AgentOS Architecture Committee
Version: 1.0.0.6
"""

from openhub.agents.architect.agent import (
    ArchitectAgent,
    ArchitectureDesignTool,
    ArchitectureInput,
    ArchitectureOutput,
    CodeAnalysisTool,
    DesignPattern,
    FileReadTool,
    FileWriteTool,
    NonFunctionalRequirement,
    demo,
)

__all__ = [
    "ArchitectAgent",
    "ArchitectureDesignTool",
    "ArchitectureInput",
    "ArchitectureOutput",
    "CodeAnalysisTool",
    "DesignPattern",
    "FileReadTool",
    "FileWriteTool",
    "NonFunctionalRequirement",
    "demo",
]
