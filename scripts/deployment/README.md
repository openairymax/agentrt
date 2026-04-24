# 部署脚本

`scripts/deployment/`

## 概述

`deployment/` 目录提供 AgentOS 的生产环境部署管理脚本，支持 Kubernetes 集群部署、Docker Compose 本地部署、环境配置管理等场景，实现一键部署和灰度发布。

## 脚本列表

| 脚本 | 说明 |
|------|------|
| `deploy.sh` | 通用部署入口，支持多种部署环境 |
| `k8s_deploy.sh` | Kubernetes 集群部署管理 |
| `rollback.sh` | 版本回滚操作 |
| `env_setup.sh` | 环境配置与初始化 |

## 使用示例

```bash
# 部署到 Kubernetes
./deployment/deploy.sh --env production --namespace agentos

# 回滚到上一版本
./deployment/rollback.sh --revision previous

# 环境初始化
./deployment/env_setup.sh --env staging
```

## 部署模式

- **Kubernetes 部署**: 生产环境推荐，支持 HPA 自动扩缩容、滚动更新、健康检查
- **Docker Compose 部署**: 开发测试环境，快速启动完整服务栈（详见 `docker/` 子目录）
- **裸机部署**: 适用于资源受限环境或嵌入式场景

---

© 2026 SPHARX Ltd. All Rights Reserved.
