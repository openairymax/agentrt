# 开发环境与构建工具

`scripts/dev/`

## 概述

`dev/` 目录包含 AgentOS 项目的开发环境搭建和辅助工具，共 **10 个文件**，涵盖跨平台构建安装、环境配置、CLI 入口、CMake 辅助及快速启动脚本。

> **版本**：v0.1.0
> **平台**：Linux / macOS / Windows (PowerShell)

## 与 agentos/ 模块对应关系

| scripts/dev/ 模块 | 支持的 agentos/ 模块 | 用途 |
|-------------------|---------------------|------|
| `build/` | `atoms/`, `commons/`, `cupolas/`, `daemon/`, `gateway/`, `heapstore/` | 跨平台自动化构建（BAN-33 源外构建） |
| `setup/` | 全部模块 | 交互式开发环境配置（依赖安装、工具链设置） |
| `cli/agentos` | `daemon/`, `manager/`, `openlab/` | 统一 CLI 命令行入口（服务管理/智能体管理/任务管理） |
| `cmake/` | `atoms/`, `commons/`, `cupolas/` | CMake 辅助配置（Windows MSVC 兼容性头） |
| `utils/` | 全部模块 | 快速启动与环境验证工具 |

## 目录结构

```
dev/                            # 共 10 个文件
├── build/                      # 构建系统脚本（3 个文件）
│   ├── build.sh                # 跨平台自动化构建（Linux/macOS，BAN-33 源外构建）
│   ├── install.sh              # 自动化安装（Linux/macOS）
│   └── install.ps1             # 自动化安装（Windows PowerShell）
├── setup/                      # 环境配置（2 个文件）
│   ├── setup.sh                # 交互式开发环境配置（Linux/macOS）
│   └── setup.ps1               # 开发环境配置（Windows PowerShell）
├── cli/                        # CLI 入口
│   └── agentos                 # 统一 CLI 命令行入口（服务管理/智能体管理/任务管理）
├── cmake/                      # CMake 辅助配置
│   └── windows_preinclude.h    # Windows MSVC 兼容性预包含头（WIN32_LEAN_AND_MEAN 等）
├── config/                     # 开发环境配置模板（待补充）
└── utils/                      # 开发辅助工具（2 个文件）
    ├── quickstart.sh           # 一键快速启动脚本
    └── validate.sh             # 环境完整性验证脚本
```

## 快速开始

### Linux/macOS

```bash
# 环境配置（交互式菜单）
chmod +x scripts/dev/setup/setup.sh
./scripts/dev/setup/setup.sh

# 构建（BAN-33 源外构建）
./scripts/dev/build/build.sh --release

# 安装
./scripts/dev/build/install.sh
```

### Windows

```powershell
# 环境配置
.\scripts\dev\setup\setup.ps1

# 安装
.\scripts\dev\build\install.ps1
```

### CLI 使用

```bash
# 服务管理
scripts/dev/cli/agentos service start

# 智能体管理
scripts/dev/cli/agentos agent list

# 查看帮助
scripts/dev/cli/agentos --help
```

---

© 2026 SPHARX Ltd. All Rights Reserved.