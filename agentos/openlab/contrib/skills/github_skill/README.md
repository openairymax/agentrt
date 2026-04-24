# GitHub 技能模块

`agentos/openlab/contrib/skills/github_skill/`

## 概述

GitHub 技能模块为 AgentOS 提供全面的 GitHub 平台集成能力，支持版本仓库管理、Issue 跟踪、代码审查、CI/CD 操作等核心功能。该模块通过 GitHub REST API 和 GraphQL API 实现与 GitHub 生态的深度交互。

## 核心功能

- **仓库管理** - 创建、删除、Fork、Star 仓库，管理分支和标签
- **Issue 管理** - 创建、分配、标记、关闭 Issue，支持批量操作和模板化
- **Pull Request 管理** - 创建 PR、代码审查、合并策略控制、自动检测冲突
- **代码审查** - 提交评论、行级审查、状态检查、合规性验证
- **CI/CD 集成** - 触发 Actions 工作流、查看运行状态、获取构建日志
- **内容管理** - 文件创建与更新、提交管理、Release 发布、Wiki 操作
- **项目管理** - Milestone 管理、看板操作、项目卡片流转

## 快速开始

```python
from openlab.contrib.skills.github_skill import GitHubSkill

skill = GitHubSkill(token="your_github_token")

# 创建 Issue
issue = skill.create_issue(
    repo="owner/repo",
    title="Bug: 登录页面响应超时",
    body="详细描述...",
    labels=["bug", "priority-high"],
    assignees=["developer1"]
)

# 创建 Pull Request
pr = skill.create_pull_request(
    repo="owner/repo",
    title="修复登录超时问题",
    head="fix/login-timeout",
    base="main",
    body="修复描述..."
)

# 触发 Actions 工作流
workflow = skill.trigger_workflow(
    repo="owner/repo",
    workflow_id="ci.yml",
    ref="main"
)
```

## 配置说明

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `token` | GitHub 个人访问令牌 | 必填 |
| `base_url` | GitHub API 地址 | `https://api.github.com` |
| `timeout` | 请求超时时间(秒) | 30 |
| `retry_count` | 失败重试次数 | 3 |
| `graphql_enabled` | 启用 GraphQL 查询 | `true` |
| `webhook_secret` | Webhook 密钥(可选) | 空 |

## 操作接口

| 接口 | 说明 | 参数 |
|------|------|------|
| `create_issue` | 创建 Issue | repo, title, body, labels, assignees |
| `list_issues` | 查询 Issue 列表 | repo, state, labels, since |
| `create_pr` | 创建 Pull Request | repo, title, head, base, body |
| `review_pr` | 审查 Pull Request | repo, pr_number, action, body |
| `merge_pr` | 合并 Pull Request | repo, pr_number, merge_method |
| `trigger_workflow` | 触发 Actions | repo, workflow_id, ref, inputs |
| `list_workflow_runs` | 查看工作流运行 | repo, workflow_id, status |
| `create_release` | 创建 Release | repo, tag, name, body, prerelease |
| `get_contents` | 获取文件内容 | repo, path, ref |
| `search_code` | 搜索代码 | query, language, repo |

## 错误处理

```python
from openlab.contrib.skills.github_skill import GitHubSkill, GitHubError

skill = GitHubSkill(token="your_github_token")

try:
    repo = skill.get_repo("owner/repo")
except GitHubError as e:
    if e.status_code == 404:
        print("仓库不存在或无权访问")
    elif e.status_code == 403:
        print("API 速率限制已超出")
    elif e.status_code == 401:
        print("认证失败，请检查 Token 有效性")
    else:
        print(f"GitHub API 错误: {e.message}")
```

## 安全说明

- Token 建议使用环境变量 `GITHUB_TOKEN` 配置，避免硬编码
- 建议使用具有最小必要权限的 Fine-grained Token
- Webhook 请求建议验证签名确保来源可信
- API 请求遵循 GitHub 速率限制策略，自动处理 Retry-After 头

---

© 2026 SPHARX Ltd. All Rights Reserved.
