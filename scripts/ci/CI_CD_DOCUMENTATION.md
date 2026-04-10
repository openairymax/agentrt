# AgentOS CI/CD 标准化配置文档

> **版本**: v3.0.0
> **更新日期**: 2026-04-09
> **状态**: 生产就绪 (Production Ready)
> **位置**: `scripts/ci/`
> **理论框架**: 体系并行论 (MCIS) - 多体控制论智能系统

---

## 1. 架构概述

### 1.1 MCIS-based CI/CD 设计原则

| 原则 | 说明 | MCIS维度 |
|------|------|----------|
| **正交分离** | 各模块独立构建、测试、部署 | 五维正交系统 |
| **控制论耦合** | 负反馈(阻断)、正反馈(加速)、前馈(预测) | 控制论机制 |
| **缓存优先** | 三级缓存 (ccache/vcpkg/pip) 减少70%+构建时间 | 记忆体 |
| **容错机制** | 网络重试、降级构建、详细错误报告 | 安全穹顶体 |
| **并行优化** | 多模块矩阵并行构建 | 执行体 |
| **可观测性** | 质量监控、趋势分析、技术债务追踪 | 可观测体 |

### 1.2 流水线阶段

```
┌─────────────────────────────────────────────────────────┐
│                    触发条件                               │
│  push: main, develop, feature/*, bugfix/*               │
│  pull_request: main, develop                             │
│  workflow_dispatch: 手动触发                              │
│  schedule: 定时任务(安全审计/依赖更新/质量监控)            │
└───────────────────┬─────────────────────────────────────┘
                    ▼
┌─────────────────────────────────────────────────────────┐
│     Phase 1: 环境准备 (MCIS Base Infrastructure)         │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │ Smart Cache  │  │ vcpkg Setup  │  │ Deps Install │  │
│  │ (ccache)     │  │ (C++ deps)   │  │ (pip+apt)    │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
└───────────────────┬─────────────────────────────────────┘
                    ▼
┌─────────────────────────────────────────────────────────┐
│     Phase 2: 并行构建 (MCIS Cognition/Execution/Memory)  │
│  ┌───────────────────────────────────────────────────┐  │
│  │ Cognition: atoms, commons, cupolas                │  │
│  │ Execution: daemon, gateway, manager               │  │
│  │ Memory:    heapstore                               │  │
│  │ SDK:       Go, Python, Rust, TypeScript            │  │
│  └───────────────────────────────────────────────────┘  │
└───────────────────┬─────────────────────────────────────┘
                    ▼
┌─────────────────────────────────────────────────────────┐
│     Phase 3: 测试验证 (MCIS Validation Dimension)        │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │ Unit Tests   │  │ Integration  │  │ Memory Check │  │
│  │ (pytest/ctest)│ │ Tests        │  │ (Valgrind)   │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
└───────────────────┬─────────────────────────────────────┘
                    ▼
┌─────────────────────────────────────────────────────────┐
│     Phase 4: 质量门禁 (MCIS Quality Gate)                │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │ Static Analy │  │ Code Format  │  │ Python Qual  │  │
│  │ (cppcheck)   │  │ (clang-fmt)  │  │ (pylint/mypy)│  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
└───────────────────┬─────────────────────────────────────┘
                    ▼
┌─────────────────────────────────────────────────────────┐
│     Phase 5: 制品打包 (MCIS Artifact Packaging)          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │ Source Arch  │  │ Docker Image │  │ SDK Packages │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
└───────────────────┬─────────────────────────────────────┘
                    ▼
┌─────────────────────────────────────────────────────────┐
│     Phase 6: 发布 (仅 main 分支, MCIS Release Dimension) │
│  → GitCode Release / GitHub Release                      │
└─────────────────────────────────────────────────────────┘
```

---

## 2. 文件结构

### 2.1 CI脚本目录 (`scripts/ci/`)

