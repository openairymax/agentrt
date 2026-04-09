Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
"From data intelligence emerges."

# AgentOS 测试指南

**版本**: Doc V1.8
**最后更新**: 2026-04-09
**状态**: 生产就绪

---

## 📋 测试体系概览

AgentOS 采用**多层次测试策略**，确保代码质量和系统稳定性：

```
┌─────────────────────────────────────────────┐
│              E2E 测试 (端到端)                 │
│   完整用户场景验证 · 集成测试 · 性能基准        │
├─────────────────────────────────────────────┤
│              集成测试                         │
│   组件交互 · API 契约 · 数据库交互            │
├─────────────────────────────────────────────┤
│              单元测试                         │
│   函数/方法级别 · 模块隔离 · Mock             │
└─────────────────────────────────────────────┘
```

---

## 🧪 测试层级与工具

### 1. 单元测试 (Unit Tests)

**目标**: 验证单个函数/方法的正确性

| 语言 | 框架 | 位置 |
|------|------|------|
| C/C++ | CTest + Google Test | `tests/unit/c/` |
| Python | pytest + unittest.mock | `tests/unit/python/` |
| Go | testing package | `*_test.go` 文件 |
| Rust | cargo test | `tests/` 目录 |

#### Python 单元测试示例

```python
import pytest
from unittest.mock import Mock, patch, MagicMock
from agentos.memory import MemoryClient

class TestMemoryClientStore:
    """MemoryClient.store() 方法测试"""

    def test_store_success(self):
        """测试成功存储"""
        client = MemoryClient(base_url="http://test")

        with patch.object(client.session, 'post') as mock_post:
            mock_post.return_value.json.return_value = {
                "jsonrpc": "2.0",
                "result": {"record_id": "rec-001"},
                "id": "test"
            }

            result = client.store(data="test data", tags=["test"])

            assert result == "rec-001"
            mock_post.assert_called_once()

    def test_store_with_invalid_data(self):
        """测试无效数据"""
        client = MemoryClient(base_url="http://test")

        with pytest.raises(ValidationError) as exc_info:
            client.store(data=None)

        assert "data cannot be None" in str(exc_info.value)

    def test_store_network_error(self):
        """测试网络错误"""
        client = MemoryClient(base_url="http://test")

        with patch.object(client.session, 'post') as mock_post:
            mock_post.side_effect = ConnectionError("Network error")

            with pytest.raises(NetworkError) as exc_info:
                client.store(data="test")

            assert "Network error" in str(exc_info.value)
```

#### Go 单元测试示例

```go
package memory_test

import (
    "context"
    "testing"

    "github.com/stretchr/testify/assert"
    "github.com/stretchr/testify/mock"
    agentos "github.com/spharx/agentos-go-sdk"
)

func TestMemoryClient_Store(t *testing.T) {
    // 创建 mock HTTP server
    server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        assert.Equal(t, http.MethodPost, r.Method)
        assert.Equal(t, "/api/v1/memory/store", r.URL.Path)

        w.WriteHeader(http.StatusOK)
        w.Write([]byte(`{
            "jsonrpc": "2.0",
            "result": {"record_id": "rec-001"},
            "id": "test"
        }`))
    }))
    defer server.Close()

    client := agentos.NewClient(&agentos.Config{BaseURL: server.URL})
    memory := client.Memory()

    ctx := context.Background()
    recordID, err := memory.Store(ctx, &agentos.StoreRequest{
        Data: "test data",
    })

    assert.NoError(t, err)
    assert.Equal(t, "rec-001", recordID)
}

func TestMemoryClient_Store_InvalidData(t *testing.T) {
    client := agentos.NewClient(&agentos.Config{})
    memory := client.Memory()

    ctx := context.Background()
    _, err := memory.Store(ctx, &agentos.StoreRequest{
        Data: nil,
    })

    assert.Error(t, err)
    assert.Contains(t, err.Error(), "data cannot be empty")
}
```

### 2. 集成测试 (Integration Tests)

**目标**: 验证组件间的交互正确性

