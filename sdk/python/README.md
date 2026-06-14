# Toolkit Python — AgentOS Python SDK

**模块路径**: `sdk/python/`
**版本**: v0.1.0 (SDK v0.1.0)

## 概述

AgentOS Python SDK 是 AgentOS 系统的生产级 Python 接口，提供同步和异步两种客户端模式。SDK 遵循 ARCHITECTURAL_PRINCIPLES.md 五维正交设计体系，包含客户端层、业务模块层（Task/Memory/Session/Skill）、应用框架层、遥测和插件系统。支持跨平台（Linux/macOS/Windows）、Checkpoint 断点续传、Token 使用效率优化（LRU 缓存）和 OpenTelemetry 可观测性集成。

## 目录结构

```
python/
├── agentos/                    # 核心包
│   ├── __init__.py             # 模块入口，导出所有公共 API
│   ├── agent.py                # AgentOS/AsyncAgentOS 同步/异步客户端
│   ├── task.py                 # Task 领域模型
│   ├── memory.py               # Memory 领域模型
│   ├── session.py              # Session 领域模型
│   ├── skill.py                # Skill 领域模型
│   ├── protocol.py             # 协议处理
│   ├── syscall.py              # 系统调用绑定
│   ├── _syscall.py             # Syscall 内部实现
│   ├── telemetry.py            # TelemetryManager/Tracer/Span/Metrics
│   ├── exceptions.py           # 异常层级与错误码常量
│   ├── types.py                # 向后兼容类型定义
│   ├── utils.py                # 向后兼容工具函数
│   ├── client/                 # 客户端层
│   │   ├── __init__.py         # 导出 Client/APIClient/ClientConfig
│   │   ├── client.py           # APIClient/ClientConfig/RequestOptions
│   │   └── mock.py             # MockClient 测试客户端
│   ├── modules/                # 业务模块层
│   │   ├── __init__.py         # 导出所有 Manager
│   │   ├── base_manager.py     # BaseManager 基类
│   │   ├── task/
│   │   │   ├── __init__.py
│   │   │   ├── manager.py      # TaskManager
│   │   │   └── checkpoint.py   # Checkpoint 断点续传
│   │   ├── memory/
│   │   │   ├── __init__.py
│   │   │   └── manager.py      # MemoryManager
│   │   ├── session/
│   │   │   ├── __init__.py
│   │   │   └── manager.py      # SessionManager
│   │   └── skill/
│   │       ├── __init__.py
│   │       └── manager.py      # SkillManager
│   ├── framework/              # 应用框架层
│   │   ├── __init__.py
│   │   ├── application.py      # Application 基类
│   │   ├── plugin.py           # Plugin 系统
│   │   ├── lifecycle.py        # 生命周期管理
│   │   ├── config.py           # 配置管理
│   │   ├── event.py            # 事件系统
│   │   ├── state.py            # 状态管理
│   │   ├── task.py             # 框架任务
│   │   ├── skill.py            # 框架技能
│   │   ├── errors.py           # 框架错误
│   │   └── plugins/            # 内置插件
│   │       ├── __init__.py
│   │       ├── metrics_plugin.py   # 指标插件
│   │       └── logger_plugin.py    # 日志插件
│   ├── types/                  # 新类型定义模块
│   │   ├── __init__.py
│   │   └── common.py           # 公共类型
│   └── utils/                  # 工具函数模块
│       ├── __init__.py
│       ├── helpers.py          # 通用工具函数
│       ├── api_helpers.py      # API 辅助函数
│       ├── event_emitter.py    # EventEmitter 事件驱动
│       └── token_optimizer.py  # Token 使用优化（LRU 缓存）
├── tests/                      # 测试套件
│   ├── base_test_case.py       # 测试基类
│   ├── test_agent.py           # Agent 测试
│   ├── test_managers.py        # Manager 测试
│   ├── test_managers_extended.py  # 扩展 Manager 测试
│   ├── test_task_manager_refactored.py  # 重构 TaskManager 测试
│   ├── test_checkpoint.py      # Checkpoint 测试
│   ├── test_comprehensive.py   # 综合测试
│   ├── test_integration_e2e.py # 端到端集成测试
│   ├── test_cross_platform.py  # 跨平台测试
│   ├── test_concurrent_stress.py  # 并发压力测试
│   ├── test_benchmark_performance.py  # 性能基准测试
│   ├── test_plugin_lifecycle.py  # 插件生命周期测试
├── examples/                   # 使用示例
│   ├── long_task_with_checkpoint.py  # Checkpoint 示例
│   ├── openlab_integration.py  # OpenLab 集成示例
│   ├── event_emitter_usage.py  # EventEmitter 示例
│   └── framework_usage_example.py  # Framework 示例
├── setup.py                    # 包配置
└── README.md                   # 本文件
```

## 核心组件

### 同步客户端 (`AgentOS`)

```python
from agentos import AgentOS

client = AgentOS(endpoint="http://localhost:18789", timeout=30, api_key="key")

task = client.submit_task('{"input": "analyze this data"}')
result = task.wait(timeout=30)

memory_id = client.write_memory("content", metadata={"tag": "important"})
memories = client.search_memory("query", top_k=5)

session = client.create_session()
skill = client.load_skill("browser-skill")

client.close()
```

