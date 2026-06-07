# AgentOS 核心引擎

`agentos/`

## 概述

`agentos/` 是 AgentOS 项目的核心代码目录，包含了从微内核到应用生态层的完整实现。该目录按照"分层架构、模块隔离、协议驱动"的设计理念组织，涵盖 Atoms 微内核层、Commons 基础库、Cupolas 安全层、Daemon 服务层、Gateway 网关层、Heapstore 存储层、Manager 配置中心以及 Toolkit SDK 工具包，构成了完整的智能体操作系统内核。

## 架构总览

AgentOS 采用自底向上的四层分层架构，每层职责明确、接口清晰：

```
┌───────────────────────────────────────────────────────────┐
│                    OpenLab 开放生态层                       │
│  (agentos/openlab/) — Apps / Contrib / Markets / Templates  │
├───────────────────────────────────────────────────────────┤
│                    用户态服务层 (Daemon)                     │
│  gateway_d → llm_d / tool_d / sched_d / market_d / monit_d  │
├───────────────────────────────────────────────────────────┤
│                    基础设施层                                │
│  gateway / heapstore / manager / cupolas / commons           │
├───────────────────────────────────────────────────────────┤
│                    Atoms 微内核层                            │
│  CoreKern / CoreLoopThree / Memory / MemoryRovol / Syscall / ... │
└───────────────────────────────────────────────────────────┘
```

## 目录结构

