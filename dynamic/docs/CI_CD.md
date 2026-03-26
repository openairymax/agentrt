# AgentOS Dynamic Module CI/CD 配置文档

| 文档信息 | |
|---------|---------|
| **版本** | v1.0.0 |
| **编制日期** | 2026-03-24 |
| **适用模块** | AgentOS Dynamic Module |
| **文档状态** | 正式发布 |

---

## 一、CI/CD 架构概览

### 1.1 流水线架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    Dynamic Module CI/CD Pipeline                        │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌──────────┐    ┌──────────────────────────────────────────────────┐ │
│  │  代码提交 │───▶│              阶段0: 代码质量检查                   │ │
│  └──────────┘    │  • cppcheck 静态分析                              │ │
│                   │  • clang-format 格式检查                          │ │
│                   │  • clang-tidy 代码规范                            │ │
│                   └──────────────────────────────────────────────────┘ │
│                                      │                                   │
│                                      ▼                                   │
│                   ┌──────────────────────────────────────────────────┐ │
│                   │              阶段1: 跨平台构建                     │ │
│                   │  • Ubuntu (gcc/clang)                             │ │
│                   │  • macOS (clang)                                  │ │
│                   │  • Windows (gcc/MSVC)                             │ │
│                   └──────────────────────────────────────────────────┘ │
│                                      │                                   │
│                                      ▼                                   │
│                   ┌──────────────────────────────────────────────────┐ │
│                   │              阶段2: 安全扫描                       │ │
│                   │  • Trivy 漏洞扫描                                 │ │
│                   │  • CodeQL 代码分析                                │ │
│                   │  • TruffleHog 秘钥扫描                            │ │
│                   └──────────────────────────────────────────────────┘ │
│                                      │                                   │
│                                      ▼                                   │
│                   ┌──────────────────────────────────────────────────┐ │
│                   │              阶段3: 集成测试                       │ │
│                   │  • 单元测试                                       │ │
│                   │  • 集成测试                                       │ │
│                   │  • 覆盖率报告                                     │ │
│                   └──────────────────────────────────────────────────┘ │
│                                      │                                   │
│                                      ▼                                   │
│                   ┌──────────────────────────────────────────────────┐ │
│                   │              阶段4: 质量门禁                       │ │
│                   │  • 构建状态检查                                   │ │
│                   │  • 测试覆盖率阈值                                 │ │
│                   │  • 安全扫描结果                                   │ │
│                   └──────────────────────────────────────────────────┘ │
│                                      │                                   │
│                                      ▼                                   │
│                   ┌──────────────────────────────────────────────────┐ │
│                   │              阶段5: Docker 构建                   │ │
│                   │  • 多架构镜像构建                                 │ │
│                   │  • 镜像安全扫描                                   │ │
│                   │  • 推送到 GHCR                                    │ │
│                   └──────────────────────────────────────────────────┘ │
│                                      │                                   │
│                                      ▼                                   │
│                   ┌──────────────────────────────────────────────────┐ │
│                   │              阶段6: 发布                          │ │
│                   │  • GitHub Release                                │ │
│                   │  • 变更日志生成                                   │ │
│                   │  • 产物打包                                       │ │
│                   └──────────────────────────────────────────────────┘ │
│                                      │                                   │
│                                      ▼                                   │
│                   ┌──────────────────────────────────────────────────┐ │
│                   │              阶段7: 环境部署                       │ │
│                   │  • development → staging → production            │ │
│                   │  • 蓝绿部署                                       │ │
│                   │  • 自动回滚                                       │ │
│                   └──────────────────────────────────────────────────┘ │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 1.2 文件结构

```
dynamic/
├── .github/
│   └── workflows/
│       ├── ci.yml                    # 主CI工作流
│       └── release.yml               # 发布工作流
├── config/
│   ├── cppcheck.cfg                  # C代码静态分析配置
│   └── trivy.yaml                    # 安全扫描配置
├── deploy/
│   └── k8s/
│       ├── namespace.yaml            # Kubernetes命名空间
│       ├── deployment.yaml           # 部署配置
│       └── service.yaml              # 服务配置
├── docker/
│   ├── Dockerfile                    # 多阶段构建文件
│   ├── docker-compose.yml            # 开发环境编排
│   ├── docker-compose.dev.yml        # 开发环境覆盖
│   ├── docker-compose.prod.yml       # 生产环境覆盖
│   └── monitoring/
│       ├── prometheus.yml            # Prometheus配置
│       └── alerts.yml                # 告警规则
├── .clang-format                     # 代码格式配置
└── .clang-tidy                       # 代码规范配置
```

