# AgentOS Gateway 部署指南

**版本**: v1.0.0  
**更新日期**: 2026-04-05  
**适用环境**: Docker / Kubernetes / 裸机部署

---

## 📋 目录

1. [快速开始](#快速开始)
2. [Docker 部署](#docker-部署)
3. [Kubernetes 部署](#kubernetes-部署)
4. [环境变量配置](#环境变量配置)
5. [安全配置](#安全配置)
6. [监控与日志](#监控与日志)
7. [故障排查](#故障排查)

---

## 🚀 快速开始

### 前置要求

- Docker 20.10+ 或 Kubernetes 1.20+
- 至少 512MB 可用内存
- 端口 8080（可配置）

### 最小化启动（开发模式）

```bash
# 使用 Docker 快速启动
docker run -d \
  --name agentos-gateway \
  -p 8080:8080 \
  -e GATEWAY_CORS_MODE=dev \
  -e GATEWAY_RATE_LIMIT_ENABLED=false \
  agentos/gateway:latest

# 验证服务
curl http://localhost:8080/health
```

### 生产环境启动

```bash
# 使用 Docker Compose
docker-compose -f docker-compose.prod.yml up -d

# 或使用 Kubernetes
kubectl apply -f deploy/k8s/
```

---

## 🐳 Docker 部署

### 开发环境

使用 `docker-compose.dev.yml`：

```bash
docker-compose -f docker-compose.dev.yml up -d
```

**特点**:
- CORS 开发模式（允许所有来源）
- 速率限制禁用
- 详细日志输出
- 自动重启

### 生产环境

使用 `docker-compose.prod.yml`：

```bash
# 1. 配置环境变量
export GATEWAY_CORS_ORIGINS="https://your-domain.com,https://api.your-domain.com"
export GATEWAY_RATE_LIMIT_ENABLED=true
export GATEWAY_RATE_LIMIT_RPS=100

# 2. 启动服务
docker-compose -f docker-compose.prod.yml up -d

# 3. 查看日志
docker-compose logs -f gateway
```

**特点**:
- CORS 白名单模式
- 速率限制启用
- 资源限制（CPU 2核，内存 512MB）
- 健康检查
- 日志轮转

### Docker 镜像构建

```bash
# 本地构建
docker build -t agentos/gateway:local -f docker/Dockerfile .

# 多阶段构建（优化镜像大小）
docker build -t agentos/gateway:alpine -f docker/Dockerfile.alpine .
```

---

## ☸️ Kubernetes 部署

### 部署步骤

```bash
# 1. 创建命名空间
kubectl apply -f deploy/k8s/namespace.yaml

# 2. 创建配置
kubectl apply -f deploy/k8s/configmap.yaml

# 3. 创建密钥（敏感信息）
kubectl create secret generic gateway-secrets \
  --from-literal=cors-origins='https://your-domain.com' \
  -n agentos

# 4. 部署应用
kubectl apply -f deploy/k8s/deployment.yaml

# 5. 创建服务
kubectl apply -f deploy/k8s/service.yaml

# 6. 验证部署
kubectl get pods -n agentos -l app=gateway
kubectl logs -n agentos -l app=gateway -f
```

### 高可用配置

```yaml
# deployment.yaml 关键配置
spec:
  replicas: 3  # 3个副本
  strategy:
    type: RollingUpdate
    rollingUpdate:
      maxSurge: 1
      maxUnavailable: 0  # 零停机更新
  template:
    spec:
      affinity:
        podAntiAffinity:  # 分散部署
          requiredDuringSchedulingIgnoredDuringExecution:
          - labelSelector:
              matchLabels:
                app: gateway
            topologyKey: kubernetes.io/hostname
```

### 自动伸缩（HPA）

```yaml
apiVersion: autoscaling/v2
kind: HorizontalPodAutoscaler
metadata:
  name: gateway-hpa
  namespace: agentos
spec:
  scaleTargetRef:
    apiVersion: apps/v1
    kind: Deployment
    name: gateway
  minReplicas: 3
  maxReplicas: 10
  metrics:
  - type: Resource
    resource:
      name: cpu
      target:
        type: Utilization
        averageUtilization: 70
  - type: Resource
    resource:
      name: memory
      target:
        type: Utilization
        averageUtilization: 80
```

---

## ⚙️ 环境变量配置

### 核心配置

| 变量名 | 默认值 | 说明 |
|--------|--------|------|
| `GATEWAY_PORT` | `8080` | 监听端口 |
| `GATEWAY_HOST` | `0.0.0.0` | 监听地址 |

### 安全配置

| 变量名 | 默认值 | 说明 |
|--------|--------|------|
| `GATEWAY_CORS_MODE` | `prod` | CORS 模式：`dev`（允许所有）或 `prod`（白名单） |
| `GATEWAY_CORS_ORIGINS` | 空 | 允许的来源列表（逗号分隔） |
| `GATEWAY_MAX_REQUEST_SIZE` | `1048576` | 最大请求体（字节，默认1MB） |
| `GATEWAY_RATE_LIMIT_ENABLED` | `false` | 是否启用速率限制 |
| `GATEWAY_RATE_LIMIT_RPS` | `100` | 每秒最大请求数 |
| `GATEWAY_RATE_LIMIT_RPM` | `6000` | 每分钟最大请求数 |

### 性能配置

| 变量名 | 默认值 | 说明 |
|--------|--------|------|
| `GATEWAY_HTTP_TIMEOUT` | `30` | HTTP 连接超时（秒） |
| `GATEWAY_CONNECTION_LIMIT` | `1000` | 最大并发连接数 |

### 日志配置

| 变量名 | 默认值 | 说明 |
|--------|--------|------|
| `GATEWAY_LOG_LEVEL` | `info` | 日志级别：`debug`, `info`, `warn`, `error` |
| `GATEWAY_LOG_FORMAT` | `json` | 日志格式：`json` 或 `text` |

### 配置示例

```bash
# .env 文件示例
GATEWAY_PORT=8080
GATEWAY_CORS_MODE=prod
GATEWAY_CORS_ORIGINS=https://example.com,https://api.example.com
GATEWAY_MAX_REQUEST_SIZE=2097152
GATEWAY_RATE_LIMIT_ENABLED=true
GATEWAY_RATE_LIMIT_RPS=200
GATEWAY_LOG_LEVEL=info
```

---

## 🔒 安全配置

### CORS 配置

#### 开发环境

```bash
# 允许所有来源（仅用于开发！）
GATEWAY_CORS_MODE=dev
```

#### 生产环境

```bash
# 白名单模式
GATEWAY_CORS_MODE=prod
GATEWAY_CORS_ORIGINS=https://your-domain.com,https://api.your-domain.com
```

**安全建议**:
- ✅ 生产环境必须使用白名单模式
- ✅ 只添加信任的域名
- ✅ 定期审查白名单

### 速率限制

#### 基础配置

```bash
# 启用速率限制
GATEWAY_RATE_LIMIT_ENABLED=true
GATEWAY_RATE_LIMIT_RPS=100
GATEWAY_RATE_LIMIT_RPM=6000
```

#### 高级配置（通过配置文件）

```yaml
rate_limit:
  enabled: true
  requests_per_second: 100
  requests_per_minute: 6000
  requests_per_hour: 360000
  burst_size: 150
  cleanup_interval_sec: 300
```

### TLS/HTTPS 配置

**推荐方式**: 在反向代理层（Nginx/Envoy）处理 TLS

```nginx
# Nginx 配置示例
server {
    listen 443 ssl http2;
    server_name api.your-domain.com;

    ssl_certificate /path/to/cert.pem;
    ssl_certificate_key /path/to/key.pem;
    ssl_protocols TLSv1.2 TLSv1.3;

    location / {
        proxy_pass http://gateway:8080;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
}
```

---

## 📊 监控与日志

### Prometheus 指标

Gateway 暴露以下指标端点：

```bash
# 获取指标
curl http://localhost:8080/metrics
```

**关键指标**:

| 指标名称 | 类型 | 说明 |
|---------|------|------|
| `gateway_http_requests_total` | Counter | 总请求数 |
| `gateway_http_request_duration_seconds` | Histogram | 请求延迟 |
| `gateway_http_active_connections` | Gauge | 活跃连接数 |
| `gateway_rate_limit_rejected_total` | Counter | 速率限制拒绝数 |

### Grafana 监控面板

导入预配置的 Dashboard：

```bash
# 导入 JSON 配置
kubectl create configmap grafana-dashboard-gateway \
  --from-file=dashboard.json=monitoring/grafana-dashboard.json \
  -n monitoring
```

### 日志查看

#### Docker

```bash
# 查看实时日志
docker-compose logs -f gateway

# 查看最近100行
docker-compose logs --tail=100 gateway

# 过滤错误日志
docker-compose logs gateway | grep ERROR
```

#### Kubernetes

```bash
# 查看Pod日志
kubectl logs -n agentos -l app=gateway -f

# 查看特定Pod
kubectl logs -n agentos gateway-xxxxx -c gateway

# 导出日志到文件
kubectl logs -n agentos -l app=gateway > gateway.log
```

---

## 🔧 故障排查

### 常见问题

#### 1. 服务无法启动

**症状**: 容器启动后立即退出

**排查步骤**:
```bash
# 查看容器日志
docker logs agentos-gateway

# 检查端口占用
netstat -tuln | grep 8080

# 检查资源限制
docker inspect agentos-gateway | grep -A 10 "Memory"
```

**常见原因**:
- 端口被占用 → 更改 `GATEWAY_PORT`
- 内存不足 → 增加容器内存限制
- 配置错误 → 检查环境变量

#### 2. CORS 错误

**症状**: 浏览器控制台显示 CORS 错误

**解决方案**:
```bash
# 开发环境：允许所有来源
GATEWAY_CORS_MODE=dev

# 生产环境：添加域名到白名单
GATEWAY_CORS_ORIGINS=https://your-frontend.com
```

#### 3. 速率限制误伤

**症状**: 正常请求被拒绝（429错误）

**解决方案**:
```bash
# 调整限制
GATEWAY_RATE_LIMIT_RPS=200
GATEWAY_RATE_LIMIT_RPM=12000

# 或暂时禁用（不推荐生产环境）
GATEWAY_RATE_LIMIT_ENABLED=false
```

#### 4. 性能问题

**症状**: 响应延迟高

**排查步骤**:
```bash
# 查看资源使用
docker stats agentos-gateway

# 查看Prometheus指标
curl http://localhost:8080/metrics | grep duration

# 分析慢请求
kubectl logs -n agentos -l app=gateway | grep "duration.*[0-9]{4,}ms"
```

**优化建议**:
- 增加副本数（K8s HPA）
- 调整资源限制
- 优化客户端请求频率

### 健康检查

```bash
# HTTP 健康检查
curl http://localhost:8080/health

# 预期响应
{
  "status": "healthy",
  "service": "gateway"
}
```

### 调试模式

```bash
# 启用详细日志
GATEWAY_LOG_LEVEL=debug

# 查看详细日志
docker-compose logs -f gateway | grep DEBUG
```

---

## 📚 相关文档

- [架构设计文档](../../docs/architecture/overview.md)
- [API 接口文档](../README.md)
- [安全最佳实践](../../docs/security/SECURITY.md)
- [性能调优指南](../../docs/performance/tuning.md)

---

## 🆘 获取帮助

- **GitHub Issues**: https://github.com/spharx/agentos/issues
- **文档**: https://docs.agentos.io
- **社区**: https://community.agentos.io

---

**© 2026 SPHARX Ltd. All Rights Reserved.**  
*"From data intelligence emerges."*
