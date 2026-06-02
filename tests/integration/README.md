# 集成测试

`tests/integration/`

## 概述

`integration/` 目录包含 AgentOS 的集成测试（含端到端测试），共 **17 个文件**，验证多组件间的数据流、协议交互和工作流正确性。与单元测试的隔离验证不同，集成测试关注模块间的协作行为，确保各子系统组合后能够正确完成端到端业务流程。

集成测试覆盖以下关键场景：
- **端到端核心链路**：从 Atoms 内核层到 Daemon 服务层再到 Gateway 网关层的完整请求路径
- **协议兼容性**：HTTP/WebSocket/Stdio 到 JSON-RPC 2.0 的协议转换与消息格式一致性
- **跨模块协作**：三环系统（认知-执行-学习）的联动、记忆系统的层级检索、系统调用的完整流程
- **跨平台兼容**：不同操作系统环境下的 API 行为一致性

> **版本**：v0.1.0

## 与 agentos/ 模块对应关系

| tests/integration/ 目录 | 对应的 agentos/ 模块 | 测试内容 |
|-------------------------|---------------------|----------|
| `c/` | `atoms/`, `commons/`, `gateway/` | C 层端到端核心集成与协议兼容性 |
| `python/` | `daemon/`, `gateway/`, `heapstore/` | Python 层端到端工作流与协议兼容性 |
| `commons/` | `agentos/commons/` | Commons 统一基础库集成测试（公共模块跨组件协作） |
| `coreloopthree/` | `atoms/coreloopthree/` | 三环系统集成（认知-执行联动、记忆演化） |
| `cupolas/` | `agentos/cupolas/` | Cupolas 安全穹顶集成测试（跨模块防护链路） |
| `memoryrovol/` | `atoms/memoryrovol/` | MemoryRovol 记忆系统集成（层级检索与缓存） |
| `platform/` | `atoms/`, `commons/` | 跨平台 API 兼容性验证（目录递归创建、平台抽象） |
| `syscall/` | `atoms/syscall/` | 系统调用层端到端流程（5 类接口完整调用链） |

## 目录结构

```
integration/                       # 共 17 个文件
├── README.md                      # 本文档
├── c/                             # C 集成测试（CMocka，3 个文件）
│   ├── CMakeLists.txt             #   C 集成测试构建配置
│   ├── test_e2e_core.c            #   端到端核心集成测试
│   │                              #     验证 atoms → commons → gateway 完整链路
│   └── test_protocol_compatibility.c # 协议兼容性测试
│                                  #     验证 HTTP/WS/Stdio → JSON-RPC 2.0 转换
├── python/                        # Python 集成测试（pytest，3 个文件）
│   ├── __init__.py                #   Python 包初始化
│   ├── test_e2e_workflows.py      #   端到端工作流测试
│   │                              #     验证 daemon → gateway → heapstore 完整流程
│   └── test_protocol_compatibility.py # 协议兼容性测试
│                                  #     验证 Python 层协议消息格式
├── commons/                       # Commons 集成测试（2 个文件）
│   ├── test_common_integration.c  #   Commons 公共模块集成测试
│   └── test_unified_modules.c     #   统一模块集成测试
├── coreloopthree/                 # CoreLoopThree 三环系统集成（2 个文件）
│   ├── test_cognition_execution.py #  认知-执行联动测试
│   │                              #     验证认知环决策驱动执行环动作
│   └── test_memory_evolution.py   #   记忆演化测试
│                                  #     验证学习环更新记忆层级
├── cupolas/                       # Cupolas 安全穹顶集成（1 个文件）
│   └── test_cupolas_integration.c #   安全穹顶跨模块集成测试
│                                  #     验证防护→清洗→权限→审计完整链路
├── memoryrovol/                   # MemoryRovol 记忆系统集成（2 个文件）
│   ├── test_layers.py             #   记忆层级测试
│   │                              #     验证 L1-L4 层读写与缓存机制
│   └── test_retrieval.py          #   记忆检索测试
│                                  #     验证语义检索与排序算法
├── platform/                      # 跨平台兼容性测试（3 个文件）
│   └── c/
│       ├── CMakeLists.txt         #   平台测试构建配置
│       ├── test_mkdir_recursive.c #   递归目录创建测试
│       │                              验证跨平台目录创建行为
│       └── test_platform_compat.c #   平台 API 兼容性测试
│                                  #     验证不同 OS 下 API 行为一致性
└── syscall/                       # 系统调用层集成（1 个文件）
    └── test_syscalls.py           #   系统调用端到端流程测试
                                   #     验证 5 类接口完整调用链与 4 层保护
```

