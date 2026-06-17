# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
"""
AgentRT Plugin Type Base Classes

Defines the four foundational plugin types that form the basis of
AgentRT's extensibility model. Each type provides a specialized
interface for a specific extension domain.

Plugin Types:
  - AgentPlugin:  Extends agent capabilities (cognition, planning, execution)
  - ToolPlugin:   Provides custom tool implementations
  - HookPlugin:   Intercepts lifecycle events with custom hooks
  - SkillPlugin:  Registers reusable skill definitions

All types inherit from BasePlugin (framework/plugin.py) and add
domain-specific interfaces.

Usage:
    from agentos.plugin_types import AgentPlugin, ToolPlugin, HookPlugin, SkillPlugin

    class MyAgent(AgentPlugin):
        async def on_cognition(self, context):
            # Custom cognition logic
            pass
"""

from __future__ import annotations

from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional

from agentos.framework.plugin import BasePlugin


# ─── Agent Plugin ────────────────────────────────────────────────


class AgentPlugin(BasePlugin):
    """Base class for Agent-type plugins.

    Agent plugins extend the core agent with custom cognition,
    planning, execution, or post-processing logic. They integrate
    into the CoreLoopThree lifecycle.

    Lifecycle hooks (called by CoreLoopThree):
      - on_cognition: Before the cognition phase
      - on_planning: Before task planning
      - on_execution: Before tool execution
      - on_reflection: After receiving results
    """

    PLUGIN_TYPE = "agent"

    async def on_cognition(
        self, context: Dict[str, Any]
    ) -> Dict[str, Any]:
        """Called before the agent performs cognition.

        Override to inject custom reasoning, modify the prompt,
        or preprocess input before the LLM call.

        Args:
            context: Contains 'messages', 'system_prompt', 'model', etc.

        Returns:
            Modified context dict. Return unchanged context to skip.
        """
        return context

    async def on_planning(
        self, context: Dict[str, Any]
    ) -> Dict[str, Any]:
        """Called before task planning / decomposition.

        Override to customize how tasks are broken down into subtasks.

        Args:
            context: Contains 'task', 'plan', 'available_tools', etc.

        Returns:
            Modified context or plan dict.
        """
        return context

    async def on_execution(
        self, context: Dict[str, Any]
    ) -> Dict[str, Any]:
        """Called before tool execution.

        Override to intercept, modify, or approve tool calls.

        Args:
            context: Contains 'tool_name', 'tool_input', 'agent_id', etc.

        Returns:
            Context with optional 'approved' (bool) and 'modified_input'.
        """
        return context

    async def on_reflection(
        self, context: Dict[str, Any]
    ) -> Dict[str, Any]:
        """Called after receiving tool results or LLM response.

        Override to perform post-processing, quality checks,
        or trigger follow-up actions.

        Args:
            context: Contains 'result', 'tool_name', 'llm_response', etc.

        Returns:
            Context with optional 'feedback', 'should_retry', etc.
        """
        return context

    def get_agent_role(self) -> str:
        """Return the agent's specialized role description."""
        return self.__class__.__doc__ or "Custom Agent"

    def get_required_tools(self) -> List[str]:
        """Return list of tool names this agent requires."""
        return []


# ─── Tool Plugin ─────────────────────────────────────────────────


@dataclass
class ToolParameter:
    """Definition of a tool parameter."""
    name: str
    type: str = "string"
    description: str = ""
    required: bool = False
    default: Any = None
    enum: Optional[List[str]] = None


@dataclass
class ToolMetadata:
    """Metadata describing a tool for registration and discovery."""
    name: str
    description: str = ""
    version: str = "0.1.0"
    parameters: List[ToolParameter] = field(default_factory=list)
    returns: str = "any"
    category: str = "general"
    tags: List[str] = field(default_factory=list)
    is_async: bool = True
    timeout_seconds: float = 30.0
    requires_confirmation: bool = False

    def to_dict(self) -> Dict[str, Any]:
        return {
            "name": self.name,
            "description": self.description,
            "version": self.version,
            "parameters": [
                {
                    "name": p.name,
                    "type": p.type,
                    "description": p.description,
                    "required": p.required,
                    "default": p.default,
                }
                for p in self.parameters
            ],
            "returns": self.returns,
            "category": self.category,
            "tags": self.tags,
        }


