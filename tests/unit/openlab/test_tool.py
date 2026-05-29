# Copyright (c) 2026 SPHARX. All Rights Reserved.
# "From data intelligence emerges."

"""
Unit Tests for Core Tool Module
================================

Tests for openlab.core.tool module.
These tests verify the Tool, ToolRegistry, ToolExecutor, and related classes.
"""

import pytest
import asyncio
from typing import Dict, Any
import sys
import os
from unittest.mock import AsyncMock, MagicMock

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))


class TestToolCategory:
    """Tests for ToolCategory enum."""

    def test_category_values(self):
        """Test ToolCategory enum has correct values."""
        from openlab.core.tool import ToolCategory

        assert ToolCategory.INPUT_OUTPUT.value == "input_output"
        assert ToolCategory.COMPUTATION.value == "computation"
        assert ToolCategory.COMMUNICATION.value == "communication"
        assert ToolCategory.DATA_ACCESS.value == "data_access"
        assert ToolCategory.SYSTEM.value == "system"
        assert ToolCategory.CUSTOM.value == "custom"


class TestToolCapability:
    """Tests for ToolCapability enum."""

    def test_capability_values(self):
        """Test ToolCapability enum has correct values."""
        from openlab.core.tool import ToolCapability

        assert ToolCapability.READ.value == "read"
        assert ToolCapability.WRITE.value == "write"
        assert ToolCapability.EXECUTE.value == "execute"
        assert ToolCapability.QUERY.value == "query"
        assert ToolCapability.TRANSFORM.value == "transform"
        assert ToolCapability.ANALYZE.value == "analyze"


class TestToolContext:
    """Tests for ToolContext dataclass."""

    def test_context_creation(self):
        """Test ToolContext can be created."""
        from openlab.core.tool import ToolContext

        context = ToolContext(tool_id="tool-001")
        
        assert context.tool_id == "tool-001"
        assert context.agent_id is None
        assert context.task_id is None
        assert context.timeout is None
        assert isinstance(context.metadata, dict)

    def test_context_with_all_params(self):
        """Test ToolContext with all parameters."""
        from openlab.core.tool import ToolContext

        context = ToolContext(
            tool_id="tool-002",
            agent_id="agent-001",
            task_id="task-001",
            session_id="session-001",
            timeout=60.0,
            metadata={"key": "value"},
        )
        
        assert context.agent_id == "agent-001"
        assert context.task_id == "task-001"
        assert context.session_id == "session-001"
        assert context.timeout == 60.0
        assert context.metadata["key"] == "value"


class TestToolResult:
    """Tests for ToolResult dataclass."""

    def test_success_result(self):
        """Test successful result creation."""
        from openlab.core.tool import ToolResult

        result = ToolResult(success=True, output={"data": "test"})
        
        assert result.success is True
        assert result.output == {"data": "test"}
        assert result.error is None
        assert result.execution_time == 0.0

    def test_failure_result(self):
        """Test failed result creation."""
        from openlab.core.tool import ToolResult

        result = ToolResult(
            success=False,
            error="Operation failed",
            error_code="OP_FAILED",
            execution_time=1.5,
        )
        
        assert result.success is False
        assert result.error == "Operation failed"
        assert result.error_code == "OP_FAILED"
        assert result.execution_time == 1.5

    def test_result_with_warnings(self):
        """Test result with warnings."""
        from openlab.core.tool import ToolResult

        result = ToolResult(
            success=True,
            output="done",
            warnings=["Warning 1", "Warning 2"],
        )
        
        assert len(result.warnings) == 2
        assert "Warning 1" in result.warnings


class SimpleTestTool:
    """Simple concrete implementation of Tool for testing."""

    NAME = "test_tool"
    DESCRIPTION = "A test tool"
    CATEGORY = None
    CAPABILITIES = set()
    INPUT_SCHEMA = {
        "type": "object",
        "properties": {"value": {"type": "string"}},
        "required": ["value"],
    }

    def __init__(self, tool_id=None):
        self.tool_id = tool_id or f"{self.NAME}_{id(self)}"
        self._enabled = True
        self._last_used = None
        self._usage_count = 0

    async def _do_execute(self, parameters: Dict[str, Any], context) -> Any:
        return ToolResult(success=True, output=f"Processed: {parameters['value']}")


