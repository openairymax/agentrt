# AgentOS 测试套件 (Tests)

<div align="center">

[![Version](https://img.shields.io/badge/version-1.0.0.6-blue.svg)](https://gitee.com/spharx/agentos)
[![License](https://img.shields.io/badge/license-Apache--2.0-green.svg)](https://gitee.com/spharx/agentos/blob/main/LICENSE)

**AgentOS 完整测试体系 - 单元测试、集成测试、安全测试、性能基准测试**

*"Test everything that can break."*

---

Language: **简体中文** | [English](../partdocs/readme/en/README.md)

</div>

---

## 📋 概述

`tests/` 目录包含 AgentOS 的完整测试套件，涵盖单元测试、集成测试、安全测试、性能基准测试和合约测试。

### 测试金字塔


<!-- From data intelligence emerges. by spharx -->
```
        /
       /  \         E2E Tests (端到端测试)
      /----\
     /      \      Integration Tests (集成测试)
    /--------\
   /          \    Unit Tests (单元测试)
  /------------\
```

---

## 📁 测试结构

```
tests/
├── README.md                      # 本文件
│
├── unit/                          # 单元测试
│   ├── kernel/                    # 内核层单元测试
│   │   ├── test_ipc_binder.c     # IPC Binder 测试
│   │   ├── test_memory_pool.c    # 内存池测试
│   │   ├── test_task_scheduler.c # 任务调度器测试
│   │   └── test_time_service.c   # 时间服务测试
│   ├── sdk/                       # SDK 层单元测试
│   │   ├── python/               # Python SDK 测试
│   │   └── go/                   # Go SDK 测试
│   └── services/                  # 服务层单元测试
│       ├── test_llm_service.c    # LLM 服务测试
│       └── test_tool_service.c   # 工具服务测试
│
├── integration/                   # 集成测试 ⭐
│   ├── coreloopthree/            # 三层一体集成测试
│   │   ├── test_cognition.py     # 认知层测试
│   │   └── test_execution.py     # 行动层测试
│   ├── memoryrovol/              # 记忆系统集成测试
│   │   ├── test_l1_raw.py        # L1 层测试
│   │   ├── test_l2_feature.py    # L2 层测试
│   │   └── test_retrieval.py     # 检索机制测试
│   └── syscall/                  # 系统调用集成测试
│       └── test_syscalls.py      # 系统调用端到端测试
│
├── security/                      # 安全测试 🔒
│   ├── test_permissions.py       # 权限控制测试
│   ├── test_sandbox.py           # 沙箱隔离测试
│   └── test_input_sanitizer.py   # 输入净化测试
│
├── contract/                      # 合约测试 📋
│   ├── test_agent_contracts.py   # Agent 合约验证
│   └── test_skill_contracts.py   # 技能合约验证
│
└── benchmarks/                    # 性能基准测试 ⭐
    ├── concurrency/              # 并发性能测试
    │   └── test_concurrent_connections.py
    ├── retrieval_latency/        # 检索延迟测试
    │   └── test_vector_search.py
    └── token_efficiency/         # Token 效率测试
        └── test_token_usage.py
```

---

## 🧪 运行测试

### 运行所有测试

```bash
# 方法 1: 使用测试脚本
cd tests
./run_all_tests.sh

# 方法 2: 使用 CTest（C/C++ 测试）
cd build
ctest --output-on-failure

# 方法 3: 使用 pytest（Python 测试）
pytest tests/ -v
```

### 分类测试

```bash
# 单元测试
ctest -R unit --output-on-failure

# 集成测试
ctest -R integration --output-on-failure

# 安全测试
python tests/security/test_permissions.py
python tests/security/test_sandbox.py

# 合约测试
python tests/contract/test_agent_contracts.py
python tests/contract/test_skill_contracts.py

# 性能测试
python scripts/benchmark.py
```

### 选择性运行测试

```bash
# C/C++ 测试 - 运行特定测试
ctest -R test_ipc_binder --output-on-failure

# Python 测试 - 运行特定模块
python -m pytest tests/unit/kernel/ -v

# 运行标签匹配的测试
pytest -m "slow" tests/integration/

# 并行运行测试（加速）
pytest -n auto tests/unit/

# 运行单个测试文件
pytest tests/integration/coreloopthree/test_cognition.py::test_intent_understanding -v
```

---

## 📊 测试覆盖率

### 测试分类与覆盖率要求

| 测试类型 | 目标 | 工具 | 覆盖率要求 |
|---------|------|------|----------|
| **单元测试** | 验证最小可测试单元 | CTest, pytest | ≥ 85% |
| **集成测试** | 验证模块间协作 | pytest | ≥ 80% |
| **安全测试** | 验证安全机制 | pytest, OWASP ZAP | 100% 关键路径 |
| **性能测试** | 验证性能指标 | pytest-benchmark | 符合基准要求 |
| **合约测试** | 验证接口契约 | JSON Schema | 100% 接口 |

### 生成覆盖率报告

#### C/C++ 覆盖率（使用 gcov/lcov）

```bash
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
make -j4
ctest
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_report

# 查看 HTML 报告
open coverage_report/index.html  # macOS
xdg-open coverage_report/index.html  # Linux
```

#### Python 覆盖率

```bash
# 安装工具
pip install coverage pytest-cov

# 运行测试并收集覆盖率
coverage run -m pytest tests/

# 生成文本报告
coverage report

# 生成 HTML 报告
coverage html
open htmlcov/index.html  # macOS
xdg-open htmlcov/index.html  # Linux

# 示例输出：
# Name                                      Stmts   Miss  Cover
# -------------------------------------------------------------
# agentos/client.py                           150      12    92%
# agentos/task.py                              85       5    94%
# agentos/memory.py                           120       8    93%
# -------------------------------------------------------------
# TOTAL                                       355      25    93%
```

---

---

## 🔬 单元测试详解

### C 语言单元测试示例

**test_ipc_binder.c**:
```c
#include <assert.h>
#include <stdio.h>
#include "ipc_binder.h"

void test_binder_init() {
    binder_handle_t handle;
    int ret = binder_init(&handle);
    assert(ret == 0);
    assert(handle != NULL);
    printf("✓ test_binder_init passed\n");
}

void test_binder_transact() {
    binder_handle_t handle;
    binder_init(&handle);
    
    const char* request = "Hello";
    char* response = NULL;
    
    int ret = binder_transact(handle, request, &response);
    assert(ret == 0);
    assert(response != NULL);
    
    free(response);
    binder_destroy(handle);
    printf("✓ test_binder_transact passed\n");
}

int main() {
    test_binder_init();
    test_binder_transact();
    printf("All tests passed!\n");
    return 0;
}
```

### Python 单元测试示例

**test_task_scheduler.py**:
```python
import pytest
from agentos import TaskScheduler, Task

class TestTaskScheduler:
    @pytest.fixture
    def scheduler(self):
        """创建测试用调度器"""
        return TaskScheduler(algorithm="round_robin")
    
    def test_submit_task(self, scheduler):
        """测试任务提交"""
        task = scheduler.submit_task("Test task")
        assert task is not None
        assert task.id > 0
        assert task.status == "pending"
    
    def test_schedule_tasks(self, scheduler):
        """测试任务调度"""
        # 提交多个任务
        tasks = [
            scheduler.submit_task(f"Task {i}")
            for i in range(10)
        ]
        
        # 执行调度
        scheduled = scheduler.schedule_tasks()
        
        assert len(scheduled) == 10
        assert all(t.status == "running" for t in scheduled)
    
    def test_cancel_task(self, scheduler):
        """测试任务取消"""
        task = scheduler.submit_task("Long running task")
        result = scheduler.cancel_task(task.id)
        
        assert result is True
        assert task.status == "cancelled"

if __name__ == "__main__":
    pytest.main([__file__, "-v"])
```

### Go 语言单元测试示例

**go_test_example.go**:
```go
package agentos_test

import (
    "testing"
    "github.com/spharx/agentos-go-sdk"
    "github.com/stretchr/testify/assert"
)

func TestClient_CreateTask(t *testing.T) {
    client := agentos.NewClient("http://localhost:18789")
    
    task, err := client.CreateTask("Test task")
    
    assert.NoError(t, err)
    assert.NotNil(t, task)
    assert.Greater(t, task.ID, int64(0))
}
```

---

## 🔗 集成测试详解

### CoreLoopThree 集成测试

**test_cognition.py**:
```python
"""认知层集成测试"""

from agentos import AgentOS, Intent, TaskDAG

def test_intent_understanding():
    """测试意图理解"""
    client = AgentOS()
    
    # 复杂意图
    intent = client.parse_intent(
        "帮我分析上季度的销售数据，找出增长最快的三个产品"
    )
    
    assert intent.type == "data_analysis"
    assert intent.goal == "find_top_products"
    assert intent.constraints["count"] == 3
    assert intent.constraints["metric"] == "growth_rate"

def test_task_planning():
    """测试任务规划"""
    planner = TaskDAG()
    
    # 生成任务图
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

def test_agent_dispatching():
    """测试 Agent 调度"""
    dispatcher = AgentDispatcher()
    
    # 调度决策
    assignment = dispatcher.dispatch(
        task_type="backend_development",
        required_skills=["python", "fastapi", "postgresql"],
        available_agents=[
            {"id": 1, "skills": ["python", "django"]},
            {"id": 2, "skills": ["python", "fastapi", "postgresql"]},
            {"id": 3, "skills": ["java", "spring"]}
        ]
    )
    
    assert assignment.agent_id == 2
    assert assignment.match_score > 0.9
```

### MemoryRovol 集成测试

**test_retrieval.py**:
```python
"""记忆检索集成测试"""

from agentos import MemoryRovol, MemoryRecord
import time

def test_attractor_network_retrieval():
    """测试吸引子网络检索"""
    memory = MemoryRovol()
    
    # 写入测试数据
    records = [
        memory.write(f"Product A sales increased by {i}%")
        for i in range(100)
    ]
    
    # 检索测试
    results = memory.search(
        query="product sales growth",
        top_k=10,
        use_attractor_network=True
    )
    
    assert len(results) == 10
    assert all(r.score > 0.7 for r in results)
    assert results[0].content.startswith("Product A")

def test_context_mounting():
    """测试上下文挂载"""
    memory = MemoryRovol()
    
    # 创建会话上下文
    session_id = memory.create_session()
    
    # 挂载相关记忆
    mounted = memory.mount_context(
        session_id=session_id,
        keywords=["python", "programming"]
    )
    
    assert len(mounted) > 0
    
    # 验证自动关联
    context = memory.get_context(session_id)
    assert "python" in context.keywords

def test_lru_cache_performance():
    """测试 LRU 缓存性能"""
    memory = MemoryRovol(cache_size_mb=512)
    
    # 预热缓存
    for i in range(1000):
        memory.write(f"Test record {i}")
    
    # 测试缓存命中率
    cache_hits = 0
    total_queries = 100
    
    for i in range(total_queries):
        result = memory.search("test", use_cache=True)
        if result.from_cache:
            cache_hits += 1
    
    hit_rate = cache_hits / total_queries
    assert hit_rate > 0.8, f"Cache hit rate too low: {hit_rate}"
```

---

## 🔒 安全测试详解

### 权限控制测试

**test_permissions.py**:
```python
"""权限裁决测试"""

from agentos import PermissionEngine, PermissionRule

def test_rbac_enforcement():
    """测试基于角色的访问控制"""
    engine = PermissionEngine()
    
    # 定义角色
    engine.add_role("admin", permissions=["read", "write", "delete"])
    engine.add_role("developer", permissions=["read", "write"])
    engine.add_role("viewer", permissions=["read"])
    
    # 分配角色
    engine.assign_role("user1", "admin")
    engine.assign_role("user2", "developer")
    engine.assign_role("user3", "viewer")
    
    # 测试权限检查
    assert engine.check_permission("user1", "delete") is True
    assert engine.check_permission("user2", "delete") is False
    assert engine.check_permission("user3", "write") is False

def test_permission_cache():
    """测试权限缓存性能"""
    engine = PermissionEngine()
    
    # 添加大量规则
    for i in range(1000):
        engine.add_rule(f"rule_{i}", {"resource": f"res_{i}", "action": "read"})
    
    # 测试缓存命中率
    start_time = time.time()
    for i in range(100):
        engine.check_permission("user1", "read", "res_50")
    end_time = time.time()
    
    avg_latency = (end_time - start_time) / 100 * 1000  # ms
    assert avg_latency < 1.0, f"Permission check too slow: {avg_latency}ms"
```

### 沙箱隔离测试

**test_sandbox.py**:
```python
"""沙箱隔离测试"""

from agentos import Sandbox, SandboxViolation

def test_file_system_isolation():
    """测试文件系统隔离"""
    sandbox = Sandbox(isolate_fs=True)
    
    # 尝试访问受限目录
    with pytest.raises(SandboxViolation):
        sandbox.execute("cat /etc/passwd")
    
    # 允许访问工作目录
    result = sandbox.execute("ls /workspace")
    assert result.returncode == 0

def test_network_isolation():
    """测试网络隔离"""
    sandbox = Sandbox(isolate_network=True)
    
    # 尝试外部网络连接
    with pytest.raises(SandboxViolation):
        sandbox.execute("curl https://example.com")
    
    # 允许本地连接
    result = sandbox.execute("curl http://localhost:8080")
    assert result.returncode == 0

def test_resource_limits():
    """测试资源限制"""
    sandbox = Sandbox(
        max_memory_mb=256,
        max_cpu_percent=50,
        max_processes=10
    )
    
    # 尝试超出内存限制
    with pytest.raises(SandboxViolation) as exc_info:
        sandbox.execute("python -c 'x = b\"0\" * 500000000'")
    
    assert "memory limit exceeded" in str(exc_info.value)
```

---

## 📋 合约测试详解

### Agent 合约测试

**test_agent_contracts.py**:
```python
"""Agent 服务合约测试"""

import json
import pytest

def load_agent_contract(path):
    """加载 Agent 合约"""
    with open(path) as f:
        return json.load(f)

def validate_contract(contract):
    """验证合约格式"""
    required_fields = ["name", "version", "capabilities", "interface_version"]
    
    for field in required_fields:
        assert field in contract, f"Missing required field: {field}"
    
    # 版本号格式
    import re
    assert re.match(r'^\d+\.\d+\.\d+$', contract["version"])
    
    # Capabilities 必须是列表
    assert isinstance(contract["capabilities"], list)
    assert len(contract["capabilities"]) > 0

@pytest.mark.parametrize("agent_name", [
    "architect",
    "backend_dev",
    "devops",
    "frontend_dev",
    "product_manager",
    "security",
    "tester"
])
def test_agent_contract(agent_name):
    """测试所有 Agent 合约"""
    contract_path = f"openhub/contrib/agents/{agent_name}/contract.json"
    contract = load_agent_contract(contract_path)
    validate_contract(contract)
```

---

## ⚡ 性能基准测试

### 并发连接测试

**test_concurrent_connections.py**:
```python
"""并发连接性能测试"""

import asyncio
import aiohttp
import statistics

async def create_connection(session, url):
    """创建单个连接"""
    try:
        async with session.post(url, json={"method": "ping"}) as resp:
            return await resp.json()
    except Exception as e:
        return None

async def test_max_connections():
    """测试最大并发连接数"""
    url = "http://localhost:18789/rpc"
    
    async with aiohttp.ClientSession() as session:
        # 逐步增加并发数
        for concurrent_count in [100, 500, 1000, 1500]:
            tasks = [
                create_connection(session, url)
                for _ in range(concurrent_count)
            ]
            
            results = await asyncio.gather(*tasks)
            success_count = sum(1 for r in results if r is not None)
            
            print(f"Concurrent: {concurrent_count}, Success: {success_count}")
            
            # 成功率应该 > 95%
            assert success_count / concurrent_count > 0.95

# 运行测试
# python -m pytest tests/benchmarks/concurrency/test_concurrent_connections.py -v
```

### 向量检索延迟测试

**test_vector_search.py**:
```python
"""向量检索延迟测试"""

import time
from agentos import MemoryRovol

def benchmark_vector_search():
    """基准测试向量检索"""
    memory = MemoryRovol()
    
    # 准备测试数据
    print("Indexing 100,000 vectors...")
    for i in range(100000):
        memory.write(f"Document {i} content")
    
    # 测试不同 k 值的检索延迟
    test_cases = [1, 5, 10, 50, 100]
    
    for k in test_cases:
        latencies = []
        
        # 运行 100 次查询
        for _ in range(100):
            start = time.time()
            results = memory.search("random query", top_k=k)
            end = time.time()
            
            latencies.append((end - start) * 1000)  # ms
        
        avg_latency = statistics.mean(latencies)
        p95_latency = sorted(latencies)[95]
        p99_latency = sorted(latencies)[99]
        
        print(f"k={k}: avg={avg_latency:.2f}ms, p95={p95_latency:.2f}ms, p99={p99_latency:.2f}ms")
        
        # 断言性能要求
        if k <= 10:
            assert avg_latency < 10, f"Search too slow for k={k}"
```

---

## 🎯 测试最佳实践

### 1. 测试命名规范

```python
# ❌ 不好的命名
def test_1():
    pass

def test_stuff():
    pass

# ✅ 好的命名
def test_submit_task_with_valid_description():
    pass

def test_cancel_task_that_does_not_exist():
    pass

def test_search_memory_with_empty_query_should_raise_error():
    pass
```

### 2. 测试组织

```
# 按功能分组
tests/
├── unit/
│   └── kernel/
│       ├── test_ipc.py          # IPC 相关测试
│       ├── test_memory.py       # 内存相关测试
│       └── test_scheduler.py    # 调度相关测试
└── integration/
    └── coreloopthree/
        ├── test_cognition.py    # 认知层测试
        └── test_execution.py    # 行动层测试
```

### 3. 测试夹具 (Fixtures)

```python
import pytest

@pytest.fixture
def memory_client():
    """创建记忆客户端"""
    client = MemoryRovol()
    yield client
    client.cleanup()

@pytest.fixture
def sample_task():
    """创建示例任务"""
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
    assert input * 2 == expected
```

---

## 📊 测试统计与性能基准

### v1.0.0.5 测试统计

| 类别 | 测试文件 | 测试用例 | 通过率 | 平均执行时间 |
|------|----------|----------|--------|-------------|
| **单元测试** | 25 个 | 350+ | 98% | 2.3s |
| **集成测试** | 15 个 | 120+ | 95% | 15.7s |
| **安全测试** | 8 个 | 60+ | 97% | 8.2s |
| **合约测试** | 5 个 | 40+ | 99% | 3.1s |
| **性能测试** | 10 个 | 80+ | 92% | 45.6s |
| **总计** | **63 个** | **650+** | **96%** | **75.0s** |

### 性能基准指标

基于标准测试环境 (Intel i7-12700K, 32GB RAM, NVMe SSD):

| 测试项 | 指标 | 要求 | 实测 | 状态 |
|-------|------|------|------|------|
| **IPC Binder 延迟** | p99 | < 1ms | 0.3ms | ✅ |
| **记忆检索延迟** | p95 | < 10ms | 8.2ms | ✅ |
| **任务调度吞吐** | 任务/秒 | > 1000 | 1250 | ✅ |
| **并发连接数** | 最大连接 | > 1000 | 1500 | ✅ |
| **向量检索 QPS** | 查询/秒 | > 10000 | 12500 | ✅ |
| **LRU 缓存命中率** | 命中率 | > 80% | 87% | ✅ |

### CI/CD 集成配置

#### GitHub Actions

```yaml
# .github/workflows/tests.yml
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
          pip install -r requirements-test.txt
      
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

---

## 📚 相关文档

- [测试规范](../partdocs/specifications/testing.md) - 测试编写标准与最佳实践
- [编码规范](../partdocs/specifications/coding_standards.md) - C/C++/Python代码规范
- [故障排查](../partdocs/guides/troubleshooting.md) - 测试失败排查指南
- [CoreLoopThree 架构](../partdocs/architecture/coreloopthree.md) - 三层一体架构详解
- [MemoryRovol 架构](../partdocs/architecture/memoryrovol.md) - 四层记忆卷载系统
- [系统调用详解](../partdocs/architecture/syscall.md) - 系统调用接口文档

---

## 🤝 贡献测试

我们欢迎社区贡献测试用例！贡献测试可以帮助：

1. **提高代码质量**: 发现潜在的 bug 和边界情况
2. **防止回归**: 确保新功能不会破坏现有功能
3. **改进文档**: 测试用例本身就是最好的使用示例

### 如何贡献测试

1. Fork 项目仓库
2. 创建测试分支 (`git checkout -b test/my-awesome-test`)
3. 编写测试用例 (参考本文档的示例)
4. 运行测试确保通过 (`pytest tests/ -v`)
5. 提交更改 (`git commit -am 'Add test for feature X'`)
6. 推送到分支 (`git push origin test/my-awesome-test`)
7. 创建 Pull Request

---

<div align="center">

**© 2026 SPHARX Ltd. 保留所有权利。**

*"From data intelligence emerges"*

**AgentOS v1.0.0.5**

[返回顶部](#agentos-测试套件 -tests)

</div>
