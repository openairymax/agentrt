# Contributions — 社区贡献

**模块路径**: `agentos/openlab/contrib/`
**版本**: v0.1.0

> **Status**: 本模块作为 AgentOS v0.1.0 的正式组成部分，API 已稳定。本模块通过 JSON-RPC 2.0 协议与 AgentOS 核心运行时集成。

## 概述

Contributions 是 OpenLab 生态系统的社区贡献层，汇聚社区驱动的技能模块、调度策略和 Agent 实现。该模块提供三大贡献体系：Skills（可复用技能）、Strategies（调度与规划策略）和 Agents（角色化智能体），构建开放、协作的 Agent 能力生态。

## 架构定位

```
+-------------------------------------------------------------------+
|                         OpenLab 开放生态                            |
+-------------------------------------------------------------------+
|  +------------------+  +------------------+  +------------------+ |
|  |   Applications   |  |   Contributions  |  |    Markets       | |
|  |  (app/)          |  |  (contrib/)      |  |  (markets/)      | |
|  |                  |  |                  |  |                  | |
|  | • DocGen         |  | • Skills         |  | • Templates      | |
|  | • E-Commerce     |  | • Strategies     |  | • Agent Market   | |
|  | • Research       |  | • Agents         |  | • Skill Market   | |
|  | • VideoEdit      |  |                  |  |                  | |
|  +------------------+  +------------------+  +------------------+ |
+-------------------------------------------------------------------+
|                     OpenLab Core (openlab/)                        |
|  Agent Registry | Task Scheduler | Tool Executor | Storage         |
+-------------------------------------------------------------------+
```

Contributions 位于 OpenLab 顶层，是社区能力的汇聚入口。社区贡献的组件通过 Markets 的契约验证和安装器进行分发，通过 Core 的注册表和调度器进行运行时管理。

## 目录结构

```
contrib/
├── skills/                         # 可复用技能模块
│   ├── browser_skill/              # 浏览器自动化技能
│   ├── database_skill/             # 数据库操作技能
│   ├── github_skill/               # GitHub 集成技能
│   └── README.md                   # Skills 模块文档
├── strategies/                     # 调度和规划策略
│   ├── dispatching/                # 任务调度策略
│   ├── planning/                   # 任务规划策略
│   ├── __init__.py                 # 策略包导出
│   └── README.md                   # Strategies 模块文档
└── agents/                         # 社区贡献 Agent
    ├── architect/                  # 架构师 Agent
    ├── backend/                    # 后端开发 Agent
    ├── frontend/                   # 前端开发 Agent
    ├── devops/                     # DevOps Agent
    ├── security/                   # 安全审计 Agent
    ├── tester/                     # 测试生成 Agent
    ├── product_manager/            # 产品经理 Agent
    └── README.md                   # Agents 模块文档
```

## 贡献体系

### Skills — 可复用技能模块

技能是 Agent 的能力单元，提供特定领域的操作接口。每个 Skill 独立封装，可被多个 Agent 复用。

| Skill | 路径 | 核心能力 |
|-------|------|----------|
| **Browser Skill** | `skills/browser_skill/` | 浏览器自动化：网页导航、数据抓取、表单操作、页面交互 |
| **Database Skill** | `skills/database_skill/` | 数据库操作：多库支持、安全查询、模式探索、数据导出 |
| **GitHub Skill** | `skills/github_skill/` | GitHub 集成：仓库管理、Issue/PR 管理、CI/CD、代码审查 |

> 各 Skill 的详细文档请参阅其目录下的 README.md。

### Strategies — 调度与规划策略

策略定义了 Agent 协作的模式，包括任务如何分配和如何分解。

| Strategy | 路径 | 核心能力 |
|----------|------|----------|
| **Dispatching** | `strategies/dispatching/` | 任务调度：将任务分发给最合适的 Agent 执行 |
| **Planning** | `strategies/planning/` | 任务规划：将复杂目标分解为可执行的步骤序列 |

> 各 Strategy 的详细文档请参阅其目录下的 README.md。

### Agents — 角色化智能体

Agent 是完整的智能体实现，具备特定的角色能力和交互模式。每个 Agent 继承自 `openlab.core.Agent` 基类，遵循统一的契约规范。

