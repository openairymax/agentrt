# 脚本工具集

## 概述

AgentOS 脚本工具集涵盖 CI/CD 流水线、开发环境搭建、运维部署和项目资源等全生命周期管理功能。所有脚本按职能分类存放于五个子模块中，与 `agentos/` 核心模块紧密对应。

> **版本**：v0.1.0
>
> **规范**：所有构建脚本遵循 BAN-33 规则（禁止源内构建），构建产物必须输出到独立构建目录中。

## 目录结构

```
scripts/
├── ci/                            # CI/CD 流水线与质量工具
│   ├── pipeline/                  #   流水线编排（构建/测试/质量门禁/部署）
│   ├── quality/                   #   代码质量分析
│   │   ├── encoding/              #     编码检查与修复
│   │   └── docs/                  #     文档一致性验证
│   ├── verify/                    #   构建验证与安全扫描
│   └── release/                   #   发布管理
├── dev/                           # 开发环境工具
│   ├── build/                     #   跨平台构建（BAN-33 源外构建）
│   ├── setup/                     #   环境配置（Linux/macOS/Windows）
│   ├── cli/                       #   CLI 入口（agentos 命令）
│   ├── cmake/                     #   CMake 辅助（Windows MSVC 兼容）
│   └── utils/                     #   开发辅助（快速启动/环境验证）
├── ops/                           # 运维部署
│   ├── deploy/                    #   Docker 部署（多环境编排）
│   ├── benchmark/                 #   性能基准测试框架
│   ├── demo/                      #   技术演示
│   ├── lib/                       #   Shell 公共库（日志/错误码/平台检测）
│   └── tests/                     #   运维测试套件
├── toolkit/                        # Python 运维工具包
│   └── src/                       #   CLI/诊断/性能/记忆/Token/安全等模块
└── resources/                     # 项目资源
    ├── images/                    #   静态资源图片
    └── tutorial/                  #   交互式教程引擎与数据
```

## 与 agentos/ 模块对应关系

| scripts/ 模块 | 对应的 agentos/ 模块 | 用途 |
|---------------|---------------------|------|
| `ci/pipeline/` | `atoms/`, `commons/`, `cupolas/`, `daemon/`, `gateway/`, `heapstore/` | 全模块 CI/CD 流水线（构建→测试→质量→部署） |
| `ci/quality/` | `toolkit/` | 多语言 SDK 质量分析（C/C++/Python/Go/Rust/TypeScript） |
| `ci/verify/` | `toolkit/python/`, `toolkit/go/`, `toolkit/rust/`, `toolkit/typescript/` | SDK 构建验证、MemoryRovol 构建模式、安全扫描 |
| `dev/build/` | `atoms/`, `commons/`, `cupolas/`, `daemon/`, `gateway/`, `heapstore/` | 跨平台自动化构建（BAN-33 源外构建） |
| `dev/setup/` | 全部模块 | 交互式开发环境配置（依赖安装、工具链设置） |
| `dev/cli/` | `daemon/`, `manager/`, `openlab/` | 统一 CLI 入口（服务管理/智能体管理/任务管理） |
| `ops/deploy/` | `daemon/`, `gateway/` | Docker 容器化部署（gateway_d, llm_d, sched_d, heapstore, monit_d 等） |
| `ops/benchmark/` | `atoms/coreloopthree/`, `atoms/corekern/` | 性能基准测试覆盖核心内核组件 |
| `ops/tests/` | `daemon/`, `cupolas/`, `manager/` | 运维集成测试（核心/检查点/记忆/Token/安全/遥测） |
| `toolkit/` | `commons/`, `daemon/`, `manager/` | Python 运维工具集（诊断/记忆/Token/契约/CLI/插件/遥测） |

## 快速开始

```bash
# 开发环境配置
scripts/dev/setup/setup.sh

# 构建项目（BAN-33 源外构建）
scripts/dev/build/build.sh --release

# CLI 工具
scripts/dev/cli/agentos --help

# CI 主运行
scripts/ci/pipeline/ci-run.sh

# Docker 部署
scripts/ops/deploy/quickstart.sh

# 性能基准测试
python scripts/ops/benchmark/benchmark_core.py --rounds 100
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