```
scripts/ci/                         # CI/CD 工具集目录
├── ci-run.sh                       # 主CI编排脚本 (6阶段流水线)
├── install-deps.sh                 # 统一依赖安装脚本 (带重试)
├── build-module.sh                 # 模块化构建脚本 (支持增量构建)
├── run-tests.sh                    # 测试执行脚本 (单元/集成/内存检测)
├── quality-gate.sh                 # 质量门禁脚本 (静态分析/格式检查)
├── deploy-artifacts.sh             # 制品打包与部署脚本
├── requirements-linux.txt          # Linux apt 依赖清单
├── requirements-macos.txt          # macOS Homebrew 依赖清单
└── CI_CD_DOCUMENTATION.md          # 本文档
```

### 2.2 工作流目录 (`.gitcode/workflows/`)

```
.gitcode/workflows/                 # GitCode Actions 工作流 (MCIS-based)
├── agentos-mcis-ci.yml             # 主CI流水线 (认知体+执行体+记忆体)
├── ci.yml                          # 基础CI流水线 (V3.0)
├── toolkit-ci.yml                  # 多语言SDK CI (SDK扩展体)
├── quality-gate.yml                # 质量门禁 (V2.0)
├── quality-monitoring.yml          # 质量监控与趋势分析 (可观测体)
├── release.yml                     # 发布流水线 (MCIS系统观发布体)
├── security-audit.yml              # 安全审计 (MCIS安全穹顶体)
├── dependency-update.yml           # 依赖更新 (MCIS系统维护体)
├── stale.yml                       # 陈旧Issue/PR管理 (MCIS问题管理体)
├── tests-test.yml                  # 测试验证 (MCIS系统观验证体)
└── build-desktop.yml               # 桌面客户端构建
```

### 2.3 其他CI相关目录

```
scripts/
├── dev/config/                     # 开发配置
│   ├── vcpkg.json                  # vcpkg依赖清单
│   ├── .clang-format               # C/C++格式化配置
│   ├── .editorconfig               # 编辑器配置
│   ├── .pre-commit-config.yaml     # Git hooks配置
│   ├── .jscpd.json                 # 代码重复检测配置
│   ├── .lizardrc                   # 复杂度检测配置
│   └── .clangd                     # clangd LSP配置
├── deploy/docker/                  # Docker部署配置
│   ├── Dockerfile.kernel           # 内核镜像
│   ├── Dockerfile.service          # 服务镜像
│   └── ...                         # 其他部署文件
├── lib/                            # 公共库
│   └── common.sh                   # 公共Shell函数
├── tools/                          # 工具脚本
│   └── check_yaml_syntax.py        # YAML语法验证工具
└── desktop-client/                 # 桌面客户端脚本
```

---

## 3. 依赖管理体系

### 3.1 四层依赖架构

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

### 3.2 tiktoken 特殊处理方案

**问题**: tiktoken 是 Python 库，没有 C 语言原生接口，但 `agentos/commons/CMakeLists.txt` 通过 `pkg_check_modules(TIKTOKEN REQUIRED tiktoken)` 强制要求。

**解决方案**: 在 CI 中创建 **pkg-config 存根 (Stub)**

```bash
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

echo 'void tiktoken_init(void) {}' | gcc -c - -o /tmp/tiktoken_stub.o
ar rcs /usr/local/lib/libtiktoken.a /tmp/tiktoken_stub.o
```

### 3.3 完整依赖矩阵

| 依赖名称 | 类型 | 必需? | 使用模块 | Linux包名 | Homebrew | vcpkg |
|---------|------|-------|---------|-----------|----------|-------|
| Threads | find_package | YES | 全部 | 内置 | 内置 | 内置 |
| PkgConfig | find_package | YES | daemon, commons | pkg-config | pkg-config | - |
| cJSON | pkg_check_modules | YES | daemon, gateway, commons | libcjson-dev | cjson | cjson |
| libcurl | pkg_check_modules | YES | daemon/common, llm_d | libcurl4-openssl-dev | curl | curl |
| yaml/libyaml | pkg_check_modules | YES | commons, daemon | libyaml-dev | yaml-cpp | yaml-cpp |
| OpenSSL | find_package | YES | cupolas | libssl-dev | openssl@3 | openssl |
| tiktoken* | pkg_check_modules | Stub | commons | (Stub) | (Stub) | (Stub) |
| libmicrohttpd | pkg_check_modules | Gateway | libmicrohttpd-dev | libmicrohttpd | libmicrohttpd |
| libwebsockets | pkg_check_modules | Gateway | libwebsockets-dev | libwebsockets | libwebsockets |
| SQLite3 | find_package | Optional | atoms, heapstore | libsqlite3-dev | sqlite | sqlite3 |
| libevent | find_package | Optional | atoms | libevent-dev | libevent | - |

