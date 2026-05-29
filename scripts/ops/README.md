# 运维与测试

`scripts/ops/`

## 概述

`ops/` 目录包含 AgentOS 项目的运维部署、性能基准测试、功能测试和 Shell 工具库，覆盖从 Docker 容器化部署到性能回归测试的完整运维链路。

## 目录结构

```
ops/
├── README.md
├── deploy/                              # Docker 容器化部署
│   ├── Dockerfile.kernel                # 内核镜像
│   ├── Dockerfile.service               # 服务镜像
│   ├── Makefile                         # 构建管理
│   ├── build.sh                         # 构建脚本
│   ├── check_config.sh                  # 配置检查
│   ├── quickstart.sh                    # 快速启动
│   ├── docker-compose.yml               # 默认编排
│   ├── docker-compose.preview.yml       # 预览环境
│   ├── docker-compose.staging.yml       # 预发布环境
│   ├── docker-compose.prod.yml          # 生产环境
│   ├── .env.example                     # 环境变量模板
│   ├── .gitignore                       # Git 忽略规则
│   └── secrets/                         # 密钥目录
├── benchmark/                           # 性能基准测试
│   ├── benchmark_core.py                # 测试框架核心
│   ├── statistics_engine.py             # 统计引擎
│   ├── report_generator.py              # 报告生成器
│   ├── history_comparator.py            # 历史比较器
│   └── example_coreloopthree_benchmark.py # 示例
├── tests/                               # 测试脚本
│   ├── python/                          # Python 测试
│   │   ├── conftest.py
│   │   ├── test_core.py
│   │   ├── test_checkpoint_manager.py
│   │   ├── test_memory_manager.py
│   │   ├── test_token_budget.py
│   │   └── test_token_counter.py
│   └── shell/                           # Shell 测试
│       ├── test_common_utils.sh
│       └── test_framework.sh
├── demo/                                # 技术演示
│   └── phase3_technology_demo.py
└── lib/                                 # Shell 工具库
    ├── common.sh                        # 通用工具函数
    ├── error.sh                         # 错误处理函数
    ├── log.sh                           # 日志输出函数
    └── platform.sh                      # 平台检测函数
```