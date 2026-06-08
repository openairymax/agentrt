# 测试工具函数与脚本

`tests/utils/`

## 概述

`utils/` 目录提供测试基础设施，共 **16+ 个文件**，包括测试基类、数据生成器、Mock 工厂、断言辅助、环境隔离、运行入口、报告生成器、测试夹具和测试模板等。这些工具被所有测试层级（单元/集成/契约/安全/基准）共享使用，是 AgentOS 测试体系的核心支撑层。

工具模块的核心能力：
- **测试基类**：提供 `BaseTestCase`、`SDKTestCase`、`APITestCase`、`IntegrationTestCase` 等预置基类
- **数据管理**：`data_generator.py` 和 `data_manager.py` 提供测试数据生成与管理
- **环境隔离**：`test_isolation.py` 确保测试之间文件系统/数据库/资源的完全隔离
- **统一入口**：`run_tests.py` 提供命令行接口，支持按类型/模块/标记运行测试
- **质量报告**：`test_quality_reporter.py` 和 `generate_combined_report.py` 生成综合测试报告
- **测试模板**：提供 C/Python 测试文件模板，规范新测试的编写

> **版本**：v0.0.5

## 与 agentos/ 模块对应关系

| tests/utils/ 目录 | 对应的 agentos/ 模块 | 用途说明 |
|-------------------|---------------------|----------|
| `fixtures/data/memories/` | `atoms/memory/`, `atoms/memoryrovol/` | 记忆子系统测试数据（L1-L4 层样本数据） |
| `fixtures/data/sessions/` | `daemon/`, `gateway/` | 会话管理测试数据（会话创建/恢复/过期样本） |
| `fixtures/data/skills/` | `openlab/contrib/` | 技能模块测试数据（Skill 定义/元数据样本） |
| `fixtures/data/tasks/` | `daemon/sched_d/`, `daemon/tool_d/` | 任务调度测试数据（任务定义/优先级/依赖样本） |
| `python/base_test.py` | 全部模块 | 测试基类（BaseTestCase/SDKTestCase/APITestCase/IntegrationTestCase） |
| `python/data_generator.py` | 全部模块 | 测试数据生成器（随机/参数化/边界值数据） |
| `python/data_manager.py` | 全部模块 | 测试数据管理器（数据加载/清理/持久化） |
| `python/environment_validator.py` | 全部模块 | 环境验证器（依赖检查/版本兼容/配置验证） |
| `python/test_helpers.py` | 全部模块 | Mock/断言/性能/内存工具集合 |
| `python/test_isolation.py` | 全部模块 | 测试环境隔离（文件系统/数据库/资源限制） |
| `python/test_quality_reporter.py` | 全部模块 | 测试质量报告生成器 |
| `python/check_syntax.py` | 全部模块 | Python 语法检查器 |
| `python/generate_combined_report.py` | 全部模块 | 综合报告生成器（合并多维度测试结果） |
| `python/run_tests.py` | 全部模块 | 测试统一运行入口 |
| `templates/c/test_template.c` | `atoms/`, `commons/`, `cupolas/`, `heapstore/` | C 语言测试文件模板 |
| `templates/python/test_template.py` | `daemon/`, `manager/`, `openlab/`, `toolkit/` | Python 单元测试模板 |
| `templates/python/test_template_integration.py` | `daemon/`, `gateway/`, `heapstore/` | Python 集成测试模板 |
| `templates/python/test_template_security.py` | `cupolas/`, `daemon/common/` | Python 安全测试模板 |

## 目录结构

```
utils/                             # 共 16+ 个文件
├── README.md                      # 本文档
├── fixtures/                      # 测试夹具和数据
│   └── data/
│       ├── memories/              #   记忆子系统测试数据
│       │   └── sample_memories.json #   L1-L4 层记忆样本
│       ├── sessions/              #   会话管理测试数据
│       │   └── sample_sessions.json #   会话创建/恢复/过期样本
│       ├── skills/                #   技能模块测试数据
│       │   └── sample_skills.json #   Skill 定义/元数据样本
│       └── tasks/                 #   任务调度测试数据
│           └── sample_tasks.json  #   任务定义/优先级/依赖样本
├── python/                        # Python 测试工具（11 个文件）
│   ├── __init__.py                #   Python 包初始化
│   ├── base_test.py               #   测试基类
│   │                              #     BaseTestCase — 通用测试基类
│   │                              #     SDKTestCase — SDK 测试基类
│   │                              #     APITestCase — API 测试基类
│   │                              #     IntegrationTestCase — 集成测试基类
│   ├── data_generator.py          #   测试数据生成器
│   │                              #     随机数据生成、参数化数据、边界值数据
│   ├── data_manager.py            #   测试数据管理器
│   │                              #     数据加载、清理、持久化、版本管理
│   ├── environment_validator.py   #   环境验证器
│   │                              #     依赖检查、版本兼容性、配置验证
│   ├── test_helpers.py            #   Mock/断言/性能/内存工具集合
│   │                              #     MockFactory — Mock 对象工厂
│   │                              #     AssertHelpers — 断言辅助函数
│   │                              #     PerformanceTimer — 性能计时器
│   │                              #     MemoryTracker — 内存追踪器
│   ├── test_isolation.py          #   测试环境隔离
│   │                              #     文件系统隔离、数据库隔离、资源限制隔离
│   ├── test_quality_reporter.py   #   测试质量报告生成器
│   │                              #     测试覆盖率、通过率、执行时间统计
│   ├── check_syntax.py            #   Python 语法检查器
│   │                              #     静态语法检查、导入验证
│   ├── generate_combined_report.py #  综合报告生成器
│   │                              #     合并单元/集成/安全/基准测试结果
│   └── run_tests.py               #   测试统一运行入口
│                                  #     支持 --type/--module/--report/--all 参数
└── templates/                     # 测试模板（5 个文件）
    ├── c/
    │   └── test_template.c        #   C 测试模板
    │                              #     CMocka 测试组结构、setup/teardown 模板
    └── python/
        ├── __init__.py            #   Python 包初始化
        ├── test_template.py               # Python 单元测试模板
        │                              #   pytest 测试类结构、夹具使用模板
        ├── test_template_integration.py   # 集成测试模板
        │                              #   多模块初始化、端到端验证模板
        └── test_template_security.py      # 安全测试模板
                                       #   输入验证、权限检查、沙箱测试模板
```

