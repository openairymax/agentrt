# AgentOS 部署脚本

**版本**: v1.0.0.6  
**最后更新**: 2026-03-21  

---

## 📋 概述

`scripts/deploy/` 目录包含 AgentOS 的生产环境部署工具，提供容器化、自动化和一键部署能力。

---

## 📁 目录结构

```
deploy/
└── docker/                 # Docker 容器化部署
    ├── README.md          # Docker 部署完整指南 ⭐
    ├── build.sh           # 镜像构建脚本
    ├── docker-compose.yml # 容器编排配置
    ├── Dockerfile.kernel  # 内核镜像
    └── Dockerfile.service # 服务镜像
```

---

## 🚀 快速开始

### Docker 部署（推荐）

```bash
# 进入 Docker 部署目录
cd scripts/deploy/docker

# 构建所有镜像（生产版本）
./build.sh all release

# 启动服务
docker-compose up -d

# 查看运行状态
docker-compose ps

# 查看日志
docker-compose logs -f
```

📖 **详细文档**: [deploy/docker/README.md](docker/README.md)

---

## 📊 部署方式对比

| 部署方式 | 适用场景 | 复杂度 | 资源需求 |
|---------|---------|--------|---------|
| **Docker** | 生产环境 | ⭐⭐⭐ | 中等 |
| **源码编译** | 开发/测试 | ⭐⭐⭐⭐ | 高 |
| **二进制包** | 快速部署 | ⭐⭐ | 低 |

---

## 🔧 其他部署工具

### 1. 系统级部署

```bash
# Ubuntu/Debian
sudo apt install agentos

# macOS (Homebrew)
brew install agentos

# Windows (Chocolatey)
choco install agentos
```

### 2. Kubernetes 部署

```bash
# 使用 Helm Chart
helm repo add agentos https://charts.agentos.org
helm install my-agentos agentos/agentos

# 或使用 kubectl
kubectl apply -f k8s/manifest.yaml
```

---

## 📝 最佳实践

### 生产环境检查清单

- [ ] 已更新所有默认密码
- [ ] 已配置 HTTPS/TLS
- [ ] 已启用日志轮转
- [ ] 已设置监控告警
- [ ] 已配置备份策略
- [ ] 已进行压力测试

### 回滚策略

```bash
# Docker 回滚到上一版本
docker-compose down
docker tag agentos:latest agentos:backup
docker tag agentos:previous latest
docker-compose up -d
```

---

## 📞 相关文档

- [主 README](../README.md) - 脚本总览
- [Docker 部署指南](docker/README.md) - 详细教程
- [运维手册](../ops/README.md) - 监控和维护

---

© 2026 SPHARX Ltd. All Rights Reserved.

*From data intelligence emerges.*  
*始于数据，终于智能。*
