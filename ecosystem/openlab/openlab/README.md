# OpenLab Core — 开放生态核心管理模块

**模块路径**: `ecosystem/openlab/openlab/`
**版本**: v0.1.0

> **Status**: 本模块作为 AgentRT 的正式组成部分，API 持续演进中。本模块通过 JSON-RPC 2.0 协议与 AgentRT 核心运行时集成。

## 概述

OpenLab Core 是 OpenLab 生态系统的核心管理模块，提供 Agent 管理、任务调度、工具执行、数据存储四大核心能力，以及异常处理、日志记录等基础设施。本模块遵循 AgentRT 架构设计原则 V1.8，实现了生产级多智能体编排框架的核心抽象层。

## 目录结构

```
openlab/
├── __init__.py                 # 模块入口，导出所有核心类
├── core/                       # 核心组件
│   ├── __init__.py             # 核心模块导出
│   ├── agent.py                # Agent 基类、状态、注册表
│   ├── task.py                 # 任务定义、状态机、调度器
│   ├── tool.py                 # 工具抽象、注册表、执行器
│   └── storage.py              # 存储抽象（Memory/SQLite）
├── agents/                     # 预构建 Agent 实现
│   └── architect/              # 架构师 Agent
│       ├── __init__.py
│       └── agent.py
├── protocols/                  # 协议处理
│   └── __init__.py
├── utils/                      # 工具函数
│   ├── __init__.py
│   ├── exceptions.py           # 异常层级定义
│   └── logging.py              # 日志配置
├── config.yaml                 # 核心配置文件
├── requirements.txt            # Python 依赖
└── run.sh                      # 启动脚本
```

## 核心组件

### Agent 管理 (`core/agent.py`)

提供 Agent 生命周期管理的完整抽象：

| 类/枚举 | 说明 |
|---------|------|
| `Agent` | Agent 抽象基类，定义 `initialize()`/`execute()`/`shutdown()` 生命周期，支持 `workbench_id` 和 `manager` 关联 |
| `AgentStatus` | Agent 状态枚举：CREATED/INITIALIZING/READY/RUNNING/PAUSED/SHUTTING_DOWN/SHUTDOWN/ERROR |
| `AgentCapability` | Agent 能力枚举：ARCHITECTURE_DESIGN/CODE_GENERATION/TEST_GENERATION/DOCUMENTATION/DEBUGGING/OPTIMIZATION |
| `AgentContext` | Agent 执行上下文，包含 agent_id/task_id/session_id/metadata/timeout |
| `AgentRegistry` | 线程安全的 Agent 注册表，支持 register/unregister/get/list_agents/count |
| `TaskResult` | 任务执行结果，包含 success/output/error/error_code/metrics/warnings |
| `Message` | Agent 消息传递对象，支持 type/content/sender/receiver/metadata/timestamp |

### 任务调度 (`core/task.py`)

提供任务调度和状态机管理：

| 类/枚举 | 说明 |
|---------|------|
| `TaskStatus` | 任务状态：PENDING/QUEUED/RUNNING/PAUSED/COMPLETED/FAILED/CANCELLED/TIMEOUT |
| `TaskCategory` | 任务类别：IMMEDIATE/SCHEDULED/PERIODIC/LONG_RUNNING |
| `TaskDefinition` | 任务定义，包含 task_id/name/description/category/priority/input_data/metadata/timeout/max_retries/scheduled_at |
| `TaskState` | 任务运行时状态，支持 `to_dict()`/`from_dict()` 序列化和 checkpoint_data |
| `ExecutionPlan` | 执行计划，支持 `add_step()`/`get_next_step()` 步骤管理和 resources 分配 |
| `TaskScheduler` | 异步任务调度器，支持优先级队列、并发控制（Semaphore）、超时管理、checkpoint、`get_stats()` 统计 |

### 工具系统 (`core/tool.py`)

提供工具注册和执行的完整框架：

| 类/枚举 | 说明 |
|---------|------|
| `Tool` | 工具抽象基类，支持 INPUT_SCHEMA/OUTPUT_SCHEMA 验证、`_pre_execute_check()` 安全前置检查、`get_info()` 元信息 |
| `ToolCategory` | 工具类别：INPUT_OUTPUT/COMPUTATION/COMMUNICATION/DATA_ACCESS/SYSTEM/CUSTOM |
| `ToolCapability` | 工具能力：READ/WRITE/EXECUTE/QUERY/TRANSFORM/ANALYZE |
| `ToolContext` | 工具执行上下文，包含 tool_id/agent_id/task_id/session_id/timeout/metadata |
| `ToolResult` | 工具执行结果，包含 success/output/error/error_code/execution_time/metrics/warnings |
| `ToolRegistry` | 工具注册表，支持 register/unregister/get/list_tools/find_by_category/find_by_capability |
| `ToolExecutor` | 工具执行器，支持并发控制（Semaphore）、超时管理、执行历史记录、`get_stats()` 统计 |

### 存储系统 (`core/storage.py`)

提供数据存储的抽象层和两种实现：

