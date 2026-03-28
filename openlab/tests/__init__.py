# Copyright (c) 2026 SPHARX. All Rights Reserved.
# "From data intelligence emerges."

"""
OpenLab Test Suite
================

This module provides the test suite for OpenLab AgentOS.

Test Structure:
- unit/: Unit tests for individual components
- integration/: Integration tests for component interactions
- conftest.py: Shared pytest fixtures

Running Tests:
    pytest tests/ -v
    pytest tests/unit/ -v
    pytest tests/integration/ -v
    pytest tests/ -v --cov=openlab
"""

import sys
import os

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
