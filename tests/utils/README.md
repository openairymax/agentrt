# 测试工具函数与脚本

`tests/utils/`

## 概述

`utils/` 目录提供测试基础设施，共 **11 个文件**，包括测试基类、数据生成器、Mock 工厂、断言辅助、环境隔离、运行入口和报告生成器等。

> **版本**：v0.1.0

## 目录结构

```
utils/                             # 共 11 个文件
├── README.md                      # 本文档
└── python/                        # Python 测试工具（11 个文件）
    ├── __init__.py
    ├── base_test.py               # 测试基类（BaseTestCase/SDKTestCase/APITestCase/IntegrationTestCase）
    ├── data_generator.py          # 测试数据生成器
    ├── data_manager.py            # 测试数据管理器
    ├── environment_validator.py   # 环境验证器
    ├── test_helpers.py            # Mock/断言/性能/内存工具集合
    ├── test_isolation.py          # 测试环境隔离（文件系统/数据库/资源限制）
    ├── test_quality_reporter.py   # 测试质量报告生成器
    ├── check_syntax.py            # Python 语法检查器
    ├── generate_combined_report.py # 综合报告生成器
    └── run_tests.py               # 测试统一运行入口
```

## 使用方式

```python
from tests.utils.python.base_test import BaseTestCase, IntegrationTestCase
from tests.utils.python.test_helpers import MockFactory, AssertHelpers
from tests.utils.python.data_generator import TestDataFactory
```

### 运行测试

```bash
# 统一入口
python tests/utils/python/run_tests.py

# 运行所有测试
python tests/utils/python/run_tests.py --all

# 仅运行单元测试
python tests/utils/python/run_tests.py --type unit

# 运行特定模块
python tests/utils/python/run_tests.py --module atoms
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
