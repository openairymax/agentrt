# Deploy — 部署配置

`deploy/`

## 概述

`deploy/` 目录包含 AgentRT 的部署配置，提供容器化部署方案、多环境编排和监控集成。当前以 Docker 容器化部署为核心，支持开发、调试和生产三种环境的一键部署。

## 目录结构

```
deploy/
├── docker/                        # Docker 容器化部署
│   ├── Dockerfile                 #   多阶段构建镜像（builder → runtime → development）
│   ├── docker-compose.yml         #   基础服务编排
│   ├── docker-compose.dev.yml     #   开发环境覆盖
│   ├── docker-compose.test.yml    #   测试环境覆盖
│   ├── docker-compose.staging.yml #   预发布环境覆盖
│   ├── docker-compose.preview.yml #   预览环境覆盖
│   ├── docker-compose.prod.yml    #   生产环境覆盖
│   ├── .env.example               #   环境变量模板
│   ├── monitoring/                #   监控配置
│   │   ├── prometheus.yml         #     Prometheus 采集配置
│   │   ├── alerts.yml             #     告警规则
│   │   └── grafana_agentrt_dashboard.json  # Grafana 仪表盘
│   ├── secrets/                   #   Docker Swarm 密钥目录（.gitkeep）
│   └── README.md                  #   Docker 部署详细文档
└── README.md                      # 本文件
```

## 核心组件

### docker/ — Docker 容器化部署

Docker 部署方案采用双 Dockerfile 分层架构和多环境 Compose 编排：

| 组件 | 说明 |
|------|------|
| **Dockerfile** | 三阶段构建：builder（编译）→ runtime（生产）→ development（调试） |
| **docker-compose.yml** | 基础编排：Gateway + Redis + Prometheus + Grafana |
| **docker-compose.dev.yml** | 开发覆盖：挂载源码、启用调试工具 |
| **docker-compose.prod.yml** | 生产覆盖：资源限制、安全加固、日志轮转 |
| **monitoring/** | Prometheus 采集 + Grafana 仪表盘 + 告警规则 |

### 暴露端口

| 端口 | 协议 | 用途 |
|------|------|------|
| 8080 | HTTP | API 网关 |
| 8081 | WebSocket | WebSocket 网关 |
| 9090 | HTTP | Prometheus Metrics |

## 快速启动

```bash
# 1. 复制环境变量模板
cp deploy/docker/.env.example deploy/docker/.env
# 编辑 .env 填入实际密钥

# 2. 开发环境启动
docker-compose -f deploy/docker/docker-compose.yml \
               -f deploy/docker/docker-compose.dev.yml up -d

# 3. 生产环境启动
docker-compose -f deploy/docker/docker-compose.yml \
               -f deploy/docker/docker-compose.prod.yml up -d

# 4. 查看日志
docker-compose -f deploy/docker/docker-compose.yml logs -f gateway
```

## 依赖关系

| 组件 | 版本 | 用途 |
|------|------|------|
| Docker | ≥ 20.10 | 容器运行时 |
| Docker Compose | ≥ 2.0 | 服务编排 |
| Alpine | 3.19 | 基础镜像 |
| Redis | 7-alpine | 缓存与会话存储 |
| Prometheus | v2.45.0 | 指标采集 |
| Grafana | 10.2.0 | 可视化仪表盘 |

## 部署目录说明

`deploy/docker/` 是 AgentRT 唯一的 Docker 容器化部署目录，提供从开发到生产的全环境编排（dev/test/staging/preview/prod）。早期的 `scripts/ops/deploy/` 配置已合并至此，详见 [docker/README.md](docker/README.md)。

---

© 2026 SPHARX Ltd. All Rights Reserved.
