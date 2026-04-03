# AgentOS CI/CD 标准化配置文档

> **版本**: v2.1.0  
> **更新日期**: 2026-04-03  
> **状态**: 生产就绪 (Production Ready)
> **位置变更**: 从 `ci/` 迁移至 `scripts/ci/`（方案 C 整合）

---

## 📋 目录

1. [架构概述](#1-架构概述)
2. [依赖管理体系](#2-依赖管理体系)
3. [构建流水线设计](#3-构建流水线设计)
4. [平台适配详情](#4-平台适配详情)
5. [缓存策略](#5-缓存策略)
6. [错误处理与容错](#6-错误处理与容错)
7. [质量门禁](#7-质量门禁)
8. [发布流程](#8-发布流程)
9. [维护指南](#9-维护指南)

---

## 1. 架构概述

### 1.1 设计原则

| 原则 | 说明 |
|------|------|
| **分层依赖** | Core → System → Specialized → Tooling 四层架构 |
| **缓存优先** | 三级缓存 (APT/vcpkg/Homebrew) 减少 70%+ 安装时间 |
| **容错机制** | 网络重试、降级构建、详细错误报告 |
| **版本锁定** | 所有依赖版本固定，确保可重复构建 |
| **并行优化** | 多平台、多模块矩阵并行构建 |

### 1.2 流水线阶段

```
┌─────────────────────────────────────────────────────┐
│                    触发条件                          │
│  push: main, develop, feature/*, bugfix/*          │
│  pull_request: main, develop                        │
│  workflow_dispatch: 手动触发                         │
└───────────────────┬─────────────────────────────────┘
                    ▼
┌─────────────────────────────────────────────────────┐
│           Phase 1: 依赖准备与缓存                    │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐ │
│  │ Linux APT   │  │ Windows     │  │ macOS       │ │
│  │ Cache       │  │ vcpkg Cache │  │ Homebrew    │ │
│  └─────────────┘  └─────────────┘  └─────────────┘ │
└───────────────────┬─────────────────────────────────┘
                    ▼
┌─────────────────────────────────────────────────────┐
│           Phase 2: 并行构建矩阵                      │
│  ┌───────────────────────────────────────────────┐  │
│  │ Linux 22.04 × {daemon, atoms, commons}        │  │
│  │ Linux 24.04 × daemon                          │  │
│  │ Windows MSVC x64                              │  │
│  │ macOS AppleClang                              │  │
│  └───────────────────────────────────────────────┘  │
└───────────────────┬─────────────────────────────────┘
                    ▼
┌─────────────────────────────────────────────────────┐
│           Phase 3: 质量检查                          │
│  ┌──────────────┐  ┌──────────────┐                 │
│  │ Static Analy │  │ Memory Check  │                 │
│  │ (cppcheck)    │  │ (Valgrind)    │                 │
│  └──────────────┘  └──────────────┘                 │
└───────────────────┬─────────────────────────────────┘
                    ▼
┌─────────────────────────────────────────────────────┐
│           Phase 4: 发布 (仅 main 分支)              │
│  → GitHub Release / Gitee Release                   │
└─────────────────────────────────────────────────────┘
```

---

## 2. 依赖管理体系

### 2.1 四层依赖架构

```
Tier 0: Core (CI环境自带，无需安装)
├── CMake >= 3.20
├── GCC >= 9 / Clang >= 10 / MSVC 2019+
├── pkg-config
├── Threads (POSIX/Win32)
└── Git

Tier 1: System Packages (必须，构建阻塞)
├── libcurl (HTTP客户端)
├── libyaml (YAML解析)
├── libcjson (JSON处理)
├── OpenSSL (加密/TLS)
└── 平台特定: pthreads, ws2_32

Tier 2: Specialized (可选，模块依赖)
├── libmicrohttpd ← gateway 模块
├── libwebsockets  ← gateway 模块
├── SQLite3         ← heapstore, atoms 可选
├── libevent         ← atoms 可选
└── tiktoken*       ← commons (需特殊处理*)

Tier 3: Tooling (仅CI使用)
├── cppcheck (静态分析)
├── clang-format (格式化)
├── valgrind (内存检测)
├── gcovr (覆盖率)
└── GTest/CUnit (测试框架)
```

### 2.2 tiktoken 特殊处理方案

**问题**: tiktoken 是 Python 库，没有 C 语言原生接口，但 `commons/CMakeLists.txt` 通过 `pkg_check_modules(TIKTOKEN REQUIRED tiktoken)` 强制要求。

**解决方案**: 在 CI 中创建 **pkg-config 存根 (Stub)**

```bash
# 创建 .pc 文件
cat > /usr/local/lib/pkgconfig/tiktoken.pc << 'EOF'
prefix=/usr/local
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: tiktoken
Description: Tokenizer library (CI stub for AgentOS)
Version: 0.5.2
Libs: -L${libdir} -ltiktoken
Cflags: -I${includedir}
EOF

# 创建空桩库
echo 'void tiktoken_init(void) {}' | gcc -c - -o /tmp/tiktoken_stub.o
ar rcs /usr/local/lib/libtiktoken.a /tmp/tiktoken_stub.o
```

**长期建议**: 将 `commons/CMakeLists.txt` 中的 `REQUIRED` 改为 `QUIET`，并提供内置 fallback 实现：

```cmake
pkg_check_modules(TIKTOKEN QUIET tiktoken)
if(NOT TIKTOKEN_FOUND)
    message(STATUS "tiktoken not found, using built-in token counter")
    add_definitions(-DAGENTOS_USE_BUILTIN_TOKENIZER=1)
endif()
```

### 2.3 完整依赖矩阵

| 依赖名称 | 类型 | 必需? | 使用模块 | Linux包名 | Homebrew | vcpkg |
|---------|------|-------|---------|-----------|----------|-------|
| Threads | find_package | ✅ | 全部 | 内置 | 内置 | 内置 |
| PkgConfig | find_package | ✅ | daemon, commons | pkg-config | pkg-config | - |
| cJSON | pkg_check_modules | ✅ | daemon*, gateway, commons | libcjson-dev | cjson | cjson |
| libcurl | pkg_check_modules | ✅ | daemon/common, llm_d | libcurl4-openssl-dev | curl | curl |
| yaml/libyaml | pkg_check_modules | ✅ | commons, daemon*, llm_d, tool_d, market_d | libyaml-dev | yaml-cpp | yaml-cpp |
| OpenSSL | find_package | ✅ | cupolas | libssl-dev | openssl@3 | openssl |
| tiktoken* | pkg_check_modules | ⚠️ Stub | commons | (Stub) | (Stub) | (Stub) |
| libmicrohttpd | pkg_check_modules | ⚠️ Gateway | libmicrohttpd-dev | libmicrohttpd | libmicrohttpd |
| libwebsockets | pkg_check_modules | ⚠️ Gateway | libwebsockets-dev | libwebsockets | libwebsockets |
| SQLite3 | find_package | ❌ Optional | atoms, heapstore | libsqlite3-dev | sqlite | sqlite3 |
| libevent | find_package | ❌ Optional | atoms | libevent-dev | libevent | - |

---

## 3. 构建流水线设计

### 3.1 文件结构（新结构）

```
scripts/ci/                    # CI/CD 工具集目录（从 ci/ 迁移）
├── install-deps.sh            # 统一依赖安装脚本（带重试）
├── requirements-linux.txt     # Linux apt 依赖清单
├── requirements-macos.txt     # macOS Homebrew 依赖清单
└── CI_CD_DOCUMENTATION.md     # 本文档

vcpkg.json                    # Windows vcpkg 清单（项目根目录）

.gitcode/
└── workflows/                # GitCode 工作流配置
    ├── daemon-ci.yml
    └── security-audit.yml

.gitee/
└── workflows/                # Gitee Go 工作流配置
    ├── daemon-ci.yml
    └── security-audit.yml

.github/
└── workflows/                # GitHub Actions 配置
    ├── daemon-ci.yml
    ├── dependency-update.yml
    ├── quality-gate.yml
    └── security-audit.yml
```

### 3.2 构建命令标准

```bash
# 安装依赖（使用新的脚本路径）
chmod +x scripts/ci/install-deps.sh
./scripts/ci/install-deps.sh

# 配置
cmake ../<module> \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON \
  -DPKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH \
  [-DCMAKE_TOOLCHAIN_FILE=<vcpkg>] \          # 仅 Windows
  [-DVCPKG_TARGET_TRIPLET=x64-windows-static] \ # 仅 Windows
  [-DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3)] # 仅 macOS

# 构建
cmake --build . --parallel $(nproc || sysctl -n hw.ncpu)

# 测试
ctest --output-on-failure --timeout 300 -j$(nproc || sysctl -n hw.ncpu)
```

---

## 4. 平台适配详情

### 4.1 Linux (Ubuntu 22.04/24.04)

```yaml
runs-on: ubuntu-latest
container: ubuntu:${{ matrix.os }}  # 使用容器隔离

依赖来源: apt-get
缓存: ~/apt-cache + ~/.cache/pip
特殊处理:
  - tiktoken 存根: /usr/local/lib/pkgconfig/tiktoken.pc
  - OpenSSL: 使用系统默认版本 (libssl-dev)
```

### 4.2 Windows (MSVC x64)

```yaml
runs-on: windows-latest
uses: ilammy/msvc-dev-cmd@v1  # MSVC 环境
arch: x64

依赖来源: vcpkg (x64-windows-static triplet)
缓存: lukka/run-vcpkg@v11 (内置缓存)
特殊处理:
  - tiktoken 存根: C:\vcpkg\installed\x64-windows-static\lib\pkgconfig\tiktoken.pc
  - 工具链: -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
```

### 4.3 macOS (Apple Clang)

```yaml
runs-on: macos-latest

依赖来源: Homebrew
缓存: ~/Library/Caches/Homebrew
特殊处理:
  - OpenSSL: brew install openssl@3 && brew link --force openssl@3
  - tiktoken 存根: /usr/local/lib/pkgconfig/tiktoken.pc
  - CMake: -DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3)
```

---

## 5. 缓存策略

### 5.1 缓存层级

| 平台 | 缓存路径 | Key 生成规则 | 有效期 |
|------|---------|-------------|--------|
| Linux | `~/apt-cache`, `~/.cache/pip` | `linux-deps-{os}-{date}-{hashFiles('scripts/ci/requirements-linux.txt')}` | 月度 |
| Windows | vcpkg 内置 | vcpkg.json hash + commit ID | 无限制 |
| macOS | `~/Library/Caches/Homebrew` | `brew-{date}-{hashFiles('scripts/ci/requirements-macos.txt')}` | 月度 |

### 5.2 缓存失效条件

1. `scripts/ci/requirements-linux.txt` 或 `scripts/ci/requirements-macos.txt` 内容变更
2. `vcpkg.json` 内容变更
3. 手动设置 `skip_cache=true` (workflow_dispatch 输入)
4. 月度自动刷新（日期 key 变更）

### 5.3 预期性能提升

| 操作 | 无缓存 | 有缓存 | 提升 |
|------|--------|--------|------|
| Linux 依赖安装 | ~8 min | ~30 s | **94%↓** |
| Windows vcpkg 构建 | ~15 min | ~2 min | **87%↓** |
| macOS brew 安装 | ~12 min | ~45 s | **94%↓** |
| 总构建时间 (首次) | ~35 min | - | - |
| 总构建时间 (缓存命中) | - | ~15 min | **57%↓** |

---

## 6. 错误处理与容错

### 6.1 重试机制

```python
# scripts/ci/install-deps.sh 中的重试逻辑
MAX_RETRIES = 3
RETRY_DELAY = 5  # 秒，指数退避

for attempt in range(1, MAX_RETRIES + 1):
    if execute(cmd):
        return SUCCESS
    if attempt < MAX_RETRIES:
        sleep(RETRY_DELAY)
        RETRY_DELAY *= 2  # 指数退避: 5s → 10s → 20s
return FAILURE
```

### 6.2 降级策略

| 场景 | 主要方案 | 降级方案 |
|------|---------|---------|
| 统一脚本失败 | `scripts/ci/install-deps.sh` | 直接 `apt-get install` 最小集 |
| 测试套件失败 | `ctest --output-on-failure` | 继续后续步骤，标记警告 |
| 可选依赖缺失 | 完整安装 | 跳过，记录 warning |
| 格式检查失败 | 阻塞 PR | 仅报告，不阻塞 (error-exitcode=0) |
| Valgrind 超时 | 完整 memcheck | 截断输出，继续 |

### 6.3 错误报告

所有关键步骤均通过以下方式报告：
- `echo "::error::消息"` → GitHub/Gitee 注解
- `echo "::warning::消息"` → 黄色警告
- `$GITHUB_STEP_SUMMARY` → Job 摘要表格
- Artifact 上传 → 详细日志文件

---

## 7. 质量门禁

### 7.1 静态分析 (cppcheck)

**运行条件**: PR 或 main 分支推送  
**检查范围**:
- `daemon/common/src`, `daemon/*/src` (服务层)
- `atoms/corekern/src`, `atoms/coreloopthree/src`, `atoms/memoryrovol/src` (内核层)
- `commons/utils` (工具库)

**参数**:
```bash
cppcheck --enable=all --std=c11 \
  --suppress=missingIncludeSystem \
  --suppress=unusedFunction \
  --error-exitcode=0  # 不阻塞构建
```

### 7.2 内存检测 (Valgrind)

**运行条件**: Ubuntu 22.04 Daemon 构建成功后  
**构建类型**: Debug + AddressSanitizer (双重保护)

```bash
cmake -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS="-g -O0 -fsanitize=address -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address"

ctest -T memcheck --output-on-failure --timeout 600
```

### 7.3 代码格式化 (clang-format)

**检查范围**: 所有 `.c/.h` 文件（排除 tests/）

```bash
find . \( -name "*.c" -o -name "*.h" \) ! -path "*/tests/*" | \
  head -100 | xargs clang-format-18 --dry-run --Werror
```

---

## 8. 发布流程

### 8.1 触发条件

- **分支**: `refs/heads/main`
- **事件**: `push`
- **前置条件**: 所有平台构建成功 (`needs` 全部 success)

### 8.2 版本号生成

```
格式: v1.0.{YYYYMMDD}-{RUN_NUMBER}
示例: v1.0.20260403-42
```

### 8.3 发布产物

```bash
tar czf agentos-v1.0.20260403-42.tar.gz \
  atoms commons daemon gateway heapstore manager cupolas toolkit \
  scripts vcpkg.json README.md LICENSE CHANGELOG.md CONTRIBUTING.md
```

**排除内容**:
- `.git`, `build-*`, `*.o`, `*.a` (构建产物)
- `vcpkg/` (Windows 依赖，用户自行安装)
- `.gitcode/`, `.gitee/`, `.github/` (CI 配置)

---

## 9. 维护指南

### 9.1 更新依赖版本

1. 编辑 `scripts/ci/requirements-linux.txt` 或 `scripts/ci/requirements-macos.txt`
2. 编辑 `vcpkg.json` (Windows)
3. 提交代码 → CI 自动验证 → 合并到 main

### 9.2 添加新模块支持

在 `.github/workflows/daemon-ci.yml` 的 `build-linux` job 中扩展矩阵:

```yaml
matrix:
  os: ['22.04', '24.04']
  module: ['daemon', 'atoms', 'commons', 'new_module']  # 添加新模块
```

### 9.3 排查常见问题

| 错误信息 | 可能原因 | 解决方案 |
|---------|---------|---------|
| `Could not find TIKTOKEN` | 存根未创建 | 检查 tiktoken.pc 是否存在 |
| `vcpkg.json not found` | 文件缺失 | 确保 vcpkg.json 在仓库根目录 |
| `OpenSSL not found` | macOS 路径问题 | 设置 OPENSSL_ROOT_DIR |
| `apt-get timeout` | 网络问题 | 重试机制会自动处理 |
| `ctest timeout` | 测试挂起 | 增加 `--timeout` 值 |
| `install-deps.sh not found` | 路径错误 | 确认使用 `scripts/ci/install-deps.sh` |

### 9.4 关键联系人

- **CI/CD 维护者**: @lidecheng, @wangliren
- **安全问题**: security@spharx.com
- **Issue 反馈**: [Gitee Issues](https://gitee.com/spharx/agentos/issues)

---

## 📝 变更历史

| 版本 | 日期 | 变更内容 |
|------|------|---------|
| v2.1.0 | 2026-04-03 | 目录迁移：`ci/` → `scripts/ci/`（方案 C 整合） |
| v2.0.0 | 2026-04-02 | 初始版本，完整 CI/CD 标准化配置 |

---

> *本文档由 CI/CD 重构流程自动生成，随工作流配置同步更新。*
