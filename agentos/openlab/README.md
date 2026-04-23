# OpenLab — 开放生态系统

`agentos/openlab/` 是 AgentOS 的开放生态系统层，提供应用、贡献、市场和模板四大能力体系，构建开放、协作的 Agent 开发生态。

## 设计目标

- **开放协作**：社区驱动的应用和技能共享平台
- **快速开发**：提供模板和示例，加速 Agent 应用开发
- **市场流通**：Agent、Skill、Tool 的统一分发和市场机制
- **生态兼容**：支持第三方贡献，灵活集成外部能力

## 核心模块

| 模块 | 路径 | 说明 |
|------|------|------|
| **OpenLab 核心** | `openlab/` | 生态系统的核心管理与调度能力 |
| **应用市场** | `app/` | 官方和社区驱动的 Agent 应用 |
| **社区贡献** | `contrib/` | 社区贡献的技能和策略 |
| **市场模板** | `markets/templates/` | 可复用的项目模板 |

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
|  | • E-Commerce     |  | • Strategies     |  | • Agent/Skill     | |
|  | • Research       |  | • Agents         |  |   Distributions  | |
|  | • VideoEdit      |  |                  |  |                  | |
|  +------------------+  +------------------+  +------------------+ |
+-------------------------------------------------------------------+
|                        AgentOS 核心运行时                           |
+-------------------------------------------------------------------+
```

## 应用概述

| 应用 | 路径 | 说明 |
|------|------|------|
| **DocGen** | `app/docgen/` | 智能文档生成应用 |
| **E-Commerce** | `app/ecommerce/` | 智能电商助手应用 |
| **Research** | `app/research/` | 智能研究助手应用 |
| **VideoEdit** | `app/videoedit/` | 智能视频编辑应用 |

## 贡献指南

社区贡献分为三类：

| 贡献类型 | 目录 | 说明 |
|----------|------|------|
| **Skills** | `contrib/skills/` | 可复用的能力模块 |
| **Strategies** | `contrib/strategies/` | 调度和规划策略 |
| **Agents** | `contrib/agents/` | 完整的 Agent 实现 |

## 市场模板

| 模板 | 路径 | 说明 |
|------|------|------|
| **Python Agent** | `markets/templates/python-agent/` | Python Agent 项目模板 |
| **Rust Skill** | `markets/templates/rust-skill/` | Rust Skill 项目模板 |

---

*AgentOS OpenLab — 开放生态系统*
