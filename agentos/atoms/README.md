# Atoms 微内核原子组件层

`agentos/atoms/`

**版本**: v0.0.5

---

## 概述

Atoms 层是 AgentOS 的**核心系统层**，位于四层架构的最底层，直接构建于操作系统之上。它遵循微内核设计原则，以最小化核心、最大化可扩展性为宗旨，为上层 Daemon 服务和应用程序提供稳定、高效的底层运行时支撑。

Atoms 层封装了系统资源（CPU、内存、存储、网络），将其抽象为统一的智能体运行时能力，包括进程间通信、内存管理、任务调度、时间服务、系统调用、任务编排、核心运行时循环和框架集成等原子化服务。所有模块均以 C 语言实现，通过 C ABI 导出，确保跨语言 FFI 兼容性和极致性能。

---

## 架构定位

AgentOS 采用四层分层架构设计，Atoms 层位于最底层：

```
┌──────────────────────────────────────────────┐
│             Applications (OpenLab)            │
├──────────────────────────────────────────────┤
│             Ecosystem (Toolkit/SDK)           │
├──────────────────────────────────────────────┤
│              Daemon Services                  │
├──────────────────────────────────────────────┤
│          ★ Atoms (Core System Layer) ★       │
├──────────────────────────────────────────────┤
│            Operating System / Hardware        │
└──────────────────────────────────────────────┘
```

Atoms 层直接构建于操作系统之上，通过封装系统资源为上层提供统一的智能体运行时抽象。它是整个 AgentOS 的基石，所有上层能力均依赖 Atoms 层提供的基础服务。

---

## 目录结构

```
atoms/
├── CMakeLists.txt              # 顶层构建配置（INTERFACE 库 + 子目录聚合）
├── README.md                   # 本文件
├── corekern/                   # 微内核 — IPC/Binder、内存、调度、时间
│   ├── include/                # 公共头文件（agentos.h 统一入口）
│   ├── src/                    # 内核实现（ipc/、mem/、task/、time/、observability/）
│   ├── tests/                  # 单元测试
│   └── CMakeLists.txt
├── coreloopthree/              # 核心运行时 — 认知/执行/记忆三循环引擎
│   ├── include/                # 公共头文件（loop.h、cognition.h、execution.h、memory.h）
│   ├── src/                    # 运行时实现
│   │   ├── cognition/          # 认知引擎（意图解析、规划、协同、调度、进化）
│   │   ├── execution/          # 执行引擎（执行单元、补偿事务、追踪）
│   │   ├── memory/             # 记忆引擎（记忆服务、引擎核心）
│   │   └── utils/              # 工具函数（ID 生成、错误处理）
│   ├── tests/                  # 单元测试
│   └── CMakeLists.txt
├── memory/                     # 内置记忆子系统 — L1+L2 可拔插架构
│   ├── memory_provider.h       # 提供商接口（函数指针表 + 能力标记）
│   ├── builtin_provider.c      # 内置免费提供商实现
│   ├── builtin_storage.c       # L1 原始存储（文件系统 + SQLite）
│   ├── builtin_index.c         # L2 特征索引（FAISS 向量搜索）
│   ├── builtin_retrieval.c     # 检索策略实现
│   └── CMakeLists.txt
├── memoryrovol/                # 商业记忆桥接 — L1-L4 PRO（独立仓库）
│   ├── agentos_mr_force_includes.h
│   └── CMakeLists.txt
├── syscall/                    # 系统调用层 — 5 类调用 + 4 级保护
│   ├── include/                # syscalls.h 公共接口
│   ├── src/                    # 系统调用实现（含沙箱、熔断、限流）
│   ├── tests/                  # 单元测试
│   └── CMakeLists.txt
├── taskflow/                   # 任务流引擎 — Pregel 超步模型 DAG 编排
│   ├── include/                # taskflow.h、taskflow_types.h 等
│   ├── src/                    # 引擎实现（核心、图、Pregel、工作流模式）
│   ├── tests/                  # 单元测试
│   └── CMakeLists.txt
└── frameworks/                 # 框架集成层 — 五大框架统一抽象
    ├── include/                # agentos_frameworks.h
    ├── src/                    # 框架管理器实现
    └── CMakeLists.txt
```

---

## 核心组件说明

| 组件 | 路径 | 核心能力 | 语言 | 详细文档 |
|------|------|----------|------|----------|
| **CoreKern** | `corekern/` | IPC/Binder、内存管理、任务调度、时间服务、可观测性 | C11 | [corekern/README.md](corekern/README.md) |
| **CoreLoopThree** | `coreloopthree/` | 认知循环、执行循环、记忆循环、补偿事务、认知进化 | C11 | [coreloopthree/README.md](coreloopthree/README.md) |
| **Memory** | `memory/` | L1 原始存储 + L2 关键词索引，可拔插提供商架构 | C11 | [memory/README.md](memory/README.md) |
| **MemoryRovol** | `memoryrovol/` | L1-L4 全功能记忆（商业），桥接内置 Memory | C11 | 独立仓库 |
| **Syscall** | `syscall/` | 任务/内存/会话/遥测/代理 5 类调用 + Skill 管理 | C11 | [syscall/README.md](syscall/README.md) |
| **TaskFlow** | `taskflow/` | Pregel 超步模型、DAG 编排、检查点容错 | C11 | [taskflow/README.md](taskflow/README.md) |
| **Frameworks** | `frameworks/` | Agent/Memory/Task/Safety/Tool 五大框架统一抽象 | C11 | [frameworks/README.md](frameworks/README.md) |