| Agent | 路径 | 核心能力 | Agent ID |
|-------|------|----------|----------|
| **Architect** | `agents/architect/` | 架构设计、技术选型、架构评审 | `contrib-architect` |
| **Backend** | `agents/backend/` | API 设计、数据库开发、服务架构、性能优化 | `contrib-backend` |
| **Frontend** | `agents/frontend/` | 前端开发、UI 生成 | `contrib-frontend` |
| **DevOps** | `agents/devops/` | 部署自动化、运维优化 | `contrib-devops` |
| **Security** | `agents/security/` | 安全审计、漏洞检测 | `contrib-security` |
| **Tester** | `agents/tester/` | 测试生成、质量保障 | `contrib-tester` |
| **Product Manager** | `agents/product_manager/` | 产品规划、需求文档 | `contrib-product-manager` |

> 各 Agent 的详细文档请参阅其目录下的 README.md。

## 贡献规范

### Agent 贡献结构

每个社区贡献的 Agent 应遵循以下目录结构：

```
<agent_name>/
├── __init__.py          # 模块导出
├── agent.py             # Agent 实现（继承 openlab.core.Agent）
├── contract.json        # Agent 契约（符合 markets 契约 Schema）
├── prompts/             # 提示词模板
│   ├── system1.md       # 主系统提示词
│   └── system2.md       # 辅助系统提示词
└── README.md            # Agent 文档
```

### Skill 贡献结构

每个社区贡献的 Skill 应遵循以下目录结构：

```
<skill_name>/
├── __init__.py          # 模块导出
├── skill.py             # Skill 核心实现
├── contract.json        # Skill 契约（符合 markets 契约 Schema）
└── README.md            # Skill 文档
```

### Strategy 贡献结构

每个社区贡献的策略应遵循以下目录结构：

```
<strategy_name>/
├── __init__.py          # 模块导出
├── <strategy>.py        # 策略核心实现
└── README.md            # 策略文档
```

## 使用指南

### 使用社区 Agent

```python
from contrib.agents.architect import ArchitectAgent
from contrib.agents.backend import BackendAgent

# 创建架构师 Agent
architect = ArchitectAgent(config={"llm_provider": "openai"})
await architect.initialize()

result = await architect.execute(
    input_data={"task_type": "design", "requirements": {...}},
    context=agent_context
)
```

### 使用社区 Skill

```python
from contrib.skills.browser_skill import BrowserSkill
from contrib.skills.database_skill import DatabaseSkill

# 浏览器自动化
browser = BrowserSkill(headless=True)
await browser.navigate("https://example.com")

# 数据库操作
db = DatabaseSkill()
await db.connect(db_type="postgresql", host="localhost", database="agentos")
```

### 使用社区 Strategy

```python
from contrib.strategies.dispatching import DispatchingStrategy
from contrib.strategies.planning import PlanningStrategy

# 任务调度
dispatcher = DispatchingStrategy()
result = await dispatcher.dispatch(task, agents=available_agents)

# 任务规划
planner = PlanningStrategy()
plan = await planner.plan(task={"description": "开发 REST API 服务"})
```

## 与其他模块的关系

| 模块 | 关系 |
|------|------|
| **markets/** | 社区贡献的 Agent/Skill 通过 Markets 的契约验证器进行合规校验，通过安装器进行分发 |
| **openlab/core/** | Agent 继承 Core 的 `Agent` 基类，使用 `AgentCapability`、`AgentContext`、`TaskResult` 等核心抽象 |
| **app/** | 应用层可组合使用 contrib 中的 Agent、Skill 和 Strategy 构建复杂应用 |
| **openlab/protocols/** | 所有模块间交互基于 JSON-RPC 2.0 协议规范 |

## 依赖关系

- **核心依赖**: Python >= 3.10, openlab.core（Agent 基类、核心抽象）
- **Skill 依赖**: Playwright/Selenium（Browser）、SQLAlchemy（Database）、PyGithub（GitHub）
- **协议依赖**: AgentOS protocols 层（JSON-RPC 2.0）

---

© 2026 SPHARX Ltd. All Rights Reserved.