```
agentos/
├── atoms/              # 微内核原子组件层 — 系统的最底层基础
│   ├── corekern/       #     微内核核心 — IPC/Binder、内存管理、任务调度、定时器
│   ├── coreloopthree/  #     三环核心运行时 — 认知环/执行环/学习环
│   ├── memory/         #     内置记忆子系统（L1+L2）
│   ├── memoryrovol/    #     MemoryRovol PRO 桥接（L1-L4，需 -DAGENTOS_WITH_MEMORYROVOL=ON）
│   ├── syscall/        #     系统调用接口 — 任务/内存/会话/遥测/Agent
│   ├── taskflow/       #     任务流引擎 — DAG 编排、优先级队列
│   └── frameworks/     #     框架适配器 — LangChain/MCP/A2A/OpenAI
├── commons/            # 统一基础库 — 平台抽象、日志、配置、内存、同步等
│   ├── include/        #     公共头文件
│   ├── platform/       #     跨平台兼容层（含 compat）
│   ├── utils/          #     工具集（logging/config_unified/strategy/cognition 等 20+ 子模块）
│   └── tests/          #     Commons 单元与集成测试
├── cupolas/            # 安全穹顶层 — 全方位安全防护体系
│   ├── src/
│   │   ├── security/   #     安全防护引擎
│   │   ├── sanitizer/  #     输入清洗器（XSS/SQL 注入/路径遍历）
│   │   ├── permission/ #     权限管理（RBAC+ABAC）
│   │   ├── audit/      #     审计系统（HMAC 签名链）
│   │   ├── workbench/  #     安全工作台
│   │   ├── platform/   #     安全平台抽象
│   │   ├── guards/     #     安全守卫框架
│   │   └── utils/      #     安全工具库
│   ├── include/        #     公共头文件
│   ├── docs/           #     安全文档
│   └── tests/          #     单元/集成/压力/模糊测试
├── daemon/             # 用户态守护进程层 — 10+ 核心服务
│   ├── gateway_d/      #     API 网关守护进程
│   ├── llm_d/          #     LLM 服务守护进程（多 Provider 支持）
│   ├── tool_d/         #     工具执行守护进程
│   ├── sched_d/        #     任务调度守护进程
│   ├── market_d/       #     应用市场守护进程
│   ├── monit_d/        #     监控告警守护进程
│   ├── channel_d/      #     通道服务守护进程
│   ├── info_d/         #     信息服务守护进程
│   ├── notify_d/       #     通知服务守护进程
│   ├── observe_d/      #     观测服务守护进程
│   ├── common/         #     公共服务库（19 个组件）
│   ├── examples/       #     守护进程使用示例
│   └── scripts/        #     守护进程管理脚本
├── gateway/            # 协议网关层 — HTTP/WS/Stdio → JSON-RPC 2.0
│   ├── src/            #     网关实现（gateway + utils）
│   ├── include/        #     公共头文件
│   ├── config/         #     网关配置
│   ├── deploy/         #     部署配置（K8s）
│   ├── docker/         #     Docker 容器化配置
│   └── tests/          #     网关测试
├── heapstore/          # 运行时数据存储 — SQLite + 内存后端混合存储
│   ├── src/            #     存储引擎实现
│   ├── include/        #     公共头文件
│   ├── kernel/         #     内核集成（IPC/内存管理/服务）
│   ├── registry/       #     服务注册表
│   ├── services/       #     服务存储（llm_d/market_d/tool_d）
│   ├── scripts/        #     管理脚本
│   ├── tests/          #     存储引擎测试
│   └── examples/       #     使用示例
├── manager/            # 统一配置管理中心 — 多模块 Schema + 热重载
│   ├── schema/         #     各模块 Schema 定义(agent/kernel/model/security 等)
│   ├── agent/          #     Agent 配置
│   ├── deployment/     #     部署配置
│   ├── environment/    #     环境配置
│   ├── kernel/         #     内核配置
│   ├── logging/        #     日志配置
│   ├── model/          #     模型配置
│   ├── monitoring/     #     监控配置（告警/仪表盘）
│   ├── sanitizer/      #     清洗器配置
│   ├── security/       #     安全配置
│   ├── service/        #     服务配置（tool_d）
│   ├── skill/          #     技能配置
│   ├── audit/          #     审计配置
│   ├── tests/          #     配置测试
│   ├── tools/          #     配置工具
│   └── benchmark/      #     性能基准测试
├── toolkit/            # 多语言 SDK — Python/Go/Rust/TypeScript
│   ├── python/         #     Python SDK（v0.1.0）
│   ├── go/             #     Go SDK
│   ├── rust/           #     Rust SDK
│   └── typescript/     #     TypeScript SDK
├── openlab/            # 开放生态系统 — Apps/Contrib/Markets
│   ├── openlab/        #     核心管理模块（agents/core/protocols/utils）
│   ├── app/            #     应用（DocGen/E-Commerce/Research/VideoEdit）
│   ├── contrib/        #     社区贡献（Skills/Strategies/Agents）
│   ├── markets/        #     应用市场（agents/skills/templates）
│   └── tests/          #     单元测试
├── protocols/          # 统一协议栈 — 五层架构
│   ├── common/         #     公共层（统一协议接口与实现）
│   ├── core/           #     核心层（路由/扩展框架/转换器/注册中心/适配器）
│   ├── standards/      #     标准协议层（A2A/MCP/AGNTCY ACP）
│   ├── integrations/   #     集成适配层（OpenAI/Claude/OpenClaw/OpenJiuwen/ChinaEco）
│   ├── frameworks/     #     框架适配层（LangChain/AutoGen）
│   ├── include/        #     公共头文件
│   ├── src/            #     协议源文件
│   └── tests/          #     协议测试
└── CMakeLists.txt      # 顶层构建配置
```

## 核心模块说明

### Atoms 微内核层

Atoms 层是 AgentOS 的最底层基础，包含 7 个核心微内核组件：

| 模块 | 说明 | 关键特性 |
|------|------|----------|
| **CoreKern** | 微内核核心 | IPC/Binder、内存管理、任务调度、定时器、插件管理 |
| **CoreLoopThree** | 三环运行时 | 认知环/执行环/学习环、System 1/System 2 双处理理论 |
| **Memory** | 内置记忆子系统 | L1 原始层+L2 关键词索引，可拔插提供商架构 |
| **MemoryRovol** | 商业记忆桥接 | L1-L4 全功能，需外部 MemoryRovol 仓库 |
| **Syscall** | 系统调用接口 | 5 类接口、4 层保护、线程安全（Mutex + RCU） |
| **TaskFlow** | 任务流引擎 | DAG 编排、5 级优先级队列、SQLite 持久化 |
| **Frameworks** | 框架适配器 | LangChain/MCP/A2A/OpenAI 适配器、框架管理器 |

