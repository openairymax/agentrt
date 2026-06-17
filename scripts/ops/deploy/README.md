# Docker 容器化部署

`scripts/ops/deploy/`

## 概述

`deploy/` 目录提供 AgentRT 的容器化部署方案，采用双 Dockerfile 分层架构（内核基础镜像 + 服务层镜像）和多环境 Docker Compose 编排配置。支持开发、预览、预发布和生产四种场景的快速部署，覆盖从镜像构建到服务编排的完整容器化生命周期。

部署方案的设计原则：

- **分层构建**：双 Dockerfile 架构将内核编译和服务运行分离，优化构建缓存和镜像体积
- **多环境支持**：提供四种 Docker Compose 编排配置，从开发到生产的渐进式环境配置
- **安全加固**：生产环境启用安全加固、资源限制和健康检查，密钥通过 secrets 目录管理
- **一键操作**：`quickstart.sh` 和 `Makefile` 提供快捷操作入口，降低部署复杂度

> **版本**：v0.1.0
> **基础镜像**：Ubuntu 22.04

## 与 agentos/ 模块对应关系

| deploy/ 组件 | 对应的 agentos/ 模块 | 说明 |
|-------------|---------------------|------|
| `Dockerfile.kernel` | `atoms/`, `commons/`, `cupolas/` | 内核基础镜像，包含 C/C++ 编译依赖和内核组件 |
| `Dockerfile.service` | `daemon/`, `gateway/`, `heapstore/`, `manager/` | 服务层镜像，包含完整运行时和所有守护进程 |
| `docker-compose.yml` | `daemon/`, `gateway/` | 默认编排（开发环境），覆盖 gateway_d, llm_d, sched_d 等服务 |
| `docker-compose.preview.yml` | `daemon/`, `gateway/` | 预览环境编排，增加基本资源限制和健康检查 |
| `docker-compose.staging.yml` | `daemon/`, `gateway/` | 预发布环境编排，配置与生产环境一致 |
| `docker-compose.prod.yml` | `daemon/`, `gateway/` | 生产环境编排（高可用/安全加固/监控完备） |
| `build.sh` | `daemon/`, `gateway/` | 镜像构建脚本（dev/release 模式） |
| `quickstart.sh` | 全部模块 | 一键快速入门（环境检查→镜像构建→服务启动） |
| `check_config.sh` | `manager/` | 配置和环境检查脚本 |
| `Makefile` | 全部模块 | Docker 操作快捷命令（build/up/down/clean） |

## 目录结构

```
deploy/
├── README.md                      # 本文档
├── Dockerfile.kernel              # 内核基础镜像（Ubuntu 22.04，含编译依赖）
├── Dockerfile.service             # 服务层镜像（自包含构建，完整运行时）
├── Makefile                       # Docker 操作 Makefile（构建/启动/停止/清理）
├── build.sh                       # 镜像构建脚本（支持 dev/release 模式）
├── check_config.sh                # 配置文件和环境检查脚本
├── quickstart.sh                  # 一键快速入门（环境检查→镜像构建→服务启动）
├── docker-compose.yml             # 默认编排（开发环境）
├── docker-compose.preview.yml     # 预览环境编排
├── docker-compose.staging.yml     # 预发布环境编排
├── docker-compose.prod.yml        # 生产环境编排（高可用/安全加固/监控完备）
├── .env.example                   # 环境变量模板
├── .gitignore                     # Git 忽略规则
└── secrets/                       # 密钥目录
    └── .gitkeep                   #   保持目录结构
```

## 核心组件说明

### Dockerfile.kernel — 内核基础镜像

基于 Ubuntu 22.04 的内核基础镜像，包含以下内容：

- C/C++ 编译工具链（GCC、CMake、Make）
- `atoms/`、`commons/`、`cupolas/` 内核组件的编译产物
- 系统级依赖库（libc、libssl 等）
- 作为 `Dockerfile.service` 的构建基础，优化构建缓存

### Dockerfile.service — 服务层镜像

基于内核基础镜像的服务层镜像，包含以下内容：

- `daemon/` 下的所有守护进程（llm_d、sched_d、monit_d）
- `gateway/` API 网关服务
- `heapstore/` 运行时数据存储
- `manager/` 管理服务
- Python 和 Node.js 运行时
- 采用自包含构建模式，确保镜像可独立运行

### docker-compose.*.yml — 多环境编排

四种环境的 Docker Compose 编排配置：

