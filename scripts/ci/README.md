# CI/CD 流水线与质量工具

`scripts/ci/`

## 概述

CI/CD 流水线脚本覆盖 `agentos/` 下全部模块的构建、测试、质量门禁和安全扫描，是持续集成和质量保证的核心基础设施。该模块按照流水线阶段划分为四个子模块：`pipeline/`（流水线编排）、`quality/`（代码质量分析）、`verify/`（构建验证与安全扫描）和 `release/`（发布管理），形成从代码提交到版本发布的完整自动化链路。

流水线设计遵循以下原则：

- **全链路自动化**：从依赖安装到产物部署，每个阶段可独立运行也可组合编排
- **安全左移**：集成 C 语言安全编码静态检查和 SEC-017 桩函数检测，在 CI 阶段即拦截安全问题
- **多语言覆盖**：质量分析器支持 C/C++/Python/Go/Rust/TypeScript 六种语言的 SDK 质量检测
- **跨平台支持**：依赖安装和 SDK 验证同时覆盖 Linux、macOS 和 Windows 三大平台

> **版本**：v0.1.0

## 与 agentos/ 模块对应关系

| scripts/ci/ 脚本 | 覆盖的 agentos/ 模块 | 用途 |
|-----------------|---------------------|------|
| `pipeline/build/build-module.sh` | `atoms/`, `commons/`, `cupolas/`, `daemons/`, `gateway/`, `heapstore/` | 多模块并行/增量编译 |
| `pipeline/test/run-tests.sh` | 所有模块 | CTest + pytest 双引擎测试执行 |
| `pipeline/validate/quality-gate.sh` | 所有模块 | 代码质量门禁检查 |
| `pipeline/security/security_check.py` | `atoms/`, `commons/`, `cupolas/` | C 语言安全编码静态检查 |
| `pipeline/security/security_regression.sh` | `atoms/`, `commons/` | 安全回归测试 |
| `pipeline/build/install-deps.sh` | 所有模块 | 跨平台依赖安装 |
| `pipeline/deploy/deploy-artifacts.sh` | 所有模块 | 构建产物归档与部署 |
| `quality/unified_quality_analyzer.py` | `toolkit/` | 多语言 SDK 统一质量分析（C/C++/Python/Go/Rust/TypeScript） |
| `quality/fix_encoding.py` | 所有模块 | 编码问题检测与修复（check/fix-bom/fix-double） |
| `quality/check-quality.sh` | 所有模块 | 提交前质量检查 |
| `quality/check_yaml_syntax.py` | 所有模块 | YAML 配置文件语法检查 |
| `quality/enhance_coverage.py` | 所有模块 | 测试覆盖率提升工具 |
| `quality/verify_consistency.py` | 所有模块 | 文档一致性验证 |
| `quality/update_openlab_paths.py` | `openlab/` | OpenLab 路径更新 |
| `verify/verify_sdks.sh` | `toolkit/python/`, `toolkit/go/`, `toolkit/rust/`, `toolkit/typescript/` | SDK 构建验证（Linux/macOS） |
| `verify/verify_sdks.ps1` | `toolkit/python/`, `toolkit/go/`, `toolkit/rust/`, `toolkit/typescript/` | SDK 构建验证（Windows） |
| `verify/test_build_modes.sh` | `atoms/memory/`, `atoms/memoryrovol/` | MemoryRovel OSS/PRO 构建模式测试 |
| `verify/sec017_scan.sh` | `atoms/`, `commons/`, `cupolas/` | SEC-017 桩函数检测 |
| `verify/forbidden_functions.sh` | `atoms/`, `commons/`, `cupolas/` | 禁止函数检测 |
| `verify/check_memcpy_dynamic.sh` | `atoms/`, `commons/` | 动态 memcpy 检查 |
| `release/release.sh` | 所有模块 | 一键版本发布 |
| `release/cleanup_builds.sh` | 所有模块 | 历史构建产物清理 |

## 目录结构

