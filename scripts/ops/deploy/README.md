# Docker 容器化部署

`scripts/ops/deploy/`

## 概述

`deploy/` 目录提供 AgentOS 的容器化部署方案，共 **14 个文件**，包含双 Dockerfile（内核+服务）和多环境 Docker Compose 编排配置，支持开发、预览、预发布和生产四种场景的快速部署。

> **版本**：v0.1.0
> **基础镜像**：Ubuntu 22.04

## 与 agentos/ 模块对应关系

| deploy/ 组件 | 对应的 agentos/ 模块 | 说明 |
|-------------|---------------------|------|
| `Dockerfile.kernel` | `atoms/`, `commons/`, `cupolas/` | 内核基础镜像，包含 C/C++ 编译依赖和内核组件 |
| `Dockerfile.service` | `daemon/`, `gateway/`, `heapstore/`, `manager/` | 服务层镜像，包含完整运行时和所有守护进程 |
| `docker-compose.*.yml` | `daemon/`, `gateway/` | 多环境编排，覆盖 gateway_d, llm_d, sched_d, market_d, monit_d 等服务 |
| `build.sh` | `daemon/`, `gateway/` | 镜像构建脚本（dev/release 模式） |
| `quickstart.sh` | 全部模块 | 一键快速入门（环境检查→镜像构建→服务启动） |
| `check_config.sh` | `manager/` | 配置和环境检查脚本 |

## 文件清单

| 文件 | 类型 | 说明 |
|------|------|------|
| `Dockerfile.kernel` | 镜像定义 | 内核基础镜像（Ubuntu 22.04，含编译依赖） |
| `Dockerfile.service` | 镜像定义 | 服务层镜像（自包含构建，完整运行时） |
| `Makefile` | 构建配置 | Docker 操作 Makefile（构建/启动/停止/清理） |
| `build.sh` | 脚本 | 镜像构建脚本（支持 dev/release 模式） |
| `check_config.sh` | 脚本 | 配置文件和环境检查脚本 |
| `quickstart.sh` | 脚本 | 一键快速入门（环境检查→镜像构建→服务启动） |
| `docker-compose.yml` | 编排配置 | 默认编排（开发环境） |
| `docker-compose.preview.yml` | 编排配置 | 预览环境编排 |
| `docker-compose.staging.yml` | 编排配置 | 预发布环境编排 |
| `docker-compose.prod.yml` | 编排配置 | 生产环境编排（高可用/安全加固/监控完备） |
| `.env.example` | 环境模板 | 环境变量模板 |
| `.gitignore` | 忽略规则 | Git 忽略规则 |
| `secrets/` | 目录 | 密钥目录（.gitkeep） |

## 服务组件

| 服务 | 对应的 agentos/ 模块 | 说明 | 端口 |
|------|---------------------|------|------|
| `gateway` | `gateway/` | API 网关 | 8080 |
| `llm_d` | `daemon/llm_d/` | LLM 服务守护进程 | 8081 |
| `sched_d` | `daemon/sched_d/` | 任务调度守护进程 | 8082 |
| `heapstore` | `heapstore/` | 运行时数据存储 | 8083 |
| `monit_d` | `daemon/monit_d/` | 监控告警守护进程 | 8084 |

## 快速开始

```bash
# 一键快速启动
cd scripts/ops/deploy
./quickstart.sh

# 手动构建和启动
./build.sh --release
docker-compose up -d

# 生产环境
docker-compose -f docker-compose.prod.yml up -d
```

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