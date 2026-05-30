# OpenLab — 开放生态系统

**模块路径**: `agentos/openlab/`
**版本**: v0.0.5

> **Preview Status**: OpenLab 当前处于预览/开发阶段，API 和功能可能在未来版本中发生变化。

## 概述

OpenLab 是 AgentOS 的开放生态系统层，提供应用（Applications）、社区贡献（Contributions）、市场（Markets）和核心管理（Core）四大能力体系，构建开放、协作的 Agent 开发生态。各模块通过 JSON-RPC 2.0 协议与 AgentOS 核心运行时集成，并通过 protocols 层进行通信。OpenLab 遵循 AgentOS 架构设计原则 V1.8，实现了生产级多智能体编排框架。

## 设计目标

- **开放协作**：社区驱动的应用和技能共享平台
- **快速开发**：提供模板和示例，加速 Agent 应用开发
- **市场流通**：Agent、Skill、Tool 的统一分发和市场机制
- **生态兼容**：支持第三方贡献，灵活集成外部能力
- **协议统一**：所有模块间交互基于 JSON-RPC 2.0 协议规范

## 目录结构

```
openlab/
├── openlab/                    # 核心管理模块
│   ├── core/                   # 核心组件（Agent/Task/Tool/Storage）
│   │   ├── agent.py            # Agent 基类、注册表、状态管理
│   │   ├── task.py             # 任务调度、状态机、执行计划
│   │   ├── tool.py             # 工具抽象、注册表、执行器
│   │   └── storage.py          # 存储抽象（Memory/SQLite）
│   ├── agents/                 # 预构建 Agent 实现
│   │   └── architect/          # 架构师 Agent
│   ├── protocols/              # 协议处理模块
│   ├── utils/                  # 工具函数
│   │   ├── exceptions.py       # 异常层级定义
│   │   └── logging.py          # 日志配置
│   ├── config.yaml             # 核心配置文件
│   └── requirements.txt        # Python 依赖
├── app/                        # 官方智能应用
│   ├── docgen/                 # 文档生成应用
│   ├── ecommerce/              # 电商助手应用
│   ├── research/               # 研究助手应用
│   └── videoedit/              # 视频编辑应用
├── contrib/                    # 社区贡献
│   ├── skills/                 # 可复用技能模块
│   │   ├── browser_skill/      # 浏览器自动化技能
│   │   ├── database_skill/     # 数据库操作技能
│   │   └── github_skill/       # GitHub 集成技能
│   ├── strategies/             # 调度和规划策略
│   │   ├── dispatching/        # 任务调度策略
│   │   └── planning/           # 任务规划策略
│   └── agents/                 # 社区贡献 Agent
│       ├── architect/          # 架构师
│       ├── backend/            # 后端开发
│       ├── frontend/           # 前端开发
│       ├── devops/             # DevOps
│       ├── security/           # 安全
│       ├── tester/             # 测试
│       └── product_manager/    # 产品经理
├── markets/                    # 市场与模板
│   ├── templates/              # 项目模板
│   │   ├── python-agent/       # Python Agent 模板
│   │   └── rust-skill/         # Rust Skill 模板
│   ├── agents/                 # Agent 市场
│   │   ├── contracts/          # 契约验证
│   │   ├── installer/          # 安装器 CLI
│   │   └── registry/           # 注册索引
│   └── skills/                 # Skill 市场
│       ├── contracts/          # 契约验证
│       ├── installer/          # 安装器 CLI
│       └── registry/           # 注册索引
├── tests/                      # 测试套件
│   └── unit/                   # 单元测试
├── pyproject.toml              # 项目构建配置
├── Dockerfile                  # 容器化构建
└── README.md                   # 本文件
```

## 核心模块

| 模块 | 路径 | 说明 |
|------|------|------|
| **OpenLab 核心** | `openlab/` | 生态系统的核心管理与调度能力（Agent/Task/Tool/Storage/Protocols/Utils） |
| **应用市场** | `app/` | 官方和社区驱动的 Agent 应用（DocGen/E-Commerce/Research/VideoEdit） |
| **社区贡献** | `contrib/` | 社区贡献的技能、策略和 Agent 实现 |
| **市场模板** | `markets/` | 可复用的项目模板和分发机制 |

