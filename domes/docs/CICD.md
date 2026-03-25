# domes Module CI/CD Documentation

| 文档信息 | |
|---------|---------|
| **版本** | v1.0.0 |
| **编制日期** | 2026-03-24 |
| **适用模块** | domes 安全沙箱模块 |
| **文档状态** | 正式发布 |

---

## 一、CI/CD 流程概览

### 1.1 流水线架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    domes Module CI/CD Pipeline                          │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌──────────┐    ┌──────────────────────────────────────────────────┐ │
│  │  代码提交 │───▶│              Phase 0: Pre-flight                  │ │
│  └──────────┘    │  • 版本提取  • 部署条件判断  • 环境选择            │ │
│                   └──────────────────────────────────────────────────┘ │
│                                      │                                   │
│         ┌────────────────────────────┼────────────────────────────┐    │
│         ▼                            ▼                            ▼    │
│  ┌─────────────┐           ┌─────────────┐           ┌─────────────┐│
│  │Phase 1:     │           │Phase 2:     │           │Phase 3:     ││
│  │Code Quality │           │Security Scan│           │Build        ││
│  │• cppcheck   │           │• Trivy      │           │• Linux/macOS││
│  │• clang-format│          │• Semgrep    │           │• Windows    ││
│  │• clang-tidy │           │• TruffleHog │           │• 多编译器   ││
│  └─────────────┘           └─────────────┘           └─────────────┘│
│         │                            │                            │    │
│         └────────────────────────────┼────────────────────────────┘    │
│                                      ▼                                   │
│                   ┌────────────────────────────────────────┐           │
│                   │           Phase 4: Coverage             │           │
│                   │  • lcov 覆盖率  • 阈值检查 (80%)        │           │
│                   └────────────────────────────────────────┘           │
│                                      │                                   │
│         ┌────────────────────────────┼────────────────────────────┐    │
│         ▼                            ▼                            ▼    │
│  ┌─────────────┐           ┌─────────────┐           ┌─────────────┐│
│  │Phase 5:     │           │Phase 6:     │           │Phase 7:     ││
│  │Fuzzing      │           │Benchmark    │           │Quality Gates││
│  │• ASan/UBSan │           │• 性能基准   │           │• 门禁检查   ││
│  │• 短时模糊   │           │• 回归检测   │           │• 结果汇总   ││
│  └─────────────┘           └─────────────┘           └─────────────┘│
│                                      │                                   │
│                                      ▼                                   │
│                   ┌────────────────────────────────────────┐           │
│                   │           Phase 8: Package              │           │
│                   │  • 产物打包  • GitHub Release           │           │
│                   └────────────────────────────────────────┘           │
│                                      │                                   │
│                                      ▼                                   │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │                    Phase 9-10: Deploy & Verify                    │  │
│  │  dev ──▶ staging ──▶ production (with health checks)             │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                                      │                                   │
│                                      ▼                                   │
│                   ┌────────────────────────────────────────┐           │
│                   │           Phase 11: Cleanup            │           │
│                   │  • 清理旧产物  • 资源回收               │           │
│                   └────────────────────────────────────────┘           │
└─────────────────────────────────────────────────────────────────────────┘
```

### 1.2 触发条件

| 触发类型 | 条件 | 执行阶段 |
|---------|------|---------|
| **Push to main** | `main` 分支代码变更 | 全流程 + staging 部署 |
| **Push to develop** | `develop` 分支代码变更 | 全流程 + development 部署 |
| **Push to release** | `release/*` 分支代码变更 | 全流程 |
| **Tag** | `domes-v*` 标签 | 全流程 + production 部署 |
| **Pull Request** | PR 到 main/develop | 全流程（无部署） |
| **Manual** | workflow_dispatch | 可选环境部署 |

---

## 二、工作流文件说明

### 2.1 主要工作流

| 文件 | 用途 | 触发条件 |
|------|------|---------|
| `domes-ci.yml` | 主 CI/CD 流水线 | Push/PR/Tag/Manual |
| `domes-notifications.yml` | 通知工作流 | workflow_run 触发 |
| `domes-rollback.yml` | 回滚工作流 | Manual |

### 2.2 配置文件

| 文件 | 用途 |
|------|------|
| `domes/VERSION` | 版本号文件 |
| `domes/config/deployment.yaml` | 多环境部署配置 |
| `domes/config/alerts.yml` | Prometheus 告警规则 |
| `domes/config/grafana-dashboard.json` | Grafana 仪表板配置 |

### 2.3 脚本文件

| 文件 | 用途 |
|------|------|
| `domes/scripts/version.sh` | 版本管理脚本 |
| `domes/scripts/collect_logs.sh` | 日志收集脚本 |

---

## 三、环境管理

### 3.1 环境层次

| 环境 | 触发条件 | 部署方式 | 审批 |
|------|---------|---------|------|
| **development** | develop 分支推送 | 自动 | 无 |
| **staging** | main 分支推送 | 自动 | 无 |
| **production** | domes-v* 标签 | 自动 | 可选 |

### 3.2 环境配置

```yaml
# development 环境
debug: true
log_level: DEBUG
resources:
  cpu_limit: "500m"
  memory_limit: "512Mi"

# staging 环境
debug: false
log_level: INFO
resources:
  cpu_limit: "1000m"
  memory_limit: "1Gi"

# production 环境
debug: false
log_level: WARNING
resources:
  cpu_limit: "2000m"
  memory_limit: "2Gi"
  replicas: 3
```

---

## 四、版本管理

### 4.1 语义化版本

```
主版本.次版本.修订版本[-预发布版本]
   │        │        │         │
   │        │        │         └── rc.1, beta.1, alpha.1
   │        │        └── 修复版本 (1.0.1)
   │        └── 功能版本 (1.1.0)
   └── 重大版本 (2.0.0)
```

### 4.2 版本管理命令

```bash
# 获取当前版本
./scripts/version.sh get

# 从 git 标签获取版本
./scripts/version.sh get-git

# 版本升级
./scripts/version.sh bump major    # 1.0.0 → 2.0.0
./scripts/version.sh bump minor    # 1.0.0 → 1.1.0
./scripts/version.sh bump patch    # 1.0.0 → 1.0.1
./scripts/version.sh bump prerelease  # 1.0.0 → 1.0.1-rc.1

# 设置版本
./scripts/version.sh set 1.2.0

# 检查版本一致性
./scripts/version.sh check

# 生成产物名称
./scripts/version.sh artifact 1.0.0 linux x86_64
# 输出: agentos-domes-1.0.0-linux-x86_64
```

### 4.3 产物命名规范

```
agentos-domes-{版本}-{操作系统}-{架构}.{格式}

示例:
- agentos-domes-1.0.0-linux-x86_64.tar.gz
- agentos-domes-1.0.0-windows-x64.zip
- agentos-domes-1.0.0-macos-arm64.tar.gz
```

---

## 五、质量门禁

### 5.1 门禁阈值

| 指标 | 阈值 | 失败行为 |
|------|------|---------|
| 代码覆盖率 | ≥ 80% | 阻断部署 |
| cppcheck 错误 | = 0 | 阻断构建 |
| 高危漏洞 | = 0 | 阻断部署 |
| 单元测试 | 100% 通过 | 阻断构建 |

### 5.2 质量报告

每次构建会生成以下报告：
- `code-quality-reports/` - 代码质量报告
- `security-reports/` - 安全扫描报告
- `coverage-report/` - 覆盖率报告
- `quality-report/quality_report.json` - 综合质量报告

---

## 六、监控告警

### 6.1 告警规则

| 告警名称 | 条件 | 严重级别 |
|---------|------|---------|
| DomesBuildFailure | 构建失败 | Critical |
| DomesTestCoverageDrop | 覆盖率 < 80% | Warning |
| DomesCriticalVulnerability | 发现高危漏洞 | Critical |
| DomesDeploymentFailure | 部署失败 | Critical |
| DomesHealthCheckFailure | 健康检查失败 | Critical |

### 6.2 通知渠道

- **Slack**: `#agentos-staging`, `#agentos-production`
- **Email**: `team@spharx.cn`, `oncall@spharx.cn`
- **GitHub Issues**: 自动创建失败问题

---

## 七、回滚流程

### 7.1 触发回滚

1. 进入 Actions → domes Rollback
2. 选择目标环境
3. 指定目标版本（留空则回滚到上一版本）
4. 填写回滚原因
5. 执行回滚

### 7.2 回滚检查清单

- [ ] 确认回滚原因已记录
- [ ] 验证目标版本可用
- [ ] 通知相关团队
- [ ] 执行回滚
- [ ] 验证服务健康
- [ ] 创建调查 Issue

---

## 八、故障排查

### 8.1 常见问题

| 问题 | 可能原因 | 解决方案 |
|------|---------|---------|
| 构建失败 | 编译错误 | 检查 cppcheck 报告 |
| 测试失败 | 测试用例失败 | 查看 CTest 日志 |
| 覆盖率不足 | 测试覆盖不够 | 添加测试用例 |
| 安全扫描失败 | 发现漏洞 | 修复漏洞或申请豁免 |
| 部署失败 | 环境问题 | 检查部署配置 |

### 8.2 日志收集

```bash
# 收集所有日志
./scripts/collect_logs.sh collect

# 收集特定类型日志
./scripts/collect_logs.sh test
./scripts/collect_logs.sh coverage
./scripts/collect_logs.sh security

# 生成构建摘要
./scripts/collect_logs.sh summary

# 清理旧日志
./scripts/collect_logs.sh cleanup 30
```

---

## 九、最佳实践

### 9.1 提交前检查

1. 运行本地测试
2. 检查代码格式
3. 更新版本号（如需要）
4. 更新 CHANGELOG

### 9.2 PR 规范

1. 标题清晰描述变更
2. 关联相关 Issue
3. 添加必要的测试
4. 等待 CI 通过后再合并

### 9.3 发布流程

1. 创建 release 分支
2. 更新版本号和 CHANGELOG
3. 合并到 main
4. 创建 tag `domes-v*`
5. 等待自动部署

---

*本文档由 AgentOS CTO (SOLO Coder) 编制*
*© 2026 SPHARX Ltd. 保留所有权利*
