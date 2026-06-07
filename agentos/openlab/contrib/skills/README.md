# Skills — 可复用技能模块

**模块路径**: `agentos/openlab/contrib/skills/`
**版本**: v0.1.0

> **Status**: 本模块作为 AgentOS v0.1.0 的正式组成部分，API 已稳定。本模块通过 JSON-RPC 2.0 协议与 AgentOS 核心运行时集成。

## 概述

Skills 是 OpenLab 社区贡献的可复用技能模块集合，为 Agent 提供特定领域的操作能力。每个 Skill 独立封装，提供标准化的接口和配置，可被多个 Agent 复用和组合。当前包含三大技能：Browser Skill（浏览器自动化）、Database Skill（数据库操作）和 GitHub Skill（GitHub 平台集成），覆盖 Web 自动化、数据管理和代码协作三大核心场景。

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
|  | • Browser Skill  |  | • Dispatching    |  | • Architect      | |
|  | • Database Skill |  | • Planning       |  | • Backend        | |
|  | • GitHub Skill   |  |                  |  | • Frontend       | |
|  +------------------+  +------------------+  | • ...            | |
|                                              +------------------+ |
+-------------------------------------------------------------------+
|                     OpenLab Core (openlab/)                        |
|  Agent Registry | Task Scheduler | Tool Executor | Storage         |
+-------------------------------------------------------------------+
```

Skills 作为 Contributions 三大子模块之一，是 Agent 的能力供给层。Agent 通过调用 Skills 获得特定领域的操作能力，Strategies 通过 Skills 了解 Agent 的能力边界进行任务调度。

## 目录结构

```
skills/
├── browser_skill/                  # 浏览器自动化技能
│   └── README.md                   # Browser Skill 详细文档
├── database_skill/                 # 数据库操作技能
│   └── README.md                   # Database Skill 详细文档
├── github_skill/                   # GitHub 集成技能
│   └── README.md                   # GitHub Skill 详细文档
└── README.md                       # 本文件
```

## 技能列表

| Skill | 路径 | 领域 | 核心能力 |
|-------|------|------|----------|
| **Browser Skill** | `browser_skill/` | Web 自动化 | 网页导航、数据抓取、表单操作、页面交互、截图 |
| **Database Skill** | `database_skill/` | 数据管理 | 多库支持、安全查询、模式探索、数据导出、事务 |
| **GitHub Skill** | `github_skill/` | 代码协作 | 仓库管理、Issue/PR 管理、CI/CD、代码审查、搜索 |

## 技能体系

### Browser Skill — 浏览器自动化

基于 Playwright/Selenium 的浏览器控制能力，适用于 Web 自动化测试、数据采集和页面监控。

| 能力 | 说明 |
|------|------|
| 网页导航 | URL 跳转、标签页管理、前进/后退/刷新 |
| 数据抓取 | CSS Selector/XPath 元素提取、表格数据抓取、截图 |
| 表单操作 | 自动填写、提交、文件上传、下拉选择 |
| 页面交互 | 点击、滚动、悬停、键盘输入、拖拽 |
| 等待策略 | 显式等待、隐式等待、条件等待 |

> 详细接口和配置请参阅 `browser_skill/README.md`。

### Database Skill — 数据库操作

基于 SQLAlchemy 的多数据库操作能力，内置 SQL 注入防护，适用于数据分析、报表生成和数据库管理。

| 能力 | 说明 |
|------|------|
| 多库支持 | MySQL、PostgreSQL、SQLite |
| 安全查询 | 参数化查询，防止 SQL 注入 |
| 模式探索 | 自动发现表结构、列信息和关系 |
| 数据导出 | 查询结果导出为 JSON、CSV 格式 |
| 连接池 | 自动管理连接池，优化连接复用 |
| 事务支持 | 支持事务操作，确保数据一致性 |

> 详细接口和配置请参阅 `database_skill/README.md`。

### GitHub Skill — GitHub 平台集成

基于 GitHub REST API 和 GraphQL API 的深度集成能力，内置速率限制处理和错误重试机制。

| 能力 | 说明 |
|------|------|
| 仓库管理 | 创建、删除、Fork、Star，管理分支和标签 |
| Issue 管理 | 创建、分配、标记、关闭，支持批量操作 |
| PR 管理 | 创建 PR、代码审查、合并策略控制 |
| CI/CD 集成 | 触发 Actions、查看运行状态、获取构建日志 |
| 内容管理 | 文件创建与更新、Release 发布、Wiki 操作 |
| 搜索功能 | 代码搜索、Issue 搜索、仓库搜索 |

> 详细接口和配置请参阅 `github_skill/README.md`。

## 技能接口规范

所有 Skills 遵循统一的接口规范：

### 生命周期

```python
class Skill:
    async def initialize(self)       # 初始化技能资源
    async def execute(self, action, params)  # 执行技能操作
    async def cleanup(self)          # 清理技能资源