| 类/枚举 | 说明 |
|---------|------|
| `Storage` | 存储抽象基类，定义 initialize/close/get/set/delete/exists/query/clear/get_json/set_json 接口 |
| `StorageType` | 存储类型：MEMORY/SQLITE/FILE/REDIS/CUSTOM |
| `DataCategory` | 数据类别：TASK/AGENT/TOOL/CHECKPOINT/LOG/METADATA |
| `StorageRecord` | 存储记录，支持 TTL/expires_at/版本号/元数据，支持 `to_dict()`/`from_dict()` |
| `QueryResult` | 查询结果，包含 records/total/offset/limit |
| `MemoryStorage` | 内存存储实现，支持 TTL 过期和条件查询 |
| `SQLiteStorage` | SQLite 存储实现，支持索引（category/expires_at）和异步操作（run_in_executor） |

## 预构建 Agent (`agents/architect/`)

内置架构师 Agent 实现：

| 类 | 说明 |
|----|------|
| `ArchitectAgent` | 架构设计 Agent，支持 analyze/review/design 三种任务类型 |
| `ArchitectConfig` | 配置类，包含 workspace_root/max_file_size/forbidden_paths/allowed_extensions |

## 异常层级 (`utils/exceptions.py`)

```
OpenLabError
├── AgentError
│   ├── AgentInitializationError
│   ├── AgentExecutionError
│   └── AgentNotFoundError
├── TaskError
│   ├── TaskCreationError
│   ├── TaskExecutionError
│   └── TaskNotFoundError
├── ToolError
│   ├── ToolInitializationError
│   ├── ToolExecutionError
│   └── ToolNotFoundError
├── StorageError
│   ├── StorageConnectionError
│   ├── StorageReadError
│   └── StorageWriteError
└── ValidationError
    ├── InputValidationError
    └── ConfigurationError
```

## 日志配置 (`utils/logging.py`)

提供集中式日志配置：

| 函数 | 说明 |
|------|------|
| `setup_logger(name, level, log_file)` | 创建并配置 Logger 实例，支持控制台和文件输出 |
| `get_logger(name)` | 获取已存在的 Logger 实例 |

## 接口说明

### Agent 生命周期

```python
class Agent(ABC):
    async def initialize(self) -> None           # 初始化 Agent
    async def execute(self, input_data, context) # 执行任务，返回 TaskResult
    async def shutdown(self) -> None             # 关闭 Agent
    async def handle_message(self, message)      # 处理消息
    def register_tool(self, name, tool)          # 注册工具
    def get_tool(self, name) -> Optional[Any]    # 获取已注册工具
```

### TaskScheduler 核心 API

```python
class TaskScheduler:
    async def submit(self, definition) -> str                    # 提交任务
    async def schedule(self, definition, executor) -> asyncio.Task  # 调度执行
    async def get_state(self, task_id) -> Optional[TaskState]    # 获取状态
    async def cancel(self, task_id) -> bool                      # 取消任务
    async def save_checkpoint(self, task_id, data) -> bool       # 保存检查点
    async def load_checkpoint(self, task_id) -> Optional[Dict]   # 加载检查点
    async def shutdown(self, wait, timeout)                      # 关闭调度器
    def get_stats(self) -> Dict[str, Any]                        # 获取统计信息
```

### Storage 核心 API

```python
class Storage(ABC):
    async def initialize(self) -> None
    async def close(self) -> None
    async def get(self, key) -> Optional[StorageRecord]
    async def set(self, key, value, category, metadata, ttl) -> bool
    async def delete(self, key) -> bool
    async def exists(self, key) -> bool
    async def query(self, category, filter_func, offset, limit) -> QueryResult
    async def get_json(self, key) -> Optional[Any]
    async def set_json(self, key, value, **kwargs) -> bool
```

## 依赖关系

- **Python**: >= 3.10
- **核心依赖**: asyncio, dataclasses, abc, sqlite3, json, uuid, time
- **无外部依赖**: 核心模块仅使用 Python 标准库

## 配置说明

核心配置文件 `config.yaml` 包含以下主要配置项：

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `agent.max_instances` | 最大 Agent 实例数 | 100 |
| `agent.shutdown_timeout` | 关闭超时（秒） | 30.0 |
| `scheduler.max_concurrent` | 最大并发任务数 | 100 |
| `scheduler.max_queue_size` | 最大队列大小 | 10000 |
| `scheduler.default_timeout` | 默认超时（秒） | 300.0 |
| `tool.max_concurrent` | 最大并发工具执行数 | 50 |
| `storage.backend` | 存储后端 | sqlite |

## 使用示例

```python
import asyncio
from openlab.core.agent import Agent, AgentRegistry, AgentContext, AgentCapability
from openlab.core.task import TaskScheduler, TaskDefinition, TaskCategory
from openlab.core.storage import MemoryStorage, DataCategory

async def main():
    registry = AgentRegistry()
    storage = MemoryStorage()
    await storage.initialize()

    await storage.set("key1", {"data": "value"}, category=DataCategory.METADATA)
    record = await storage.get("key1")

    scheduler = TaskScheduler(max_concurrent=10)
    definition = TaskDefinition(
        name="example_task",
        category=TaskCategory.IMMEDIATE,
        priority=5,
    )
    task_id = await scheduler.submit(definition)

    await storage.close()
    await scheduler.shutdown()

asyncio.run(main())
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