### Commons 基础库

Commons 是 AgentOS 的统一基础库，不依赖任何上层模块，所有组件均可基于它构建：

| 模块 | 说明 |
|------|------|
| `platform/` | 跨平台抽象层（Linux/Windows/macOS） |
| `utils/logging/` | 日志系统 — 三层架构（Core→Atomic→Service） |
| `utils/config_unified/` | 配置管理 — 三层架构（Core→Source→Service） |
| `utils/strategy/` | 加权评分引擎 — 用于候选选择 |
| `utils/cognition/` | 认知管理 — Agent 信息管理、任务分发 |
| `utils/execution/` | 安全命令执行 — 跨平台、超时控制 |
| `utils/sync/` | 同步原语 — Mutex、RWLock、Semaphore、Event |
| `utils/token/` | Token 管理 — 预算控制、计数、标准 |

### Cupolas 安全层

Cupolas 提供全方位的安全防护体系：

| 模块 | 说明 |
|------|------|
| **security** | 安全防护引擎 — 文件扫描、API 保护、行为分析 |
| **sanitizer** | 输入清洗器 — XSS/SQL 注入/命令注入/路径遍历 |
| **permission** | 权限管理 — RBAC+ABAC 双模型 |
| **audit** | 审计系统 — 事件分类、HMAC 签名链防篡改 |
| **workbench** | 安全工作台 — 策略模拟、样本测试 |

### Daemon 服务层

Daemon 层包含 10+ 个用户态守护进程，通过 IPC 进行通信：

| 模块 | 说明 | 通信方式 |
|------|------|----------|
| **gateway_d** | API 网关 — 协议转换（HTTP/WS→JSON-RPC） | IPC → Service Bus |
| **llm_d** | LLM 服务 — 多 Provider（OpenAI/Anthropic/DeepSeek） | IPC → Provider Adapter |
| **tool_d** | 工具执行 — 沙箱执行、参数验证、结果缓存 | IPC → Executor |
| **sched_d** | 任务调度 — 5 种策略（轮询/加权/优先级/亲和/自定义） | IPC → Dispatcher |
| **market_d** | 应用市场 — Agent/Skill/Tool/Template 资源管理 | IPC → Registry |
| **monit_d** | 监控告警 — 指标采集、健康检查、告警管理 | IPC → Metrics Collector |
| **channel_d** | 通道服务 — 多通道消息路由与分发 | IPC → Channel Router |
| **info_d** | 信息服务 — 系统信息查询与聚合 | IPC → Info Aggregator |
| **notify_d** | 通知服务 — 多渠道通知推送与管理 | IPC → Notify Dispatcher |
| **observe_d** | 观测服务 — 链路追踪、性能剖析、可观测性 | IPC → Observe Collector |
| **common** | 公共服务库 — 18 个共享组件 | 静态链接 |

### Gateway 网关层

Gateway 负责外部协议到 JSON-RPC 2.0 的转换：

| 组件 | 说明 |
|------|------|
| `gateway_api.c` | 网关 API 核心 |
| `http_gateway.c` | HTTP 协议网关 |
| `ws_gateway.c` | WebSocket 协议网关 |
| `stdio_gateway.c` | 标准输入输出网关 |
| `jsonrpc.c` | JSON-RPC 2.0 协议实现 |
| `syscall_router.c` | 系统调用路由器 |

### Heapstore 存储层

Heapstore 提供运行时数据持久化，采用 SQLite + 内存后端混合存储方案，支持条件编译自动回退：

