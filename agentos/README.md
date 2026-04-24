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
│  CoreKern / CoreLoopThree / MemoryRovol / Syscall / ...     │
└───────────────────────────────────────────────────────────┘
```

## 目录结构

```
agentos/
├── atoms/              # 微内核原子组件层 — 系统的最底层基础
│   ├── corekern/       #     微内核核心 — IPC/Binder、内存管理、任务调度、定时器
│   ├── coreloopthree/  #     三环核心运行时 — 认知环/执行环/学习环
│   ├── memoryrovol/    #     记忆系统 — L1-L4 四级记忆层次
│   ├── syscall/        #     系统调用接口 — 任务/内存/会话/遥测/Agent
│   ├── taskflow/       #     任务流引擎 — DAG 编排、优先级队列
│   └── frameworks/     #     框架适配器 — LangChain/MCP/A2A/OpenAI
├── commons/            # 统一基础库 — 平台抽象、日志、配置、内存、同步等
│   ├── platform/       #     跨平台兼容层
│   ├── utils/          #     工具集
│   │   ├── logging/    #       日志系统（三层架构）
│   │   ├── config_unified/ #   统一配置管理
│   │   ├── strategy/   #       加权评分引擎
│   │   ├── cognition/  #       认知管理
│   │   └── execution/  #       安全命令执行
│   ├── include/        #     公共头文件
│   └── tests/          #     Commons 单元测试
├── cupolas/            # 安全穹顶层 — 全方位安全防护体系
│   ├── src/
│   │   ├── security/   #     安全防护引擎
│   │   ├── sanitizer/  #     输入清洗器（XSS/SQL 注入/路径遍历）
│   │   ├── permission/ #     权限管理（RBAC+ABAC）
│   │   ├── audit/      #     审计系统（HMAC 签名链）
│   │   ├── workbench/  #     安全工作台
│   │   └── utils/      #     安全工具库
│   └── include/        #     公共头文件
├── daemon/             # 用户态守护进程层 — 8 个核心服务
│   ├── gateway_d/      #     API 网关守护进程
│   ├── llm_d/          #     LLM 服务守护进程（多 Provider 支持）
│   ├── tool_d/         #     工具执行守护进程
│   ├── sched_d/        #     任务调度守护进程
│   ├── market_d/       #     应用市场守护进程
│   ├── monit_d/        #     监控告警守护进程
│   └── common/         #     公共服务库（18 个组件）
├── gateway/            # 协议网关层 — HTTP/WS/Stdio → JSON-RPC 2.0
│   ├── src/            #     网关实现
│   ├── include/        #     公共头文件
│   ├── deploy/         #     部署配置（K8s）
│   ├── docker/         #     Docker 容器化配置
│   └── tests/          #     网关测试
├── heapstore/          # 运行时数据存储 — LMDB+SQLite+Redis 混合存储
│   ├── src/            #     存储引擎实现
│   ├── include/        #     公共头文件
│   ├── tests/          #     存储引擎测试
│   └── examples/       #     使用示例
├── manager/            # 统一配置管理中心 — JSON Schema + 热重载
│   ├── schemas/        #     9 个 JSON Schema 文件
│   ├── scripts/        #     配置管理脚本
│   ├── tests/          #     配置测试
│   ├── tools/          #     配置工具
│   └── benchmark/      #     性能基准测试
├── toolkit/            # 多语言 SDK — Python/Go/Rust/TypeScript
│   ├── python/         #     Python SDK
│   ├── go/             #     Go SDK
│   ├── rust/           #     Rust SDK
│   └── typescript/     #     TypeScript SDK
├── openlab/            # 开放生态系统 — Apps/Contrib/Markets
│   ├── openlab/        #     核心管理模块
│   ├── app/            #     应用（DocGen/E-Commerce/Research/VideoEdit）
│   ├── contrib/        #     社区贡献（Skills/Strategies/Agents）
│   └── markets/        #     应用市场模板
├── protocols/          # 通信协议定义 — Binder/JSON-RPC/HTTP/WS/MQTT/gRPC
└── CMakeLists.txt      # 顶层构建配置
```

## 核心模块说明

### Atoms 微内核层

Atoms 层是 AgentOS 的最底层基础，包含 6 个核心微内核组件：

| 模块 | 说明 | 关键特性 |
|------|------|----------|
| **CoreKern** | 微内核核心 | IPC/Binder、内存管理、任务调度、定时器、插件管理 |
| **CoreLoopThree** | 三环运行时 | 认知环/执行环/学习环、System 1/System 2 双处理理论 |
| **MemoryRovol** | 记忆系统 | L1-L4 四级记忆层次、吸引子网络检索、混合搜索策略 |
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

Daemon 层包含 8 个用户态守护进程，通过 IPC 进行通信：

| 模块 | 说明 | 通信方式 |
|------|------|----------|
| **gateway_d** | API 网关 — 协议转换（HTTP/WS→JSON-RPC） | IPC → Service Bus |
| **llm_d** | LLM 服务 — 多 Provider（OpenAI/Anthropic/DeepSeek） | IPC → Provider Adapter |
| **tool_d** | 工具执行 — 沙箱执行、参数验证、结果缓存 | IPC → Executor |
| **sched_d** | 任务调度 — 5 种策略（轮询/加权/优先级/亲和/自定义） | IPC → Dispatcher |
| **market_d** | 应用市场 — Agent/Skill/Tool/Template 资源管理 | IPC → Registry |
| **monit_d** | 监控告警 — 指标采集、健康检查、告警管理 | IPC → Metrics Collector |
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

Heapstore 提供运行时数据持久化，采用 LMDB+SQLite+Redis 混合存储方案：

| 组件 | 说明 | 存储引擎 |
|------|------|----------|
| `heapstore_core` | 核心存储引擎 | LMDB/SQLite/Redis |
| `heapstore_log` | 日志存储 | SQLite |
| `heapstore_registry` | 注册表存储 | SQLite |
| `heapstore_trace` | 追踪数据存储 | SQLite |
| `heapstore_memory` | 内存数据存储 | LMDB |
| `heapstore_token` | Token 记录存储 | SQLite |
| `heapstore_batch` | 批量写操作 | LMDB |

### Manager 配置中心

Manager 是统一配置管理中心：

| 组件 | 说明 |
|------|------|
| `schemas/` | 9 个 JSON Schema 文件，272 个验证项 |
| `scripts/` | 配置生成、验证、应用脚本 |
| `tests/` | 配置语法/Schema/集成测试 |
| `tools/` | 配置差异比较、版本清理工具 |
| `benchmark/` | TaskManager/SessionManager/MemoryManager/SkillManager 性能测试 |

### Toolkit SDK

多语言 SDK 工具包：

| 语言 | 版本 | 目录 |
|------|------|------|
| Python | v1.0.0.9 | `toolkit/python/` |
| Go | v1.0.0.9 | `toolkit/go/` |
| Rust | v1.0.0.9 | `toolkit/rust/` |
| TypeScript | v1.0.0.9 | `toolkit/typescript/` |

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
# 配置构建
mkdir build && cd build
cmake ..

# 编译
make -j$(nproc)

# 运行测试
ctest --output-on-failure
```

### 依赖要求

| 依赖 | 版本 | 用途 |
|------|------|------|
| CMake | ≥ 3.20 | 构建系统 |
| GCC/G++ | ≥ 11.0 | C/C++ 编译器 |
| OpenSSL | ≥ 1.1.1 | 加密/TLS |
| libmicrohttpd | ≥ 0.9.70 | HTTP 服务 |
| libwebsockets | ≥ 4.3.0 | WebSocket 服务 |
| cJSON | ≥ 1.7.15 | JSON 解析 |

---

*AgentOS 核心引擎*
