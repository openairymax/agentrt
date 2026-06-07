# Agents — 社区贡献智能体

**模块路径**: `agentos/openlab/contrib/agents/`
**版本**: v0.1.0

> **Status**: 本模块作为 AgentOS v0.1.0 的正式组成部分，API 已稳定。本模块通过 JSON-RPC 2.0 协议与 AgentOS 核心运行时集成。

## 概述

Agents 是 OpenLab 社区贡献的角色化智能体集合，提供 7 种专业角色的 Agent 实现。每个 Agent 继承自 `openlab.core.Agent` 基类，具备独立的能力声明、契约规范和提示词模板，可独立运行或组合协作，覆盖软件工程全生命周期。

## 架构定位

```
+-------------------------------------------------------------------+
|                         OpenLab 开放生态                            |
+-------------------------------------------------------------------+
|                     Contributions (contrib/)                       |
|  +------------------+  +------------------+  +------------------+ |
|  |     Skills       |  |    Strategies    |  |     Agents       | |
|  |  能力单元         |  |  协作模式        |  |  角色化智能体     | |
|  |                  |  |                  |  |                  | |
|  | • Browser        |  | • Dispatching    |  | • Architect      | |
|  | • Database       |  | • Planning       |  | • Backend        | |
|  | • GitHub         |  |                  |  | • Frontend       | |
|  |                  |  |                  |  | • DevOps         | |
|  |                  |  |                  |  | • Security       | |
|  |                  |  |                  |  | • Tester         | |
|  |                  |  |                  |  | • Product Mgr    | |
|  +------------------+  +------------------+  +------------------+ |
+-------------------------------------------------------------------+
|                     OpenLab Core (openlab/)                        |
|  Agent Registry | Task Scheduler | Tool Executor | Storage         |
+-------------------------------------------------------------------+
```

Agents 作为 Contributions 三大子模块之一，是智能体的角色化实现层，通过 Core 的 Agent 基类获得生命周期管理能力，通过 Strategies 获得协作编排能力。

## 目录结构

```
agents/
├── architect/                      # 架构师 Agent
│   ├── __init__.py                 # 模块导出
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

    async def initialize(self) -> None:    # 初始化，设置状态为 READY
    async def execute(self, input_data, context) -> TaskResult:  # 执行任务
    async def shutdown(self) -> None:      # 关闭，设置状态为 SHUTDOWN
```

### 契约规范 (`contract.json`)

每个 Agent 配备标准化的契约文件，包含以下核心字段：

| 字段 | 说明 |
|------|------|
| `agent_id` | Agent 唯一标识符 |
| `agent_name` | Agent 显示名称 |
| `description` | 功能描述 |
| `version` | 语义化版本号 |
| `capabilities` | 能力声明列表（含输入/输出 Schema） |
| `performance` | 性能指标（响应时间/吞吐量/可用性） |
| `resources` | 资源需求（CPU/内存/存储） |
| `dependencies` | 依赖声明 |
| `security` | 安全配置（认证/授权/加密/审计） |
| `monitoring` | 监控配置（指标/日志/告警） |
| `lifecycle` | 生命周期参数（初始化超时/关闭超时/健康检查） |
| `examples` | 使用示例 |

### 提示词模板 (`prompts/`)

每个 Agent 包含两级系统提示词：

| 文件 | 说明 |
|------|------|
| `system1.md` | 主系统提示词，定义 Agent 的核心行为和角色定位 |
| `system2.md` | 辅助系统提示词，提供补充约束和交互规范 |

## 使用指南

### 单独使用 Agent

```python
import asyncio
from contrib.agents.architect import ArchitectAgent
from contrib.agents.backend import BackendAgent
from openlab.core import AgentContext

async def main():
    # 创建架构师 Agent
    architect = ArchitectAgent(config={"llm_provider": "openai"})
    await architect.initialize()

    # 执行架构设计任务
    result = await architect.execute(
        input_data={
            "task_type": "design",
            "requirements": {
                "scale": "medium",
                "features": ["api", "data_processing"],
                "performance": "high"
            }
        },
        context=AgentContext()
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

# 创建 Agent 团队
architect = ArchitectAgent()
backend = BackendAgent()
tester = TesterAgent()

# 架构设计 → 后端实现 → 测试生成
design = await architect.execute({"task_type": "design", ...}, context)
impl = await backend.execute({"task_type": "implement", ...}, context)
tests = await tester.execute({"task_type": "test", ...}, context)
```

### 通过契约验证 Agent

```python
from markets.agents.contracts.validator import validate_contract

result = validate_contract("contrib/agents/architect/contract.json")
if result.is_valid:
    print("Agent 契约验证通过")
else:
    for error in result.get_errors():
        print(f"验证错误: {error}")
```

## 开发指南

### 创建新的社区 Agent

1. 在 `contrib/agents/` 下创建新目录（使用小写字母和下划线）
2. 实现 `agent.py`，继承 `openlab.core.Agent` 基类
3. 编写 `contract.json`，遵循 Markets 契约 Schema
4. 编写 `prompts/system1.md` 和 `prompts/system2.md`
5. 创建 `__init__.py` 导出 Agent 类
6. 使用 Markets 的 `AgentContractValidator` 验证契约合规性

### Agent 能力枚举

可用的 `AgentCapability` 枚举值：

| 能力 | 说明 |
|------|------|
| `ARCHITECTURE_DESIGN` | 架构设计 |
| `CODE_GENERATION` | 代码生成 |
| `CODE_REVIEW` | 代码审查 |
| `DEBUGGING` | 调试 |
| `DOCUMENTATION` | 文档生成 |
| `TEST_GENERATION` | 测试生成 |
| `OPTIMIZATION` | 优化 |

## 与其他模块的关系

| 模块 | 关系 |
|------|------|
| **openlab/core/** | Agent 继承 Core 的 `Agent` 基类，使用 `AgentCapability`、`AgentContext`、`AgentStatus`、`TaskResult` |
| **contrib/skills/** | Agent 可调用 Skills 获取特定领域能力（如浏览器、数据库、GitHub 操作） |
| **contrib/strategies/** | Strategies 的 Dispatching 策略负责将任务分配给合适的 Agent |
| **markets/** | Agent 契约通过 Markets 的 `AgentContractValidator` 进行验证和分发 |

## 依赖关系

- **核心依赖**: Python >= 3.10, openlab.core（Agent 基类、核心抽象）
- **协议依赖**: AgentOS protocols 层（JSON-RPC 2.0）

---

© 2026 SPHARX Ltd. All Rights Reserved.
