# 运维部署与测试

`scripts/ops/`

## 概述

`ops/` 目录包含 AgentRT 项目的运维部署、性能基准测试、Shell 工具库和运维测试套件，覆盖从 Docker 容器化部署到性能回归测试的完整运维链路。该模块是生产环境运维和性能质量保障的核心基础设施，提供多环境编排、统计驱动的基准测试、统一的 Shell 脚本库和全面的运维测试覆盖。

运维模块遵循以下设计原则：

- **分层部署**：采用双 Dockerfile 分层架构（内核基础镜像 + 服务层镜像），优化构建缓存和镜像体积
- **多环境编排**：提供开发、预览、预发布和生产四种 Docker Compose 编排配置，满足不同场景需求
- **统计驱动**：基准测试框架集成统计计算引擎，支持分布拟合、显著性检验和回归分析
- **统一基础库**：Shell 公共库提供日志、错误码、平台检测等基础能力，所有运维脚本共享同一套基础设施

> **版本**：v0.1.0

## 与 agentos/ 模块对应关系

> **注**：Docker 容器化部署已迁移至 `deploy/docker/`，详见 [deploy/docker/README.md](../../deploy/docker/README.md)。

| scripts/ops/ 模块 | 对应的 agentos/ 模块 | 用途 |
|-------------------|---------------------|------|
| `bin/agentrt-bootstrap.sh` | `daemons/`, `gateway/` | 按 DAG 层级顺序一键启动所有 daemon |
| `bin/quickstart.sh` | 全部模块 | 5 分钟快速创建示例 Agent 项目 |
| `benchmark/benchmark_core.py` | `atoms/` | 测试框架核心（测试定义/执行/监控/结果收集） |
| `benchmark/statistics_engine.py` | `atoms/corekern/`, `atoms/coreloopthree/` | 统计计算引擎（分布拟合/显著性检验/回归分析） |
| `benchmark/report_generator.py` | 全部模块 | 报告生成器（HTML/Markdown/PDF/JSON/Console） |
| `benchmark/history_comparator.py` | `atoms/` | 历史比较器（版本对比/回归检测/趋势分析） |
| `benchmark/example_coreloopthree_benchmark.py` | `atoms/coreloopthree/` | CoreLoopThree 三环核心运行时基准测试示例 |
| `lib/common.sh` | 全部模块 | 通用工具函数（加载 log/error/platform 依赖） |
| `lib/error.sh` | 全部模块 | 统一错误码体系（1000-2999+） |
| `lib/log.sh` | 全部模块 | 多级别日志输出（DEBUG/INFO/WARN/ERROR/FATAL） |
| `lib/platform.sh` | 全部模块 | 平台检测（OS 类型/CPU 架构） |
| `tests/python/test_core.py` | `daemons/`, `cupolas/`, `manager/` | 插件/事件/安全/遥测模块测试 |
| `tests/python/test_checkpoint_manager.py` | `daemons/` | 检查点管理器测试 |
| `tests/python/test_memory_manager.py` | `daemons/` | 记忆管理器测试 |
| `tests/python/test_token_budget.py` | `daemons/` | Token 预算管理测试 |
| `tests/python/test_token_counter.py` | `daemons/` | Token 计数器测试 |
| `tests/shell/test_common_utils.sh` | 全部模块 | 通用工具函数测试 |
| `tests/shell/test_framework.sh` | 全部模块 | Shell 测试框架（bats-core） |

## 目录结构

