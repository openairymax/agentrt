# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# Gitee CI/CD 环境配置说明
# Version: 1.0.0

# =============================================================================
# Gitee GoCI 环境配置指南
# =============================================================================

## 一、Gitee GoCI 与 GitHub Actions 语法对比

| 功能 | GitHub Actions | Gitee GoCI |
|------|---------------|------------|
| 工作流文件 | `.github/workflows/*.yml` | `.gitee/workflows/*.yml` |
| 触发条件 | `on: push, pull_request` | `triggers:` |
| Job 定义 | `jobs: {job-name}:` | `jobs:` |
| 运行器 | `runs-on: ubuntu-latest` | `runs-on:` |
| 容器 | `container: image` | `container:` |
| 步骤 | `steps:` | `script:` / `jobs:` |
| 工件 | `artifacts:` | `artifacts:` |
| 缓存 | `actions/cache` | `cache:` |
| 环境变量 | `env:` | `env:` |
| 条件执行 | `if:` | `only:` / `except:` |

## 二、Gitee 触发条件详解

### 2.1 支持的触发类型

```yaml
triggers:
  - push:                    # 代码推送
      branches: [master, develop, 'release/*']
      tags: ['v*']
      paths: ['**/*.c', '**/*.h']
  - pull_request:            # 合并请求
      branches: [master, develop]
      paths-ignore: ['**.md']
  - tag:                     # 标签推送
      pattern: 'v[0-9]+.[0-9]+.[0-9]+'
  - schedule:                # 定时任务
      cron: '0 2 * * *'      # 每天凌晨2点
  - api:                     # API触发
      events: ['deploy']
  - manual:                  # 手动触发
      name: 'Deploy to Prod'
```

### 2.2 Gitee 特有环境变量

| 变量 | 说明 |
|------|------|
| `$GITEE_USER` | 当前用户名 |
| `$GITEE_REPOSITORY` | 仓库名称 |
| `$GITEE_COMMIT_SHA` | 提交 SHA |
| `$GITEE_COMMIT_REF_NAME` | 分支/标签名 |
| `$GITEE_COMMIT_TAG` | 标签名 (仅 tag 触发) |
| `$GITEE_PULL_REQUEST_SOURCE_BRANCH` | PR 源分支 |
| `$GITEE_PULL_REQUEST_TARGET_BRANCH` | PR 目标分支 |
| `$GITEE_WORKFLOW` | 工作流名称 |
| `$GITEE_JOB_NAME` | Job 名称 |
| `$GITEE_JOB_STATUS` | Job 状态 |
| `$GITEE_PACKAGES_TOKEN` | 包仓库 Token |
| `$CI_COMMIT_SHA` | 提交 SHA |
| `$CI_COMMIT_REF_NAME` | 分支名 |
| `$CI_COMMIT_TAG` | 标签名 |

## 三、Gitee Runner 配置

### 3.1 使用 Gitee 官方 Runner

```yaml
runs-on:
  - gitee
  - ubuntu-22.04
  - 4c8g
```

### 3.2 使用自建 Runner

```yaml
runs-on:
  - self-hosted
  - linux        # 标签
  - label:linux-ubuntu
```

### 3.3 Runner 标签说明

| 标签 | 说明 |
|------|------|
| `gitee` | Gitee 官方托管 Runner |
| `self-hosted` | 自建 Runner |
| `linux` | Linux 系统 |
| `macos` | macOS 系统 |
| `windows` | Windows 系统 |
| `docker` | 支持 Docker |
| `large` | 大型 Runner (8c16g) |

## 四、Gitee 缓存配置

```yaml
cache:
  key: ${CI_COMMIT_REF_NAME}-${CI_COMMIT_SHA}
  paths:
    - .cache/
    - build/
    - node_modules/
  policy: pull-push    # pull-push | pull | push
```

## 五、Gitee 流水线变量

### 5.1 在 Gitee 平台配置

在仓库 `设置` → `流水线变量` 中配置:

