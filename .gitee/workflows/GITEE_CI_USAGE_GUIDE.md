# AgentOS Gitee CI/CD 使用指南

| 文档信息 | |
|---------|---------|
| **版本** | v1.0.0 |
| **编制日期** | 2026-03-24 |
| **适用平台** | Gitee GoCI |
| **文档状态** | 正式发布 |

---

## 一、快速开始

### 1.1 前提条件

1. **Gitee 账号**: 拥有 Gitee 仓库访问权限
2. **仓库访问**: 已将 Gitee 仓库克隆到本地
3. **流水线变量**: 在 Gitee 平台配置必要的环境变量

### 1.2 文件结构

AgentOS 项目的 Gitee CI/CD 配置文件位于:

```
AgentOS/
├── .gitee/
│   └── workflows/
│       ├── agentos-ci.yml           # 主 CI/CD 流水线配置
│       ├── GITEE_CI_ENVIRONMENT.md  # 环境配置说明
│       └── GITEE_DEPLOYMENT_STRATEGY.md  # 部署策略说明
├── scripts/
│   └── deploy/
│       ├── docker/
│       │   ├── docker-compose.yml        # 开发/测试环境
│       │   ├── docker-compose.staging.yml # 预发布环境
│       │   ├── docker-compose.prod.yml    # 生产环境
│       │   ├── Dockerfile.kernel         # 内核镜像
│       │   ├── Dockerfile.dynamic         # 网关镜像
│       │   └── Dockerfile.service         # 服务镜像
│       └── deploy/
│           ├── blue-green.sh         # 蓝绿部署脚本
│           ├── rollback.sh           # 回滚脚本
│           └── health-check.sh       # 健康检查脚本
```

---

## 二、流水线变量配置

### 2.1 必需变量

在 Gitee 仓库 `设置` → `流水线变量` 中配置以下变量:

| 变量名 | 说明 | 加密 | 示例 |
|--------|------|------|------|
| `GITEE_USER` | Gitee 用户名 | 否 | `spharx` |
| `GITEE_PACKAGES_TOKEN` | 容器镜像仓库访问令牌 | **是** | `gph_xxxx` |
| `DINGTALK_TOKEN` | 钉钉机器人 Webhook Token | **是** | `xxxxxx` |
| `WECOM_WEBHOOK` | 企业微信机器人 Webhook | **是** | `https://xxx` |
| `POSTGRES_PASSWORD` | PostgreSQL 数据库密码 | **是** | `xxxxxx` |
| `GRAFANA_PASSWORD` | Grafana 管理员密码 | **是** | `xxxxxx` |

### 2.2 生成访问令牌

1. 登录 Gitee: https://gitee.com
2. 进入 `设置` → `私人令牌`
3. 点击 `生成新令牌`
4. 选择权限:
   - ✅ `projects` - 仓库访问
   - ✅ `packages` - 包仓库
   - ✅ `pipeline` - 流水线
5. 复制生成的令牌并配置为 `GITEE_PACKAGES_TOKEN`

---

## 三、流水线阶段说明

### 3.1 流水线架构

