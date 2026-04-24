# Gateway 部署指南

`gateway/deploy/` 包含 AgentOS Gateway 的 Docker 和 Kubernetes 部署配置。

## 目录结构

```
deploy/
├── Dockerfile              # Docker 构建文件
├── docker-compose.yml      # Docker Compose 配置
├── kubernetes/             # K8s 部署配置
│   ├── namespace.yaml
│   ├── configmap.yaml
│   ├── deployment.yaml
│   ├── service.yaml
│   └── hpa.yaml
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
  -p 8443:8443 \
  -v /etc/agentos/config:/etc/agentos/config \
  -v /etc/agentos/certs:/etc/agentos/certs \
  agentos-gateway:latest
```

## Kubernetes 部署

```bash
# 创建命名空间
kubectl apply -f kubernetes/namespace.yaml

# 创建配置和密钥
kubectl apply -f kubernetes/configmap.yaml

# 部署网关
kubectl apply -f kubernetes/deployment.yaml
kubectl apply -f kubernetes/service.yaml

# 配置自动伸缩
kubectl apply -f kubernetes/hpa.yaml
```

### K8s 配置说明

| 资源 | 说明 |
|------|------|
| **Namespace** | 隔离环境，避免资源冲突 |
| **ConfigMap** | 网关配置文件，挂载到容器 `/etc/agentos/config` |
| **Deployment** | 3 副本部署，滚动更新策略 |
| **Service** | ClusterIP 类型，内部服务发现 |
| **HPA** | 基于 CPU(70%) 和内存(80%) 自动扩缩容，范围 3-10 副本 |

## 环境变量

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `GATEWAY_HTTP_PORT` | 8080 | HTTP 监听端口 |
| `GATEWAY_HTTPS_PORT` | 8443 | HTTPS 监听端口 |
| `GATEWAY_LOG_LEVEL` | info | 日志级别 |
| `GATEWAY_RATE_LIMIT` | 1000 | 每秒请求限制 |

## 安全配置

- **CORS**：支持跨域配置，生产环境建议限制来源
- **速率限制**：基于令牌桶算法，防止 DDoS
- **TLS**：支持 HTTPS，建议使用 Let's Encrypt 自动证书管理
- **监控**：集成 Prometheus 指标端点，Grafana 可视化

---

*AgentOS Gateway Deploy*
