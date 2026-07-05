# 单元测试

`tests/unit/`

## 概述

`unit/` 目录是 AgentRT 的单元测试中心，共 **138 个文件**（含 3 个结构文件），涵盖从底层内核到上层框架的全面验证。C 语言使用 CMocka 框架，Python 使用 pytest 框架。每个测试文件专注于验证单个模块或函数的正确性，确保各组件在隔离环境下行为符合预期。

单元测试遵循以下原则：
- **隔离性**：每个测试用例独立运行，不依赖其他测试的状态
- **可重复性**：相同输入始终产生相同结果
- **快速执行**：单元测试应毫秒级完成，不涉及外部 I/O
- **单一职责**：每个测试函数只验证一个行为

> **版本**：v0.1.0

## 与 agentos/ 模块对应关系

| tests/unit/ 目录 | 对应的 agentos/ 模块 | 测试内容 |
|------------------|---------------------|----------|
| `atoms/corekern/` | `agentrt/atoms/corekern/` | 微核心核心（IPC/Binder、内存管理、任务调度、定时器） |
| `atoms/coreloopthree/` | `agentrt/atoms/coreloopthree/` | 三环核心运行时（认知环/执行环/学习环、协调器、多数投票） |
| `atoms/memory/` | `agentrt/atoms/memory/` | 内置记忆子系统（L1+L2 层 Memory Provider） |
| `atoms/syscall/` | `agentrt/atoms/syscall/` | 系统调用接口（任务/内存/会话/遥测/Agent 5 类接口 + 4 层保护） |
| `atoms/test_common_utils.c` | `agentrt/atoms/` | Atoms 层公共工具函数 |
| `commons/` | `agentrt/commons/` | 统一基础库（平台抽象/日志/配置/内存/同步/网络/IPC/Token/成本/可观测性） |
| `cupolas/` | `agentrt/cupolas/` | 安全穹顶单元测试（核心/签名/配置/工作台/指标/安全/金库/审计溢出/熔断器/清洗缓存） |
| `daemons/gateway_d/` | `agentrt/daemons/gateway_d/` | API 网关守护进程（网关核心/JSON-RPC/系统调用路由/RPC 处理器/服务层） |
| `daemons/llm_d/` | `agentrt/daemons/llm_d/` | LLM 服务守护进程（LLM 核心/服务层/响应处理/成本追踪/Token 计数/缓存/多 Provider） |
| `daemons/tool_d/` | `agentrt/daemons/tool_d/` | 工具执行守护进程（执行器/服务层/注册表/工具核心/验证器/缓存） |
| `daemons/sched_d/` | `agentrt/daemons/sched_d/` | 任务调度守护进程（调度器/策略） |
| `daemons/market_d/` | `agentrt/daemons/market_d/` | 应用市场守护进程（市场核心/安装器/Agent 注册/技能注册/Agent 注册核心） |
| `daemons/monit_d/` | `agentrt/daemons/monit_d/` | 监控告警守护进程（监控器/追踪/告警/指标） |
| `daemons/common/` | `agentrt/daemons/common/` | 公共服务库（IPC 客户端/输入验证/服务认证/配置/错误处理/安全字符串/JSON-RPC 辅助/平台/日志） |
| `heapstore/` | `agentrt/heapstore/` | 运行时数据存储（核心/注册表/批量/IPC/日志/内存/追踪/集成/安全路径遍历/模糊并发/边界/基准/批量性能） |
| `manager/` | `ecosystem/manager/` | 统一配置管理中心（配置验证/语法检查/集成/Schema 验证/漂移检测/审计日志验证） |
| `openlab/` | `ecosystem/openlab/` | 开放生态系统（Agent/任务/工具/存储/规划/调度/视频编辑/多 Agent） |
| `toolkit/` | `sdk/` + `scripts/toolkit/` | 运维工具包与多语言 SDK（Python SDK/Rust SDK/管理器/检查点/导入/综合/基准性能/Agent/任务管理器重构） |

## 目录结构

