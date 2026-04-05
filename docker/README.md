# AgentOS Docker 镜像

**版本**: 1.0.0  
**最后更新**: 2026-04-05  
**维护者**: @spharx-team/devops  

---

## 📋 镜像列表

| 镜像名称 | 用途 | 基础镜像 | 大小 |
|---------|------|---------|------|
| `spharx/agentos:kernel` | 内核构建环境 | ubuntu:22.04 | ~1.2GB |
| `spharx/agentos:daemon` | 服务层运行时 | ubuntu:22.04 | ~800MB |
| `spharx/agentos:openlab` | OpenLab应用 | python:3.11-slim | ~500MB |
| `spharx/agentos:full` | 完整系统（开发用） | ubuntu:22.04 | ~2GB |

---

## 🚀 快速开始

### 方式一：使用Docker Compose（推荐）

```bash
# 克隆仓库
git clone https://gitcode.com/spharx/agentos.git && cd agentos

# 启动所有服务
docker compose -f docker/docker-compose.yml up -d

# 查看服务状态
docker compose -f docker/docker-compose.yml ps

# 查看日志
docker compose -f docker/docker-compose.yml logs -f
```

### 方式二：单独启动内核镜像

```bash
# 拉取镜像
docker pull spharx/agentos:latest

# 运行内核
docker run -d \
  --name agentos-kernel \
  -p 8080:8080 \
  -v agentos-data:/app/data \
  -v ./config:/app/config \
  spharx/agentos:latest
```

---

## 🏗️ 构建镜像

### 从源码构建

```bash
# 构建所有镜像
docker compose -f docker/docker-compose.yml build

# 仅构建内核镜像
docker build -t spharx/agentos:kernel -f docker/Dockerfile.kernel .

# 仅构建OpenLab镜像
docker build -t spharx/agentos:openlab -f docker/Dockerfile.openlab .
```

### 自定义构建选项

```bash
# Release版本构建
docker build \
  --build-arg BUILD_TYPE=Release \
  --build-arg ENABLE_TESTS=OFF \
  -t spharx/agentos:custom \
  -f docker/Dockerfile.kernel .
```

---

## ⚙️ 配置说明

### 环境变量

复制示例配置文件：

```bash
cp docker/.env.example docker/.env
```

编辑 `.env` 文件：

```env
# =============================================================================
# AgentOS Docker 环境变量配置
# =============================================================================

# -----------------------------------------------------------------------------
# 通用配置 (General)
# -----------------------------------------------------------------------------

# 日志级别: DEBUG, INFO, WARNING, ERROR, CRITICAL
LOG_LEVEL=INFO

# 时区
TZ=Asia/Shanghai

# 语言
LANG=en_US.UTF-8

# -----------------------------------------------------------------------------
# 内核配置 (Kernel)
# -----------------------------------------------------------------------------

# IPC绑定地址
AGENTOS_IPC_BIND_ADDR=0.0.0.0

# IPC绑定端口
AGENTOS_IPC_BIND_PORT=8080

# 最大并发连接数
AGENTOS_MAX_CONNECTIONS=1000

# 内存限制(MB)
AGENTOS_MEMORY_LIMIT=2048

# CPU核心数限制
AGENTOS_CPU_CORES=4

# -----------------------------------------------------------------------------
# 记忆系统配置 (Memory)
# -----------------------------------------------------------------------------

# L1存储路径
AGENTOS_L1_STORAGE_PATH=/app/data/memory/l1

# L2索引类型: faiss_ivf, faiss_hnsw
AGENTOS_L2_INDEX_TYPE=faiss_ivf

# FAISS分区数
AGENTOS_FAISS_NLIST=1024

# FAISS探测数
AGENTOS_FAISS_NPROBE=32

# 向量维度
AGENTOS_EMBEDDING_DIM=768

# -----------------------------------------------------------------------------
# 安全配置 (Security)
# -----------------------------------------------------------------------------

# 启用安全穹顶
AGENTOS_CUPOLAS_ENABLED=true

# 权限检查模式: strict, relaxed, disabled
AGENTOS_PERMISSION_MODE=strict

# 输入净化模式: strict, relaxed, disabled
AGENTOS_SANITIZER_MODE=strict

# 启用审计日志
AGENTOS_AUDIT_ENABLED=true

# 审计日志路径
AGENTOS_AUDIT_LOG_PATH=/app/logs/audit.log

# -----------------------------------------------------------------------------
# LLM服务配置 (LLM Service)
# -----------------------------------------------------------------------------

# LLM提供商: openai, deepseek, local
AGENTOS_LLM_PROVIDER=openai

# API密钥 (生产环境请使用secrets管理)
AGENTOS_LLM_API_KEY=sk-your-api-key-here

# API基础URL
AGENTOS_LLM_BASE_URL=https://api.openai.com/v1

# 模型名称
AGENTOS_LLM_MODEL=gpt-4-turbo

# 最大Token数
AGENTOS_LLM_MAX_TOKENS=4096

# 温度参数
AGENTOS_LLM_TEMPERATURE=0.7

# -----------------------------------------------------------------------------
# OpenLab配置 (OpenLab)
# -----------------------------------------------------------------------------

# OpenLab监听端口
OPENLAB_PORT=5173

# 允许的CORS来源
OPENLAB_CORS_ORIGINS=http://localhost:5173,http://127.0.0.1:5173

# 会话超时(秒)
OPENLAB_SESSION_TIMEOUT=3600

# 最大会话数
OPENLAB_MAX_SESSIONS=100
```

