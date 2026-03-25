# AgentOS Scripts 模块 CI/CD 文档

| 文档信息 | |
|---------|---------|
| **版本** | v1.0.0 |
| **编制日期** | 2026-03-24 |
| **适用项目** | AgentOS Scripts 模块 |
| **文档状态** | 正式发布 |

---

## 一、CI/CD 流程总览

```
代码提交 → Pre-commit → CI Pipeline → Quality Gates → Docker Build → Release → Deploy → Monitor
              ↓            ↓              ↓              ↓           ↓        ↓        ↓
           本地检查      并行执行        必须通过        制品构建     版本管理   环境部署   监控告警
```

### 流程阶段

| 阶段 | 名称 | 说明 | 失败处理 |
|------|------|------|----------|
| 0 | 代码质量 | Ruff/Mypy 检查 | 阻断 |
| 1 | Shell 检查 | ShellCheck 验证 | 阻断 |
| 2 | 单元测试 | Pytest 多版本测试 | 阻断 |
| 3 | 集成测试 | Shell 脚本集成 | 警告 |
| 4 | 安全扫描 | Bandit/Safety/TruffleHog | 警告 |
| 5 | 构建验证 | 语法/结构验证 | 阻断 |
| 6 | 跨平台测试 | Ubuntu/macOS/Windows | 警告 |
| 7 | 性能基准 | Benchmark 测试 | 警告 |
| 8 | Docker 构建 | 镜像构建推送 | 阻断 |
| 9 | 质量门禁 | 汇总检查结果 | 阻断 |
| 10 | 版本发布 | GitHub Release | 手动 |
| 11 | 部署 | 多环境部署 | 需审批 |
| 12 | 回滚 | 版本回滚 | 需审批 |

---

## 二、工作流配置

### 2.1 GitHub Actions 工作流

主工作流文件: `.github/workflows/scripts-cicd.yml`

**触发条件:**

```yaml
on:
  push:
    branches: [main, develop]
    paths:
      - 'scripts/**'
      - '.github/workflows/scripts-cicd.yml'
  pull_request:
    branches: [main]
  workflow_dispatch:
```

### 2.2 环境配置

| 环境 | 触发方式 | 审批 | 用途 |
|------|----------|------|------|
| **dev** | 本地 | 无 | 开发调试 |
| **staging** | workflow_dispatch | 无 | 预发布验证 |
| **preview** | PR 创建 | 无 | PR 预览 |
| **production** | workflow_dispatch | 需审批 | 生产部署 |

---

## 三、部署配置

### 3.1 Docker Compose 配置

| 文件 | 环境 | 说明 |
|------|------|------|
| `docker-compose.yml` | production | 生产环境完整配置 |
| `docker-compose.staging.yml` | staging | 预发布环境配置 |
| `docker-compose.preview.yml` | preview | PR 预览配置 |

### 3.2 蓝绿部署策略

```
┌─────────────────────────────────────────────────────┐
│                   Blue-Green 部署                      │
├─────────────────────────────────────────────────────┤
│                                                      │
│   [Blue] ──→ [Green] ──→ [Switch] ──→ [Cleanup]    │
│     ↓          ↓           ↓            ↓           │
│   当前版本    新版本      流量切换     旧版本清理     │
│                                                      │
└─────────────────────────────────────────────────────┘
```

### 3.3 回滚策略

```bash
# 回滚到指定版本
./rollback.sh rollback --version v1.0.0 --environment production

# 查看部署历史
./rollback.sh history --environment production

# 查看可用版本
./rollback.sh versions

# 清理旧版本
./rollback.sh cleanup --keep 5
```

---

## 四、安全扫描

### 4.1 扫描工具

| 工具 | 类型 | 检查内容 |
|------|------|----------|
| **Bandit** | SAST | Python 安全问题 |
| **Safety** | SCA | Python 依赖漏洞 |
| **TruffleHog** | Secrets | 秘钥/凭证泄露 |
| **ShellCheck** | SAST | Shell 脚本问题 |

### 4.2 安全报告

扫描结果保存在:
```
build/security-reports/
├── bandit-report.json
├── safety-report.json
└── trufflehog-report.json
```

---

## 五、日志管理

### 5.1 日志收集

```bash
# 收集构建日志
./buildlog.sh collect --build-id 12345

# 实时监控
./buildlog.sh tail --file /path/to/log

# 搜索日志
./buildlog.sh search --pattern "ERROR.*timeout"
```

### 5.2 日志归档