## 架构

```
+-------------------------------------------------------------------+
|                         OpenLab 开放生态                            |
+-------------------------------------------------------------------+
|  +------------------+  +------------------+  +------------------+ |
|  |   Applications   |  |   Contributions  |  |    Markets       | |
|  |  (app/)          |  |  (contrib/)      |  |  (markets/)      | |
|  |                  |  |                  |  |                  | |
|  | • DocGen         |  | • Skills         |  | • Templates      | |
|  | • E-Commerce     |  | • Strategies     |  | • Agents         | |
|  | • Research       |  | • Agents         |  | • Skills         | |
|  | • VideoEdit      |  |                  |  |                  | |
|  +------------------+  +------------------+  +------------------+ |
+-------------------------------------------------------------------+
|                     OpenLab Core (openlab/)                        |
|  Agent Registry | Task Scheduler | Tool Executor | Storage         |
+-------------------------------------------------------------------+
|                        AgentOS 核心运行时 (protocols)              |
+-------------------------------------------------------------------+
```

> **注意**: OpenLab 通过 `agentos/protocols/` 协议层与 AgentOS 核心运行时通信，所有模块间交互均基于 JSON-RPC 2.0 协议规范。

## 应用概述

| 应用 | 路径 | 说明 |
|------|------|------|
| **DocGen** | `app/docgen/` | 智能文档生成，支持 Markdown/HTML/PDF 多格式输出，Jinja2 模板渲染 |
| **E-Commerce** | `app/ecommerce/` | 智能电商助手，支持商品管理、订单处理、Stripe 支付集成 |
| **Research** | `app/research/` | 智能研究助手，支持文献检索、数据分析、报告生成 |
| **VideoEdit** | `app/videoedit/` | 智能视频编辑，基于 FFmpeg，支持剪辑/合并/特效/字幕/GIF |

## 贡献指南

社区贡献分为三类：

| 贡献类型 | 目录 | 说明 |
|----------|------|------|
| **Skills** | `contrib/skills/` | 可复用的能力模块（Browser/Database/GitHub） |
| **Strategies** | `contrib/strategies/` | 调度和规划策略（Dispatching/Planning） |
| **Agents** | `contrib/agents/` | 完整的 Agent 实现（7 种角色 Agent） |

## 市场模板

| 模板 | 路径 | 说明 |
|------|------|------|
| **Python Agent** | `markets/templates/python-agent/` | Python Agent 项目模板 |
| **Rust Skill** | `markets/templates/rust-skill/` | Rust Skill 项目模板 |

## 依赖关系

- **核心依赖**: Python >= 3.10, FastAPI, Pydantic, SQLAlchemy, PyYAML
- **协议依赖**: AgentOS protocols 层（JSON-RPC 2.0）
- **可选依赖**: Redis, Stripe, OpenCV, MoviePy, Playwright, Selenium 等（按应用场景）

## 构建说明

```bash
# 安装核心包
pip install -e .

# 安装特定应用依赖
pip install -e ".[docgen]"
pip install -e ".[ecommerce]"
pip install -e ".[videoedit]"
pip install -e ".[browser]"

# Docker 构建
docker build -t agentos-openlab .
```

## 使用示例

```python
import asyncio
from openlab.core.agent import AgentManager, AgentRegistry
from openlab.core.task import TaskScheduler, TaskDefinition, TaskCategory
from openlab.agents.architect import ArchitectAgent

async def main():
    registry = AgentRegistry()
    manager = AgentManager(registry)

    agent = await manager.create_agent(
        agent_class=ArchitectAgent,
        agent_id="architect-001",
        manager={"verbose": True},
    )

    result = await agent.execute(context, input_data)
    print(result)

    await manager.shutdown()

asyncio.run(main())
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
