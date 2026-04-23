# 测试指南

**版本**: v2.2.0  
**最后更新**: 2026-04-22

---

## 📋 概述

本指南介绍如何高效使用 AgentOS 测试框架，包括测试编写、运行、调试和报告。

---

## 🏗️ 测试架构

AgentOS 采用**双层测试架构**：

1. **模块自测层** (`agentos/*/tests/`)
   - C/C++ 单元测试，与源码相邻
   - 使用 CMockery2 框架
   - 通过 CMake/CTest 运行

2. **集中测试层** (`tests/`)
   - Python 集成/契约/性能测试
   - 使用 pytest 框架
   - 通过 `run_tests.py` 运行

详细架构说明见 [ARCHITECTURE.md](ARCHITECTURE.md)

---

## 🚀 快速开始

### 安装依赖

```bash
cd tests
pip install -r requirements.txt        # 基本依赖
pip install -r requirements-dev.txt    # 开发依赖（推荐）
```

### 运行测试

```bash
# 运行所有 Python 测试
python run_tests.py

# 使用 pytest 直接运行
pytest unit/ -v              # 单元测试
pytest integration/ -v       # 集成测试
pytest security/ -v          # 安全测试
pytest benchmarks/ -v        # 性能测试

# 带覆盖率
pytest --cov=agentos --cov-report=html

# 并行运行（需要 pytest-xdist）
pytest -n auto

# 运行特定标记的测试
pytest -m "unit and not slow"
```

---

## 📝 编写测试

### Python 单元测试

```python
import pytest
from agentos.core import Agent

class TestAgent:
    """Agent 单元测试"""

    def test_agent_creation(self):
        """测试 Agent 创建"""
        agent = Agent(name="TestAgent")
        assert agent.name == "TestAgent"
        assert agent.status == "inactive"

    def test_agent_activation(self):
        """测试 Agent 激活"""
        agent = Agent(name="TestAgent")
        agent.activate()
        assert agent.status == "active"

    @pytest.mark.parametrize("name", ["Agent1", "Agent2", "Agent3"])
    def test_multiple_agents(self, name):
        """参数化测试"""
        agent = Agent(name=name)
        assert agent.name == name
```

### 使用测试工具

```python
from tests.utils.test_helpers import TestDataGenerator, MockFactory

def test_with_mock():
    """使用 Mock 对象"""
    mock_response = MockFactory.create_mock_response(
        status_code=200,
        json_data={"status": "ok", "data": "test"}
    )
    assert mock_response.status_code == 200
    assert mock_response.json()["status"] == "ok"

def test_with_fixture_data():
    """使用夹具数据"""
    from tests.utils.test_helpers import load_fixture
    task_data = load_fixture("tasks/sample_tasks.json")
    assert len(task_data) > 0
```

### 性能测试

```python
import pytest
from tests.utils.test_helpers import PerformanceTester

def test_performance():
    """性能测试"""
    with PerformanceTester("my_test") as pt:
        # 执行被测代码
        result = expensive_operation()

    assert pt.elapsed_ms < 100  # 小于 100ms
    assert result is not None
```

---

## 🏷️ 测试标记

使用 pytest markers 组织测试：

```python
@pytest.mark.unit
def test_basic_functionality():
    """单元测试"""
    pass

@pytest.mark.integration
def test_module_interaction():
    """集成测试"""
    pass

@pytest.mark.slow
def test_large_dataset():
    """慢速测试（>5秒）"""
    pass

@pytest.mark.security
def test_input_validation():
    """安全测试"""
    pass

@pytest.mark.benchmark
def test_performance():
    """性能基准测试"""
    pass

@pytest.mark.skip(reason="暂时跳过")
def test_temporarily_skipped():
    pass

@pytest.mark.xfail
def test_known_failure():
    """预期失败的测试"""
    pass
```

---

## 🔧 调试技巧

### 查看详细输出

```bash
# 详细输出
pytest -v

# 显示打印内容
pytest -s

# 显示局部变量
pytest --tb=long

# 进入调试器
pytest --pdb
```

### 运行特定测试

```bash
# 运行特定文件
pytest tests/unit/atoms/test_corekern.py

# 运行特定类
pytest tests/unit/atoms/test_corekern.py::TestCoreKern

# 运行特定方法
pytest tests/unit/atoms/test_corekern.py::TestCoreKern::test_memory

# 使用关键词过滤
pytest -k "memory"
```

---

## 📊 覆盖率报告

```bash
# 生成覆盖率报告
pytest --cov=agentos --cov-report=html

# 查看 HTML 报告
open coverage_html/index.html  # macOS
xdg-open coverage_html/index.html  # Linux
start coverage_html/index.html  # Windows

# 生成 XML 报告（用于 CI）
pytest --cov=agentos --cov-report=xml

# 设置覆盖率阈值
pytest --cov=agentos --cov-fail-under=85
```

---

## 🎯 最佳实践

### 1. 测试命名

- 文件名: `test_<模块名>.py`
- 类名: `Test<类名>`
- 方法名: `test_<场景>_<预期结果>`

```python
class TestMemoryManager:
    def test_store_and_retrieve_memory(self):
        pass

    def test_store_invalid_memory_raises_error(self):
        pass
```

### 2. 夹具使用

```python
import pytest

@pytest.fixture
def sample_agent():
    return Agent(name="TestAgent")

@pytest.fixture
def mock_llm_service():
    return MockLLMService()

def test_agent_with_llm(sample_agent, mock_llm_service):
    sample_agent.set_llm(mock_llm_service)
    result = sample_agent.process("Hello")
    assert result is not None
```

### 3. 测试隔离

```python
@pytest.fixture(autouse=True)
def clean_state():
    """每个测试前清理状态"""
    Agent.reset_all()
    yield
    Agent.cleanup()
```

### 4. 参数化测试

```python
@pytest.mark.parametrize("input,expected", [
    ("hello", 5),
    ("world!", 6),
    ("", 0),
])
def test_string_length(input, expected):
    assert len(input) == expected
```

---

## 📁 测试数据管理

### 夹具数据位置

```
tests/fixtures/data/
├── tasks/           # 任务测试数据
├── memories/        # 记忆测试数据
├── sessions/        # 会话测试数据
└── skills/          # 技能测试数据
```

### 添加新测试数据

1. 在对应目录创建 JSON 文件
2. 使用 `load_fixture()` 加载：

```python
from tests.utils.test_helpers import load_fixture

data = load_fixture("tasks/new_test_data.json")
```

---

## 🔍 常见问题

### Q: 测试运行很慢怎么办？

```bash
# 跳过慢速测试
pytest -m "not slow"

# 并行运行
pytest -n 4

# 只运行单元测试
pytest -m unit
```

### Q: 如何调试失败的测试？

```bash
# 进入调试器
pytest --pdb

# 显示局部变量
pytest --tb=short -v

# 只运行上次失败的测试
pytest --lf
```

### Q: 覆盖率为什么很低？

- 确保测试覆盖了核心逻辑
- 使用 `pytest --cov-report=term-missing` 查看未覆盖行
- 添加更多边界条件测试

---

## 📚 参考资源

- [pytest 官方文档](https://docs.pytest.org/)
- [pytest-cov 文档](https://pytest-cov.readthedocs.io/)
- [CMockery2 文档](https://code.google.com/archive/p/cmockery2/)
- [ARCHITECTURE.md](ARCHITECTURE.md) - 测试架构说明

---

© 2026 SPHARX Ltd. All Rights Reserved.
