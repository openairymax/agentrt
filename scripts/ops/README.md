# 运维部署与测试

`scripts/ops/`

## 概述

`ops/` 目录包含 AgentOS 项目的运维部署、性能基准测试、功能演示、Shell 工具库和运维测试套件，共 **33 个文件**，覆盖从 Docker 容器化部署到性能回归测试的完整运维链路。

> **版本**：v0.1.0

## 与 agentos/ 模块对应关系

| scripts/ops/ 模块 | 对应的 agentos/ 模块 | 用途 |
|-------------------|---------------------|------|
| `deploy/` | `daemon/`, `gateway/` | Docker 容器化部署（gateway_d, llm_d, sched_d, heapstore, monit_d 等） |
| `benchmark/` | `atoms/`, `commons/` | 性能基准测试（CoreLoopThree, CoreKern, Memory 等内核组件） |
| `demo/` | 全部模块 | 技术演示（服务框架/基准测试/工具链/开源治理） |
| `lib/` | 全部模块 | Shell 脚本公共库（日志/错误码/平台检测） |
| `tests/` | `daemon/`, `cupolas/`, `manager/` | 运维测试套件（插件/事件/安全/遥测/检查点/记忆/Token） |

## 目录结构

```
ops/                                           # 共 33 个文件
├── deploy/                                    # Docker 容器化部署（14 个文件）
│   ├── Dockerfile.kernel                      # 内核基础镜像（Ubuntu 22.04）
│   ├── Dockerfile.service                     # 服务层镜像（自包含构建）
│   ├── Makefile                               # Docker 操作 Makefile
│   ├── build.sh                               # 镜像构建脚本（dev/release 模式）
│   ├── check_config.sh                        # 配置检查脚本
│   ├── quickstart.sh                          # 一键快速入门
│   ├── docker-compose.yml                     # 默认编排（开发环境）
│   ├── docker-compose.preview.yml             # 预览环境
│   ├── docker-compose.staging.yml             # 预发布环境
│   ├── docker-compose.prod.yml                # 生产环境
│   ├── .env.example                           # 环境变量模板
│   ├── .gitignore                             # Git 忽略规则
│   └── secrets/                               # 密钥目录（.gitkeep）
├── benchmark/                                 # 性能基准测试框架（5 个文件）
│   ├── benchmark_core.py                      # 测试框架核心（测试定义/执行/监控/结果收集）
│   ├── statistics_engine.py                   # 统计计算引擎（分布拟合/显著性检验/回归分析）
│   ├── report_generator.py                    # 报告生成器（HTML/Markdown/PDF/JSON/Console）
│   ├── history_comparator.py                  # 历史比较器（版本对比/回归检测/趋势分析）
│   └── example_coreloopthree_benchmark.py     # CoreLoopThree 基准测试示例
├── demo/                                      # 技术演示
│   └── phase3_technology_demo.py              # 第三阶段技术演示（服务框架/基准测试/工具链/开源治理）
├── lib/                                       # Shell 脚本公共库（4 个文件）
│   ├── common.sh                              # 通用工具函数（加载 log/error/platform 依赖）
│   ├── error.sh                               # 统一错误码体系（1000-2999+）
│   ├── log.sh                                 # 多级别日志输出（DEBUG/INFO/WARN/ERROR/FATAL）
│   └── platform.sh                            # 平台检测（OS 类型/CPU 架构）
└── tests/                                     # 运维脚本测试套件
    ├── python/                                # Python 测试（6 个文件）
    │   ├── conftest.py                        #   pytest 配置（slow/integration/security 标记）
    │   ├── test_core.py                       #   插件/事件/安全/遥测模块测试
    │   ├── test_checkpoint_manager.py         #   检查点管理器测试
    │   ├── test_memory_manager.py             #   记忆管理器测试
    │   ├── test_token_budget.py               #   Token 预算管理测试
    │   └── test_token_counter.py              #   Token 计数器测试
    └── shell/                                 # Shell 测试（2 个文件）
        ├── test_common_utils.sh               #   通用工具函数测试
        └── test_framework.sh                  #   Shell 测试框架（bats-core）
```

## 典型调用

```bash
# Docker 快速启动
scripts/ops/deploy/quickstart.sh

# Docker 镜像构建
scripts/ops/deploy/build.sh --release

# 配置检查
scripts/ops/deploy/check_config.sh

# 性能基准测试
python scripts/ops/benchmark/benchmark_core.py --rounds 100

# CoreLoopThree 基准测试示例
python scripts/ops/benchmark/example_coreloopthree_benchmark.py

# 运维脚本测试
pytest scripts/ops/tests/python/
bash scripts/ops/tests/shell/test_framework.sh
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
