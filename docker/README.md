# AgentOS Docker 部署方案 - 生产级容器化架构

<p align="center">
  <strong>版本: 2.0.0 (Production-Grade) | 最后更新: 2026-04-06</strong><br>
  <em>基于 CIS Docker Benchmark 合规 | 支持高可用部署 | 内置可观测性</em>
</p>

---

## 📖 目录

- [1. 项目概述](#1-项目概述)
- [2. 系统架构](#2-系统架构)
  - [2.1 整体架构图](#21-整体架构图)
  - [2.2 服务拓扑关系](#22-服务拓扑关系)
  - [2.3 端口规划表](#23-端口规划表)
- [3. 前置要求](#3-前置要求)
- [4. 快速开始](#4-快速开始)
  - [4.1 开发环境一键启动](#41-开发环境一键启动)
  - [4.2 生产环境部署](#42-生产环境部署)
  - [4.3 验证部署状态](#43-验证部署状态)
- [5. 服务详解](#5-服务详解)
  - [5.1 Kernel 内核服务](#51-kernel-内核服务)
  - [5.2 Gateway 网关服务](#52-gateway-网关服务)
  - [5.3 PostgreSQL 数据库](#53-postgresql-数据库)
  - [5.4 Redis 缓存服务](#54-redis-缓存服务)
  - [5.5 OpenLab 应用服务](#55-openlab-应用服务)
  - [5.6 Prometheus 监控系统](#56-prometheus-监控系统)
  - [5.7 Grafana 可视化平台](#57-grafana-可视化平台)
- [6. 安全最佳实践](#6-安全最佳实践)
  - [6.1 CIS Docker Benchmark 合规](#61-cis-docker-benchmark-合规)
  - [6.2 密钥管理策略](#62-密钥管理策略)
  - [6.3 网络隔离方案](#63-网络隔离方案)
- [7. 运维手册](#7-运维手册)
  - [7.1 日常运维命令](#71-日常运维命令)
  - [7.2 日志管理策略](#72-日志管理策略)
  - [7.3 备份恢复流程](#73-备份恢复流程)
  - [7.4 性能调优建议](#74-性能调优建议)
- [8. 监控与告警](#8-监控与告警)
  - [8.1 Prometheus 指标体系](#81-prometheus-指标体系)
  - [8.2 Grafana 仪表盘](#82-grafana-仪表盘)
  - [8.3 告警规则说明](#83-告警规则说明)
- [9. 扩展指南](#9-扩展指南)
  - [9.1 水平扩展 (Horizontal Scaling)](#91-水平扩展-horizontal-scaling)
  - [9.2 Kubernetes 迁移](#92-kubernetes-迁移)
- [10. 故障排查](#10-故障排查)
  - [10.1 常见问题 FAQ](#101-常见问题-faq)
  - [10.2 故障诊断清单](#102-故障诊断清单)
- [11. 附录](#11-附录)
  - [11.1 文件结构说明](#11-文件结构说明)
  - [11.2 版本更新日志](#12-版本更新日志)

---

## 1. 项目概述

### 1.1 什么是 AgentOS Docker？

AgentOS Docker 是 **AgentOS 智能体操作系统** 的官方容器化部署方案，提供从开发环境到生产环境的**一站式容器编排解决方案**。

### 核心特性

| 特性类别 | 具体能力 | 实现方式 |
|---------|---------|---------|
| 🔒 **安全加固** | CIS Benchmark 合规、非 root 用户、seccomp、只读文件系统 | Dockerfile 安全配置 + Compose security_opt |
| 🛡️ **高可用** | 多实例部署、健康检查、自动重启、优雅降级 | restart_policy + healthcheck + replicas |
| 📊 **可观测性** | Prometheus 指标采集 + Grafana 可视化 + 结构化日志 | monitoring/ 目录 + JSON 日志驱动 |
| ⚡ **性能优化** | 连接池调优、内存管理、缓存策略、资源限制 | deploy.resources + 数据库参数调优 |
| 🔄 **数据安全** | ACID 事务保证、AOF/RDB 持久化、自动化备份 | PostgreSQL WAL + Redis AOF + backup.sh 脚本 |
| 🚀 **快速部署** | 一键启动、环境变量模板、多目标构建 | docker-compose.yml + .env.production.example |

### 技术栈

```
┌─────────────────────────────────────────────────────┐
│                  应用层 (Application)                │
│   OpenLab Web UI │ Python SDK v3.0.0 │ Go SDK       │
├─────────────────────────────────────────────────────┤
│                  网关层 (Gateway)                     │
│   HTTP REST │ WebSocket │ stdio (Port: 18789)      │
├─────────────────────────────────────────────────────┤
│                  核心层 (Kernel)                      │
│   Microkernel IPC │ CoreLoopThree │ MemoryRovol     │
│   Cupolas Security │ HeapStore (Port: 18080)        │
├─────────────────────────────────────────────────────┤
│                  数据层 (Data)                       │
│   PostgreSQL 15 │ Redis 7 │ HeapStore 6 Partitions  │
├─────────────────────────────────────────────────────┤
│               基础设施层 (Infrastructure)             │
│   Docker Engine │ Prometheus │ Grafana │ AlertManager│
└─────────────────────────────────────────────────────┘
```

### 关键版本信息

| 组件 | 版本 | 端口 | 说明 |
|------|------|------|------|
| AgentOS Kernel | v2.0.0 | 18080 (IPC) / 9090 (Metrics) | 微内核核心 |
| AgentOS Gateway | v2.0.0 | 18789 (API) / 18790 (Admin) | 三协议网关 |
| Python SDK | v3.0.0 | 默认连接 18789 | `agentos` Python 包 |
| PostgreSQL | 15-alpine | 5432 (内网) | 关系型数据库 |
| Redis | 7-alpine | 6379 (内网) | 内存缓存 |
| Prometheus | v2.45.0 | 9091 (偏移) | 监控引擎 |
| Grafana | 10.2.0 | 3000 | 可视化平台 |

---

## 2. 系统架构

### 2.1 整体架构图

#### 开发环境拓扑 (Development)

```
                         ┌──────────────────────────────────┐
                         │        开发者机器 (Developer)     │
                         └─────────────┬────────────────────┘
                                       │ HTTP/WS :18789
                                       ▼
┌──────────────────────────────────────────────────────────────────────┐
│                        agentos-frontend (172.29.0.0/16)              │
│  ┌─────────────┐    ┌──────────────┐    ┌─────────────────────────┐ │
│  │   OpenLab   │    │   Gateway    │    │       Grafana          │ │
│  │  :5173/:8000│◄──►│  :18789/:18790│◄──►│         :3000          │ │
│  │  [optional] │    │  (统一入口)   │    │    [monitoring]        │ │
│  └─────────────┘    └──────┬───────┘    └─────────────────────────┘ │
│                             │                                        │
└─────────────────────────────┼────────────────────────────────────────┘
                              │
                              ▼
┌──────────────────────────────────────────────────────────────────────┐
│                        agentos-backend (172.28.0.0/16)               │
│  ┌─────────────┐    ┌──────────┐    ┌──────────┐    ┌──────────────┐ │
│  │   Kernel    │    │PostgreSQL│    │  Redis   │    │  Prometheus  │ │
│  │:18080/:9090 │◄──►│  :5432   │◄──►│  :6379   │◄──►│    :9091      │ │
│  │(微内核核心) │    │ (ACID)   │    │ (LRU)    │    │[monitoring]  │ │
│  └─────────────┘    └──────────┘    └──────────┘    └──────────────┘ │
│                                                                      │
│  HeapStore 六大数据分区 (命名卷):                                      │
│  ├── heapstore-data (PATH_KERNEL)                                     │
│  ├── heapstore-logs (PATH_LOGS)                                       │
│  ├── heapstore-registry (PATH_REGISTRY)                               │
│  ├── heapstore-traces (PATH_TRACES)                                   │
│  ├── heapstore-ipc (PATH_KERNEL_IPC)                                  │
│  └── heapstore-memory (PATH_KERNEL_MEMORY)                             │
└──────────────────────────────────────────────────────────────────────┘
```

#### 生产环境拓扑 (Production)

```
                    Internet / CDN
                         │ HTTPS :443
                         ▼
              ┌─────────────────────┐
              │   Load Balancer     │
              │  (Nginx/HAProxy)    │
              └──────────┬──────────┘
                         │ HTTP :18789
                         ▼
┌─────────────────────────────────────────────────────────────────────┐
│                   DMZ / Frontend Network (172.31.0.0/16)            │
│                                                                     │
│  ┌─────────────────────┐    ┌──────────────────────────────────┐    │
│  │   Gateway Cluster    │    │        Grafana                  │    │
│  │   :18789 (LB)        │◄──►│        :3000 (VPN Only)         │    │
│  │   [N instances]      │    │        [monitoring profile]     │    │
│  └──────────┬──────────┘    └──────────────────────────────────┘    │
│             │                                                      │
└─────────────┼──────────────────────────────────────────────────────┘
              │
              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                   Internal / Backend Network (172.30.0.0/16)        │
│                                                                     │
│  ┌──────────────┐    ┌────────────┐    ┌────────────────────────┐   │
│  │Kernel Cluster │    │ PostgreSQL │    │ Redis Cluster          │   │
│  │:18080 (内部)  │◄──►│  HA (主从)  │◄──►│ Sentinel (高可用)      │   │
│  │[M instances] │    │  :5432     │    │ :6379                 │   │
│  └──────────────┘    └────────────┘    └────────────────────────┘   │
│                                                                     │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │                    Monitoring Stack                           │   │
│  │  Prometheus (:9091) → AlertManager → PagerDuty/Slack/Email   │   │
│  └──────────────────────────────────────────────────────────────┘   │
│                                                                     │
│  🔒 安全加固:                                                       │
│  ├── 只读文件系统 (read_only: true)                                  │
│  ├── seccomp 配置文件限制危险系统调用                                 │
│  ├── 非 root 用户运行 (agentos:1000)                                │
│  ├── 能力白名单 (仅 NET_BIND_SERVICE)                               │
│  └── no-new-privileges:true                                         │
└─────────────────────────────────────────────────────────────────────┘
```

### 2.2 服务拓扑关系

```
依赖关系图 (Dependency Graph):

OpenLab (可选)
    │
    ▼
Gateway (必需) ◄──── 外部客户端 / Python SDK v3.0.0
    │
    ├──────────────────┐
    ▼                  ▼
Kernel (核心)      Redis (缓存)
    │                  │
    ▼                  ▼
PostgreSQL (DB)   [Session Store]
    │
    ▼
HeapStore (6 Partitions)
```

### 2.3 端口规划表

#### 对外暴露端口 (External Ports)

| 端口 | 服务 | 协议 | 环境 | 用途 | 安全级别 |
|------|------|------|------|------|---------|
| **18789** | Gateway | HTTP/WS | Dev + Prod | **统一 API 入口** (Python SDK 默认) | 公开 |
| **18790** | Gateway Admin | HTTP | Dev + Prod | 网关管理 API (限制 IP) | 受限 |
| 5173 | OpenLab Web | HTTP | Dev only | Vite 开发服务器 | 仅开发 |
| 8000 | OpenLab API | HTTP | Dev only | 后端 API | 仅开发 |
| 3000 | Grafana | HTTP | Dev + Prod | 监控可视化 (生产需 VPN) | 受限 |

#### 内部通信端口 (Internal Ports - 不对外暴露)

| 端口 | 服务 | 协议 | 用途 |
|------|------|------|------|
| **18080** | Kernel IPC | HTTP | 内核 Syscall API (仅 Gateway 访问) |
| **9090** | Kernel Metrics | HTTP | Prometheus /metrics 端点 |
| 5432 | PostgreSQL | TCP | 数据库连接 (仅后端网络) |
| 6379 | Redis | TCP | 缓存连接 (仅后端网络) |
| 9091 | Prometheus | HTTP | 监控 UI (生产仅本地回环) |

---

## 3. 前置要求

### 3.1 系统要求

| 组件 | 最低配置 | 推荐配置 (开发) | 推荐配置 (生产) |
|------|---------|----------------|----------------|
| **操作系统** | Ubuntu 20.04+ / CentOS 8+ | Ubuntu 22.04 LTS | Ubuntu 22.04 LTS ( hardened ) |
| **CPU** | 4 核心 | 8 核心 | 16+ 核心 (独立 Kernel/Gateway) |
| **内存** | 8 GB RAM | 16 GB RAM | 32+ GB RAM |
| **磁盘** | 50 GB SSD | 100 GB NVMe SSD | 500GB+ SSD (分离 DB/IOPS) |
| **Docker** | ≥ 20.10.0 | ≥ 24.0.0 | ≥ 24.0.0 (最新稳定版) |
| **Docker Compose** | ≥ 2.0.0 (V2 插件) | ≥ 2.20.0 | ≥ 2.20.0 |

### 3.2 软件依赖

```bash
# 必需软件
✅ Docker Engine (≥ 20.10)
✅ Docker Compose Plugin (≥ 2.0, 推荐 V2)
✅ Git (用于克隆仓库)
✅ curl/wget (健康检查用)

# 可选但推荐
⚪ jq (JSON 处理, 脚本依赖)
⚪ openssl (密钥生成)
⚪ make (构建辅助)

# 安装示例 (Ubuntu):
sudo apt-get update && sudo apt-get install -y \
    docker.io docker-compose-v2 git curl jq openssl make
```

### 3.3 权限要求

```bash
# 将用户添加到 docker 组 (避免 sudo)
sudo usermod -aG docker $USER
newgrp docker

# 验证安装
docker --version          # 应输出: Docker version 24.x.x
docker compose version     # 应输出: Docker Compose version v2.x.x
```

---

## 4. 快速开始

### 4.1 开发环境一键启动

```bash
# ============================================================
# 步骤 1: 克隆仓库
# ============================================================
git clone https://gitcode.com/spharx/agentos.git
cd agentos

# ============================================================
# 步骤 2: 启动核心服务 (Kernel + Gateway + DB + Cache)
# ============================================================
docker compose -f docker/docker-compose.yml up -d

# 输出示例:
# ✔ Container agentos-postgres-dev   Started
# ✔ Container agentos-redis-dev      Started
# ✔ Container agentos-kernel-dev     Started (healthy)
# ✔ Container agentos-gateway-dev    Started (healthy)

# ============================================================
# 步骤 3: 启动监控栈 (可选, 但推荐)
# ============================================================
docker compose -f docker/docker-compose.yml --profile monitoring up -d

# 输出示例:
# ✔ Container agentos-prometheus-dev  Started
# ✔ Container agentos-grafana-dev     Started

# ============================================================
# 步骤 4: 验证服务状态
# ============================================================
docker compose -f docker/docker-compose.yml ps

# 预期输出:
# NAME                    STATUS                   PORTS
# agentos-gateway-dev     running (healthy)       0.0.0.0:18789->18789/tcp, 0.0.0.0:18790->18790/tcp
# agentos-kernel-dev      running (healthy)       0.0.0.0:18080->18080/tcp, 0.0.0.0:9090->9090/tcp
# agentos-postgres-dev    running (healthy)       0.0.0.0:5432->5432/tcp
# agentos-redis-dev       running (healthy)       0.0.0.0:6379->6379/tcp
```

### 4.2 生产环境部署

```bash
# ============================================================
# ⚠️ 重要: 生产部署前必须完成以下步骤!
# ============================================================

# 步骤 1: 创建生产环境变量文件
cp docker/.env.production.example docker/.env.production
vim docker/.env.production  # 编辑所有密码和密钥!

# 步骤 2: 生成强随机密码 (使用提供的命令)
# PostgreSQL 密码:
openssl rand -base64 32 | tr -d '\n' | head -c 32; echo

# Redis 密码:
openssl rand -hex 32

# JWT Secret:
openssl rand -base64 48 | tr -d '\n=' | head -c 43; echo

# OpenLab Secret Key:
python3 -c "import secrets; print(secrets.token_urlsafe(50))"

# 步骤 3: 启动生产环境
docker compose -f docker/docker-compose.prod.yml \
    --env-file docker/.env.production \
    up -d

# 步骤 4: 验证健康状态
docker/scripts/healthcheck.sh --env prod --json

# 步骤 5: (可选) 创建首次备份
docker/scripts/backup.sh backup --env prod
```

### 4.3 验证部署状态

#### 方式一：使用健康检查脚本 (推荐)

```bash
# 开发环境检查
./docker/scripts/healthcheck.sh

# 生产环境检查 (JSON 格式, 便于集成 CI/CD)
./docker/scripts/healthcheck.sh --env prod --json

# 详细模式 (含资源使用情况)
./docker/scripts/healthcheck.sh --verbose
```

**预期输出示例 (text 模式):**

```
==========================================
 AgentOS Health Check Report v2.0.0
 Environment: prod
 Timestamp: 2026-04-06 09:30:00 CST
==========================================

[INFO] Checking Docker daemon...
[✓] Docker daemon is running

[INFO] Checking container status...
[✓] agentos-gateway-prod: Up (healthy)
[✓] agentos-kernel-prod: Up (healthy)
[✓] agentos-postgres-prod: Up (healthy)
[✓] agentos-redis-prod: Up (healthy)

[INFO] Checking HTTP health endpoints...
[✓] Gateway (http://localhost:18789/api/v1/health): HTTP 200

[INFO] Checking port connectivity...
[✓] Gateway API :18789 is OPEN
[✓] Gateway Admin :18790 is OPEN

==========================================
 Summary
==========================================
  Total Services:    4
  Healthy:           4
  Unhealthy:         0
  Critical:          0

  Overall Status:    ✓ HEALTHY
==========================================
```

#### 方式二：手动验证

```bash
# 1. 检查 Gateway 是否响应
curl http://localhost:18789/api/v1/health
# 预期: {"status":"healthy","timestamp":"..."}

# 2. 检查 Kernel IPC (仅开发环境)
curl http://localhost:18080/api/v1/health
# 预期: {"status":"ok","version":"2.0.0","ipc_ready":true}

# 3. 测试 Python SDK 连接
python3 -c "
from agentos import Client
client = Client(endpoint='http://localhost:18789')
print(client.health_check())
"

# 4. 检查数据库连接
docker compose -f docker/docker-compose.yml exec postgres pg_isready -U agentos
# 预期: postgres:5432 - accepting connections

# 5. 检查 Redis
docker compose -f docker/docker-compose.yml exec redis redis-cli ping
# 预期: PONG
```

---

## 5. 服务详解

### 5.1 Kernel 内核服务

**镜像**: `spharx/agentos-kernel:2.0.0`  
**Dockerfile**: [Dockerfile.kernel](./Dockerfile.kernel) (target: `runtime`)  
**源码位置**: [agentos/atoms/corekern/](../agentos/atoms/corekern/)  
**端口**: 18080 (IPC API) + 9090 (Prometheus Metrics)

#### 功能定位

Kernel 是 AgentOS 的**微内核核心**，实现四大原子机制：

| 机制 | 英文 | 功能描述 |
|------|------|---------|
| **IPC** | Inter-Process Communication | 进程间通信，统一 Syscall 接口 |
| **Memory** | Memory Management | 内存分配与回收，虚拟内存管理 |
| **Task** | Task Scheduling | 任务调度，优先级队列，时间片轮转 |
| **Time** | Time Management | 定时器，时钟同步，超时机制 |

#### 多阶段构建说明

```
Builder (编译) → Runtime (运行时) → Debug (调试)
     ↓                ↓                    ↓
  gcc/cmake      最小依赖              gdb/strace
  ninja build    非root用户             调试符号
  ctest测试      只读文件系统           开发工具
```

#### 关键配置项

```yaml
# docker-compose.yml 中的关键配置
environment:
  LOG_LEVEL: INFO                          # 生产使用 INFO
  AGENTOS_PERMISSION_MODE: strict          # 严格权限控制
  HEAPSTORE_CIRCUIT_THRESHOLD: 10          # 熔断阈值
  MALLOC_ARENA_MAX: 2G                     # 内存优化

deploy:
  resources:
    limits:
      cpus: '4'                            # 最大 CPU
      memory: 4G                           # OOM 上限
    reservations:
      cpus: '2'                            # 保证 CPU
      memory: 2G                           # 保证内存
```

#### 健康检查端点

```bash
# HTTP 健康检查
GET /api/v1/health
Response: {"status": "ok", "version": "2.0.0", "uptime": 3600}

# Prometheus 指标
GET /metrics
Metrics: agentos_syscall_duration_seconds, agentos_ipc_active_connections, ...
```

---

### 5.2 Gateway 网关服务

**镜像**: `spharx/agentos-gateway:2.0.0`  
**Dockerfile**: [Dockerfile.daemon](./Dockerfile.daemon) (target: `gateway`)  
**源码位置**: [agentos/gateway/](../agentos/gateway/)  
**端口**: 18789 (API) + 18790 (Admin)

#### 功能定位

Gateway 是**唯一对外入口**，实现三协议网关：

| 协议 | 端点路径 | 用途 | 适用场景 |
|------|---------|------|---------|
| **HTTP REST** | `/api/v1/*` | 同步请求-响应 | 传统 Web/API 客户端 |
| **WebSocket** | `/ws` | 全双工实时通信 | 实时推送、流式数据 |
| **stdio** | stdin/stdout | 管道通信 | CLI 工具、子进程嵌入 |

#### 核心能力

```
┌─────────────────────────────────────────────────────┐
│                   Gateway 架构                       │
│                                                     │
│  [Client]                                           │
│      │                                              │
│      ▼                                              │
│  ┌─────────┐    ┌─────────────┐    ┌──────────────┐ │
│  │ Rate    │──►│ Auth/JWT    │──►│ Route/Syscall │ │
│  │ Limiter │    │ Validator   │    │ Router       │ │
│  └─────────┘    └─────────────┘    └──────┬───────┘ │
│                                          │          │
│  ┌─────────┐    ┌─────────────┐         ▼          │
│  │ CORS    │◄──│ Error       │    ┌──────────┐    │
│  │ Handler │    │ Handler     │    │ Upstream │    │
│  └─────────┘    └─────────────┘    │ Pool     │    │
│                                     │ (Kernel) │    │
│  ┌─────────┐                         └──────────┘    │
│  │ Metrics │◄──────────────────────────────────────│
│  │ Exporter │    Prometheus /metrics                │
│  └─────────┘                                         │
└─────────────────────────────────────────────────────┘
```

#### 配置文件

详细配置见 [gateway.yaml](./config/gateway.yaml)，主要配置项：

```yaml
gateway:
  protocols:
    http:
      cors:
        allowed_origins: ["http://localhost:5173"]
    websocket:
      max_connections: 1000
  
  rate_limiting:
    global_qps: 5000          # 生产环境 QPS 上限
    per_ip_qps: 50             # 单 IP 限制
  
  upstream:
    endpoint: "http://kernel:18080"
    pool_size: 10              # 连接池大小
  
  security:
    auth:
      jwt_secret: ${GATEWAY_JWT_SECRET}
      token_expiry_hours: 8
```

---

### 5.3 PostgreSQL 数据库

**镜像**: `postgres:15-alpine`  
**端口**: 5432 (仅内网访问)  
**用途**: HeapStore 慢速存储层、事务性数据、复杂查询

#### 生产级调优参数

```yaml
command:
  - "postgres"
  - "-c", "max_connections=200"           # 连接池大小
  - "-c", "shared_buffers=256MB"           # 共享缓冲区 (建议: 25% RAM)
  - "-c", "effective_cache_size=1GB"       # 有效缓存 (建议: 75% RAM)
  - "-c", "work_mem=4MB"                   # 排序/哈希内存
  - "-c", "maintenance_work_mem=64MB"      # 维护操作内存
  - "-c", "wal_level=replica"             # 支持 PITR 和复制
  - "-c", "log_min_duration_statement=1000" # 慢查询阈值 (1秒)
```

#### 数据持久化

```yaml
volumes:
  - postgres-prod-data:/var/lib/postgresql/data  # PGDATA 目录
  # 可选: 绑定挂载到高性能磁盘
  # driver_opts:
  #   type: none
  #   o: bind
  #   device: /data/ssd/postgres
```

---

### 5.4 Redis 缓存服务

**镜像**: `redis:7-alpine`  
**端口**: 6379 (仅内网访问)  
**用途**: IPC 高速通道、权限缓存、会话存储、Rate Limiting 计数器

#### 生产级配置

```yaml
command: >
  redis-server
  --appendonly yes                          # AOF 持久化开启
  --appendfsync everysec                    # 每秒同步 (性能/安全平衡)
  --maxmemory 1024mb                        # 最大内存 1GB
  --maxmemory-policy allkeys-lru           # LRU 淘汰策略
  --requirepass ${REDIS_PASSWORD}           # 🔐 密码认证
  --lazyfree-lazy-eviction yes              # 惰性删除 (避免阻塞)
```

#### 使用场景

| 场景 | Key 模式 | TTL | 数据类型 |
|------|---------|-----|---------|
| 会话存储 | `session:{user_id}` | 8h | Hash |
| 权限缓存 | `perm:{role}:{resource}` | 1h | String |
| Rate Limiting | `ratelimit:{ip}:{endpoint}` | 60s | Counter (INCR + EXPIRE) |
| IPC 缓存 | `ipc:cache:{syscall_id}` | 5min | JSON String |
| 发布订阅 | `channel:*` | N/A | Pub/Sub |

---

### 5.5 OpenLab 应用服务 (可选)

**镜像**: `spharx/agentos-openlab:2.0.0`  
**端口**: 5173 (Web) + 8000 (API)  
**启动**: `--profile openlab` (按需启动)

#### 功能

- **Web UI**: AgentOS 可视化管理界面
- **交互式实验平台**: 在线调试和测试 AgentOS 功能
- **API Backend**: 提供 RESTful API 给前端调用

#### 连接配置

```yaml
environment:
  # 通过 Gateway 访问后端 (推荐)
  AGENTOS_GATEWAY_URL: http://gateway:18789
  # 直接连接 Kernel (备用, 低延迟)
  AGENTOS_IPC_URL: http://kernel:18080
```

---

### 5.6 Prometheus 监控系统

**镜像**: `prom/prometheus:v2.45.0`  
**端口**: 9091 (开发) / 127.0.0.1:9091 (生产)  
**启动**: `--profile monitoring`

#### 监控架构

```
Prometheus (每15s拉取一次)
    │
    ├──► Kernel (:9090/metrics)
    │     └── agentos_syscall_duration_seconds
    │     └── agentos_ipc_active_connections
    │     └── process_cpu_seconds_total
    │
    ├──► Gateway (:18789/metrics)
    │     └── http_request_duration_seconds
    │     └── gateway_rate_limited_requests_total
    │
    ├──► PostgreSQL Exporter (:9187)
    │     └── pg_stat_activity_count
    │     └── pg_stat_statements_total_exec_time
    │
    └──► Redis Exporter (:9121)
          └── redis_memory_used_bytes
          └── redis_keyspace_hits_total
```

#### TSDB 配置

```yaml
command:
  - '--tsdb.retention.time=30d'            # 保留 30 天数据
  - '--storage.tsdb.retention.size=20GB'    # 最大 20GB
  - '--query.timeout=2m'                   # 查询超时
```

#### 告警规则

完整的告警规则定义在 [monitoring/rules/agentos_alerts.yml](./monitoring/rules/agentos_alerts.yml)，包含：

| 告警名称 | 级别 | 条件 | 说明 |
|---------|------|------|------|
| `KernelServiceDown` | critical | up == 0 (1m) | 内核宕机 |
| `GatewayHighLatency` | warning | P99 > 2s (5m) | 网关延迟过高 |
| `PostgresConnectionPoolExhausted` | critical | 连接数 > 90% (2m) | 数据库连接池耗尽 |
| `RedisLowHitRate` | warning | 命中率 < 70% (15m) | 缓存未命中过多 |
| `ContainerOOMKilled` | critical | OOMKilled (即时) | 容器被 OOM Kill |

---

### 5.7 Grafana 可视化平台

**镜像**: `grafana/grafana:10.2.0`  
**端口**: 3000  
**启动**: `--profile monitoring`

#### 预置仪表盘

通过 [monitoring/grafana/provisioning/](./monitoring/grafana/provisioning/) 自动配置：

| 仪表盘名称 | 用途 | 关键面板 |
|-----------|------|---------|
| **AgentOS Overview** | 系统总览 | QPS、错误率、延迟分布、活跃连接 |
| **Kernel Performance** | 内核性能 | Syscall 延迟、IPC 吞吐量、内存使用 |
| **Gateway Analytics** | 网关分析 | 请求路由、速率限制、JWT 鉴权统计 |
| **Database Insights** | 数据库洞察 | 连接池、慢查询、锁等待、复制延迟 |
| **Redis Dashboard** | Redis 面板 | 内存使用、命中率、键空间、淘汰统计 |

#### 数据源配置

自动配置 Prometheus 数据源：
```yaml
# monitoring/grafana/provisioning/datasources/prometheus.yml
datasources:
  - name: Prometheus
    type: prometheus
    url: http://prometheus:9090
    access: proxy
    isDefault: true
```

---

## 6. 安全最佳实践

### 6.1 CIS Docker Benchmark 合规清单

本方案遵循 **CIS Docker Benchmark 1.5.0** 标准，以下是关键安全措施：

| CIS 编号 | 要求 | 实现方式 | 状态 |
|---------|------|---------|------|
| **4.1** | 使用受信任的基础镜像 | ubuntu:22.04 (官方) | ✅ |
| **4.6** | 添加 HEALTHCHECK | 所有服务均有 healthcheck | ✅ |
| **5.4** | 使用 rootless 容器 | USER agentos:1000 | ✅ |
| **5.7** | 配置 seccomp | seccomp-profile.json | ✅ |
| **5.9** | 只读文件系统 | read_only: true (prod) | ✅ |
| **5.10** | 禁止 suid/sgid | cap_drop: ALL | ✅ |
| **5.11** | 禁止获取新权限 | no-new-privileges:true | ✅ |
| **5.26** | 限制容器能力 | cap_add: NET_BIND_SERVICE | ✅ |
| **5.29** | 日志驱动配置 | json-file + max-size | ✅ |

#### 安全配置示例 (Production)

```yaml
# docker-compose.prod.yml 中 Kernel 服务的安全配置
kernel:
  security_opt:
    - no-new-privileges:true              # CIS 5.11
    - seccomp:./config/seccomp-profile.json  # CIS 5.7
  read_only: true                         # CIS 5.9
  tmpfs:
    - /tmp:size=512M,mode=1777            # 可写临时目录
  cap_drop:
    - ALL                                 # CIS 5.10
  cap_add:
    - NET_BIND_SERVICE                    # 仅保留绑定端口能力
```

### 6.2 密钥管理策略

#### 原则

1. **绝不硬编码**: 所有敏感信息通过环境变量注入
2. **强随机性**: 使用密码学安全的随机数生成器
3. **定期轮换**: 建议 90 天轮换一次密钥
4. **最小权限**: 每个服务使用独立的凭据
5. **审计追踪**: 记录所有密钥变更历史

#### 密钥生成命令

```bash
# PostgreSQL 密码 (32字符 alphanumeric + special)
POSTGRES_PW=$(openssl rand -base64 32 | tr -d '\n' | head -c 32; echo)
echo "POSTGRES_PASSWORD=$POSTGRES_PW"

# Redis 密码 (64字符 hex)
REDIS_PW=$(openssl rand -hex 32)
echo "REDIS_PASSWORD=$REDIS_PW"

# JWT Secret (256位 base64url, 至少 43 字符)
JWT_SECRET=$(openssl rand -base64 48 | tr -d '\n=' | head -c 43; echo)
echo "GATEWAY_JWT_SECRET=$JWT_SECRET"

# OpenLab Secret Key (URL-safe base64, 50 字符)
OPENLAB_KEY=$(python3 -c "import secrets; print(secrets.token_urlsafe(50))")
echo "OPENLAB_SECRET_KEY=$OPENLAB_KEY"
```

#### .env 文件保护

```bash
# 设置文件权限 (仅所有者可读写)
chmod 600 docker/.env.production

# 添加到 .gitignore (防止意外提交)
echo ".env.production" >> .gitignore
echo ".env.*.local" >> .gitignore

# 验证不被跟踪
git status docker/.env.production  # 应显示为 untracked
```

### 6.3 网络隔离方案

#### 双层网络架构

```
外部流量 (Internet)
    │
    ▼
┌─────────────────────────────────────────────────┐
│  agentos-frontend-prod (172.31.0.0/16)          │
│                                                 │
│  ✓ 可访问: Gateway, Grafana, OpenLab            │
│  ✗ 无法访问: Kernel, PostgreSQL, Redis          │
│                                                 │
│  防火墙规则:                                    │
│  - 仅允许 :18789 (HTTPS) 从公网进入             │
│  - :3000 仅允许 VPN IP 访问                     │
│  - :18790 仅允许管理子网访问                     │
└─────────────────────┬───────────────────────────┘
                      │ (受控的跨网络访问)
                      ▼
┌─────────────────────────────────────────────────┐
│  agentos-backend-prod (172.30.0.0/16)           │
│                                                 │
│  ✓ 可访问: Kernel, PostgreSQL, Redis, Prom      │
│  ✗ 无法访问: 外部网络                            │
│                                                 │
│  安全措施:                                      │
│  - 无公网 IP 映射                               │
│  - 服务间通过 DNS 名称通信                      │
│  - 网络策略限制横向移动                          │
└─────────────────────────────────────────────────┘
```

#### iptables 示例 (生产防火墙)

```bash
# 允许 Gateway 端口从公网访问
iptables -A INPUT -p tcp --dport 18789 -j ACCEPT

# 仅允许 VPN 子网访问 Grafana
iptables -A INPUT -p tcp --dport 3000 -s 10.0.0.0/8 -j ACCEPT

# 仅允许管理子网访问 Gateway Admin
iptables -A INPUT -p tcp --dport 18790 -s 192.168.100.0/24 -j ACCEPT

# 拒绝其他所有入站连接 (默认策略)
iptables -P INPUT DROP
```

---

## 7. 运维手册

### 7.1 日常运维命令

#### 服务管理

```bash
# ========================================
# 启动和停止
# ========================================

# 启动所有服务
docker compose -f docker/docker-compose.yml up -d

# 停止所有服务 (优雅关闭, 等待 60 秒)
docker compose -f docker/docker-compose.yml down --timeout 60

# 重启单个服务
docker compose -f docker/docker-compose.yml restart kernel

# 强制重建并重启 (代码更新后)
docker compose -f docker/docker-compose.yml up -d --build --force-recreate

# ========================================
# 查看状态和日志
# ========================================

# 查看所有服务状态
docker compose -f docker/docker-compose.yml ps

# 实时查看日志 (跟随输出)
docker compose -f docker/docker-compose.yml logs -f gateway kernel

# 查看最近 100 行日志
docker compose -f docker/docker-compose.yml logs --tail 100 postgres

# 查看特定时间范围的日志
docker compose -f docker/docker-compose.yml logs --since "2026-04-06T08:00:00" redis

# ========================================
# 进入容器调试
# ========================================

# 进入 Kernel 容器 (注意: 生产环境是 non-root 用户, 可能受限)
docker compose -f docker/docker-compose.yml exec kernel bash

# 以 root 身份进入 (仅 debug target)
docker compose -f docker/docker-compose.yml run --rm -u root kernel-debug bash

# 执行单条命令
docker compose -f docker/docker-compose.yml exec redis redis-cli info server
```

#### 扩展和缩容

```bash
# ========================================
# 水平扩展 (需要前置负载均衡器)
# ========================================

# 扩展 Kernel 到 3 个实例
docker compose -f docker/docker-compose.prod.yml \
    --env-file .env.production \
    up -d --scale kernel=3

# 扩展 Gateway 到 2 个实例
docker compose -f docker/docker-compose.prod.yml \
    --env-file .env.production \
    up -d --scale gateway=2

# 查看当前实例数
docker compose -f docker/docker-compose.prod.yml ps | grep kernel
```

#### 资源监控

```bash
# ========================================
# 实时资源使用情况
# ========================================

# 查看所有容器资源占用
docker stats --no-stream

# 持续监控 (每秒刷新)
docker stats

# 查看单个容器详细信息
docker inspect agentos-kernel-prod | jq '.[0] | {State: .State, HostConfig: .HostConfig}'

# 查看 Docker 磁盘使用
docker system df
```

### 7.2 日志管理策略

#### 日志格式 (JSON Structured Logging)

所有服务使用 **JSON 格式日志**，便于集中收集和分析：

```json
{
  "timestamp": "2026-04-06T09:30:00.123Z",
  "level": "INFO",
  "service": "agentos-gateway",
  "message": "Request completed",
  "request_id": "abc123",
  "method": "POST",
  "path": "/api/v1/syscall/invoke",
  "status_code": 200,
  "duration_ms": 45,
  "client_ip": "192.168.1.100"
}
```

#### 日志轮转配置

```yaml
# docker-compose 中的全局日志配置
logging:
  driver: json-file
  options:
    max-size: "100m"          # 单文件最大 100MB
    max-file: "5"             # 最多保留 5 个文件 (共 500MB)
    labels: "service,environment"
    tag: "{{.Name}}"          # 容器名作为 tag
```

#### 日志收集方案 (推荐)

**方案一: Docker Log Driver → ELK Stack**

```yaml
# 修改 logging driver 为 fluentd/fluent-bit
logging:
  driver: fluentd
  options:
    fluentd-address: "localhost:24224"
    tag: "agentos.{{.Name}}"
```

**方案二: Sidecar Container → Filebeat**

```yaml
# 在每个服务旁添加 filebeat sidecar
services:
  kernel:
    # ... 其他配置 ...
    volumes:
      - /var/lib/docker/containers:/var/lib/docker/containers:ro

  filebeat:
    image: elastic/filebeat:8.11.0
    volumes:
      - ./config/filebeat.yml:/usr/share/filebeat/filebeat.yml:ro
      - /var/lib/docker/containers:/var/lib/docker/containers:ro
      - /var/run/docker.sock:/var/run/docker.sock:ro
```

### 7.3 备份恢复流程

#### 自动化备份脚本

使用 [scripts/backup.sh](./scripts/backup.sh) 进行备份管理：

```bash
# ========================================
# 备份操作
# ========================================

# 完整备份 (数据库 + 数据卷 + 配置)
./docker/scripts/backup.sh backup --env prod

# 仅备份数据库
./docker/scripts/backup.sh backup --env prod --target db-only

# 仅备份数据卷 (HeapStore)
./docker/scripts/backup.sh backup --env prod --target data-only

# 加密备份 (使用 GPG)
./docker/scripts/backup.sh backup --env prod --encrypt

# ========================================
# 列出和管理备份
# ========================================

# 列出所有备份
./docker/scripts/backup.sh list

# 清理 7 天前的备份
./docker/scripts/backup.sh clean --retain 7

# 验证备份完整性
./docker/scripts/backup.sh verify /data/backups/agentos/agentos_prod_20260406_120000_full.tar.gz
```

#### 手动恢复步骤

```bash
# ========================================
# 从备份恢复 (灾难恢复场景)
# ========================================

# 1️⃣ 停止所有服务
docker compose -f docker/docker-compose.prod.yml down

# 2️⃣ 验证备份文件
./docker/scripts/backup.sh verify <BACKUP_FILE>

# 3️⃣ 执行恢复 (自动停止服务并恢复)
./docker/scripts/backup.sh restore <BACKUP_FILE> --env prod

# 4️⃣ 重启服务
docker compose -f docker/docker-compose.prod.yml \
    --env-file .env.production \
    up -d

# 5️⃣ 验证恢复结果
./docker/scripts/healthcheck.sh --env prod --json

# 6️⃣ (可选) 通知团队恢复完成
echo "✅ Disaster recovery completed at $(date)" | mail -s "AgentOS Recovery" ops@spharx.cn
```

#### 备份内容清单

| 备份项目 | 内容 | 格式 | 大小估算 |
|---------|------|------|---------|
| **PostgreSQL** | 全库 SQL dump | `.sql.gz` (gzip) | ~100MB - 1GB |
| **Redis** | RDB 二进制快照 | `.rdb` | ~50MB - 500MB |
| **HeapStore Data** | PATH_KERNEL 分区 | `.tar.gz` | ~1GB - 10GB |
| **HeapStore Logs** | PATH_LOGS 分区 | `.tar.gz` | ~500MB - 5GB |
| **HeapStore Registry** | PATH_REGISTRY 分区 | `.tar.gz` | ~100MB |
| **HeapStore Traces** | PATH_TRACES 分区 | `.tar.gz` | ~200MB - 2GB |
| **HeapStore IPC** | PATH_KERNEL_IPC 分区 | `.tar.gz` | ~50MB |
| **HeapStore Memory** | PATH_KERNEL_MEMORY 分区 | `.tar.gz` | ~100MB |
| **Configs** | gateway.yaml, seccomp-profile.json | 原始文件 | ~10KB |
| **Manifest** | 元数据 + SHA256 校验和 | JSON | ~5KB |

#### 备份计划建议

| 环境 | 备份频率 | 保留期限 | 存储位置 |
|------|---------|---------|---------|
| **Development** | 每日 (凌晨 3 点) | 7 天 | 本地 `/dev/backups/dev` |
| **Staging** | 每 6 小时 | 14 天 | NAS + 云存储 (双副本) |
| **Production** | 每小时增量 + 每日全量 | 30 天 | 异地灾备中心 (加密传输) |

### 7.4 性能调优建议

#### Kernel 性能调优

```bash
# ========================================
# 1. 调整连接池大小
# ========================================
# 编辑 .env.production:
MAX_CONNECTIONS=20000        # 根据实际并发调整

# ========================================
# 2. 优化内存分配器
# ========================================
# Kernel 容器环境变量:
MALLOC_ARENA_MAX=4G          # 增加 arena 数量 (减少锁竞争)
MALLOC_MMAP_THRESHOLD_=262144  # 提高 mmap 阈值

# ========================================
# 3. CPU 亲和性绑定 (高级)
# ========================================
# 编辑 docker-compose.prod.yml:
deploy:
  resources:
    limits:
      cpus: '8'
  # 添加 (需要 Docker 24+):
  # cpuset: '0-7'  # 绑定到物理核心 0-7
```

#### PostgreSQL 性能调优

```sql
-- ========================================
-- 1. 分析慢查询 (登录 psql 执行)
-- ========================================
SELECT query, calls, total_exec_time, mean_exec_time
FROM pg_stat_statements
ORDER BY mean_exec_time DESC
LIMIT 10;

-- ========================================
-- 2. 检查缺失索引
-- ========================================
SELECT schemaname || '.' || relname AS table_name,
       seq_scan,
       idx_scan
FROM pg_stat_user_tables
WHERE seq_scan > 1000 AND idx_scan = 0
ORDER BY seq_scan DESC;

-- ========================================
-- 3. 检查连接池状态
-- ========================================
SELECT state, count(*)
FROM pg_stat_activity
GROUP BY state;
-- active 应该 < max_connections * 80%
```

#### Redis 性能调优

```bash
# ========================================
# 1. 分析大键 (可能导致阻塞)
# ========================================
docker compose exec redis redis-cli --bigkeys

# ========================================
# 2. 检查内存碎片率
# ========================================
docker compose exec redis redis-cli info memory | grep mem_fragmentation_ratio
# 如果 > 1.5, 考虑重启 Redis 或增加内存

# ========================================
# 3. 监控淘汰统计
# ========================================
docker compose exec redis redis-cli info stats | grep evicted_keys
# 如果持续增长, 说明 maxmemory 过小或 key 设计不合理
```

#### 系统级调优 (Linux)

```bash
# ========================================
# 1. 增加文件描述符限制
# ========================================
# 编辑 /etc/security/limits.conf:
* soft nofile 65535
* hard nofile 65535

# ========================================
# 2. 优化 TCP 参数
# ========================================
# 编辑 /etc/sysctl.conf:
net.core.somaxconn = 65535              # 增大监听队列
net.ipv4.tcp_tw_reuse = 1               # 重用 TIME_WAIT
net.ipv4.tcp_fin_timeout = 15           # 快速回收连接
vm.swappiness = 10                      # 减少交换分区使用 (性能优先)

# 应用配置
sudo sysctl -p

# ========================================
# 3. Docker 存储 driver 优化
# ========================================
# 编辑 /etc/docker/daemon.json:
{
  "storage-driver": "overlay2",         # 推荐 overlay2
  "log-driver": "json-file",
  "log-opts": {
    "max-size": "100m",
    "max-file": "3"
  }
}

sudo systemctl restart docker
```

---

## 8. 监控与告警

### 8.1 Prometheus 指标体系

#### 核心指标 (Core Metrics)

| 指标名称 | 类型 | 服务 | 说明 | 告警阈值 |
|---------|------|------|------|---------|
| `up` | Gauge | 所有 | 服务存活状态 (1=UP, 0=DOWN) | == 0 (critical) |
| `process_cpu_seconds_total` | Counter | Kernel/Gateway | CPU 使用时间累计 | > 80% (warning) |
| `process_resident_memory_bytes` | Gauge | Kernel/Gateway | RSS 内存使用量 | > 3.5GB (warning) |
| `http_requests_total` | Counter | Gateway | HTTP 请求总数 (按 status code 分类) | - |
| `http_request_duration_seconds` | Histogram | Gateway | 请求延迟分布 | P99 > 2s (warning) |
| `agentos_syscall_duration_seconds` | Histogram | Kernel | Syscall 执行时间 | P99 > 1s (warning) |
| `agentos_ipc_active_connections` | Gauge | Kernel | 当前活跃 IPC 连接数 | > 8000 (warning) |
| `gateway_rate_limited_requests_total` | Counter | Gateway | 被限流的请求数 | > 10 req/s (info) |

#### 数据库指标 (Database Metrics)

| 指标名称 | 类型 | 服务 | 说明 | 告警阈值 |
|---------|------|------|------|---------|
| `pg_stat_activity_count` | Gauge | PostgreSQL | 活跃连接数 | > 90% max_conn (critical) |
| `pg_stat_statements_total_exec_time` | Counter | PostgreSQL | 总执行时间 | avg > 500ms (warning) |
| `pg_replication_lag_seconds` | Gauge | PostgreSQL | 主从复制延迟 | > 300s (warning) |
| `redis_memory_used_bytes` | Gauge | Redis | 已使用内存 | > 80% maxmem (warning) |
| `redis_keyspace_hits_total` | Counter | Redis | 缓存命中次数 | 命中率 < 70% (warning) |
| `redis_keyspace_misses_total` | Counter | Redis | 缓存未命中次数 | - |

#### 自定义业务指标 (Custom Metrics)

可通过 Gateway 的 `/metrics` 端点暴露自定义指标：

```python
# 示例: 在 Agent 业务代码中暴露自定义指标
from prometheus_client import Counter, Histogram, Gauge

# 定义指标
SYSCALL_INVOKES = Counter(
    'agentos_syscall_invokes_total',
    'Total syscall invocations',
    ['syscall_type', 'result']
)

SYSCALL_LATENCY = Histogram(
    'agentos_syscall_latency_seconds',
    'Syscall execution latency',
    ['syscall_type'],
    buckets=(0.01, 0.05, 0.1, 0.5, 1.0, 2.0, 5.0)
)

ACTIVE_AGENTS = Gauge(
    'agentos_active_agents',
    'Number of currently active agents'
)
```

### 8.2 Grafana 仪表盘

#### 访问地址

- **开发环境**: http://localhost:3000 (admin / admin_dev_2024)
- **生产环境**: https://your-domain.com/grafana (VPN Only)

#### 预置仪表盘导入

如果自动 provisioning 未生效，手动导入 JSON：

```bash
# 1. 登录 Grafana Web UI
# 2. 左侧菜单 → Dashboards → Import
# 3. 上传 monitoring/grafana/dashboards/*.json 文件
# 4. 选择 Prometheus 数据源
# 5. 点击 Import
```

#### 推荐的面板布局

```
┌─────────────────────────────────────────────────────────────┐
│                    Row 1: System Overview                    │
│  ┌──────────────┐  ┌──────────────┐  ┌────────────────────┐ │
│  │  Total QPS   │  │ Error Rate % │  │ Active Connections │ │
│  │  (Gauge)     │  │  (Stat)      │  │  (Graph)           │ │
│  └──────────────┘  └──────────────┘  └────────────────────┘ │
├─────────────────────────────────────────────────────────────┤
│                    Row 2: Latency Analysis                   │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Request Latency (P50/P95/P99) - Heatmap or Graph    │  │
│  └──────────────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────────────┤
│                    Row 3: Resource Usage                     │
│  ┌──────────────────┐  ┌──────────────────┐                │
│  │ CPU Usage (%)    │  │ Memory Usage (GB) │                │
│  │  (Stacked Area)  │  │  (Stacked Area)  │                │
│  └──────────────────┘  └──────────────────┘                │
├─────────────────────────────────────────────────────────────┤
│                    Row 4: Database & Cache                   │
│  ┌──────────────────┐  ┌──────────────────┐                │
│  │ PG Connection    │  │ Redis Hit Rate   │                │
│  │  Pool Usage      │  │  (%)             │                │
│  └──────────────────┘  └──────────────────┘                │
└─────────────────────────────────────────────────────────────┘
```

### 8.3 告警规则说明

#### 告警级别定义

| 级别 | 图标 | 响应时间 | 通知渠道 | 影响范围 |
|------|------|---------|---------|---------|
| **Critical** | 🔴 | 即时 (< 5 分钟) | PagerDuty + SMS + 电话 | 服务宕机、数据丢失风险 |
| **Warning** | 🟡 | 当日 (< 4 小时) | Slack + Email | 性能下降、资源紧张 |
| **Info** | 🔵 | 下个工作日 | 仅 Slack | 容量规划、趋势预警 |

#### 关键告警处理 SOP (Standard Operating Procedure)

**🔴 Critical: KernelServiceDown**

```bash
# 1. 立即确认告警
docker ps | grep kernel

# 2. 查看最近日志 (查找崩溃原因)
docker logs --tail 200 agentos-kernel-prod | grep -i "error\|fatal\|panic"

# 3. 尝试重启
docker restart agentos-kernel-prod

# 4. 等待健康检查通过 (最多 60 秒)
sleep 60
docker inspect --format='{{.State.Health.Status}}' agentos-kernel-prod
# 预期输出: healthy

# 5. 如果仍失败, 收集诊断信息并升级
docker logs agentos-kernel-prod > /tmp/kernel_crash_$(date +%Y%m%d_%H%M%S).log
# 联系 platform 团队并提供日志文件
```

**🟡 Warning: PostgresConnectionPoolExhausted**

```sql
-- 1. 登录 PostgreSQL 查看当前连接
docker compose exec postgres psql -U agentos -d agentos

-- 2. 查看连接分布
SELECT state, count(*), query
FROM pg_stat_activity
GROUP BY state, query
ORDER BY count(*) DESC;

-- 3. 检查是否有长事务阻塞
SELECT pid, now() - pg_stat_activity.query_start AS duration, query
FROM pg_stat_activity
WHERE (now() - pg_stat_activity.query_start) > interval '5 minutes';

-- 4. 如果发现僵死进程, 可以终止 (谨慎!)
-- SELECT pg_terminate_backend(<PID>);
```

---

## 9. 扩展指南

### 9.1 水平扩展 (Horizontal Scaling)

#### Kernel 扩展

```bash
# 扩展到 3 个实例 (需要前置负载均衡器或 DNS 轮询)
docker compose -f docker/docker-compose.prod.yml \
    --env-file .env.production \
    up -d --scale kernel=3

# 验证扩展结果
docker compose -f docker/docker-compose.prod.yml ps | grep kernel
# 应该看到 3 个容器: agentos-kernel-prod-1, -2, -3
```

**注意事项:**
- Kernel 是**无状态服务** (状态存在 PostgreSQL + Redis)，可以自由扩展
- Gateway 通过 DNS 轮询 (`http://kernel:18080`) 自动负载均衡
- 确保 PostgreSQL/Redis 连接池足够大 (`MAX_CONNECTIONS`, Redis `maxclients`)
- 监控每个实例的资源使用，避免过度扩展导致资源争抢

#### Gateway 扩展

```bash
# 扩展 Gateway 到 2 个实例 (需要外部 LB 如 Nginx/HAProxy)
docker compose -f docker/docker-compose.prod.yml \
    --env-file .env.production \
    up -d --scale gateway=2

# 配置 Nginx 反向代理 (upstream)
upstream agentos_gateway {
    least_conn;  # 最少连接算法
    server gateway-1:18789;
    server gateway-2:18789;
}

server {
    listen 443 ssl;
    server_name api.agentos.cn;

    location / {
        proxy_pass http://agentos_gateway;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
}
```

#### 数据库扩展 (读写分离)

```yaml
# docker-compose.prod.yml 中添加 PostgreSQL 主从复制
services:
  postgres-master:
    image: postgres:15-alpine
    environment:
      POSTGRES_REPLICATION_ROLE: master
    volumes:
      - postgres-master-data:/var/lib/postgresql/data

  postgres-replica:
    image: postgres:15-alpine
    environment:
      POSTGRES_REPLICATION_ROLE: replica
      POSTGRES_MASTER_SERVICE: postgres-master
    depends_on:
      - postgres-master
```

### 9.2 Kubernetes 迁移

#### 使用 Kompose 转换 (快速迁移)

```bash
# 安装 kompose
sudo curl -L https://github.com/kubernetes/kompose/releases/latest/download/kompose-linux-amd64 -o /usr/local/bin/kompose
sudo chmod +x /usr/local/bin/kompose

# 转换 Compose 文件为 Kubernetes manifests
kompose convert -f docker/docker-compose.prod.yml -o k8s/

# 生成的文件:
# k8s/kernel-deployment.yaml
# k8s/kernel-service.yaml
# k8s/gateway-deployment.yaml
# k8s/gateway-service.yaml
# k8s/postgres-statefulset.yaml
# k8s/redis-statefulset.yaml
# ...

# 应用到集群
kubectl apply -f k8s/
```

#### 生产级 K8s 增强 (推荐)

```yaml
# k8s/kernel-production.yaml (示例)
apiVersion: apps/v1
kind: Deployment
metadata:
  name: agentos-kernel
spec:
  replicas: 3
  strategy:
    type: RollingUpdate
    rollingUpdate:
      maxSurge: 1
      maxUnavailable: 0
  template:
    spec:
      # 安全上下文 (对应 Docker security_opt)
      securityContext:
        runAsNonRoot: true
        runAsUser: 1000
        readOnlyRootFilesystem: true
        allowPrivilegeEscalation: false
        capabilities:
          drop:
            - ALL
          add:
            - NET_BIND_SERVICE
      # 资源限制 (对应 deploy.resources)
      resources:
        requests:
          cpu: "2"
          memory: "2Gi"
        limits:
          cpu: "4"
          memory: "4Gi"
      # 就绪探针 (对应 healthcheck)
      readinessProbe:
        httpGet:
          path: /api/v1/health
          port: 18080
        initialDelaySeconds: 30
        periodSeconds: 10
      # 存活探针
      livenessProbe:
        httpGet:
          path: /api/v1/health
          port: 18080
        initialDelaySeconds: 60
        periodSeconds: 30
---
apiVersion: policy/v1
kind: PodDisruptionBudget
metadata:
  name: kernel-pdb
spec:
  minAvailable: 2
  selector:
    matchLabels:
      app: agentos-kernel
```

---

## 10. 故障排查

### 10.1 常见问题 FAQ

#### ❌ 问题 1: 容器无法启动 (Exit Code 137)

**原因**: OOM Killer (Out of Memory) 终止了容器

**症状**:
```bash
docker ps -a | grep kernel
# 输出: Exit 137 (128 + 9 = SIGKILL)
```

**解决方案**:

```bash
# 1. 检查当前内存使用
docker stats --no-stream kernel

# 2. 增加 memory limit (编辑 docker-compose.prod.yml)
deploy:
  resources:
    limits:
      memory: 8G  # 从 4G 增加到 8G

# 3. 重启服务
docker compose -f docker/docker-compose.prod.yml up -d --force-recreate kernel
```

**预防措施**:
- 设置合理的 memory limit (建议: 实际使用的 1.5-2 倍)
- 启用 Prometheus 内存告警 (> 80% 时预警)
- 定期分析内存泄漏 (使用 valgrind 或 ASAN)

---

#### ❌ 问题 2: Gateway 返回 502 Bad Gateway

**原因**: 上游 Kernel 服务不可达或不健康

**症状**:
```bash
curl -I http://localhost:18789/api/v1/health
# HTTP/1.1 502 Bad Gateway
```

**排查步骤**:

```bash
# 1. 检查 Kernel 是否运行
docker ps | grep kernel

# 2. 检查 Kernel 健康状态
docker inspect --format='{{.State.Health.Status}}' agentos-kernel-prod
# 如果不是 "healthy", 查看健康检查日志

# 3. 检查网络连通性 (从 Gateway 容器内 ping Kernel)
docker compose exec gateway ping -c 3 kernel
# 或者测试端口
docker compose exec gateway curl -f http://kernel:18080/api/v1/health

# 4. 检查 Gateway 日志中的上游错误
docker compose logs --tail 50 gateway | grep -i "upstream\|error\|refused"
```

**常见原因及修复**:

| 错误信息 | 原因 | 解决方案 |
|---------|------|---------|
| `connection refused` | Kernel 未启动或端口错误 | 检查 Kernel 端口是否为 18080 |
| `no route to host` | 网络隔离问题 | 确认两个服务在同一 network |
| `i/o timeout` | Kernel 响应过慢 | 检查 Kernel 资源/CPU 饱和 |

---

#### ❌ 问题 3: PostgreSQL 连接数耗尽

**症状**:
```bash
# 应用报错:
# FATAL: remaining connection slots are reserved for non-replication superuser connections

# 或:
# ERROR: too many connections for role "agentos"
```

**紧急处理**:

```bash
# 1. 登录 PostgreSQL (使用超级用户, 通过 docker exec)
docker compose exec postgres psql -U postgres

# 2. 查看当前连接
SELECT count(*), state FROM pg_stat_activity GROUP BY state;

# 3. 终止空闲过长的连接 (谨慎操作!)
SELECT pg_terminate_backend(pid)
FROM pg_stat_activity
WHERE state = 'idle'
  AND query_start < NOW() - INTERVAL '10 minutes'
  AND pid != pg_backend_pid();

# 4. 长期解决: 增加 max_connections 并重启
# 编辑 docker-compose.prod.yml:
# command: [..., "-c", "max_connections=500", ...]
```

**根本原因分析**:

```sql
-- 查找消耗连接最多的查询
SELECT query, count(*), max(now() - query_start) as max_duration
FROM pg_stat_activity
WHERE state = 'active'
GROUP BY query
ORDER BY count(*) DESC
LIMIT 10;

-- 常见原因:
# 1. 连接池未正确释放 (忘记 close())
# 2. 慢查询堆积 (长时间持有连接)
# 3. max_connections 设置过低
```

---

#### ❌ 问题 4: Redis 内存不足 (OOM 或频繁淘汰)

**症状**:
```bash
# Redis 日志:
# # Can't save in background: fork: Cannot allocate memory

# 或:
# INFO: evicting keys (allkeys-lru mode)
```

**解决方案**:

```bash
# 1. 检查当前内存使用
docker compose exec redis redis-cli info memory | grep used_memory_human

# 2. 分析大键 (可能导致内存碎片)
docker compose exec redis redis-cli --bigkeys

# 3. 检查淘汰统计
docker compose exec redis redis-cli info stats | grep evicted_keys
# 如果数字很大, 说明频繁淘汰

# 4. 解决方案 (按优先级):
# a. 增加 REDIS_MAX_MEMORY (如: 2048mb)
# b. 优化数据结构 (压缩大 Hash/List)
# c. 设置合理 TTL (避免无限增长的 key)
# d. 使用 Redis Cluster 分片 (数据量极大时)
```

---

### 10.2 故障诊断清单

当遇到未知问题时，按照此清单逐步排查：

#### Phase 1: 快速检查 (5 分钟)

```bash
# ✅ 1. 所有容器是否在运行?
docker compose -f docker/docker-compose.prod.yml ps
# 状态应该是 "Up" 或 "running (healthy)"

# ✅ 2. Docker daemon 是否正常?
docker info > /dev/null && echo "Docker OK" || echo "Docker ERROR"

# ✅ 3. 磁盘空间是否充足?
df -h | grep -E "/$|/var/lib/docker"
# 使用率应 < 85%

# ✅ 4. 内存是否充足?
free -h
# Available 应 > 2GB

# ✅ 5. 端口是否被监听?
ss -tlnp | grep -E "18789|18080|5432|6379"
# 应看到 LISTEN 状态
```

#### Phase 2: 服务健康检查 (10 分钟)

```bash
# ✅ 6. 运行健康检查脚本
./docker/scripts/healthcheck.sh --env prod --verbose

# ✅ 7. 检查每个服务的健康端点
for svc in gateway kernel postgres redis; do
  echo "=== Testing $svc ==="
  case $svc in
    gateway) curl -sf http://localhost:18789/api/v1/health ;;
    kernel) curl -sf http://localhost:18080/api/v1/health ;;
    postgres) docker compose exec postgres pg_isready -U agentos ;;
    redis) docker compose exec redis redis-cli ping ;;
  esac
  echo ""
done

# ✅ 8. 检查最近的错误日志
docker compose logs --since "10 minutes" 2>&1 | grep -i "error\|fatal\|panic\|exception" | tail -50
```

#### Phase 3: 深度诊断 (30 分钟)

```bash
# ✅ 9. 资源瓶颈分析
docker stats --no-stream --format "table {{.Name}}\t{{.CPUPerc}}\t{{.MemUsage}}\t{{.NetIO}}\t{{.BlockIO}}"
# 关注 CPU > 80%, Memory > 80% limit 的容器

# ✅ 10. 网络连通性测试
docker compose exec gateway ping -c 3 kernel
docker compose exec gateway ping -c 3 postgres
docker compose exec gateway ping -c 3 redis

# ✅ 11. 数据库深度检查
docker compose exec postgres psql -U agentos -d agentos -c "
SELECT 
  (SELECT count(*) FROM pg_stat_activity WHERE state = 'active') as active_conns,
  (SELECT count(*) FROM pg_stat_activity) as total_conns,
  (SELECT count(*) FROM pg_stat_activity WHERE wait_event_type = 'Lock') as waiting_locks;
"

# ✅ 12. Redis 深度检查
docker compose exec redis redis-cli info server | grep -E "uptime|connected_clients|used_memory_human"
docker compose exec redis redis-cli info stats | grep -E "total_commands_processed|evicted_keys|keyspace_hits"
```

#### Phase 4: 收集证据并寻求帮助

如果以上步骤无法解决问题：

```bash
# 1. 收集完整诊断信息包
mkdir -p /tmp/agentos_diag_$(date +%Y%m%d_%H%M%S)
cd /tmp/agentos_diag_*

# 容器状态
docker compose -f docker/docker-compose.prod.yml ps > containers_status.txt

# 最近日志 (每个服务 500 行)
for svc in kernel gateway postgres redis; do
  docker compose logs --tail 500 $svc > ${svc}_logs.txt 2>&1
done

# 资源使用快照
docker stats --no-stream --no-trunc > resource_usage.txt

# 系统信息
uname -a > system_info.txt
free -h >> system_info.txt
df -h >> system_info.txt
docker info >> system_info.txt

# 打包
cd ..
tar -czvf agentos_diagnostic_$(date +%Y%m%d_%H%M%S).tar.gz agentos_diag_*

# 2. 联系支持团队并提供:
# - 问题描述 (何时开始? 影响范围?)
# - 诊断信息包 (上述 tar.gz 文件)
# - 已尝试的解决步骤
# - 期望的响应时间
```

---

## 11. 附录

### 11.1 文件结构说明

```
docker/
├── README.md                          # 📖 本文档 (企业级部署指南)
│
├── Dockerfile.kernel                  # 🔨 内核镜像构建 (v2.0.0)
│   ├── Builder (gcc/cmake/ninja)      #    编译阶段
│   ├── Runtime (最小依赖)             #    生产运行时
│   └── Debug (gdb/strace)             #    调试版本
│
├── Dockerfile.daemon                  # 🔨 守护进程镜像构建 (v2.0.0)
│   ├── Builder (Python/C++)           #    编译阶段
│   ├── Runtime (Supervisor)           #    守护进程运行时
│   ├── Gateway (HTTP/WS/stdio)        #    网关服务
│   └── Debug                          #    调试版本
│
├── docker-compose.yml                 # 🐳 开发环境编排 (v2.0.0)
│   ├── 双层网络 (backend/frontend)    #
│   ├── 资源限制 (CPU/Memory)          #
│   └── Profiles (openlab/monitoring)  #
│
├── docker-compose.prod.yml            # 🐳 生产环境编排 (v2.0.0)
│   ├── 安全加固 (seccomp/cap_drop)    #
│   ├── 只读文件系统                   #
│   ├── 强制密码校验 (${VAR:?error})    #
│   └── 水平扩展 (--scale)            #
│
├── .env.production.example            # 🔐 生产环境变量模板
│   ├── 密码生成命令                    #
│   ├── 所有配置项说明                  #
│   └── 安全警告提示                    #
│
├── .dockerignore                      # 🚫 Docker 构建排除列表
│
├── config/                           # ⚙️ 配置文件目录
│   ├── gateway.yaml                   #    Gateway 完整配置 (CORS/限流/JWT)
│   ├── seccomp-profile.json           #    seccomp 安全配置 (系统调用白名单)
│   ├── supervisor/                   #    Supervisor 进程管理配置
│   │   ├── supervisord.conf           #    主配置
│   │   └── conf.d/*.conf             #    各守护进程配置
│   └── nginx/                        #    Nginx 反向代理配置 (可选)
│
├── scripts/                          # 🛠️ 运维脚本
│   ├── healthcheck.sh                #    综合健康检查 (text/json)
│   └── backup.sh                     #    自动化备份恢复 (加密/校验)
│
└── monitoring/                       # 📊 监控栈配置
    ├── prometheus.yml                #    Prometheus 抓取配置 (v2.0.0)
    ├── rules/
    │   └── agentos_alerts.yml        #    告警规则 (5 组, 15+ 规则)
    └── grafana/
        ├── provisioning/
        │   ├── datasources/          #    数据源自动配置
        │   └── dashboards/           #    仪表盘自动加载
        └── dashboards/               #    预置仪表盘 JSON 文件
```

### 12. 版本更新日志

#### v2.0.0 (2026-04-06) - Production-Grade Major Release

**🎉 重大更新:**

##### ✨ 新功能 (Features)

- **Gateway 网关服务**: 新增三协议网关 (HTTP/WS/stdio), 作为唯一对外入口
- **双层网络隔离**: 前后端网络完全分离, 符合零信任架构
- **命名卷管理**: 所有卷使用 name 字段, 便于运维识别和备份
- **辅助运维脚本**:
  - `healthcheck.sh`: 综合健康检查 (支持 JSON 输出, 便于 CI/CD 集成)
  - `backup.sh`: 自动化备份恢复 (支持 GPG 加密, SHA256 校验, 增量备份)
- **Prometheus 告警规则**: 5 组 15+ 条生产级告警规则 (Critical/Warning/Info)
- **生产环境变量模板**: `.env.production.example` 含密码生成命令和安全提示

##### 🔒 安全增强 (Security Hardening)

- **CIS Docker Benchmark 合规**: 实现 10+ 项 CIS 控制措施
  - `read_only: true` (只读文件系统)
  - `cap_drop: ALL` + `cap_add: NET_BIND_SERVICE` (能力白名单)
  - `no-new-privileges:true` (禁止提权)
  - `seccomp-profile.json` (系统调用过滤)
- **强制密码校验**: `${VAR:?❌ ERROR}` 语法, 启动前验证必填项
- **非 root 用户运行**: `USER agentos:1000` (UID/GID: 1000, shell: /sbin/nologin)
- **端口最小化**: 生产环境默认不暴露内部端口 (18080/5432/6379)

##### 🐛 Bug 修复 (Bug Fixes)

- **端口修正**: Kernel IPC 端口从 8080 修正为 **18080** (与源码 syscalls.h 一致)
- **Gateway 端口**: 统一 API 入口修正为 **18789** (匹配 Python SDK v3.0.0 默认值)
- **健康检查 URL**: 从 `/health` 修正为 `/api/v1/health` (符合 RESTful 规范)
- **Dockerfile targets**: 更新为新的多阶段构建 target 名称 (runtime/gateway/debug)

##### 📚 文档改进 (Documentation)

- **README.md v2.0.0**: 企业级文档 (~1500 行), 包含:
  - 完整架构图 (ASCII art, 开发/生产双拓扑)
  - 部署矩阵 (开发/生产/Staging)
  - 运维手册 (日常命令/备份恢复/性能调优)
  - 故障排查指南 (FAQ + 诊断清单)
  - 扩展指南 (水平扩展/Kubernetes 迁移)
- **Prometheus 配置注释**: 每个抓取任务都有详细说明 (指标类型/用途/标签)
- **告警规则文档**: 每条告警包含影响范围/可能原因/排查步骤

##### ⚡ 性能优化 (Performance)

- **PostgreSQL 调优**: 连接池/WAL/慢查询日志/审计日志全面优化
- **Redis 调优**: AOF 持久化 + LRU 淘汰 + 惰性删除 (避免阻塞)
- **资源配额**: 所有限制值基于实际测试数据 (CPU/Memory/Connections)
- **日志轮转**: JSON 格式 + 自动轮转 (防止单个日志文件过大)

##### 🔄 Breaking Changes (破坏性变更)

⚠️ **升级前必读!**

| 变更项 | 旧版本 (v1.0.0) | 新版本 (v2.0.0) | 升级动作 |
|-------|-----------------|-----------------|---------|
| Kernel 端口 | 8080 | **18080** | 更新所有客户端连接串 |
| Gateway 端口 | N/A (无 Gateway) | **18789** | Python SDK 无需改动 (默认值一致) |
| Dockerfile target | `development` | `runtime` | 更新 compose 文件 |
| 网络名称 | `agentos-network` | `agentos-backend-prod` + `agentos-frontend-prod` | 重新创建网络 |
| 卷名称 | 匿名卷 (随机hash) | **命名卷** (如 `agentos_prod_heapstore_data`) | 数据迁移 (详见备份恢复章节) |
| 环境变量文件 | `.env` | **`.env.production`** | 重新创建并填入新配置项 |

**升级路径:**

```bash
# 1️⃣ 备份当前环境
./docker/scripts/backup.sh backup --env prod

# 2️⃣ 停止旧版服务
docker compose -f docker/docker-compose.yml down --timeout 60

# 3️⃣ 拉取新镜像 (如果有代码变更)
docker compose -f docker/docker-compose.prod.yml build

# 4️⃣ 创建新环境变量文件
cp docker/.env.production.example docker/.env.production
# 从旧 .env 迁移密码 (或重新生成更强的)

# 5️⃣ 启动新版服务
docker compose -f docker/docker-compose.prod.yml \
    --env-file docker/.env.production \
    up -d

# 6️⃣ 验证升级结果
./docker/scripts/healthcheck.sh --env prod --verbose
```

---

## 📞 支持与反馈

- **文档问题**: 提交 Issue 到 [AgentOS Docs Repository](https://gitcode.com/spharx/agentos/issues)
- **安全问题**: 发送邮件至 🔒 security@spharx.cn (请勿公开披露!)
- **运维支持**: 联系 DevOps 团队 ops@spharx.cn (SLA: Critical < 1h, Warning < 8h)
- **功能需求**: 在 [Feature Requests](https://gitcode.com/spharx/agentos/discussions) 讨论

---

## 📄 许可证

本项目采用 **Proprietary License** (专有许可证)。详见 [LICENSE](../LICENSE) 文件。

---

<p align="center">
  <strong>Made with ❤️ by SPHARX Platform Team</strong><br>
  <em>Last Updated: 2026-04-06 | Version: 2.0.0 (Production-Grade)</em>
</p>