```
ci/
├── README.md                              # 本文档
├── pipeline/                              # 流水线编排
│   ├── ci-run.sh                          #   CI 主运行脚本（依赖→构建→测试→质量→部署）
│   ├── build/                             #   构建工具（模块编译/依赖安装/依赖清单）
│   │   ├── build-module.sh                #     多模块并行/增量构建
│   │   ├── install-deps.sh                #     跨平台依赖安装
│   │   ├── requirements-linux.txt         #     Linux 依赖清单
│   │   └── requirements-macos.txt         #     macOS 依赖清单
│   ├── test/                              #   测试执行
│   │   ├── run-tests.sh                   #     CTest/pytest 双引擎测试
│   │   ├── run-connection-tests.sh        #     12 条连接线集成测试
│   │   ├── test-integration.sh            #     集成测试环境启动与验证
│   │   └── wait-for-it.sh                 #     服务就绪等待工具
│   ├── security/                          #   安全扫描
│   │   ├── security-scan.sh               #     综合安全扫描
│   │   ├── security-scan.py               #     安全扫描（Python 实现）
│   │   ├── security_check.py              #     C 语言安全编码静态检查
│   │   ├── security_regression.sh         #     安全回归测试
│   │   └── leak-detection.sh              #     内存泄漏检测
│   ├── validate/                          #   验证工具
│   │   ├── quality-gate.sh                #     代码质量门禁
│   │   ├── contract-version-check.sh      #     合约版本变更检测
│   │   ├── cross-repo-verify.sh           #     跨子仓库交叉验证
│   │   └── config_validator.py            #     agentrt.yaml Schema 校验
│   └── deploy/                            #   部署工具
│       ├── deploy-artifacts.sh            #     构建产物归档与部署
│       └── db-migrate.sh                  #     heapstore 数据库迁移
├── quality/                               # 代码质量分析（8 个文件）
│   ├── unified_quality_analyzer.py        #   统一质量分析器（替代 analyze_quality.py）
│   ├── check-quality.sh                   #   提交前质量检查
│   ├── check_yaml_syntax.py               #   YAML 语法检查
│   ├── enhance_coverage.py                #   覆盖率提升
│   ├── fix_encoding.py                    #   编码修复工具（check/fix-bom/fix-double 子命令）
│   ├── verify_consistency.py              #   文档一致性验证
│   ├── update_openlab_paths.py            #   OpenLab 路径更新
│   └── requirements.txt                   #   Python 依赖清单
├── verify/                                # 构建验证（6 个文件）
│   ├── test_build_modes.sh                #   构建模式集成测试
│   ├── sec017_scan.sh                     #   SEC-017 桩函数检测
│   ├── forbidden_functions.sh             #   禁止函数检测
│   ├── check_memcpy_dynamic.sh            #   动态 memcpy 检查
│   ├── verify_sdks.sh                     #   SDK 构建验证（Linux/macOS）
│   └── verify_sdks.ps1                    #   SDK 构建验证（Windows PowerShell）
└── release/                               # 发布管理（2 个文件）
    ├── release.sh                         #   一键发布
    └── cleanup_builds.sh                  #   构建产物清理
```

## 核心组件说明

### pipeline/ — 流水线编排

流水线的核心编排模块，`ci-run.sh` 是 CI 主入口，按序调用以下阶段：

1. **install-deps.sh**：跨平台依赖安装，根据 `requirements-linux.txt` 或 `requirements-macos.txt` 安装对应平台的系统级依赖
2. **build-module.sh**：模块编译，支持多模块并行构建和增量构建，遵循 BAN-33 源外构建规范
3. **run-tests.sh**：测试执行，同时支持 CTest（C/C++ 单元测试）和 pytest（Python 测试）双引擎
4. **quality-gate.sh**：代码质量门禁，在测试通过后执行质量检查，不达标则阻断流水线
5. **security_check.py**：C 语言安全编码静态检查，检测缓冲区溢出、格式化字符串漏洞等常见安全问题
6. **security_regression.sh**：安全回归测试，确保修复的安全问题不会重新引入
7. **deploy-artifacts.sh**：构建产物归档与部署，将编译产物打包并推送到制品仓库

### quality/ — 代码质量分析

代码质量分析工具集，提供多维度的质量保障：

- **unified_quality_analyzer.py**：统一质量分析器，替代原有的 `analyze_quality.py`，支持 C/C++/Python/Go/Rust/TypeScript 六种语言的 SDK 质量检测，输出统一格式的质量报告
- **fix_encoding.py**：编码修复工具，合并了原有的三个独立编码修复脚本，提供三个子命令：
  - `check`：检测文件编码问题（BOM 标记、双重编码等）
  - `fix-bom`：修复 BOM（Byte Order Mark）编码问题
  - `fix-double`：修复双重编码问题
- **check-quality.sh**：提交前质量检查，可在 git pre-commit hook 中调用
- **check_yaml_syntax.py**：YAML 配置文件语法检查，验证所有 YAML 文件的语法正确性
- **enhance_coverage.py**：测试覆盖率提升工具，分析未覆盖代码并生成覆盖率提升建议
- **verify_consistency.py**：文档一致性验证，确保文档与代码实现保持同步
- **update_openlab_paths.py**：OpenLab 路径更新工具，批量更新项目中的 OpenLab 相关路径

