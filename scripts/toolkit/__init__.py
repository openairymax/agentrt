#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS Toolkit - Unified Python tools for AgentOS

"""
AgentOS Toolkit Module

Consolidated Python utilities for AgentOS, including:
- Initializer: Environment setup and configuration generation
- Doctor: System health diagnostics
- MemoryManager: Memory layer management
- CheckpointManager: State persistence
- TokenUtils: Token counting and budget management
- Benchmark: Performance measurement
- ValidateContracts: Interface contract validation
- ConfigEngine: Jinja2-based template rendering

Usage:
    from scripts.toolkit import (
        ConfigInitializer,
        AgentOSDoctor,
        MemoryManager,
        TokenCounter,
        TokenBudget,
    )
"""

from .initializer import ConfigInitializer
from .doctor import AgentOSDoctor
from .memory_manager import MemoryManager
from .checkpoint_manager import CheckpointManager
from .token_utils import TokenCounter, TokenBudget, get_token_counter, get_token_budget
from .benchmark import AgentOSBenchmark, BenchmarkReporter
from .validate_contracts import ContractValidator
from .config_engine import ConfigEngine, Environment, create_default_engine

__version__ = "1.0.0"
__author__ = "SPHARX Ltd."

__all__ = [
    "ConfigInitializer",
    "AgentOSDoctor",
    "MemoryManager",
    "CheckpointManager",
    "TokenCounter",
    "TokenBudget",
    "get_token_counter",
    "get_token_budget",
    "AgentOSBenchmark",
    "BenchmarkReporter",
    "ContractValidator",
    "ConfigEngine",
    "Environment",
    "create_default_engine",
]
