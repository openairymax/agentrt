# 脚本工具集

`scripts/`

## 概述

`scripts/` 目录是 AgentOS 项目的脚本工具集合，涵盖环境配置、验证扫描、发布清理、CLI工具等全生命周期管理功能。所有脚本按功能分类存放，为开发者提供统一的命令行操作入口。

## 目录结构

```
scripts/
├── README.md                     # 目录说明文档
├── .editorconfig                 # 编辑器配置
│
├── setup/                        # 环境配置
│   ├── setup.sh                  # Linux/macOS 环境配置脚本
│   └── setup.ps1                 # Windows 环境配置脚本
│
├── verify/                       # 验证/检查
│   ├── verify_sdks.sh            # Linux SDK 构建验证
│   ├── verify_sdks.ps1           # Windows SDK 构建验证
│   └── sec017_scan.sh            # SEC-017 桩函数安全扫描
│
├── release/                      # 发布工具
│   ├── release.sh                # 一键发布脚本（质量门禁+版本验证）
│   └── cleanup_builds.sh         # 构建产物清理脚本
│
├── cli/                          # CLI工具
│   └── agentos                   # 统一命令行入口
│
└── [保留现有子目录]
    ├── core/              # 核心功能脚本
    ├── build/             # 构建和安装脚本
    ├── deployment/        # 部署配置脚本
    ├── development/       # 开发辅助脚本
    ├── library/           # 公共库脚本
    ├── tests/             # 测试脚本
    ├── tools/             # 运维工具
    ├── toolkit/           # 运维工具集
    ├── benchmark/         # 性能基准测试
    ├── demo/              # 演示示例脚本
    └── tutorial/          # 教程引导脚本
```

## 快速开始脚本

### 环境配置

```bash
# Linux/macOS 环境配置
./scripts/setup/setup.sh

# Windows 环境配置
.\scripts\setup\setup.ps1
```

### 验证与扫描

```bash
# SDK 构建验证（Linux）
./scripts/verify/verify_sdks.sh

# SDK 构建验证（Windows）
.\scripts\verify\verify_sdks.ps1

# SEC-017 安全扫描
./scripts/verify/sec017_scan.sh
```

### 发布与清理

```bash
# 一键发布（需要版本号）
./scripts/release/release.sh 1.0.0 stable

# 清理构建产物
./scripts/release/cleanup_builds.sh
```

### CLI 工具

```bash
# 运行 CLI 工具
./scripts/cli/agentos --help

# 查看服务状态
./scripts/cli/agentos service list

# 协议操作
./scripts/cli/agentos protocol list
./scripts/cli/agentos protocol test jsonrpc
```

## 子模块说明

| 模块 | 路径 | 说明 |
|------|------|------|
| 环境配置 | `setup/` | Linux/macOS/Windows 开发环境配置和依赖安装 |
| 验证扫描 | `verify/` | SDK构建验证、安全扫描、代码质量检查 |
| 发布工具 | `release/` | 一键发布、质量门禁、构建产物清理 |
| CLI工具 | `cli/` | 统一命令行入口，管理服务、代理、任务、协议等 |
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
# 环境配置
scripts/setup/setup.sh

# SDK 验证
scripts/verify/verify_sdks.sh

# 安全扫描
scripts/verify/sec017_scan.sh all

# 构建项目
scripts/build/build.sh --release

# 一键发布
scripts/release/release.sh 2.0.0 stable

# 清理构建产物
scripts/release/cleanup_builds.sh

# 运行测试
scripts/tests/run_tests.sh

# 部署到 Kubernetes
scripts/deployment/deploy.sh --env production

# 性能测试
scripts/benchmark/benchmark_core.py --rounds 100

# CLI 工具
scripts/cli/agentos service list
scripts/cli/agentos protocol test jsonrpc
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