---

## 二、工作流触发条件

### 2.1 CI 工作流触发

| 触发条件 | 说明 |
|---------|------|
| `push` to `main`, `develop`, `release/**` | 代码推送到主分支 |
| `push` tags `dynamic-v*` | 版本标签推送 |
| `pull_request` to `main`, `develop` | PR 创建/更新 |
| `workflow_dispatch` | 手动触发 |

### 2.2 发布工作流触发

| 触发条件 | 说明 |
|---------|------|
| `push` tags `dynamic-v*.*.*` | 版本标签推送 |
| `workflow_dispatch` with version | 手动指定版本 |

---

## 三、环境管理

### 3.1 环境层次

| 环境 | 用途 | 触发条件 | 部署方式 |
|------|------|---------|----------|
| **development** | 本地开发 | 手动 | Docker Compose |
| **staging** | 功能验证 | PR合并/main推送 | Kubernetes |
| **preview** | PR预览 | PR创建 | Kubernetes |
| **production** | 生产运行 | 手动审批 | Kubernetes |

### 3.2 部署命令

```bash
# 开发环境
docker-compose -f docker/docker-compose.yml -f docker/docker-compose.dev.yml up -d

# 预生产环境
docker-compose -f docker/docker-compose.yml -f docker/docker-compose.prod.yml up -d

# Kubernetes部署
kubectl apply -f deploy/k8s/
```

---

## 四、监控告警

### 4.1 告警级别

| 级别 | 说明 | 通知方式 |
|------|------|---------|
| **critical** | 严重问题，需立即处理 | Slack + Email + SMS |
| **warning** | 警告，需关注 | Slack + Email |
| **info** | 信息，供参考 | Slack |

### 4.2 关键指标

| 指标 | 阈值 | 说明 |
|------|------|------|
| 实例可用性 | > 99.9% | 服务正常运行 |
| HTTP 延迟 P95 | < 1s | 请求响应时间 |
| 错误率 | < 5% | HTTP 错误比例 |
| 内存使用 | < 80% | 进程内存占用 |
| 会话数 | < 10000 | 活跃会话数量 |

---

## 五、版本发布

### 5.1 版本号规范

```
dynamic-v主版本.次版本.修订版本[-预发布版本]

示例:
- dynamic-v1.0.0        # 正式版
- dynamic-v1.1.0-beta.1 # Beta版
- dynamic-v1.2.0-rc.1   # 候选版
```

### 5.2 发布流程

1. **创建标签**: `git tag dynamic-v1.0.0`
2. **推送标签**: `git push origin dynamic-v1.0.0`
3. **自动触发**: CI/CD 自动构建和发布
4. **创建 Release**: 自动生成变更日志
5. **部署验证**: 自动运行冒烟测试

---

## 六、配置说明

### 6.1 必需的 GitHub Secrets

| Secret | 说明 |
|--------|------|
| `KUBE_CONFIG` | Kubernetes 配置文件 (base64) |
| `JWT_SECRET` | JWT 签名密钥 |
| `SLACK_WEBHOOK` | Slack 通知地址 |
| `GRAFANA_PASSWORD` | Grafana 管理员密码 |

### 6.2 环境变量

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `AGENTOS_LOG_LEVEL` | INFO | 日志级别 |
| `AGENTOS_HTTP_PORT` | 8080 | HTTP端口 |
| `AGENTOS_WS_PORT` | 8081 | WebSocket端口 |
| `AGENTOS_ENABLE_AUTH` | true | 启用认证 |
| `AGENTOS_REDIS_HOST` | redis | Redis主机 |

---

## 七、故障排查

### 7.1 常见问题

| 问题 | 可能原因 | 解决方案 |
|------|---------|---------|
| 构建失败 | 依赖缺失 | 检查 CMakeLists.txt |
| 测试失败 | 代码缺陷 | 查看测试日志 |
| 部署失败 | 配置错误 | 检查 K8s 配置 |
| 镜像拉取失败 | 权限问题 | 检查 GHCR 权限 |

### 7.2 回滚操作

```bash
# Kubernetes 回滚
kubectl rollout undo deployment/agentos-dynamic -n agentos-dynamic

# Docker Compose 回滚
docker-compose down
docker-compose up -d --force-recreate
```

---

*本文档由 AgentOS CTO (SOLO Coder) 基于《工程控制论》《论系统工程》思想设计*
*© 2026 SPHARX Ltd. 保留所有权利*