class ToolPlugin(BasePlugin):
    """Base class for Tool-type plugins.

    Tool plugins register custom tools that agents can invoke during
    execution. Each tool has defined parameters, a return type, and
    an execute method.

    Example:
        class WeatherTool(ToolPlugin):
            def get_metadata(self):
                return ToolMetadata(
                    name="get_weather",
                    description="Get current weather for a city",
                    parameters=[
                        ToolParameter(name="city", type="string",
                                      description="City name", required=True),
                    ],
                )

            async def execute(self, params):
                return {"temperature": 22, "condition": "sunny"}
    """

    PLUGIN_TYPE = "tool"

    @abstractmethod
    def get_metadata(self) -> ToolMetadata:
        """Return tool metadata for registration.

        Must be implemented by all tool plugins.
        """
        ...

    @abstractmethod
    async def execute(self, params: Dict[str, Any]) -> Any:
        """Execute the tool with given parameters.

        This is the main entry point called by the tool executor.

        Args:
            params: Dictionary of parameter name → value.

        Returns:
            Tool execution result (any JSON-serializable value).
        """
        ...

    async def validate_params(
        self, params: Dict[str, Any]
    ) -> Dict[str, Any]:
        """Validate and coerce input parameters.

        Override to add custom validation logic.

        Args:
            params: Raw input parameters.

        Returns:
            Validated parameters dict.

        Raises:
            ValueError: If required parameters are missing or invalid.
        """
        meta = self.get_metadata()
        validated = {}
        errors = []

        for p in meta.parameters:
            if p.name in params:
                validated[p.name] = params[p.name]
            elif p.required:
                errors.append(f"Missing required parameter: {p.name}")
            elif p.default is not None:
                validated[p.name] = p.default

        if errors:
            raise ValueError("; ".join(errors))

        return validated

    async def on_error(self, error: Exception, params: Dict[str, Any]) -> Dict[str, Any]:
        """Handle tool execution errors.

        Override to provide graceful error recovery.

        Args:
            error: The exception that occurred.
            params: Original parameters.

        Returns:
            Error response dict with 'error' key.
        """
        return {
            "error": str(error),
            "error_type": type(error).__name__,
        }


# ─── Hook Plugin ─────────────────────────────────────────────────


class HookPlugin(BasePlugin, ABC):
    """Base class for Hook-type plugins.

    Hook plugins intercept agent lifecycle events (defined in
    ecosystem/hooks/__init__.py) and can modify or block operations.

    Implement on_<event> methods for the events you want to handle.

    Example:
        class LoggingHook(HookPlugin):
            def on_tool_call(self, ctx, tool_name, tool_input):
                print(f"Tool called: {tool_name}")
                return HookResult(allowed=True)

            def on_llm_response(self, ctx, response, usage):
                print(f"LLM response: {usage} tokens")
                return HookResult(allowed=True)
    """

    PLUGIN_TYPE = "hook"

    def get_events(self) -> List[str]:
        """Return the list of events this hook listens to.

        Override to declare which events this hook handles.
        Default: all on_* methods that are overridden.
        """
        events = []
        for name in dir(self):
            if name.startswith("on_") and callable(getattr(self, name)):
                # Check if method is overridden from base class
                method = getattr(self.__class__, name, None)
                base_method = getattr(HookPlugin, name, None)
                if method is not base_method:
                    events.append(name)
        return events

    # ── Agent lifecycle ──────────────────────────────

    def on_agent_start(self, ctx: Any, data: Any = None) -> Any:
        """Called when an agent starts execution."""
        return None

    def on_agent_end(self, ctx: Any, data: Any = None) -> Any:
        """Called when an agent finishes execution."""
        return None

    # ── Tool events ──────────────────────────────────

    def on_tool_call(self, ctx: Any, tool_name: str = "", tool_input: Any = None) -> Any:
        """Called before a tool is invoked."""
        return None

    def on_tool_result(self, ctx: Any, tool_name: str = "", result: Any = None) -> Any:
        """Called after a tool returns a result."""
        return None

    # ── LLM events ───────────────────────────────────

    def on_llm_request(self, ctx: Any, messages: Any = None, model: str = "") -> Any:
        """Called before sending a request to LLM."""
        return None

    def on_llm_response(self, ctx: Any, response: Any = None, usage: Optional[Dict] = None) -> Any:
        """Called after receiving a response from LLM."""
        return None

    # ── Memory events ────────────────────────────────

    def on_memory_read(self, ctx: Any, key: str = "", layer: str = "") -> Any:
        """Called when reading from memory."""
        return None

    def on_memory_write(self, ctx: Any, key: str = "", data: Any = None) -> Any:
        """Called when writing to memory."""
        return None


