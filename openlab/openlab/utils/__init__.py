"""
Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

openlab.utils - Common utilities for openlab
============================================

Provides common utilities including logging configuration and custom exceptions.

Example:
    from openlab.utils import setup_logger, OpenLabError

    logger = setup_logger(__name__)
    logger.info("message")

Available Modules:
    logging: Logging configuration utilities
    exceptions: Custom exception classes

Author: AgentOS Architecture Committee
Version: 1.0.0.6
"""

from openlab.utils.logging import setup_logger, get_logger
from openlab.utils.exceptions import (
    OpenLabError,
    AgentError,
    TaskError,
    ToolError,
    StorageError,
    ValidationError,
)

__all__ = [
    # Logging
    "setup_logger",
    "get_logger",
    # Exceptions
    "OpenLabError",
    "AgentError",
    "TaskError",
    "ToolError",
    "StorageError",
    "ValidationError",
]
