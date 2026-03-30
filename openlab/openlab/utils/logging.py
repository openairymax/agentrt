"""
Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

openlab.utils.logging - Logging configuration utilities
=====================================================

Provides centralized logging configuration for openlab modules.

Example:
    from openlab.utils.logging import setup_logger

    logger = setup_logger(__name__)
    logger.info("message")

Author: AgentOS Architecture Committee
Version: 1.0.0.6
"""

import logging
import sys
from typing import Optional, Dict, Any
from pathlib import Path


DEFAULT_FORMAT = "%(asctime)s - %(name)s - %(levelname)s - %(message)s"
DEFAULT_DATE_FORMAT = "%Y-%m-%d %H:%M:%S"


_loggers: Dict[str, logging.Logger] = {}


def setup_logger(
    name: str,
    level: int = logging.INFO,
    log_file: Optional[str] = None,
    format_string: Optional[str] = None,
    date_format: Optional[str] = None,
) -> logging.Logger:
    """Set up a logger with consistent configuration.

    Args:
        name: Logger name, typically __name__
        level: Logging level (default: logging.INFO)
        log_file: Optional log file path
        format_string: Optional custom format string
        date_format: Optional custom date format

    Returns:
        Configured logger instance

    Example:
        logger = setup_logger(__name__, level=logging.DEBUG)
    """
    if name in _loggers:
        return _loggers[name]

    logger = logging.getLogger(name)
    logger.setLevel(level)
    logger.propagate = False

    if logger.handlers:
        for handler in logger.handlers[:]:
            logger.removeHandler(handler)

    fmt = format_string or DEFAULT_FORMAT
    date_fmt = date_format or DEFAULT_DATE_FORMAT
    formatter = logging.Formatter(fmt, date_fmt)

    console_handler = logging.StreamHandler(sys.stdout)
    console_handler.setLevel(level)
    console_handler.setFormatter(formatter)
    logger.addHandler(console_handler)

    if log_file:
        log_path = Path(log_file)
        log_path.parent.mkdir(parents=True, exist_ok=True)
        file_handler = logging.FileHandler(log_file, encoding="utf-8")
        file_handler.setLevel(level)
        file_handler.setFormatter(formatter)
        logger.addHandler(file_handler)

    _loggers[name] = logger
    return logger


def get_logger(name: str) -> logging.Logger:
    """Get an existing logger or create a new one with defaults.

    Args:
        name: Logger name, typically __name__

    Returns:
        Logger instance
    """
    if name in _loggers:
        return _loggers[name]
    return setup_logger(name)


def set_log_level(name: str, level: int) -> bool:
    """Set log level for an existing logger.

    Args:
        name: Logger name
        level: New logging level

    Returns:
        True if logger was found and updated, False otherwise
    """
    if name in _loggers:
        logger = _loggers[name]
        logger.setLevel(level)
        for handler in logger.handlers:
            handler.setLevel(level)
        return True
    return False


def add_file_handler(
    name: str,
    log_file: str,
    level: Optional[int] = None,
    format_string: Optional[str] = None,
) -> bool:
    """Add a file handler to an existing logger.

    Args:
        name: Logger name
        log_file: Log file path
        level: Optional logging level for this handler
        format_string: Optional custom format string

    Returns:
        True if logger was found and handler added, False otherwise
    """
    if name not in _loggers:
        return False

    logger = _loggers[name]
    log_path = Path(log_file)
    log_path.parent.mkdir(parents=True, exist_ok=True)

    fmt = format_string or DEFAULT_FORMAT
    formatter = logging.Formatter(fmt)

    file_handler = logging.FileHandler(log_file, encoding="utf-8")
    if level is not None:
        file_handler.setLevel(level)
    file_handler.setFormatter(formatter)
    logger.addHandler(file_handler)
    return True
