# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# Gitee CI/CD 部署策略配置
# Version: 1.0.0

# =============================================================================
# AgentOS Gitee 平台部署策略
# =============================================================================

## 一、部署架构概览

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      AgentOS Gitee 部署架构                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Code Push ──▶ Gitee CI ──▶ Build ──▶ Test ──▶ Security Scan        │
│       │                                                         │       │
│       │                    ┌────────────────────────────────────┘       │
│       │                    ▼                                            │
│       │            ┌───────────────┐                                     │
│       │            │   Staging    │                                     │
│       │            │  Environment  │                                     │
│       │            └───────┬───────┘                                     │
│       │                    │                                            │
│       │                    ▼                                            │
│       │    ┌──────────────────────────────┐                            │
│       │    │     Approval Gate            │                            │
│       │    │  (Manual/自动审批)          │                            │
│       │    └──────────────────────────────┘                            │
│       │                    │                                            │
│       │                    ▼                                            │
│       │            ┌───────────────┐                                     │
│       │            │  Production   │                                     │
│       │            │  Environment  │                                     │
│       │            └───────────────┘                                     │
│       │                    │                                            │
│       │                    ▼                                            │
│       │            ┌───────────────┐                                     │
│       │            │   Monitoring  │                                     │
│       │            │   & Alerting  │                                     │
│       │            └───────────────┘                                     │
│       │                                                             │
│       ▼                                                             │
│   ┌─────────────────────────────────────┐                             │
│   │         Rollback Mechanism           │                             │
│   │   (Blue-Green / Canary / Manual)    │                             │
│   └─────────────────────────────────────┘                             │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

## 二、部署环境配置

### 2.1 环境矩阵

| 环境 | 用途 | 触发条件 | 审批 | 自动部署 |
|------|------|----------|------|----------|
| **dev** | 开发调试 | PR 创建/更新 | 否 | 是 |
| **test** | 集成测试 | 代码推送 | 否 | 是 |
| **staging** | 预发布 | develop 分支推送 | 是 | 是 |
| **preview** | PR 预览 | PR 创建 | 否 | 是 |
| **production** | 生产 | master/tag | 是 | 手动 |

### 2.2 Gitee 流水线阶段定义

```yaml
stages:
  - name: build
    triggers:
      - push:
          branches: [master, develop, 'release/*']
  - name: test
    triggers:
      - push:
          branches: [master, develop]
  - name: security
    triggers:
      - push:
          branches: [master, develop]
      - schedule:
          cron: '0 2 * * *'
  - name: deploy-staging
    triggers:
      - push:
          branches: [develop]
  - name: deploy-production
    triggers:
      - push:
          branches: [master]
      - tag:
          pattern: 'v[0-9]+.[0-9]+.[0-9]+'
```

## 三、部署策略

### 3.1 蓝绿部署 (Blue-Green Deployment)

适用于: **生产环境**

```yaml
deploy-production:
  runs-on: self-hosted
  script: |
    # 蓝绿部署脚本
    export BLUE_GREEN_VERSION=$(date +%s)

    # 构建新版本镜像
    docker build -t $DOCKER_REGISTRY/agentos:latest-green .

    # 启动绿色环境
    docker-compose -f docker-compose.prod.yml up -d --scale agentos=0
    docker-compose -f docker-compose.prod.yml up -d agentos-green

    # 等待绿色环境就绪
    sleep 30

    # 健康检查
    curl -f https://agentos.spharx.cn/health || {
      echo "Green environment health check failed"
      docker-compose -f docker-compose.prod.yml rm -f agentos-green
      exit 1
    }

    # 切换流量 (通过 Nginx)
    # 将 agentos-upstream 指向 green

    # 监控 5 分钟
    sleep 300

    # 如果一切正常，停止蓝色环境
    docker-compose -f docker-compose.prod.yml rm -f agentos-blue || true

    echo "✅ Blue-green deployment completed"
```

### 3.2 金丝雀部署 (Canary Deployment)

适用于: **重大功能发布**