```
ops/
├── README.md                              # 本文档
├── bin/                                   # 运维入口脚本（2 个文件）
│   ├── agentrt-bootstrap.sh               #   AgentRT 一键按序启动所有 daemon
│   └── quickstart.sh                      #   5 分钟快速创建示例 Agent 项目
├── deploy/                                # Docker 部署已迁移至 deploy/docker/（仅保留指向 README）
├── benchmark/                             # 性能基准测试框架（6 个文件）
│   ├── README.md                          #   基准测试框架说明文档
│   ├── benchmark_core.py                  #   测试框架核心（测试定义/执行/监控/结果收集）
│   ├── statistics_engine.py               #   统计计算引擎（分布拟合/显著性检验/回归分析）
│   ├── report_generator.py                #   报告生成器（HTML/Markdown/PDF/JSON/Console）
│   ├── history_comparator.py              #   历史比较器（版本对比/回归检测/趋势分析）
│   └── example_coreloopthree_benchmark.py #   CoreLoopThree 基准测试示例
├── lib/                                   # Shell 脚本公共库（4 个文件）
│   ├── common.sh                          #   通用工具函数（加载 log/error/platform 依赖）
│   ├── error.sh                           #   统一错误码体系（1000-2999+）
│   ├── log.sh                             #   多级别日志输出（DEBUG/INFO/WARN/ERROR/FATAL）
│   └── platform.sh                        #   平台检测（OS 类型/CPU 架构）
└── tests/                                 # 运维脚本测试套件
    ├── python/                            # Python 测试（6 个文件）
    │   ├── conftest.py                    #   pytest 配置（slow/integration/security 标记）
    │   ├── test_core.py                   #   插件/事件/安全/遥测模块测试
    │   ├── test_checkpoint_manager.py     #   检查点管理器测试
    │   ├── test_memory_manager.py         #   记忆管理器测试
    │   ├── test_token_budget.py           #   Token 预算管理测试
    │   └── test_token_counter.py          #   Token 计数器测试
    └── shell/                             # Shell 测试（2 个文件）
        ├── test_common_utils.sh           #   通用工具函数测试
        └── test_framework.sh              #   Shell 测试框架（bats-core）
```

## 核心组件说明

### deploy/ — Docker 容器化部署（已迁移）

Docker 容器化部署方案已迁移至 `deploy/docker/`，采用单 Dockerfile 多阶段构建架构。原 `scripts/ops/deploy/` 下的双 Dockerfile（内核 + 服务层）配置已合并为统一的 `deploy/docker/Dockerfile`（多 daemon 运行时镜像）。

多环境编排（dev/test/staging/preview/prod）、监控集成（Prometheus + Grafana）和密钥目录均位于 `deploy/docker/`，详见 [deploy/docker/README.md](../../deploy/docker/README.md)。

### bin/ — 运维入口脚本

运维操作的入口脚本集合：

- **agentrt-bootstrap.sh**：AgentRT 一键启动脚本，按 DAG 层级顺序启动所有 daemon（5 层启动 DAG：基础设施→核心服务→Agent 服务→业务服务→网关），等待每层健康检查通过后再启动下一层。支持 `-c`（配置文件）、`-b`（二进制目录）、`-r`（运行时目录）、`-t`（超时）、`-s`（静默）、`-n`（dry-run）等选项。`scripts/install/agentrt-bootstrap.sh` 为其安装侧包装器。
- **quickstart.sh**：5 分钟快速创建示例 Agent 项目脚本，从 `examples/` 复制指定示例项目到目标目录，生成默认 `config.yaml` 和 `agents/main.agent.yaml`，引导新用户快速上手。

### benchmark/ — 性能基准测试框架

统计驱动的性能基准测试框架，详见 [benchmark/README.md](benchmark/README.md)：

- **benchmark_core.py**：测试框架核心，提供测试定义、执行引擎、性能监控和结果收集功能。支持自定义测试用例、迭代轮次和超时控制。
- **statistics_engine.py**：统计计算引擎，提供分布拟合（正态/对数正态/指数分布）、显著性检验（t 检验/Mann-Whitney U 检验）和回归分析功能。
- **report_generator.py**：报告生成器，支持 HTML、Markdown、PDF、JSON 和 Console 五种输出格式。
- **history_comparator.py**：历史比较器，支持版本对比、回归检测和趋势分析，可自动识别性能退化。
- **example_coreloopthree_benchmark.py**：CoreLoopThree 三环核心运行时基准测试示例，展示如何编写自定义基准测试。

### lib/ — Shell 脚本公共库

所有运维 Shell 脚本共享的基础库：

- **common.sh**：通用工具函数入口，自动加载 `log.sh`、`error.sh`、`platform.sh` 依赖。提供 `source_common_lib` 函数供其他脚本引用。
- **error.sh**：统一错误码体系，定义 1000-2999+ 范围的错误码常量和错误处理函数。错误码按模块分段分配，确保全局唯一。
- **log.sh**：多级别日志输出，支持 DEBUG、INFO、WARN、ERROR、FATAL 五个级别。支持彩色终端输出和日志文件持久化。
- **platform.sh**：平台检测，自动识别操作系统类型（Linux/macOS/Windows/WSL）和 CPU 架构（x86_64/ARM64），为其他脚本提供平台相关的条件判断。

