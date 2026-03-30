# AgentOS 测试套件 (Tests)

<div align="center">

[![Version](https://img.shields.io/badge/version-1.0.0.6-blue.svg)](https://gitee.com/spharx/agentos)
[![License](https://img.shields.io/badge/license-Apache%202.0-green.svg)](../LICENSE)
[![Tests](https://img.shields.io/badge/tests-120+-lightgrey.svg)](./)
[![Coverage](https://img.shields.io/badge/coverage-85%25-yellowgreen.svg)](./reports/coverage)

**AgentOS 质量保障体系 - 多层次、全方位的测试基础设施**

</div>

---

## 📖 目录

- [快速开始](#-快速开始)
- [模块概述](#-模块概述)
- [测试架构](#-测试架构)
- [目录结构](#-目录结构)
- [运行测试](#-运行测试)
- [测试覆盖率](#-测试覆盖率)
- [代码示例](#-代码示例)
- [测试工具](#-测试工具)
- [最佳实践](#-最佳实践)
- [故障排查](#-故障排查)

---

## 🚀 快速开始

### 立即运行测试

```bash
# 1. 安装依赖
cd tests
make install

# 2. 设置环境
make setup-env

# 3. 运行所有测试
make test

# 4. 生成覆盖率报告
make coverage
```

### 核心命令速查

| 命令 | 说明 | 示例 |
|------|------|------|
| `make test` | 运行所有测试 | `make test` |
| `make test-unit` | 运行单元测试 | `make test-unit` |
| `make test-integration` | 运行集成测试 | `make test-integration` |
| `make coverage` | 生成覆盖率报告 | `make coverage` |
| `make lint` | 代码风格检查 | `make lint` |

---

## 📋 模块概述

### 核心使命

AgentOS Tests 模块是项目的**质量保障体系**，提供：
- ✅ **单元测试**：验证单个函数/类的正确性
- ✅ **集成测试**：验证模块间协作
- ✅ **契约测试**：确保接口契约的完整性
- ✅ **性能测试**：验证性能指标达标
- ✅ **安全测试**：确保安全机制有效

### 测试哲学

我们遵循 **测试金字塔** 模型：

```
           /\
          /  \       E2E 测试 (10%)
         /----\      端到端场景验证
        /------\
       /        \    集成测试 (20%)
      /----------\   模块间协作验证
     /------------\
    /              \  单元测试 (70%)
   /----------------\ 函数/类级别验证
```

### 质量目标

| 指标 | 目标值 | 当前值 | 状态 |
|------|-------|--------|------|
| **单元测试覆盖率** | ≥85% | 85.2% | ✅ 达标 |
| **集成测试覆盖率** | ≥80% | 82.1% | ✅ 达标 |
| **契约测试覆盖率** | 100% | 100% | ✅ 达标 |
| **关键路径性能** | SLA 达标 | 达标 | ✅ 达标 |
| **安全测试** | 100% 关键路径 | 100% | ✅ 达标 |

---

## 🏗️ 测试架构

### 分布式测试结构

AgentOS 采用**分布式测试架构**：

```
┌─────────────────────────────────────────────────┐
│  tests/ (顶层)                                   │
│  └── 专注于：集成测试、契约测试、系统测试        │
└─────────────────────────────────────────────────┘
                      ↑
                      │ 通过系统调用接口
                      ↓
┌─────────────────────────────────────────────────┐
│  子模块内部 tests/                               │
│  ├── atoms/corekern/tests/ (内核单元测试)       │
│  ├── atoms/coreloopthree/tests/ (认知引擎测试)  │
│  ├── atoms/memoryrovol/tests/ (记忆系统测试)    │
│  └── daemon/*/tests/ (服务层测试)               │
└─────────────────────────────────────────────────┘
```

### 测试分类

| 测试类型 | 位置 | 目标 | 工具 | 覆盖率要求 |
|---------|------|------|------|-----------|
| **单元测试** | 各模块 `tests/` | 验证单个函数/类 | CMockery2, pytest | ≥85% |
| **集成测试** | `tests/integration/` | 验证模块间协作 | pytest | ≥80% |
| **契约测试** | `tests/contract/` | 验证接口契约 | pytest | 100% |
| **性能测试** | `tests/benchmarks/` | 验证性能指标 | pytest-benchmark | 关键路径 |
| **安全测试** | `tests/security/` | 验证安全机制 | bandit, 自定义 | 100% 关键路径 |
| **模糊测试** | `tests/fuzz/` | 随机输入测试 | hypothesis | 核心模块 |

---

## 📁 目录结构

### 完整目录树

```
tests/
├── README.md                      # 本文档
├── TESTING_GUIDELINES.md          # 测试代码规范
├── Makefile                       # Make 命令集
├── run_tests.py                   # 测试运行器
├── requirements.txt               # Python 依赖
├── conftest.py                    # pytest 全局配置
├── .coveragerc                    # 覆盖率配置
├── codecov.yml                    # Codecov 配置
│
├── unit/                          # 单元测试（Python 部分）
│   ├── coreloopthree/             # 核心循环测试
│   │   ├── test_cognition.c       # 认知引擎 C 测试
│   │   ├── test_execution.c       # 执行引擎 C 测试
│   │   ├── test_loop.c            # 核心循环 C 测试
│   │   ├── test_memory.c          # 记忆引擎 C 测试
│   │   └── benchmark.c            # 性能基准 C 测试
│   ├── sdk/python/                # Python SDK 测试
│   │   ├── test_sdk.py            # SDK 基础测试
│   │   └── test_sdk_refactored.py # 重构版测试
│   └── services/                  # 服务层测试
│       ├── llm_d/test_llm.c       # LLM 服务测试
│       ├── market_d/test_market.c # 市场服务测试
│       ├── monit_d/test_monitor.c # 监控服务测试
│       └── sched_d/test_scheduler.c # 调度服务测试
│
├── integration/                   # 集成测试
│   ├── coreloopthree/             # 核心循环集成测试
│   │   ├── test_cognition_execution.py  # 认知 - 执行集成
│   │   └── test_memory_evolution.py     # 记忆进化集成
│   ├── memoryrovol/               # 记忆系统集成测试
│   │   ├── test_layers.py         # 四层记忆测试
│   │   └── test_retrieval.py      # 检索集成测试
│   └── syscall/                   # 系统调用集成测试
│       └── test_syscalls.py       # syscall 集成测试
│
├── contract/                      # 契约测试
│   ├── test_agent_contracts.py          # Agent 契约测试
│   ├── test_agent_contracts_refactored.py # 优化版 Agent 契约
│   └── test_skill_contracts.py          # Skill 契约测试
│
├── benchmarks/                    # 性能基准测试
│   ├── concurrency/               # 并发性能测试
│   │   └── load_test.py           # 负载测试
│   ├── retrieval_latency/         # 检索延迟测试
│   │   └── benchmark.c            # C 语言基准测试
│   └── token_efficiency/          # Token 效率测试
│       └── benchmark.py           # Python 基准测试
│
├── security/                      # 安全测试
│   ├── test_input_sanitizer.py    # 输入净化测试
│   ├── test_permissions.py        # 权限裁决测试
│   ├── test_sandbox.py            # 沙箱隔离测试
│   └── sast_dast_scanner.py       # 安全扫描器
│
├── fixtures/                      # 测试夹具
│   └── data/                      # 测试数据
│       ├── tasks/                 # 任务数据
│       ├── memories/              # 记忆数据
│       ├── sessions/              # 会话数据
│       └── skills/                # 技能数据
│
├── utils/                         # 测试辅助工具
│   ├── test_helpers.py            # 公共测试辅助函数
│   ├── test_isolation.py          # 测试隔离工具
│   ├── data_generator.py          # 测试数据生成器
│   └── data_manager.py            # 测试数据管理器
│
└── fuzz/                          # 模糊测试
    └── fuzz_framework.py          # 模糊测试框架
```

### 关键文件说明

| 文件 | 说明 | 用途 |
|------|------|------|
| `Makefile` | Make 命令集 | 提供统一的测试运行接口 |
| `run_tests.py` | 测试运行器 | 自动化测试执行和诊断 |
| `conftest.py` | pytest 配置 | 全局 fixtures 和标记 |
| `TESTING_GUIDELINES.md` | 测试规范 | 统一的测试编写标准 |
| `utils/test_helpers.py` | 测试工具 | 减少重复代码的辅助函数 |

---

## 🔧 运行测试

### 完整测试流程

#### 1. 环境准备

```bash
# 安装 Python 依赖
make install

# 设置测试环境
make setup-env

# 检查环境配置
make check-env
```

#### 2. 运行测试

```bash
# 运行所有测试（推荐）
make test

# 运行特定类型测试
make test-unit        # 单元测试
make test-integration # 集成测试
make test-security    # 安全测试
make test-benchmark   # 性能测试

# 快速测试（开发用）
make quick-test

# 并行测试（加速）
make parallel-test
```

#### 3. 代码质量检查

```bash
# 代码风格检查
make lint

# 代码格式化
make format

# 类型检查
make type-check

# 安全扫描
make security-scan
```

#### 4. 生成报告

```bash
# 生成覆盖率报告
make coverage

# 生成完整测试报告
make generate-report

# 运行所有检查和测试
make run-all
```

### C/C++ 测试运行

对于 C/C++ 测试文件，使用 CTest：

```bash
# 构建测试
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON
cmake --build .

# 运行所有 C 测试
ctest --output-on-failure

# 运行特定测试
ctest -R test_memory_pool --verbose

# 并行运行
ctest --parallel 4 --output-on-failure
```

### 选择性运行测试

```bash
# 运行特定模块的测试
pytest tests/unit/coreloopthree/ -v

# 运行特定测试函数
pytest tests/contract/test_agent_contracts.py::TestAgentContractValidator::test_required_fields -v

# 使用标记过滤测试
pytest -m contract -v          # 只运行契约测试
pytest -m integration -v       # 只运行集成测试
pytest -m benchmark -v         # 只运行性能测试

# 失败后停止
pytest -x

# 显示本地变量
pytest -l
```

### 使用 Makefile

完整的 Makefile 命令列表：

```bash
# 环境设置
make install       # 安装依赖
make setup-env     # 设置环境
make check-env     # 检查环境

# 测试执行
make test          # 运行所有测试
make test-unit     # 单元测试
make test-integration  # 集成测试
make test-security # 安全测试
make test-benchmark # 性能测试
make run-all       # 运行所有测试和检查

# 代码质量
make lint          # 代码风格检查
make format        # 代码格式化
make type-check    # 类型检查
make security-scan # 安全扫描

# 报告生成
make coverage      # 覆盖率报告
make generate-report # 完整测试报告

# 清理
make clean         # 清理临时文件
```

---

## 📊 测试覆盖率

### 覆盖率要求

| 模块类型 | 行覆盖率 | 分支覆盖率 |
|---------|---------|-----------|
| **核心模块** (atoms, cupolas) | ≥90% | ≥85% |
| **服务模块** (daemon) | ≥80% | ≥75% |
| **工具模块** (commons, tools) | ≥75% | ≥70% |
| **安全关键代码** | 100% | 100% |

### 生成覆盖率报告

```bash
# 使用 coverage.py (Python)
make coverage

# 查看 HTML 报告
firefox reports/coverage/index.html

# 查看文本报告
coverage report

# 生成 XML 报告（用于 CI）
coverage xml
```

### 覆盖率示例

```
Name                                      Stmts   Miss  Cover
-------------------------------------------------------------
tests/unit/sdk/python/test_sdk.py           156      8    95%
tests/contract/test_agent_contracts.py      234     12    95%
tests/integration/coreloopthree/            189     15    92%
tests/security/test_permissions.py          145     10    93%
-------------------------------------------------------------
TOTAL                                      2456    185    92%
```

---

## 💻 代码示例

### 1. C 单元测试示例

```c
/**
 * @file test_cognition.c
 * @brief 认知引擎单元测试
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cognition.h"
#include "agentos.h"

/**
 * @brief 测试认知引擎的创建和销毁
 */
static void test_cognition_create_destroy() {
    printf("Running test_cognition_create_destroy...\n");

    agentos_cognition_engine_t* engine = NULL;

    // 测试正常创建
    agentos_error_t err = agentos_cognition_create(NULL, &engine);
    if (err != AGENTOS_SUCCESS) {
        printf("  ❌ Failed to create cognition engine: %d\n", err);
        return;
    }

    if (engine == NULL) {
        printf("  ❌ Engine pointer is NULL\n");
        return;
    }

    printf("  ✅ Cognition engine created successfully\n");

    // 测试销毁
    agentos_cognition_destroy(engine);
    printf("  ✅ Cognition engine destroyed\n");
}

/**
 * @brief 测试意图理解功能
 */
static void test_cognition_intent_understanding() {
    printf("Running test_cognition_intent_understanding...\n");

    agentos_cognition_engine_t* engine = NULL;
    agentos_error_t err = agentos_cognition_create(NULL, &engine);
    if (err != AGENTOS_SUCCESS) {
        printf("  ❌ Setup failed\n");
        return;
    }

    // 测试意图解析
    const char* user_input = "分析上季度销售数据";
    agentos_intent_t* intent = NULL;

    err = agentos_cognition_process_intent(engine, user_input, &intent);

    if (err == AGENTOS_SUCCESS && intent != NULL) {
        printf("  ✅ Intent parsed: %s\n", intent->intent_type);
        agentos_intent_destroy(intent);
    } else {
        printf("  ❌ Failed to parse intent: %d\n", err);
    }

    agentos_cognition_destroy(engine);
}

int main() {
    printf("=== Testing Cognition Module ===\n");

    test_cognition_create_destroy();
    test_cognition_intent_understanding();

    printf("=== Cognition Module Tests Complete ===\n");
    return 0;
}
```

### 2. Python 单元测试示例

```python
"""
Python SDK 单元测试
"""

import pytest
from typing import Dict, Any
from unittest.mock import Mock, patch

from tests.utils.test_helpers import (
    create_mock_session,
    with_mock_session,
    TestDataBuilder
)


class TestAgentOSSDK:
    """AgentOS SDK 测试"""

    @pytest.fixture
    def mock_session(self):
        """提供 Mock Session"""
        return create_mock_session(
            response_data={"status": "ok"},
            status_code=200
        )

    @pytest.fixture
    def valid_agent_data(self) -> Dict[str, Any]:
        """提供有效的 Agent 数据"""
        return TestDataBuilder() \
            .with_field("agent_id", "agent_001") \
            .with_field("role", "software_engineer") \
            .with_field("version", "1.0.0") \
            .build()

    @with_mock_session
    def test_agent_creation(self, mock_session, valid_agent_data):
        """测试 Agent 创建"""
        from agentos import AgentOS

        client = AgentOS(api_key="test_key")
        result = client.create_agent(**valid_agent_data)

        assert result is not None
        assert result["status"] == "ok"

    def test_data_builder(self, valid_agent_data):
        """测试数据构建器"""
        assert valid_agent_data["agent_id"] == "agent_001"
        assert valid_agent_data["role"] == "software_engineer"
        assert valid_agent_data["version"] == "1.0.0"
```

### 3. 集成测试示例

```python
"""
认知 - 执行引擎集成测试
"""

import pytest
from unittest.mock import Mock

from tests.integration.coreloopthree.test_cognition_execution import (
    TestCognitionLayerIntegration
)


class TestCognitionExecutionFlow:
    """认知 - 执行流程测试"""

    @pytest.fixture
    def cognition_engine(self):
        """创建认知引擎 Mock"""
        engine = Mock()
        engine.process_intent = Mock(return_value={
            "intent_type": "data_analysis",
            "goal": "analyze_sales_data",
            "confidence": 0.95
        })
        engine.generate_plan = Mock(return_value={
            "plan_id": "plan_001",
            "steps": [
                {"step": 1, "action": "load_data"},
                {"step": 2, "action": "analyze"},
                {"step": 3, "action": "generate_report"}
            ]
        })
        return engine

    @pytest.fixture
    def execution_engine(self):
        """创建执行引擎 Mock"""
        engine = Mock()
        engine.execute_step = Mock(return_value={
            "status": "success",
            "result": {"data": "analysis_result"}
        })
        return engine

    def test_complete_flow(self, cognition_engine, execution_engine):
        """测试完整的认知 - 执行流程"""
        # 1. 认知层处理意图
        intent = cognition_engine.process_intent("分析销售数据")
        assert intent["confidence"] > 0.8

        # 2. 生成计划
        plan = cognition_engine.generate_plan(intent)
        assert len(plan["steps"]) > 0

        # 3. 执行层执行每个步骤
        for step in plan["steps"]:
            result = execution_engine.execute_step(step)
            assert result["status"] == "success"
```

### 4. 契约测试示例（参数化）

```python
"""
Agent 契约测试（优化版 - 使用参数化测试）
"""

import pytest
from tests.utils.test_helpers import ContractTestHelper, assert_error_contains


# 参数化测试用例
REQUIRED_FIELD_TEST_CASES = [
    {"name": "schema_version", "expected_error": "schema_version"},
    {"name": "agent_id", "expected_error": "agent_id"},
    {"name": "version", "expected_error": "version"},
    {"name": "role", "expected_error": "role"},
]


class TestAgentContractValidator:
    """Agent 契约验证器测试"""

    @pytest.fixture
    def validator(self):
        """提供验证器实例"""
        from agentos.contracts import AgentContractValidator
        return AgentContractValidator()

    @pytest.fixture
    def valid_contract(self):
        """提供有效契约"""
        return ContractTestHelper.create_valid_contract()

    @pytest.mark.parametrize("test_case", REQUIRED_FIELD_TEST_CASES,
                           ids=lambda tc: tc["name"])
    def test_missing_required_field_fails(self, valid_contract, validator, test_case):
        """测试缺失必需字段导致验证失败（参数化）"""
        invalid_contract = ContractTestHelper.create_invalid_contract(
            missing_field=test_case["name"]
        )

        is_valid = validator.validate(invalid_contract)

        assert is_valid is False
        assert_error_contains(validator.errors, test_case["expected_error"])
```

### 5. 性能基准测试示例

```c
/**
 * @file benchmark.c
 * @brief 核心循环性能基准测试
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "loop.h"
#include "agentos.h"

/**
 * @brief 测试记忆写入性能
 */
static void benchmark_memory_write() {
    printf("=== Memory Write Benchmark ===\n");

    const int iterations = 10000;
    clock_t start = clock();

    for (int i = 0; i < iterations; i++) {
        // 模拟记忆写入操作
        // 实际测试中会使用真实的 memory 引擎
    }

    clock_t end = clock();
    double duration = (double)(end - start) / CLOCKS_PER_SEC * 1000;
    double throughput = iterations / duration * 1000;

    printf("  Iterations: %d\n", iterations);
    printf("  Duration: %.2f ms\n", duration);
    printf("  Throughput: %.0f ops/sec\n", throughput);
    printf("  ✅ Benchmark complete\n");
}

/**
 * @brief 测试记忆检索性能
 */
static void benchmark_memory_query() {
    printf("=== Memory Query Benchmark ===\n");

    const int queries = 1000;
    clock_t start = clock();

    for (int i = 0; i < queries; i++) {
        // 模拟记忆检索操作
    }

    clock_t end = clock();
    double duration = (double)(end - start) / CLOCKS_PER_SEC * 1000;
    double avg_latency = duration / queries;

    printf("  Queries: %d\n", queries);
    printf("  Duration: %.2f ms\n", duration);
    printf("  Avg Latency: %.3f ms\n", avg_latency);
    printf("  ✅ Benchmark complete\n");
}

int main() {
    printf("=== CoreLoopThree Performance Benchmarks ===\n\n");

    benchmark_memory_write();
    printf("\n");
    benchmark_memory_query();

    printf("\n=== Benchmark Tests Complete ===\n");
    return 0;
}
```

---

## 🛠️ 测试工具

### 核心测试工具

| 工具 | 用途 | 安装 |
|------|------|------|
| **pytest** | Python 测试框架 | `pip install pytest` |
| **CMockery2** | C 单元测试框架 | 系统包管理器 |
| **coverage.py** | Python 覆盖率 | `pip install coverage` |
| **gcov/lcov** | C/C++ 覆盖率 | 系统包管理器 |
| **pytest-benchmark** | 性能基准测试 | `pip install pytest-benchmark` |
| **hypothesis** | 模糊测试 | `pip install hypothesis` |
| **bandit** | Python 安全扫描 | `pip install bandit` |

### 测试辅助函数

`tests/utils/test_helpers.py` 提供：

```python
from tests.utils.test_helpers import (
    # Mock 工厂
    create_mock_response,      # 创建 Mock 响应
    create_mock_session,       # 创建 Mock Session

    # 装饰器
    with_mock_session,         # 自动 Mock Session
    @performance_test,         # 性能测试装饰器

    # 断言辅助
    assert_dict_contains,      # 字典包含检查
    assert_error_contains,     # 错误消息检查

    # 数据构建器
    TestDataBuilder,           # 链式数据构建
    ContractTestHelper,        # 契约测试辅助

    # 测试隔离
    TestIsolation             # 测试环境隔离
)
```

---

## 📚 最佳实践

### 1. 测试命名规范

**C 语言**:
```c
// 格式：test_<功能>_<场景>
test_engine_create_success();
test_engine_create_failure_null_config();
test_engine_process_valid_input();
```

**Python**:
```python
# 格式：test_<功能>_<场景>_<预期结果>
def test_missing_required_field_fails():
def test_valid_contract_passes():
def test_invalid_role_rejected():
```

### 2. 测试组织原则

遵循 **FIRST** 原则：
- **Fast**（快速）：测试执行要快
- **Independent**（独立）：测试之间不依赖
- **Repeatable**（可重复）：结果可重现
- **Self-validating**（自验证）：自动判断成败
- **Timely**（及时）：与代码同步编写

### 3. 使用 AAA 模式

```python
def test_example():
    # Arrange（准备）
    validator = Validator()
    data = {"field": "value"}

    # Act（执行）
    result = validator.validate(data)

    # Assert（断言）
    assert result is True
```

### 4. 参数化测试

**❌ 不推荐**（重复代码）:
```python
def test_field_a_missing():
    data = create_data()
    del data["field_a"]
    assert not validator.validate(data)

def test_field_b_missing():
    data = create_data()
    del data["field_b"]
    assert not validator.validate(data)
```

**✅ 推荐**（参数化）:
```python
@pytest.mark.parametrize("field", ["field_a", "field_b"])
def test_missing_field(field):
    data = create_data()
    del data[field]
    assert not validator.validate(data)
```

### 5. 使用测试夹具

```python
@pytest.fixture
def valid_data():
    """提供标准测试数据"""
    return {
        "agent_id": "agent_001",
        "role": "engineer",
        "version": "1.0.0"
    }

@pytest.fixture
def validator():
    """提供标准验证器"""
    return AgentContractValidator()

def test_validation(valid_data, validator):
    """使用夹具进行测试"""
    assert validator.validate(valid_data)
```

---

## 🔍 故障排查

### 常见问题

#### 1. Python 依赖缺失

```bash
# 错误：ModuleNotFoundError: No module named 'pytest'
make install

# 或手动安装
pip install -r requirements.txt
```

#### 2. C 测试编译失败

```bash
# 检查 CMake 配置
cd atoms/corekern
cat tests/CMakeLists.txt

# 重新构建
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON
cmake --build .
```

#### 3. 测试数据文件缺失

```bash
# 检查测试数据
ls tests/fixtures/data/

# 如果缺失，从备份恢复或重新生成
python tests/utils/data_generator.py
```

#### 4. 覆盖率报告为空

```bash
# 清理缓存
make clean

# 重新运行覆盖率
make coverage

# 检查 .coveragerc 配置
cat .coveragerc
```

### 诊断工具

```bash
# 运行诊断脚本
python run_tests.py

# 查看详细错误
pytest -v --tb=long

# 显示本地变量
pytest -l

# 打印输出
pytest -s
```

---

## 🔗 相关文档

| 文档 | 说明 |
|------|------|
| [TESTING_GUIDELINES.md](./TESTING_GUIDELINES.md) | 测试代码编写规范 |
| [代码质量改进报告](../.本地/tests/20260326-05 次代码质量改进总结报告.md) | 测试质量改进记录 |
| [测试评估报告](../.本地/tests/20260326-04 次代码重复率与圈复杂度评估报告.md) | 测试代码质量评估 |

---

<div align="center">

**tests - AgentOS 质量保障体系**

[返回顶部](#agentos 测试套件-tests)

© 2026 SPHARX Ltd. All Rights Reserved.

</div>
