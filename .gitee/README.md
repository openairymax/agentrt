# AgentOS Gitee 配置

本文档包含 AgentOS 项目的 Gitee 平台相关配置文件。

## 目录结构

```
.gitee/
├── workflows/              # Gitee GoCI 工作流文件
├── pipelines/              # Gitee 流水线配置 (可选)
└── README.md               # 本文件
```

## 可用资源

| 资源 | 说明 |
|------|------|
| [workflows/](workflows/) | Gitee GoCI CI/CD 流水线配置 |

## Gitee 流水线优势

- **免费托管**: Gitee 提供免费 CI/CD 流水线
- **极速启动**: 配置简单，部署快速
- **私有仓库**: 支持私有仓库免费使用
- **国内访问**: 服务器位于国内，访问速度快

## 快速链接

- [Gitee CI/CD 使用指南](workflows/GITEE_CI_USAGE_GUIDE.md)
- [Gitee 环境配置说明](workflows/GITEE_CI_ENVIRONMENT.md)
- [Gitee 部署策略](workflows/GITEE_DEPLOYMENT_STRATEGY.md)
- [CI/CD 全面检查报告](../../.本地总结/CI_CD全面检查报告.md)
- [CI/CD 解决方案](../../.本地总结/CI_CD解决方案.md)
- [AgentOS README](../../README.md)

## Gitee 与 GitHub 语法对比

Gitee GoCI 使用与 GitHub Actions 相似的 YAML 语法，但关键字有所不同：

| 功能 | GitHub Actions | Gitee GoCI |
|------|---------------|------------|
| 触发条件 | `on:` | `triggers:` |
| Job 定义 | `jobs:` | `jobs:` |
| 步骤 | `steps:` | `script:` / `jobs:` |
| 运行器 | `runs-on:` | `runs-on:` |
| 动作 | `uses:` | `uses:` |
| 环境变量 | `env:` | `env:` |
| 工件 | `artifacts:` | `artifacts:` |
| 条件执行 | `if:` | `only:` / `except:` |

## 平台选择建议

| 场景 | 推荐平台 | 原因 |
|------|----------|------|
| 开源项目 | GitHub | 生态更完善 |
| 国内项目 | Gitee | 访问速度快 |
| 企业项目 | GitHub/Gitee 双平台 | 兼顾国内外访问 |

---

*© 2026 SPHARX Ltd. 保留所有权利*