## 测试框架说明

### C 语言集成测试（CMocka）

C 语言集成测试使用 CMocka 框架，通过各子目录的 `CMakeLists.txt` 构建。集成测试与单元测试的区别在于：集成测试会初始化多个模块并验证它们之间的交互，而非仅测试单个函数。测试使用 `ctest` 运行，支持通过 `-R` 参数按名称过滤。

### Python 集成测试（pytest）

Python 集成测试使用 pytest 框架，标记为 `@pytest.mark.integration`。集成测试通常需要启动多个服务或初始化多个模块，因此执行时间比单元测试长。建议使用 `--timeout` 参数控制超时，使用 `-v` 查看详细输出。

## 运行方式

```bash
# C 集成测试（全部）
cd build && ctest -R e2e_core -V
cd build && ctest -R protocol_compatibility -V

# Python 集成测试（全部）
pytest tests/integration/python/ -v -m integration

# 按子模块运行
pytest tests/integration/coreloopthree/ -v
pytest tests/integration/memoryrovol/ -v
pytest tests/integration/syscall/ -v

# Commons 集成测试
cd build && ctest -R common_integration -V

# Cupolas 集成测试
cd build && ctest -R cupolas_integration -V

# 平台兼容性测试
cd build && ctest -R platform_compat -V
cd build && ctest -R mkdir_recursive -V

# 使用统一入口
python tests/utils/python/run_tests.py --type integration
python tests/utils/python/run_tests.py --type integration --module coreloopthree
python tests/utils/python/run_tests.py --type integration --module memoryrovol

# 并行运行
pytest tests/integration/ -v -n auto -m integration
```

## 测试场景

| 场景 | 涉及的 agentos/ 模块 | 验证目标 | 测试文件 |
|------|---------------------|----------|---------|
| **端到端核心** | `atoms/` → `daemon/` → `gateway/` | 完整请求链路 | `c/test_e2e_core.c`, `python/test_e2e_workflows.py` |
| **协议兼容** | `gateway/`, `protocols/` | HTTP/WS/Stdio → JSON-RPC 2.0 转换 | `c/test_protocol_compatibility.c`, `python/test_protocol_compatibility.py` |
| **三环系统** | `atoms/coreloopthree/` | 认知环/执行环/学习环协作 | `coreloopthree/test_cognition_execution.py` |
| **记忆演化** | `atoms/coreloopthree/` | 学习环驱动记忆层级更新 | `coreloopthree/test_memory_evolution.py` |
| **记忆检索** | `atoms/memoryrovol/` | L1-L4 层检索与缓存机制 | `memoryrovol/test_layers.py`, `memoryrovol/test_retrieval.py` |
| **系统调用** | `atoms/syscall/` | 5 类接口、4 层保护完整流程 | `syscall/test_syscalls.py` |
| **Commons 集成** | `commons/` | 公共模块跨组件协作 | `commons/test_common_integration.c`, `commons/test_unified_modules.c` |
| **Cupolas 集成** | `cupolas/` | 安全穹顶跨模块防护链路 | `cupolas/test_cupolas_integration.c` |
| **平台兼容** | `atoms/`, `commons/` | 跨平台 API 一致性 | `platform/c/test_mkdir_recursive.c`, `platform/c/test_platform_compat.c` |

## 测试覆盖说明

| agentos/ 模块 | 集成测试文件 | 测试框架 | 覆盖范围 |
|--------------|------------|---------|---------|
| `atoms/corekern/` | `c/test_e2e_core.c` | CMocka | 内核 IPC → 任务调度 → 内存管理完整链路 |
| `atoms/coreloopthree/` | `coreloopthree/` | pytest | 认知-执行联动、记忆演化、三环协调 |
| `atoms/memoryrovol/` | `memoryrovol/` | pytest | L1-L4 层读写、语义检索、缓存策略 |
| `atoms/syscall/` | `syscall/test_syscalls.py` | pytest | 5 类系统调用完整流程、4 层保护机制 |
| `commons/` | `commons/` | CMocka | 公共模块跨组件集成、统一模块协作 |
| `cupolas/` | `cupolas/test_cupolas_integration.c` | CMocka | 防护→清洗→权限→审计完整链路 |
| `gateway/` | `c/`, `python/` | CMocka + pytest | 协议转换、路由分发、请求处理 |
| `daemon/` | `python/test_e2e_workflows.py` | pytest | 守护进程间协作、服务发现、任务分发 |
| `heapstore/` | `python/test_e2e_workflows.py` | pytest | 数据存储在工作流中的读写一致性 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
