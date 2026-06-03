# 开发环境与构建工具

`scripts/dev/`

## 概述

`dev/` 目录包含 AgentOS 项目的开发环境搭建和辅助工具，涵盖跨平台构建安装、环境配置、CLI 入口、CMake 辅助及快速启动脚本。该模块是开发者日常使用的核心工具集，从项目初始化到编译构建的全流程均可通过该模块的脚本完成。

开发工具集遵循以下设计原则：

- **BAN-33 源外构建**：所有构建脚本严格遵循 BAN-33 规则，构建产物输出到独立构建目录，不污染源码树
- **三平台覆盖**：核心脚本同时提供 Bash（Linux/macOS）和 PowerShell（Windows）两种实现
- **交互式引导**：环境配置脚本采用交互式菜单，自动检测系统环境并给出最优配置建议
- **统一 CLI 入口**：`agentos` 命令行工具提供服务管理、智能体管理、任务管理等一站式操作

> **版本**：v0.1.0
> **平台**：Linux / macOS / Windows (PowerShell)

## 与 agentos/ 模块对应关系

| scripts/dev/ 模块 | 支持的 agentos/ 模块 | 用途 |
|-------------------|---------------------|------|
| `build/build.sh` | `atoms/`, `commons/`, `cupolas/`, `daemon/`, `gateway/`, `heapstore/` | 跨平台自动化构建（BAN-33 源外构建） |
| `build/install.sh` | `atoms/`, `commons/`, `cupolas/`, `daemon/`, `gateway/`, `heapstore/` | 自动化安装（Linux/macOS） |
| `build/install.ps1` | `atoms/`, `commons/`, `cupolas/`, `daemon/`, `gateway/`, `heapstore/` | 自动化安装（Windows） |
| `setup/setup.sh` | 全部模块 | 交互式开发环境配置（Linux/macOS） |
| `setup/setup.ps1` | 全部模块 | 开发环境配置（Windows PowerShell） |
| `cli/agentos` | `daemon/`, `manager/`, `openlab/` | 统一 CLI 命令行入口（服务管理/智能体管理/任务管理） |
| `cmake/windows_preinclude.h` | `atoms/`, `commons/`, `cupolas/` | CMake 辅助配置（Windows MSVC 兼容性头） |
| `utils/quickstart.sh` | 全部模块 | 一键快速启动脚本 |
| `utils/validate.sh` | 全部模块 | 环境完整性验证脚本 |
| `utils/add_error_push_ex.py` | `commons/`, `daemon/` | 错误码推送扩展生成工具 |

## 目录结构

```
dev/
├── README.md                      # 本文档
├── build/                         # 构建系统脚本（3 个文件）
│   ├── build.sh                   #   跨平台自动化构建（Linux/macOS，BAN-33 源外构建）
│   ├── install.sh                 #   自动化安装（Linux/macOS）
│   └── install.ps1                #   自动化安装（Windows PowerShell）
├── setup/                         # 环境配置（2 个文件）
│   ├── setup.sh                   #   交互式开发环境配置（Linux/macOS）
│   └── setup.ps1                  #   开发环境配置（Windows PowerShell）
├── cli/                           # CLI 入口
│   └── agentos                    #   统一 CLI 命令行入口（服务管理/智能体管理/任务管理）
├── cmake/                         # CMake 辅助配置
│   └── windows_preinclude.h       #   Windows MSVC 兼容性预包含头（WIN32_LEAN_AND_MEAN 等）
└── utils/                         # 开发辅助工具（3 个文件）
    ├── quickstart.sh              #   一键快速启动脚本
    ├── validate.sh                #   环境完整性验证脚本
    └── add_error_push_ex.py       #   错误码推送扩展生成工具
```

## 核心组件说明

### build/ — 构建系统脚本

跨平台构建和安装的核心脚本：

- **build.sh**：跨平台自动化构建脚本，严格遵循 BAN-33 源外构建规范。支持 `--release`（优化构建）和 `--debug`（调试构建）两种模式，自动检测系统上的 CMake 和编译器（GCC/Clang），在独立的构建目录中执行编译，确保源码树不被构建产物污染。
- **install.sh**：Linux/macOS 自动化安装脚本，将构建产物安装到系统指定路径，支持自定义安装前缀。
- **install.ps1**：Windows PowerShell 自动化安装脚本，功能与 `install.sh` 对等，适配 Windows 环境的路径和权限模型。

### setup/ — 环境配置

交互式开发环境配置工具：

