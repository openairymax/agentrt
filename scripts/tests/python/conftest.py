﻿﻿﻿#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS Scripts 测试套件
# pytest 配置文件

"""
pytest configuration for AgentOS Scripts

Usage:
    pytest scripts/tests/python/
    pytest scripts/tests/python/ -v
    pytest scripts/tests/python/ --cov=scripts/core
"""

import os
import sys
from pathlib import Path

# 添加项目根目录到 Python 路径
PROJECT_ROOT = Path(__file__).parent.parent.parent
sys.path.insert(0, str(PROJECT_ROOT))

# pytest 配置
def pytest_configure(manager):
    """pytest 配置钩子"""
    manager.addinivalue_line("markers", "slow: marks tests as slow")
    manager.addinivalue_line("markers", "integration: marks tests as integration tests")
    manager.addinivalue_line("markers", "security: marks tests as security tests")


def pytest_collection_modifyitems(manager, items):
    """修改测试收集"""
    for item in items:
        # 为 core 模块的测试添加慢速标记
        if "core" in str(item.fspath):
            item.add_marker("slow")