# AgentRT 极境智能体运行底座平台工程 (AirymaxRT)

> 面向 AI 智能体团队的奠基级运行时平台工程 — 定位类比 Kubernetes 之于微服务：为 Agent 团队提供标准化编排、调度、通信、记忆与安全的完整平台范式。
> [airymaxhub](https://atomgit.com/openairymax/airymaxhub) 伞仓下用户态工程大管理仓 `agent-workload`（v0.1.4 由 `agent-runtim` 改名）内的管理仓，以 git submodule 形式聚合 7 个叶子仓。

**语言:** [English](README.md) | 简体中文

[![Version](https://img.shields.io/badge/version-0.1.6b-5a6b7e)](https://atomgit.com/openairymax/agentrt)
[![License](https://img.shields.io/badge/license-AGPL--3.0+Apache--2.0-4a90d9)](LICENSE)
[![C11](https://img.shields.io/badge/C-11-00599C?logo=c\&logoColor=white)](https://en.cppreference.com/w/c/11)

***

## 概述

**AgentRT**（全称：**极境智能体运行底座平台工程**，英文 **AirymaxAgentRT**，*AI Agent Runtime Platform Engineering*）是 Airymax 平台的运行时工程层 — 面向 AI 智能体团队的操作系统级运行底座。定位类比 Kubernetes 之于微服务：AgentRT 将多智能体的认知循环、记忆演化、安全隔离、协议互通标准化为一套运行时平台，定义了 AI Agent 团队的运行方式。

本仓库是**管理仓**（git superproject），以 git submodule 形式聚合 **7 个叶子仓**，并继承原 AgentRT 单体仓库的**全部 git 历史**，以保持提交连续性。

AgentRT 是 `airymaxhub` 伞仓下用户态工程大管理仓 `agent-workload` 中的**管理仓**（伞仓同级另有内核态 `agent-linux`，以及顶层仓 `docs`、`closed-docs`、`tools`、`closed-dev-build`）。每个叶子仓可独立构建与版本控制，管理仓通过 git submodule 将它们钉合在一起，产出连贯、可复现的运行时平台。

## 快速开始

### 一键安装（终端用户）

终端用户一行安装，无需编译。安装器始终选择**最新 release**
（`releases/download/latest/`），命令永不因版本升级而变动；自动完成
GPG 验签 + sha256 校验 + 架构自检（x86_64 / aarch64 / riscv64）：

```bash
# Linux / macOS
curl -fsSL "https://atomgit.com/openairymax/agentrt/releases/download/latest/install.sh" | bash

# Windows（atoms/commons 已闭源，原生安装不可用——推荐 WSL2）：
#   wsl --install 安装 WSL2 + Ubuntu，在 WSL 终端执行上方 Linux 一行命令
#   即可完整使用（含闭源模块的 Linux 完全体二进制包）。
#   Windows 预编译包 / atoms 预编译模块暂未发布，PowerShell 安装器会回退
#   源码构建并因缺少闭源模块失败（0.1.6b 已知限制）。
powershell -ExecutionPolicy Bypass -Command "irm https://atomgit.com/openairymax/agentrt/releases/download/latest/install.ps1 | iex"
```

常用变体：

```bash
# 自定义安装路径（默认 $HOME/.airymaxrt）
curl -fsSL "https://atomgit.com/openairymax/agentrt/releases/download/latest/install.sh" | \
   bash -s -- --prefix "$HOME/.airymaxrt"

# 测试通道（更激进更新）
curl -fsSL "https://atomgit.com/openairymax/agentrt/releases/download/latest/install.sh" | bash -s -- --channel beta

# 卸载（--keep-data 保留记忆数据）
curl -fsSL "https://atomgit.com/openairymax/agentrt/releases/download/latest/install.sh" | bash -s -- --uninstall
```

安装完成后 `airymaxrt` 即入 PATH，`airymaxrt start` 拉起运行时。安装器
自动按硬件裁剪运行画像（full/minimal），`airymaxrt monitor` 常驻检测外设
增强（加内存/插显卡）后自动恢复被裁剪功能。

## 核心能力

AgentRT 面向终端用户与智能体开发者提供以下能力：

| 能力 | 说明 |
|------|------|
| **对话与任务双模式** | 面向聊天的对话模式与面向复杂任务的执行模式，统一由 `airy_cli` / `agentrt-tui` 驱动；同一套指令在大任务场景自动进入规划-执行闭环 |
| **多智能体协作** | 支持一个主 Agent 编排多个子 Agent（执行体），并行拆分与汇总复杂工作；多 Agent 间经标准协议互通 |
| **智能规划与反思** | 对模糊目标进行交互式确认，规划后生成可执行任务图并调度执行；执行过程含自我反思与失败重调度 |
| **持久记忆** | 内置免费记忆后端（L1 会话级 + L2 跨会话），支持遗忘与检索；可扩展商业记忆提供方 |
| **内置工具回路** | 聊天回路向 LLM 暴露 `web_search` / `web_fetch` 等真实工具（Bing 搜索 + URL 抓取），以 `role="tool"` 回填并 markdown 渲染最终回复 |
| **安全穹顶** | 四层内生安全：沙箱隔离、RBAC 授权、输入输出净化、审计日志，默认 fail-closed |
| **统一网关** | `gateway_d` 将 HTTP、WebSocket、stdio 统一翻译为 JSON-RPC 2.0 流，外部系统经单一入口接入运行时 |
| **18 个运行时服务** | 调度、工具分发、LLM 桥接、双思考、记忆、监控、通知、插件管理、Agent 执行等长期驻留守护进程，按依赖顺序编排启动并自愈 |
| **可观测** | 全局事件流 + 任务看板 + 健康监控，`airymaxrt monitor` 提供运行状态总览 |

## 仓库结构

```
airymaxhub/                     ← 伞仓（git superproject 根）
├── agent-workload/             ← 用户态工程大管理仓（v0.1.4 由 agent-runtim 改名）
│   ├── agentrt/                ← 本仓库（管理仓）
│   │   ├── atoms/              ← submodule：微核心原语
│   │   ├── commons/            ← submodule：共享基础工具库
│   │   ├── cupolas/            ← submodule：安全穹顶
│   │   ├── heapstore/          ← submodule：堆式存储
│   │   ├── protocols/          ← submodule：AgentsIPC & A2A/A2T 协议栈
│   │   ├── gateway/            ← submodule：HTTP/WS/Stdio → JSON-RPC 2.0 网关
│   │   ├── daemons/            ← submodule：18 个运行时守护进程
│   │   ├── cmake/              ← 构建系统模块
│   │   ├── scripts/            ← 官方安装器 install.sh/install.ps1
│   │   ├── CMakeLists.txt      ← 顶层 CMake 入口
│   │   └── Doxyfile            ← API 文档配置
│   ├── sdk/                    ← SDK 管理仓（Python/Go/Rust/TypeScript）
│   ├── ecosystem/              ← 生态管理仓
│   └── products/               ← 产品管理仓
├── agent-linux/                ← 内核态工程大管理仓（原 agentrt-linux，v0.1.3 改名）
├── tools/                      ← 开发工具（v0.1.4 由 devtools 改名）
├── docs/                       ← 开放文档
├── closed-docs/                ← 内部文档
└── closed-dev-build/           ← 内部构建/部署
```

## 叶子仓

| 模块            | 仓库 URL                                      | 说明                                                                                                                              |
| ------------- | ------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------- |
| **atoms**     | `git@atomgit.com:openairymax/atoms.git`     | 微核心原语（A 类）                                                                                                                   |
| **commons**   | `git@atomgit.com:openairymax/commons.git`   | 共享基础库（A 类）                                                                                                                   |
| **cupolas**   | `git@atomgit.com:openairymax/cupolas.git`   | 安全穹顶：四层内生安全（B 类）                                                                                                          |
| **heapstore** | `git@atomgit.com:openairymax/heapstore.git` | 堆式运行时数据持久化（A 类）                                                                                                            |
| **protocols** | `git@atomgit.com:openairymax/protocols.git` | AgentsIPC（128 字节消息头）& A2A/A2T 协议栈                                                                                           |
| **gateway**   | `git@atomgit.com:openairymax/gateway.git`   | HTTP/WS/Stdio → JSON-RPC 2.0 网关守护进程（`gateway_d`）                                                                          |
| **daemons**   | `git@atomgit.com:openairymax/daemons.git`   | 18 个运行时守护进程：`gateway_d`、`llm_d`、`tool_d`、`sched_d`、`market_d`、`monit_d`、`channel_d`、`info_d`、`notify_d`、`observe_d`、`hook_d`、`plugin_d`、`mem_d`、`agent_d`、`a2a_d`、`think_d`、`cupolas_d`、`maths_d` |

> **分类说明：** A = 基础/原子层（被上层依赖）；B = 行为/安全层；— = 服务/组合层。

## 架构（分层）

AgentRT 采用循环分层架构。每一层仅依赖其下层；支撑层提供统一基础库，SDK 层最终回绑至此，闭合循环。

```
⬇️  SDK 层     — Python / Go / Rust / TypeScript SDK                          (sdk/ 仓)
⇅   服务层     — 18 个守护进程服务                                            (daemons/)
⇅   协议层     — AgentsIPC & A2A/A2T 协议栈                                   (protocols/)
⇅   网关层     — HTTP / WS / Stdio → JSON-RPC 2.0 网关守护进程                (gateway/)
⇅   存储层     — 堆式运行时数据持久化                                         (heapstore/)
⇅   安全层     — 四层内生安全穹顶                                             (cupolas/)
⇅   内核层     — 5 个原子微内核模块                                           (atoms/)
⇅   支撑层     — 统一基础库（24+ 工具模块）                                    (commons/)
⬆️  SDK 层     — （循环）SDK 回绑基础库并向上暴露给消费者                      (sdk/ 仓)
```

**各层职责：**

- **SDK 层** — 多语言绑定（Python/Go/Rust/TypeScript），向智能体开发者暴露 AgentRT API。位于栈顶，通过依赖支撑层基础库闭合循环。
- **服务层** — 18 个长驻守护进程，实现运行时编排：调度、工具分发、LLM 桥接、记忆、多智能体协作、监控、通知与插件管理。
- **协议层** — AgentsIPC（固定 128 字节消息头）用于进程内与跨进程消息传递，以及 A2A（智能体间）与 A2T（智能体-工具）协议栈。
- **网关层** — `gateway_d` 将 HTTP、WebSocket、stdio 传输统一翻译为 JSON-RPC 2.0 流，提供进入运行时的外部入口。
- **存储层** — `heapstore` 提供堆式持久化，承载运行时状态、智能体记忆与瞬态数据。
- **安全层** — `cupolas` 实施四层内生安全：沙箱隔离、RBAC 授权、输入输出净化、审计日志。
- **内核层** — `atoms` 包含 5 个原子微内核模块（`corekern`、`coreloopthree`、`syscall`、`taskflow`、`memory`），提供调度、认知循环与记忆原语。
- **支撑层** — `commons` 提供 24+ 共享工具模块（日志、同步、内存、字符串处理、IPC 助手等），是所有其他层的构建基础。

## 构建

### 前置条件

- **操作系统**：Ubuntu 22.04+ / macOS 13+ / Windows 11 (WSL2)
- **编译器**：GCC 11+ / Clang 14+（要求 C11）
- **构建工具**：CMake 3.20+，Ninja（推荐）或 Make
- **依赖库**：libsqlite3-dev、libcjson-dev、libyaml-dev、libcurl4-openssl-dev、libssl-dev

> 注：`atoms` / `commons` 为闭源模块。从源码构建需要授权访问对应子仓，或使用官方发布的完全体二进制包（见上文"一键安装"）。

### 构建步骤

```bash
# 1. 克隆伞仓（递归拉取所有 submodule）
git clone --recursive git@atomgit.com:openairymax/airymaxhub.git
cd airymaxhub/agentrt

# 2. 配置（源树外构建为强制要求）
cmake -S . -B /tmp/agentrt-build \
    -DCMAKE_BUILD_TYPE=Release \
    -DAIRY_WITH_MEMORYROVOL=ON

# 3. 并行构建
cmake --build /tmp/agentrt-build --parallel $(nproc)

# 4. 运行测试套件
cd /tmp/agentrt-build && ctest --output-on-failure
```

> **注意：** 构建必须采用源码树外（out-of-source）方式。构建目录必须位于源码树之外；CMake 检测到构建目录位于源码树内时将发出 `FATAL_ERROR`。

### 关键 CMake 选项

| 选项                       | 默认值       | 说明                                 |
| ------------------------ | --------- | ---------------------------------- |
| `BUILD_TESTS`            | ON        | 构建单元测试（在顶层启用 CTest）                |
| `BUILD_SHARED_LIBS`      | OFF       | 构建动态库而非静态库                         |
| `AIRY_BUILD_ALL`         | ON        | 构建全部 AgentRT 组件                    |
| `AIRY_WITH_MEMORYROVOL`  | OFF       | 启用 MemoryRovol 商业记忆提供者             |
| `AIRY_MEMORY_BACKEND`    | `builtin` | 记忆后端选择（`builtin` \| `memoryrovol`） |
| `BUILD_CLI`              | ON        | 构建交互式产品 CLI                    |
| `AIRY_COMPLIANCE_STRICT` | ON        | 严格合规模式（投毒不安全函数，如 `strcpy`）         |
| `ENABLE_SANITIZERS`      | ON        | 启用 ASan + LSan + UBSan             |
| `ENABLE_COVERAGE`        | OFF       | 启用代码覆盖率报告                          |
| `WARNINGS_AS_ERRORS`     | OFF       | 将编译器警告视为错误                         |

## 发布记录

- **v0.1.6b** — 修复二进制分发的跨系统可移植性（运行库自包含、兼容旧版 Linux 发行版），并修复窄终端下的界面渲染问题。
- **v0.1.6 / v0.1.6a** — 修复 daemon 群一键启动与更新路径。
- **v0.1.5a** — 修复 riscv64 构建链、更新器与安装器问题。

完整变更见 [CHANGELOG.md](CHANGELOG.md)。

## 分支策略

- **本管理仓**：仅 `main` 分支 — 稳定，按发行版打标签。
- **叶子仓**：`feature/official-hubs-01` — 各 submodule 跟踪的活跃开发分支。

`.gitmodules` 中所有 7 个叶子仓的 submodule 钉均指向 `feature/official-hubs-01`。管理仓的 `main` 分支记录每个 submodule 应解析的确切提交，确保每个 Airymax 发行版的构建可复现性。

## 许可证

采用 **AGPL v3 + Apache 2.0** 双许可证（SPDX 标识：`AGPL-3.0-or-later OR Apache-2.0`）。详见 [LICENSE](LICENSE)。

接收方可选择任一许可证来约束其对 AgentRT 的使用：AGPL v3 适用于衍生网络服务；Apache 2.0 适用于专有集成。

### 双许可证使用指南

你可以**任选其一**适用——不是同时遵守两个，也不是都不遵守。

**SPDX 表达式**：`AGPL-3.0-or-later OR Apache-2.0`

| 你的场景                       | 选择             | 原因                |
| -------------------------- | -------------- | ----------------- |
| 构建**SaaS 网络服务**并修改 AgentRT | **AGPL v3**    | 网络服务条款要求公开修改后的源代码 |
| 开发**开源衍生作品**（copyleft 项目）  | **AGPL v3**    | 衍生作品必须同样以 AGPL 开源 |
| 在**商业闭源产品**中集成 AgentRT     | **Apache 2.0** | 宽松许可证，允许闭源衍生      |
| 构建**企业内部工具**               | **Apache 2.0** | 无需公开源代码           |
| 需要**专利保护**                 | **Apache 2.0** | 贡献者明确授予专利使用权      |
| 仅用于学习与研究                   | **任一**         | 两者均允许个人使用         |

权威许可证政策见 [12-license-policy.md](../docs/AirymaxOS/50-engineering-standards/12-license-policy.md)。

Copyright (c) 2025-2026 **SPHARX Ltd.** All Rights Reserved.
