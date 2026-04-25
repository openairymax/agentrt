# AgentOS Python SDK - Client Layer
# Version: 3.0.0
# Last updated: 2026-03-24

"""
Client layer providing HTTP communication with AgentOS server.

This module implements the APIClient interface pattern from Go SDK,
providing both sync and async client implementations.

Architecture:
    client/
    ├── __init__.py      # Public API exports
    ├── client.py        # HTTP client implementation
    └── mock.py          # Mock client for testing
"""

from .client import (
    Client,
    APIClient,
    ClientConfig,
    RequestOptions,
    APIResponse,
    HealthStatus,
    Metrics,
)
from .mock import MockClient

__all__ = [
    "Client",
    "APIClient",
    "ClientConfig",
    "RequestOptions",
    "APIResponse",
    "HealthStatus",
    "Metrics",
    "MockClient",
]