# ─── Skill Plugin ────────────────────────────────────────────────


@dataclass
class SkillDefinition:
    """Definition of a skill for registration and discovery."""
    name: str
    description: str = ""
    version: str = "0.1.0"
    category: str = "general"
    tags: List[str] = field(default_factory=list)
    input_schema: Dict[str, Any] = field(default_factory=dict)
    output_schema: Dict[str, Any] = field(default_factory=dict)
    examples: List[Dict[str, str]] = field(default_factory=list)
    requires: List[str] = field(default_factory=list)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "name": self.name,
            "description": self.description,
            "version": self.version,
            "category": self.category,
            "tags": self.tags,
            "input_schema": self.input_schema,
            "output_schema": self.output_schema,
            "examples": self.examples,
            "requires": self.requires,
        }


class SkillPlugin(BasePlugin):
    """Base class for Skill-type plugins.

    Skill plugins register reusable skill definitions that agents
    can activate and apply to their tasks. Skills include a prompt
    template, execution logic, and metadata.

    Example:
        class CodeReviewSkill(SkillPlugin):
            def get_definition(self):
                return SkillDefinition(
                    name="code_review",
                    description="Review code for issues",
                    category="development",
                )

            async def execute(self, context):
                # Perform code review
                return {"issues": [], "suggestions": []}
    """

    PLUGIN_TYPE = "skill"

    @abstractmethod
    def get_definition(self) -> SkillDefinition:
        """Return the skill definition.

        Must be implemented by all skill plugins.
        """
        ...

    @abstractmethod
    async def execute(self, context: Dict[str, Any]) -> Any:
        """Execute the skill with given context.

        Args:
            context: Contains input data, agent state, session info.

        Returns:
            Skill execution result.
        """
        ...

    def get_prompt_template(self) -> Optional[str]:
        """Return the skill's prompt template.

        Override to provide a custom prompt that guides the LLM
        when this skill is activated. The template may contain
        {variable} placeholders.
        """
        return None

    def get_system_instructions(self) -> Optional[str]:
        """Return additional system instructions for this skill."""
        return None

    async def pre_execute(self, context: Dict[str, Any]) -> Dict[str, Any]:
        """Called before execution. Override for setup/validation."""
        return context

    async def post_execute(
        self, context: Dict[str, Any], result: Any
    ) -> Any:
        """Called after execution. Override for post-processing."""
        return result

    def validate_input(self, context: Dict[str, Any]) -> bool:
        """Validate input context against input_schema."""
        return True

    def get_examples(self) -> List[Dict[str, str]]:
        """Return usage examples for documentation."""
        return self.get_definition().examples


# ─── Plugin Type Registry ────────────────────────────────────────

# Maps plugin type strings to their base classes
PLUGIN_TYPE_MAP: Dict[str, type] = {
    "agent": AgentPlugin,
    "tool": ToolPlugin,
    "hook": HookPlugin,
    "skill": SkillPlugin,
}


def get_plugin_base_class(plugin_type: str) -> Optional[type]:
    """Get the base class for a given plugin type.

    Args:
        plugin_type: One of 'agent', 'tool', 'hook', 'skill'.

    Returns:
        The corresponding base class, or None if unknown.
    """
    return PLUGIN_TYPE_MAP.get(plugin_type)


__all__ = [
    "AgentPlugin",
    "ToolPlugin",
    "ToolParameter",
    "ToolMetadata",
    "HookPlugin",
    "SkillPlugin",
    "SkillDefinition",
    "PLUGIN_TYPE_MAP",
    "get_plugin_base_class",
]