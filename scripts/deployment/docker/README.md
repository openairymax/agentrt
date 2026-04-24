# Docker 部署

`scripts/deployment/docker/`

## 概述

`docker/` 目录提供 AgentOS 的容器化部署方案，包含 Dockerfile 和多容器编排配置文件，支持开发和生产两种场景的快速部署。

## 文件清单

| 文件 | 说明 |
|------|------|
| `Dockerfile` | 生产环境 Docker 镜像构建 |
| `Dockerfile.dev` | 开发环境 Docker 镜像构建（含热重载） |
| `docker-compose.yml` | 多容器编排配置 |
| `.dockerignore` | Docker 构建忽略规则 |

## 快速开始

```bash
# 开发环境（热重载）
cd scripts/deployment/docker
docker-compose up

# 生产环境构建
docker build -t agentos:latest -f Dockerfile .
docker run -d -p 8080:8080 agentos:latest

# 生产环境完整启动
docker-compose -f docker-compose.yml up -d
```

## 服务组件

Docker Compose 默认启用的服务：

| 服务 | 说明 | 端口 |
|------|------|------|
| `gateway` | API 网关 | 8080 |
| `llm_d` | LLM 服务守护进程 | 8081 |
| `sched_d` | 任务调度守护进程 | 8082 |
| `heapstore` | 运行时数据存储 | 8083 |
| `monit_d` | 监控告警守护进程 | 8084 |

## 环境变量

| 变量 | 说明 | 默认值 |
|------|------|--------|
| `AGENTOS_ENV` | 运行环境 | `production` |
| `AGENTOS_LOG_LEVEL` | 日志级别 | `info` |
| `AGENTOS_CONFIG_DIR` | 配置目录 | `/etc/agentos` |
| `AGENTOS_DATA_DIR` | 数据目录 | `/var/lib/agentos` |

## 生产优化

- 使用多阶段构建减小镜像体积
- 设置资源限制（CPU/Memory）
- 配置健康检查探针
- 日志挂载到宿主机持久化存储
- 使用命名卷持久化数据

---

© 2026 SPHARX Ltd. All Rights Reserved.
