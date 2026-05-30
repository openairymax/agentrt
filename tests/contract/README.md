# 契约测试

`tests/contract/`

## 概述

`contract/` 目录包含 AgentOS 的接口契约测试，共 **4 个文件**，用于验证 Agent 和 Skill 的接口定义与行为是否符合预期规范。契约测试确保各服务之间的接口约定得到遵守，防止接口变更导致的集成故障。

契约测试与集成测试的区别在于：集成测试验证多组件协作的端到端行为，而契约测试专注于验证每个组件的接口契约（输入格式、输出格式、元数据完整性、错误码语义）是否与规范一致。契约测试的核心价值：

- **接口一致性**：确保 Agent/Skill 的接口定义与实际行为一致
- **变更安全**：接口变更时自动检测破坏性变更
- **自动生成**：通过 `contract_test_generator.py` 自动生成契约测试用例
- **文档化**：契约测试本身就是接口规范的活文档

> **版本**：v0.0.5

## 与 agentos/ 模块对应关系

| tests/contract/ 目录 | 对应的 agentos/ 模块 | 测试内容 |
|---------------------|---------------------|----------|
| `python/contract_test_generator.py` | `openlab/contrib/`, `daemon/` | 契约测试用例自动生成器（从接口定义自动生成测试代码） |
| `python/test_agent_contracts.py` | `daemon/`, `openlab/` | Agent 接口契约验证（注册/发现/通信/生命周期） |
| `python/test_skill_contracts.py` | `openlab/contrib/` | Skill 接口契约验证（输入/输出/元数据/版本兼容） |

## 目录结构

```
contract/                          # 共 4 个文件
├── README.md                      # 本文档
└── python/                        # Python 契约测试（4 个文件）
    ├── __init__.py                #   Python 包初始化
    ├── contract_test_generator.py #   契约测试用例生成器
    │                              #     从 Agent/Skill 接口定义自动生成契约测试
    │                              #     支持 JSON Schema 验证、参数组合枚举
    ├── test_agent_contracts.py    #   Agent 接口契约验证
    │                              #     注册接口契约、发现接口契约、通信协议契约
    │                              #     生命周期管理契约、错误码语义验证
    └── test_skill_contracts.py    #   Skill 接口契约验证
                                   #     输入格式契约、输出格式契约、元数据完整性
                                   #     版本兼容性、依赖声明验证
```

## 测试框架说明

### Python 契约测试（pytest）

契约测试使用 pytest 框架，标记为 `@pytest.mark.contract`。测试遵循以下模式：

1. **定义契约**：通过 JSON Schema 或 Python 数据类定义接口契约
2. **验证输入**：验证接口接受的输入参数符合契约定义
3. **验证输出**：验证接口返回的结果符合契约定义
4. **验证元数据**：验证接口的元数据（版本、描述、依赖）完整且一致
5. **验证错误**：验证错误码和错误消息符合契约定义

### 契约测试生成器

`contract_test_generator.py` 提供自动化的契约测试生成能力：

- **Schema 驱动**：从 JSON Schema 定义自动生成测试用例
- **参数枚举**：自动枚举参数组合，生成边界值测试
- **模板化**：支持自定义测试模板，适配不同接口风格

## 运行方式

```bash
# 运行全部契约测试
pytest tests/contract/python/ -v -m contract

# Agent 契约测试
pytest tests/contract/python/test_agent_contracts.py -v

# Skill 契约测试
pytest tests/contract/python/test_skill_contracts.py -v

# 生成契约测试用例
python tests/contract/python/contract_test_generator.py

# 使用统一入口
python tests/utils/python/run_tests.py --type contract

# 并行运行
pytest tests/contract/ -v -n auto -m contract

# 生成覆盖率报告
pytest tests/contract/ -v --cov=agentos --cov-report=html -m contract
```

## 契约测试范围

| 契约类型 | 对应的 agentos/ 模块 | 验证目标 | 测试文件 |
|---------|---------------------|----------|---------|
| **Agent 注册契约** | `daemon/`, `openlab/` | 注册接口参数、返回值、错误码 | `test_agent_contracts.py` |
| **Agent 发现契约** | `daemon/`, `openlab/` | 发现接口查询参数、返回格式、分页 | `test_agent_contracts.py` |
| **Agent 通信契约** | `daemon/` | 通信协议格式、消息结构、超时处理 | `test_agent_contracts.py` |
| **Agent 生命周期契约** | `daemon/` | 启动/停止/重启接口、状态转换 | `test_agent_contracts.py` |
| **Skill 输入契约** | `openlab/contrib/` | 输入参数类型、必填项、默认值、约束 | `test_skill_contracts.py` |
| **Skill 输出契约** | `openlab/contrib/` | 输出格式、字段类型、必填项、枚举值 | `test_skill_contracts.py` |
| **Skill 元数据契约** | `openlab/contrib/` | 版本号、描述、依赖声明、兼容性 | `test_skill_contracts.py` |
| **Tool 契约** | `daemon/tool_d/` | 参数验证、执行结果、错误处理 | `test_agent_contracts.py` |

## 测试覆盖说明

| agentos/ 模块 | 契约测试文件 | 测试框架 | 覆盖范围 |
|--------------|------------|---------|---------|
| `daemon/` | `test_agent_contracts.py` | pytest | Agent 注册/发现/通信/生命周期接口契约 |
| `openlab/` | `test_agent_contracts.py` | pytest | Agent 在开放生态中的接口契约 |
| `openlab/contrib/` | `test_skill_contracts.py` | pytest | Skill 输入/输出/元数据/版本兼容契约 |
| `daemon/tool_d/` | `test_agent_contracts.py` | pytest | Tool 参数/执行/错误契约 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