---

## 📁 目录结构

```
docker/
├── .env.example              # 环境变量示例
├── .gitignore               # Docker忽略文件
├── README.md                # 本文档
├── Dockerfile.kernel         # 内核镜像构建文件
├── Dockerfile.daemon         # 服务层镜像构建文件
├── Dockerfile.openlab        # OpenLab应用镜像构建文件
├── docker-compose.yml        # 开发环境编排
└── docker-compose.prod.yml   # 生产环境编排
```

---

## 🔧 开发指南

### 本地开发环境

使用 `docker-compose.yml` 进行本地开发：

```yaml
# docker-compose.yml 关键配置
services:
  kernel:
    build:
      context: ..
      dockerfile: docker/Dockerfile.kernel
      target: development
    volumes:
      # 挂载源码目录，支持热重载
      - ../agentos:/app/source:ro
      # 挂载构建产物
      - kernel-build:/app/build
      # 挂载配置
      - ../config:/app/config:ro
    ports:
      - "8080:8080"
      - "9090:9090"  # 监控端口
    environment:
      - LOG_LEVEL=DEBUG
      - AGENTOS_CUPOLAS_ENABLED=true

  openlab:
    build:
      context: ..
      dockerfile: docker/Dockerfile.openlab
    volumes:
      - ../openlab:/app/openlab:ro
      - openlab-data:/app/data
    ports:
      - "5173:5173"
    environment:
      - OPENLAB_DEBUG=true
```

### 调试技巧

```bash
# 进入容器shell
docker exec -it agentos-kernel-1 bash

# 查看实时日志
docker logs -f agentos-kernel-1

# 重启服务
docker restart agentos-kernel-1

# 查看资源使用
docker stats agentos-kernel-1
```

---

## 🌐 生产部署

### 使用 docker-compose.prod.yml

```bash
# 生产环境部署
docker compose -f docker/docker-compose.prod.yml up -d

# 扩展服务实例数量
docker compose -f docker/docker-compose.prod.yml scale kernel=3 daemon=2

# 更新服务（零停机）
docker compose -f docker/docker-compose.prod.yml up -d --no-deps
```

### Kubernetes部署

参考 [Kubernetes部署指南](../docs/kubernetes-deployment.md)。

### 性能优化建议

#### 1. 资源限制

```yaml
# docker-compose.prod.yml 示例
services:
  kernel:
    deploy:
      resources:
        limits:
          cpus: '4'
          memory: 4G
        reservations:
          cpus: '2'
          memory: 2G
```

