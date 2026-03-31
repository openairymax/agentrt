"""
Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

openlab.utils.exceptions - Custom exception classes
===================================================

Provides a hierarchy of custom exceptions for openlab.

Base Exception Hierarchy:
    OpenLabError
    ├── AgentError
    │   ├── AgentInitializationError
    │   ├── AgentExecutionError
    │   └── AgentNotFoundError
    ├── TaskError
    │   ├── TaskCreationError
    │   ├── TaskExecutionError
    │   └── TaskNotFoundError
    ├── ToolError
    │   ├── ToolInitializationError
    │   ├── ToolExecutionError
    │   └── ToolNotFoundError
    ├── StorageError
    │   ├── StorageConnectionError
    │   ├── StorageReadError
    │   └── StorageWriteError
    └── ValidationError
        ├── InputValidationError
        └── ConfigurationError

Example:
    from openlab.utils.exceptions import AgentError, AgentExecutionError

    try:
        await agent.execute(context, input_data)
    except AgentExecutionError as e:
        logger.error(f"Agent execution failed: {e}")
    except AgentError as e:
        logger.error(f"Agent error: {e}")

Author: AgentOS Architecture Committee
Version: 1.0.0.6
"""


class OpenLabError(Exception):
    """Base exception for all openlab errors.

    All custom exceptions in openlab should inherit from this class
    to enable catching all openlab-specific errors with a single except clause.

    Example:
        try:
            # openlab code
            pass
        except OpenLabError as e:
            # Handle any openlab error
            logger.error(f"OpenLab error: {e}")
    """

    def __init__(self, message: str = "", cause: Exception = None):
        super().__init__(message)
        self.message = message
        self.cause = cause

    def __str__(self) -> str:
        if self.cause:
            return f"{self.message} (caused by: {self.cause})"
        return self.message


class AgentError(OpenLabError):
    """Base exception for agent-related errors.

    Raised when errors occur during agent lifecycle operations
    including initialization, execution, and management.

    Example:
        from openlab.utils.exceptions import AgentError

        raise AgentError("Agent failed to initialize", cause=e)
    """


class AgentInitializationError(AgentError):
    """Raised when agent initialization fails.

    This includes failures during agent creation, capability loading,
    and resource allocation.

    Example:
        raise AgentInitializationError(
            f"Failed to initialize agent {agent_id}",
            cause=e
        )
    """


class AgentExecutionError(AgentError):
    """Raised when agent execution fails.

    This includes failures during task processing, planning,
    and result generation.

    Example:
        raise AgentExecutionError(
            f"Agent {agent_id} failed to process task {task_id}",
            cause=e
        )
    """


class AgentNotFoundError(AgentError):
    """Raised when a requested agent cannot be found.

    Example:
        raise AgentNotFoundError(f"Agent {agent_id} not found in registry")
    """


class TaskError(OpenLabError):
    """Base exception for task-related errors.

    Raised when errors occur during task lifecycle operations
    including creation, scheduling, and execution.

    Example:
        from openlab.utils.exceptions import TaskError

        raise TaskError("Task creation failed", cause=e)
    """


class TaskCreationError(TaskError):
    """Raised when task creation fails.

    This includes invalid task definitions, missing required fields,
    and validation failures.

    Example:
        raise TaskCreationError(
            f"Invalid task definition: {definition}",
            cause=e
        )
    """


class TaskExecutionError(TaskError):
    """Raised when task execution fails.

    This includes runtime errors, handler failures, and timeout errors.

    Example:
        raise TaskExecutionError(
            f"Task {task_id} execution failed",
            cause=e
        )
    """


class TaskNotFoundError(TaskError):
    """Raised when a requested task cannot be found.

    Example:
        raise TaskNotFoundError(f"Task {task_id} not found")
    """


class ToolError(OpenLabError):
    """Base exception for tool-related errors.

    Raised when errors occur during tool lifecycle operations
    including registration, invocation, and result processing.

    Example:
        from openlab.utils.exceptions import ToolError

        raise ToolError("Tool invocation failed", cause=e)
    """


class ToolInitializationError(ToolError):
    """Raised when tool initialization fails.

    Example:
        raise ToolInitializationError(
            f"Failed to initialize tool {tool_name}",
            cause=e
        )
    """


class ToolExecutionError(ToolError):
    """Raised when tool execution fails.

    Example:
        raise ToolExecutionError(
            f"Tool {tool_name} execution failed",
            cause=e
        )
    """


class ToolNotFoundError(ToolError):
    """Raised when a requested tool cannot be found.

    Example:
        raise ToolNotFoundError(f"Tool {tool_name} not found in registry")
    """


class StorageError(OpenLabError):
    """Base exception for storage-related errors.

    Raised when errors occur during storage operations
    including read, write, and query operations.

    Example:
        from openlab.utils.exceptions import StorageError

        raise StorageError("Storage read failed", cause=e)
    """


class StorageConnectionError(StorageError):
    """Raised when storage connection fails.

    Example:
        raise StorageConnectionError(
            f"Failed to connect to storage at {uri}",
            cause=e
        )
    """


class StorageReadError(StorageError):
    """Raised when storage read operation fails.

    Example:
        raise StorageReadError(
            f"Failed to read record {record_id}",
            cause=e
        )
    """


class StorageWriteError(StorageError):
    """Raised when storage write operation fails.

    Example:
        raise StorageWriteError(
            f"Failed to write record {record_id}",
            cause=e
        )
    """


class ValidationError(OpenLabError):
    """Base exception for validation errors.

    Raised when input or configuration validation fails.

    Example:
        from openlab.utils.exceptions import ValidationError

        raise ValidationError("Input validation failed", cause=e)
    """


class InputValidationError(ValidationError):
    """Raised when input validation fails.

    Example:
        raise InputValidationError(
            f"Invalid input for field '{field}': {value}",
            cause=e
        )
    """


class ConfigurationError(ValidationError):
    """Raised when configuration validation fails.

    Example:
        raise ConfigurationError(
            f"Invalid configuration for '{key}': {value}",
            cause=e
        )
    """
