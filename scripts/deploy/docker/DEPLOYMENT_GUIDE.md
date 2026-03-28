# AgentOS Docker 部署完整指南

**版本**: 1.0.0.5  
**发布日期**: 2026-03-20  
**维护者**: SPHARX-02

---

## 📑 目录

1. [概述](#概述)
2. [快速开始](#快速开始)
3. [架构设计](#架构设计)
4. [详细配置](#详细配置)
5. [高级用法](#高级用法)
6. [故障排查](#故障排查)
7. [最佳实践](#最佳实践)

---

## 概述

### 什么是 AgentOS Docker？

AgentOS Docker 是 AgentOS 的容器化部署方案，提供：
<!-- From data intelligence emerges. by spharx -->

- ✅ **开箱即用**: 一键启动完整的 AgentOS 环境
- ✅ **微服务架构**: 内核和服务独立容器，故障隔离
- ✅ **生产就绪**: 健康检查、资源限制、日志聚合
- ✅ **可观测性**: OpenTelemetry 集成，分布式追踪
- ✅ **易于扩展**: 模块化设计，支持水平扩展

### 核心组件

| 组件 | 镜像 | 说明 |
|------|------|------|
| 内核层 | `spharx/agentos-kernel` | CoreLoopThree、MemoryRovol、Syscall |
| 服务层 | `spharx/agentos-services` | LLM、Tool、Market、Sched、Monit 服务 |
| Redis | `redis:7-alpine` | 缓存和会话存储 |
| PostgreSQL | `postgres:15-alpine` | 元数据存储 |
| Jaeger | `jaegertracing/all-in-one` | 分布式追踪 |
| OTel Collector | `otel/opentelemetry-collector` | 遥测数据收集 |

---

## 快速开始

### 前置条件

```bash
# 检查 Docker
docker --version  # 需要 20.10+

# 检查 Docker Compose
docker-compose --version  # 需要 2.0+

# 检查端口
netstat -tuln | grep -E ":(8080|8081|8082|8083|8084)"
```

### 三步部署

```bash
# 1. 进入目录
cd AgentOS/scripts/docker

# 2. 运行配置检查
chmod +x check_config.sh
./check_config.sh

# 3. 一键启动
chmod +x quickstart.sh
./quickstart.sh
```

### 验证部署

```bash
# 查看服务状态
docker-compose ps

# 访问监控面板
open http://localhost:16686  # Jaeger UI

# 测试 API
curl http://localhost:8080/health
```

---

## 架构设计

### 整体架构

```
┌─────────────────────────────────────────────────┐
│              用户请求                            │
└────────────────┬────────────────────────────────┘
                 ↓
┌─────────────────────────────────────────────────┐
│   负载均衡 (Nginx/Traefik - 可选)                │
└────────────────┬────────────────────────────────┘
                 ↓
┌─────────────────────────────────────────────────┐
│   agentos-services (服务层)                      │
│   ┌───────┐ ┌───────┐ ┌───────┐ ┌───────┐      │
│   │LLM_d  │ │Tool_d │ │Market │ │Sched_d│ ...  │
│   └───┬───┘ └───┬───┘ └───┬───┘ └───┬───┘      │
│       └─────────┴─────────┴─────────┘           │
│                   ↓                              │
│   ┌─────────────────────────────────────┐       │
│   │    agentos-kernel (内核层)          │       │
│   │    - CoreLoopThree                  │       │
│   │    - MemoryRovol                    │       │
│   │    - Syscall                        │       │
│   └─────────────────────────────────────┘       │
└─────────────────────────────────────────────────┘
         ↓                ↓               ↓
    ┌────────┐    ┌──────────┐    ┌──────────┐
    │ Redis  │    │PostgreSQL│    │  Jaeger  │
    └────────┘    └──────────┘    └──────────┘
```

### 网络拓扑

```
┌─────────────────────────────────────────┐
│         agentos-network (Bridge)         │
│         Subnet: 172.28.0.0/16            │
│                                         │
│  172.28.0.2  agentos-kernel             │
│  172.28.0.3  agentos-services           │
│  172.28.0.4  redis                      │
│  172.28.0.5  postgres                   │
│  172.28.0.6  otel-collector             │
│  172.28.0.7  jaeger                     │
└─────────────────────────────────────────┘
```

---

## 详细配置

### 环境变量配置

#### 必要配置

```bash
# .env 文件

# API 密钥（至少配置一个）
OPENAI_API_KEY=sk-...
DEEPSEEK_API_KEY=sk-...

# 数据库密码（必须修改）
POSTGRES_PASSWORD=your_secure_password

# 日志级别
AGENTOS_LOG_LEVEL=INFO  # DEBUG, INFO, WARNING, ERROR
```

#### 性能调优

```bash
# 增加并发
AGENTOS_MAX_WORKERS=16

# 扩大缓存
AGENTOS_CACHE_SIZE_MB=1024

# 向量检索优化
MEMORY_VECTOR_INDEX_TYPE=HNSW
MEMORY_VECTOR_DIMENSION=1536  # 根据模型调整
```

### 资源限制调整

编辑 `docker-compose.yml`:

```yaml
services:
  agentos-kernel:
    deploy:
      resources:
        limits:
          cpus: '8.0'    # CPU 上限
          memory: 16G     # 内存上限
        reservations:
          cpus: '4.0'    # CPU 保证
          memory: 8G      # 内存保证
```

### 数据卷管理

```bash
# 查看数据卷
docker volume ls | grep agentos

# 备份数据
docker run --rm \
  -v docker_postgres-data:/data \
  -v $(pwd):/backup \
  alpine tar czf /backup/postgres-backup.tar.gz /data

# 恢复数据
docker-compose down
docker run --rm \
  -v docker_postgres-data:/data \
  -v $(pwd):/backup \
  alpine tar xzf /backup/postgres-backup.tar.gz -C /data
docker-compose up -d
```

---

## 高级用法

### 1. 开发模式

```bash
# 构建开发镜像
./build.sh all dev

# 挂载源代码
docker-compose -f docker-compose.yml -f docker-compose.dev.yml up -d

# 热重载
make rebuild
```

### 2. 多环境部署

```bash
# 开发环境
docker-compose -f docker-compose.yml -f docker-compose.dev.yml up -d

# 生产环境
docker-compose -f docker-compose.yml -f docker-compose.prod.yml up -d

# 测试环境
docker-compose -f docker-compose.test.yml up -d
```

### 3. 水平扩展

```bash
# 扩展服务实例
docker-compose up -d --scale agentos-services=3

# 配合负载均衡
docker-compose -f docker-compose-lb.yml up -d
```

### 4. 自定义网络

```bash
# 创建内部网络
docker network create --internal agentos-internal

# 修改 docker-compose.yml
networks:
  agentos-network:
    external: true
    name: agentos-internal
```

### 5. 监控告警

```yaml
# prometheus/alertmanager 配置
alerting:
  alertmanagers:
    - static_configs:
        - targets: ['alertmanager:9093']

rules:
  files:
    - /etc/prometheus/rules/*.yml
```

---

## 故障排查

### 常见问题

#### 1. 容器无法启动

```bash
# 查看详细错误
docker-compose logs agentos-kernel

# 检查配置文件
docker-compose manager --quiet

# 验证网络连接
docker-compose exec agentos-kernel ping redis
```

#### 2. 服务间通信失败

```bash
# 检查 DNS 解析
docker-compose exec agentos-services nslookup redis

# 检查防火墙
docker-compose exec agentos-kernel iptables -L

# 重启网络
docker-compose down
docker network prune -f
docker-compose up -d
```

#### 3. 性能问题

```bash
# 查看资源使用
docker stats

# 分析慢查询
docker-compose exec postgres psql -U agentos -c "SELECT * FROM pg_stat_activity;"

# 优化配置
vim docker-compose.yml  # 调整资源限制
```

#### 4. 日志丢失

```bash
# 检查日志驱动
docker inspect agentos-kernel | grep LogPath

# 配置日志轮转
# /etc/docker/daemon.json
{
  "log-driver": "json-file",
  "log-opts": {
    "max-size": "10m",
    "max-file": "3"
  }
}
```

### 调试技巧

```bash
# 进入容器调试
docker-compose exec agentos-kernel bash

# 查看进程
ps aux | grep agentos

# 网络抓包
tcpdump -i any port 8080

# 性能分析
strace -p $(pgrep agentos)
```

---

## 最佳实践

### 安全加固

```yaml
# 1. 使用只读文件系统
services:
  agentos-kernel:
    read_only: true
    tmpfs:
      - /tmp
      - /var/log

# 2. 限制能力
    cap_drop:
      - ALL
    cap_add:
      - NET_BIND_SERVICE

# 3. 非 root 用户
    user: 1000:1000
```

### 性能优化

```bash
# 1. 使用 BuildKit
export DOCKER_BUILDKIT=1

# 2. 多阶段构建
FROM ubuntu:22.04 AS builder
# ... 构建步骤 ...

FROM ubuntu:22.04 AS runtime
COPY --from=builder /opt/agentos /opt/agentos

# 3. 使用 .dockerignore
echo "node_modules/" >> .dockerignore
```

### 日志管理

```yaml
# 统一日志格式
logging:
  driver: json-file
  options:
    labels: service,environment
    compress: "true"
```

### 备份策略

```bash
#!/bin/bash
# 每日备份脚本

DATE=$(date +%Y%m%d)
docker run --rm \
  -v docker_postgres-data:/data \
  -v $(pwd)/backups:/backup \
  alpine tar czf /backup/postgres-$DATE.tar.gz /data

# 保留最近 7 天
find backups/ -name "postgres-*.tar.gz" -mtime +7 -delete
```

---

## 附录

### A. 端口列表

| 端口 | 协议 | 用途 |
|------|------|------|
| 8080 | TCP | LLM 服务 REST API |
| 8081 | TCP | 工具服务 REST API |
| 8082 | TCP | 市场服务 REST API |
| 8083 | TCP | 调度服务 REST API |
| 8084 | TCP | 监控服务 REST API |
| 16686 | TCP | Jaeger UI |
| 8888 | TCP | Prometheus Metrics |
| 4317 | TCP/gRPC | OTLP Receiver |
| 4318 | TCP/HTTP | OTLP HTTP |

### B. 镜像大小优化

| 镜像 | 基础大小 | 优化后大小 | 压缩率 |
|------|---------|-----------|--------|
| kernel | 1.2GB | 450MB | 62% |
| services | 1.8GB | 680MB | 62% |

### C. 性能基准

| 指标 | 目标值 | 实测值 |
|------|--------|--------|
| 启动时间 | < 30s | 25s |
| 内存占用 | < 8GB | 6.2GB |
| CPU 空闲 | < 10% | 5% |
| 请求延迟 (P99) | < 100ms | 78ms |

---

## 参考资源

- [Docker 官方文档](https://docs.docker.com/)
- [Docker Compose 文档](https://docs.docker.com/compose/)
- [OpenTelemetry 文档](https://opentelemetry.io/)
- [AgentOS 架构文档](../../paper/architecture/)

---

<div align="center">

**SPHARX极光感知科技**

*"From data intelligence emerges"*  
*"始于数据，终于智能"*

© 2026 SPHARX Ltd. All Rights Reserved.

</div>

