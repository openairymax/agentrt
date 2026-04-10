"""
Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

Architect Agent - Complete Implementation
========================================

A production-ready Agent specialized in software architecture design and review.
This agent demonstrates:
- Complete initialization with configuration
- Task reception and processing pipeline
- Tool calling workflow
- Structured result return mechanism

Author: Spharx AgentOS Team
Version: 1.0.0.6
"""

from __future__ import annotations

import asyncio
import json
import logging
import time
import uuid
from dataclasses import dataclass, field
from enum import Enum, auto
from pathlib import Path
from typing import (
    Any,
    Dict,
    List,
    Optional,
    Set,
)

from openlab.core.agent import (
    Agent,
    AgentCapability,
    AgentContext,
    TaskResult,
)
from openlab.core.tool import (
    Tool,
    ToolCapability,
    ToolCategory,
    ToolExecutor,
    ToolRegistry,
)

logger = logging.getLogger(__name__)


class DesignPattern(Enum):
    """Software architecture design patterns."""
    MICROSERVICES = "microservices"
    MONOLITHIC = "monolithic"
    LAYERED = "layered"
    EVENT_DRIVEN = "event_driven"
    CQRS = "cqrs"
    HEXAGONAL = "hexagonal"
    CLEAN = "clean_architecture"
    SERVERLESS = "serverless"


class NonFunctionalRequirement(Enum):
    """Non-functional requirement categories."""
    PERFORMANCE = "performance"
    SCALABILITY = "scalability"
    AVAILABILITY = "availability"
    SECURITY = "security"
    MAINTAINABILITY = "maintainability"
    TESTABILITY = "testability"
    DEPLOYABILITY = "deployability"


@dataclass
class ArchitectureInput:
    """
    Input specification for architecture design task.

    Attributes:
        project_name: Name of the project.
        project_type: Type of project (web_app, mobile_app, api, etc.).
        requirements: List of functional requirements.
        constraints: Technical constraints (budget, timeline, team_size).
        quality_attributes: Non-functional requirements with priorities.
    """
    project_name: str
    project_type: str
    requirements: List[str]
    constraints: Dict[str, Any]
    quality_attributes: Dict[str, str]


@dataclass
class ArchitectureOutput:
    """
    Output specification for architecture design task.

    Attributes:
        architecture_type: Selected architecture pattern.
        component_diagram: Component relationship description.
        technology_stack: Recommended technologies per component.
        deployment_model: Deployment architecture.
        tradeoffs: Design decisions with rationale.
        risks: Identified architectural risks.
    """
    architecture_type: DesignPattern
    component_diagram: Dict[str, Any]
    technology_stack: Dict[str, List[str]]
    deployment_model: Dict[str, Any]
    tradeoffs: List[Dict[str, str]]
    risks: List[Dict[str, str]]
    estimated_complexity: str
    recommended_iterations: int