```yaml
deploy-canary:
  runs-on: self-hosted
  script: |
    # 金丝雀部署脚本
    CANARY_PERCENTAGE=10  # 初始流量 10%

    # 部署金丝雀版本
    kubectl set image deployment/agentos agentos=$DOCKER_REGISTRY/agentos:canary

    # 等待金丝雀就绪
    kubectl rollout status deployment/agentos-canary

    # 监控金丝雀指标
    for i in {1..60}; do
      ERROR_RATE=$(curl -s https://agentos.spharx.cn/metrics | grep error_rate | awk '{print $2}')
      if (( $(echo "$ERROR_RATE > 0.05" | bc -l) )); then
        echo "Error rate too high: $ERROR_RATE"
        kubectl rollout undo deployment/agentos
        exit 1
      fi
      sleep 10
    done

    # 逐步增加流量
    for PERCENT in 25 50 75 100; do
      kubectl patch service agentos -p '{"spec":{"selector":{"version":"'$PERCENT'%"}}}}'
      sleep 60
    done

    echo "✅ Canary deployment completed"
```

### 3.3 滚动更新 (Rolling Update)

适用于: **常规版本更新**

```yaml
deploy-rolling:
  runs-on: self-hosted
  script: |
    # 滚动更新脚本
    MAX_UNAVAILABLE=1
    MAX Surge=1

    # 执行滚动更新
    docker-compose -f docker-compose.prod.yml up -d --no-deps --scale agentos=2

    # 健康检查每个实例
    for i in {1..3}; do
      sleep 15
      HEALTH=$(curl -s http://localhost:8091/health)
      if [[ "$HEALTH" == *"healthy"* ]]; then
        echo "Instance $i is healthy"
      else
        echo "Instance $i health check failed"
        exit 1
      fi
    done

    echo "✅ Rolling update completed"
```

## 四、部署流程详解

### 4.1 开发环境部署

```yaml
deploy-dev:
  stage: build
  runs-on: ubuntu-22.04
  only:
    - /^feature/
    - /^bugfix/
    - merge_request
  script: |
    # 构建开发镜像
    docker build -t $DOCKER_REGISTRY/agentos:dev-$CI_COMMIT_SHA .

    # 部署到开发环境
    docker-compose -f docker-compose.dev.yml up -d

    # 输出服务地址
    echo "Dev environment deployed:"
    echo "  - API: http://dev-$CI_COMMIT_REF_NAME.agentos.local"
    echo "  - Gateway: http://dev-gateway-$CI_COMMIT_REF_NAME.agentos.local"
```

### 4.2 测试环境部署

```yaml
deploy-test:
  stage: build
  runs-on: ubuntu-22.04
  only:
    - develop
  script: |
    # 构建测试镜像
    docker build -t $DOCKER_REGISTRY/agentos:test-$CI_COMMIT_SHA .

    # 部署到测试环境
    docker-compose -f docker-compose.test.yml up -d

    # 运行集成测试
    pytest tests/integration/ -v

    # 清理
    docker-compose -f docker-compose.test.yml down
```

### 4.3 预发布环境部署

```yaml
deploy-staging:
  stage: deploy-staging
  runs-on: self-hosted
  only:
    - develop
  script: |
    # 构建预发布镜像
    docker build \
      --tag $DOCKER_REGISTRY/agentos:staging \
      --tag $DOCKER_REGISTRY/agentos:$CI_COMMIT_SHA \
      .

    # 推送到镜像仓库
    docker push $DOCKER_REGISTRY/agentos:staging
    docker push $DOCKER_REGISTRY/agentos:$CI_COMMIT_SHA

    # 更新预发布环境
    docker-compose -f docker-compose.staging.yml pull
    docker-compose -f docker-compose.staging.yml up -d

    # 等待服务就绪
    sleep 30

    # 执行冒烟测试
    pytest tests/smoke/ -v

    # 执行回归测试
    pytest tests/regression/ -v

    echo "✅ Staging deployment completed"
```

### 4.4 生产环境部署

