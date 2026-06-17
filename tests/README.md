# 测试套件

`tests/`

## 概述

`tests/` 目录是 AgentRT 项目的集中测试套件，涵盖从底层内核到上层应用的完整测试体系。测试框架采用 C（CMocka）和 Python（pytest）双语言实现，支持单元测试、集成测试、契约测试、性能基准测试和安全测试等多层次验证。

本目录作为所有测试活动的根入口，包含全局配置文件、构建脚本以及五个子目录，分别对应不同测试层级。C 语言测试通过 CMake + CMocka 构建，Python 测试通过 pytest 驱动，性能基准测试使用 pytest-benchmark，Shell 脚本测试使用 bats-core。所有测试均可通过统一入口 `utils/python/run_tests.py` 执行，也支持直接使用 pytest 或 ctest 运行。

> **版本**：v0.1.0

## 与 agentos/ 的对应关系

| 测试目录 | 对应的 agentos/ 模块 | 测试框架 |
|----------|---------------------|----------|
| `unit/atoms/corekern/` | `agentos/atoms/corekern/` | CMocka — 微内核核心（IPC/Binder、内存管理、任务调度、定时器） |
| `unit/atoms/coreloopthree/` | `agentos/atoms/coreloopthree/` | CMocka + pytest — 三环运行时（认知环/执行环/学习环） |
| `unit/atoms/memory/` | `agentos/atoms/memory/` | CMocka — 内置记忆子系统（L1+L2 层） |
| `unit/atoms/syscall/` | `agentos/atoms/syscall/` | CMocka — 系统调用接口（5 类接口 + 4 层保护） |
| `unit/commons/` | `agentos/commons/` | CMocka + pytest — 统一基础库（平台抽象/日志/配置/内存/同步等 20+ 子模块） |
| `unit/cupolas/` | `agentos/cupolas/` | CMocka — 安全穹顶单元测试（防护/清洗/权限/审计/守卫框架） |
| `unit/daemon/common/` | `agentos/daemon/common/` | CMocka — 公共服务库（19 个组件） |
| `unit/daemon/gateway_d/` | `agentos/daemon/gateway_d/` | CMocka + pytest — API 网关守护进程 |
| `unit/daemon/llm_d/` | `agentos/daemon/llm_d/` | CMocka + pytest — LLM 服务守护进程（多 Provider） |
| `unit/daemon/sched_d/` | `agentos/daemon/sched_d/` | CMocka — 任务调度守护进程 |
| `unit/daemon/market_d/` | `agentos/daemon/market_d/` | CMocka — 应用市场守护进程 |
| `unit/daemon/monit_d/` | `agentos/daemon/monit_d/` | CMocka — 监控告警守护进程 |
| `unit/daemon/tool_d/` | `agentos/daemon/tool_d/` | CMocka + pytest — 工具执行守护进程 |
| `unit/heapstore/` | `agentos/heapstore/` | CMocka — 运行时数据存储（SQLite + 内存后端混合存储） |
| `unit/manager/` | `ecosystem/manager/` | pytest — 统一配置管理中心（多模块 Schema + 热重载） |
| `unit/openlab/` | `ecosystem/openlab/` | pytest — 开放生态系统（Apps/Contrib/Markets） |
| `unit/toolkit/` | `sdk/` + `scripts/toolkit/` | pytest — 运维工具包与多语言 SDK（Python/Rust） |
| `integration/c/` | `agentos/atoms/`, `agentos/commons/`, `agentos/gateway/` | CMocka — C 层端到端核心集成与协议兼容性 |
| `integration/python/` | `agentos/daemon/`, `agentos/gateway/`, `agentos/heapstore/` | pytest — Python 层端到端工作流与协议兼容性 |
| `integration/commons/` | `agentos/commons/` | CMocka — Commons 统一基础库集成测试 |
| `integration/coreloopthree/` | `agentos/atoms/coreloopthree/` | pytest — 三环系统集成（认知-执行联动） |
| `integration/cupolas/` | `agentos/cupolas/` | CMocka — Cupolas 安全穹顶集成测试 |
| `integration/memoryrovol/` | `agentos/atoms/memoryrovol/` | pytest — 记忆系统检索与层级测试 |
| `integration/platform/` | `agentos/atoms/`, `agentos/commons/` | CMocka — 跨平台 API 兼容性验证 |
| `integration/syscall/` | `agentos/atoms/syscall/` | pytest — 系统调用端到端流程 |
| `benchmarks/atoms/` | `agentos/atoms/` | C — Atoms 层性能基准 |
| `benchmarks/concurrency/` | `agentos/daemon/`, `agentos/gateway/` | pytest-benchmark — 并发压力测试 |
| `benchmarks/cupolas/` | `agentos/cupolas/` | C — Cupolas 安全基准与压力测试 |
| `security/c/` | `agentos/cupolas/` | CMocka — C 层安全审计 |
| `security/cupolas/` | `agentos/cupolas/` | C — Cupolas 安全模糊测试 |
| `security/python/` | `agentos/cupolas/` | pytest — Python 层安全测试 |
| `contract/python/` | `agentos/daemon/`, `ecosystem/openlab/` | pytest — 接口契约验证 |

