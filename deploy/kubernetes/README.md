# Kubernetes — AgentRT Kubernetes 部署

**模块路径**: `deploy/kubernetes/`
**Chart 版本**: v0.1.1
**应用版本**: 0.1.1

## 概述

AgentRT Kubernetes 部署方案提供完整的 Helm Chart，用于在生产环境中部署和管理 AgentRT 智能体运行时平台。包含 Deployment、Service、Ingress、ConfigMap 等核心资源模板，支持多环境配置（默认/生产）。

## 目录结构

```
kubernetes/
├── helm/
│   ├── Chart.yaml                   # Chart 元信息
│   ├── values.yaml                  # 默认配置值
│   ├── values-prod.yaml             # 生产环境覆盖配置
│   └── templates/
│       ├── _helpers.tpl             # 模板辅助函数
│       ├── configmap.yaml           # ConfigMap 配置
│       ├── deployments.yaml         # Deployment 部署
│       ├── ingress.yaml             # Ingress 入口
│       └── services.yaml            # Service 暴露
└── README.md                        # 本文件
```

## 核心组件

| 资源模板 | 说明 |
|----------|------|
| **deployments.yaml** | AgentRT Gateway 和 Worker 的 Deployment 配置，含资源限制、健康检查、亲和性调度 |
| **services.yaml** | ClusterIP 和 LoadBalancer Service 暴露内部与外部访问 |
| **ingress.yaml** | Ingress 规则配置，支持域名和 TLS |
| **configmap.yaml** | 环境变量和配置文件注入 |
| **_helpers.tpl** | 通用模板函数（标签、名称、选择器） |

## 部署步骤

### 前置条件

- Kubernetes 集群（v1.22+）
- Helm v3.8+
- kubectl 已配置集群访问

### 安装 Chart

```bash
# 使用默认配置安装
helm install agentrt ./helm

# 使用生产配置安装
helm install agentrt ./helm -f ./helm/values-prod.yaml

# 指定命名空间
helm install agentrt ./helm --namespace agentrt --create-namespace
```

### 配置说明

编辑 `values.yaml` 或使用 `values-prod.yaml` 覆盖默认值：

```yaml
# 镜像配置
image:
  repository: spharx/agentrt
  tag: "0.1.1"
  pullPolicy: IfNotPresent

# 资源限制
resources:
  gateway:
    requests:
      cpu: 500m
      memory: 512Mi
    limits:
      cpu: 2
      memory: 2Gi

# 服务配置
service:
  type: ClusterIP
  port: 8080

# Ingress 配置
ingress:
  enabled: true
  host: agentrt.example.com
  tls:
    enabled: true
    secretName: agentrt-tls
```

### 升级与回滚

```bash
# 升级
helm upgrade agentrt ./helm -f ./helm/values-prod.yaml

# 回滚
helm rollback agentrt 1

# 查看部署历史
helm history agentrt
```

### 卸载

```bash
helm uninstall agentrt
```

## 自定义资源

### 生产环境配置

使用 `values-prod.yaml` 覆盖生产环境特定配置：

```bash
helm install agentrt ./helm \
  -f ./helm/values-prod.yaml \
  --set image.tag=0.1.1 \
  --set ingress.host=agentrt.production.com
```

### 多副本部署

```yaml
replicaCount: 3
autoscaling:
  enabled: true
  minReplicas: 3
  maxReplicas: 10
  targetCPUUtilizationPercentage: 80
```

## 依赖关系

- **容器运行时**: Docker / containerd
- **编排平台**: Kubernetes v1.22+
- **包管理**: Helm v3.8+
- **基础镜像**: 基于 Alpine / Distroless 构建

---

© 2026 SPHARX Ltd. All Rights Reserved.