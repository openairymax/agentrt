# Copyright (c) 2026 SPHARX. All Rights Reserved.
# "From data intelligence emerges."

"""
Architect Agent
==============

This is a compatibility shim that re-exports from openhub.agents.architect.

For new code, use:
    from openhub.agents.architect import ArchitectAgent

This module exists for backward compatibility with the contrib directory structure.
"""

from openhub.agents.architect import (
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