### 异步客户端 (`AsyncAgentOS`)

```python
from agentos import AsyncAgentOS

async with AsyncAgentOS(endpoint="http://localhost:18789") as client:
    task = await client.submit_task("analyze data")
    result = await task.wait(timeout=30)
    memory_id = await client.write_memory("content")
```

### 业务模块管理器

| 管理器 | 核心方法 | 说明 |
|--------|----------|------|
| `TaskManager` | submit/get/cancel/list/wait | 任务提交、查询、取消、列表、等待 |
| `MemoryManager` | write/read/search/delete/list | 记忆写入、读取、搜索、删除、列表 |
| `SessionManager` | create/get/close/list | 会话创建、获取、关闭、列表 |
| `SkillManager` | load/execute/unload/list | 技能加载、执行、卸载、列表 |

### 应用框架层

| 组件 | 说明 |
|------|------|
| `Application` | 应用基类，管理生命周期和插件 |
| `Plugin` | 插件系统，支持 hooks 和中间件 |
| `Lifecycle` | 生命周期管理（INIT/START/RUN/STOP/SHUTDOWN） |
| `EventEmitter` | 事件驱动架构，支持 on/emit/off |
| `Config` | 配置管理，支持 YAML/环境变量 |
| `MetricsPlugin` | 内置指标收集插件 |
| `LoggerPlugin` | 内置日志插件 |

### Checkpoint 断点续传

```python
from agentos.modules.task.checkpoint import CheckpointManager

checkpoint = CheckpointManager(checkpoint_dir="./checkpoints")
await checkpoint.save(task_id="task-001", step=5, data=state)
state = await checkpoint.load(task_id="task-001")
```

### Token 优化器

```python
from agentos.utils.token_optimizer import TokenOptimizer

optimizer = TokenOptimizer(max_cache_size=1000)
optimized = optimizer.optimize(prompt, max_tokens=4096)
```

## 类型系统

### 枚举类型

| 枚举 | 值 |
|------|-----|
| `TaskStatus` | PENDING/RUNNING/COMPLETED/FAILED/CANCELLED |
| `MemoryLayer` | L1/L2/L3/L4 |
| `MemoryRecordType` | EPISODIC/SEMANTIC/PROCEDURAL |
| `SessionStatus` | ACTIVE/EXPIRED/CLOSED |
| `SkillStatus` | LOADED/EXECUTING/COMPLETED/FAILED |
| `SpanStatus` | OK/ERROR/UNSET |

### 领域模型

| 模型 | 字段 |
|------|------|
| `Task` | task_id/description/status/result/created_at/updated_at |
| `TaskResult` | success/output/error/metrics |
| `Memory` | memory_id/content/created_at/metadata |
| `MemorySearchResult` | memory/score/highlight |
| `Session` | session_id/status/created_at/metadata |
| `Skill` | skill_id/name/status/capabilities |
| `SkillResult` | success/output/error/execution_time |

## 异常层级

```
AgentOSError
├── NetworkError
├── AgentOSTimeoutError
├── ValidationError
├── AuthenticationError
├── RateLimitError
├── ServerError
├── InvalidResponseError
├── ConfigError
├── SyscallError
├── TelemetryError
├── TaskError
├── SessionError
├── SkillError
├── AgentOSMemoryError
└── InitializationError
```

## 依赖关系

- **Python**: >= 3.10
- **核心依赖**: requests, aiohttp, pydantic
- **可选依赖**: opentelemetry-api, opentelemetry-sdk
- **开发依赖**: pytest, pytest-asyncio

## 构建说明

```bash
# 安装
pip install -e .

# 运行测试
pytest tests/

# 运行特定测试
pytest tests/test_agent.py -v
pytest tests/test_integration_e2e.py -v

# 运行基准测试
pytest tests/test_benchmark_performance.py -v --benchmark
```

## 使用示例

### 基本使用

```python
from agentos import AgentOS

client = AgentOS(endpoint="http://localhost:18789")

task = client.submit_task("Generate a summary of this document")
result = task.wait(timeout=60)
print(result.output)

client.close()
```

### 异步使用

```python
import asyncio
from agentos import AsyncAgentOS

async def main():
    async with AsyncAgentOS() as client:
        task = await client.submit_task("Analyze data")
        result = await task.wait(timeout=30)
        print(result.output)

asyncio.run(main())
```

### 使用模块管理器

```python
from agentos import AgentOS, TaskManager, MemoryManager

client = AgentOS()
task_mgr = TaskManager(client)
memory_mgr = MemoryManager(client)

task = task_mgr.submit("Process data")
memory_id = memory_mgr.write("Important context", metadata={"source": "doc"})
results = memory_mgr.search("context", top_k=3)
```

### 使用框架层

```python
from agentos.framework import Application, Plugin, Lifecycle

class MyApp(Application):
    def on_start(self):
        print("Application started")

    def on_message(self, message):
        return self.process(message)

app = MyApp(config={"name": "my-agent"})
app.run()
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