## 目录结构

```
tests/
├── 配置文件
│   ├── conftest.py              # pytest 全局夹具与配置
│   ├── pytest.ini               # pytest 运行参数与标记定义
│   ├── .coveragerc              # coverage 覆盖率采集规则
│   ├── codecov.yml              # Codecov 报告上传配置
│   ├── .editorconfig            # 编辑器统一代码风格
│   ├── .pre-commit-config.yaml  # Git pre-commit 钩子链
│   ├── requirements.txt         # Python 运行依赖
│   └── requirements-dev.txt     # Python 开发依赖
├── 构建配置
│   ├── CMakeLists.txt           # 测试构建入口（C 测试）
│   └── Makefile                 # Make 测试入口
├── unit/                        # 单元测试（138 个文件）
│   ├── atoms/                   #   Atoms 微内核层测试（26 个文件）
│   │   ├── corekern/            #     微内核核心（7 个文件）
│   │   ├── coreloopthree/       #     三环运行时（11 个文件）
│   │   ├── memory/              #     内置记忆（1 个文件）
│   │   └── syscall/             #     系统调用层（6 个文件）
│   ├── commons/                 #   Commons 基础库测试（17 个文件）
│   │   └── unit/                #     公共工具单元测试
│   ├── cupolas/                 #   Cupolas 安全模块（10 个文件）
│   │   └── unit/                #     安全单元测试
│   ├── daemon/                  #   Daemon 守护进程测试（39 个文件）
│   │   ├── common/              #     公共服务库（9 个文件）
│   │   ├── gateway_d/           #     网关守护进程（5 个文件）
│   │   ├── llm_d/               #     LLM 服务（7 个文件）
│   │   ├── market_d/            #     应用市场（5 个文件）
│   │   ├── monit_d/             #     监控告警（4 个文件）
│   │   ├── sched_d/             #     任务调度（2 个文件）
│   │   └── tool_d/              #     工具执行（7 个文件）
│   ├── heapstore/               #   堆存储测试（13 个文件）
│   ├── manager/                 #   管理器配置测试（8 个文件）
│   ├── openlab/                 #   OpenLab 生态测试（10 个文件）
│   └── toolkit/                 #   工具包与 SDK 测试（12 个文件）
├── integration/                 # 集成测试（含端到端，17 个文件）
│   ├── c/                       #   C 集成测试（3 个文件）
│   ├── python/                  #   Python 集成测试（3 个文件）
│   ├── commons/                 #   Commons 集成测试（2 个文件）
│   ├── coreloopthree/           #   三环系统集成（2 个文件）
│   ├── cupolas/                 #   Cupolas 安全集成测试（1 个文件）
│   ├── memoryrovol/             #   记忆系统集成（2 个文件）
│   ├── platform/                #   跨平台兼容性测试（3 个文件）
│   └── syscall/                 #   系统调用集成（1 个文件）
├── contract/                    # 契约测试（4 个文件）
│   └── python/                  #   Python 契约测试
├── benchmarks/                  # 性能基准测试（16 个文件）
│   ├── atoms/                   #   Atoms 层基准（2 个文件）
│   ├── c/                       #   C 基准测试（2 个文件）
│   ├── concurrency/             #   并发压力测试（2 个文件）
│   ├── cupolas/                 #   Cupolas 安全基准与压力测试（2 个文件）
│   ├── python/                  #   Python 基准测试（3 个文件）
│   ├── retrieval_latency/       #   检索延迟测试
│   └── token_efficiency/        #   Token 效率测试（2 个文件）
├── security/                    # 安全测试（11 个文件）
│   ├── c/                       #   C 安全审计（3 个文件）
│   ├── cupolas/                 #   Cupolas 安全模糊测试（2 个文件）
│   └── python/                  #   Python 安全测试（6 个文件）
└── utils/                       # 测试工具函数与脚本（16+ 个文件）
    ├── fixtures/                #   测试夹具和数据
    │   └── data/                #     记忆/会话/技能/任务数据
    ├── python/                  #   Python 测试工具（11 个文件）
    └── templates/               #   测试模板（5 个文件）
        ├── c/                   #     C 测试模板
        └── python/              #     Python 测试模板
```

## 测试层级

