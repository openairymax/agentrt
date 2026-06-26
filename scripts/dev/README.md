# 开发环境与构建工具

`scripts/dev/`

## 概述

`dev/` 目录包含 AgentRT 项目的开发环境搭建和辅助工具，涵盖跨平台构建安装、环境配置、CLI 入口、CMake 辅助及快速启动脚本。该模块是开发者日常使用的核心工具集，从项目初始化到编译构建的全流程均可通过该模块的脚本完成。

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
| `cmake/Sanitizers.cmake` | `atoms/`, `commons/`, `cupolas/` | CMake Sanitizers 配置（ASan/MSan/UBSan） |
| `docs/Doxyfile` | 全部模块 | Doxygen 文档生成配置 |
| `utils/quickstart.sh` | 全部模块 | 一键快速启动脚本 |
| `utils/validate.sh` | 全部模块 | 环境完整性验证脚本 |
| `utils/run_all_fixes.sh` | 全部模块 | BAN 规则批量自动修复入口 |
| `utils/fixes/add_error_push_ex.py` | `commons/`, `daemon/` | 错误码推送扩展生成工具 |
| `utils/fixes/inject_error_push_ex.py` | `commons/`, `daemon/` | 错误码推送注入工具 |
| `utils/fixes/fix_strncpy.py` | `atoms/`, `commons/` | strncpy 安全修复工具 |
| `utils/fixes/fix_return_neg_N.py` | `atoms/`, `commons/` | 负返回值修复工具 |
| `utils/fixes/fix_error_push_ex_order.py` | `commons/`, `daemon/` | 错误码推送顺序修复工具 |
| `utils/fixes/fix_braces_and_codes.py` | `atoms/`, `commons/` | 大括号与错误码修复工具 |
| `utils/fixes/fix_agentos_efail_macro.py` | `atoms/`, `commons/` | AGENTOS_EFAIL 宏修复工具 |
| `utils/fixes/fix_agentos_efail.py` | `atoms/`, `commons/` | AGENTOS_EFAIL 修复工具 |
| `utils/fixes/fix_indent_and_codes.py` | `atoms/`, `commons/` | 缩进与错误码修复工具 |
| `utils/fixes/fix_includes_and_braces.py` | `atoms/`, `commons/` | 包含与大括号修复工具 |
| `utils/fixes/fix_error_handle.py` | `commons/`, `daemon/` | 错误处理修复工具 |
| `utils/fixes/check_memory_compat.py` | `atoms/`, `commons/` | 内存兼容性检查工具 |

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
├── cmake/                         # CMake 辅助配置（2 个文件）
│   ├── windows_preinclude.h       #   Windows MSVC 兼容性预包含头（WIN32_LEAN_AND_MEAN 等）
│   └── Sanitizers.cmake           #   CMake Sanitizers 配置（ASan/MSan/UBSan）
├── docs/                          # 文档生成（1 个文件）
│   └── Doxyfile                   #   Doxygen 文档生成配置
└── utils/                         # 开发辅助工具
    ├── quickstart.sh              #   一键快速启动脚本
    ├── validate.sh                #   环境完整性验证脚本
    ├── run_all_fixes.sh           #   BAN 规则批量自动修复入口
    ├── fixes/                     #   代码修复工具集（13 个文件）
    │   ├── add_error_push_ex.py       #   错误码推送扩展生成工具
    │   ├── inject_error_push_ex.py    #   错误码推送注入工具
    │   ├── fix_strncpy.py             #   strncpy 安全修复工具
    │   ├── fix_return_neg_N.py        #   负返回值修复工具
    │   ├── fix_error_push_ex_order.py #   错误码推送顺序修复工具
    │   ├── fix_braces_and_codes.py    #   大括号与错误码修复工具
    │   ├── fix_agentos_efail_macro.py #   AGENTOS_EFAIL 宏修复工具
    │   ├── fix_agentos_efail.py       #   AGENTOS_EFAIL 修复工具
    │   ├── fix_indent_and_codes.py    #   缩进与错误码修复工具
    │   ├── fix_includes_and_braces.py #   包含与大括号修复工具
    │   ├── fix_error_handle.py        #   错误处理修复工具
    │   ├── fix_sec22.py               #   SEC-22 安全规则修复工具
    │   └── check_memory_compat.py     #   内存兼容性检查工具
    └── archive/                   #   已完成的一次性脚本归档
        └── restructure_sdk.py     #   SDK 重构脚本（已完成）
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
- **Sanitizers.cmake**：CMake Sanitizers 配置模块，支持 AddressSanitizer（ASan）、MemorySanitizer（MSan）、UndefinedBehaviorSanitizer（UBSan）等编译器插桩工具，用于在开发和测试阶段检测内存错误和未定义行为。

