# 脚本工具集

`scripts/`

## 概述

`scripts/` 目录是 AgentOS 项目的脚本工具集合，涵盖环境配置、构建安装、代码质量分析、部署管理、性能基准测试、CI/CD 流水线等全生命周期管理功能。所有脚本按功能分类存放，为开发者提供统一的命令行操作入口。

> **注意**：所有构建脚本遵循 BAN-33 规则（禁止源内构建 / No in-source builds），构建产物必须输出到源码目录之外的独立构建目录中。

## 目录结构

```
scripts/
├── assets/           # 项目媒体资源（图片、GIF）
├── benchmark/        # 性能基准测试工具
├── build/            # 构建和安装脚本
├── cli/              # AgentOS 统一 CLI 工具
├── cmake/            # CMake 构建配置（Windows preinclude）
├── code-quality/     # 代码质量工具
├── demo/             # 技术演示脚本
├── deployment/       # 部署配置（Docker）
├── development/      # 开发环境配置
├── library/          # Shell 脚本公共库
├── pipeline/         # CI/CD 流水线编排
├── release/          # 发布管理脚本
├── setup/            # 环境配置脚本
├── tests/            # 脚本测试套件
├── toolkit/          # 统一工具集（含原 core/ 功能）
├── tutorial/         # 教程引擎
└── verify/           # 验证和扫描脚本
```

## 子模块说明

| 模块 | 路径 | 说明 |
|------|------|------|
| 媒体资源 | `assets/` | 项目图片和 GIF 动画资源 |
| 基准测试 | `benchmark/` | 核心组件性能基准测试框架 |
| 构建脚本 | `build/` | 跨平台（Linux/macOS/Windows）自动化编译和安装 |
| CLI 工具 | `cli/` | 统一命令行入口 agentos |
| CMake 配置 | `cmake/` | CMake 构建辅助文件 |
| 代码质量 | `code-quality/` | 代码质量工具 |
| 演示脚本 | `demo/` | AgentOS 技术功能演示 |
| 部署配置 | `deployment/` | Docker Compose 部署配置 |
| 开发环境 | `development/` | 开发环境快速配置和代码格式化 |
| 公共库 | `library/` | Shell 脚本共享函数库 |
| 流水线 | `pipeline/` | CI/CD 流水线编排和质量门禁 |
| 发布工具 | `release/` | 一键发布、构建产物清理 |
| 环境配置 | `setup/` | Linux/macOS/Windows 开发环境配置 |
| 测试套件 | `tests/` | Python 和 Shell 脚本测试 |
| 工具集 | `toolkit/` | 统一工具集（含原 core/ 功能） |
| 教程引擎 | `tutorial/` | 引导开发者学习 AgentOS 的教程系统 |
| 验证扫描 | `verify/` | SDK 构建验证、安全扫描 |

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
scripts/release/release.sh 0.0.5 stable

# 清理构建产物
scripts/release/cleanup_builds.sh

# 运行测试
pytest scripts/tests/python/

# 性能测试
python scripts/benchmark/benchmark_core.py --rounds 100

# CLI 工具
scripts/cli/agentos --help
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
