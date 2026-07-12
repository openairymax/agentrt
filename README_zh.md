# AgentRT — 极境智能体运行底座平台工程 (AirymaxAgentRT)

> 面向 AI 智能体团队的奠基级运行时平台工程 — 定位对标 JVM/containerd 之于语言/容器。
> [airymaxhub](https://atomgit.com/openairymax/airymaxhub) 伞仓下的管理仓之一，以 git submodule 形式聚合 7 个叶子仓。

**语言:** [English](README.md) | 简体中文

[![Version](https://img.shields.io/badge/version-0.1.1-5a6b7e)](https://atomgit.com/openairymax/agentrt)
[![License](https://img.shields.io/badge/license-AGPL--3.0+Apache--2.0-4a90d9)](LICENSE)
[![C11](https://img.shields.io/badge/C-11-00599C?logo=c&logoColor=white)](https://en.cppreference.com/w/c/11)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=c%2B%2B&logoColor=white)](https://isocpp.org)

---

## 概述

**AgentRT**（全称：**极境智能体运行底座平台工程**，英文 **AirymaxAgentRT**，*AI Agent Runtime Platform Engineering*）是 Airymax 平台的运行时工程层 — 面向 AI 智能体团队的操作系统级运行底座，定位对标 JVM 之于编程语言、containerd 之于容器。JVM 为字节码提供虚拟机，containerd 为容器提供运行时，AgentRT 则为智能体团队提供编排、调度、隔离与观测的平台工程机制。**0.1.1** 是唯一奠基版本，所有后续 Airymax 发行版均在此基础上构建。

本仓库是**管理仓**（git superproject），以 git submodule 形式聚合 **7 个叶子仓**，并继承原 AgentRT 单体仓库的**全部 git 历史**。仓库 URL 沿用历史名称 `git@atomgit.com:openairymax/agentos.git` 以保持提交连续性。AgentRT 暴露大规模运行智能体团队所需的操作系统级机制：微核心原语、认知循环、记忆分层、安全穹顶、IPC 协议、网关服务与长驻守护进程。

AgentRT 是 `airymaxhub` 伞仓下**五个管理仓之一**（其余四个为 `sdk`、`ecosystem`、`products`、`agentrt-linux`）。Airymax 工作区共拆分为 38 个仓库：1 个伞仓 + 5 个管理仓 + 29 个叶子仓 + 3 个顶层仓。每个叶子仓可独立构建与版本控制，管理仓通过 git submodule 将它们钉合在一起，产出连贯、可复现的运行时平台。

## 仓库结构

```
airymaxhub/                     ← 伞仓（git superproject 根）
├── agentrt/                    ← 本仓库（管理仓）
│   ├── atoms/                  ← submodule：微核心原语（A 类）
│   ├── commons/                ← submodule：共享基础工具库（A 类）
│   ├── cupolas/                ← submodule：安全穹顶（B 类）
│   ├── heapstore/              ← submodule：堆式存储（A 类）
│   ├── protocols/              ← submodule：AgentsIPC & A2A/A2T 协议栈
│   ├── gateway/                ← submodule：HTTP/WS/Stdio → JSON-RPC 2.0 网关
│   ├── daemons/                ← submodule：12 个运行时守护进程
│   ├── contracts/              ← 契约头文件（符号链接 → atoms/contracts）
│   ├── CMakeLists.txt          ← 顶层 CMake 入口
│   └── Doxyfile                ← API 文档配置
├── sdk/                        ← SDK 管理仓
├── ecosystem/                  ← 生态管理仓
├── products/                   ← 产品管理仓
├── agentrt-linux/              ← AgentRT-Linux 管理仓
├── cmake/                      ← 共享 CMake 模块（5 通用 + 4 AgentRT 专用）
├── devtools/                   ← 开发工具
├── docs/                       ← 开放文档
└── docs-closed/                ← 内部文档
```

## 叶子仓

| 模块 | 仓库 URL | 分类 | 说明 |
|------|---------|------|------|
| **atoms** | `git@atomgit.com:openairymax/atoms.git` | A | 微核心原语：`corekern`、`coreloopthree`、`syscall`、`taskflow`、`frameworks`、`memory` |
| **commons** | `git@atomgit.com:openairymax/commons.git` | A | 共享基础库：24+ 工具模块（logging、sync、memory、string、ipc 等） |
| **cupolas** | `git@atomgit.com:openairymax/cupolas.git` | B | 安全穹顶：四层内生安全（沙箱、RBAC、净化、审计） |
| **heapstore** | `git@atomgit.com:openairymax/heapstore.git` | A | 堆式运行时数据持久化 |
| **protocols** | `git@atomgit.com:openairymax/protocols.git` | — | AgentsIPC（128 字节消息头）& A2A/A2T 协议栈 |
| **gateway** | `git@atomgit.com:openairymax/gateway.git` | — | HTTP/WS/Stdio → JSON-RPC 2.0 网关守护进程（`gateway_d`） |
| **daemons** | `git@atomgit.com:openairymax/daemons.git` | — | 12 个运行时守护进程：`gateway_d`、`llm_d`、`tool_d`、`sched_d`、`market_d`、`monit_d`、`channel_d`、`info_d`、`notify_d`、`observe_d`、`hook_d`、`plugin_d` |

> **分类说明：** A = 基础/原子层（被上层依赖）；B = 行为/安全层；— = 服务/组合层。

## 架构（分层）

AgentRT 采用循环分层架构。每一层仅依赖其下层；支撑层提供统一基础库，SDK 层最终回绑至此，闭合循环。

```
⬇️  SDK 层     — Python / Go / Rust / TypeScript SDK                          (sdk/ 仓)
⇅   服务层     — 12 个守护进程服务                                            (daemons/)
⇅   协议层     — AgentsIPC & A2A/A2T 协议栈                                   (protocols/)
⇅   网关层     — HTTP / WS / Stdio → JSON-RPC 2.0 网关守护进程                (gateway/)
⇅   存储层     — 堆式运行时数据持久化                                         (heapstore/)
⇅   安全层     — 四层内生安全穹顶                                             (cupolas/)
⇅   内核层     — 7 个原子微内核模块                                           (atoms/)
⇅   支撑层     — 统一基础库（24+ 工具模块）                                    (commons/)
⬆️  SDK 层     — （循环）SDK 回绑基础库并向上暴露给消费者                      (sdk/ 仓)
```

**各层职责：**

- **SDK 层** — 多语言绑定（Python/Go/Rust/TypeScript），向智能体开发者暴露 AgentRT API。位于栈顶，通过依赖支撑层基础库闭合循环。
- **服务层** — 12 个长驻守护进程，实现运行时编排：调度、工具分发、LLM 桥接、监控、通知与插件管理。
- **协议层** — AgentsIPC（固定 128 字节消息头）用于进程内与跨进程消息传递，以及 A2A（智能体间）与 A2T（智能体-工具）协议栈。
- **网关层** — `gateway_d` 将 HTTP、WebSocket、stdio 传输统一翻译为 JSON-RPC 2.0 流，提供进入运行时的外部入口。
- **存储层** — `heapstore` 提供堆式持久化，承载运行时状态、智能体记忆与瞬态数据。
- **安全层** — `cupolas` 实施四层内生安全：沙箱隔离、RBAC 授权、输入输出净化、审计日志。
- **内核层** — `atoms` 包含 7 个原子微内核模块（`corekern`、`coreloopthree`、`syscall`、`taskflow`、`frameworks`、`memory`），提供调度、认知循环与记忆原语。
- **支撑层** — `commons` 提供 24+ 共享工具模块（日志、同步、内存、字符串处理、IPC 助手等），是所有其他层的构建基础。

## 构建

### 前置条件

- **操作系统**：Ubuntu 22.04+ / macOS 13+ / Windows 11 (WSL2)
- **编译器**：GCC 11+ / Clang 14+（要求 C11 与 C++17）
- **构建工具**：CMake 3.20+，Ninja（推荐）或 Make
- **依赖库**：libsqlite3-dev、libcjson-dev、libyaml-dev、libcurl4-openssl-dev、libssl-dev

### 构建步骤

```bash
# 1. 克隆伞仓（递归拉取所有 submodule）
git clone --recursive git@atomgit.com:openairymax/airymaxhub.git
cd airymaxhub/agentrt

# 2. 配置（源树外构建为强制要求 — 由 BAN-33 强制执行）
cmake -S . -B /tmp/agentrt-build \
    -DCMAKE_BUILD_TYPE=Release \
    -DAIRY_WITH_MEMORYROVOL=ON

# 3. 并行构建
cmake --build /tmp/agentrt-build --parallel $(nproc)

# 4. 运行测试套件
cd /tmp/agentrt-build && ctest --output-on-failure
```

> **BAN-33：** 禁止源码树内构建。构建目录必须位于源码树之外；CMake 检测到构建目录位于源码树内时将发出 `FATAL_ERROR`。

### 关键 CMake 选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `BUILD_TESTS` | ON | 构建单元测试（在顶层启用 CTest） |
| `BUILD_SHARED_LIBS` | OFF | 构建动态库而非静态库 |
| `AIRY_BUILD_ALL` | ON | 构建全部 AgentRT 组件 |
| `AIRY_WITH_MEMORYROVOL` | OFF | 启用 MemoryRovol 商业记忆提供者 |
| `AIRY_MEMORY_BACKEND` | `builtin` | 记忆后端选择（`builtin` \| `memoryrovol`） |
| `AIRY_COMPLIANCE_STRICT` | ON | 严格合规模式（投毒不安全函数，如 `strcpy`） |
| `ENABLE_SANITIZERS` | ON | 启用 ASan + LSan + UBSan |
| `ENABLE_COVERAGE` | OFF | 启用代码覆盖率报告 |
| `WARNINGS_AS_ERRORS` | OFF | 将编译器警告视为错误 |
| `AIRY_DOCKER_BUILD` | OFF | Docker 构建模式 |

## 分支策略

- **本管理仓**：仅 `main` 分支 — 稳定，按发行版打标签。
- **叶子仓**：`feature/official-hubs-01` — 各 submodule 跟踪的活跃开发分支。

`.gitmodules` 中所有 7 个叶子仓的 submodule 钉均指向 `feature/official-hubs-01`。管理仓的 `main` 分支记录每个 submodule 应解析的确切提交，确保 Airymax 0.1.1 奠基版本构建的可复现性。

## 许可证

采用 **AGPL v3 + Apache 2.0** 双许可证（SPDX 标识：`AGPL-3.0-or-later OR Apache-2.0`）。详见 [LICENSE](LICENSE)。

接收方可选择任一许可证来约束其对 AgentRT 的使用：AGPL v3 适用于衍生网络服务；Apache 2.0 适用于专有集成。

Copyright (c) 2025-2026 **SPHARX Ltd.** All Rights Reserved.
