Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
"From data intelligence emerges."

# AgentOS 测试编写规范

**版本**: V1.0  
**最后更新**: 2026-04-12  
**状态**: 🟢 生产就绪

---

## 🎯 概述

本文档定义 AgentOS 项目测试代码的编写规范，涵盖 Python 和 C 两种语言的测试标准。所有贡献者必须遵循本规范编写测试代码，确保测试套件的一致性、可维护性和有效性。

---

## 📐 测试金字塔

AgentOS 遵循测试金字塔模型：

```
        ╱  E2E  ╲           10% - 端到端测试
       ╱─────────╲          验证完整用户场景
      ╱ Integration ╲       20% - 集成测试
     ╱───────────────╲      验证模块间交互
    ╱    Unit Tests    ╲    70% - 单元测试
   ╱───────────────────╲   验证单个函数/类
```

### 覆盖率要求

| 测试类型 | 最低覆盖率 | 目标覆盖率 |
|---------|-----------|-----------|
| 单元测试 | 80% | 85%+ |
| 集成测试 | 70% | 80%+ |
| 契约测试 | 100% | 100% |
| 关键路径 | 100% | 100% |

---

## 🐍 Python 测试规范

### 框架与工具

| 工具 | 用途 | 版本要求 |
|------|------|---------|
| pytest | 测试框架 | >=7.0 |
| pytest-cov | 覆盖率 | >=4.0 |
| pytest-mock | Mock 支持 | >=3.0 |
| pytest-asyncio | 异步测试 | >=0.21 |
| pytest-timeout | 超时控制 | >=2.0 |
| hypothesis | 模糊测试 | >=6.0 |

### 文件命名

- 测试文件：`test_<module_name>.py`
- 测试夹具：`conftest.py`
- 测试数据：`fixtures/data/<module>/`
- 测试工具：`utils/<tool_name>.py`

### 类命名

```python
class Test<FeatureName>:
    """测试 <功能名称>"""
    pass

class Test<FeatureName><Aspect>:
    """测试 <功能名称> 的 <方面>"""
    pass
```

示例：
```python
class TestAgentLifecycle:
    pass

class TestAgentPermission:
    pass
```

### 函数命名

```python
def test_<function>_<scenario>_<expected_result>(self):
    pass
```

示例：
```python
def test_agent_create_with_valid_config_returns_success(self):
    pass

def test_agent_create_with_null_config_raises_error(self):
    pass

def test_memory_query_with_empty_results_returns_empty_list(self):
    pass
```

### 测试结构（AAA 模式）

每个测试必须遵循 **Arrange-Act-Assert** 模式：

```python
def test_task_submit_with_valid_input_returns_task_id(self):
    # Arrange - 准备测试数据和环境
    config = {"name": "test_agent", "type": "TASK"}
    agent_id = create_test_agent(config)

    # Act - 执行被测操作
    result = agentos_task_submit(agent_id, "process data", 30000)

    # Assert - 验证结果
    assert result.error_code == AGENTOS_SUCCESS
    assert result.task_id is not None
    assert result.task_id.startswith("task_")
```

### Marker 使用

```python
@pytest.mark.unit
class TestMemoryWrite:
    pass

@pytest.mark.integration
class TestMemoryPersistence:
    pass

@pytest.mark.security
def test_input_sanitization_blocks_xss():
    pass

@pytest.mark.benchmark
def test_memory_query_performance():
    pass

@pytest.mark.slow
@pytest.mark.timeout(300)
def test_large_dataset_import():
    pass

@pytest.mark.contract
def test_agent_contract_required_fields():
    pass
```

### Fixture 使用

```python
# conftest.py 中定义共享 fixture
@pytest.fixture
def sample_agent_config():
    return {
        "name": "test_agent",
        "type": "TASK",
        "max_concurrent_tasks": 4
    }

@pytest.fixture
def mock_agentos_client(mocker):
    client = mocker.MagicMock()
    client.agent_create.return_value = {"agent_id": "agent_0"}
    return client

# 测试文件中使用
class TestAgentCreation:
    def test_create_with_valid_config(self, sample_agent_config, mock_agentos_client):
        result = mock_agentos_client.agent_create(sample_agent_config)
        assert result["agent_id"] == "agent_0"
```