class TestTool:
    """Tests for Tool base class functionality."""

    @pytest.fixture
    def tool(self):
        """Create a test tool."""
        return SimpleTestTool()

    def test_tool_creation(self, tool):
        """Test tool can be created."""
        assert tool.NAME == "test_tool"
        assert tool.DESCRIPTION == "A test tool"
        assert tool.enabled is True
        assert tool.usage_count == 0

    @pytest.mark.asyncio
    async def test_tool_execute_success(self, tool):
        """Test successful tool execution."""
        from openlab.core.tool import ToolContext

        context = ToolContext(tool_id=tool.tool_id)
        result = await tool.execute({"value": "test_data"}, context)

        assert result.success is True
        assert "Processed" in result.output
        assert result.execution_time > 0
        assert tool.usage_count == 1

    @pytest.mark.asyncio
    async def test_tool_execute_disabled(self, tool):
        """Test disabled tool returns error."""
        tool.enabled = False
        
        result = await tool.execute({"value": "test"}, None)

        assert result.success is False
        assert result.error_code == "TOOL_DISABLED"

    @pytest.mark.asyncio
    async def test_tool_invalid_input(self, tool):
        """Test invalid input validation."""
        # Missing required field
        result = await tool.execute({}, None)

        assert result.success is False
        assert result.error_code == "INVALID_INPUT"

    def test_get_info(self, tool):
        """Test getting tool info."""
        info = tool.get_info()

        assert info["name"] == "test_tool"
        assert info["enabled"] is True
        assert info["usage_count"] == 0


class TestToolRegistry:
    """Tests for ToolRegistry class."""

    @pytest.fixture
    def registry(self):
        """Create a test registry."""
        from openlab.core.tool import ToolRegistry
        return ToolRegistry()

    @pytest.mark.asyncio
    async def test_register_tool(self, registry):
        """Test registering a tool."""
        tool = SimpleTestTool()
        
        success = await registry.register(tool)
        
        assert success is True

    @pytest.mark.asyncio
    async def test_register_duplicate(self, registry):
        """Test registering duplicate tool fails."""
        tool = SimpleTestTool(tool_id="dup-tool")
        
        await registry.register(tool)
        success = await registry.register(tool)
        
        assert success is False

    @pytest.mark.asyncio
    async def test_unregister_tool(self, registry):
        """Test unregistering a tool."""
        tool = SimpleTestTool()
        
        await registry.register(tool)
        success = await registry.unregister(tool.tool_id)
        
        assert success is True

    @pytest.mark.asyncio
    async def test_get_tool(self, registry):
        """Test getting a tool by ID."""
        tool = SimpleTestTool(tool_id="get-test")
        
        await registry.register(tool)
        retrieved = await registry.get("get-test")
        
        assert retrieved is not None
        assert retrieved.tool_id == "get-test"

    @pytest.mark.asyncio
    async def test_list_tools(self, registry):
        """Test listing all tools."""
        tool1 = SimpleTestTool(tool_id="list-1")
        tool2 = SimpleTestTool(tool_id="list-2")
        
        await registry.register(tool1)
        await registry.register(tool2)
        
        tools = await registry.list_tools()
        
        assert len(tools) == 2

    @pytest.mark.asyncio
    async def test_find_by_category(self, registry):
        """Test finding tools by category (empty category)."""
        tools = await registry.find_by_category(ToolCategory.COMPUTATION)
        
        assert isinstance(tools, list)

    @pytest.mark.asyncio
    async def test_find_by_capability(self, registry):
        """Test finding tools by capability."""
        tools = await registry.find_by_capability(ToolCapability.READ)
        
        assert isinstance(tools, list)


class TestToolExecutor:
    """Tests for ToolExecutor class."""

    @pytest.fixture
    def executor(self):
        """Create a test executor."""
        from openlab.core.tool import ToolExecutor
        return ToolExecutor(max_concurrent=5, default_timeout=30.0)

    def test_executor_creation(self, executor):
        """Test executor can be created."""
        stats = executor.get_stats()
        
        assert stats["max_concurrent"] == 5
        assert stats["default_timeout"] == 30.0
        assert stats["registered_tools"] == 0

    @pytest.mark.asyncio
    async def test_register_and_execute(self, executor):
        """Test registering and executing a tool."""
        tool = SimpleTestTool(tool_id="exec-test")
        
        await executor.register_tool(tool)
        result = await executor.execute("exec-test", {"value": "hello"})
        
        assert result.success is True
        assert "Processed" in result.output

    @pytest.mark.asyncio
    async def test_execute_nonexistent_tool(self, executor):
        """Test executing non-existent tool returns error."""
        result = await executor.execute("nonexistent", {})
        
        assert result.success is False
        assert result.error_code == "TOOL_NOT_FOUND"

    @pytest.mark.asyncio
    async def test_shutdown_executor(self, executor):
        """Test shutting down executor."""
        await executor.shutdown(wait=False)
        
        assert executor._shutdown is True


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
