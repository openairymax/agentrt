# 测试模板

`tests/templates/`

## 概述

`templates/` 目录提供 C 和 Python 测试模板文件，共 **5 个文件**，用于快速创建新的测试用例。复制对应模板到目标目录，替换占位符即可使用。

> **版本**：v0.1.0

## 目录结构

```
templates/                         # 共 5 个文件
├── README.md                      # 本文档
├── c/                             # C 测试模板
│   └── test_template.c            # C 单元测试模板（CMocka）
└── python/                        # Python 测试模板（4 个文件）
    ├── __init__.py
    ├── test_template.py           #   基础单元测试模板
    ├── test_template_integration.py # 集成测试模板
    └── test_template_security.py  # 安全测试模板
```

## 使用方式

1. 复制对应模板到目标目录
2. 替换 `<Module>` 占位符为实际模块名
3. 替换 `module_function()` 为实际被测试函数
4. 设置 `EXPECTED_VALUE` / `ERROR_CODE` 等宏

## 命名规范

| 模板 | 用途 | 标记 |
|------|------|------|
| `test_template.py` | 基础单元测试 | `@pytest.mark.unit` |
| `test_template_integration.py` | 集成测试 | `@pytest.mark.integration` |
| `test_template_security.py` | 安全测试 | `@pytest.mark.security` |

---

© 2026 SPHARX Ltd. All Rights Reserved.
