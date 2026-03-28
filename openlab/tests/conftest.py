# Copyright (c) 2026 SPHARX. All Rights Reserved.
# "From data intelligence emerges."

"""
Pytest Configuration and Shared Fixtures
======================================

This module provides shared pytest fixtures for the OpenLab test suite.
"""

import pytest
import sys
import os
from typing import Generator, Dict, Any

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


@pytest.fixture
def mock_config() -> Dict[str, Any]:
    """Provide a mock configuration for testing."""
    return {
        "database_url": "sqlite:///:memory:",
        "redis_url": "redis://localhost:6379",
        "jwt_secret_key": "test-secret-key",
        "jwt_algorithm": "HS256",
        "jwt_access_token_expire_minutes": 30,
        "cors_origins": ["http://localhost:3000"],
        "debug": True,
        "security": {
            "bcrypt_rounds": 4,
            "rate_limit_requests": 100,
            "rate_limit_period": 60
        }
    }


@pytest.fixture
def sample_agent_data() -> Dict[str, Any]:
    """Provide sample agent data for testing."""
    return {
        "name": "TestAgent",
        "version": "1.0.0",
        "capabilities": [
            {"name": "coding", "level": 0.9},
            {"name": "testing", "level": 0.8}
        ],
        "constraints": {
            "max_concurrent_tasks": 5,
            "timeout_seconds": 300
        }
    }


@pytest.fixture
def sample_task_data() -> Dict[str, Any]:
    """Provide sample task data for testing."""
    return {
        "id": "task-001",
        "description": "Test task for unit testing",
        "type": "code_generation",
        "priority": "high",
        "complexity": 5.0,
        "metadata": {
            "required_capabilities": ["coding"],
            "preferred_agent": None
        }
    }


@pytest.fixture
def sample_product_data() -> Dict[str, Any]:
    """Provide sample product data for testing."""
    return {
        "name": "Test Product",
        "description": "A test product for unit testing",
        "price": 99.99,
        "currency": "USD",
        "category": "Electronics",
        "sku": "TEST-001",
        "stock_quantity": 100,
        "is_active": True,
        "is_featured": False,
        "images": ["http://example.com/image.jpg"],
        "attributes": {"color": "black", "size": "large"},
        "tags": ["test", "sample"]
    }


@pytest.fixture
def sample_video_metadata() -> Dict[str, Any]:
    """Provide sample video metadata for testing."""
    return {
        "file_path": "/path/to/video.mp4",
        "duration": 120.5,
        "width": 1920,
        "height": 1080,
        "fps": 30.0,
        "codec": "h264",
        "audio_codec": "aac",
        "audio_channels": 2,
        "audio_sample_rate": 44100,
        "bitrate": 5000000,
        "file_size": 75000000,
        "format": "mp4"
    }
