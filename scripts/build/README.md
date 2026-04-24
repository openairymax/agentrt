# 构建脚本

`scripts/build/`

## 概述

`build/` 目录包含 AgentOS 项目的所有构建和安装脚本，支持跨平台（Linux/macOS/Windows）自动化编译和部署，涵盖依赖安装、编译构建、产物输出等完整流程。

## 文件清单

| 文件 | 说明 | 平台 |
|------|------|------|
| `build.sh` | 自动化构建脚本 | Linux/macOS |
| `install.sh` | 自动化安装脚本 | Linux/macOS |
| `install.ps1` | 自动化安装脚本 | Windows |

## 快速开始

### Linux/macOS

```bash
# 进入构建目录
cd scripts/build

# 赋予执行权限
chmod +x install.sh build.sh

# 运行安装脚本
./install.sh

# 构建项目
./build.sh --release
```

### Windows PowerShell

```powershell
# 进入构建目录
cd scripts\build

# 允许执行脚本
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass

# 运行安装脚本
.\install.ps1

# 构建项目（需创建 build.ps1）
.\build.ps1 -Configuration Release
```

## 使用说明

### install.sh (Linux/macOS)

**功能**: 检查并安装系统依赖、配置编译环境、设置必要的环境变量

**选项**:
```bash
./install.sh [选项]

选项:
  --dev         安装开发依赖
  --prod        仅安装生产依赖（默认）
  --clean       清理旧的安装文件
  --help        显示帮助信息
```

### build.sh (Linux/macOS)

**功能**: 编译 C/C++ 内核模块、构建 Python/Go/Rust SDK、生成可执行文件和库

**选项**:
```bash
./build.sh [选项]

选项:
  --debug       构建调试版本
  --release     构建发布版本（默认）
  --clean       清理构建产物
  --test        运行测试套件
  --parallel=N  并行编译任务数（默认：CPU 核心数）
```

## 构建输出

### 目录结构

```
build/
├── agentos/atoms/                 # 内核编译产物
│   ├── corekern/         # 微内核库
│   ├── coreloopthree/    # 运行时库
│   └── memoryrovol/      # 记忆系统库
├── agentos/toolkit/                # 工具程序
│   ├── go/              # Go SDK
│   ├── python/          # Python SDK
│   └── rust/            # Rust SDK
└── tests/               # 测试二进制文件
```

### 产物类型

| 类型 | 位置 | 说明 |
|------|------|------|
| 静态库 | `build/lib/*.a` | 用于静态链接 |
| 动态库 | `build/lib/*.so` / `*.dylib` | 用于动态链接 |
| 可执行文件 | `build/bin/*` | 独立运行的程序 |
| 头文件 | `build/include/` | C/C++ 开发接口 |

## 故障排查

### 依赖缺失

```bash
# Ubuntu/Debian
sudo apt-get install cmake gcc g++ make

# macOS (Homebrew)
brew install cmake gcc make

# Windows (Chocolatey)
choco install cmake visualstudio2022buildtools
```

### 编译加速

```bash
# 使用 ccache
sudo apt-get install ccache  # Ubuntu/Debian
export CC="ccache gcc"
export CXX="ccache g++"

# 并行编译
./build.sh --parallel=$(nproc)
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