| 文件 | 环境 | 特点 |
|------|------|------|
| `docker-compose.yml` | 开发 | 服务直接暴露端口，启用调试日志，无资源限制 |
| `docker-compose.preview.yml` | 预览 | 增加基本资源限制和健康检查，适合功能演示 |
| `docker-compose.staging.yml` | 预发布 | 配置与生产环境一致，用于最终验证 |
| `docker-compose.prod.yml` | 生产 | 高可用配置、安全加固、监控完备、资源限制严格 |

### build.sh — 镜像构建脚本

支持两种构建模式：

- `--dev`：开发模式，包含调试工具（gdb、strace、htop 等），镜像体积较大
- `--release`：发布模式，精简镜像，使用多阶段构建减小体积

### quickstart.sh — 一键快速入门

自动执行以下流程：

1. 环境检查（Docker 版本、磁盘空间、端口占用）
2. 镜像构建（默认 release 模式）
3. 服务启动（默认开发环境编排）

### check_config.sh — 配置检查

验证部署前置条件：

- Docker Engine 版本是否满足要求
- 磁盘空间是否充足
- 所需端口是否可用
- 环境变量是否正确配置

### Makefile — 快捷操作

提供以下快捷命令：

- `make build`：构建镜像
- `make up`：启动服务
- `make down`：停止服务
- `make clean`：清理镜像和容器
- `make logs`：查看服务日志

## 服务组件

| 服务 | 对应的 agentos/ 模块 | 说明 | 默认端口 |
|------|---------------------|------|---------|
| `gateway` | `gateway/` | API 网关，统一入口和路由 | 8080 |
| `llm_d` | `daemon/llm_d/` | LLM 服务守护进程，管理大语言模型推理 | 8081 |
| `sched_d` | `daemon/sched_d/` | 任务调度守护进程，负责任务分发和调度 | 8082 |
| `heapstore` | `heapstore/` | 运行时数据存储，持久化智能体状态 | 8083 |
| `monit_d` | `daemon/monit_d/` | 监控告警守护进程，系统健康监控 | 8084 |

## 使用方式

### 一键快速启动

```bash
cd scripts/ops/deploy
./quickstart.sh
```

### 手动构建和启动

```bash
# 构建镜像（发布模式）
./build.sh --release

# 构建镜像（开发模式，包含调试工具）
./build.sh --dev

# 启动开发环境
docker-compose up -d

# 启动预览环境
docker-compose -f docker-compose.preview.yml up -d

# 启动预发布环境
docker-compose -f docker-compose.staging.yml up -d

# 启动生产环境
docker-compose -f docker-compose.prod.yml up -d
```

### 使用 Makefile 快捷命令

```bash
# 构建镜像
make build

# 启动服务
make up

# 停止服务
make down

# 清理镜像和容器
make clean

# 查看服务日志
make logs
```

### 配置检查

```bash
# 检查部署前置条件
./check_config.sh
```

### 环境变量配置

```bash
# 复制环境变量模板
cp .env.example .env

# 编辑环境变量
vim .env
```

## 环境变量

| 变量 | 说明 | 默认值 |
|------|------|--------|
| `AGENTOS_ENV` | 运行环境（development/preview/staging/production） | `production` |
| `AGENTOS_LOG_LEVEL` | 日志级别（debug/info/warn/error） | `info` |
| `AGENTOS_CONFIG_DIR` | 配置目录 | `/etc/agentos` |
| `AGENTOS_DATA_DIR` | 数据目录 | `/var/lib/agentos` |
| `AGENTOS_SECRET_KEY` | 密钥（生产环境必须设置） | - |

## 生产优化

- 使用多阶段构建减小镜像体积
- 设置资源限制（CPU/Memory），防止单服务占用过多资源
- 配置健康检查探针，自动重启不健康的容器
- 日志挂载到宿主机持久化存储，避免容器重启导致日志丢失
- 使用命名卷持久化数据，确保数据安全
- 密钥通过 secrets 目录或 Docker Secrets 管理，不硬编码在镜像中

## 依赖说明

| 依赖 | 版本要求 | 用途 |
|------|---------|------|
| Docker Engine | 20.10+ | 容器运行时 |
| Docker Compose | v2 | 多容器编排 |
| Ubuntu | 22.04 | 基础镜像操作系统 |
| GCC | 11+ | C/C++ 编译器（内核镜像） |
| CMake | 3.20+ | 构建系统（内核镜像） |
| Python | 3.10+ | 服务层运行时 |
| Node.js | 18+ | 服务层运行时 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
