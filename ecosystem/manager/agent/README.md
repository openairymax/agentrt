# Manager Agent — Agent 注册表配置

**模块路径**: `ecosystem/manager/agent/`
**版本**: v0.1.0

## 概述

`manager/agent/` 包含 AgentRT 的 Agent 注册表配置，定义了 12 个内置 Agent 和 1 个自定义模板的完整注册信息。每个 Agent 包含角色定义、能力声明、双系统模型配置、权限要求、成本概览、信任指标和资源限制等维度，遵循 `schema/agent-registry.schema.json` 规范。

## 目录结构

```
agent/
└── registry.yaml          # Agent 注册表定义（12 个内置 Agent + 1 个自定义模板）
```

## 核心组件

### registry.yaml

Agent 注册表，每个 Agent 包含以下字段：

| 字段 | 说明 |
|------|------|
| `agent_id` | Agent 唯一标识符 |
| `role` | 角色类型 |
| `source` | 来源（builtin/custom） |
| `capabilities` | 能力列表（name/description/input_schema/output_schema/estimated_tokens） |
| `models` | 双系统模型配置（system1 快速响应 / system2 深度推理） |
| `required_permissions` | 权限声明 |
| `cost_profile` | 成本概览（token 单价/预算/成本中心） |
| `trust_metrics` | 信任指标（trust_score/reliability_score/quality_score/security_score） |
| `resource_limits` | 资源限制（memory/cpu/timeout/concurrent_tasks） |

## 内置 Agent 列表

| Agent ID | 角色 | 核心能力 |
|----------|------|----------|
| `product_manager_v1` | 产品经理 | 需求分析、项目规划、利益相关者沟通、路线图 |
| `architect_v1` | 架构师 | 架构设计、技术选型、代码审查、性能分析 |
| `frontend_v1` | 前端开发 | UI 开发、CSS 样式、JavaScript 编码、响应式设计 |
| `backend_v1` | 后端开发 | API 开发、数据库设计、服务端编码、性能优化 |
| `tester_v1` | 测试 | 测试用例设计、自动化测试、缺陷检测、质量保证 |
| `devops_v1` | DevOps | 部署、CI/CD、监控、基础设施管理 |
| `security_v1` | 安全审计 | 安全审计、漏洞检测、合规检查、渗透测试 |
| `data_engineer_v1` | 数据工程师 | 数据管道、ETL 开发、数据建模、数据分析 |
| `coordinator_v1` | 协调者 | 任务协调、资源分配、冲突解决、进度跟踪 |
| `reviewer_v1` | 代码审查 | 代码审查、最佳实践、重构建议、文档审查 |
| `analyst_v1` | 分析师 | 数据分析、趋势检测、报告生成、数据可视化 |
| `custom_template_v1` | 自定义模板 | 可配置的自定义 Agent（默认禁用） |

## 依赖关系

| 组件 | 用途 |
|------|------|
| `schema/agent-registry.schema.json` | 注册表格式校验 |
| PyYAML | YAML 配置解析 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