class FileReadTool(Tool):
    """
    Tool for reading files from the filesystem.

    Capabilities:
    - Read text files
    - List directory contents
    - Get file metadata

    Security:
    - Path whitelist enforcement
    - Workspace sandbox isolation
    - Sensitive file access prevention
    """

    CAPABILITIES = {ToolCapability.FILE_READ}
    INPUT_SCHEMA = {
        "type": "object",
        "properties": {
            "path": {"type": "string", "description": "File or directory path"},
            "max_depth": {"type": "integer", "default": 1, "description": "Maximum directory traversal depth"},
        },
        "required": ["path"],
    }
    
    # 瀹夊叏閰嶇疆
    ALLOWED_EXTENSIONS = {'.txt', '.md', '.py', '.js', '.ts', '.json', '.yaml', '.yml', '.xml', '.html', '.css', '.c', '.cpp', '.h', '.java', '.go', '.rs'}
    FORBIDDEN_PATHS = {'/etc', '/proc', '/sys', '/dev', '/boot', '/root', '/home'}
    MAX_FILE_SIZE = 10 * 1024 * 1024  # 10MB

    def __init__(self, workspace_path: Optional[str] = None):
        """
        鍒濆鍖栨枃浠惰鍙栧伐鍏?        
        Args:
            workspace_path: 宸ヤ綔绌洪棿璺緞锛堟矙绠辨牴鐩綍锛?        """
        super().__init__()
        self.workspace_path = Path(workspace_path).resolve() if workspace_path else Path.cwd()
    
    def _do_validate_input(self, parameters: Dict[str, Any]) -> None:
        """Validate read parameters with security checks."""
        path = Path(parameters["path"]).resolve()

        # 瀹夊叏妫€鏌?1: 璺緞蹇呴』鍦ㄥ伐浣滅┖闂村唴
        if not self._is_in_workspace(path):
            raise ValueError(f"Path is outside workspace: {path}")
        
        # 瀹夊叏妫€鏌?2: 绂佹璁块棶鏁忔劅璺緞
        if self._is_forbidden_path(path):
            raise ValueError(f"Access to sensitive path is forbidden: {path}")
        
        # 瀹夊叏妫€鏌?3: 鏂囦欢鎵╁睍鍚嶇櫧鍚嶅崟
        if path.is_file() and not self._is_allowed_extension(path):
            raise ValueError(f"File extension not allowed: {path.suffix}")
        
        # 瀹夊叏妫€鏌?4: 鏂囦欢澶у皬闄愬埗
        if path.is_file():
            file_size = path.stat().st_size
            if file_size > self.MAX_FILE_SIZE:
                raise ValueError(f"File too large: {file_size} bytes (max: {self.MAX_FILE_SIZE})")

    def _is_in_workspace(self, path: Path) -> bool:
        """妫€鏌ヨ矾寰勬槸鍚﹀湪宸ヤ綔绌洪棿鍐?""
        try:
            path.relative_to(self.workspace_path)
            return True
        except ValueError:
            return False
    
    def _is_forbidden_path(self, path: Path) -> bool:
        """妫€鏌ヨ矾寰勬槸鍚﹀湪绂佹鍒楄〃涓?""
        path_str = str(path)
        for forbidden in self.FORBIDDEN_PATHS:
            if path_str.startswith(forbidden):
                return True
        return False
    
    def _is_allowed_extension(self, path: Path) -> bool:
        """妫€鏌ユ枃浠舵墿灞曞悕鏄惁鍦ㄧ櫧鍚嶅崟涓?""
        return path.suffix.lower() in self.ALLOWED_EXTENSIONS

    async def _do_execute(
        self, parameters: Dict[str, Any], context: Dict[str, Any]
    ) -> Any:
        """Execute file read operation with security checks."""
        path = Path(parameters["path"]).resolve()

        if path.is_file():
            content = path.read_text(encoding="utf-8")
            return {
                "type": "file",
                "path": str(path.relative_to(self.workspace_path)),
                "size": path.stat().st_size,
                "content": content,
            }
        elif path.is_dir():
            items = []
            max_depth = parameters.get("max_depth", 1)
            for item in path.iterdir():
                items.append({
                    "name": item.name,
                    "type": "directory" if item.is_dir() else "file",
                    "path": str(item.relative_to(self.workspace_path)),
                })
            return {
                "type": "directory",
                "path": str(path.relative_to(self.workspace_path)),
                "items": items,
                "max_depth": max_depth,
            }

        raise ValueError(f"Unsupported path type: {path}")


class FileWriteTool(Tool):
    """
    Tool for writing files to the filesystem.

    Capabilities:
    - Create new files
    - Overwrite existing files
    - Create directories

    Security:
    - Path whitelist enforcement
    - Workspace sandbox isolation
    - Dangerous file type prevention
    """

    CAPABILITIES = {ToolCapability.FILE_WRITE}
    INPUT_SCHEMA = {
        "type": "object",
        "properties": {
            "path": {"type": "string", "description": "Target file path"},
            "content": {"type": "string", "description": "File content"},
            "create_dirs": {"type": "boolean", "default": True, "description": "Create parent directories"},
        },
        "required": ["path", "content"],
    }
    
    # 瀹夊叏閰嶇疆
    ALLOWED_EXTENSIONS = {'.txt', '.md', '.py', '.js', '.ts', '.json', '.yaml', '.yml', '.xml', '.html', '.css', '.c', '.cpp', '.h', '.java', '.go', '.rs', '.log', '.tmp'}
    FORBIDDEN_PATHS = {'/etc', '/proc', '/sys', '/dev', '/boot', '/root', '/home', '/bin', '/sbin', '/usr'}
    FORBIDDEN_EXTENSIONS = {'.exe', '.sh', '.bat', '.cmd', '.ps1', '.dll', '.so'}
    MAX_FILE_SIZE = 10 * 1024 * 1024  # 10MB

    def __init__(self, workspace_path: Optional[str] = None):
        """
        鍒濆鍖栨枃浠跺啓鍏ュ伐鍏?        
        Args:
            workspace_path: 宸ヤ綔绌洪棿璺緞锛堟矙绠辨牴鐩綍锛?        """
        super().__init__()
        self.workspace_path = Path(workspace_path).resolve() if workspace_path else Path.cwd()
    
    def _do_validate_input(self, parameters: Dict[str, Any]) -> None:
        """Validate write parameters with security checks."""
        path = Path(parameters["path"]).resolve()
        content = parameters.get("content", "")

        # 瀹夊叏妫€鏌?1: 璺緞蹇呴』鍦ㄥ伐浣滅┖闂村唴
        if not self._is_in_workspace(path):
            raise ValueError(f"Path is outside workspace: {path}")
        
        # 瀹夊叏妫€鏌?2: 绂佹鍐欏叆鏁忔劅璺緞
        if self._is_forbidden_path(path):
            raise ValueError(f"Writing to sensitive path is forbidden: {path}")
        
        # 瀹夊叏妫€鏌?3: 鏂囦欢鎵╁睍鍚嶇櫧鍚嶅崟
        if not self._is_allowed_extension(path):
            raise ValueError(f"File extension not allowed: {path.suffix}")
        
        # 瀹夊叏妫€鏌?4: 绂佹鍗遍櫓鏂囦欢绫诲瀷
        if self._is_forbidden_extension(path):
            raise ValueError(f"Dangerous file type forbidden: {path.suffix}")
        
        # 瀹夊叏妫€鏌?5: 鏂囦欢澶у皬闄愬埗
        if len(content.encode('utf-8')) > self.MAX_FILE_SIZE:
            raise ValueError(f"Content too large: {len(content)} bytes (max: {self.MAX_FILE_SIZE})")
        
        # 瀹夊叏妫€鏌?6: 璺緞瀛樺湪鎬ф鏌?        if path.exists() and not path.is_file():
            raise ValueError(f"Path exists and is not a file: {path}")

    def _is_in_workspace(self, path: Path) -> bool:
        """妫€鏌ヨ矾寰勬槸鍚﹀湪宸ヤ綔绌洪棿鍐?""
        try:
            path.relative_to(self.workspace_path)
            return True
        except ValueError:
            return False
    
    def _is_forbidden_path(self, path: Path) -> bool:
        """妫€鏌ヨ矾寰勬槸鍚﹀湪绂佹鍒楄〃涓?""
        path_str = str(path)
        for forbidden in self.FORBIDDEN_PATHS:
            if path_str.startswith(forbidden):
                return True
        return False
    
    def _is_allowed_extension(self, path: Path) -> bool:
        """妫€鏌ユ枃浠舵墿灞曞悕鏄惁鍦ㄧ櫧鍚嶅崟涓?""
        return path.suffix.lower() in self.ALLOWED_EXTENSIONS
    
    def _is_forbidden_extension(self, path: Path) -> bool:
        """妫€鏌ユ枃浠舵墿灞曞悕鏄惁鍦ㄥ嵄闄╃被鍨嬪垪琛ㄤ腑"""
        return path.suffix.lower() in self.FORBIDDEN_EXTENSIONS

    async def _do_execute(
        self, parameters: Dict[str, Any], context: Dict[str, Any]
    ) -> Any:
        """Execute file write operation with security checks."""
        path = Path(parameters["path"]).resolve()
        content = parameters["content"]
        create_dirs = parameters.get("create_dirs", True)

        if create_dirs:
            path.parent.mkdir(parents=True, exist_ok=True)

        path.write_text(content, encoding="utf-8")

        return {
            "type": "file_written",
            "path": str(path.relative_to(self.workspace_path)),
            "size": len(content),
        }