```python
# tests/integration/test_kernel_integration.py
import pytest
from agentos import AgentOSClient

@pytest.fixture(scope="module")
def kernel_client():
    """创建内核客户端（需要运行中的内核服务）"""
    client = AgentOSClient(
        base_url="http://localhost:8080",
        api_key="test-api-key"
    )
    yield client
    client.close()

class TestKernelIntegration:
    """内核集成测试"""

    def test_health_check(self, kernel_client):
        """健康检查集成测试"""
        health = kernel_client.health_check()
        assert health.status == "healthy"
        assert health.version.startswith("1.")

    def test_create_and_chat_agent(self, kernel_client):
        """创建 Agent 并对话的完整流程"""
        from agentos import AgentConfig

        # 创建 Agent
        config = AgentConfig(
            name="integration-test-agent",
            model="gpt-4-turbo"
        )
        agent = kernel_client.create_agent(config)
        agent.start()

        try:
            # 对话
            response = agent.chat("Hello!")
            assert response.content is not None
            assert len(response.content) > 0
        finally:
            agent.stop()

    def test_memory_lifecycle(self, kernel_client):
        """记忆系统生命周期测试"""
        memory = kernel_client.memory

        # 存储
        record_id = memory.store(
            data="Integration test data",
            tags=["integration-test"]
        )
        assert record_id is not None

        # 搜索
        results = memory.search(query="integration test", top_k=5)
        assert len(results) > 0
        assert any(r.id == record_id for r in results)

        # 获取详情
        record = memory.get_by_id(record_id)
        assert record is not None
        assert record.data == "Integration test data"
```

### 3. E2E 测试 (End-to-End Tests)

**目标**: 验证完整的用户使用场景

```python
# tests/e2e/test_user_scenarios.py
import pytest
from agentos import AgentOSClient

class TestUserScenarios:
    """E2E 用户场景测试"""

    @pytest.mark.e2e
    def test_complete_agent_workflow(self):
        """
        完整工作流：
        1. 创建 Agent
        2. 配置技能
        3. 执行多轮对话
        4. 存储记忆
        5. 检索记忆
        6. 清理资源
        """
        client = AgentOSClient(
            base_url="http://localhost:8080",
            api_key=os.environ["TEST_API_KEY"]
        )

        # Step 1: 创建 Agent
        agent_config = AgentConfig(
            name="e2e-test-agent",
            tools=["web-search", "calculator"]
        )
        agent = client.create_agent(agent_config)
        agent.start()

        try:
            # Step 2: 多轮对话
            conversation_history = []
            questions = [
                "What is the weather in Beijing?",
                "Calculate 123 * 456",
                "Summarize our conversation"
            ]

            for question in questions:
                response = agent.chat(question)
                conversation_history.append({
                    "question": question,
                    "answer": response.content
                })
                assert response.content

            # Step 3: 记忆存储与检索
            memory_record_id = agent.memory.store(
                data={
                    "conversation": conversation_history,
                    "type": "e2e_test"
                },
                tags=["e2e", "conversation"]
            )

            search_results = agent.memory.search(
                query="weather calculation",
                top_k=5
            )
            assert any(r.id == memory_record_id for r in search_results)

        finally:
            # Step 4: 清理
            agent.stop()
            client.delete_agent(agent.id)

    @pytest.mark.e2e
    @pytest.mark.performance
    def test_concurrent_agents(self):
        """并发 Agent 性能测试"""
        import concurrent.futures

        client = AgentOSClient(base_url="http://localhost:8080")

        def run_agent_session(agent_index):
            agent = client.create_agent(AgentConfig(
                name=f"concurrent-agent-{agent_index}"
            ))
            agent.start()

            start_time = time.time()
            for _ in range(10):
                agent.chat(f"Message {agent_index}")

            duration = time.time() - start_time
            agent.stop()
            return duration

        # 启动 10 个并发 Agent
        with concurrent.futures.ThreadPoolExecutor(max_workers=10) as executor:
            futures = [
                executor.submit(run_agent_session, i)
                for i in range(10)
            ]
            durations = [f.result() for f in futures]

        avg_duration = sum(durations) / len(durations)
        print(f"Average session duration: {avg_duration:.2f}s")
        assert avg_duration < 30.0  # 每个会话应在 30 秒内完成
```

---

## 📊 测试覆盖率要求

### 覆盖率标准

| 组件类型 | 行覆盖率 | 分支覆盖率 | 函数覆盖率 |
|----------|---------|-----------|-----------|
| **核心内核** | ≥ 90% | ≥ 85% | ≥ 95% |
| **守护进程** | ≥ 85% | ≥ 80% | ≥ 90% |
| **SDK 客户端** | ≥ 90% | ≥ 85% | ≥ 95% |
| **工具函数** | ≥ 95% | ≥ 90% | 100% |

### 运行覆盖率报告