| 变量名 | 说明 | 加密 |
|--------|------|------|
| `GITEE_USER` | Gitee 用户名 | 否 |
| `GITEE_PACKAGES_TOKEN` | 访问令牌 | 是 |
| `DINGTALK_TOKEN` | 钉钉机器人 Token | 是 |
| `WECOM_WEBHOOK` | 企业微信机器人 | 是 |
| `POSTGRES_PASSWORD` | 数据库密码 | 是 |
| `GRAFANA_PASSWORD` | Grafana 密码 | 是 |

### 5.2 生成访问令牌

1. 登录 Gitee: https://gitee.com
2. 进入 `设置` → `私人令牌`
3. 点击 `生成新令牌`
4. 选择以下权限:
   - `projects` (仓库)
   - `packages` (包仓库)
   - `pipeline` (流水线)
5. 复制令牌并配置为流水线变量

## 六、Gitee Docker Registry

### 6.1 Gitee 容器镜像服务

Gitee 提供免费的容器镜像托管服务:

```
# 镜像地址格式
gitee.cn/{owner}/{repo}:{tag}

# 示例
gitee.cn/spharx-agentos/agentos-atoms:latest
gitee.cn/spharx-agentos/agentos-gateway:v1.0.0
```

### 6.2 登录容器仓库

```bash
docker login gitee.cn -u $GITEE_USER -p $GITEE_PACKAGES_TOKEN
```

## 七、Gitee 通知配置

### 7.1 钉钉机器人配置

1. 在钉钉群中添加机器人
2. 选择「自定义」机器人
3. 复制 Webhook 地址
4. 配置为流水线变量 `DINGTALK_TOKEN`

### 7.2 企业微信机器人配置

类似钉钉，在企业微信群中添加机器人并获取 Webhook URL。

### 7.3 邮件通知

Gitee 支持邮件通知配置，可在流水线状态变更时发送邮件。

## 八、Gitee 流水线触发示例

### 8.1 推送触发

```yaml
triggers:
  - push:
      branches:
        - master      # master 分支推送
        - develop     # develop 分支推送
        - 'release/*' # release 分支推送
```

### 8.2 PR 触发

```yaml
triggers:
  - pull_request:
      branches:
        - master
        - develop
```

### 8.3 标签触发

```yaml
triggers:
  - tag:
      pattern: 'v[0-9]+.[0-9]+.[0-9]+'  # 匹配 v1.0.0, v2.1.3 等
```

### 8.4 定时触发

```yaml
triggers:
  - schedule:
      cron: '0 2 * * *'  # 每天凌晨2点
```

## 九、Gitee 流水线条件执行

### 9.1 only (仅在指定条件执行)

```yaml
only:
  - master           # 仅 master 分支
  - develop          # 仅 develop 分支
  - tag              # 仅标签推送
```

### 9.2 except (在指定条件不执行)

```yaml
except:
  - '*.md'           # 跳过 Markdown 文件变更
  - docs/            # 跳过 docs/ 目录
```

### 9.3 变量条件

```yaml
only:
  variables:
    - $CI_COMMIT_REF_NAME == "master"
    - $CI_COMMIT_AUTHOR == "admin"
```

## 十、完整示例

```yaml
# .gitee/workflows/example.yml

name: Example Pipeline

env:
  VERSION: 1.0.0

stages:
  - build
  - test
  - deploy

jobs:
  build:
    stage: build
    runs-on: ubuntu-22.04
    script: |
      echo "Building..."
      mkdir -p build
      tar -czvf agentos-$VERSION.tar.gz build/
    artifacts:
      - name: build-output
        paths:
          - build/
        expire_in: 7 days

  test:
    stage: test
    runs-on: ubuntu-22.04
    script: |
      echo "Testing..."
      pytest tests/ -v
    dependencies:
      - build

  deploy:
    stage: deploy
    runs-on: self-hosted
    only:
      - master
    script: |
      echo "Deploying..."
      docker-compose up -d
    when: manual  # 手动确认
```

---

*© 2026 SPHARX Ltd. 保留所有权利*