```bash
# 归档日志
./buildlog.sh archive --build-id all

# 清理过期日志
./buildlog.sh cleanup --days 30
```

### 5.3 报告生成

```bash
# 生成 HTML 报告
./buildlog.sh report --output build-report.html
```

---

## 六、CI/CD 入口脚本

### 6.1 cicd.sh

统一的 CI/CD 命令行入口:

```bash
# 运行 CI 流程
./cicd.sh ci

# 构建
./cicd.sh build [版本] [类型]

# 安全扫描
./cicd.sh security

# 部署
./cicd.sh deploy <环境> [版本]

# 回滚
./cicd.sh rollback <环境> [版本]

# 发布
./cicd.sh release <标签>
```

### 6.2 构建清单

构建完成后生成清单:

```json
{
    "version": "1.0.0",
    "build_type": "release",
    "build_time": "2026-03-24T10:00:00+08:00",
    "artifacts": [
        {"type": "shell", "path": "build/scripts/1.0.0/agentos-scripts-1.0.0-shell.tar.gz"},
        {"type": "docker", "tag": "spharx/agentos-scripts:1.0.0"}
    ]
}
```

---

## 七、制品管理

### 7.1 版本控制

遵循语义化版本 (SemVer):
```
主版本.次版本.修订版本[-预发布版本]
   │        │        │         │
   │        │        │         └── 预发布 (rc, beta)
   │        │        └── 日常修复
   │        └── 功能更新
   └── 重大变更
```

### 7.2 Docker 镜像标签

| 标签 | 说明 | 生命周期 |
|------|------|----------|
| `latest` | 最新正式版 | 永久 |
| `v1.0.0` | 语义化版本 | 永久 |
| `sha-abc1234` | Git SHA | 30天 |
| `develop` | 开发分支 | 临时 |
| `pr-123` | PR 预览 | PR 关闭后删除 |

### 7.3 制品存储

| 层级 | 存储 | 保留策略 |
|------|------|----------|
| CI 临时 | GitHub Artifacts | 7天 |
| 预发布 | GitHub Releases (draft) | 30天 |
| 正式发布 | GitHub Releases | 永久 |

---

## 八、质量门禁

### 8.1 质量门禁检查点

```yaml
quality-gate:
  requires:
    - code-quality      # 必须通过
    - shell-check       # 必须通过
    - unit-tests        # 必须通过
    - shell-integration  # 必须通过
    - security-scan     # 警告
    - build-verification # 必须通过
```

### 8.2 覆盖率要求

| 模块 | 最低覆盖率 |
|------|-----------|
| core/plugin.py | 80% |
| core/events.py | 75% |
| core/security.py | 70% |
| core/telemetry.py | 70% |

---

## 九、监控告警

### 9.1 告警规则

| 告警 | 条件 | 级别 |
|------|------|------|
| 构建失败 | CI 失败 | Critical |
| 构建超时 | > 30 分钟 | Warning |
| 覆盖率下降 | < 80% | Warning |
| 安全漏洞 | 高危漏洞 | Critical |

### 9.2 通知渠道

- GitHub Actions 状态
- Slack (可选)
- Email (可选)

---

## 十、最佳实践

### 10.1 提交流程

1. 本地运行 `pre-commit run --all-files`
2. 确保所有测试通过
3. 提交 PR
4. CI 通过后请求 review
5. Merge 到 main/develop

### 10.2 部署流程

1. 在 staging 环境验证
2. 创建 Release Draft
3. 手动触发 production 部署
4. 审批后执行
5. 验证部署结果

### 10.3 回滚流程

1. 确认问题
2. 选择目标版本
3. 执行回滚
4. 验证功能
5. 更新 Release Note

---

## 十一、相关文件

| 文件 | 说明 |
|------|------|
| `.github/workflows/scripts-cicd.yml` | CI/CD 主工作流 |
| `.github/workflows/scripts-ci.yml` | 原有 CI 工作流 |
| `scripts/dev/cicd.sh` | CI/CD 入口脚本 |
| `scripts/dev/rollback.sh` | 回滚管理脚本 |
| `scripts/dev/buildlog.sh` | 日志管理脚本 |
| `scripts/deploy/docker/docker-compose*.yml` | Docker 部署配置 |

---

## 十二、版本历史

| 版本 | 日期 | 修改人 | 变更说明 |
|------|------|--------|----------|
| v1.0.0 | 2026-03-24 | CTO | 初始版本 |

---

*本文档由 AgentOS CTO (SOLO Coder) 设计*
*© 2026 SPHARX Ltd. 保留所有权利*