#### 2. 日志管理

```yaml
logging:
  driver: json-file
  options:
    max-size: "100m"
    max-file: "3"
```

#### 3. 健康检查

```yaml
healthcheck:
  test: ["CMD", "curl", "-f", "http://localhost:8080/health"]
  interval: 30s
  timeout: 10s
  retries: 3
  start_period: 40s
```

---

## 🛡️ 安全最佳实践

### 1. 最小权限运行

```dockerfile
# Dockerfile.kernel
FROM ubuntu:22.04

# 创建非root用户
RUN groupadd -r agentos && useradd -r -g agentos agentos

# 切换到非root用户
USER agentos

# 运行应用
CMD ["./bin/agentos-kernel"]
```

### 2. 只读文件系统

```yaml
services:
  kernel:
    read_only: true
    tmpfs:
      - /tmp
      - /run
      - /var/cache
```

### 3. 禁止特权模式

```yaml
security_opt:
  - no-new-privileges:true
cap_drop:
  - ALL
cap_add:
  - NET_BIND_SERVICE
```

---

## 📊 监控和运维

### Prometheus指标暴露

AgentOS默认在 `:9090` 端口暴露Prometheus指标：

```bash
# 访问指标端点
curl http://localhost:9090/metrics
```

**关键指标**:

| 指标名 | 说明 | 类型 |
|--------|------|------|
| `agentos_ipc_requests_total` | IPC请求总数 | Counter |
| `agentos_ipc_request_duration_seconds` | IPC请求延迟 | Histogram |
| `agentos_memory_records_total` | 记忆条目数 | Gauge |
| `agentos_memory_l2_query_duration_seconds` | L2查询延迟 | Histogram |
| `agentos_cupolas_permission_checks_total` | 权限检查次数 | Counter |
| `agentos_cupolas_sanitization_errors_total` | 净化错误数 | Counter |

### Grafana仪表盘

导入我们的预配置Grafana仪表盘JSON文件：
[grafana-dashboard.json](../docs/grafana-dashboard.json)

---

## ❓ 故障排查

### 常见问题

#### Q1: 容器启动失败？

**A**: 检查以下项：
1. 端口是否被占用：`lsof -i :8080`
2. 权限问题：确保有读写权限
3. 查看日志：`docker logs <container_name>`

#### Q2: 无法访问API？

**A**: 
1. 确认容器正在运行：`docker ps`
2. 检查端口映射：`docker port <container_name>`
3. 测试内部连通性：`docker exec <container_name> curl localhost:8080/health`

#### Q3: 内存不足？

**A**: 
1. 调整内存限制：修改 `.env` 中的 `AGENTOS_MEMORY_LIMIT`
2. 清理无用数据：`docker system prune`
3. 监控资源使用：`docker stats`

#### Q4: 如何备份数据？

**A**: 
```bash
# 备份记忆数据
docker exec agentos-kernel-1 tar czf /backup/memory-l1.tar.gz /app/data/memory/l1/

# 从容器复制备份
docker cp agentos-kernel-1:/backup/memory-l1.tar.gz ./memory-backup.tar.gz
```

---

## 📞 支持和反馈

- **Docker相关问题**: lidecheng@spharx.cn
- **安全问题**: wangliren@spharx.cn
- **功能建议**: zhouzhixian@spharx.cn
- **GitCode Issue**: https://gitcode.com/spharx/agentos/issues

---

## 📜 相关文档

- [AgentOS README](../README.md)
- [部署指南](../agentos/manuals/guides/deployment.md)
- [架构设计原则](../agentos/manuals/ARCHITECTURAL_PRINCIPLES.md)
- [cupolas安全穹顶文档](../agentos/cupolas/README.md)

---

**"容器化是AgentOS走向生产就绪的关键一步。"**

> *"From data intelligence emerges."*  
> **始于数据，终于智能。**

---

© 2026 SPHARX Ltd. All Rights Reserved.