### verify/ — 构建验证与安全扫描

构建后的验证和安全扫描模块：

- **verify_sdks.sh / verify_sdks.ps1**：SDK 构建验证，分别支持 Unix Shell 和 Windows PowerShell，验证 `toolkit/python/`、`toolkit/go/`、`toolkit/rust/`、`toolkit/typescript/` 四种 SDK 的构建正确性
- **test_build_modes.sh**：构建模式集成测试，验证 `atoms/memory/` 的 MemoryRovel OSS 和 PRO 两种构建模式
- **sec017_scan.sh**：SEC-017 桩函数检测，扫描代码中的桩函数（stub functions）和未实现接口，确保没有遗漏的实现
- **forbidden_functions.sh**：禁止函数检测，扫描代码中是否使用了不安全的禁止函数（如 `strcpy`、`gets` 等）
- **check_memcpy_dynamic.sh**：动态 memcpy 检查，检测运行时动态大小的 `memcpy` 调用，防止潜在的缓冲区溢出

### release/ — 发布管理

版本发布和构建清理：

- **release.sh**：一键版本发布，支持指定版本号和发布通道（stable/beta/alpha），自动完成版本号更新、CHANGELOG 生成、Git 打标签和产物发布
- **cleanup_builds.sh**：历史构建产物清理，按策略清理过期的构建产物，释放存储空间

## 使用方式

### 完整 CI 流水线

```bash
# 运行完整 CI 流水线（依赖→构建→测试→质量→部署）
scripts/ci/pipeline/ci-run.sh
```

### 单独运行各阶段

```bash
# 依赖安装
scripts/ci/pipeline/build/install-deps.sh

# 模块编译
scripts/ci/pipeline/build/build-module.sh

# 测试执行
scripts/ci/pipeline/test/run-tests.sh

# 质量门禁
scripts/ci/pipeline/validate/quality-gate.sh

# 安全检查
python scripts/ci/pipeline/security/security_check.py

# 安全回归测试
scripts/ci/pipeline/security/security_regression.sh

# 产物部署
scripts/ci/pipeline/deploy/deploy-artifacts.sh
```

### 代码质量分析

```bash
# 提交前质量检查
scripts/ci/quality/check-quality.sh

# 统一质量分析
python scripts/ci/quality/unified_quality_analyzer.py

# 编码问题检测
python scripts/ci/quality/fix_encoding.py check

# 修复 BOM 编码问题
python scripts/ci/quality/fix_encoding.py fix-bom

# 修复双重编码问题
python scripts/ci/quality/fix_encoding.py fix-double

# YAML 语法检查
python scripts/ci/quality/check_yaml_syntax.py

# 覆盖率提升
python scripts/ci/quality/enhance_coverage.py

# 文档一致性验证
python scripts/ci/quality/verify_consistency.py
```

### 构建验证与安全扫描

```bash
# SDK 构建验证（Linux/macOS）
scripts/ci/verify/verify_sdks.sh

# SDK 构建验证（Windows PowerShell）
.\scripts\ci\verify\verify_sdks.ps1

# 构建模式测试
scripts/ci/verify/test_build_modes.sh

# SEC-017 桩函数检测（扫描所有模块）
scripts/ci/verify/sec017_scan.sh all

# SEC-017 桩函数检测（指定模块）
scripts/ci/verify/sec017_scan.sh atoms commons
```

### 版本发布

```bash
# 稳定版发布
scripts/ci/release/release.sh 0.1.0 stable

# Beta 版发布
scripts/ci/release/release.sh 0.2.0-beta.1 beta

# 清理过期构建产物
scripts/ci/release/cleanup_builds.sh
```

## 依赖说明

| 子模块 | 核心依赖 | 说明 |
|--------|---------|------|
| `pipeline/` | Bash 4.0+, CMake 3.20+, GCC/Clang, pytest | 流水线运行需要 CMake 构建系统和 pytest 测试框架 |
| `pipeline/security/security_check.py` | Python 3.8+ | C 语言安全编码静态检查，纯 Python 实现 |
| `pipeline/requirements-linux.txt` | apt/yum 包管理器 | Linux 系统级依赖清单 |
| `pipeline/requirements-macos.txt` | Homebrew | macOS 系统级依赖清单 |
| `quality/` | Python 3.8+, 见 `requirements.txt` | 质量分析工具依赖 Python 生态 |
| `verify/` | Bash, PowerShell Core | SDK 验证脚本同时支持 Unix Shell 和 Windows PowerShell |
| `release/` | Bash, Git | 版本发布需要 Git 进行打标签操作 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