```
┌─────────────────────────────────────────────────────────────┐
│                    AgentOS Gitee 流水线                      │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Stage 1: Code Quality (代码质量)                           │
│  ├── C 静态分析 (cppcheck, clang-tidy)                      │
│  ├── Python Lint (ruff, mypy, bandit)                      │
│  └── Shell 检查 (shellcheck)                                │
│                                                             │
│  Stage 2: Build (构建)                                     │
│  ├── atoms (Linux/macOS/Windows)                           │
│  ├── backs (Linux)                                         │
│  ├── dynamic (Linux)                                        │
│  └── domes (Linux)                                         │
│                                                             │
│  Stage 3: Test (测试)                                       │
│  ├── 单元测试                                               │
│  ├── 集成测试                                               │
│  ├── 契约测试                                               │
│  └── 性能测试                                               │
│                                                             │
│  Stage 4: Security (安全)                                   │
│  ├── 漏洞扫描 (Trivy)                                       │
│  ├── 秘钥扫描 (TruffleHog)                                  │
│  └── 许可证扫描 (Scancode)                                  │
│                                                             │
│  Stage 5: Deploy Staging (预发布部署)                       │
│  └── 自动部署到 staging 环境                                 │
│                                                             │
│  Stage 6: Deploy Production (生产部署)                      │
│  └── 手动审批后部署到生产环境                                │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 各阶段触发条件

| 阶段 | 触发条件 | 自动/手动 | 说明 |
|------|----------|-----------|------|
| Code Quality | push/PR | 自动 | 每次代码推送 |
| Build | push/PR | 自动 | 每次代码推送 |
| Test | push/PR | 自动 | 每次代码推送 |
| Security | push/schedule | 自动/定时 | 每天凌晨2点 |
| Deploy Staging | develop 分支 | 自动 | develop 推送 |
| Deploy Production | master/tag | 手动 | 需要审批 |

---

## 四、常见操作

### 4.1 查看流水线状态

1. 进入 Gitee 仓库页面
2. 点击左侧 `流水线`
3. 查看所有流水线运行记录

```bash
# 或通过 Gitee CLI
gitee pipeline list
```

### 4.2 手动触发流水线

```bash
# 通过 Gitee CLI 触发
gitee pipeline run --branch develop

# 或在 Gitee 平台手动触发
# 流水线页面 → 点击「运行流水线」→ 选择分支
```

### 4.3 重新运行失败的 Job

```bash
# 在 Gitee 平台
# 流水线详情 → 失败的 Job → 点击「重新运行」
```

### 4.4 取消正在运行的流水线

```bash
# 在 Gitee 平台
# 流水线详情 → 点击「取消」
```

---

## 五、部署操作

### 5.1 部署到预发布环境 (Staging)

**自动触发**: 当代码推送到 `develop` 分支时自动部署

```bash
# 手动触发
git checkout develop
git push origin develop
```

**验证部署**:
- API: https://staging.agentos.spharx.cn/api/v1/status
- Gateway: https://staging.agentos.spharx.cn/health

### 5.2 部署到生产环境 (Production)

**步骤**:

1. **创建版本标签**
   ```bash
   # 切换到 master 分支
   git checkout master
   git pull origin master

   # 创建版本标签
   git tag v1.0.0
   git push origin v1.0.0
   ```

2. **等待 CI/CD 完成**
   - Code Quality → Build → Test → Security → Deploy Staging

3. **手动触发生产部署**
   ```bash
   # 在 Gitee 平台:
   # 流水线 → 选择 tag v1.0.0 的运行 → 点击「运行」Deploy Production 阶段
   ```

4. **审批确认**
   - 需要仓库管理员或指定人员审批

### 5.3 回滚操作

**手动回滚**:

1. 进入 Gitee 仓库 `流水线` 页面
2. 点击左侧 `流水线` → `回滚`
3. 选择要回滚到的版本
4. 确认回滚

**命令行回滚**:

```bash
# 使用回滚脚本
./scripts/deploy/rollback.sh v1.0.0
```

---

## 六、通知配置

### 6.1 钉钉机器人配置

1. 在钉钉群中添加「自定义机器人」
2. 选择安全设置 (建议使用「加签」)
3. 复制 Webhook 地址
4. 在 Gitee `流水线变量` 中配置 `DINGTALK_TOKEN`

**通知格式示例**:
```
✅ AgentOS CI/CD Pipeline Success
> 分支: develop
> 提交: abc1234
> 状态: Build Passed, Test Passed
```

### 6.2 企业微信机器人配置

类似钉钉，在企业微信群中添加机器人并获取 Webhook URL。

---

## 七、故障排查

### 7.1 流水线失败常见原因

| 问题 | 可能原因 | 解决方案 |
|------|----------|----------|
| Build 失败 | CMake 依赖缺失 | 检查 `.gitee/workflows/agentos-ci.yml` 中的依赖安装 |
| Test 失败 | 测试环境问题 | 检查 Redis/PostgreSQL 服务是否就绪 |
| Security 扫描失败 | 发现敏感信息 | 检查代码中是否有硬编码的密钥 |
| Deploy 失败 | 镜像拉取失败 | 检查 `GITEE_PACKAGES_TOKEN` 是否有效 |

### 7.2 查看日志

```bash
# 在 Gitee 平台
# 流水线 → 选择运行 → 选择 Job → 查看日志

