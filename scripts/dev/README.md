# 开发环境

`scripts/dev/`

## 概述

`dev/` 目录包含 AgentOS 项目的开发环境搭建和辅助工具，涵盖跨平台构建、安装、CMake 配置、CLI 入口及快速启动脚本。

## 目录结构

```
dev/
├── README.md
├── build/                    # 构建系统脚本
│   ├── build.sh              # 自动化构建 (Linux/macOS)
│   ├── install.sh            # 自动化安装 (Linux/macOS)
│   └── install.ps1           # 自动化安装 (Windows)
├── cmake/                    # CMake 辅助配置
│   └── windows_preinclude.h  # Windows MSVC 兼容性预包含头
├── setup/                    # 环境安装
│   ├── setup.sh              # 环境安装 (Linux/macOS)
│   └── setup.ps1             # 环境安装 (Windows)
├── cli/                      # CLI 入口
│   └── agentos               # 主 CLI 入口脚本 (Python)
├── config/                   # 开发环境配置模板
└── utils/                    # 开发辅助工具
    ├── quickstart.sh         # 快速启动脚本
    └── validate.sh           # 配置验证脚本

## 快速开始

### Linux/macOS

```bash
chmod +x scripts/dev/setup/setup.sh
./scripts/dev/setup/setup.sh
./scripts/dev/build/build.sh --release
```

### Windows

```powershell
.\scripts\dev\setup\setup.ps1
```