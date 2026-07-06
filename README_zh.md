# AgentRT — AI 智能体运行时平台

> Airymax 运行时工程 — AI 智能体团队的奠基级运行时平台。
> [airymaxhub](https://atomgit.com/openairymax/airymaxhub) 伞仓下四个管理仓之一。

**语言:** [English](README.md) | 简体中文

[![Version](https://img.shields.io/badge/version-0.1.1-5a6b7e)](https://atomgit.com/openairymax/agentos)
[![License](https://img.shields.io/badge/license-AGPL--3.0+Apache--2.0-4a90d9)](LICENSE)
[![C11](https://img.shields.io/badge/C-11-00599C?logo=c&logoColor=white)](https://en.cppreference.com/w/c/11)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=c%2B%2B&logoColor=white)](https://isocpp.org)

---

## 概述

**AgentRT** 是 Airymax 的运行时工程层 — AI 智能体运行时平台，定位对标 JVM/containerd 之于语言/容器的运行底座。它提供编排智能体团队所需的操作系统级机制：微核心原语、认知循环、记忆分层、安全穹顶、IPC 协议、网关服务和守护进程。

本仓库是**管理仓**（git superproject），聚合 7 个叶子仓为 git submodule。它继承了原 AgentRT 单体仓库的全部 git 历史。

## 仓库结构

```
airymaxhub/                 ← 伞仓
├── agentrt/                ← 本仓库（管理仓）
│   ├── atoms/              ← submodule：微核心原语（A 类）
│   ├── commons/            ← submodule：共享工具库（A 类基础）
│   ├── cupolas/            ← submodule：安全穹顶（B 类）
│   ├── heapstore/          ← submodule：堆式存储（A 类）
│   ├── protocols/          ← submodule：AgentsIPC & A2A/A2T 协议
│   ├── gateway/            ← submodule：HTTP/gRPC 网关（gateway_d）
│   ├── daemons/            ← submodule：12 个运行时守护进程
│   ├── contracts/          ← 契约头文件（符号链接到 atoms/contracts）
│   ├── CMakeLists.txt      ← 顶层 CMake 入口
│   └── Doxyfile            ← API 文档配置
├── sdk/                    ← SDK 管理仓
├── ecosystem/              ← 生态管理仓
├── products/               ← 产品管理仓
├── cmake/                  ← 共享 CMake 模块（5 通用 + 4 AgentRT 专用）
├── devtools/               ← 开发工具
├── docs/                   ← 开放文档
└── docs-closed/            ← 内部文档
```

## 叶子仓

| 模块 | 仓库 | 分类 | 说明 |
|------|------|------|------|
| **atoms** | `git@atomgit.com:openairymax/atoms.git` | A | 微核心原语：corekern、coreloopthree、syscall、taskflow、frameworks、memory |
| **commons** | `git@atomgit.com:openairymax/commons.git` | A | 共享工具库：24+ 工具模块（logging、sync、memory、string、ipc 等） |
| **cupolas** | `git@atomgit.com:openairymax/cupolas.git` | B | 安全穹顶：四层内生安全（沙箱、RBAC、净化、审计） |
| **heapstore** | `git@atomgit.com:openairymax/heapstore.git` | A | 堆式运行时数据持久化 |
| **protocols** | `git@atomgit.com:openairymax/protocols.git` | — | AgentsIPC（128 字节消息头）& A2A/A2T 协议栈 |
| **gateway** | `git@atomgit.com:openairymax/gateway.git` | — | HTTP/WS/Stdio → JSON-RPC 2.0 网关守护进程（`gateway_d`） |
| **daemons** | `git@atomgit.com:openairymax/daemons.git` | — | 12 个运行时守护进程：gateway_d、llm_d、tool_d、sched_d、market_d、monit_d、channel_d、info_d、notify_d、observe_d、hook_d、plugin_d |

## 架构（分层）

```
⬇️ 生态层    — 应用 / 配置 / 提示词 / 技能（在 ecosystem/ 仓中）
⇅ 服务层    — 12 个守护进程服务（daemons/）
⇅ 协议层    — AgentsIPC & A2A/A2T（protocols/）
⇅ 网关层    — HTTP/WS/Stdio → JSON-RPC 2.0（gateway/）
⇅ 存储层    — 运行时数据持久化（heapstore/）
⇅ 安全层    — 四层内生安全（cupolas/）
⇅ 内核层    — 7 个原子模块（atoms/）
⇅ 支撑层    — 统一基础库（commons/）
⬆️ SDK 层   — Python/Go/Rust/TypeScript（在 sdk/ 仓中）
```

## 构建

### 前置条件

- **操作系统**：Ubuntu 22.04+ / macOS 13+ / Windows 11 (WSL2)
- **编译器**：GCC 11+ / Clang 14+（C11/C++17）
- **构建工具**：CMake 3.20+
- **依赖库**：libsqlite3-dev、libcjson-dev、libyaml-dev、libcurl4-openssl-dev、libssl-dev

### 构建步骤

```bash
# 1. 克隆（含 submodule，从伞仓克隆）
git clone --recursive git@atomgit.com:openairymax/airymaxhub.git
cd airymaxhub/agentrt

# 2. 配置（BAN-33 强制要求源树外构建）
cmake -S . -B /tmp/agentrt-build -DCMAKE_BUILD_TYPE=Release -DAGENTRT_WITH_MEMORYROVOL=ON

# 3. 构建
cmake --build /tmp/agentrt-build --parallel $(nproc)

# 4. 测试
cd /tmp/agentrt-build && ctest --output-on-failure
```

### 关键 CMake 选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `BUILD_TESTS` | ON | 构建单元测试 |
| `AGENTRT_WITH_MEMORYROVOL` | OFF | 启用 MemoryRovol 商业记忆提供者 |
| `AGENTRT_COMPLIANCE_STRICT` | ON | 严格合规模式（禁止不安全函数） |
| `ENABLE_SANITIZERS` | ON | 启用 ASan + LSan + UBSan |

## 分支策略

- 本管理仓：仅 **`main`** 分支
- 叶子仓：**`feature/official-hubs-01`**（活跃开发）

## 许可证

采用 **AGPL v3 + Apache 2.0** 双许可证（SPDX: `AGPL-3.0-or-later OR Apache-2.0`）。详见 [LICENSE](LICENSE)。

Copyright (c) 2025-2026 **SPHARX Ltd.** All Rights Reserved.