### 异步测试

```python
@pytest.mark.asyncio
async def test_async_memory_query():
    result = await async_memory_client.query("test query", limit=5)
    assert len(result.records) <= 5

@pytest.mark.asyncio
async def test_concurrent_task_submission():
    tasks = [submit_task(f"task_{i}") for i in range(10)]
    results = await asyncio.gather(*tasks)
    assert all(r.error_code == AGENTOS_SUCCESS for r in results)
```

### 参数化测试

```python
@pytest.mark.parametrize("input_data,expected_status", [
    ("valid input", AGENTOS_SUCCESS),
    ("", AGENTOS_EINVAL),
    (None, AGENTOS_EINVAL),
    ("a" * 10001, AGENTOS_EINVAL),
])
def test_task_submit_input_validation(input_data, expected_status):
    result = agentos_task_submit(input_data, len(input_data) if input_data else 0, 30000)
    assert result == expected_status

@pytest.mark.parametrize("level,expected_min_severity", [
    ("TRACE", 0),
    ("DEBUG", 1),
    ("INFO", 2),
    ("WARN", 3),
    ("ERROR", 4),
    ("FATAL", 5),
])
def test_log_level_severity(level, expected_min_severity):
    assert log_level_to_severity(level) >= expected_min_severity
```

### Mock 最佳实践

```python
# 使用 pytest-mock（推荐）
def test_agent_invoke_with_mock(mocker):
    mock_execute = mocker.patch("agentos.cupolas.cupolas_execute_command")
    mock_execute.return_value = AGENTOS_SUCCESS

    result = agentos_agent_invoke("agent_0", "test input", 10, None)
    assert result == AGENTOS_SUCCESS
    mock_execute.assert_called_once()

# Mock 上下文管理器
def test_session_with_mock_persistence(mocker):
    mock_persist = mocker.patch(
        "agentos.syscall.session.persist_session_with_retry"
    )
    mock_persist.return_value = AGENTOS_SUCCESS

    session_id = agentos_session_create(None)
    mock_persist.assert_called_once()
```

### 断言规范

```python
# 推荐：使用明确的断言消息
assert result.error_code == AGENTOS_SUCCESS, \
    f"Expected SUCCESS, got {result.error_code}"

# 推荐：使用 pytest.approx 处理浮点数
assert result.score == pytest.approx(0.95, abs=0.01)

# 推荐：使用异常断言
with pytest.raises(ValueError, match="invalid config"):
    parse_agent_config("not json")

# 不推荐：使用裸 assert
assert result  # 不好，不明确

# 不推荐：使用 assertTrue/assertFalse（unittest 风格）
self.assertTrue(result.success)  # 不要在 pytest 中使用
```

---

## 🔧 C 测试规范

### 框架

使用 CMockery2 框架编写 C 单元测试。

### 文件命名

- 测试文件：`test_<module_name>.c`
- 测试头文件：`test_macros.h`（共享断言宏）

### 函数命名

```c
static void test_<function>_<scenario>(void **state);
```

示例：
```c
static void test_agent_spawn_with_valid_spec_returns_success(void **state);
static void test_agent_spawn_with_null_spec_returns_einval(void **state);
static void test_memory_write_with_valid_data_returns_record_id(void **state);
```

### 测试结构

```c
static void test_ipc_channel_create_with_valid_params(void **state) {
    // Arrange
    agentos_ipc_config_t config = {
        .type = IPC_CHANNEL_BUFFERED,
        .buffer_size = 4096
    };

    // Act
    agentos_ipc_channel_t* ch = agentos_ipc_channel_create(&config);

    // Assert
    assert_non_null(ch);
    assert_int_equal(agentos_ipc_channel_get_type(ch), IPC_CHANNEL_BUFFERED);

    // Cleanup
    agentos_ipc_channel_destroy(ch);
}
```

### 测试主函数

```c
int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_ipc_channel_create_with_valid_params),
        cmocka_unit_test(test_ipc_channel_create_with_null_config),
        cmocka_unit_test(test_ipc_channel_send_receive),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
```

### CMake 集成