```
unit/                              # 共 138 个文件
├── README.md                      # 本文档
├── CMakeLists.txt                 # 单元测试构建入口
├── __init__.py                    # Python 包初始化
├── atoms/                         # Atoms 层单元测试（26 个文件）
│   ├── test_common_utils.c        #   Atoms 公共工具函数测试
│   ├── corekern/                  #   内核核心测试（7 个文件）
│   │   ├── test_main.c            #     内核主入口测试
│   │   ├── test_ipc.c             #     IPC/Binder 通信测试
│   │   ├── test_mem.c             #     内存管理测试
│   │   ├── test_task.c            #     任务调度测试
│   │   ├── test_time.c            #     定时器测试
│   │   ├── test_error.c           #     错误处理测试
│   │   └── test_corekern_extended.c #   内核扩展功能测试
│   ├── coreloopthree/             #   三环运行时测试（11 个文件）
│   │   ├── test_main.c            #     主入口测试
│   │   ├── test_cognition.c       #     认知环测试
│   │   ├── test_execution.c       #     执行环测试
│   │   ├── test_memory.c          #     学习环/记忆测试
│   │   ├── test_loop.c            #     循环控制测试
│   │   ├── test_coordinator.c     #     协调器测试
│   │   ├── test_coordinator_extended.c # 协调器扩展测试
│   │   ├── test_majority.c        #     多数投票测试
│   │   ├── test_cognition_example.c #   认知示例测试
│   │   └── benchmark.c            #     性能基准
│   ├── memory/                    #   内置记忆测试（1 个文件）
│   │   └── test_memory_provider.c #     Memory Provider 测试
│   └── syscall/                   #   系统调用层测试（6 个文件）
│       ├── test_syscall_table.c   #     系统调用表测试
│       ├── test_syscall_entry.c   #     系统调用入口测试
│       ├── test_syscall_functional.c #  功能性系统调用测试
│       ├── test_syscall_extended.c #    扩展系统调用测试
│       ├── test_syscall_integration.c # 系统调用集成测试
│       └── test_full_workflow.c   #     完整工作流测试
├── commons/                       # Commons 层单元测试（17 个文件）
│   └── unit/
│       ├── CMakeLists.txt         #   Commons 测试构建配置
│       ├── test_macros.h          #   测试辅助宏定义
│       ├── core_test.c            #   核心功能测试
│       ├── io_test.c              #   I/O 操作测试
│       ├── test_config.c          #   配置管理测试（当前禁用）
│       ├── test_cost.c            #   成本计算测试
│       ├── test_error.c           #   错误处理测试
│       ├── test_input_validator.c #   输入验证测试
│       ├── test_ipc.c             #   IPC 通信测试（当前禁用）
│       ├── test_logger.c          #   日志系统测试
│       ├── test_network.c         #   网络模块测试（当前禁用）
│       ├── test_observability.c   #   可观测性测试
│       ├── test_platform.c        #   平台抽象测试
│       ├── test_resource_guard.c  #   资源守卫测试
│       ├── test_string_utils.c    #   字符串工具测试
│       ├── test_token.c           #   Token 管理测试
│       └── test_types.c           #   类型系统测试（当前禁用）
├── cupolas/                       # Cupolas 安全模块单元测试（10 个文件）
│   └── unit/
│       ├── test_cupolas_core.c    #   安全穹顶核心测试
│       ├── test_cupolas_config.c  #   安全配置测试
│       ├── test_cupolas_security.c #  安全策略测试
│       ├── test_cupolas_signature.c # HMAC 签名测试
│       ├── test_cupolas_vault.c   #   安全金库测试
│       ├── test_cupolas_workbench.c # 安全工作台测试
│       ├── test_cupolas_metrics.c #   安全指标测试
│       ├── test_audit_overflow.c  #   审计溢出测试
│       ├── test_circuit_breaker.c #   熔断器测试
│       └── test_sanitizer_cache.c #   清洗缓存测试
├── daemons/                        # Daemon 守护进程测试（39 个文件）
│   ├── common/                    #   公共守护进程测试（9 个文件）
│   │   ├── test_config.c          #     配置管理测试
│   │   ├── test_error.c           #     错误处理测试
│   │   ├── test_ipc_client.c      #     IPC 客户端测试
│   │   ├── test_input_validator.c #     输入验证测试
│   │   ├── test_jsonrpc_helpers.c #     JSON-RPC 辅助测试
│   │   ├── test_logger.c          #     日志测试
│   │   ├── test_platform.c        #     平台抽象测试
│   │   ├── test_safe_string_utils.c #   安全字符串工具测试
│   │   └── test_svc_auth.c        #     服务认证测试
│   ├── gateway_d/                 #   网关守护进程测试（5 个文件）
│   │   ├── test_gateway.c         #     网关核心测试
│   │   ├── test_gateway_rpc_handler.c # RPC 处理器测试
│   │   ├── test_jsonrpc.c         #     JSON-RPC 协议测试
│   │   ├── test_service.c         #     服务层测试
│   │   └── test_syscall_router.c  #     系统调用路由测试
│   ├── llm_d/                     #   LLM 守护进程测试（7 个文件）
│   │   ├── test_llm.c             #     LLM 核心测试
│   │   ├── test_llm_service.py    #     LLM 服务 Python 测试
│   │   ├── test_service.c         #     服务层测试
│   │   ├── test_response.c        #     响应处理测试
│   │   ├── test_cost_tracker.c    #     成本追踪测试
│   │   ├── test_token_counter.c   #     Token 计数测试
│   │   └── test_cache.c           #     缓存测试
│   ├── market_d/                  #   市场守护进程测试（5 个文件）
│   │   ├── test_market.c          #     市场核心测试
│   │   ├── test_installer.c       #     安装器测试
│   │   ├── test_agent_registry.c  #     Agent 注册表测试
│   │   ├── test_agent_registry_core.c # Agent 注册核心测试
│   │   └── test_skill_registry.c  #     技能注册表测试
│   ├── monit_d/                   #   监控守护进程测试（4 个文件）
│   │   ├── test_monitor.c         #     监控器测试
│   │   ├── test_metrics.c         #     指标采集测试
│   │   ├── test_alert.c           #     告警测试
│   │   └── test_tracing.c         #     追踪测试
│   ├── sched_d/                   #   调度守护进程测试（2 个文件）
│   │   ├── test_scheduler.c       #     调度器测试
│   │   └── test_strategies.c      #     调度策略测试
│   └── tool_d/                    #   工具守护进程测试（7 个文件）
│       ├── test_tool.c            #     工具核心测试
│       ├── test_tool_service.py   #     工具服务 Python 测试
│       ├── test_executor.c        #     执行器测试
│       ├── test_service.c         #     服务层测试
│       ├── test_registry.c        #     注册表测试
│       ├── test_validator.c       #     验证器测试
│       └── test_cache.c           #     缓存测试
├── heapstore/                     # 堆存储测试（13 个文件）
│   ├── test_heapstore_core.c      #   核心存储操作测试
│   ├── test_heapstore_registry.c  #   注册表操作测试
│   ├── test_heapstore_batch.c     #   批量操作测试
│   ├── test_heapstore_ipc.c       #   IPC 通信测试
│   ├── test_heapstore_log.c       #   日志记录测试
│   ├── test_heapstore_memory.c    #   内存后端测试
│   ├── test_heapstore_trace.c     #   追踪测试
│   ├── test_heapstore_integration.c # 集成测试
│   ├── test_security_path_traversal.c # 路径遍历安全测试
│   ├── test_fuzzing_concurrency.c #   模糊并发测试
│   ├── test_edge_cases.c          #   边界条件测试
│   ├── test_batch_performance.c   #   批量性能测试
│   └── benchmark_heapstore.c      #   性能基准
├── manager/                       # 管理器与配置测试（8 个文件）
│   ├── __init__.py                #   Python 包初始化
│   ├── run_all_tests.py           #   测试运行入口
│   ├── test_validate_config.py    #   配置验证测试
│   ├── test_config_syntax.py      #   配置语法测试
│   ├── test_config_integration.py #   配置集成测试
│   ├── test_schema_validation.py  #   Schema 验证测试
│   ├── test_drift_detector.py     #   配置漂移检测测试
│   └── test_audit_log_validation.py # 审计日志验证测试
├── openlab/                       # OpenLab 测试（10 个文件）
│   ├── __init__.py                #   Python 包初始化
│   ├── conftest.py                #   pytest 夹具配置
│   ├── test_agent.py              #   单 Agent 测试
│   ├── test_agents.py             #   多 Agent 测试
│   ├── test_task.py               #   任务管理测试
│   ├── test_tool.py               #   工具管理测试
│   ├── test_storage.py            #   存储管理测试
│   ├── test_planning.py           #   规划系统测试
│   ├── test_dispatching.py        #   调度系统测试
│   └── test_videoedit.py          #   视频编辑测试
└── toolkit/                       # 工具包与 SDK 测试（12 个文件）
    ├── __init__.py                #   Python 包初始化
    ├── base_test_case.py          #   测试基类
    ├── test_agent.py              #   Agent 工具测试
    ├── test_sdk_python.py         #   Python SDK 测试
    ├── test_sdk_rust.py           #   Rust SDK 测试
    ├── test_managers.py           #   管理器测试
    ├── test_managers_extended.py  #   管理器扩展测试
    ├── test_task_manager_refactored.py # 任务管理器重构测试
    ├── test_checkpoint.py         #   检查点测试
    ├── test_imports.py            #   导入验证测试
    ├── test_comprehensive.py      #   综合测试
    └── test_benchmark_performance.py # 基准性能测试
```

