# AgentOS 构建规范文档

> **版本**: 1.1.0  
> **最后更新**: 2026-04-28  
> **适用版本**: v0.0.4+

---

## 📋 目录

- [核心原则](#核心原则)
- [目录结构](#目录结构)
- [构建方式](#构建方式)
- [清理规则](#清理规则)
- [常见问题](#常见问题)

---

## 核心原则

### 🎯 源码和产物分离

**AgentOS严格遵循"源码和产物分离"原则：**

1. ✅ **源码目录 (`AgentOS/`)** - 仅包含源代码、配置文件和脚本
2. ✅ **构建目录 (`AgentOS-build/`)** - 所有构建产物统一输出到此目录
3. ✅ **禁止在源码目录内进行任何构建操作**

### ❌ 禁止行为

```bash
# ❌ 错误：在源码目录内运行cmake
cd AgentOS
cmake .                     # 禁止！会产生build/目录
cmake -B build              # 禁止！
mkdir build && cd build     # 禁止！
```

### ✅ 正确行为

```bash
# ✅ 正确：使用外部构建目录
cd AgentOS-build            # 切换到构建目录
cmake ../AgentOS            # 指向源码目录
make -j$(nproc)             # 编译
```

---

## 目录结构

```
OpenAirymax/
├── AgentOS/                    ← 纯源码目录
│   ├── agentos/                # 核心代码
│   │   ├── atoms/              # 原子模块
│   │   ├── commons/            # 公共库
│   │   ├── daemon/             # 守护进程
│   │   ├── protocols/          # 协议栈
│   │   └── ...
│   ├── scripts/                # 构建脚本（按功能分类）
│   │   ├── setup/              # 环境配置脚本
│   │   ├── verify/             # 验证和扫描脚本
│   │   ├── release/            # 发布和清理脚本
│   │   └── cli/                # CLI工具
│   ├── tests/                  # 测试代码
│   └── [配置文件]               # CMakeLists.txt等
│
├── AgentOS-build/              ← 唯一构建输出目录（不纳入版本控制）
│   ├── CMakeCache.txt          # CMake缓存
│   ├── CMakeFiles/             # 构建临时文件
│   ├── Makefile                # 自动生成的Makefile
│   ├── bin/                    # 可执行文件
│   ├── lib/                    # 库文件
│   ├── agentos/                # 子项目构建产物
│   │   ├── atoms/
│   │   ├── commons/
│   │   └── ...
│   └── [其他构建产物]
│
└── Docs/                       ← 独立文档目录（独立仓库）
    └── Capital_API/            # API文档
```

---

## 构建方式

### 方式1: 首次构建（推荐）

```bash
# 设置项目根目录变量（根据实际路径修改）
export AGENTOS_ROOT="$(pwd)"        # 假设当前在 AgentOS 目录
export BUILD_DIR="../AgentOS-build"  # 或使用绝对路径

# 1. 创建构建目录（如不存在）
mkdir -p ${BUILD_DIR}

# 2. 进入构建目录
cd ${BUILD_DIR}

# 3. 配置CMake
cmake ${AGENTOS_ROOT} \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DBUILD_TESTING=ON \
    -DAGENTOS_ENABLE_EXAMPLES=ON

# 4. 编译
make -j$(nproc)

# 5. 运行测试
ctest --output-on-failure -j$(nproc)

# 6. 生成发行包
make package
```

**或者使用相对路径的简化版本：**

```bash
# 从 OpenAirymax 目录执行
mkdir -p AgentOS-build && cd AgentOS-build
cmake ../AgentOS -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
ctest --output-on-failure -j$(nproc)
```

### 方式2: 增量编译

```bash
# 直接进入已有构建目录
cd AgentOS-build

# 重新编译（仅编译变更部分）
make -j$(nproc)
```

### 方式3: 清理重建

```bash
# 完全清理构建目录（从 OpenAirymax 目录执行）
rm -rf AgentOS-build
mkdir AgentOS-build && cd AgentOS-build

# 重新配置和编译
cmake ../AgentOS -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### 方式4: Debug模式

```bash
cd AgentOS-build
cmake ../AgentOS \
    -DCMAKE_BUILD_TYPE=Debug \
    -DAGENTOS_SANITIZE=ON \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
make -j$(nproc)
```

---

## 清理规则

### 🗑️ 自动清理脚本

AgentOS 提供了自动清理脚本 `scripts/release/cleanup_builds.sh`：

```bash
# 从 AgentOS 源码目录执行
./scripts/release/cleanup_builds.sh
```

该脚本会：
- ✅ 清理所有 CMake 构建产物（CMakeCache.txt, CMakeFiles 等）
- ✅ 清理 build/, _build/ 等构建目录
- ✅ 清理 Python 缓存（__pycache__, *.pyc）
- ✅ 清理编译产物（*.so, *.so.*）
- ✅ 保留 AgentOS-build/ 目录（外部构建目录）

### 手动清理命令

```bash
# 在AgentOS源码目录中执行
cd <path-to-AgentOS>

# 查找并显示所有构建产物
find . -type d \( -name "build" -o -name "_build" -o -name "CMakeFiles" \) \
    -not -path "./.git/*" -not -path "*/node_modules/*"

# 删除所有CMake产物
find . -name "CMakeCache.txt" -delete
find . -type d -name "CMakeFiles" -exec rm -rf {} +
find . -name "compile_commands.json" -type l -delete
find . -name "CTestTestfile.cmake" -delete

# 删除Python缓存
find . -type d -name "__pycache__" -exec rm -rf {} +
find . -name "*.pyc" -delete
```

---

## .gitignore 规则

### 关键规则说明

AgentOS 的 `.gitignore` 已包含以下关键规则（版本号 9.0.0+）：

```gitignore
# CMake构建目录（所有层级）
build/
_build/
AgentOS-build/

# 子目录中的构建产物（关键！）
**/build/
**/_build/
**/CMakeCache.txt
**/CMakeFiles/
**/compile_commands.json

# Python缓存
**/__pycache__/
**/*.pyc

# Rust构建
**/target/

# TypeScript构建
**/node_modules/
**/dist/

# Visual Studio 构建输出（排除 scripts/release/）
[Dd]ebug/
[Rr]elease/
!scripts/release/
```

### 验证规则生效

```bash
# 在AgentOS源码目录中执行
cd <path-to-AgentOS>

# 检查文件是否被.gitignore排除
git check-ignore -v agentos/build
git check-ignore -v CMakeCache.txt
git check-ignore -v agentos/toolkit/rust/target

# 预期输出：显示匹配的规则编号
```

---

## 常见问题

### Q1: 为什么会在源码目录内产生构建产物？

**原因**：
- 直接在 AgentOS 目录内运行 `cmake .` 或 `cmake ..`
- IDE（如 CLion）自动在源码目录创建 cmake-build-* 目录
- 测试脚本错误配置构建路径

**解决**：
- 始终使用 `AgentOS-build` 目录
- 在 IDE 中配置 "Build directory" 为外部路径
- 修改测试脚本使用正确的构建路径

### Q2: 已有构建产物在源码目录，如何清理？

```bash
# 方法1: 使用清理脚本（推荐）
./scripts/release/cleanup_builds.sh

# 方法2: 手动清理（见上文"手动清理命令"）

# 方法3: 使用 git clean（小心使用！）
cd <path-to-AgentOS>
git clean -fdX    # 删除所有 .gitignore 匹配的文件/目录
```

### Q3: 如何防止 IDE 产生构建产物？

**CLion 配置**：
```
File → Settings → Build, Execution, Deployment → CMake
- Build directory: /path/to/AgentOS-build
- 取消勾选 "Reload CMake project on editing CMakeLists.txt"
```

**VS Code 配置**：
```json
// .vscode/settings.json
{
    "cmake.buildDirectory": "${workspaceFolder}/../AgentOS-build",
    "cmake.configureOnOpen": false
}
```

### Q4: 如何验证源码目录完全干净？

```bash
cd <path-to-AgentOS>

# 检查是否有未追踪的构建产物
git status --porcelain | grep -E "build|CMake|\.pyc|target"

# 显示所有构建相关目录
find . -type d \( -name "build" -o -name "_build" -o -name "CMakeFiles" \) \
    -not -path "./.git/*"

# 无输出表示源码目录完全干净 ✅
```

### Q5: 多个构建目录可以共存吗？

**可以**。推荐以下配置组合：

```
OpenAirymax/
├── AgentOS/                  ← 源码
├── AgentOS-build/           ← Release 构建
├── AgentOS-build-debug/     ← Debug 构建
└── AgentOS-build-coverage/  ← Coverage 构建
```

每个构建目录独立配置：
```bash
# Release 构建
mkdir AgentOS-build && cd AgentOS-build
cmake ../AgentOS -DCMAKE_BUILD_TYPE=Release

# Debug 构建
mkdir AgentOS-build-debug && cd AgentOS-build-debug
cmake ../AgentOS -DCMAKE_BUILD_TYPE=Debug

# Coverage 构建
mkdir AgentOS-build-coverage && cd AgentOS-build-coverage
cmake ../AgentOS -DCMAKE_BUILD_TYPE=Debug -DAGENTOS_COVERAGE=ON
```

---

## CI/CD 集成

### GitHub Actions 示例

```yaml
name: Build AgentOS

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Create build directory
      run: mkdir -p ../AgentOS-build
    
    - name: Configure CMake
      run: |
        cd ../AgentOS-build
        cmake $GITHUB_WORKSPACE/AgentOS \
          -DCMAKE_BUILD_TYPE=Release \
          -DBUILD_TESTING=ON
    
    - name: Build
      run: |
        cd ../AgentOS-build
        make -j$(nproc)
    
    - name: Test
      run: |
        cd ../AgentOS-build
        ctest --output-on-failure
```

### AtomGit/GitLab CI 示例

```yaml
# .gitlab-ci.yml 或 AtomGit CI 配置
stages:
  - build
  - test

variables:
  AGENTOS_ROOT: "$CI_PROJECT_DIR"
  BUILD_DIR: "$CI_PROJECT_DIR/../AgentOS-build"

build_agentos:
  stage: build
  image: ubuntu:22.04
  before_script:
    - apt-get update && apt-get install -y cmake g++ make python3
  script:
    - mkdir -p ${BUILD_DIR}
    - cd ${BUILD_DIR}
    - cmake ${AGENTOS_ROOT} -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
    - make -j$(nproc)
    - ctest --output-on-failure -j$(nproc)
  artifacts:
    paths:
      - ${BUILD_DIR}/bin/*
      - ${BUILD_DIR}/lib/*
    expire_in: 1 week
```

---

## 项目仓库信息

### 远程仓库地址

**AgentOS 主仓库：**
- 主仓库：https://atomgit.com/openairymax/agentos
- GitHub 镜像：https://github.com/SpharxTeam/AgentOS
- Gitee 镜像：https://gitee.com/SpharxTeam/agentos

**关联模块仓库：**
- Docs 文档模块：https://atomgit.com/openairymax/docs
- Docker 模块：https://atomgit.com/openairymax/docker
- Desktop 桌面客户端：https://atomgit.com/openairymax/desktop

**相关项目：**
- Workshop 工作坊：https://atomgit.com/spharx/workshop
- Deepness 深度学习：https://atomgit.com/spharx/deepness
- Benchmark 基准测试：https://atomgit.com/spharx/benchmark

---

## 维护指南

### 定期检查清单

- [ ] 验证 AgentOS 目录内无构建产物
- [ ] 检查 `.gitignore` 规则是否完整
- [ ] 确认所有开发者使用外部构建
- [ ] 更新本文档（如有新规则）

### 违规处理

如果发现开发者在源码目录内构建：

1. 立即停止构建
2. 清理产生的产物
3. 教育正确的构建方式
4. 在 PR review 中检查构建路径

### 文档更新日志

- **v1.1.0 (2026-04-28)**: 
  - 移除所有硬编码本地路径
  - 使用相对路径和环境变量
  - 添加 CI/CD 配置示例
  - 添加项目仓库信息
  - 更新 scripts/ 目录结构说明
  
- **v1.0.0 (2026-04-28)**: 
  - 初始版本
  - 定义源码和产物分离原则

---

**文档维护者**: AgentOS Build Team  
**最后更新**: 2026-04-28  
**适用版本**: v0.0.4+