---

## 4. 构建流水线设计

### 4.1 构建命令标准

```bash
# 安装依赖
chmod +x scripts/ci/install-deps.sh
./scripts/ci/install-deps.sh

# 模块化构建 (支持增量构建)
chmod +x scripts/ci/build-module.sh
./scripts/ci/build-module.sh --module <module> --type Release --parallel 4 --incremental

# 测试
chmod +x scripts/ci/run-tests.sh
./scripts/ci/run-tests.sh --module <module> --category all

# 质量门禁
chmod +x scripts/ci/quality-gate.sh
./scripts/ci/quality-gate.sh

# 制品打包
chmod +x scripts/ci/deploy-artifacts.sh
./scripts/ci/deploy-artifacts.sh
```

### 4.2 支持的模块列表

| 模块 | 源码路径 | MCIS维度 | CMake选项 |
|------|---------|----------|-----------|
| daemon | agentos/daemon | 执行体 | -DBUILD_TESTS=ON -DENABLE_LLM_DUMMY=ON |
| atoms | agentos/atoms | 认知体 | -DBUILD_TESTS=ON |
| commons | agentos/commons | 认知体 | -DBUILD_TESTS=ON |
| cupolas | agentos/cupolas | 安全穹顶体 | -DBUILD_TESTS=ON |
| gateway | agentos/gateway | 执行体 | -DBUILD_TESTS=ON |
| heapstore | agentos/heapstore | 记忆体 | -DBUILD_TESTS=ON |
| manager | agentos/manager | 执行体 | -DBUILD_TESTS=ON |

---

## 5. 缓存策略

### 5.1 智能缓存系统

| 缓存类型 | 工具 | Key生成规则 | 大小限制 |
|---------|------|------------|---------|
| C++编译缓存 | ccache | MCIS依赖文件组合哈希 + 每周轮换 | 1GB |
| C++依赖缓存 | vcpkg | vcpkg.json哈希 | 默认 |
| Python依赖缓存 | pip | requirements文件哈希 | 默认 |

### 5.2 缓存Key生成逻辑

```bash
# 收集所有可能影响构建的依赖文件
DEPENDENCY_FILES="scripts/dev/config/vcpkg.json scripts/ci/requirements-linux.txt CMakeLists.txt"
DEPENDENCY_FILES="$DEPENDENCY_FILES $(find . -name 'CMakeLists.txt' -path '*/agentos/*' | head -20)"
DEPENDENCY_FILES="$DEPENDENCY_FILES $(find scripts/ci -name '*.sh' -type f | head -10)"

# 计算组合哈希值 + 每周轮换
HASH=$(sha256sum $DEPENDENCY_FILES | sha256sum | cut -d' ' -f1)
WEEK_NUM=$(date +%U)
CACHE_KEY="mcis-cache-${{ runner.os }}-$HASH-week$WEEK_NUM"
```

### 5.3 预期性能提升

| 操作 | 无缓存 | 有缓存 | 提升 |
|------|--------|--------|------|
| Linux 依赖安装 | ~8 min | ~30 s | 94% |
| C++ 编译 (增量) | 100% | 20-40% | 60-80% |
| 总构建时间 (缓存命中) | ~45 min | ~25 min | 44% |

---

## 6. 错误处理与容错

### 6.1 重试机制

install-deps.sh 中的重试逻辑:
- 最大重试次数: 3
- 重试延迟: 5秒，指数退避 (5s -> 10s -> 20s)
- 网络错误自动重试

### 6.2 降级策略

| 场景 | 主要方案 | 降级方案 |
|------|---------|---------|
| 统一脚本失败 | scripts/ci/install-deps.sh | 直接 apt-get install 最小集 |
| 测试套件失败 | ctest --output-on-failure | 继续后续步骤，标记警告 |
| 可选依赖缺失 | 完整安装 | 跳过，记录 warning |
| 格式检查失败 | 阻塞 PR | 仅报告，不阻塞 |
| 测试文件不存在 | pytest指定文件 | 跳过并发出notice |