```bash
# Python (pytest-cov)
pytest --cov=agentos --cov-report=html --cov-fail-under=90

# Go
go test -coverprofile=coverage.out ./...
go tool cover -html=coverage.out

# C++ (gcov)
cmake .. -DCMAKE_BUILD_TYPE=Coverage
make && ctest
gcov -r *.cpp
```

---

## ⚡ 性能基准测试

### 基准测试框架

| 语言 | 工具 | 用途 |
|------|------|------|
| Python | pytest-benchmark | 微基准测试 |
| Go | testing.B | 内置基准测试 |
| C++ | Google Benchmark | 高性能基准 |

### 关键性能指标

```python
# benchmarks/performance_benchmarks.py
import pytest

class TestIPCBenchmark:
    """IPC 性能基准"""

    @pytest.mark.benchmark
    def test_ipc_latency(self, benchmark):
        """IPC 调用延迟"""
        client = get_kernel_client()

        def ipc_call():
            return client.syscall("memory.ping")

        latency = benchmark(ipc_call)
        assert latency < 0.001  # < 1ms

    @pytest.mark.benchmark
    def test_ipc_throughput(self, benchmark):
        """IPC 吞吐量"""
        client = get_kernel_client()

        def batch_calls():
            for _ in range(1000):
                client.syscall("memory.ping")

        throughput = benchmark(batch_calls)
        calls_per_sec = 1000 / (throughput / 1000000000)  # 纳秒转秒
        assert calls_per_sec > 1000000  # > 1M QPS


class TestMemoryBenchmark:
    """记忆系统性能基准"""

    @pytest.mark.benchmark
    def test_memory_write_throughput(self, benchmark):
        """L1 写入吞吐量"""
        memory = get_memory_client()

        def write_batch():
            for i in range(100):
                memory.store(data=f"benchmark-data-{i}")

        benchmark(write_batch)

    @pytest.mark.benchmark
    def test_memory_search_latency(self, benchmark):
        """L2 搜索延迟"""
        memory = get_memory_client()

        def search():
            return memory.search(query="benchmark query", top_k=10)

        results = benchmark(search)
        assert len(results) > 0
```

### 运行基准测试

```bash
# Python
pytest benchmarks/ --benchmark-only --benchmark-autosave

# Go
go test -bench=. -benchmem -benchtime=10s ./...

# 输出结果对比
pytest-benchmark compare --baseline=baseline.json current.json
```

---

## 🔄 CI/CD 测试流水线

### GitHub Actions 示例

```yaml
# .github/workflows/tests.yml
name: Tests

on: [push, pull_request]

jobs:
  unit-tests:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        python-version: ['3.10', '3.11', '3.12']
        go-version: ['1.21', '1.22']

    steps:
      - uses: actions/checkout@v4

      - name: Set up Python
        uses: actions/setup-python@v4
        with:
          python-version: ${{ matrix.python-version }}

      - name: Install dependencies
        run: |
          pip install -r requirements-dev.txt
          pip install -e .

      - name: Run unit tests
        run: |
          pytest tests/unit/ -v \
            --cov=agentos \
            --cov-report=xml \
            --cov-fail-under=90

      - name: Upload coverage
        uses: codecov/codecov-action@v3

  integration-tests:
    needs: unit-tests
    runs-on: ubuntu-latest
    services:
      postgres:
        image: postgres:15-alpine
        env:
          POSTGRES_DB: test
          POSTGRES_USER: test
          POSTGRES_PASSWORD: test
        ports:
          - 5432:5432
        options: >-
          --health-cmd pg_isready
          --health-interval 10s
          --health-timeout 5s
          --health-retries 5

      redis:
        image: redis:7-alpine
        ports:
          - 6379:6379
        options: >-
          --health-cmd redis-cli ping
          --health-interval 10s
          --health-timeout 5s
          --health-retries 5

    steps:
      - uses: actions/checkout@v4

      - name: Run integration tests
        run: |
          pytest tests/integration/ -v \
            --envfile .env.test

  e2e-tests:
    needs: integration-tests
    runs-on: ubuntu-latest
    if: github.ref == 'refs/heads/main'

    steps:
      - uses: actions/checkout@v4

      - name: Deploy test environment
        run: docker compose -f docker/docker-compose.yml up -d

      - name: Wait for services
        run: |
          sleep 30
          docker compose ps

      - name: Run E2E tests
        run: |
          pytest tests/e2e/ -v -m e2e

      - name: Cleanup
        if: always
        run: docker compose down -v
```

---

## 🧰 测试工具与配置

### pytest 配置