| 层级 | 语言 | 框架 | 目标 |
|------|------|------|------|
| 单元测试 | C / Python | CMocka / pytest | 验证单个函数或模块的正确性 |
| 集成测试 | C / Python | CMocka / pytest | 验证多组件间的数据流和协议交互 |
| 契约测试 | Python | pytest | 确保接口契约的一致性 |
| 基准测试 | C / Python | CMocka / pytest-benchmark | 性能指标监控与回归检测 |
| 安全测试 | C / Python | CMocka / pytest | 权限、注入、XSS 等安全场景 |
| Shell 测试 | Shell | bats-core | 构建脚本与 CLI 工具验证 |

## 测试框架说明

### C 语言测试（CMocka）

C 语言测试使用 CMocka 框架，通过 CMake 构建系统编译和执行。每个 C 测试文件包含独立的测试组，使用 `cmocka.h` 提供的断言宏（如 `assert_int_equal`、`assert_ptr_equal`、`assert_string_equal`）进行验证。测试通过 `ctest` 运行。

### Python 测试（pytest）

Python 测试使用 pytest 框架，配合 `pytest.ini` 中定义的标记和配置。支持并行执行（pytest-xdist）、覆盖率采集（pytest-cov）和基准测试（pytest-benchmark）。全局夹具定义在 `conftest.py` 中。

### 性能基准测试（pytest-benchmark）

性能基准测试使用 pytest-benchmark 插件，支持统计比较、回归检测和结果持久化。C 语言基准测试通过自定义计时宏实现。

### Shell 测试（bats-core）

Shell 脚本和 CLI 工具的测试使用 bats-core 框架，验证构建脚本、安装流程和命令行接口的正确性。

## 运行方式

### 统一入口

```bash
# 运行所有测试
python tests/utils/python/run_tests.py

# 仅运行单元测试
python tests/utils/python/run_tests.py --type unit

# 运行集成测试
python tests/utils/python/run_tests.py --type integration

# 运行特定模块测试
python tests/utils/python/run_tests.py --module atoms

# 生成测试报告
python tests/utils/python/run_tests.py --report html
```

### 使用 pytest

```bash
# 运行所有测试
cd tests && pytest -v

# 运行特定标记的测试
pytest -v -m "unit"
pytest -v -m "integration"
pytest -v -m "contract"
pytest -v -m "benchmark" --benchmark-only
pytest -v -m "security"

# 并行运行测试
pytest -v -n auto

# 运行并生成覆盖率报告
pytest -v --cov=agentos --cov-report=html
```

### 使用 ctest（C 测试）

```bash
# 构建并运行所有 C 测试
cd build && cmake .. && make && ctest -V

# 运行特定测试
cd build && ctest -R test_corekern
cd build && ctest -R test_e2e_core
cd build && ctest -R security
```

## 测试标记

| 标记 | 说明 |
|------|------|
| `unit` | 单元测试 |
| `integration` | 集成测试 |
| `contract` | 契约测试 |
| `benchmark` | 性能基准测试 |
| `security` | 安全测试 |
| `fuzz` | 模糊测试 |
| `slow` | 慢速测试（默认排除） |

## 测试覆盖说明

测试覆盖率通过 `.coveragerc` 配置采集规则，支持 `codecov.yml` 上传至 Codecov 平台。覆盖率目标按模块划分：

| agentos/ 模块 | 单元测试 | 集成测试 | 安全测试 | 基准测试 |
|--------------|---------|---------|---------|---------|
| `atoms/corekern/` | ✅ CMocka | ✅ CMocka | — | ✅ C |
| `atoms/coreloopthree/` | ✅ CMocka | ✅ pytest | — | — |
| `atoms/memory/` | ✅ CMocka | — | — | — |
| `atoms/memoryrovol/` | — | ✅ pytest | — | ✅ pytest-benchmark |
| `atoms/syscall/` | ✅ CMocka | ✅ pytest | — | — |
| `commons/` | ✅ CMocka | ✅ CMocka | — | — |
| `cupolas/` | ✅ CMocka | ✅ CMocka | ✅ C + pytest | ✅ C |
| `daemon/` | ✅ CMocka + pytest | ✅ pytest | ✅ pytest | ✅ pytest-benchmark |
| `heapstore/` | ✅ CMocka | ✅ pytest | — | ✅ CMocka |
| `manager/` | ✅ pytest | — | — | — |
| `openlab/` | ✅ pytest | — | — | — |
| `toolkit/` | ✅ pytest | — | — | ✅ pytest-benchmark |

> **注意**：当前有 6 个 commons 测试处于禁用状态，分布在单元测试和集成测试中：
> - 单元测试（4 个）：`test_config`、`test_types`、`test_ipc`、`test_network`
> - 集成测试（2 个）：`test_common_integration`、`test_unified_modules`

---

© 2026 SPHARX Ltd. All Rights Reserved.