```yaml
deploy-production:
  stage: deploy-production
  runs-on: self-hosted
  only:
    - master
    - tag
  when: manual  # 手动触发
  timeout: 120m
  script: |
    # 提取版本号
    if [[ "$CI_COMMIT_TAG" =~ ^v(.+)$ ]]; then
      VERSION=${BASH_REMATCH[1]}
    else
      VERSION=$CI_COMMIT_SHA
    fi

    echo "Deploying version: $VERSION"

    # 构建生产镜像
    docker build \
      --build-arg VERSION=$VERSION \
      --tag $DOCKER_REGISTRY/agentos:$VERSION \
      --tag $DOCKER_REGISTRY/agentos:latest \
      .

    # 推送镜像
    docker push $DOCKER_REGISTRY/agentos:$VERSION
    docker push $DOCKER_REGISTRY/agentos:latest

    # 备份当前配置
    docker-compose -f docker-compose.prod.yml manager > backup-manager.yml

    # 执行蓝绿部署
    ./scripts/deploy/blue-green.sh $VERSION

    # 执行健康检查
    for endpoint in health api/v1/status api/v1/metrics; do
      curl -f -m 30 https://agentos.spharx.cn/$endpoint || {
        echo "::error::Health check failed: $endpoint"
        ./scripts/deploy/rollback.sh $VERSION
        exit 1
      }
    done

    # 执行全面测试
    pytest tests/production/ -v

    echo "✅ Production deployment completed: $VERSION"
```

## 五、回滚机制

### 5.1 自动回滚

触发条件:
- 健康检查连续失败
- 错误率超过阈值
- 响应时间超过 SLO

```yaml
rollback-auto:
  runs-on: self-hosted
  script: |
    # 监控检测到异常
    ERROR_RATE=$(curl -s https://agentos.spharx.cn/metrics | grep error_rate | cut -d' ' -f2)
    HEALTH_STATUS=$(curl -s -o /dev/null -w '%{http_code}' http://localhost:8091/health)

    if (( $(echo "$ERROR_RATE > 0.01" | bc -l) )) || [[ "$HEALTH_STATUS" != "200" ]]; then
      echo "Detected anomaly, initiating automatic rollback..."

      # 获取上一个稳定版本
      LAST_STABLE=$(git describe --tags --abbrev=0 HEAD^)

      # 执行回滚
      ./scripts/deploy/rollback.sh $LAST_STABLE

      # 通知
      curl -X POST "https://oapi.dingtalk.com/robot/send?access_token=$DINGTALK_TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"msgtype\": \"text\", \"text\": {\"content\": \"⚠️ 自动回滚已执行: $LAST_STABLE\"}}"
    fi
```

### 5.2 手动回滚

```yaml
rollback-manual:
  runs-on: self-hosted
  stage: deploy-production
  when: manual
  script: |
    read -p "Enter version to rollback to (e.g., v1.0.0): " TARGET_VERSION

    if [[ ! "$TARGET_VERSION" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
      echo "::error::Invalid version format"
      exit 1
    fi

    echo "Rolling back to: $TARGET_VERSION"

    # 确认操作
    read -p "Are you sure? (yes/no): " CONFIRM
    if [[ "$CONFIRM" != "yes" ]]; then
      echo "Rollback cancelled"
      exit 0
    fi

    # 执行回滚
    ./scripts/deploy/rollback.sh $TARGET_VERSION

    # 验证
    sleep 30
    curl -f https://agentos.spharx.cn/health || {
      echo "::error::Rollback verification failed"
      exit 1
    }

    echo "✅ Rollback to $TARGET_VERSION completed"
```

### 5.3 回滚脚本

```bash
#!/bin/bash
# scripts/deploy/rollback.sh

TARGET_VERSION=$1
CURRENT_VERSION=$(git describe --tags --abbrev=0 HEAD)
DOCKER_REGISTRY=gitee.cn/spharx-agentos

echo "=== Rollback Process ==="
echo "Current: $CURRENT_VERSION -> Target: $TARGET_VERSION"

# 停止当前服务
echo "Stopping current services..."
docker-compose -f docker-compose.prod.yml down

# 拉取目标版本镜像
echo "Pulling target version images..."
docker pull $DOCKER_REGISTRY/agentos:$TARGET_VERSION
docker tag $DOCKER_REGISTRY/agentos:$TARGET_VERSION $DOCKER_REGISTRY/agentos:latest

# 启动目标版本
echo "Starting target version..."
VERSION=$TARGET_VERSION docker-compose -f docker-compose.prod.yml up -d

# 等待服务就绪
echo "Waiting for services to be ready..."
sleep 30

# 健康检查
echo "Running health checks..."
for i in {1..5}; do
  if curl -f http://localhost:8091/health > /dev/null 2>&1; then
    echo "✅ Health check passed"
    exit 0
  fi
  sleep 10
done

echo "::error::Health check failed after rollback"
exit 1
```

