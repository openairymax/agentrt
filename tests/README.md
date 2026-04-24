# 测试套件

`tests/`

## 概述

`tests/` 目录是 AgentOS 项目的集中测试套件，涵盖从底层内核到上层应用的完整测试体系。测试框架采用 C（CMockery2）和 Python（pytest）双语言实现，支持单元测试、集成测试、契约测试、性能基准测试和安全测试等多层次验证。

## 目录结构

```
tests/
├── unit/                 # 单元测试
│   ├── atoms/            # Atoms 层单元测试
│   ├── commons/          # Commons 层单元测试
│   ├── cupolas/          # Cupolas 层单元测试
│   └── daemon/           # Daemon 层单元测试
├── integration/          # 集成测试
│   ├── protocols/        # 协议交互测试
│   └── workflows/        # 工作流测试
├── contract/             # 契约测试
├── benchmark/            # 性能基准测试
├── security/             # 安全测试
├── fuzz/                 # 模糊测试
├── fixtures/             # 测试夹具和数据
├── conftest.py           # pytest 全局配置
└── run_tests.py          # 测试运行入口
```

## 测试层级

| 层级 | 语言 | 框架 | 目标 |
|------|------|------|------|
| 单元测试 | C / Python | CMockery2 / pytest | 验证单个函数或模块的正确性 |
| 集成测试 | Python | pytest | 验证多组件间的数据流和协议交互 |
| 契约测试 | Python | pytest | 确保接口契约的一致性 |
| 基准测试 | Python | pytest-benchmark | 性能指标监控与回归检测 |
| 安全测试 | Python | pytest | 权限、注入、XSS 等安全场景 |
| 模糊测试 | C / Python | libFuzzer / afl | 异常输入下的系统健壮性 |

## 快速开始

```bash
# 运行所有测试
python tests/run_tests.py

# 仅运行单元测试
python tests/run_tests.py --type unit

# 运行集成测试
python tests/run_tests.py --type integration

# 运行特定模块测试
python tests/run_tests.py --module atoms

# 生成测试报告
python tests/run_tests.py --report html
```

### 使用 pytest

```bash
# 运行所有测试
cd tests
pytest -v

# 运行特定标记的测试
pytest -v -m "unit"

# 运行性能基准测试
pytest -v -m "benchmark" --benchmark-only

# 并行运行测试
pytest -v -n auto
```

## 测试标记

| 标记 | 说明 |
|------|------|
| `unit` | 单元测试 |
| `integration` | 集成测试 |
| `contract` | 契约测试 |
| `benchmark` | 性能基准测试 |
| `security` | 安全测试 |
| `fuzz` | 模糊测试 |
| `slow` | 慢速测试（默认排除） |

## 架构说明

测试套件采用分布式架构设计：

- **测试编排服务器**：管理测试任务分发、结果汇总和报告生成
- **远程 Worker Agent**：部署在不同环境（Linux/macOS/Windows）上执行测试任务
- **测试镜像**：基于 Docker 的轻量级测试环境，确保环境一致性

此架构支持大规模并行测试，可同时在多种平台和配置上运行测试套件。

## 编写测试

### C 单元测试（CMockery2）

```c
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmockery.h>

static void test_example(void **state) {
    int result = function_under_test(42);
    assert_int_equal(result, EXPECTED_VALUE);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_example),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
```

### Python 测试（pytest）

```python
import pytest
from agentos.atoms.taskflow import TaskFlow

class TestTaskFlow:
    @pytest.fixture
    def taskflow(self):
        return TaskFlow()

    @pytest.mark.unit
    def test_create_task(self, taskflow):
        task = taskflow.create_task("test", priority=1)
        assert task.id is not None
        assert task.status == "pending"

    @pytest.mark.integration
    async def test_task_lifecycle(self, taskflow):
        task = await taskflow.submit("test")
        result = await taskflow.execute(task.id)
        assert result.status == "completed"
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