class CodeAnalysisTool(Tool):
    """
    Tool for analyzing code structure and quality.

    Capabilities:
    - Parse code syntax
    - Detect code smells
    - Generate complexity metrics
    """

    CAPABILITIES = {ToolCapability.CODE_RUN}
    INPUT_SCHEMA = {
        "type": "object",
        "properties": {
            "code": {"type": "string", "description": "Source code to analyze"},
            "language": {"type": "string", "description": "Programming language"},
        },
        "required": ["code"],
    }

    def _do_validate_input(self, parameters: Dict[str, Any]) -> None:
        """Validate analysis parameters."""
        if not parameters.get("code"):
            raise ValueError("Code cannot be empty")

    async def _do_execute(
        self, parameters: Dict[str, Any], context: Dict[str, Any]
    ) -> Any:
        """Execute code analysis."""
        code = parameters["code"]
        language = parameters.get("language", "unknown")

        lines = code.split("\n")
        blank_lines = sum(1 for line in lines if not line.strip())
        comment_lines = sum(
            1 for line in lines
            if line.strip().startswith(("#", "//", "/*", "*"))
        )

        max_line_length = max(len(line) for line in lines) if lines else 0
        avg_line_length = sum(len(line)
                              for line in lines) / len(lines) if lines else 0

        complexity_indicators = {
            "total_lines": len(lines),
            "code_lines": len(lines) - blank_lines - comment_lines,
            "blank_lines": blank_lines,
            "comment_lines": comment_lines,
            "max_line_length": max_line_length,
            "avg_line_length": round(avg_line_length, 2),
            "language": language,
        }

        return complexity_indicators


