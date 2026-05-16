# 脚本测试套件

`scripts/tests/`

## 概述

`tests/` 目录包含 AgentOS 脚本工具的测试套件，分为 Python 测试和 Shell 测试两个子目录，分别使用 pytest 和 Shell 测试框架执行。

## 目录结构

```
tests/
├── python/                    # Python 测试
│   ├── conftest.py            # pytest 配置和公共 fixture
│   ├── test_core.py           # 核心功能测试
│   ├── test_token_counter.py  # Token 计数器测试
│   ├── test_token_budget.py   # Token 预算测试
│   ├── test_memory_manager.py # 内存管理器测试
│   └── test_checkpoint_manager.py  # 检查点管理器测试
└── shell/                     # Shell 测试
    ├── test_framework.sh      # Shell 测试框架
    └── test_common_utils.sh   # 公共工具函数测试
```

## 使用方式

### Python 测试

```bash
# 运行所有 Python 测试
pytest scripts/tests/python/

# 运行指定测试模块
pytest scripts/tests/python/test_core.py

# 带覆盖率报告
pytest scripts/tests/python/ --cov=scripts/toolkit
```

### Shell 测试

```bash
# 运行 Shell 测试框架
bash scripts/tests/shell/test_framework.sh

# 运行公共工具测试
bash scripts/tests/shell/test_common_utils.sh
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