| 组件 | 说明 | 存储引擎 |
|------|------|----------|
| `heapstore_core` | 核心存储引擎 | SQLite / 内存后端 |
| `heapstore_log` | 日志存储 | SQLite |
| `heapstore_registry` | 注册表存储 | SQLite |
| `heapstore_trace` | 追踪数据存储 | SQLite |
| `heapstore_memory` | 内存数据存储 | 内存后端 |
| `heapstore_token` | Token 记录存储 | SQLite |
| `heapstore_batch` | 批量写操作 | SQLite |
| `kernel/` | 内核集成层 | IPC、内存管理、服务注册 |
| `services/` | 服务级存储 | llm_d / market_d / tool_d |

### Manager 配置中心

Manager 是统一配置管理中心，按领域模块组织配置文件和 Schema：

| 组件 | 说明 |
|------|------|
| `schema/` | 各模块 JSON Schema 定义（agent/kernel/model/security 等） |
| `agent/` | Agent 配置定义与管理 |
| `kernel/` | 内核级配置（内存/调度/并发限制） |
| `model/` | 模型配置（Provider/参数/上下文窗口） |
| `security/` | 安全配置（认证/授权/加密） |
| `logging/` | 日志配置（级别/格式/输出目标） |
| `deployment/` | 部署配置（环境/集群/端口） |
| `environment/` | 环境变量与运行时配置 |
| `monitoring/` | 监控配置（告警规则/仪表盘） |
| `sanitizer/` | 输入清洗配置（XSS/SQL注入规则） |
| `service/` | 服务配置（tool_d 等守护进程） |
| `skill/` | 技能配置与管理 |
| `audit/` | 审计配置（事件分类/保留策略） |
| `tools/` | 配置差异比较、版本清理工具 |
| `tests/` | 配置语法/Schema/集成测试 |
| `benchmark/` | TaskManager/SessionManager/MemoryManager/SkillManager 性能测试 |

### Toolkit SDK

多语言 SDK 工具包：

| 语言 | 版本 | 目录 |
|------|------|------|
| Python | v0.1.0 | `toolkit/python/` |
| Go | v0.1.0 | `toolkit/go/` |
| Rust | v0.1.0 | `toolkit/rust/` |
| TypeScript | v0.1.0 | `toolkit/typescript/` |

所有 SDK 提供统一的 API 接口，涵盖 Agent/Task/Session/Memory/Skill/Syscall/Telemetry 七大核心功能。

### OpenLab 开放生态

OpenLab 是 AgentOS 的开放生态系统：

| 模块 | 说明 |
|------|------|
| `openlab/` | 核心管理模块 — 组件注册与发现 |
| `app/` | 应用目录 — DocGen/E-Commerce/Research/VideoEdit |
| `contrib/` | 社区贡献 — Skills/Strategies/Agents |
| `markets/` | 应用市场 — Python Agent / Rust Skill 模板 |

## 通信协议

AgentOS 采用三层协议体系：

| 层级 | 协议 | 用途 |
|------|------|------|
| **内核 IPC** | Binder | 微内核与用户态服务间的进程间通信 |
| **服务间通信** | JSON-RPC 2.0 | 各守护进程之间的 RPC 调用 |
| **外部协议** | HTTP/WS/MQTT/gRPC | 外部客户端与 AgentOS 的交互 |

## 构建说明

```bash
# Out-of-source 构建
mkdir /tmp/AgentOS-build && cd /tmp/AgentOS-build
cmake /path/to/agentos -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel $(nproc)

# 运行测试
ctest --output-on-failure
```

### 依赖要求

> **注意**：所有外部依赖由根 CMakeLists.txt 集中检测（BAN-12），子模块不得独立调用 `find_package`。

| 依赖 | 版本 | 用途 |
|------|------|------|
| CMake | ≥ 3.20 | 构建系统 |
| GCC/G++ | ≥ 11.0 | C/C++ 编译器 |
| OpenSSL | ≥ 1.1.1 | 加密/TLS |
| libmicrohttpd | ≥ 0.9.70 | HTTP 服务 |
| libwebsockets | ≥ 4.3.0 | WebSocket 服务 |
| cJSON | ≥ 1.7.15 | JSON 解析 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
