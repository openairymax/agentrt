# Gateway 部署指南

**模块路径**: `agentos/gateway/deploy/`
**版本**: v0.0.5

## 概述

`gateway/deploy/` 包含 AgentOS Gateway 的 Kubernetes 和 Docker 部署配置，提供从开发环境到生产环境的完整部署方案。K8s 配置支持多环境 ConfigMap、滚动更新、自动扩缩容、网络策略和 Ingress TLS 终止。

## 目录结构

```
deploy/
├── k8s/                    # Kubernetes 资源清单
│   ├── namespace.yaml      # 命名空间定义（agentos-gateway）
│   ├── configmap.yaml      # 多环境 ConfigMap（development/staging/production）
│   ├── deployment.yaml     # Deployment + ServiceAccount + HPA
│   └── service.yaml        # Service + Headless Service + Ingress + NetworkPolicy
└── README.md               # 本文件
```

## Docker 部署

### 开发环境

```bash
docker-compose up -d
```

### 生产环境

```bash
docker build -t agentos-gateway:latest .
docker run -d \
  --name agentos-gateway \
  -p 8080:8080 \
  -p 8081:8081 \
  -p 9090:9090 \
  -v /etc/agentos/config:/etc/agentos/gateway:ro \
  -v /etc/agentos/certs:/etc/agentos/certs:ro \
  agentos-gateway:latest
```

## Kubernetes 部署

### 部署步骤

```bash
# 1. 创建命名空间
kubectl apply -f k8s/namespace.yaml

# 2. 创建配置（根据目标环境选择对应 ConfigMap 段）
kubectl apply -f k8s/configmap.yaml

# 3. 部署网关（含 Deployment + ServiceAccount + HPA）
kubectl apply -f k8s/deployment.yaml

# 4. 创建服务暴露（含 Service + Ingress + NetworkPolicy）
kubectl apply -f k8s/service.yaml
```

### K8s 资源说明

| 资源 | 文件 | 说明 |
|------|------|------|
| **Namespace** | `namespace.yaml` | 隔离环境 `agentos-gateway`，避免资源冲突 |
| **ConfigMap** | `configmap.yaml` | 三环境配置：development / staging / production，挂载到容器 `/etc/agentos/gateway` |
| **Deployment** | `deployment.yaml` | 3 副本部署，滚动更新策略（maxSurge=1, maxUnavailable=0），含安全上下文（runAsNonRoot） |
| **ServiceAccount** | `deployment.yaml` | 服务账户 `agentos-gateway`，用于 RBAC 权限控制 |
| **HPA** | `deployment.yaml` | 基于 CPU(70%) 和内存(80%) 自动扩缩容，范围 3-10 副本，缩容稳定窗口 300s |
| **Service** | `service.yaml` | ClusterIP 类型，暴露 HTTP(8080) / WebSocket(8081) / Metrics(9090) 三个端口 |
| **Headless Service** | `service.yaml` | 无头服务，用于 StatefulSet 场景下的 Pod 直接寻址 |
| **Ingress** | `service.yaml` | Nginx Ingress + cert-manager TLS，支持 API 和 WebSocket 双域名路由 |
| **NetworkPolicy** | `service.yaml` | 入站限制（仅 Ingress 和监控命名空间），出站限制（DNS + Redis + 外部 HTTPS） |

### 端口映射

| 端口 | 协议 | 用途 |
|------|------|------|
| 8080 | TCP | HTTP REST API |
| 8081 | TCP | WebSocket 双向通信 |
| 9090 | TCP | Prometheus 指标端点 |

### 健康检查

| 探针 | 路径 | 初始延迟 | 周期 | 超时 | 失败阈值 |
|------|------|---------|------|------|---------|
| **livenessProbe** | `/health` | 10s | 15s | 5s | 3 |
| **readinessProbe** | `/health/ready` | 5s | 10s | 3s | 3 |
| **startupProbe** | `/health` | 0s | 5s | 3s | 30 |

### 资源限制

| 资源 | Requests | Limits |
|------|----------|--------|
| CPU | 500m | 2 |
| Memory | 128Mi | 512Mi |

## 环境变量

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `AGENTOS_MODULE` | `gateway` | 模块标识 |
| `AGENTOS_LOG_LEVEL` | `INFO` | 日志级别（DEBUG/INFO/WARN/ERROR） |
| `AGENTOS_HTTP_PORT` | `8080` | HTTP 监听端口 |
| `AGENTOS_WS_PORT` | `8081` | WebSocket 监听端口 |
| `AGENTOS_METRICS_PORT` | `9090` | Prometheus 指标端口 |
| `AGENTOS_MAX_SESSIONS` | `1000` | 最大会话数 |
| `AGENTOS_SESSION_TIMEOUT` | `900` | 会话超时（秒） |
| `AGENTOS_REDIS_HOST` | - | Redis 主机地址 |
| `AGENTOS_REDIS_PORT` | `6379` | Redis 端口 |
| `AGENTOS_JWT_SECRET` | - | JWT 签名密钥（从 Secret 挂载） |
| `AGENTOS_ENABLE_AUTH` | - | 是否启用认证 |
| `ENABLE_METRICS` | `true` | 是否启用 Prometheus 指标 |
| `ENABLE_TRACING` | `true` | 是否启用链路追踪 |

### 多环境 ConfigMap 差异

| 配置项 | Development | Staging | Production |
|--------|-------------|---------|------------|
| `AGENTOS_LOG_LEVEL` | DEBUG | INFO | WARN |
| `AGENTOS_MAX_SESSIONS` | 100 | 500 | 1000 |
| `AGENTOS_SESSION_TIMEOUT` | 3600 | 1800 | 900 |

## 安全配置

- **TLS**：通过 cert-manager + Let's Encrypt 自动证书管理，Ingress 层 TLS 终止
- **速率限制**：基于令牌桶算法，防止 DDoS 攻击
- **CORS**：支持跨域配置，生产环境建议限制来源
- **网络策略**：入站仅允许 Ingress 控制器和监控命名空间，出站限制为 DNS/Redis/外部 HTTPS
- **安全上下文**：容器以非 root 用户（UID 1000）运行，只读根文件系统
- **Pod 反亲和**：优先调度到不同节点，提升可用性

## 监控集成

- **Prometheus**：通过注解 `prometheus.io/scrape: "true"` 自动发现，采集端口 9090
- **Grafana**：预配置 AgentOS 仪表盘（`docker/monitoring/grafana_agentos_dashboard.json`）
- **告警规则**：`docker/monitoring/alerts.yml` 定义网关专用告警

---

© 2026 SPHARX Ltd. All Rights Reserved.