## 测试框架说明

### 测试基类（base_test.py）

提供四个预置测试基类，简化测试编写：

- **BaseTestCase**：通用测试基类，提供日志记录、临时目录、断言增强等基础能力
- **SDKTestCase**：SDK 测试基类，预置 SDK 客户端初始化和清理逻辑
- **APITestCase**：API 测试基类，预置 HTTP 客户端和请求辅助方法
- **IntegrationTestCase**：集成测试基类，预置多模块初始化和依赖管理

### 测试辅助工具（test_helpers.py）

提供四个核心工具类：

- **MockFactory**：Mock 对象工厂，支持快速创建 Mock 对象和配置行为
- **AssertHelpers**：断言辅助函数，提供比 pytest 原生断言更丰富的比较能力
- **PerformanceTimer**：性能计时器，支持上下文管理器模式的计时
- **MemoryTracker**：内存追踪器，检测内存泄漏和异常分配

### 环境隔离（test_isolation.py）

确保测试之间完全隔离，避免测试污染：

- **文件系统隔离**：每个测试使用独立的临时目录
- **数据库隔离**：每个测试使用独立的数据库实例或事务回滚
- **资源限制隔离**：每个测试在独立的资源配额下运行

### 测试模板（templates/）

提供标准化的测试文件模板，规范新测试的编写：

- **C 测试模板**：包含 CMocka 测试组结构、setup/teardown 函数、断言宏使用示例
- **Python 单元测试模板**：包含 pytest 测试类结构、夹具使用、参数化示例
- **Python 集成测试模板**：包含多模块初始化、端到端验证、超时处理示例
- **Python 安全测试模板**：包含输入验证、权限检查、沙箱测试示例

## 运行方式

### 使用统一入口

```bash
# 运行所有测试
python tests/utils/python/run_tests.py --all

# 按类型运行
python tests/utils/python/run_tests.py --type unit
python tests/utils/python/run_tests.py --type integration
python tests/utils/python/run_tests.py --type contract
python tests/utils/python/run_tests.py --type security
python tests/utils/python/run_tests.py --type benchmark

# 按模块运行
python tests/utils/python/run_tests.py --module atoms
python tests/utils/python/run_tests.py --module daemon
python tests/utils/python/run_tests.py --module commons

# 生成测试报告
python tests/utils/python/run_tests.py --report html
python tests/utils/python/run_tests.py --report json
```

### 使用辅助工具

```python
from tests.utils.python.base_test import BaseTestCase, IntegrationTestCase
from tests.utils.python.test_helpers import MockFactory, AssertHelpers, PerformanceTimer
from tests.utils.python.data_generator import TestDataFactory
from tests.utils.python.test_isolation import TestIsolation
```

### 环境验证

```bash
# 验证测试环境
python tests/utils/python/environment_validator.py
```

### 语法检查

```bash
# 检查 Python 语法
python tests/utils/python/check_syntax.py
```

### 生成报告

```bash
# 生成综合测试报告
python tests/utils/python/generate_combined_report.py

# 生成质量报告
python tests/utils/python/test_quality_reporter.py
```

## 测试覆盖说明

| 工具模块 | 服务对象 | 核心能力 |
|---------|---------|---------|
| `base_test.py` | 全部测试层级 | 测试基类、通用夹具、断言增强 |
| `data_generator.py` | 单元/集成/契约测试 | 随机数据生成、参数化数据、边界值数据 |
| `data_manager.py` | 全部测试层级 | 测试数据加载、清理、版本管理 |
| `environment_validator.py` | CI/CD 流水线 | 依赖检查、版本兼容性、配置验证 |
| `test_helpers.py` | 全部测试层级 | Mock 工厂、断言辅助、性能计时、内存追踪 |
| `test_isolation.py` | 集成/安全测试 | 文件系统/数据库/资源隔离 |
| `test_quality_reporter.py` | CI/CD 流水线 | 测试质量指标统计与报告 |
| `check_syntax.py` | CI/CD 流水线 | Python 语法静态检查 |
| `generate_combined_report.py` | CI/CD 流水线 | 多维度测试结果合并报告 |
| `run_tests.py` | 全部测试层级 | 统一命令行运行入口 |
| `fixtures/` | 全部测试层级 | 预置测试数据（记忆/会话/技能/任务） |
| `templates/` | 新测试编写 | C/Python 测试文件模板 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
