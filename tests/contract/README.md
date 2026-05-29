# 契约测试

`tests/contract/`

## 概述

`contract/` 目录包含 AgentOS 的接口契约测试，共 **4 个文件**，用于验证 Agent 和 Skill 的接口定义与行为是否符合预期规范。

> **版本**：v0.1.0

## 与 agentos/ 模块对应关系

| tests/contract/ 目录 | 对应的 agentos/ 模块 | 测试内容 |
|---------------------|---------------------|----------|
| `python/contract_test_generator.py` | `openlab/contrib/`, `daemon/` | 契约测试用例自动生成器 |
| `python/test_agent_contracts.py` | `daemon/`, `openlab/` | Agent 接口契约验证（注册/发现/通信） |
| `python/test_skill_contracts.py` | `openlab/contrib/` | Skill 接口契约验证（输入/输出/元数据） |

## 目录结构

```
contract/                          # 共 4 个文件
├── README.md                      # 本文档
└── python/                        # Python 契约测试（4 个文件）
    ├── __init__.py
    ├── contract_test_generator.py #   契约测试用例生成器
    ├── test_agent_contracts.py    #   Agent 接口契约验证
    └── test_skill_contracts.py    #   Skill 接口契约验证
```

## 运行方式

```bash
pytest tests/contract/python/ -v -m contract
```

## 契约测试范围

| 契约类型 | 对应的 agentos/ 模块 | 验证目标 |
|---------|---------------------|----------|
| **Agent 契约** | `daemon/`, `openlab/` | 注册、发现、生命周期管理 |
| **Skill 契约** | `openlab/contrib/` | 输入格式、输出格式、元数据完整性 |
| **Tool 契约** | `daemon/tool_d/` | 参数验证、执行结果、错误处理 |

---

© 2026 SPHARX Ltd. All Rights Reserved.