## 测试框架说明

### C 语言测试（CMocka）

C 语言单元测试使用 CMocka 框架，通过 `CMakeLists.txt` 构建配置编译。每个测试文件包含一个或多个测试组，使用以下 CMocka 断言宏：

- `assert_int_equal(expected, actual)` — 整数相等断言
- `assert_ptr_equal(expected, actual)` — 指针相等断言
- `assert_string_equal(expected, actual)` — 字符串相等断言
- `assert_null(ptr)` / `assert_non_null(ptr)` — 空指针断言
- `assert_true(condition)` / `assert_false(condition)` — 布尔断言
- `assert_return_code(rc, error)` — 返回码断言

测试入口通过 `cmocka_unit_test()` 宏注册，使用 `run_group_tests()` 执行。

### Python 测试（pytest）

Python 单元测试使用 pytest 框架，支持以下特性：

- **夹具（Fixtures）**：通过 `conftest.py` 和 `@pytest.fixture` 定义共享测试数据
- **参数化（Parametrize）**：使用 `@pytest.mark.parametrize` 进行数据驱动测试
- **标记（Markers）**：使用 `@pytest.mark.unit` 标记单元测试
- **Mock**：使用 `unittest.mock` 进行依赖隔离
- **临时目录**：使用 `tmp_path` 夹具创建临时文件系统

