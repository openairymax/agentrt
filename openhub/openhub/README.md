# OpenHub - Agent Operating System

## 概述

OpenHub 是一个生产级的多智能体编排框架，基于钱学森《工程控制论》与《论系统工程》的反馈闭环与层次分解理论，以乔布斯设计美学为风格指引，为 Agent 的生命周期管理、任务调度、工具集成和数据存储提供统一的基础设施。

## 核心特性

- **微内核架构**：核心只保留最基础的 IPC、内存、任务、时间机制
- **三层认知闭环**（CoreLoopThree）：感知-认知-行动三层嵌套反馈
- **四层记忆系统**（MemoryRovol）：L1 工作记忆 → L2 情景记忆 → L3 语义记忆 → L4 长期记忆
- **安全穹顶**（Domes）：虚拟工位、权限裁决、输入净化三重防护
- **可插拔策略**：认知规划、协同、调度、记忆遗忘均可动态替换

## 目录结构

```
openhub/
├── openhub/
│   ├── __init__.py              # 包入口
│   ├── core/                    # 核心模块
│   │   ├── __init__.py
│   │   ├── agent.py             # Agent 管理与生命周期
│   │   ├── task.py              # 任务调度与状态机
│   │   ├── tool.py              # 工具集成与执行
│   │   └── storage.py           # 数据存储抽象
│   ├── agents/                  # Agent 实现
│   │   └── architect/           # 架构师 Agent 示例
│   └── contrib/                 # 贡献组件（从原始目录迁移）
│       ├── agents/
│       ├── skills/
│       └── strategies/
├── README.md                    # 本文件
└── run.sh                       # 快速运行脚本
```

## 快速开始

### 环境要求

- Python 3.10+
- pip 或 pipenv

### 安装依赖

```bash
pip install -r requirements.txt
```

### 运行示例

```bash
# 运行架构师 Agent 示例
python -m openhub.agents.architect.agent

# 或使用运行脚本
./run.sh
```

## 核心模块

### Agent 管理

```python
from openhub.core.agent import Agent, AgentManager, AgentRegistry

manager = AgentManager(registry)
agent = await manager.create_agent(
    agent_class=MyAgent,
    agent_id="agent-001",
    config={"setting": "value"},
)
result = await agent.execute(context, input_data)
```

### 任务调度

```python
from openhub.core.task import Task, TaskScheduler, TaskPriority

scheduler = TaskScheduler(max_concurrent=10)
scheduler.set_handler(my_handler)
await scheduler.start()

task = Task(
    name="my-task",
    priority=TaskPriority.HIGH,
    agent_role="backend",
)
await scheduler.submit(task)
result = await scheduler.wait_for_completion(task.task_id)
```

### 工具集成

```python
from openhub.core.tool import Tool, ToolRegistry, ToolExecutor

registry = ToolRegistry()
executor = ToolExecutor(registry)

output = await executor.execute(
    tool_id="my-tool",
    parameters={"param1": "value"},
)
```

### 数据存储

```python
from openhub.core.storage import SQLiteStorage, StorageRecord

storage = SQLiteStorage(db_path="./data.db")
await storage.initialize()

record = StorageRecord(id="rec-001", data={"key": "value"})
await storage.put("my_collection", record)
```

## 架构师 Agent 示例

`openhub.agents.architect.Agent` 是一个完整可运行的 Agent 示例，实现：

- **初始化配置**：从 YAML/JSON 加载配置
- **任务接收处理**：接收 `ArchitectureInput` 格式的输入
- **工具调用流程**：调用 `ArchitectureDesignTool` 生成设计
- **结果返回机制**：返回 `TaskResult` 格式的结构化输出

### 使用示例

```python
import asyncio
from openhub.agents.architect import ArchitectAgent
from openhub.core.agent import AgentContext

async def main():
    agent = ArchitectAgent(
        agent_id="architect-001",
        config={"verbose": True},
    )
    await agent.initialize()

    context = AgentContext(
        task_id="task-001",
        trace_id="trace-001",
        session_id="session-001",
    )

    result = await agent.execute(context, {
        "project_name": "Smart Inventory System",
        "project_type": "web_app",
        "requirements": [
            "Real-time inventory tracking",
            "Multi-warehouse support",
        ],
        "constraints": {"budget": "medium"},
        "quality_attributes": {"scalability": "high"},
    })

    print(result)
    await agent.shutdown()

asyncio.run(main())
```

## 生产级标准

本实现符合以下生产级标准：

| 维度 | 实现 |
|------|------|
| **错误处理** | 所有公共 API 返回错误码和消息 |
| **类型注解** | 完整的 Type hints 和 generics |
| **版权声明** | 所有文件包含 Copyright header |
| **日志规范** | 结构化 JSON 日志 |
| **线程安全** | asyncio.Lock 保护的共享状态 |
| **资源清理** | try/finally 确保 cleanup 调用 |
| **超时控制** | 所有 IO 操作有超时设置 |

## 版本

当前版本：1.0.0.6

## 许可证

Copyright (c) 2026 SPHARX. All Rights Reserved.