---

## 模块关系

```
                    ┌─────────────┐
                    │  CoreKern   │
                    │  (微内核)    │
                    └──────┬──────┘
                           │
           ┌───────────────┼───────────────┐
           │               │               │
           ▼               ▼               ▼
    ┌───────────┐   ┌───────────┐   ┌───────────┐
    │ Syscall   │   │ TaskFlow  │   │ CoreLoop  │
    │ (系统调用) │◄──┤ (任务流)   │◄──┤ Three     │
    └───────────┘   └───────────┘   │ (运行时)   │
                                    └──────┬────┘
           ┌───────────┐                   │
           │ Memory    │◄──────────────────┘
           │ (内置记忆) │
           └─────┬─────┘
                 │ (桥接)
           ┌───────────┐
           │ Memory    │
           │ Rovol     │
           │ (商业桥接) │
           └───────────┘

    ┌──────────────────────────────────────┐
    │            Frameworks                │
    │  (Agent / Memory / Task / Safety /   │
    │   Tool 五大框架统一抽象)              │
    └──────────────────────────────────────┘
```

- **CoreKern** 为所有模块提供基础内核服务（IPC、内存、调度、时间、可观测性）
- **Syscall** 基于 CoreKern 提供统一的系统调用接口，包含沙箱隔离和熔断保护
- **CoreLoopThree** 使用 Syscall 和 TaskFlow 执行任务，使用 Memory 管理记忆
- **Memory** 提供内置 L1+L2 记忆能力，通过可拔插提供商架构支持扩展
- **MemoryRovol** 通过桥接扩展至 L1-L4，需 CMake 选项激活
- **TaskFlow** 在 CoreKern 调度之上提供基于 Pregel 超步模型的高级任务编排
- **Frameworks** 通过统一抽象层接入五大框架（Agent/Memory/Task/Safety/Tool）

---

## 设计原则

| 原则 | 说明 |
|------|------|
| **微内核** | 最小化核心，最大化服务化；内核仅含 IPC、内存、调度、时间四个原子能力 |
| **确定性** | 确定性的调度和资源管理，支持优先级继承和资源预留 |
| **可组合** | 模块间通过明确定义的 C ABI 接口组合，支持跨语言 FFI |
| **无锁设计** | 尽量减少锁竞争，核心路径使用原子操作和无锁数据结构 |
| **零拷贝** | 核心 IPC 数据路径上的零拷贝设计（共享内存 + 消息传递） |
| **异步优先** | 非阻塞 I/O 和事件驱动，支持同步/异步双模式调用 |
| **安全编译** | 启用栈保护、FORTIFY_SOURCE、PIE、RELRO 等安全编译选项 |

---

## 模块依赖

| 模块 | 依赖 | 被依赖 |
|------|------|--------|
| CoreKern | OS、commons、agentos_platform | 所有模块 |
| CoreLoopThree | CoreKern、Syscall、Memory、TaskFlow | Daemon 服务 |
| Memory | CoreKern、commons | CoreLoopThree |
| MemoryRovol | CoreKern、Memory | CoreLoopThree（可选） |
| Syscall | CoreKern、commons | CoreLoopThree、TaskFlow |
| TaskFlow | CoreKern、Syscall | CoreLoopThree |
| Frameworks | 所有模块 | 外部框架、Daemon 服务 |

---

## 构建说明

Atoms 层通过 CMake 构建，顶层 `CMakeLists.txt` 聚合所有子模块：

```bash
# 标准构建
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 启用 MemoryRovol 商业桥接
cmake -B build -DAGENTOS_WITH_MEMORYROVOL=ON

# 启用单元测试
cmake -B build -DBUILD_TESTS=ON
ctest --test-dir build
```

构建产物为静态库 `agentos_core`（CoreKern）及各子模块库，通过 `agentos_atoms` INTERFACE 目标统一导出头文件路径。

---

## API 版本管理

所有模块遵循语义化版本（MAJOR.MINOR.PATCH），在相同 MAJOR 版本内保证 ABI 兼容。各模块 API 版本定义于对应头文件中：

- CoreKern: `AGENTOS_CORE_API_VERSION` (1.0.0)
- CoreLoopThree: `LOOP_API_VERSION` (1.0.0)
- Syscall: `SYSCALL_API_VERSION` (1.0.0)

---

© 2026 SPHARX Ltd. All Rights Reserved.