- **setup.sh**：Linux/macOS 交互式开发环境配置脚本，提供菜单式操作界面，自动检测系统环境（操作系统版本、已安装工具、编译器版本等），引导用户安装缺失的依赖项和配置工具链。
- **setup.ps1**：Windows PowerShell 环境配置脚本，功能与 `setup.sh` 对等，适配 Windows 环境的包管理器（Chocolatey/Scoop）和 Visual Studio 工具链。

### cli/ — CLI 入口

- **agentos**：统一 CLI 命令行入口，提供以下子命令：
  - `service`：服务管理（start/stop/restart/status）
  - `agent`：智能体管理（list/create/delete/configure）
  - `task`：任务管理（submit/cancel/status/list）
  - `--help`：查看帮助信息

### cmake/ — CMake 辅助配置

- **windows_preinclude.h**：Windows MSVC 兼容性预包含头文件，定义 `WIN32_LEAN_AND_MEAN`、`NOMINMAX` 等宏，减少 Windows.h 的编译开销和命名冲突。在 CMake 构建系统中通过 `/FI` 编译选项强制预包含此头文件。

### utils/ — 开发辅助工具

- **quickstart.sh**：一键快速启动脚本，自动完成环境检查→依赖安装→项目构建→服务启动的全流程，适合新开发者快速上手。
- **validate.sh**：环境完整性验证脚本，检查系统环境是否满足项目构建和运行的所有前置条件（编译器版本、CMake 版本、Python 版本、系统库等）。
- **add_error_push_ex.py**：错误码推送扩展生成工具，用于生成和注册新的错误码及其描述信息。

## 使用方式

### 开发环境配置

```bash
# Linux/macOS 交互式环境配置
chmod +x scripts/dev/setup/setup.sh
./scripts/dev/setup/setup.sh

# Windows PowerShell 环境配置
.\scripts\dev\setup\setup.ps1
```

### 项目构建

```bash
# BAN-33 源外构建（Release 模式）
./scripts/dev/build/build.sh --release

# BAN-33 源外构建（Debug 模式）
./scripts/dev/build/build.sh --debug

# 查看构建选项
./scripts/dev/build/build.sh --help
```

### 项目安装

```bash
# Linux/macOS 安装
./scripts/dev/build/install.sh

# Windows PowerShell 安装
.\scripts\dev\build\install.ps1
```

### CLI 工具

```bash
# 查看帮助
scripts/dev/cli/agentos --help

# 服务管理
scripts/dev/cli/agentos service start
scripts/dev/cli/agentos service stop
scripts/dev/cli/agentos service restart
scripts/dev/cli/agentos service status

# 智能体管理
scripts/dev/cli/agentos agent list
scripts/dev/cli/agentos agent create --name my-agent

# 任务管理
scripts/dev/cli/agentos task submit --agent my-agent
scripts/dev/cli/agentos task list
```

### 快速启动与验证

```bash
# 一键快速启动（环境检查→依赖安装→项目构建→服务启动）
./scripts/dev/utils/quickstart.sh

# 环境完整性验证
./scripts/dev/utils/validate.sh
```

### 错误码生成

```bash
# 生成新的错误码推送扩展
python scripts/dev/utils/add_error_push_ex.py
```

## 依赖说明

| 子模块 | 核心依赖 | 说明 |
|--------|---------|------|
| `build/build.sh` | Bash 4.0+, CMake 3.20+, GCC 11+/Clang 14+ | 构建脚本依赖 CMake 和 C/C++ 编译器 |
| `build/install.sh` | Bash, CMake | 安装脚本依赖 CMake 的 install 目标 |
| `build/install.ps1` | PowerShell 5.1+, CMake 3.20+, MSVC 2019+ | Windows 安装依赖 Visual Studio 工具链 |
| `setup/setup.sh` | Bash 4.0+ | 环境配置脚本自动检测并安装所需依赖 |
| `setup/setup.ps1` | PowerShell 5.1+ | Windows 环境配置依赖 Chocolatey 或 Scoop 包管理器 |
| `cli/agentos` | Bash 4.0+ | CLI 工具为纯 Shell 脚本实现 |
| `cmake/windows_preinclude.h` | MSVC 2019+ | 预包含头仅在使用 MSVC 编译器时生效 |
| `utils/quickstart.sh` | Bash 4.0+, CMake, Python 3.8+ | 快速启动脚本依赖构建和运行时的全部工具 |
| `utils/validate.sh` | Bash 4.0+ | 验证脚本仅检查环境，不安装任何依赖 |
| `utils/add_error_push_ex.py` | Python 3.8+ | 错误码生成工具为纯 Python 实现 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
