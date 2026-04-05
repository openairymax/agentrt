# AgentOS 构建脚本

**版本**: v1.0.0.6  
**最后更新**: 2026-03-21  

---

## 📋 概述

`scripts/build/` 目录包含 AgentOS 项目的所有构建和安装脚本，支持跨平台（Linux/macOS/Windows）自动化编译和部署。

---

## 📁 文件清单

| 文件 | 说明 | 平台 | 状态 |
|------|------|------|------|
| `build.sh` | 自动化构建脚本 | Linux/macOS | ✅ 生产就绪 |
| `install.sh` | 自动化安装脚本 | Linux/macOS | ✅ 生产就绪 |
| `install.ps1` | 自动化安装脚本 | Windows | ✅ 生产就绪 |

---

## 🚀 快速开始

### Linux/macOS

```bash
# 1. 进入构建目录
cd scripts/build

# 2. 赋予执行权限
chmod +x install.sh build.sh

# 3. 运行安装脚本
./install.sh

# 4. 构建项目
./build.sh --release
```

### Windows PowerShell

```powershell
# 1. 进入构建目录
cd scripts\build

# 2. 允许执行脚本
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass

# 3. 运行安装脚本
.\install.ps1

# 4. 构建项目
.\build.ps1 -Configuration Release
```

---

## 🔧 使用说明

### install.sh (Linux/macOS)

**功能**:
- 检查并安装系统依赖
- 配置编译环境
- 设置必要的环境变量

**选项**:
```bash
./install.sh [选项]

选项:
  --dev         安装开发依赖
  --prod        仅安装生产依赖（默认）
  --clean       清理旧的安装文件
  --help        显示帮助信息
```

**示例**:
```bash
# 完整安装（包含开发工具）
./install.sh --dev

# 最小化安装（仅生产环境）
./install.sh --prod

# 清理并重新安装
./install.sh --clean
```

### build.sh (Linux/macOS)

**功能**:
- 编译 C/C++ 内核模块
- 构建 Python/Go/Rust SDK
- 生成可执行文件和库

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

**示例**:
```bash
# 构建发布版本
./build.sh --release

# 构建调试版本并运行测试
./build.sh --debug --test

# 使用 4 个并行任务编译
./build.sh --parallel=4

# 清理所有构建产物
./build.sh --clean
```

### install.ps1 (Windows)

**功能**:
- 检查 PowerShell 版本
- 安装必要的 Windows 特性
- 下载并配置依赖项

**用法**:
```powershell
.\install.ps1 [-Dev] [-Clean]

参数:
  -Dev    安装开发工具
  -Clean  清理旧安装
```

**示例**:
```powershell
# 标准安装
.\install.ps1

# 安装开发环境
.\install.ps1 -Dev

# 清理并重新安装
.\install.ps1 -Clean
```

---

## 📊 构建输出

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
| **静态库** | `build/lib/*.a` | 用于静态链接 |
| **动态库** | `build/lib/*.so` / `*.dylib` | 用于动态链接 |
| **可执行文件** | `build/bin/*` | 独立运行的程序 |
| **头文件** | `build/include/` | C/C++ 开发接口 |

---

## 🔍 故障排查

### 常见问题

#### 1. 权限不足 (Linux/macOS)

**错误**: `Permission denied`

**解决**:
```bash
chmod +x install.sh build.sh
./install.sh
```

#### 2. 依赖缺失

**错误**: `CMake not found` 或 `gcc/g++ missing`

**解决**:
```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install cmake gcc g++ make

# macOS (Homebrew)
brew install cmake gcc make

# Windows (Chocolatey)
choco install cmake visualstudio2022buildtools
```

#### 3. PowerShell 执行策略限制 (Windows)

**错误**: `cannot be loaded because running scripts is disabled on this system`

**解决**:
```powershell
# 临时允许当前会话
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass

# 或永久允许签名脚本
Set-ExecutionPolicy -Scope CurrentUser -ExecutionPolicy RemoteSigned
```

#### 4. 内存不足导致编译失败

**错误**: `out of memory` 或 `killed`

**解决**:
```bash
# 减少并行编译任务数
./build.sh --parallel=2

# 增加 swap 空间（Linux）
sudo fallocate -l 4G /swapfile
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile
```

---

## 📈 性能优化

### 编译加速技巧

1. **使用 ccache** (重复编译加速):
```bash
# 安装 ccache
sudo apt-get install ccache  # Ubuntu/Debian
brew install ccache          # macOS

# 启用 ccache
export CC="ccache gcc"
export CXX="ccache g++"
./build.sh
```

2. **并行编译**:
```bash
# 使用所有 CPU 核心
./build.sh --parallel=$(nproc)

# 或指定核心数
./build.sh --parallel=8
```

3. **增量编译**:
```bash
# 只编译修改过的文件（默认行为）
./build.sh

# 强制全量编译
./build.sh --clean
```

---

## 🛡️ 安全建议

### 构建环境安全

1. **隔离构建环境**:
   - 使用 Docker 容器进行构建
   - 或使用虚拟机隔离

2. **验证依赖完整性**:
```bash
# 检查依赖哈希值
sha256sum dependencies.tar.gz
```

3. **代码签名**:
```bash
# 对生成的二进制文件签名（macOS）
codesign -s "Developer ID" build/bin/agentos

# 或使用 GPG 签名
gpg --detach-sign build/bin/agentos
```

---

## 📝 最佳实践

### CI/CD 集成

#### GitHub Actions 示例

```yaml
name: Build and Test

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Install Dependencies
      run: |
        sudo apt-get update
        sudo apt-get install cmake gcc g++ make
    
    - name: Build
      run: |
        cd scripts/build
        ./build.sh --release --test
    
    - name: Upload Artifacts
      uses: actions/upload-artifact@v3
      with:
        name: agentos-build
        path: build/
```

### 生产环境部署

1. **使用固定版本**:
```bash
# 不要直接使用 main 分支
git checkout v1.0.0.6
./build.sh --release
```

2. **验证构建产物**:
```bash
# 运行完整性检查
./build.sh --verify

# 运行安全扫描
./build.sh --security-scan
```

3. **备份构建配置**:
```bash
# 保存当前配置
cp build/config.h /backup/config_$(date +%Y%m%d).h
```

---

## 📞 相关文档

- [主 README](../README.md) - 脚本总览
- [Docker 部署](../deploy/docker/README.md) - 容器化部署
- [运维指南](../ops/README.md) - 性能测试和健康检查

---

## 🤝 贡献

欢迎提交 Issue 和 Pull Request 来改进这些构建脚本！

**Issue 追踪**: https://github.com/SpharxTeam/AgentOS/issues  
**讨论区**: https://github.com/SpharxTeam/AgentOS/discussions

---

© 2026 SPHARX Ltd. All Rights Reserved.

*From data intelligence emerges.*  
*始于数据，终于智能。*
