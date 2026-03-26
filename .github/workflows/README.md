# GitHub Actions 工作流目录

本目录包含 AgentOS 项目 CI/CD 流水线的 GitHub Actions 工作流配置文件。

## 目录说明

GitHub Actions 会自动发现并执行本目录中的工作流文件。

## 工作流文件清单

| 文件 | 说明 | 覆盖模块 |
|------|------|----------|
| `atoms-ci.yml` | atoms/ 微内核持续集成 | 微内核 (corekern, coreloopthree, memoryrovol, syscall, utils) |
| `backs-ci.yml` | backs/ 服务层持续集成 | 服务层 (llm_d, tool_d, monit_d, sched_d, market_d) |
| `dynamic-ci.yml` | dynamic/ 网关层持续集成 | HTTP/WebSocket 网关 |
| `domes-ci.yml` | domes/ 安全沙箱持续集成 | 安全审计、沙箱隔离 |
| `domes-rollback.yml` | domes/ 回滚工作流 | 生产环境回滚 |
| `scripts-ci.yml` | scripts/ 脚本层持续集成 | 运维脚本质量检查 |
| `scripts-cicd.yml` | scripts/ 完整 CI/CD | 脚本层完整流水线 |
| `test.yml` | tests/ 测试模块持续集成 | 单元/集成/安全测试 |
| `config-ci.yml` | config/ 配置模块持续集成 | 配置管理 |
| `partdata-ci.yml` | partdata/ 数据模块持续集成 | 数据处理模块 |

## 工作流状态指示

| 指示 | 说明 |
|------|------|
| ✅ 已验证 | 工作流经过测试，可正常使用 |
| 🆕 新增 | 最近添加的工作流 |
| ⚠️ 需配置 | 需要额外配置 Secrets 或变量 |

## 相关文档

- [CI/CD 全面检查报告](../../.本地总结/CI_CD全面检查报告.md)
- [CI/CD 解决方案](../../.本地总结/CI_CD解决方案.md)
- [GitHub Actions 官方文档](https://docs.github.com/en/actions/learn-github-actions)

## 流水线运行要求

### 必需的 GitHub Secrets

| Secret | 说明 |
|--------|------|
| `SLACK_WEBHOOK` | Slack 通知 Webhook |
| `CODECOV_TOKEN` | Codecov 覆盖率上传令牌 |
| `GH_TOKEN` | GitHub 访问令牌 |

### 必需的流水线变量

| 变量 | 说明 |
|------|------|
| `BUILD_TYPE` | 构建类型 (Release/Debug) |
| `VERSION` | 版本号 |

---

*© 2026 SPHARX Ltd. 保留所有权利*