```ini
# pyproject.toml 或 setup.cfg
[tool.pytest.ini_options]
minversion = "7.0"
addopts = "-ra -q --strict-markers"
testpaths = ["tests"]
markers = [
    "unit: Unit tests",
    "integration: Integration tests (requires running services)",
    "e2e: End-to-end tests",
    "slow: Slow running tests",
    "performance: Performance benchmarks",
    "security: Security-related tests",
]
filterwarnings = [
    "ignore::DeprecationWarning",
]

[tool.coverage.run]
source = ["agentos"]
branch = true
omit = [
    "*/tests/*",
    "*/__pycache__/*",
]

[tool.coverage.report]
exclude_lines = [
    "pragma: no cover",
    "def __repr__",
    "raise NotImplementedError",
    "if __name__ == .__main__.:",
]
fail_under = 90
```

### Mock 最佳实践

```python
# 使用 pytest-mock fixture
def test_with_mock(mock_client):
    mock_client.get.return_value = {"status": "ok"}
    result = call_api(mock_client)
    assert result["status"] == "ok"

# 使用 unittest.mock.patch
@patch('agentos.client.requests.Session.post')
def test_with_patch(mock_post):
    mock_post.return_value.json.return_value = {"result": "success"}
    # ... test code

# 使用响应工厂模式
def create_mock_response(status_code=200, json_data=None):
    response = Mock()
    response.status_code = status_code
    response.json.return_value = json_data or {}
    return response
```

---

## 📈 测试数据管理

### 测试夹具 (Fixtures)

```python
# conftest.py
import pytest
import tempfile
import os

@pytest.fixture
def sample_agent_config():
    """返回标准的 Agent 配置"""
    return AgentConfig(
        name="test-agent",
        model="gpt-4-turbo",
        max_tokens=100,
        temperature=0.7
    )

@pytest.fixture
def temp_database():
    """创建临时数据库用于测试"""
    db_fd, db_path = tempfile.mkstemp(suffix=".db")
    yield db_path
    os.close(db_fd)
    os.unlink(db_path)

@pytest.fixture
def authenticated_client():
    """返回已认证的客户端"""
    client = AgentOSClient(
        base_url="http://localhost:8080",
        api_key="test-key-for-testing"
    )
    yield client
    client.close()
```

### 测试数据生成器

```python
from faker import Faker

fake = Faker()

def generate_test_user():
    """生成随机测试用户数据"""
    return {
        "name": fake.name(),
        "email": fake.email(),
        "username": fake.user_name(),
        "created_at": fake.date_time_this_year()
    }

def generate_test_conversation(num_messages=5):
    """生成测试对话历史"""
    return [
        {
            "role": "user" if i % 2 == 0 else "assistant",
            "content": fake.sentence(nb_words=10)
        }
        for i in range(num_messages)
    ]
```

---

## 🐛 测试常见问题

### 问题 1: 测试间状态污染

**解决方案**: 使用独立的测试数据库和清理机制

```python
@pytest.fixture(autouse=True)
def cleanup_database(db_session):
    """每个测试后自动清理数据库"""
    yield
    db_session.rollback()
    db_session.query(Model).delete()
    db_session.commit()
```

### 问题 2: 异步测试超时

**解决方案**: 合理设置超时和使用 async fixtures

```python
@pytest.fixture
async def async_client():
    client = AsyncAgentOSClient()
    yield client
    await client.close()

@pytest.mark.asyncio
async def test_async_operation(async_client):
    result = await asyncio.wait_for(
        async_client.some_async_method(),
        timeout=5.0
    )
    assert result
```

### 问题 3: 外部服务依赖

**解决方案**: 使用服务虚拟化或 Contract Testing

```python
# 使用 WireMock 或 Mountebank 模拟外部服务
@pytest.fixture
def mock_llm_service():
    with WireMockServer() as wiremock:
        wiremock.stub_for(
            post(url_pathEqualTo("/api/v1/chat"))
            .willReturn(a_response()
                      .with_status(200)
                      .with_body(json.dumps({"choices": [{"message": {"content": "Mock"}}]})))
        )
        yield wiremock.base_url
```

---

## 📚 相关文档

- **[编码规范](coding-standards.md)** — 代码风格与测试规范
- **[调试指南](debugging.md)** — 调试技巧与工具
- **[架构决策记录](adr/)** — ADR 索引
- **[CI/CD 流水线](../../.github/workflows/)** — 自动化测试配置

---

**© 2026 SPHARX Ltd. All Rights Reserved.**

*"From data intelligence emerges."*