class ArchitectureDesignTool(Tool):
    """
    Tool for generating architecture designs based on requirements.

    This is the primary tool used by the Architect Agent.
    """

    CAPABILITIES = set()
    INPUT_SCHEMA = {
        "type": "object",
        "properties": {
            "project_name": {"type": "string"},
            "project_type": {"type": "string"},
            "requirements": {"type": "array", "items": {"type": "string"}},
            "constraints": {"type": "object"},
            "quality_attributes": {"type": "object"},
        },
        "required": ["project_name", "project_type", "requirements"],
    }

    def _do_validate_input(self, parameters: Dict[str, Any]) -> None:
        """Validate architecture input."""
        if not parameters.get("project_name"):
            raise ValueError("Project name is required")
        if not parameters.get("requirements"):
            raise ValueError("At least one requirement is needed")

    async def _do_execute(
        self, parameters: Dict[str, Any], context: Dict[str, Any]
    ) -> Any:
        """Generate architecture design."""
        project_name = parameters["project_name"]
        project_type = parameters["project_type"]
        requirements = parameters["requirements"]
        constraints = parameters.get("constraints", {})
        quality_attrs = parameters.get("quality_attributes", {})

        complexity = self._estimate_complexity(
            len(requirements), constraints
        )

        arch_type = self._select_architecture(
            project_type, len(requirements), complexity
        )

        components = self._design_components(
            project_name, requirements, arch_type
        )

        tech_stack = self._recommend_technology(
            project_type, components, quality_attrs
        )

        deployment = self._design_deployment(
            arch_type, complexity
        )

        tradeoffs = self._analyze_tradeoffs(
            arch_type, quality_attrs
        )

        risks = self._identify_risks(
            arch_type, components, constraints
        )

        return {
            "architecture_type": arch_type.value,
            "component_diagram": {
                "components": components,
                "relationships": self._describe_relationships(arch_type),
            },
            "technology_stack": tech_stack,
            "deployment_model": deployment,
            "tradeoffs": tradeoffs,
            "risks": risks,
            "estimated_complexity": complexity,
            "recommended_iterations": max(2, len(requirements) // 5),
        }

    def _estimate_complexity(
        self, num_requirements: int, constraints: Dict[str, Any]
    ) -> str:
        """Estimate project complexity."""
        score = num_requirements

        if constraints.get("budget", "").lower() == "limited":
            score += 2
        if constraints.get("timeline_weeks", 999) < 8:
            score += 3
        if constraints.get("team_size", 1) < 3:
            score += 2

        if score < 10:
            return "low"
        elif score < 20:
            return "medium"
        else:
            return "high"

    def _select_architecture(
        self, project_type: str, num_requirements: int, complexity: str
    ) -> DesignPattern:
        """Select appropriate architecture pattern."""
        type_map = {
            "web_app": DesignPattern.LAYERED,
            "mobile_app": DesignPattern.MICROSERVICES,
            "api": DesignPattern.CLEAN,
            "data_pipeline": DesignPattern.EVENT_DRIVEN,
            "ml_service": DesignPattern.CQRS,
            "iot": DesignPattern.MICROSERVICES,
        }

        base_pattern = type_map.get(project_type, DesignPattern.LAYERED)

        if complexity == "high" and base_pattern == DesignPattern.LAYERED:
            return DesignPattern.MICROSERVICES
        elif complexity == "low" and base_pattern == DesignPattern.MICROSERVICES:
            return DesignPattern.LAYERED

        return base_pattern

    def _design_components(
        self, project_name: str, requirements: List[str], arch_type: DesignPattern
    ) -> List[Dict[str, Any]]:
        """Design system components using strategy pattern."""
        base_components = self._create_base_components(project_name)
        
        if self._needs_distributed_components(arch_type):
            base_components.extend(self._create_distributed_components(project_name))
        
        return base_components

    def _create_base_components(self, project_name: str) -> List[Dict[str, Any]]:
        """Create base architecture components."""
        return [
            self._create_component(
                project_name, "api-gateway", "gateway",
                "Request routing, authentication, rate limiting", []
            ),
            self._create_component(
                project_name, "core-service", "service",
                "Core business logic", ["database"]
            ),
            self._create_component(
                project_name, "database", "data_store",
                "Persistent data storage", []
            ),
        ]

    def _create_component(
        self, 
        project_name: str, 
        suffix: str, 
        comp_type: str,
        responsibility: str, 
        dependencies: List[str]
    ) -> Dict[str, Any]:
        """Create a component dictionary with standardized structure."""
        return {
            "name": f"{project_name}-{suffix}",
            "type": comp_type,
            "responsibility": responsibility,
            "dependencies": dependencies,
        }

    def _needs_distributed_components(self, arch_type: DesignPattern) -> bool:
        """Check if architecture type needs distributed components."""
        distributed_patterns = {DesignPattern.MICROSERVICES, DesignPattern.EVENT_DRIVEN}
        return arch_type in distributed_patterns

    def _create_distributed_components(self, project_name: str) -> List[Dict[str, Any]]:
        """Create components for distributed architectures."""
        return [
            self._create_component(
                project_name, "event-bus", "messaging",
                "Async event distribution", []
            ),
            self._create_component(
                project_name, "cache", "cache",
                "High-speed data caching", []
            ),
        ]

    def _recommend_technology(
        self,
        project_type: str,
        components: List[Dict[str, Any]],
        quality_attrs: Dict[str, str],
    ) -> Dict[str, List[str]]:
        """Recommend technology stack."""
        base_stack = {
            "api_gateway": ["Kong", "NGINX", "Envoy"],
            "backend": ["Python/FastAPI", "Go", "Node.js"],
            "database": ["PostgreSQL", "MongoDB"],
            "cache": ["Redis", "Memcached"],
            "messaging": ["RabbitMQ", "Apache Kafka"],
            "container": ["Docker", "Kubernetes"],
        }

        if quality_attrs.get("scalability") == "high":
            base_stack["backend"] = ["Go", "Rust", "Java"]
            base_stack["container"].append("Helm")

        return base_stack

    def _design_deployment(
        self, arch_type: DesignPattern, complexity: str
    ) -> Dict[str, Any]:
        """Design deployment architecture."""
        base_deployment = {
            "environment": "kubernetes",
            "regions": 1 if complexity == "low" else 2,
            "replicas": {"api_gateway": 2, "core_service": 3},
            "scaling": "horizontal",
        }

        if arch_type == DesignPattern.SERVERLESS:
            return {
                "environment": "serverless",
                "provider": "aws_lambda",
                "regions": 2,
            }

        return base_deployment

    def _analyze_tradeoffs(
        self, arch_type: DesignPattern, quality_attrs: Dict[str, str]
    ) -> List[Dict[str, str]]:
        """Analyze architecture tradeoffs."""
        tradeoffs = []

        if arch_type == DesignPattern.MICROSERVICES:
            tradeoffs.extend([
                {
                    "decision": "Microservices over Monolith",
                    "pros": "Independent scaling, technology flexibility, team autonomy",
                    "cons": "Operational complexity, network latency, data consistency",
                    "selected_for": "Scalability and team productivity",
                },
            ])
        elif arch_type == DesignPattern.LAYERED:
            tradeoffs.append({
                "decision": "Layered Architecture",
                "pros": "Simple, well-understood, easy to start",
                "cons": "Potential for tight coupling, scaling limitations",
                "selected_for": "Rapid development and simplicity",
            })

        return tradeoffs

    def _identify_risks(
        self,
        arch_type: DesignPattern,
        components: List[Dict[str, Any]],
        constraints: Dict[str, Any],
    ) -> List[Dict[str, str]]:
        """Identify architectural risks."""
        risks = []

        if arch_type == DesignPattern.MICROSERVICES:
            risks.extend([
                {
                    "risk": "Distributed system complexity",
                    "likelihood": "high",
                    "impact": "high",
                    "mitigation": "Invest in observability and DevOps tooling",
                },
                {
                    "risk": "Data consistency across services",
                    "likelihood": "medium",
                    "impact": "high",
                    "mitigation": "Implement saga pattern for cross-service transactions",
                },
            ])

        if constraints.get("team_size", 999) < 5:
            risks.append({
                "risk": "Insufficient team size for microservices",
                "likelihood": "high",
                "impact": "medium",
                "mitigation": "Consider starting with modular monolith",
            })

        return risks

    def _describe_relationships(
        self, arch_type: DesignPattern
    ) -> List[Dict[str, str]]:
        """Describe component relationships."""
        if arch_type == DesignPattern.MICROSERVICES:
            return [
                {"from": "api_gateway", "to": "*", "type": "http"},
                {"from": "core_service", "to": "database", "type": "sql"},
                {"from": "core_service", "to": "cache", "type": "redis"},
                {"from": "*", "to": "event_bus", "type": "async"},
            ]
        return [
            {"from": "api_gateway", "to": "core_service", "type": "http"},
            {"from": "core_service", "to": "database", "type": "sql"},
        ]


class ArchitectAgent(Agent):
    """
    Production-ready Architect Agent.

    This agent specializes in:
    - Software architecture design
    - Architecture review and critique
    - Technology stack recommendation
    - Design pattern selection
    - Non-functional requirement analysis

    Capabilities:
    - ARCHITECTURE_DESIGN: Create architecture designs from requirements
    - CODE_REVIEW: Review existing architectures
    - DATA_ANALYSIS: Analyze system quality attributes

    Example:
        agent = ArchitectAgent(
            agent_id="architect-001",
            manager={"verbose": True, "output_format": "json"}
        )

        await agent.initialize()

        result = await agent.execute(
            context,
            {
                "project_name": "E-Commerce Platform",
                "project_type": "web_app",
                "requirements": [
                    "User authentication with OAuth2",
                    "Product catalog with search",
                    "Shopping cart functionality",
                ],
                "constraints": {"budget": "medium", "timeline_weeks": 12},
                "quality_attributes": {"scalability": "high", "security": "high"},
            }
        )
    """

    CAPABILITIES = {
        AgentCapability.ARCHITECTURE_DESIGN,
        AgentCapability.CODE_REVIEW,
        AgentCapability.DATA_ANALYSIS,
    }

    def __init__(
        self,
        agent_id: str,
        manager: Optional[Dict[str, Any]] = None,
        workbench_id: Optional[str] = None,
    ) -> None:
        """
        Initialize the Architect Agent.

        Args:
            agent_id: Unique agent identifier.
            manager: Agent configuration with options:
                - verbose: Enable verbose logging
                - output_format: "json" or "markdown"
                - tool_timeout: Tool execution timeout
            workbench_id: Secure workbench identifier.
        """
        super().__init__(
            agent_id=agent_id,
            agent_type="architect",
            manager=manager or {},
            workbench_id=workbench_id,
        )

        self._tools: Dict[str, Tool] = {}
        self._tool_executor: Optional[ToolExecutor] = None
        self._verbose = self.manager.get("verbose", False)
        self._output_format = self.manager.get("output_format", "json")

        self._design_tool: Optional[ArchitectureDesignTool] = None
        self._file_read_tool: Optional[FileReadTool] = None
        self._file_write_tool: Optional[FileWriteTool] = None
        self._analysis_tool: Optional[CodeAnalysisTool] = None

        logger.info(
            "ArchitectAgent instance created",
            extra={"agent_id": agent_id, "verbose": self._verbose},
        )

    async def _do_initialize(self, manager: Dict[str, Any]) -> None:
        """
        Initialize agent resources and tools.

        Args:
            manager: Agent configuration dictionary.
        """
        logger.info(f"Initializing ArchitectAgent {self.agent_id}")

        await self._initialize_tools()

        self._tool_executor = ToolExecutor(
            registry=ToolRegistry(),
            max_concurrent=10,
            default_timeout=manager.get("tool_timeout", 60.0),
        )

        logger.info(f"ArchitectAgent {self.agent_id} initialized")

    async def _do_execute(
        self, context: AgentContext, input_data: Any
    ) -> TaskResult:
        """Execute architecture design task with low complexity."""
        start_time = time.time()
        
        try:
            arch_input = self._parse_and_validate(input_data)
            self._log_task_start(arch_input)
            
            design = await self._execute_design_pipeline(arch_input)
            output = self._format_output(design)
            
            return self._build_success_result(start_time, design, output)
            
        except ValueError as e:
            return self._create_error_result("INVALID_INPUT", str(e))
        except Exception as e:
            return self._create_error_result("DESIGN_ERROR", str(e))

    def _parse_and_validate(self, input_data: Any) -> ArchitectureInput:
        """Parse and validate input data with detailed error messages."""
        if not isinstance(input_data, dict):
            raise ValueError(f"Invalid input type: {type(input_data).__name__}, expected dict")
        
        required_fields = ["project_name"]
        for field in required_fields:
            if field not in input_data or not input_data[field]:
                raise ValueError(f"Missing required field: {field}")
        
        return ArchitectureInput(
            project_name=input_data["project_name"],
            project_type=input_data.get("project_type", "web_app"),
            requirements=input_data.get("requirements", []),
            constraints=input_data.get("constraints", {}),
            quality_attributes=input_data.get("quality_attributes", {}),
        )

    def _log_task_start(self, arch_input: ArchitectureInput) -> None:
        """Log task start information if verbose mode is enabled."""
        if self._verbose:
            logger.info(
                "Processing architecture task",
                extra={
                    "project_name": arch_input.project_name,
                    "num_requirements": len(arch_input.requirements),
                    "project_type": arch_input.project_type,
                },
            )

    async def _execute_design_pipeline(self, arch_input: ArchitectureInput) -> ArchitectureOutput:
        """Execute the full design pipeline with error handling."""
        if self._design_tool is None:
            raise RuntimeError("Design tool not initialized")
        
        result = await self._design_tool.execute({
            "project_name": arch_input.project_name,
            "project_type": arch_input.project_type,
            "requirements": arch_input.requirements,
            "constraints": arch_input.constraints,
            "quality_attributes": arch_input.quality_attributes,
        })
        
        if not result.success:
            raise RuntimeError(f"Design tool execution failed: {result.error}")
        
        design_data = result.result
        return ArchitectureOutput(
            architecture_type=DesignPattern(design_data["architecture_type"]),
            component_diagram=design_data["component_diagram"],
            technology_stack=design_data["technology_stack"],
            deployment_model=design_data["deployment_model"],
            tradeoffs=design_data["tradeoffs"],
            risks=design_data["risks"],
            estimated_complexity=design_data["estimated_complexity"],
            recommended_iterations=design_data["recommended_iterations"],
        )

    def _build_success_result(
        self, 
        start_time: float, 
        design: ArchitectureOutput, 
        output: Dict[str, Any]
    ) -> TaskResult:
        """Build successful task result with metrics."""
        execution_time = (time.time() - start_time) * 1000
        
        return TaskResult(
            success=True,
            output=output,
            metrics={
                "execution_time_ms": round(execution_time, 2),
                "num_components": len(design.component_diagram.get("components", [])),
                "num_risks": len(design.risks),
                "complexity": design.estimated_complexity,
                "architecture_type": design.architecture_type.value,
            },
        )

    def _create_error_result(self, error_code: str, message: str) -> TaskResult:
        """Create standardized error result."""
        log_method = logger.warning if error_code == "INVALID_INPUT" else logger.error
        log_method(f"Task failed [{error_code}]: {message}")
        
        return TaskResult(
            success=False,
            error=message,
            error_code=error_code,
        )

    async def _do_shutdown(self) -> None:
        """Cleanup agent resources."""
        logger.info(f"Shutting down ArchitectAgent {self.agent_id}")

        if self._tool_executor:
            await self._tool_executor.shutdown()

        self._tools.clear()

        logger.info(f"ArchitectAgent {self.agent_id} shutdown complete")

    async def _initialize_tools(self) -> None:
        """Initialize agent tools."""
        self._design_tool = ArchitectureDesignTool(
            tool_id=f"{self.agent_id}-design-tool",
            name="architecture-design",
        )
        self._tools["design"] = self._design_tool

        self._file_read_tool = FileReadTool(
            tool_id=f"{self.agent_id}-file-read",
            name="file-read",
        )
        self._tools["file_read"] = self._file_read_tool

        self._file_write_tool = FileWriteTool(
            tool_id=f"{self.agent_id}-file-write",
            name="file-write",
        )
        self._tools["file_write"] = self._file_write_tool

        self._analysis_tool = CodeAnalysisTool(
            tool_id=f"{self.agent_id}-analysis",
            name="code-analysis",
        )
        self._tools["analysis"] = self._analysis_tool

        for tool in self._tools.values():
            await tool.initialize()

        logger.info(
            f"ArchitectAgent tools initialized",
            extra={"tools": list(self._tools.keys())},
        )

    def _parse_input(self, input_data: Any) -> ArchitectureInput:
        """Parse and validate input data."""
        if isinstance(input_data, dict):
            return ArchitectureInput(
                project_name=input_data.get("project_name", "Unnamed Project"),
                project_type=input_data.get("project_type", "web_app"),
                requirements=input_data.get("requirements", []),
                constraints=input_data.get("constraints", {}),
                quality_attributes=input_data.get("quality_attributes", {}),
            )

        raise ValueError(f"Invalid input type: {type(input_data)}")

    async def _generate_architecture_design(
        self, arch_input: ArchitectureInput
    ) -> ArchitectureOutput:
        """Generate architecture design using tools."""
        if self._design_tool is None:
            raise RuntimeError("Design tool not initialized")

        result = await self._design_tool.execute({
            "project_name": arch_input.project_name,
            "project_type": arch_input.project_type,
            "requirements": arch_input.requirements,
            "constraints": arch_input.constraints,
            "quality_attributes": arch_input.quality_attributes,
        })

        if not result.success:
            raise RuntimeError(f"Design tool failed: {result.error}")

        design_data = result.result

        return ArchitectureOutput(
            architecture_type=DesignPattern(design_data["architecture_type"]),
            component_diagram=design_data["component_diagram"],
            technology_stack=design_data["technology_stack"],
            deployment_model=design_data["deployment_model"],
            tradeoffs=design_data["tradeoffs"],
            risks=design_data["risks"],
            estimated_complexity=design_data["estimated_complexity"],
            recommended_iterations=design_data["recommended_iterations"],
        )

    def _format_output(self, design: ArchitectureOutput) -> Dict[str, Any]:
        """Format design output based on configured format."""
        base_output = {
            "project": {
                "architecture_type": design.architecture_type.value,
                "estimated_complexity": design.estimated_complexity,
                "recommended_iterations": design.recommended_iterations,
            },
            "components": design.component_diagram,
            "technology_stack": design.technology_stack,
            "deployment": design.deployment_model,
            "tradeoffs": design.tradeoffs,
            "risks": design.risks,
        }

        if self._output_format == "markdown":
            return {
                "format": "markdown",
                "content": self._to_markdown(design),
            }

        return {
            "format": "json",
            "content": base_output,
        }

    def _to_markdown(self, design: ArchitectureOutput) -> str:
        """Convert design to markdown format."""
        lines = [
            f"# {design.architecture_type.value.title()} Architecture",
            "",
            f"**Complexity**: {design.estimated_complexity}",
            f"**Recommended Iterations**: {design.recommended_iterations}",
            "",
            "## Components",
            "",
        ]

        for comp in design.component_diagram.get("components", []):
            lines.append(f"### {comp['name']}")
            lines.append(f"- **Type**: {comp['type']}")
            lines.append(f"- **Responsibility**: {comp['responsibility']}")
            if comp.get("dependencies"):
                lines.append(
                    f"- **Dependencies**: {', '.join(comp['dependencies'])}")
            lines.append("")

        lines.extend([
            "## Technology Stack",
            "",
        ])
        for category, techs in design.technology_stack.items():
            lines.append(f"- **{category}**: {', '.join(techs)}")

        lines.extend([
            "",
            "## Tradeoffs",
            "",
        ])
        for tradeoff in design.tradeoffs:
            lines.append(f"### {tradeoff['decision']}")
            lines.append(f"- **Pros**: {tradeoff['pros']}")
            lines.append(f"- **Cons**: {tradeoff['cons']}")
            lines.append(f"- **Selected For**: {tradeoff['selected_for']}")
            lines.append("")

        return "\n".join(lines)


async def demo() -> None:
    """
    Demonstration of the Architect Agent.

    This function shows:
    1. Agent initialization
    2. Task creation and submission
    3. Result processing
    """
    import logging

    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s | %(name)-30s | %(levelname)-8s | %(message)s",
    )

    print("=" * 60)
    print("Architect Agent Demonstration")
    print("=" * 60)

    agent = ArchitectAgent(
        agent_id="architect-demo-001",
        manager={
            "verbose": True,
            "output_format": "json",
        },
    )

    print("\n[1] Initializing agent...")
    await agent.initialize()
    print("[1] Agent initialized successfully")

    context = AgentContext(
        task_id="task-demo-001",
        trace_id="trace-001",
        session_id="session-demo",
    )

    input_data = {
        "project_name": "Smart Inventory Management System",
        "project_type": "web_app",
        "requirements": [
            "Real-time inventory tracking",
            "Multi-warehouse support",
            "Barcode scanning integration",
            "Automated reorder triggers",
            "Supplier management",
            "Reporting and analytics dashboard",
            "Role-based access control",
            "Mobile responsive UI",
        ],
        "constraints": {
            "budget": "medium",
            "timeline_weeks": 16,
            "team_size": 6,
        },
        "quality_attributes": {
            "scalability": "high",
            "performance": "high",
            "security": "high",
            "maintainability": "medium",
        },
    }

    print("\n[2] Executing architecture design task...")
    print(f"    Project: {input_data['project_name']}")
    print(f"    Requirements: {len(input_data['requirements'])} items")

    result = await agent.execute(context, input_data)

    print("\n[3] Execution Result:")
    print(f"    Success: {result.success}")

    if result.success:
        output = result.output
        print(f"    Format: {output['format']}")
        print(f"    Components: {result.metrics.get('num_components', 'N/A')}")
        print(f"    Risks: {result.metrics.get('num_risks', 'N/A')}")
        print(
            f"    Execution Time: {result.metrics.get('execution_time_ms', 0):.2f}ms")

        if output["format"] == "markdown":
            print("\n" + output["content"][:1000] + "...")
        else:
            import json
            print(
                "\n" + json.dumps(output["content"], indent=2)[:1000] + "...")
    else:
        print(f"    Error: {result.error}")
        print(f"    Error Code: {result.error_code}")

    print("\n[4] Shutting down agent...")
    await agent.shutdown()
    print("[4] Agent shutdown complete")

    print("\n" + "=" * 60)
    print("Demonstration Complete")
    print("=" * 60)


if __name__ == "__main__":
    asyncio.run(demo())
