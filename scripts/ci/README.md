# CI/CD 流水线与质量工具

## 概述

CI/CD 流水线脚本覆盖 agentos/ 下全部模块的构建、测试和质量门禁，是持续集成和质量保证的核心。

> **版本**：v0.1.0

## 目录结构

```
ci/
├── pipeline/                # 流水线编排
│   ├── ci-run.sh            # CI 主运行脚本（依赖→构建→测试→质量→部署）
│   ├── build-module.sh      # 模块编译（多模块并行/增量构建）
│   ├── run-tests.sh         # 测试执行（CTest/pytest 双引擎）
│   ├── quality-gate.sh      # 代码质量门禁
│   ├── security_check.py    # C 语言安全编码静态检查
│   ├── security_regression.sh # 安全回归测试
│   ├── install-deps.sh      # 跨平台依赖安装
│   └── deploy-artifacts.sh  # 构建产物归档与部署
├── quality/                 # 代码质量分析
│   ├── unified_quality_analyzer.py  # 统一质量分析器
│   ├── analyze_quality.py           # 重复率和复杂度分析
│   ├── check-quality.sh             # 提交前质量检查
│   ├── check_yaml_syntax.py         # YAML 语法检查
│   ├── enhance_coverage.py          # 覆盖率提升
│   ├── encoding/            # 编码检查与修复
│   └── docs/                # 文档一致性验证
├── verify/                  # 构建验证
│   ├── test_build_modes.sh  # 构建模式集成测试
│   ├── sec017_scan.sh       # SEC-017 桩函数检测
│   └── verify_sdks.sh       # SDK 构建验证
└── release/                 # 发布管理
    ├── release.sh           # 一键发布
    └── cleanup_builds.sh    # 构建产物清理
```

## 覆盖的 agentos/ 模块

| scripts/ci/ 脚本 | 覆盖的 agentos/ 模块 |
|-----------------|---------------------|
| `pipeline/build-module.sh` | `atoms/`, `commons/`, `cupolas/`, `daemon/`, `gateway/`, `heapstore/` |
| `pipeline/run-tests.sh` | 所有模块的 C 单元测试 + Python 测试 |
| `quality/unified_quality_analyzer.py` | `toolkit/` 多语言 SDK（C/C++/Python/Go/TypeScript） |
| `verify/verify_sdks.sh` | `toolkit/python/`, `toolkit/go/`, `toolkit/rust/`, `toolkit/typescript/` |
| `verify/test_build_modes.sh` | `atoms/memory/` 的 MemoryRovel OSS/PRO 模式 |

## 典型调用

```bash
scripts/ci/pipeline/ci-run.sh
scripts/ci/quality/check-quality.sh
scripts/ci/verify/sec017_scan.sh all
scripts/ci/release/release.sh 0.1.0 stable
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