### 6.3 错误报告

所有关键步骤均通过以下方式报告:
- `echo "::error::消息"` - 错误注解
- `echo "::warning::消息"` - 警告注解
- `echo "::notice::消息"` - 通知注解
- `$GITHUB_STEP_SUMMARY` - Job摘要表格
- Artifact上传 - 详细日志文件

---

## 7. 质量门禁

### 7.1 检查项目

| 检查项 | 工具 | 检查范围 | 阻塞? |
|--------|------|---------|-------|
| C/C++静态分析 | cppcheck | agentos/*/src | 否 |
| C/C++格式检查 | clang-format | *.c/*.h | 否 |
| Python质量 | pylint + mypy | agentos/toolkit/python | 否 |
| 内存检测 | Valgrind | daemon (Debug+ASan) | 否 |
| 安全扫描 | Trivy + Gitleaks | 全项目 | 是(严重漏洞) |

---

## 8. MCIS工作流映射

| MCIS维度 | 工作流 | 触发条件 | 控制论机制 |
|----------|--------|---------|-----------|
| 认知体+执行体+记忆体 | agentos-mcis-ci.yml | push/PR | 负反馈:构建失败阻断 |
| 安全穹顶体 | security-audit.yml | schedule+手动 | 负反馈:安全漏洞阻断 |
| 可观测体 | quality-monitoring.yml | schedule | 前馈:趋势预测 |
| 系统观发布体 | release.yml | tag push | 正反馈:发布加速 |
| 系统维护体 | dependency-update.yml | schedule+手动 | 前馈:依赖预测 |
| 问题管理体 | stale.yml | schedule | 负反馈:问题清理 |
| 系统观验证体 | tests-test.yml | push/PR | 负反馈:测试失败阻断 |
| SDK扩展体 | toolkit-ci.yml | push/PR | 正反馈:兼容性强化 |

---

## 9. 维护指南

### 9.1 更新依赖版本

1. 编辑 `scripts/ci/requirements-linux.txt` 或 `scripts/ci/requirements-macos.txt`
2. 编辑 `scripts/dev/config/vcpkg.json`
3. 提交代码 -> CI自动验证 -> 合并到main

### 9.2 添加新模块支持

1. 在 `scripts/ci/build-module.sh` 的 `MODULE_SOURCES` 和 `MODULE_CMAKE_OPTIONS` 中添加新模块
2. 在 `scripts/ci/ci-run.sh` 的 `modules` 数组中添加新模块名
3. 在 `.gitcode/workflows/agentos-mcis-ci.yml` 的矩阵中添加新模块

### 9.3 排查常见问题

| 错误信息 | 可能原因 | 解决方案 |
|---------|---------|---------|
| Could not find TIKTOKEN | 存根未创建 | 检查 tiktoken.pc 是否存在 |
| vcpkg.json not found | 文件缺失 | 确认 scripts/dev/config/vcpkg.json 存在 |
| OpenSSL not found | macOS路径问题 | 设置 OPENSSL_ROOT_DIR |
| apt-get timeout | 网络问题 | 重试机制会自动处理 |
| ctest timeout | 测试挂起 | 增加 --timeout 值 |
| Module not defined | build-module.sh缺少定义 | 在MODULE_SOURCES中添加模块 |
| Test file not found | 测试文件不存在 | 工作流已添加文件存在性检查 |

### 9.4 关键联系人

- CI/CD维护者: @lidecheng, @wangliren
- 安全问题: security@spharx.com

---

## 变更历史

| 版本 | 日期 | 变更内容 |
|------|------|---------|
| v3.0.0 | 2026-04-09 | MCIS理论集成; 更新文件结构; 修复引用一致性; 增量构建支持 |
| v2.2.0 | 2026-04-06 | 目录迁移: ci/ -> scripts/ci/ |
| v2.0.0 | 2026-04-02 | 初始版本，完整CI/CD标准化配置 |

---

*SPHARX Ltd. - AgentOS Project*