### tests/ — 运维脚本测试套件

运维模块的自动化测试：

- **python/conftest.py**：pytest 配置，定义 `slow`、`integration`、`security` 等测试标记，配置 fixtures 和通用测试工具。
- **python/test_core.py**：插件/事件/安全/遥测模块的集成测试，验证 `toolkit/` 中对应模块的功能正确性。
- **python/test_checkpoint_manager.py**：检查点管理器测试，验证检查点的创建、恢复、轮转和 JSON 序列化功能。
- **python/test_memory_manager.py**：记忆管理器测试，验证 L1-L4 四层记忆的存储、检索和淘汰策略。
- **python/test_token_budget.py**：Token 预算管理测试，验证预算分配、使用追踪和超限告警功能。
- **python/test_token_counter.py**：Token 计数器测试，验证多策略计数的准确性和一致性。
- **shell/test_common_utils.sh**：通用工具函数测试，验证 `lib/common.sh` 中的工具函数。
- **shell/test_framework.sh**：Shell 测试框架，基于 bats-core，提供 Shell 脚本测试的基础设施。

## 使用方式

### Docker 部署

Docker 部署配置已迁移至 `deploy/docker/`，以下为常用命令：

```bash
# 开发环境启动
docker-compose -f deploy/docker/docker-compose.yml \
               -f deploy/docker/docker-compose.dev.yml up -d

# 生产环境启动
docker-compose -f deploy/docker/docker-compose.yml \
               -f deploy/docker/docker-compose.prod.yml up -d

# 预发布环境启动
docker-compose -f deploy/docker/docker-compose.staging.yml up -d

# 预览环境启动
docker-compose -f deploy/docker/docker-compose.preview.yml up -d
```

### 性能基准测试

```bash
# 基础性能基准测试（100 轮迭代）
python scripts/ops/benchmark/benchmark_core.py --rounds 100

# 生成 HTML 报告
python scripts/ops/benchmark/benchmark_core.py --rounds 100 --report html --output results/

# 生成 JSON 格式结果
python scripts/ops/benchmark/benchmark_core.py --rounds 50 --report json --output results/

# CoreLoopThree 基准测试示例
python scripts/ops/benchmark/example_coreloopthree_benchmark.py

# 历史对比分析
python scripts/ops/benchmark/history_comparator.py --baseline results/v1.json --current results/v2.json
```

### Shell 公共库使用

```bash
# 在 Shell 脚本中引用公共库
source scripts/ops/lib/common.sh

# 使用日志功能
log_info "服务启动成功"
log_warn "内存使用率较高"
log_error "连接超时"

# 使用平台检测
if is_linux; then
    echo "Linux 平台"
elif is_macos; then
    echo "macOS 平台"
fi
```

### 运维测试

```bash
# 运行全部 Python 测试
pytest scripts/ops/tests/python/

# 运行指定测试文件
pytest scripts/ops/tests/python/test_core.py

# 运行慢速测试
pytest scripts/ops/tests/python/ -m slow

# 运行集成测试
pytest scripts/ops/tests/python/ -m integration

# 运行 Shell 测试
bash scripts/ops/tests/shell/test_framework.sh
bash scripts/ops/tests/shell/test_common_utils.sh
```

## 依赖说明

| 子模块 | 核心依赖 | 说明 |
|--------|---------|------|
| `bin/` | Bash 4.0+ | 运维入口脚本为纯 Bash 实现，依赖已安装的 daemon 二进制 |
| `deploy/` | Docker Engine 20.10+, Docker Compose v2 | 容器化部署已迁移至 `deploy/docker/`，详见对应 README |
| `benchmark/` | Python 3.8+, numpy, scipy, matplotlib | 统计计算依赖 numpy/scipy，报告生成依赖 matplotlib |
| `lib/` | Bash 4.0+ | Shell 公共库为纯 Bash 实现，无外部依赖 |
| `tests/python/` | Python 3.8+, pytest 7.0+ | Python 测试使用 pytest 框架 |
| `tests/shell/` | Bash 4.0+, bats-core | Shell 测试使用 bats-core 框架 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
