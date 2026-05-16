# 部署配置

`scripts/deployment/`

## 概述

`deployment/` 目录提供 AgentOS 的部署配置，当前仅包含 Docker 相关部署方案。所有实际部署内容位于 `docker/` 子目录中。

## 目录结构

```
deployment/
└── docker/               # Docker 部署配置
    ├── Dockerfile.kernel      # 内核服务镜像构建
    ├── Dockerfile.service     # 应用服务镜像构建
    ├── docker-compose.yml     # 开发环境编排
    ├── docker-compose.staging.yml  # 预发布环境编排
    ├── docker-compose.prod.yml     # 生产环境编排
    ├── docker-compose.preview.yml  # 预览环境编排
    ├── build.sh               # 镜像构建脚本
    ├── check_config.sh        # 配置检查脚本
    ├── quickstart.sh          # 快速启动脚本
    ├── Makefile               # 构建和管理快捷命令
    ├── .env.example           # 环境变量示例
    ├── .gitignore             # Git 忽略规则
    ├── secrets/               # 密钥目录（.gitkeep）
    └── README.md              # Docker 部署详细说明
```

## 使用方式

```bash
# 快速启动（开发环境）
cd scripts/deployment/docker
./quickstart.sh

# 构建镜像
./build.sh

# 配置检查
./check_config.sh

# 使用 docker-compose 启动
docker-compose up -d

# 生产环境部署
docker-compose -f docker-compose.prod.yml up -d
```

详细配置说明请参阅 [docker/README.md](docker/README.md)。

---

© 2026 SPHARX Ltd. All Rights Reserved.
