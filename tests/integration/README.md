# 集成测试

`tests/integration/`

## 概述

`integration/` 目录包含 AgentOS 的集成测试（含端到端测试），共 **14 个文件**，验证多组件间的数据流、协议交互和工作流正确性。

> **版本**：v0.1.0

## 与 agentos/ 模块对应关系

| tests/integration/ 目录 | 对应的 agentos/ 模块 | 测试内容 |
|-------------------------|---------------------|----------|
| `c/` | `atoms/`, `commons/`, `gateway/` | C 层端到端核心集成与协议兼容性 |
| `python/` | `daemon/`, `gateway/`, `heapstore/` | Python 层端到端工作流与协议兼容性 |
| `coreloopthree/` | `atoms/coreloopthree/` | 双思考系统集成（认知执行/记忆演化） |
| `memoryrovol/` | `atoms/memoryrovol/` | MemoryRovol 记忆系统集成（层检索） |
| `syscall/` | `atoms/syscall/` | 系统调用层集成测试 |

## 目录结构

```
integration/                       # 共 14 个文件
├── README.md                      # 本文档
├── c/                             # C 集成测试（CMocka，3 个文件）
│   ├── CMakeLists.txt
│   ├── test_e2e_core.c            #   端到端核心集成
│   └── test_protocol_compatibility.c # 协议兼容性测试
├── python/                        # Python 集成测试（pytest，3 个文件）
│   ├── __init__.py
│   ├── test_e2e_workflows.py      #   端到端工作流测试
│   └── test_protocol_compatibility.py
├── coreloopthree/                 # CoreLoopThree 双思考系统集成（2 个文件）
│   ├── test_cognition_execution.py
│   └── test_memory_evolution.py
├── memoryrovol/                   # MemoryRovol 记忆系统集成（2 个文件）
│   ├── test_layers.py
│   └── test_retrieval.py
└── syscall/                       # 系统调用层集成
    └── test_syscalls.py
```

## 运行方式

```bash
# C 集成测试
cd build && ctest -R e2e_core

# Python 集成测试
pytest tests/integration/python/ -v -m integration
pytest tests/integration/coreloopthree/ -v
pytest tests/integration/memoryrovol/ -v
pytest tests/integration/syscall/ -v
```

## 测试场景

| 场景 | 涉及的 agentos/ 模块 | 验证目标 |
|------|---------------------|----------|
| **端到端核心** | `atoms/` → `daemon/` → `gateway/` | 完整请求链路 |
| **协议兼容** | `gateway/`, `protocols/` | HTTP/WS/Stdio → JSON-RPC 2.0 转换 |
| **双思考系统** | `atoms/coreloopthree/` | 认知环/执行环/学习环协作 |
| **记忆检索** | `atoms/memoryrovol/` | L1-L4 层检索与缓存机制 |
| **系统调用** | `atoms/syscall/` | 5 类接口、4 层保护 |

---

© 2026 SPHARX Ltd. All Rights Reserved.