## 运行方式

```bash
# 全部单元测试
pytest tests/unit/ -v -m unit

# 按模块运行
pytest tests/unit/atoms/ -v
pytest tests/unit/daemons/ -v
pytest tests/unit/commons/ -v
pytest tests/unit/cupolas/ -v
pytest tests/unit/heapstore/ -v
pytest tests/unit/manager/ -v
pytest tests/unit/openlab/ -v
pytest tests/unit/toolkit/ -v

# 按守护进程子模块运行
pytest tests/unit/daemons/gateway_d/ -v
pytest tests/unit/daemons/llm_d/ -v
pytest tests/unit/daemons/tool_d/ -v
pytest tests/unit/daemons/market_d/ -v
pytest tests/unit/daemons/monit_d/ -v
pytest tests/unit/daemons/sched_d/ -v
pytest tests/unit/daemons/common/ -v

# C 语言单元测试
cd build && ctest -N | grep test_

# 使用统一入口
python tests/utils/python/run_tests.py --type unit
python tests/utils/python/run_tests.py --type unit --module atoms
python tests/utils/python/run_tests.py --type unit --module daemon

# 并行运行
pytest tests/unit/ -v -n auto
```

## 测试覆盖说明

| agentos/ 模块 | 测试文件数 | 测试类型 | 关键验证点 |
|--------------|-----------|----------|-----------|
| `atoms/corekern/` | 7 | CMocka | IPC 通信、内存分配/释放、任务调度、定时器精度 |
| `atoms/coreloopthree/` | 11 | CMocka | 认知环推理、执行环动作、学习环更新、协调器同步、多数投票 |
| `atoms/memory/` | 1 | CMocka | Memory Provider 接口、L1/L2 层读写 |
| `atoms/syscall/` | 6 | CMocka | 系统调用表完整性、入口验证、5 类接口功能、4 层保护 |
| `commons/` | 17 | CMocka | 平台抽象、日志输出、配置解析、字符串处理、Token 管理 |
| `cupolas/` | 10 | CMocka | 安全策略执行、HMAC 签名、金库访问、审计链完整性、熔断器状态机 |
| `daemons/common/` | 9 | CMocka | IPC 客户端、服务认证、JSON-RPC 协议、安全字符串操作 |
| `daemons/gateway_d/` | 5 | CMocka | 网关路由、JSON-RPC 处理、系统调用转发、RPC 处理器 |
| `daemons/llm_d/` | 7 | CMocka + pytest | LLM 调用、响应解析、成本追踪、Token 计数、缓存命中 |
| `daemons/market_d/` | 5 | CMocka | Agent/技能注册、安装流程、市场查询 |
| `daemons/monit_d/` | 4 | CMocka | 指标采集、告警触发、追踪链路 |
| `daemons/sched_d/` | 2 | CMocka | 调度算法、策略切换 |
| `daemons/tool_d/` | 7 | CMocka + pytest | 工具注册/执行/验证、缓存管理 |
| `heapstore/` | 13 | CMocka | CRUD 操作、批量处理、IPC 通信、内存后端、路径遍历防护 |
| `manager/` | 8 | pytest | 配置验证、Schema 校验、漂移检测、审计日志 |
| `openlab/` | 10 | pytest | Agent 生命周期、任务调度、工具管理、视频编辑 |
| `toolkit/` | 12 | pytest | Python/Rust SDK、管理器、检查点、导入完整性 |

> **注意**：当前有 4 个 commons 单元测试处于禁用状态：`test_config`、`test_types`、`test_ipc`、`test_network`。另有 2 个 commons 集成测试（`test_common_integration`、`test_unified_modules`）也处于禁用状态，共计 6 个。

---

© 2026 SPHARX Ltd. All Rights Reserved.
