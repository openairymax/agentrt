# Ecosystem — 生态层

`ecosystem/`

## 概述

`ecosystem/` 是 AgentRT 的生态层，包含开放应用平台（OpenLab）、统一配置管理中心（Manager）、Prompt 模板体系、Hook 系统和技能注册等五大子系统。生态层构建于 `agentos/` 核心引擎之上，通过 JSON-RPC 2.0 协议与守护进程层通信，为上层应用和开发者提供完整的生态支撑。

## 目录结构

```
ecosystem/
├── openlab/               # 开放应用平台 — 应用/贡献/市场/核心管理
│   ├── openlab/           #   核心管理模块（Agent/Task/Tool/Storage）
│   ├── app/               #   官方应用（DocGen/E-Commerce/Research/VideoEdit）
│   ├── contrib/           #   社区贡献（Skills/Strategies/Agents）
│   ├── markets/           #   应用市场（Agent/Skill/Template 资源管理）
│   └── tests/             #   单元测试
├── manager/               # 统一配置管理中心 — 多模块 Schema + 热重载
│   ├── schema/            #   JSON Schema 定义（11 个 Schema，272 项校验规则）
│   ├── agent/             #   Agent 配置与注册表
│   ├── kernel/            #   内核级配置（调度/内存/IPC/定时器）
│   ├── model/             #   模型配置（Provider/参数/上下文窗口）
│   ├── security/          #   安全配置（认证/授权/加密/策略）
│   ├── logging/           #   日志配置（级别/格式/输出目标/轮转）
│   ├── deployment/        #   部署配置（环境/资源/端点/监控）
│   ├── environment/       #   环境配置（development/staging/production）
│   ├── monitoring/        #   监控配置（告警规则/Grafana 仪表盘）
│   ├── sanitizer/         #   清洗器配置（XSS/SQL 注入规则/Valgrind 抑制）
│   ├── service/           #   服务配置（tool_d 等守护进程）
│   ├── skill/             #   技能注册表配置
│   ├── audit/             #   审计配置（事件分类/保留策略/SHA-256 校验）
│   ├── tools/             #   配置工具（差异比较/版本清理/审计日志生成/漂移检测）
│   ├── tests/             #   配置测试
│   └── benchmark/         #   性能基准测试
├── prompts/               # Prompt 模板体系 — 四类模板 + 调优器
│   ├── templates/         #   模板目录
│   │   ├── cognition/     #     认知类（意图分类/实体提取/规划生成/反思）
│   │   ├── memory/        #     记忆类（事实提取/去重决策/规则生成/摘要）
│   │   ├── security/      #     安全类（代码审查/输入验证/安全扫描）
│   │   └── system/        #     系统类（默认/编码/研究 Agent 系统提示词）
│   ├── tuner/             #   Prompt 调优器（评估与优化）
│   └── registry.yaml      #   模板注册表
├── hooks/                 # Hook 系统 — 生命周期回调
│   ├── audit_hook.py      #   审计 Hook（操作记录与追踪）
│   ├── cost_tracker.py    #   成本追踪 Hook（Token 消耗统计）
│   ├── prompt_injector.py #   Prompt 注入 Hook（动态模板注入）
│   └── security_reminder.py # 安全提醒 Hook（安全策略注入）
└── skills/                # 技能注册目录（预留）
```

## 核心子系统

### OpenLab — 开放应用平台

OpenLab 是 AgentRT 的开放生态系统，提供四大能力体系：

| 模块 | 说明 | 详细文档 |
|------|------|----------|
| **openlab/** | 核心管理 — Agent/Task/Tool/Storage 抽象层 | [openlab/README.md](openlab/README.md) |
| **app/** | 官方应用 — DocGen/E-Commerce/Research/VideoEdit | [app/README.md](openlab/app/README.md) |
| **contrib/** | 社区贡献 — Skills/Strategies/Agents | [contrib/README.md](openlab/contrib/README.md) |
| **markets/** | 应用市场 — 资源注册与模板 | [markets/README.md](openlab/markets/README.md) |

### Manager — 统一配置管理中心

Manager 按 16 个领域模块组织配置文件和 Schema，支持热重载和多环境覆盖：

| 模块 | 说明 | 详细文档 |
|------|------|----------|
| **schema/** | 11 个 JSON Schema，272 项校验规则 | [schema/README.md](manager/schema/README.md) |
| **agent/** | Agent 配置与注册表 | [agent/README.md](manager/agent/README.md) |
| **kernel/** | 内核级配置 | [kernel/README.md](manager/kernel/README.md) |
| **model/** | 模型配置 | [model/README.md](manager/model/README.md) |
| **security/** | 安全配置 | [security/README.md](manager/security/README.md) |
| **logging/** | 日志配置 | [logging/README.md](manager/logging/README.md) |
| **deployment/** | 部署配置 | [deployment/README.md](manager/deployment/README.md) |
| **environment/** | 环境配置 | [environment/README.md](manager/environment/README.md) |
| **monitoring/** | 监控配置 | [monitoring/README.md](manager/monitoring/README.md) |
| **sanitizer/** | 清洗器配置 | [sanitizer/README.md](manager/sanitizer/README.md) |
| **service/** | 服务配置 | [service/README.md](manager/service/README.md) |
| **skill/** | 技能注册表 | [skill/README.md](manager/skill/README.md) |
| **audit/** | 审计配置 | [audit/README.md](manager/audit/README.md) |
| **tools/** | 配置工具 | [tools/README.md](manager/tools/README.md) |
| **tests/** | 配置测试 | [tests/README.md](manager/tests/README.md) |
| **benchmark/** | 性能基准 | [benchmark/README.md](manager/benchmark/README.md) |

### Prompts — Prompt 模板体系

四类 Prompt 模板 + 调优器，支持动态注入和版本管理：

| 类别 | 模板 | 用途 |
|------|------|------|
| **cognition** | intent_classify / entity_extract / plan_generate / reflection | 认知循环各阶段提示词 |
| **memory** | extract_facts / dedup_decision / rule_generate / summarize | 记忆管理各阶段提示词 |
| **security** | code_review / input_validate / security_scan | 安全审查提示词 |
| **system** | default_agent / coding_agent / research_agent | Agent 系统提示词 |

### Hooks — Hook 系统

生命周期回调机制，在关键执行点注入自定义逻辑：

| Hook | 触发点 | 功能 |
|------|--------|------|
| `audit_hook.py` | 操作执行前后 | 记录操作审计日志 |
| `cost_tracker.py` | LLM 调用前后 | 追踪 Token 消耗与成本 |
| `prompt_injector.py` | Prompt 构建时 | 动态注入模板变量 |
| `security_reminder.py` | 工具执行前 | 注入安全策略提醒 |

## 依赖关系

```
ecosystem/openlab/  ← agentos/daemon/ (JSON-RPC 2.0)
ecosystem/manager/  ← ecosystem/openlab/ (配置消费)
ecosystem/prompts/  ← ecosystem/openlab/ (模板注入)
ecosystem/hooks/    ← agentos/atoms/coreloopthree/ (生命周期回调)
ecosystem/skills/   ← ecosystem/openlab/contrib/skills/ (技能注册)
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
