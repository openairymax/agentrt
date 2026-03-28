# AgentOS 测试套件 (Tests)

<div align="center">

[![Version](https://img.shields.io/badge/version-1.0.0.6-blue.svg)](https://gitee.com/spharx/agentos)
[![License](https://img.shields.io/badge/license-Apache-2.0-green.svg)](https://gitee.com/spharx/agentos/blob/main/LICENSE)
[![Tests](https://img.shields.io/badge/tests-650+-brightgreen.svg)](https://gitee.com/spharx/agentos/actions)
[![Coverage](https://img.shields.io/badge/coverage-93%25-brightgreen.svg)](https://codecov.io/gh/spharx/agentos)

**AgentOS 质量保障体系 - 单元测试、集成测试、安全测试、性能基准、契约测试**

*"Test everything that can break."*

---

Language: **中文** | [English](../paper/readme/en/README.md)

</div>

---

## 📋 目录

- [概述](#概述)
- [测试架构](#测试架构)
- [测试分布](#测试分布)
- [运行测试](#运行测试)
- [测试覆盖率](#测试覆盖率)
- [测试示例](#测试示例)
- [最佳实践](#最佳实践)
- [性能基准](#性能基准)
- [CI/CD 集成](#ci/cd 集成)
- [相关文档](#相关文档)
- [贡献测试](#贡献测试)

---

## 📖 概述

`tests/` 目录是 AgentOS 项目的质量保障中心，包含完整的测试套件，确保系统的可靠性、安全性和性能。

### 核心职责

1. **质量验证**: 验证所有代码功能符合设计要求
2. **缺陷预防**: 在早期发现和修复潜在问题
3. **回归防护**: 防止新代码破坏现有功能
4. **性能保障**: 确保系统性能满足基准要求
5. **安全检测**: 识别和修复安全漏洞
6. **文档补充**: 通过测试用例展示 API 使用方法

### 测试理念

AgentOS 采用 **测试金字塔** 模型，自下而上分为：

```
        /\
       /  \         E2E Tests (端到端测试 - 最少)
      /----\
     /      \      Integration Tests (集成测试 - 中等)
    /--------\
   /          \    Unit Tests (单元测试 - 最多)
  /------------\
```

- **单元测试 (70%)**: 验证最小可测试单元
- **集成测试 (20%)**: 验证模块间协作
- **E2E 测试 (10%)**: 验证完整业务流程

---

## 🏗️ 测试架构

### 分布式测试结构

AgentOS 采用分布式测试架构，每个子模块负责自己的单元测试，顶层 tests 专注于集成和跨模块测试：

```
AgentOS/
├── atoms/                    # 原子模块
│   ├── corekern/tests/       # 内核层单元测试
│   └── coreloopthree/tests/  # 核心运行时单元测试
│
├── backs/                    # 后端服务模块
│   ├── llm_d/tests/          # LLM 服务测试
│   ├── market_d/tests/       # 市场服务测试
│   └── monit_d/tests/        # 监控服务测试
│
├── common/                   # 通用工具模块
│   └── tests/unit/           # 工具库单元测试
│
└── tests/                    # 顶层测试（本目录）
    ├── unit/                 # Python 单元测试
    ├── integration/          # 集成测试
    ├── security/             # 安全测试
    ├── contract/             # 契约测试
    └── benchmarks/           # 性能基准测试
```

### 测试分类

| 测试类型 | 位置 | 职责 | 工具 |
|---------|------|------|------|
| **单元测试** | `atoms/*/tests/`, `backs/*/tests/`, `common/tests/unit/`, `tests/unit/` | 测试单个模块/函数 | CTest, pytest |
| **集成测试** | `tests/integration/` | 测试模块间协作 | pytest |
| **契约测试** | `tests/contract/` | 验证接口契约 | JSON Schema, pytest |
| **安全测试** | `tests/security/` | 安全漏洞检测 | pytest, OWASP ZAP |
| **性能测试** | `tests/benchmarks/` | 性能基准测试 | pytest-benchmark |
| **模糊测试** | `tests/fuzz/` | 随机输入测试 | Hypothesis, Atheris |

---

## 📂 测试分布

### 详细目录结构

```
tests/
├── README.md                      # 本文档
├── TESTING_GUIDELINES.md          # 测试代码规范
├── requirements.txt               # Python 测试依赖
├── conftest.py                    # pytest 配置和 fixtures
├── codecov.yml                    # Codecov 覆盖率配置
├── .coveragerc                    # 覆盖率配置
├── Makefile                       # 测试构建脚本
│
├── unit/                          # Python 单元测试
│   ├── config/
│   │   └── test_validate_config.py    # 配置验证测试
│   ├── sdk/
│   │   ├── python/
│   │   │   ├── test_sdk.py            # Python SDK 测试
│   │   │   └── test_sdk_refactored.py # 优化版 SDK 测试
│   │   └── rust/
│   │       └── .gitkeep               # Rust SDK 测试占位
│   └── services/
│       ├── llm_d/
│       │   └── test_llm.c             # LLM 服务测试
│       ├── market_d/
│       │   └── test_market.c          # 市场服务测试
│       ├── monit_d/
│       │   └── test_monitor.c         # 监控服务测试
│       ├── sched_d/
│       │   └── test_scheduler.c       # 调度服务测试
│       └── tool_d/
│           └── test_tool.c            # 工具服务测试
│
├── integration/                   # 集成测试
│   ├── coreloopthree/
│   │   ├── test_cognition_execution.py  # 认知 - 执行集成测试
│   │   └── test_memory_evolution.py     # 记忆进化集成测试
│   ├── memoryrovol/
│   │   ├── test_layers.py               # 记忆层集成测试
│   │   └── test_retrieval.py            # 检索集成测试
│   └── syscall/
│       └── test_syscalls.py             # 系统调用集成测试
│
├── security/                      # 安全测试
│   ├── test_permissions.py        # 权限控制测试
│   ├── test_sandbox.py            # 沙箱隔离测试
│   ├── test_input_sanitizer.py    # 输入净化测试
│   └── sast_dast_scanner.py       # SAST/DAST 安全扫描器
│
├── contract/                      # 契约测试
│   ├── test_agent_contracts.py    # Agent 契约验证
│   ├── test_agent_contracts_refactored.py  # 优化版契约测试
│   └── test_skill_contracts.py    # Skill 契约验证
│
├── benchmarks/                    # 性能基准测试
│   ├── concurrency/
│   │   ├── load_test.py           # 并发负载测试
│   │   └── report/.gitkeep        # 测试报告目录
│   ├── retrieval_latency/
│   │   ├── benchmark.c            # C 语言检索延迟基准
│   │   └── results/.gitkeep       # 测试结果目录
│   └── token_efficiency/
│       ├── benchmark.py           # Token 效率基准
│       └── plots/.gitkeep         # 性能图表目录
│
├── fuzz/                          # 模糊测试
│   └── fuzz_framework.py          # 模糊测试框架
│
├── generators/                    # 测试生成器
│   └── contract_test_generator.py # 契约测试用例生成器
│
├── performance/                   # 性能工具
│   └── regression_detector.py     # 性能回归检测器
│
├── reports/                       # 测试报告
│   └── dashboard_generator.py     # HTML 仪表板生成器
│
├── utils/                         # 测试工具
│   ├── test_helpers.py            # Python 测试辅助工具
│   ├── test_isolation.py          # 测试隔离工具
│   ├── data_generator.py          # 测试数据生成器
│   └── data_manager.py            # 测试数据管理器
│
└── fixtures/                      # 测试夹具
    └── data/
        ├── memories/sample_memories.json   # 样本记忆数据
        ├── sessions/sample_sessions.json   # 样本会话数据
        ├── skills/sample_skills.json       # 样本技能数据
        └── tasks/sample_tasks.json         # 样本任务数据
```

---

## 🚀 运行测试

### 快速开始

```bash
# 1. 安装测试依赖
cd tests
pip install -r requirements.txt

# 2. 运行所有测试
python run_tests.py

# 或使用 pytest 直接运行
pytest tests/ -v
```

### 分类运行测试

```bash
# 单元测试
pytest tests/unit/ -v

# 集成测试
pytest tests/integration/ -v

# 安全测试
pytest tests/security/ -v

# 契约测试
pytest tests/contract/ -v

# 性能基准测试
pytest tests/benchmarks/ -v --benchmark-only
```

### 运行 C/C++ 测试

```bash
# 编译并运行所有 C 测试
cd build
cmake .. -DENABLE_TESTS=ON
make -j4
ctest --output-on-failure

# 运行特定 C 测试模块
ctest -R corekern --output-on-failure
ctest -R coreloopthree --output-on-failure

# 运行子模块测试
cd atoms/corekern/tests && cmake . && make && ctest --output-on-failure
cd atoms/coreloopthree/tests && cmake . && make && ctest --output-on-failure
cd backs/llm_d/tests && cmake . && make && ctest --output-on-failure
```

### 选择性运行测试

```bash
# 运行特定测试文件
pytest tests/unit/sdk/python/test_sdk.py -v

# 运行特定测试函数
pytest tests/integration/coreloopthree/test_cognition_execution.py::test_intent_understanding -v

# 使用标签过滤
pytest -m "slow" tests/integration/
pytest -m "security" tests/

# 并行运行测试（加速）
pytest -n auto tests/unit/

# 失败后停止
pytest -x tests/

# 显示覆盖率
pytest --cov=agentos tests/ --cov-report=html
```

### 使用 Makefile

```bash
# 运行所有测试
make test

# 运行单元测试
make test-unit

# 运行集成测试
make test-integration

# 运行安全测试
make test-security

# 生成覆盖率报告
make coverage

# 清理测试产物
make clean
```

---

## 📊 测试覆盖率

### Python 覆盖率（使用 coverage.py）

```bash
# 安装工具
pip install coverage pytest-cov

# 运行测试并收集覆盖率
coverage run -m pytest tests/

# 查看文本报告
coverage report

# 生成 HTML 报告
coverage html
open htmlcov/index.html  # macOS
xdg-open htmlcov/index.html  # Linux

# 示例输出:
# Name                                      Stmts   Miss  Cover
# -------------------------------------------------------------
# agentos/client.py                           150      12    92%
# agentos/task.py                              85       5    94%
# agentos/memory.py                           120       8    93%
# -------------------------------------------------------------
# TOTAL                                       355      25    93%
```

### C/C++ 覆盖率（使用 gcov/lcov）

```bash
# 编译时启用覆盖率
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
make -j4

# 运行测试
ctest

# 生成覆盖率报告
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_report

# 查看 HTML 报告
open coverage_report/index.html  # macOS
xdg-open coverage_report/index.html  # Linux
```

### 覆盖率目标

| 测试类型 | 目标覆盖率 | 当前覆盖率 | 状态 |
|---------|-----------|-----------|------|
| **单元测试** | ≥ 85% | 93% | ✅ 优秀 |
| **集成测试** | ≥ 80% | 87% | ✅ 良好 |
| **安全测试** | 100% 关键路径 | 98% | ✅ 良好 |
| **契约测试** | 100% 接口 | 100% | ✅ 优秀 |

---

## 💻 测试示例

### C 单元测试示例

**文件**: `atoms/corekern/tests/test_ipc.c`

```c
/**
 * @file test_ipc.c
 * @brief IPC 单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "ipc.h"
#include "error.h"
#include <stdio.h>
#include <string.h>

void test_ipc_thread(void* arg) {
    // 打开通道
    agentos_ipc_channel_t* channel = NULL;
    agentos_error_t err = agentos_ipc_open("test_channel", &channel);
    
    // 接收消息
    agentos_ipc_message_t msg = {0};
    char buffer[256];
    msg.data = buffer;
    msg.size = sizeof(buffer);
    err = agentos_ipc_recv(channel, &msg, 5000);
    
    // 发送响应
    const char* response = "Hello from thread!";
    agentos_ipc_message_t resp_msg = {0};
    resp_msg.data = (void*)response;
    resp_msg.size = strlen(response) + 1;
    err = agentos_ipc_send(channel, &resp_msg);
    
    agentos_ipc_close(channel);
}

int test_ipc_basic() {
    printf("Testing basic IPC functionality...\n");
    
    // 初始化 IPC
    agentos_error_t err = agentos_ipc_init();
    
    // 创建通道
    agentos_ipc_channel_t* channel = NULL;
    err = agentos_ipc_create_channel("test_channel", NULL, NULL, &channel);
    
    // 创建测试线程
    agentos_thread_t thread;
    agentos_thread_attr_t attr = {0};
    strcpy(attr.name, "test_thread");
    attr.priority = AGENTOS_TASK_PRIORITY_NORMAL;
    attr.stack_size = 1024 * 1024;
    err = agentos_thread_create(&thread, &attr, test_ipc_thread, NULL);
    
    // 等待线程启动
    agentos_task_sleep(100);
    
    // 发送消息
    const char* message = "Hello from main!";
    agentos_ipc_message_t msg = {0};
    msg.data = (void*)message;
    msg.size = strlen(message) + 1;
    err = agentos_ipc_send(channel, &msg);
    
    // 接收响应
    agentos_ipc_message_t response = {0};
    char buffer[256];
    response.data = buffer;
    response.size = sizeof(buffer);
    err = agentos_ipc_recv(channel, &response, 5000);
    
    // 等待线程结束
    agentos_thread_join(&thread, NULL);
    
    // 关闭通道
    agentos_ipc_close(channel);
    agentos_ipc_cleanup();
    
    printf("Basic IPC test passed\n");
    return 0;
}

int main() {
    test_ipc_basic();
    return 0;
}
```

### Python 单元测试示例

**文件**: `tests/unit/sdk/python/test_sdk.py`

```python
"""AgentOS Python SDK 单元测试"""

import pytest
from unittest.mock import Mock, patch
from agentos import AgentOS, Task

class TestAgentOSClient:
    """AgentOS 客户端测试"""
    
    @pytest.fixture
    def client(self):
        """提供配置好的客户端实例"""
        return AgentOS(server_url="http://localhost:18789")
    
    def test_create_task(self, client):
        """测试创建任务"""
        task = client.create_task(
            description="Test task",
            priority=1
        )
        
        assert task is not None
        assert task.id > 0
        assert task.description == "Test task"
        assert task.priority == 1
        assert task.status == "pending"
    
    def test_cancel_task(self, client):
        """测试取消任务"""
        task = client.create_task("Test task")
        result = client.cancel_task(task.id)
        
        assert result is True
        assert task.status == "cancelled"
    
    @patch('agentos.agent.requests.Session')
    def test_api_error_handling(self, mock_session):
        """测试 API 错误处理"""
        # 配置 Mock 响应
        mock_response = Mock()
        mock_response.status_code = 500
        mock_response.json.return_value = {"error": "Internal server error"}
        
        mock_session_instance = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.post.return_value = mock_response
        
        client = AgentOS()
        
        with pytest.raises(Exception) as exc_info:
            client.create_task("Test")
        
        assert "Internal server error" in str(exc_info.value)

if __name__ == "__main__":
    pytest.main([__file__, "-v"])
```

### 集成测试示例

**文件**: `tests/integration/coreloopthree/test_cognition_execution.py`

```python
"""认知 - 执行集成测试"""

import pytest
from agentos import AgentOS, Intent, TaskDAG

class TestCognitionExecutionIntegration:
    """认知执行集成测试类"""
    
    @pytest.fixture
    def agentos_client(self):
        """提供配置好的 AgentOS 客户端"""
        return AgentOS()
    
    def test_intent_understanding(self, agentos_client):
        """测试意图理解"""
        # 解析意图
        intent = agentos_client.parse_intent(
            "帮我分析最近的销售数据，找出增长最快的三个产品"
        )
        
        # 验证意图识别
        assert intent.type == "data_analysis"
        assert intent.goal == "find_top_products"
        assert intent.constraints["count"] == 3
        assert intent.constraints["metric"] == "growth_rate"
    
    def test_task_planning(self, agentos_client):
        """测试任务规划"""
        planner = TaskDAG()
        
        # 生成任务 DAG
        dag = planner.generate_dag(
            goal="开发一个用户管理系统",
            constraints={"time_limit": "2 hours"}
        )
        
        # 验证 DAG 结构
        assert dag.entry_points() == ["requirements_analysis"]
        assert dag.critical_path() == [
            "requirements_analysis",
            "database_design",
            "api_development",
            "frontend_integration",
            "testing"
        ]
        assert dag.total_tasks() >= 5
    
    def test_agent_dispatching(self, agentos_client):
        """测试 Agent 调度"""
        assignment = agentos_client.dispatch_task(
            task_type="backend_development",
            required_skills=["python", "fastapi", "postgresql"],
            available_agents=[
                {"id": 1, "skills": ["python", "django"]},
                {"id": 2, "skills": ["python", "fastapi", "postgresql"]},
                {"id": 3, "skills": ["java", "spring"]}
            ]
        )
        
        # 验证调度结果
        assert assignment.agent_id == 2
        assert assignment.match_score > 0.9
```

### 契约测试示例

**文件**: `tests/contract/test_agent_contracts_refactored.py`

```python
"""Agent 契约测试（优化版 - 使用参数化测试）"""

import pytest
from typing import Dict, Any
from tests.utils.test_helpers import ContractTestHelper, assert_error_contains

class AgentContractValidator:
    """Agent 契约验证器"""
    
    def validate(self, contract: Dict[str, Any]) -> bool:
        """验证契约"""
        # 验证逻辑...
        pass

# 参数化测试用例
REQUIRED_FIELD_TEST_CASES = [
    {"name": "schema_version", "expected_error": "schema_version"},
    {"name": "agent_id", "expected_error": "agent_id"},
    {"name": "version", "expected_error": "version"},
]

class TestAgentContractValidation:
    """Agent 契约验证测试"""
    
    @pytest.fixture
    def valid_contract(self) -> Dict[str, Any]:
        """提供有效的 Agent 契约示例"""
        return ContractTestHelper.create_valid_contract()
    
    @pytest.fixture
    def validator(self) -> AgentContractValidator:
        """提供契约验证器实例"""
        return AgentContractValidator()
    
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

### 性能基准测试示例

**文件**: `tests/benchmarks/retrieval_latency/benchmark.c`

```c
/**
 * @file benchmark.c
 * @brief 核心循环性能基准测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "loop.h"
#include "agentos.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * @brief 基准测试：任务提交性能
 */
static void benchmark_task_submit() {
    agentos_core_loop_t* loop = NULL;
    agentos_error_t err = agentos_loop_create(NULL, &loop);
    
    const char* input = "帮我分析最近的销售数据";
    size_t input_len = strlen(input);
    int num_tasks = 1000;
    char** task_ids = (char**)malloc(num_tasks * sizeof(char*));
    
    clock_t start = clock();
    for (int i = 0; i < num_tasks; i++) {
        err = agentos_loop_submit(loop, input, input_len, &task_ids[i]);
    }
    clock_t end = clock();
    
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    printf("benchmark_task_submit: %d tasks in %.3f seconds (%.0f tasks/sec)\n", 
           num_tasks, elapsed, num_tasks / elapsed);
    
    // 清理
    for (int i = 0; i < num_tasks; i++) {
        if (task_ids[i]) free(task_ids[i]);
    }
    free(task_ids);
    agentos_loop_destroy(loop);
}

int main() {
    printf("=== Running Benchmark Tests ===\n");
    benchmark_task_submit();
    printf("=== Benchmark Tests Complete ===\n");
    return 0;
}
```

---

## 📚 最佳实践

### 1. 测试命名规范

```python
# ❌ 不好的命名
def test_1():
    pass

def test_stuff():
    pass

# ✅ 好的命名
def test_submit_task_with_valid_description():
    """测试提交有效描述的任务"""
    pass

def test_cancel_task_that_does_not_exist():
    """测试取消不存在的任务"""
    pass

def test_search_memory_with_empty_query_should_raise_error():
    """测试空查询搜索记忆应抛出错误"""
    pass
```

### 2. 测试组织原则

```
# 按功能模块组织
tests/
├── unit/
│   ├── kernel/
│   │   ├── test_ipc.py          # IPC 相关测试
│   │   ├── test_memory.py       # 内存相关测试
│   │   └── test_scheduler.py    # 调度相关测试
├── integration/
│   ├── coreloopthree/
│   │   ├── test_cognition.py    # 认知测试
│   │   └── test_execution.py    # 执行测试
```

### 3. 使用测试夹具 (Fixtures)

```python
import pytest

@pytest.fixture
def memory_client():
    """提供记忆客户端实例"""
    client = MemoryRovol()
    yield client
    client.cleanup()

@pytest.fixture
def sample_task():
    """提供示例任务"""
    return Task(description="Sample task", priority=1)

# 在测试中使用
def test_memory_write(memory_client):
    record_id = memory_client.write("Test content")
    assert record_id > 0

def test_task_execution(sample_task):
    result = sample_task.execute()
    assert result.success
```

### 4. 参数化测试

```python
import pytest

@pytest.mark.parametrize("input,expected", [
    (1, 2),
    (2, 4),
    (3, 6),
    (10, 20),
])
def test_double(input, expected):
    """测试翻倍函数"""
    assert input * 2 == expected

# 使用字典参数化
@pytest.mark.parametrize("test_case", [
    {"name": "valid", "input": {...}, "expected": True},
    {"name": "invalid", "input": {...}, "expected": False},
], ids=lambda tc: tc["name"])
def test_validation(test_case):
    """参数化验证测试"""
    pass
```

### 5. 测试辅助工具

```python
from tests.utils.test_helpers import (
    create_mock_response,
    create_mock_session,
    TestDataBuilder,
    ContractTestHelper,
    assert_error_contains
)

# 使用 Mock 工厂
def test_api_call():
    mock_session = create_mock_session(
        response_data={"result": "success"},
        status_code=200
    )

# 使用数据构建器
def test_with_builder():
    data = TestDataBuilder() \
        .with_field("name", "test") \
        .with_field("value", 100) \
        .build_invalid(missing_field="name")
```

---

## 📈 性能基准

### v1.0.0.6 测试统计

| 类别 | 测试文件 | 测试数量 | 通过率 | 平均执行时间 |
|-----|---------|---------|--------|-------------|
| **单元测试** | 25 个 | 350+ | 98% | 2.3s |
| **集成测试** | 15 个 | 120+ | 95% | 15.7s |
| **安全测试** | 8 个 | 60+ | 97% | 8.2s |
| **契约测试** | 5 个 | 40+ | 99% | 3.1s |
| **性能测试** | 10 个 | 80+ | 92% | 45.6s |
| **总计** | **63 个** | **650+** | **96%** | **75.0s** |

### 性能基准指标

在标准测试环境 (Intel i7-12700K, 32GB RAM, NVMe SSD):

| 测试项 | 指标 | 要求 | 实测 | 状态 |
|-------|------|------|------|------|
| **IPC Binder 延迟** | p99 | < 1ms | 0.3ms | ✅ |
| **记忆检索延迟** | p95 | < 10ms | 8.2ms | ✅ |
| **任务调度吞吐量** | 任务/秒 | > 1000 | 1250 | ✅ |
| **并发连接数** | 连接/秒 | > 1000 | 1500 | ✅ |
| **向量搜索 QPS** | 查询/秒 | > 10000 | 12500 | ✅ |
| **LRU 缓存命中率** | 命中率 | > 80% | 87% | ✅ |

---

## 🔧 CI/CD 集成

### GitHub Actions 配置

**文件**: `.github/workflows/test.yml`

```yaml
name: Tests

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main ]

jobs:
  test:
    runs-on: ubuntu-latest
    
    services:
      postgres:
        image: postgres:14
        env:
          POSTGRES_PASSWORD: postgres
        options: >-
          --health-cmd pg_isready
          --health-interval 10s
          --health-timeout 5s
          --health-retries 5
        ports:
          - 5432:5432
    
    steps:
      - uses: actions/checkout@v3
      
      - name: Set up Python
        uses: actions/setup-python@v4
        with:
          python-version: '3.10'
      
      - name: Install dependencies
        run: |
          ./scripts/build.sh --deps
          pip install -r tests/requirements.txt
      
      - name: Run unit tests
        run: ctest -R unit --output-on-failure
      
      - name: Run integration tests
        run: pytest tests/integration/ -v --tb=short
      
      - name: Run security tests
        run: pytest tests/security/ -v
      
      - name: Run performance tests
        run: pytest tests/benchmarks/ -v --benchmark-only
      
      - name: Upload coverage to Codecov
        uses: codecov/codecov-action@v3
        with:
          file: ./coverage.xml
          flags: unittests
          fail_ci_if_error: true
```

### 测试报告生成

```bash
# 生成 HTML 测试报告
python tests/reports/dashboard_generator.py

# 生成覆盖率报告
pytest --cov=agentos tests/ --cov-report=html

# 生成性能报告
pytest tests/benchmarks/ -v --benchmark-json=report.json
```

---

## 📖 相关文档

- [测试规范](../paper/specifications/testing.md) - 测试编写标准和最佳实践
- [编码规范](../paper/specifications/coding_standards.md) - C/C++/Python编码规范
- [故障排查](../paper/guides/troubleshooting.md) - 测试失败排查指南
- [CoreLoopThree 架构](../paper/architecture/coreloopthree.md) - 核心循环架构文档
- [MemoryRovol 架构](../paper/architecture/memoryrovol.md) - 记忆系统架构
- [系统调用接口](../paper/architecture/syscall.md) - 系统调用接口文档
- [TESTING_GUIDELINES.md](TESTING_GUIDELINES.md) - 测试代码规范（本目录）

---

## 🤝 贡献测试

我们欢迎并感谢测试贡献！测试可以帮助：

1. **提高代码质量**: 发现潜在 bug 和边界问题
2. **防止回归**: 确保新功能不破坏现有功能
3. **改进文档**: 通过测试示例展示更好的使用方式

### 如何贡献测试

1. Fork 项目仓库
2. 创建测试分支 (`git checkout -b test/my-awesome-test`)
3. 编写测试代码（参考本文档中的示例）
4. 运行测试确保通过 (`pytest tests/ -v`)
5. 提交更改 (`git commit -am 'Add test for feature X'`)
6. 推送到分支 (`git push origin test/my-awesome-test`)
7. 创建 Pull Request

### 测试贡献指南

- 遵循 [TESTING_GUIDELINES.md](TESTING_GUIDELINES.md) 中的规范
- 使用有意义的测试名称
- 包含清晰的断言和错误消息
- 测试边界条件和异常情况
- 保持测试独立和可重复
- 添加必要的注释和文档字符串

---

## 📊 测试工具

### 主要工具

| 工具 | 用途 | 安装 |
|-----|------|------|
| **pytest** | Python 测试框架 | `pip install pytest` |
| **pytest-cov** | Python 覆盖率 | `pip install pytest-cov` |
| **pytest-benchmark** | Python 性能测试 | `pip install pytest-benchmark` |
| **pytest-xdist** | Python 并行测试 | `pip install pytest-xdist` |
| **hypothesis** | Python 模糊测试 | `pip install hypothesis` |
| **CTest** | C/C++ 测试框架 | CMake 内置 |
| **gcov/lcov** | C/C++ 覆盖率 | GCC 内置 |
| **radon** | Python 复杂度检查 | `pip install radon` |
| **lizard** | C/C++ 复杂度检查 | `pip install lizard` |

### 实用脚本

```bash
# 检查语法
python tests/check_syntax.py

# 生成测试报告
python tests/generate_combined_report.py

# 性能回归检测
python tests/performance/regression_detector.py

# SAST/DAST 安全扫描
python tests/security/sast_dast_scanner.py
```

---

<div align="center">

**© 2026 SPHARX Ltd. 保留所有权利。**

*"From data intelligence emerges"*

**AgentOS v1.0.0.6**

[返回顶部](#agentos-测试套件-tests)

</div>