### docs/ — 文档生成

- **Doxyfile**：Doxygen 文档生成配置文件，用于从 C/C++ 源码注释自动生成 API 参考文档。配置了输入源码路径、输出格式（HTML/LaTeX）和文档样式等参数。

### utils/ — 开发辅助工具

- **quickstart.sh**：一键快速启动脚本，自动完成环境检查→依赖安装→项目构建→服务启动的全流程，适合新开发者快速上手。
- **validate.sh**：环境完整性验证脚本，检查系统环境是否满足项目构建和运行的所有前置条件（编译器版本、CMake 版本、Python 版本、系统库等）。
- **run_all_fixes.sh**：BAN 规则批量自动修复入口，按依赖顺序执行 `fixes/` 下所有自动修复脚本，支持 `--dry-run` 和 `--verbose` 选项。
- **fixes/**：代码修复工具集，包含 BAN 规则自动修复脚本和错误码生成/注入/检查工具。由 `run_all_fixes.sh` 统一编排执行。
  - **add_error_push_ex.py**：错误码推送扩展生成工具，用于生成和注册新的错误码及其描述信息。
  - **inject_error_push_ex.py**：错误码推送注入工具，将错误码定义注入到源码文件中。
  - **fix_strncpy.py**：strncpy 安全修复工具，将不安全的 `strncpy` 调用替换为安全替代方案。
  - **fix_return_neg_N.py**：负返回值修复工具，修复返回负数常量的错误码模式。
  - **fix_error_push_ex_order.py**：错误码推送顺序修复工具，确保错误码推送语句的正确顺序。
  - **fix_braces_and_codes.py**：大括号与错误码修复工具，统一代码风格和错误码格式。
  - **fix_agentos_efail_macro.py**：AGENTOS_EFAIL 宏修复工具，规范化 AGENTOS_EFAIL 宏的使用方式。
  - **fix_agentos_efail.py**：AGENTOS_EFAIL 修复工具，修复 AGENTOS_EFAIL 调用的常见问题。
  - **fix_indent_and_codes.py**：缩进与错误码修复工具，统一代码缩进和错误码格式。
  - **fix_includes_and_braces.py**：包含与大括号修复工具，修复头文件包含顺序和大括号风格。
  - **fix_error_handle.py**：错误处理修复工具，统一错误处理模式。
  - **fix_sec22.py**：SEC-22 安全规则修复工具。
  - **check_memory_compat.py**：内存兼容性检查工具，检测内存相关 API 的兼容性问题。
- **archive/**：已完成的一次性脚本归档，保留以备历史查阅，不再活跃使用。
  - **restructure_sdk.py**：AgentRT Python SDK 重构脚本（已完成的一次性重构）。

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

# 批量执行 BAN 规则自动修复
bash scripts/dev/utils/run_all_fixes.sh --dry-run
```

### 错误码生成

```bash
# 生成新的错误码推送扩展
python scripts/dev/utils/fixes/add_error_push_ex.py
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
| `cmake/Sanitizers.cmake` | CMake 3.20+, Clang/GCC | Sanitizers 需要 Clang 或 GCC 编译器支持 |
| `docs/Doxyfile` | Doxygen 1.9+ | 文档生成需要安装 Doxygen 工具 |
| `utils/quickstart.sh` | Bash 4.0+, CMake, Python 3.8+ | 快速启动脚本依赖构建和运行时的全部工具 |
| `utils/validate.sh` | Bash 4.0+ | 验证脚本仅检查环境，不安装任何依赖 |
| `utils/run_all_fixes.sh` | Bash 4.0+, Python 3.8+ | 批量修复入口编排 `fixes/` 下的 Python 脚本 |
| `utils/fixes/*.py` | Python 3.8+ | 代码修复工具为纯 Python 实现 |
| `utils/archive/restructure_sdk.py` | Python 3.8+ | 已完成的一次性 SDK 重构脚本 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