## 六、审批流程

### 6.1 生产部署审批

```yaml
deploy-production:
  stage: deploy-production
  runs-on: self-hosted
  only:
    - master
    - tag
  script: |
    # 检查是否为授权部署
    if [[ "$GITEE_USER" != "admin" ]] && \
       [[ "$GITEE_USER" != "deploy-bot" ]]; then
      echo "::error::You are not authorized to deploy to production"
      exit 1
    fi

    # 检查版本标签是否存在
    if [[ -n "$CI_COMMIT_TAG" ]]; then
      echo "Deploying tag: $CI_COMMIT_TAG"
    else
      echo "::error::Production deployment requires a version tag"
      exit 1
    fi

    # 执行部署
    ./scripts/deploy/production.sh $CI_COMMIT_TAG
```

### 6.2 审批记录

所有生产部署都必须记录:
- 部署人员
- 部署时间
- 版本信息
- 变更内容
- 审批人

## 七、监控与告警

### 7.1 部署后监控

```yaml
post-deploy-monitor:
  stage: deploy-production
  runs-on: self-hosted
  script: |
    # 监控指标
    METRICS=(
      "error_rate"
      "request_latency_p99"
      "cpu_usage"
      "memory_usage"
      "active_connections"
    )

    echo "=== Post-Deployment Monitoring (30 minutes) ==="

    for i in {1..30}; do
      for METRIC in "${METRICS[@]}"; do
        VALUE=$(curl -s https://agentos.spharx.cn/metrics | grep "^$METRIC " | awk '{print $2}')
        echo "[$(date '+%H:%M:%S')] $METRIC = $VALUE"

        # 检查阈值
        case $METRIC in
          error_rate)
            if (( $(echo "$VALUE > 0.01" | bc -l) )); then
              echo "::warning::High error rate: $VALUE"
            fi
            ;;
        esac
      done
      sleep 60
    done

    echo "✅ Monitoring completed"
```

### 7.2 告警配置

```yaml
alerting:
  triggers:
    - name: deployment-failed
      condition: $CI_JOB_STATUS == "failed"
      channels: [dingtalk, email]
      severity: critical

    - name: deployment-succeeded
      condition: $CI_JOB_STATUS == "success"
      channels: [dingtalk]
      severity: info
```

## 八、部署检查清单

### 8.1 部署前检查

- [ ] 代码审查已通过
- [ ] 所有测试已通过
- [ ] 安全扫描已完成
- [ ] 版本号已更新
- [ ] CHANGELOG 已更新
- [ ] 文档已更新 (如需要)
- [ ] 回滚计划已准备
- [ ] 监控告警已配置

### 8.2 部署后检查

- [ ] 服务健康检查通过
- [ ] API 端点响应正常
- [ ] 错误率在正常范围
- [ ] 响应时间在 SLO 内
- [ ] 日志正常输出
- [ ] 监控数据正常
- [ ] 告警规则正常

## 九、最佳实践

### 9.1 部署频率

| 环境 | 部署频率 | 说明 |
|------|----------|------|
| dev | 多次/天 | 每次 PR 更新 |
| test | 每次合并 | 自动化 |
| staging | 每次 develop | 自动化 |
| production | 按需 | 手动审批 |

### 9.2 部署时间窗口

| 环境 | 建议时间 | 说明 |
|------|----------|------|
| production | 工作日 10:00-16:00 | 便于监控和问题响应 |
| staging | 任何时间 | 自动化,无需限制 |
| test/dev | 任何时间 | 自动化,无需限制 |

### 9.3 变更大小

- **小变更**: 修复、小的功能 → 滚动更新
- **中变更**: 新功能、较大修复 → 蓝绿部署
- **大变更**: 架构变更、重大版本 → 金丝雀部署

---

*© 2026 SPHARX Ltd. 保留所有权利*
