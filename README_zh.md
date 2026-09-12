# AgentRT 极境智能体运行底座（AirymaxAgentRT）

> 面向 AI 智能体团队的操作系统级运行底座。它把智能体"如何思考、如何执行、
> 如何记忆、如何彼此通信"标准化为一套运行时，角色类比于容器运行时之于微服务。

**语言：** [English](README.md) | 简体中文

[![Version](https://img.shields.io/badge/version-0.1.15-5a6b7e)](https://atomgit.com/openairymax/agentrt/releases/tag/v0.1.15)
[![License](https://img.shields.io/badge/license-AGPL--3.0+Apache--2.0-4a90d9)](LICENSE)
[![C11](https://img.shields.io/badge/C-11-00599C?logo=c&logoColor=white)](https://en.cppreference.com/w/c/11)

---

## 概述

**AgentRT** 是 Airymax 平台的运行时层，为智能体团队在真实硬件上长期运行提供
所需机制：

- 具备调度、系统调用与内存原语的微内核核心；
- 三阶段认知循环（认知 → 执行 → 记忆）；
- 会话级与会话间的分层持久记忆；
- 默认 fail-closed 的四层安全穹顶；
- 进程间通信与智能体互联协议栈；
- 单一网关进程，将 HTTP / WebSocket / SSE / MCP / A2A / OpenAI 统一翻译为
  JSON-RPC 2.0；
- 一组长期驻留的守护进程，负责上述能力的整体编排；
- 用于交互使用的命令行工具与终端界面。

运行时核心以 C11 实现，并提供 Python、Go、Rust、TypeScript 四种语言绑定。
设计目标是：笔记本上的单智能体与规模化多智能体部署，使用完全相同的编程模型。

本仓库是运行时的**发布聚合仓库**。各组件在独立仓库中维护，以 git submodule
形式引入，并被固定到每次发布对应的确切提交，从而任一已发布版本都可以逐提交
复现。

## 安装

### Linux 与 macOS

一行命令，无需编译。安装器自动识别平台、下载匹配的预构建包并完成校验：

```bash
curl -fsSL https://raw.githubusercontent.com/openairymax/agentrt/main/scripts/install.sh | bash
```

若当前网络无法访问 GitHub raw，可使用随发布包一同提供的等价脚本：

```bash
curl -fsSL https://atomgit.com/openairymax/agentrt/releases/download/v0.1.15/install.sh | bash
```

### Windows

x86-64 与 x86-32 均提供原生 PowerShell 安装器：

```powershell
irm https://atomgit.com/openairymax/agentrt/releases/download/v0.1.15/install.ps1 | iex
```

### 可选参数

在 `| bash -s --` 之后追加参数（从文件执行脚本时直接传参即可）：

```bash
# 自定义安装路径（默认 $HOME/.airymaxrt）
... | bash -s -- --prefix "$HOME/.airymaxrt"

# 安装候选版本通道，而非稳定通道
... | bash -s -- --channel rc

# 强制重装（清理下载缓存，并先停止运行中的守护进程）
... | bash -s -- --reinstall

# 卸载；加 --keep-data 可保留记忆数据
... | bash -s -- --uninstall
```

安装根目录会被记录到 `<安装根>/config/install.env`。此后 `airymaxrt update`、
`airymaxrt uninstall` 与 `--reinstall` 都会作用于其自身所属的那份安装，无需
重复传 `--prefix`。同一台机器上存在多份安装时，用 `which airymaxrt` 确认
shell 实际解析到的是哪一份。

### 支持平台

| 平台 | 已发布的架构 |
|------|-------------|
| Linux | x86-64、x86-32、arm-64（aarch64）、arm-32（armv7l） |
| macOS | arm-64（Apple Silicon）、x86-64 |
| Windows | x86-64、x86-32 |

Linux 与 macOS 发布 `.tar.gz`，Windows 发布 `.zip`。未列入上表的架构不在
预构建矩阵内，请从源码构建。

### 首次运行

启动终端界面即可拉起运行时及其服务：

```bash
airymaxrt                 # 终端界面（非交互终端下自动回退为 CLI）
airymaxrt cli             # 强制使用命令行前端
airymaxrt status          # 查看当前运行状态
airymaxrt doctor          # 组件健康检查
airymaxrt logs 100        # 查看最近 100 行运行日志
airymaxrt logs llm_d      # 查看指定守护进程的日志
```

`airymaxrt profile` 用于查看或切换按硬件推导的运行画像（`full`、`minimal`、
`auto`）。安装阶段已为你选好画像；`airymaxrt monitor --daemon` 会持续观察，
后续扩充内存或加装 GPU 时，此前被裁剪的能力会自动恢复。Windows 上上述命令
同样可用。

## 核心能力

| 能力 | 你得到什么 |
|------|-----------|
| **对话与任务双模式** | 面向交流的模式一，面向复杂工作的模式二，均由命令行与终端界面驱动；同一条指令在任务较大时自动进入"先规划、后执行"的闭环。 |
| **多智能体协作** | 一个主智能体编排多个子智能体，并行拆分工作并汇总结果；智能体之间通过公开定义的协议互通。 |
| **规划与反思** | 对含糊目标先做交互式澄清，再编译为可执行任务图并调度；执行过程具备自我反思与失败重调度。 |
| **持久记忆** | 内置免费记忆后端，含 L1 会话级与 L2 跨会话两层，支持检索与遗忘；可插接商业记忆提供方。 |
| **工具回路** | 聊天回路向模型暴露 `web_search`、`web_fetch` 等真实工具，以 tool 消息回填结果，并将最终回复以 markdown 渲染。 |
| **安全穹顶** | 四层：沙箱隔离、RBAC 授权、输入输出净化、审计日志。默认姿态为 fail-closed。 |
| **统一网关** | `gateway_d` 将 HTTP、WebSocket、SSE、MCP、A2A、OpenAI 传输统一为一条 JSON-RPC 2.0 流，外部系统只有唯一一个入口。 |
| **15 个运行时服务** | 覆盖调度、智能体执行、LLM 桥接、工具分发、双思考认知、记忆、市场、监控、通知、通道、A2A、策略执行、数学与钩子的长驻进程，按依赖顺序启动并可自愈。 |
| **可观测** | 全局事件流 + 任务看板 + 健康监控，经 `airymaxrt status` 与 `airymaxrt doctor` 呈现，并按进程落盘日志。 |

## 架构

AgentRT 采用分层结构，每一层只依赖其下的层。SDK 层在栈顶回绑支撑层，因此
依赖关系呈环状。

```
SDK 层      — 命令行、终端界面、Python / Go / Rust / TypeScript 绑定
服务层      — 15 个守护进程，负责运行时编排                     (daemons/)
协议层      — AgentsIPC 与 A2A / A2T 协议栈                     (protocols/)
网关层      — 各类传输 → JSON-RPC 2.0                           (gateway/)
存储层      — 堆式运行时数据持久化                              (heapstore/)
安全层      — 四层穹顶：策略决策点与各进程本地执行点            (cupolas/)
内核层      — 5 个原子微内核模块                                (atoms/)
支撑层      — 32 个内聚工具模块 + 共享头文件                    (commons/)
```

- **支撑层（`commons`）** — 其他各层的共同基础：日志、同步、内存辅助、字符串
  处理、IPC 助手、配置、可观测性等，同时提供权威的类型与错误契约。
- **内核层（`atoms`）** — 5 个原子模块：`corekern`（初始化与生命周期）、
  `coreloopthree`（认知 → 执行 → 记忆循环）、`syscall`（统一的系统调用接口）、
  `taskflow`（任务图与调度）、`memory`（记忆原语与后端）。
- **安全层（`cupolas`）** — 策略决策点负责加载、下发与回滚策略；每个守护进程
  通过自身的本地执行点应用策略，秒级生效。
- **存储层（`heapstore`）** — 承载运行时状态、智能体记忆与瞬态数据的持久化。
- **网关层（`gateway`）** — 只做协议翻译，不含业务逻辑，是进入运行时的唯一
  进程边界。
- **协议层（`protocols`）** — AgentsIPC 使用固定 128 字节消息头，用于进程内与
  跨进程消息传递；另含智能体互联（A2A）与智能体-工具（A2T）协议栈。
- **服务层（`daemons`）** — 15 个长驻进程，使运行时成为一个真正在跑的系统。
- **SDK 层** — 位于同级 [`sdk/`](../sdk) 目录，把下层能力重新暴露给应用开发者。

## 仓库结构

```
agentrt/
├── atoms/                # 微内核原语（submodule）
├── commons/              # 共享基础库（submodule）
├── cupolas/              # 安全穹顶（submodule）
├── gateway/              # 协议网关（submodule）
├── heapstore/            # 堆式持久化（submodule）
├── protocols/            # AgentsIPC、A2A / A2T（submodule）
├── daemons/              # 15 个运行时守护进程 + 框架（submodule）
├── cmake/                # 构建系统模块
├── scripts/              # 安装器 install.sh / install.ps1
├── tests/                # 冒烟测试与工具链自测
├── tools/                # 开发者工具（airy_cli、airy_depgraph、codegen）
├── latest/               # 滚动发布清单与签名密钥
├── RELEASE_NOTES.d/      # 面向用户的版本说明，每个版本一个文件
├── LICENSES/             # 附加许可证全文
├── CMakeLists.txt        # 顶层构建入口
├── VERSION               # 本目录树对应的发布版本
└── Doxyfile              # API 文档配置
```

`dist/` 是本地的构建与打包暂存目录，不随仓库分发。

## 组件

各组件在独立仓库中开发。递归克隆本仓库即可按固定提交取得全部组件。

| 组件 | 仓库 | 职责 |
|------|------|------|
| **atoms** | [openairymax/atoms](https://atomgit.com/openairymax/atoms) | 微内核层：`corekern`、`coreloopthree`、`syscall`、`taskflow`、`memory` |
| **commons** | [openairymax/commons](https://atomgit.com/openairymax/commons) | 类型与错误契约，以及 32 个内聚工具模块 |
| **cupolas** | [openairymax/cupolas](https://atomgit.com/openairymax/cupolas) | 四层安全穹顶：策略决策点与本地执行点 |
| **heapstore** | [openairymax/heapstore](https://atomgit.com/openairymax/heapstore) | 堆式运行时数据持久化 |
| **protocols** | [openairymax/protocols](https://atomgit.com/openairymax/protocols) | AgentsIPC（128 字节消息头）、A2A 与 A2T 协议栈 |
| **gateway** | [openairymax/gateway](https://atomgit.com/openairymax/gateway) | HTTP / WS / SSE / MCP / A2A / OpenAI → JSON-RPC 2.0 |
| **daemons** | [openairymax/daemons](https://atomgit.com/openairymax/daemons) | `gateway_d`、`agent_d`、`llm_d`、`tool_d`、`sched_d`、`think_d`、`mem_d`、`market_d`、`monit_d`、`notify_d`、`channel_d`、`a2a_d`、`cupolas_d`、`maths_d`、`hook_d` |

## 从源码构建

### 前置条件

| 项目 | 最低要求 |
|------|---------|
| 操作系统 | Ubuntu 22.04+、macOS 13+，或带 MSVC 的 Windows |
| C 编译器 | GCC 11+ 或 Clang 14+（要求 C11） |
| 构建工具 | CMake 3.20+，Ninja（推荐）或 Make |
| 依赖库 | SQLite3、cJSON、libyaml、libcurl、OpenSSL；网关另需 libmicrohttpd 与 libwebsockets |
| 可选 | Rust（终端界面）、Python 3（随包脚本运行时） |

Windows 上的依赖建议通过 [vcpkg](https://github.com/microsoft/vcpkg) 获取：
`sqlite3 cjson libyaml curl openssl zlib libmicrohttpd libwebsockets libevent nghttp2`。

### 构建步骤

```bash
# 1. 递归克隆，取回全部组件
git clone --recursive https://atomgit.com/openairymax/agentrt.git
cd agentrt

# 2. 配置——构建目录必须位于源码树之外
cmake -S . -B ../agentrt-build -DCMAKE_BUILD_TYPE=Release

# 3. 构建
cmake --build ../agentrt-build --parallel "$(nproc)"

# 4. 测试
cd ../agentrt-build && ctest --output-on-failure
```

源码树内构建会被拒绝：若构建目录位于源码树内，CMake 将报 `FATAL_ERROR`。

在 Windows 上，若需要守护进程集群与 CLI，请在配置阶段显式加上
`-DBUILD_DAEMON=ON -DBUILD_CLI=ON`，原因见下一节。

### 关键 CMake 选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `AIRY_BUILD_ALL` | `ON` | 构建全部 AgentRT 组件 |
| `BUILD_TESTS` | `ON` | 构建单元测试并启用 CTest |
| `BUILD_SHARED_LIBS` | `OFF` | 构建动态库而非静态库 |
| `BUILD_ATOMS` / `BUILD_COMMONS` / `BUILD_CUPOLAS` / `BUILD_GATEWAY` / `BUILD_HEAPSTORE` | `ON` | 各组件独立构建开关 |
| `BUILD_DAEMON` | POSIX `ON`，Windows `OFF` | 构建守护进程集群 |
| `BUILD_CLI` | POSIX `ON`，Windows `OFF` | 构建交互式 CLI |
| `BUILD_TOOLKIT` | `OFF` | 构建各语言 SDK 的 toolkit 模块 |
| `AIRY_MEMORY_BACKEND` | `builtin` | 记忆后端：`builtin` 或 `memoryrovol` |
| `AIRY_WITH_MEMORYROVOL` | `ON` | 链接商业记忆提供方；当外部源缺失时自动降级为内置后端，不破坏构建 |
| `AIRY_COMPLIANCE_STRICT` | `ON` | 严格合规模式，投毒 `strcpy` 等不安全函数 |
| `ENABLE_SANITIZERS` | `OFF` | ASan + LSan + UBSan；排查内存问题时显式开启 |
| `ENABLE_TSAN` | `OFF` | ThreadSanitizer；与上一项互斥 |
| `ENABLE_COVERAGE` | `OFF` | 代码覆盖率插桩 |
| `WARNINGS_AS_ERRORS` | `OFF` | 将编译器警告视为错误 |

发布包一律在关闭消毒器的状态下构建。`ENABLE_SANITIZERS` 是排障手段，不是
部署配置。

## 版本与发布通道

- **stable** 为默认通道，也是安装器使用的通道。
- **rc** 提供候选版本用于预览，接口仍可能调整。
- **beta** 偶尔为定向验证开启。

```bash
airymaxrt update              # 更新到当前通道的最新构建
airymaxrt update --check      # 只检查，不做任何变更
airymaxrt update --channel rc # 切换通道
airymaxrt update --rollback   # 回滚到上一版本
```

每个版本都在 [`latest/`](latest) 中提供 GPG 签名的发布清单，更新器读取的正是
它。版本说明位于 [`RELEASE_NOTES.d/`](RELEASE_NOTES.d)，每个版本一个文件；
更完整的历史记录见 [CHANGELOG.md](CHANGELOG.md)。

当前版本为 **v0.1.15**，要点如下：

- `corekern` 经 `airy_init()` 正式接入 CLI 启动链路，并输出可追溯的启动证据；
  修复了 IPC 通道、IPC 回复路由、binder 关闭顺序与零长度分配等一批缺陷。
- 终端界面在收到致命信号时恢复终端状态，不再残留乱码或隐藏光标；并能正确
  消费终端 OSC 回复。
- 网关入口鉴权与监听绑定得到加固；`commons`、`gateway`、`corekern` 引入
  ASan / UBSan 质量门禁，内存安全缺陷在合并前即被拦截。
- 新增 `AIRY_KEEP_SYMBOLS` 开关，便于构建保留符号用于现场诊断。
- Windows x86-64 与 x86-32 包与 Linux、macOS 由同一流水线一同发布。

## 文档

设计文档、接口参考与应用开发指南位于
[Airymax 文档仓库](https://atomgit.com/openairymax/docs)的 `AirymaxRT/` 目录下。
完整导航见 `AirymaxRT/README.md`；五分钟上手见
`AirymaxRT/140-application-development/01-getting-started.md`；API 参考见
`AirymaxRT/30-interfaces/`。

## 参与贡献

开发流程与编码约定见 [CONTRIBUTING.md](CONTRIBUTING.md)。提交 Pull Request
前请在本地跑通测试套件，CI 会执行同样的质量门禁。

- 缺陷反馈与功能建议：<https://github.com/openairymax/agentrt/issues>
- 安全漏洞：见 [SECURITY.md](SECURITY.md)。请勿直接开公开 issue 讨论可被利用的问题。
- 获取支持：[SUPPORT.md](SUPPORT.md)

## 许可证

采用 **AGPL-3.0-or-later OR Apache-2.0** 双许可证。你可以任选其一——既不必
同时遵守两者，也不能两者都不遵守。全文见 [LICENSE](LICENSE)。

| 你的场景 | 建议选择 | 原因 |
|---------|---------|------|
| 基于 AgentRT 修改并对外提供网络服务 | AGPL v3 | 网络服务条款要求公开修改后的源码 |
| 开发开源衍生作品 | AGPL v3 | copyleft 使衍生作品保持开源 |
| 集成进闭源商业产品 | Apache 2.0 | 宽松许可，允许专有衍生 |
| 企业内部工具 | Apache 2.0 | 无公开源码义务 |
| 需要明确的专利授权 | Apache 2.0 | 贡献者显式授予专利权 |
| 学习与研究 | 任一 | 两者均允许 |

伞仓中 `products/memoryrovol` 的商业记忆提供方以独立的 SPHARX Ltd. 最终用户
许可协议单独分发，不受上述双许可证约束。

Copyright (c) 2025-2026 **SPHARX Ltd.** All Rights Reserved.
