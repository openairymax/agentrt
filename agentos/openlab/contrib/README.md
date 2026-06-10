# Contributions — 社区贡献

**模块路径**: `agentos/openlab/contrib/`
**版本**: v0.1.0

> **Status**: 本模块作为 AgentOS 的正式组成部分，API 持续演进中。本模块通过 JSON-RPC 2.0 协议与 AgentOS 核心运行时集成。

## 概述

Contributions 是 OpenLab 生态系统的社区贡献层，汇聚社区驱动的技能模块、调度策略和 Agent 实现。该模块提供三大贡献体系：Skills（可复用技能）、Strategies（调度与规划策略）和 Agents（角色化智能体），构建开放、协作的 Agent 能力生态。

## 目录结构

```
contrib/
├── skills/                         # 可复用技能模块
│   ├── browser_skill/              # 浏览器自动化技能（规范定义阶段）
│   ├── database_skill/             # 数据库操作技能（规范定义阶段）
│   ├── github_skill/               # GitHub 集成技能（规范定义阶段）
│   └── README.md                   # Skills 模块文档
├── strategies/                     # 调度和规划策略
│   ├── dispatching/                # 任务调度策略
│   │   ├── __init__.py
│   │   ├── dispatching.py          # DispatchingStrategy 核心实现
│   │   └── README.md
│   ├── planning/                   # 任务规划策略
│   │   ├── __init__.py
│   │   ├── planning.py             # PlanningStrategy/PlanStep 核心实现
│   │   └── README.md
│   ├── __init__.py                 # 策略包导出
│   └── README.md                   # Strategies 模块文档
└── agents/                         # 社区贡献 Agent
    ├── architect/                  # 架构师 Agent
    │   ├── __init__.py
    │   ├── agent.py
    │   ├── contract.json
    │   └── prompts/
    │       ├── system1.md
    │       └── system2.md
    ├── backend/                    # 后端开发 Agent
    │   ├── __init__.py
    │   ├── agent.py
    │   ├── contract.json
    │   └── prompts/
    │       ├── system1.md
    │       └── system2.md
    ├── frontend/                   # 前端开发 Agent
    │   ├── __init__.py
    │   ├── agent.py
    │   ├── contract.json
    │   └── prompts/
    │       ├── system1.md
    │       └── system2.md
    ├── devops/                     # DevOps Agent
    │   ├── __init__.py
    │   ├── agent.py
    │   ├── contract.json
    │   └── prompts/
    │       ├── system1.md
    │       └── system2.md
    ├── security/                   # 安全审计 Agent
    │   ├── __init__.py
    │   ├── agent.py
    │   ├── contract.json
    │   └── prompts/
    │       ├── system1.md
    │       └── system2.md
    ├── tester/                     # 测试生成 Agent
    │   ├── __init__.py
    │   ├── agent.py
    │   ├── contract.json
    │   └── prompts/
    │       ├── system1.md
    │       └── system2.md
    ├── product_manager/            # 产品经理 Agent
    │   ├── __init__.py
    │   ├── agent.py
    │   ├── contract.json
    │   └── prompts/
    │       ├── system1.md
    │       └── system2.md
    └── README.md                   # Agents 模块文档
```

## 贡献体系

### Skills — 可复用技能模块

技能是 Agent 的能力单元，提供特定领域的操作接口。每个 Skill 独立封装，可被多个 Agent 复用。

| Skill | 路径 | 核心能力 | 状态 |
|-------|------|----------|------|
| **Browser Skill** | `skills/browser_skill/` | 浏览器自动化：网页导航、数据抓取、表单操作、页面交互 | 规范定义阶段 |
| **Database Skill** | `skills/database_skill/` | 数据库操作：多库支持、安全查询、模式探索、数据导出 | 规范定义阶段 |
| **GitHub Skill** | `skills/github_skill/` | GitHub 集成：仓库管理、Issue/PR 管理、CI/CD、代码审查 | 规范定义阶段 |

### Strategies — 调度与规划策略

策略定义了 Agent 协作的模式，包括任务如何分配和如何分解。

| Strategy | 路径 | 核心能力 | 状态 |
|----------|------|----------|------|
| **Dispatching** | `strategies/dispatching/` | 任务调度：将任务分发给最合适的 Agent 执行 | 骨架实现 |
| **Planning** | `strategies/planning/` | 任务规划：将复杂目标分解为可执行的步骤序列 | 骨架实现 |

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

## 贡献规范

### Agent 贡献结构

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

```
<skill_name>/
├── __init__.py          # 模块导出
├── skill.py             # Skill 核心实现
├── contract.json        # Skill 契约（符合 markets 契约 Schema）
└── README.md            # Skill 文档
```

### Strategy 贡献结构

```
<strategy_name>/
├── __init__.py          # 模块导出
├── <strategy>.py        # 策略核心实现
└── README.md            # 策略文档
```

## 依赖关系

- **核心依赖**: Python >= 3.10, openlab.core（Agent 基类、核心抽象）
- **Skill 依赖**: Playwright/Selenium（Browser）、SQLAlchemy（Database）、PyGithub（GitHub）
- **协议依赖**: AgentOS protocols 层（JSON-RPC 2.0）

---

© 2026 SPHARX Ltd. All Rights Reserved.
