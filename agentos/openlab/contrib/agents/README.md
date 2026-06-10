# Agents — 社区贡献智能体

**模块路径**: `agentos/openlab/contrib/agents/`
**版本**: v0.1.0

> **Status**: 本模块作为 AgentOS 的正式组成部分，API 持续演进中。本模块通过 JSON-RPC 2.0 协议与 AgentOS 核心运行时集成。

## 概述

Agents 是 OpenLab 社区贡献的角色化智能体集合，提供 7 种专业角色的 Agent 实现。每个 Agent 继承自 `openlab.core.Agent` 基类，具备独立的能力声明、契约规范和提示词模板，可独立运行或组合协作，覆盖软件工程全生命周期。当前所有 Agent 均为贡献骨架（Contrib Skeleton）实现。

## 目录结构

```
agents/
├── architect/                      # 架构师 Agent
│   ├── __init__.py
│   ├── agent.py                    # ArchitectAgent 实现
│   ├── contract.json               # Agent 契约
│   ├── prompts/
│   │   ├── system1.md              # 主系统提示词
│   │   └── system2.md              # 辅助系统提示词
│   └── README.md
├── backend/                        # 后端开发 Agent
│   ├── __init__.py
│   ├── agent.py                    # BackendAgent 实现
│   ├── contract.json
│   ├── prompts/
│   │   ├── system1.md
│   │   └── system2.md
│   └── README.md
├── frontend/                       # 前端开发 Agent
│   ├── __init__.py
│   ├── agent.py                    # FrontendAgent 实现
│   ├── contract.json
│   ├── prompts/
│   │   ├── system1.md
│   │   └── system2.md
│   └── README.md
├── devops/                         # DevOps Agent
│   ├── __init__.py
│   ├── agent.py                    # DevOpsAgent 实现
│   ├── contract.json
│   ├── prompts/
│   │   ├── system1.md
│   │   └── system2.md
│   └── README.md
├── security/                       # 安全审计 Agent
│   ├── __init__.py
│   ├── agent.py                    # SecurityAgent 实现
│   ├── contract.json
│   ├── prompts/
│   │   ├── system1.md
│   │   └── system2.md
│   └── README.md
├── tester/                         # 测试生成 Agent
│   ├── __init__.py
│   ├── agent.py                    # TesterAgent 实现
│   ├── contract.json
│   ├── prompts/
│   │   ├── system1.md
│   │   └── system2.md
│   └── README.md
├── product_manager/                # 产品经理 Agent
│   ├── __init__.py
│   ├── agent.py                    # ProductManagerAgent 实现
│   ├── contract.json
│   ├── prompts/
│   │   ├── system1.md
│   │   └── system2.md
│   └── README.md
└── README.md                       # 本文件
```

## Agent 列表

| Agent | 类名 | Agent ID | 核心能力 | 默认任务类型 |
|-------|------|----------|----------|-------------|
| **Architect** | `ArchitectAgent` | `contrib-architect` | `ARCHITECTURE_DESIGN` | `design` |
| **Backend** | `BackendAgent` | `contrib-backend` | `CODE_GENERATION` | `implement` |
| **Frontend** | `FrontendAgent` | `contrib-frontend` | `CODE_GENERATION` | `generate` |
| **DevOps** | `DevOpsAgent` | `contrib-devops` | `OPTIMIZATION` | `deploy` |
| **Security** | `SecurityAgent` | `contrib-security` | `DEBUGGING` | `audit` |
| **Tester** | `TesterAgent` | `contrib-tester` | `TEST_GENERATION` | `test` |
| **Product Manager** | `ProductManagerAgent` | `contrib-product-manager` | `DOCUMENTATION` | `plan` |

## 核心组件

### Agent 基类继承

所有社区 Agent 继承自 `openlab.core.Agent`，实现统一的生命周期接口：

```python
from openlab.core import Agent, AgentCapability, AgentContext, AgentStatus, TaskResult

class CustomAgent(Agent):
    def __init__(self, config: Optional[Dict[str, Any]] = None):
        super().__init__(
            agent_id="contrib-custom",
            capabilities={AgentCapability.CUSTOM},
        )
        self.config = config or {}

    async def initialize(self) -> None
    async def execute(self, input_data, context) -> TaskResult
    async def shutdown(self) -> None
```

### 契约规范 (`contract.json`)

每个 Agent 配备标准化的契约文件，核心字段：

| 字段 | 说明 |
|------|------|
| `agent_id` | Agent 唯一标识符 |
| `agent_name` | Agent 显示名称 |
| `description` | 功能描述 |
| `version` | 语义化版本号 |
| `capabilities` | 能力声明列表（含输入/输出 Schema） |
| `performance` | 性能指标 |
| `resources` | 资源需求 |
| `dependencies` | 依赖声明 |
| `security` | 安全配置 |
| `lifecycle` | 生命周期参数 |

### 提示词模板 (`prompts/`)

| 文件 | 说明 |
|------|------|
| `system1.md` | 主系统提示词，定义 Agent 的核心行为和角色定位 |
| `system2.md` | 辅助系统提示词，提供补充约束和交互规范 |

## 使用指南

### 单独使用 Agent

```python
import asyncio
from contrib.agents.architect import ArchitectAgent
from openlab.core import AgentContext

async def main():
    architect = ArchitectAgent(config={"llm_provider": "openai"})
    await architect.initialize()

    result = await architect.execute(
        input_data={"task_type": "design", "requirements": {...}},
        context=AgentContext(agent_id="test")
    )
    print(f"Success: {result.success}, Output: {result.output}")

    await architect.shutdown()

asyncio.run(main())
```

### 多 Agent 协作

```python
from contrib.agents.architect import ArchitectAgent
from contrib.agents.backend import BackendAgent
from contrib.agents.tester import TesterAgent

architect = ArchitectAgent()
backend = BackendAgent()
tester = TesterAgent()

design = await architect.execute({"task_type": "design", ...}, context)
impl = await backend.execute({"task_type": "implement", ...}, context)
tests = await tester.execute({"task_type": "test", ...}, context)
```

## 依赖关系

- **核心依赖**: Python >= 3.10, openlab.core（Agent 基类、核心抽象）
- **协议依赖**: AgentOS protocols 层（JSON-RPC 2.0）

---

© 2026 SPHARX Ltd. All Rights Reserved.