```

### 契约规范

每个 Skill 应配备 `contract.json`，遵循 Markets 的 Skill 契约 Schema，包含以下必填字段：

| 字段 | 说明 |
|------|------|
| `name` | 技能名称 |
| `version` | 语义化版本号 |
| `description` | 功能描述 |
| `capabilities` | 能力列表 |
| `interface` | 接口类型（stdio/http_rest/websocket/grpc/message_queue/function_call） |
| `permissions` | 权限声明（filesystem/network/process/memory/storage/system） |

## 使用指南

### 组合使用多个 Skill

```python
from contrib.skills.browser_skill import BrowserSkill
from contrib.skills.database_skill import DatabaseSkill
from contrib.skills.github_skill import GitHubSkill

# 浏览器抓取 → 数据库存储
browser = BrowserSkill(headless=True)
db = DatabaseSkill()

await db.connect(db_type="postgresql", host="localhost", database="agentos")

await browser.navigate("https://example.com/data")
data = await browser.extract(selector=".data-table", attribute="text")

await db.execute(
    "INSERT INTO scraped_data (content) VALUES (:content)",
    {"content": data}
)

await browser.close()
await db.disconnect()
```

### Agent 集成 Skill

```python
from contrib.agents.backend import BackendAgent
from contrib.skills.database_skill import DatabaseSkill
from contrib.skills.github_skill import GitHubSkill

# 后端 Agent 使用数据库和 GitHub 技能
agent = BackendAgent(config={
    "skills": ["database_skill", "github_skill"]
})
```

### Skill 契约验证

```python
from markets.skills.contracts.validator import SkillContractValidator

validator = SkillContractValidator()
contract = validator.load(Path("skill_contract.yaml"))
errors = validator.validate()
```

## 开发指南

### 创建新的社区 Skill

1. 在 `contrib/skills/` 下创建新目录（使用小写字母和下划线，以 `_skill` 结尾）
2. 实现技能核心逻辑，遵循统一的接口规范
3. 编写 `contract.json`，遵循 Markets 的 Skill 契约 Schema
4. 编写 `README.md`，包含概述、核心能力、接口说明、配置参数、依赖关系和使用示例
5. 使用 Markets 的 `SkillContractValidator` 验证契约合规性
6. 通过 Markets 的 `SkillInstallerCLI` 测试安装流程

### Skill 权限声明

创建 Skill 时需声明所需权限，可用的权限范围：

| 权限范围 | 说明 |
|----------|------|
| `filesystem:read` / `filesystem:write` | 文件系统读写 |
| `network:http` / `network:ws` | 网络 HTTP/WebSocket 访问 |
| `process:spawn` | 子进程创建 |
| `memory:read` / `memory:write` | 内存读写 |
| `storage:local` / `storage:cache` | 本地存储和缓存 |
| `system:info` / `system:metrics` | 系统信息和指标 |

## 与其他模块的关系

| 模块 | 关系 |
|------|------|
| **contrib/agents/** | Agent 调用 Skills 获取特定领域能力 |
| **contrib/strategies/** | Strategies 根据 Agent 拥有的 Skills 进行任务调度 |
| **markets/skills/** | Skill 契约通过 Markets 的 `SkillContractValidator` 验证，通过 `SkillInstallerCLI` 分发 |
| **openlab/core/** | Skills 通过 Core 的 Tool 抽象注册到运行时 |
| **app/** | 应用层可组合使用 Skills 构建特定领域的应用 |

## 依赖关系

| Skill | 核心依赖 | 可选依赖 |
|-------|----------|----------|
| Browser Skill | AgentOS OpenLab Core | Playwright >= 1.40.0 或 Selenium >= 4.15.0 |
| Database Skill | AgentOS OpenLab Core, SQLAlchemy | psycopg2 (PostgreSQL), pymysql (MySQL) |
| GitHub Skill | AgentOS OpenLab Core, PyGithub >= 1.59.0, requests >= 2.31.0 | — |

- **Python**: >= 3.10
- **协议依赖**: AgentOS protocols 层（JSON-RPC 2.0）

---

© 2026 SPHARX Ltd. All Rights Reserved.
