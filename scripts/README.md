# 脚本工具集

`scripts/`

## 概述

`scripts/` 目录是 AgentOS 项目的脚本工具集合，涵盖构建部署、开发调试、性能测试、系统诊断、运维监控等全生命周期管理功能。所有脚本按功能分类存放，为开发者提供统一的命令行操作入口。

## 目录结构

```
scripts/
├── core/              # 核心功能脚本
├── build/             # 构建和安装脚本
├── deployment/        # 部署配置脚本
│   └── docker/        # Docker 容器化部署
├── development/       # 开发辅助脚本
├── library/           # 公共库脚本
├── tests/             # 测试脚本
├── tools/             # 运维工具
├── toolkit/           # 运维工具集
├── benchmark/         # 性能基准测试
├── demo/              # 演示示例脚本
└── tutorial/          # 教程引导脚本
```

## 子模块说明

| 模块 | 路径 | 说明 |
|------|------|------|
| 核心脚本 | `core/` | 系统初始化、进程管理、日志轮转等核心运维操作 |
| 构建脚本 | `build/` | 跨平台（Linux/macOS/Windows）自动化编译和安装 |
| 部署脚本 | `deployment/` | Kubernetes、Docker Compose 等部署配置管理 |
| 开发脚本 | `development/` | 代码格式化、静态检查、依赖管理、开发环境配置 |
| 公共库 | `library/` | 各脚本共享的函数库和工具函数 |
| 测试脚本 | `tests/` | 功能测试、集成测试、E2E 测试脚本 |
| 运维工具 | `tools/` | 日志分析、性能诊断、配置管理等日常运维 |
| 运维工具集 | `toolkit/` | 系统诊断、性能测试、内存管理、Token 统计等 |
| 基准测试 | `benchmark/` | 核心组件性能基准测试框架 |
| 演示脚本 | `demo/` | 快速展示 AgentOS 各项功能的示例脚本 |
| 教程脚本 | `tutorial/` | 引导开发者逐步学习和使用 AgentOS |

## 使用方式

```bash
# 构建项目
scripts/build/build.sh --release

# 运行测试
scripts/tests/run_tests.sh

# 部署到 Kubernetes
scripts/deployment/deploy.sh --env production

# 性能测试
scripts/benchmark/benchmark_core.py --rounds 100
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