# 或下载日志
gitee pipeline artifact download --job <job-id> --name logs
```

### 7.3 常见错误处理

**错误: `docker login failed`**
```bash
# 检查令牌是否正确
echo $GITEE_PACKAGES_TOKEN | docker login gitee.cn -u $GITEE_USER --password-stdin

# 重新生成令牌
# Gitee → 设置 → 私人令牌 → 生成新令牌
```

**错误: `permission denied`**
```bash
# 检查用户权限
# Gitee → 仓库 → 设置 → 仓库成员
# 确保有 Developer 或 Owner 权限
```

---

## 八、最佳实践

### 8.1 提交流程

```bash
# 1. 创建功能分支
git checkout -b feature/your-feature

# 2. 编写代码并提交
git add .
git commit -m "feat: add new feature"

# 3. 推送分支触发 CI
git push origin feature/your-feature

# 4. 创建 Pull Request
# 在 Gitee 平台创建 PR, 等待 Code Review

# 5. 合并到 develop
# PR 审核通过后, 合并到 develop 自动部署到 Staging
```

### 8.2 Commit Message 规范

```
<type>(<scope>): <subject>

类型:
- feat: 新功能
- fix: 修复bug
- docs: 文档更新
- style: 代码格式
- refactor: 重构
- test: 测试相关
- chore: 构建/工具

示例:
feat(atoms): add new IPC mechanism
fix(dynamic): resolve memory leak issue
docs(openhub): update API documentation
```

### 8.3 分支管理

```
master      ──────────────────────────────────► 生产环境
                ▲                                 ▲
                │                                 │
develop      ──┴─────────────────────────────────► Staging 环境
                ▲
                │
feature/*   ───┴──► PR → Code Review → 合并
bugfix/*    ───┴──► PR → Code Review → 合并
release/*   ───┴──► 预发布版本
hotfix/*    ───┴──► 紧急修复 → 直接合并到 master
```

---

## 九、相关文档

| 文档 | 路径 |
|------|------|
| Gitee CI/CD 完整配置 | [.gitee/workflows/agentos-ci.yml](file:///d:/Spharx/SpharxWorks/AgentOS/.gitee/workflows/agentos-ci.yml) |
| Gitee 环境配置说明 | [.gitee/workflows/GITEE_CI_ENVIRONMENT.md](file:///d:/Spharx/SpharxWorks/AgentOS/.gitee/workflows/GITEE_CI_ENVIRONMENT.md) |
| Gitee 部署策略 | [.gitee/workflows/GITEE_DEPLOYMENT_STRATEGY.md](file:///d:/Spharx/SpharxWorks/AgentOS/.gitee/workflows/GITEE_DEPLOYMENT_STRATEGY.md) |
| GitHub CI/CD 解决方案 | [.本地总结/CI_CD解决方案.md](file:///d:/Spharx/SpharxWorks/AgentOS/.本地总结/CI_CD解决方案.md) |
| CI/CD 全面检查报告 | [.本地总结/CI_CD全面检查报告.md](file:///d:/Spharx/SpharxWorks/AgentOS/.本地总结/CI_CD全面检查报告.md) |
| Docker 生产配置 | [scripts/deploy/docker/docker-compose.prod.yml](file:///d:/Spharx/SpharxWorks/AgentOS/scripts/deploy/docker/docker-compose.prod.yml) |

---

## 十、联系方式

如有 CI/CD 相关问题，请联系:

- **CTO (SOLO Coder)**: 技术总负责人
- **DevOps Team**: CI/CD 运维支持

---

*© 2026 SPHARX Ltd. 保留所有权利*
