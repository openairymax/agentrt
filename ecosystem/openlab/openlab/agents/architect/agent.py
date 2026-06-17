"""
openlab.agents.architect - Architecture Design Agent

Provides architecture design and code review capabilities.
"""

from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional
from openlab.core import Agent, AgentCapability, AgentContext, AgentStatus


@dataclass
class ArchitectConfig:
    workspace_root: str = "."
    max_file_size: int = 1024 * 1024
    forbidden_paths: List[str] = field(default_factory=lambda: [
        "/etc", "/root", "/var/log", ".ssh", ".env"
    ])
    allowed_extensions: List[str] = field(default_factory=lambda: [
        ".py", ".js", ".ts", ".go", ".rs", ".c", ".h", ".md", ".json", ".yaml"
    ])
    dangerous_extensions: List[str] = field(default_factory=lambda: [
        ".exe", ".bat", ".sh", ".cmd", ".com", ".vbs", ".ps1"
    ])


class ArchitectAgent(Agent):
    """Architecture design and code review agent."""

    def __init__(self, config: Optional[ArchitectConfig] = None):
        super().__init__(
            name="architect",
            capabilities=[
                AgentCapability.CODE_GENERATION,
                AgentCapability.CODE_REVIEW,
                AgentCapability.ARCHITECTURE_DESIGN,
            ]
        )
        self.config = config or ArchitectConfig()

    async def initialize(self, context: AgentContext) -> None:
        self._status = AgentStatus.READY

    async def execute(self, context: AgentContext, input_data: Dict[str, Any]) -> Dict[str, Any]:
        self._status = AgentStatus.RUNNING
        try:
            task_type = input_data.get("task_type", "analyze")

            if task_type == "analyze":
                result = await self._analyze_architecture(input_data)
            elif task_type == "review":
                result = await self._review_code(input_data)
            elif task_type == "design":
                result = await self._design_architecture(input_data)
            else:
                result = await self._analyze_architecture(input_data)

            self._status = AgentStatus.READY
            return result
        except Exception as e:
            self._status = AgentStatus.ERROR
            return {"error": str(e), "status": "failed"}

    async def _analyze_architecture(self, input_data: Dict[str, Any]) -> Dict[str, Any]:
        return {
            "status": "completed",
            "analysis": {
                "architecture_type": "modular",
                "components": [],
                "dependencies": [],
                "recommendations": [],
            }
        }

    async def _review_code(self, input_data: Dict[str, Any]) -> Dict[str, Any]:
        return {
            "status": "completed",
            "review": {
                "issues": [],
                "suggestions": [],
                "score": 0,
            }
        }

    async def _design_architecture(self, input_data: Dict[str, Any]) -> Dict[str, Any]:
        return {
            "status": "completed",
            "design": {
                "modules": [],
                "interfaces": [],
                "data_flow": [],
            }
        }

    def _is_path_in_forbidden_list(self, path: str) -> bool:
        for forbidden in self.config.forbidden_paths:
            if path.startswith(forbidden):
                return True
        return False

    def _is_path_within_workspace(self, path: str) -> bool:
        return path.startswith(self.config.workspace_root)

    def _is_extension_dangerous(self, filename: str) -> bool:
        for ext in self.config.dangerous_extensions:
            if filename.endswith(ext):
                return True
        return False

    async def shutdown(self) -> None:
        self._status = AgentStatus.STOPPED


__all__ = ["ArchitectAgent", "ArchitectConfig"]