```cmake
add_executable(test_ipc test_ipc.c)
target_link_libraries(test_ipc agentos_ipc cmocka)
add_test(NAME ipc CHANNEL COMMAND test_ipc)
```

---

## 🏗️ 测试层次规范

### 单元测试（tests/unit/）

**职责**：验证单个函数、类或模块的行为。

**规则**：
- 不依赖外部服务（数据库、网络、文件系统）
- 使用 Mock 隔离依赖
- 每个测试只验证一个行为
- 测试执行时间 < 100ms

### 集成测试（tests/integration/）

**职责**：验证模块间的交互和数据流。

**规则**：
- 可依赖本地服务（SQLite、临时文件系统）
- 测试跨模块的数据传递
- 验证接口契约的兼容性
- 测试执行时间 < 5s

### 契约测试（tests/contract/）

**职责**：验证 API 契约的完整性和一致性。

**规则**：
- 验证所有必填字段存在
- 验证字段类型正确
- 验证版本格式符合语义化版本规范
- 100% 覆盖所有契约字段

### 安全测试（tests/security/）

**职责**：验证安全防护措施的有效性。

**规则**：
- 覆盖 OWASP Top 10 攻击向量
- 验证输入净化（XSS、SQL 注入、命令注入）
- 验证权限控制（RBAC）
- 验证沙箱隔离

### 性能测试（tests/benchmarks/）

**职责**：验证系统性能指标。

**规则**：
- 使用 pytest-benchmark 或 C 计时器
- 记录基线数据用于回归检测
- 测试关键路径的延迟和吞吐量

---

## 📊 测试数据管理

### 测试数据位置

```
tests/fixtures/data/
├── memories/sample_memories.json
├── sessions/sample_sessions.json
├── skills/sample_skills.json
└── tasks/sample_tasks.json
```

### 数据生成

使用 `tests/utils/data_generator.py` 生成测试数据：

```python
from tests.utils.data_generator import TestDataFactory

def test_with_generated_data():
    task_data = TestDataFactory.create_task()
    memory_data = TestDataFactory.create_memory_entry()
    session_data = TestDataFactory.create_user_session()
```

### 数据清理

测试结束后必须清理所有临时数据：

```python
@pytest.fixture
def temp_agent(agentos_client):
    agent_id = agentos_client.create({"name": "temp"})
    yield agent_id
    agentos_client.destroy(agent_id)
```

---

## 🔒 测试隔离

### 环境隔离

```python
from tests.utils.test_isolation import isolated_test

@isolated_test
def test_with_isolated_environment():
    # 测试运行在独立环境中
    # 自动清理环境变量、临时文件、Mock 补丁
    pass
```

### 并行安全

```python
@pytest.mark.parallel_safe
def test_read_only_operation():
    # 只读操作可安全并行执行
    pass

@pytest.mark.sequential_only
def test_write_operation():
    # 写操作必须顺序执行
    pass
```

---

## ✅ 检查清单

每个测试文件提交前必须通过以下检查：

- [ ] 文件命名符合规范
- [ ] 类/函数命名符合规范
- [ ] 遵循 AAA 模式
- [ ] 使用正确的 pytest marker
- [ ] 无硬编码路径或端口
- [ ] 测试数据使用 fixture 或工厂生成
- [ ] 异步测试使用 `@pytest.mark.asyncio`
- [ ] 浮点数使用 `pytest.approx`
- [ ] Mock 使用 pytest-mock
- [ ] 测试执行时间在限制内
- [ ] 无跳过测试（除非有明确原因和 TODO）
- [ ] 覆盖率满足最低要求

---

## 📚 相关文档

| 文档 | 描述 |
|------|------|
| [测试 README](./README.md) | 测试套件总览 |
| [架构设计原则](../../docs/ARCHITECTURAL_PRINCIPLES.md) | 五维正交设计体系 |
| [编码规范](../../docs/Capital_Specifications/coding_standard/Python_coding_style_standard.md) | Python 编码规范 |
| [C 编码规范](../../docs/Capital_Specifications/coding_standard/C_coding_style_standard.md) | C 编码规范 |

---

**最后更新**: 2026-04-12  
**维护者**: AgentOS 质量保障团队

---

© 2026 SPHARX Ltd. All Rights Reserved.  
*"From data intelligence emerges."*
