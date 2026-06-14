# GitHub Skill — GitHub 平台集成技能

**模块路径**: `ecosystem/openlab/contrib/skills/github_skill/`
**版本**: v0.1.0

> **Status**: 本模块作为 AgentOS 的正式组成部分，API 持续演进中。本模块通过 JSON-RPC 2.0 协议与 AgentOS 核心运行时集成。

## 概述

GitHub Skill 为 AgentOS 提供全面的 GitHub 平台集成能力，支持版本仓库管理、Issue 跟踪、代码审查、CI/CD 操作等核心功能。该模块通过 GitHub REST API 和 GraphQL API 实现与 GitHub 生态的深度交互，内置速率限制处理和错误重试机制。

## 目录结构

```
github_skill/
└── README.md                   # 本文件
```

> **注意**: 本技能当前为规范定义阶段，源代码尚未实现。

## 核心能力

- **仓库管理**：创建、删除、Fork、Star 仓库，管理分支和标签
- **Issue 管理**：创建、分配、标记、关闭 Issue，支持批量操作和模板化
- **Pull Request 管理**：创建 PR、代码审查、合并策略控制、自动检测冲突
- **代码审查**：提交评论、行级审查、状态检查、合规性验证
- **CI/CD 集成**：触发 Actions 工作流、查看运行状态、获取构建日志
- **内容管理**：文件创建与更新、提交管理、Release 发布、Wiki 操作
- **项目管理**：Milestone 管理、看板操作、项目卡片流转
- **搜索功能**：代码搜索、Issue 搜索、仓库搜索

## 接口说明

### 仓库操作

| 接口 | 说明 | 参数 |
|------|------|------|
| `get_repo` | 获取仓库信息 | repo |
| `create_repo` | 创建仓库 | name, description, private |
| `fork_repo` | Fork 仓库 | repo |
| `list_branches` | 列出分支 | repo |
| `list_tags` | 列出标签 | repo |

### Issue 操作

| 接口 | 说明 | 参数 |
|------|------|------|
| `create_issue` | 创建 Issue | repo, title, body, labels, assignees |
| `list_issues` | 查询 Issue 列表 | repo, state, labels, since |
| `update_issue` | 更新 Issue | repo, issue_number, title, body, state |
| `add_comment` | 添加评论 | repo, issue_number, body |

### Pull Request 操作

| 接口 | 说明 | 参数 |
|------|------|------|
| `create_pr` | 创建 Pull Request | repo, title, head, base, body |
| `review_pr` | 审查 Pull Request | repo, pr_number, action, body |
| `merge_pr` | 合并 Pull Request | repo, pr_number, merge_method |
| `list_pr_reviews` | 列出审查记录 | repo, pr_number |

### CI/CD 操作

| 接口 | 说明 | 参数 |
|------|------|------|
| `trigger_workflow` | 触发 Actions | repo, workflow_id, ref, inputs |
| `list_workflow_runs` | 查看工作流运行 | repo, workflow_id, status |
| `get_workflow_run` | 获取运行详情 | repo, run_id |
| `get_workflow_logs` | 获取构建日志 | repo, run_id |

### 内容操作

| 接口 | 说明 | 参数 |
|------|------|------|
| `get_contents` | 获取文件内容 | repo, path, ref |
| `create_file` | 创建文件 | repo, path, content, message |
| `update_file` | 更新文件 | repo, path, content, sha, message |
| `create_release` | 创建 Release | repo, tag, name, body, prerelease |
| `search_code` | 搜索代码 | query, language, repo |

## 配置说明

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `token` | GitHub 个人访问令牌 | 必填 |
| `base_url` | GitHub API 地址 | `https://api.github.com` |
| `timeout` | 请求超时时间（秒） | 30 |
| `retry_count` | 失败重试次数 | 3 |
| `graphql_enabled` | 启用 GraphQL 查询 | `true` |
| `webhook_secret` | Webhook 密钥（可选） | 空 |

## 依赖关系

- **核心依赖**: AgentOS OpenLab Core, PyGithub >= 1.59.0, requests >= 2.31.0
- **安装**: 已包含在核心依赖中

## 安全说明

- Token 建议使用环境变量 `GITHUB_TOKEN` 配置，避免硬编码
- 建议使用具有最小必要权限的 Fine-grained Token
- Webhook 请求建议验证签名确保来源可信
- API 请求遵循 GitHub 速率限制策略，自动处理 Retry-After 头

---

© 2026 SPHARX Ltd. All Rights Reserved.
