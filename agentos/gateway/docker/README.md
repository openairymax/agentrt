# Gateway Docker — 容器化部署配置

**模块路径**: `agentos/gateway/docker/`
**版本**: v0.0.5

## 概述

`gateway/docker/` 包含 AgentOS Gateway 的 Docker 容器化部署配置，提供多阶段构建、多环境编排和监控集成。支持 MCP/A2A/OpenAI API 多协议网关，通过 Docker Compose 实现开发、调试和生产三种环境的一键部署。

## 目录结构

```
docker/
├── Dockerfile                          # 多阶段构建镜像（builder → runtime → development）
├── docker-compose.yml                  # 基础服务编排（Gateway + Redis + Prometheus + Grafana）
├── docker-compose.dev.yml              # 开发调试环境覆盖
├── docker-compose.prod.yml             # 生产环境覆盖
├── .env.example                        # 环境变量模板
├── monitoring/                         # 监控配置
│   ├── prometheus.yml                  # Prometheus 采集配置
│   ├── alerts.yml                      # 告警规则
│   └── grafana_agentos_dashboard.json  # Grafana 仪表盘
└── README.md                           # 本文件
```

## 核心组件

### Dockerfile

三阶段构建设计：

| 阶段 | 基础镜像 | 用途 |
|------|---------|------|
| **builder** | alpine:3.19 | 编译构建（CMake + Ninja，启用 MCP/A2A/OpenAI） |
| **runtime** | alpine:3.19 | 生产运行时（最小依赖，非 root 用户） |
| **development** | builder | 开发调试（含 gdb/valgrind/cppcheck） |

**运行时特性**：
- 非 root 用户 `agentos`（UID 1000）运行
- 健康检查：`wget http://localhost:8080/health`
- 暴露端口：8080（HTTP）/ 8081（WebSocket）/ 9090（Metrics）
- 入口命令：`/usr/bin/gateway --manager /etc/agentos/gateway/manager.yaml`

### docker-compose.yml

编排四个服务：

| 服务 | 镜像 | 用途 |
|------|------|------|
| **gateway** | 本地构建 | 多协议网关主服务 |
| **redis** | redis:7-alpine | 缓存与会话存储 |
| **prometheus** | prom/prometheus:v2.45.0 | 指标采集 |
| **grafana** | grafana/grafana:10.2.0 | 可视化仪表盘 |

**Gateway 服务配置**：
- 端口映射：8080/8081/9090
- 资源限制：CPU 2 核 / 内存 512MB
- 健康检查：30s 间隔，10s 超时
- 日志轮转：最大 10MB × 3 文件

### .env.example

环境变量模板，包含：
- 安全密钥配置（JWT_SECRET / POSTGRES_PASSWORD / GRAFANA_PASSWORD）
- 数据库配置（POSTGRES_USER / POSTGRES_DB）
- 第三方 API 密钥（OPENAI_API_KEY）
- 时区设置（TZ）

## 使用说明

```bash
# 复制环境变量模板
cp .env.example .env
# 编辑 .env 填入实际密钥

# 开发环境启动
docker-compose -f docker-compose.yml -f docker-compose.dev.yml up -d

# 生产环境启动
docker-compose -f docker-compose.yml -f docker-compose.prod.yml up -d

# 查看日志
docker-compose logs -f gateway

# 停止服务
docker-compose down
```

### 手动构建与运行

```bash
# 构建镜像
docker build -t agentos-gateway:latest -f docker/Dockerfile .

# 运行容器
docker run -d \
  --name agentos-gateway \
  -p 8080:8080 -p 8081:8081 -p 9090:9090 \
  -v /etc/agentos/config:/etc/agentos/gateway:ro \
  -v /etc/agentos/certs:/etc/agentos/certs:ro \
  agentos-gateway:latest
```

## 依赖关系

| 组件 | 用途 |
|------|------|
| Docker ≥ 20.10 | 容器运行时 |
| Docker Compose ≥ 2.0 | 服务编排 |
| Alpine 3.19 | 基础镜像 |
| libmicrohttpd | HTTP 服务器运行时 |
| libwebsockets | WebSocket 运行时 |
| cjson | JSON 解析运行时 |
| OpenSSL | TLS/SSL 运行时 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
