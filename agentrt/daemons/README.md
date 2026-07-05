# Daemon — AgentRT 用户态服务层

> **模块路径**: `agentrt/daemons/` | **版本**: v0.1.0

## 概述

`agentrt/daemons/` 是 AgentRT 的用户态服务层，由 10+ 个独立进程的守护进程（Daemon）组成，为智能体系统提供完整的后端服务支持。每个守护进程遵循**职责单一原则**，独立运行并通过统一的 IPC 服务总线协作通信，构成一个高可用、可扩展、可插拔的微服务架构。

### 设计目标

- **服务化架构**：每个守护进程独立运行，通过 IPC 通信协作，支持独立部署与扩缩容
- **职责单一**：每个守护进程只负责一个核心领域，降低耦合度
- **可插拔**：守护进程可独立部署、升级和替换，不影响其他服务
- **高可用**：支持主备切换、熔断器保护、故障转移和自动恢复
- **安全内生**：集成 cupolas 安全框架，所有请求必须验证，零信任架构
- **协议统一**：所有守护进程通过 JSON-RPC 2.0 协议通信，支持 MCP/A2A/OpenAI API 等协议转换

## 目录结构

```
daemons/
├── CMakeLists.txt          # 顶层构建文件，管理所有子模块
├── Dockerfile.ci           # CI 环境 Docker 镜像定义
├── README.md               # 本文件
├── common/                 # 公共服务库（18+ 组件）
├── gateway_d/              # API 网关守护进程
├── llm_d/                  # LLM 服务守护进程
├── tool_d/                 # 工具执行守护进程
├── sched_d/                # 任务调度守护进程
├── market_d/               # 应用市场守护进程
├── monit_d/                # 监控告警守护进程
├── channel_d/              # 通信通道守护进程
├── info_d/                 # 信息服务守护进程
├── notify_d/               # 通知推送守护进程
├── observe_d/              # 观测服务守护进程
├── examples/               # 使用示例
│   └── example_svc_usage.c
└── scripts/                # 构建/CI/分析脚本
    ├── ci.sh
    ├── local-ci.sh
    ├── static-analysis.sh
    └── verify-coverage.sh
```

## 核心守护进程

| 守护进程 | 目录 | 职责 | CMake Target |
|----------|------|------|-------------|
| **API 网关** | `gateway_d/` | 外部请求接入，协议转换（HTTP/WS/MCP/A2A/OpenAI API）与路由 | `gateway_d` |
| **LLM 服务** | `llm_d/` | 大语言模型调用、Token 计数、成本追踪、响应缓存 | `llm_d` |
| **工具执行** | `tool_d/` | 工具注册/发现、沙箱执行、参数校验、结果缓存 | `tool_d` |
| **任务调度** | `sched_d/` | 任务分发、4 种调度策略（轮询/加权/优先级/ML） | `sched_d` |
| **应用市场** | `market_d/` | Agent/Skill/Tool/Template 资源管理、安装、版本控制 | `market_d` |
| **监控告警** | `monit_d/` | 指标采集、健康检查、告警管理、Agent 死循环检测 | `monit_d` |
| **通道服务** | `channel_d/` | 通信通道管理与消息路由 | `channel_d` |
| **信息服务** | `info_d/` | 系统信息查询与状态报告 | `info_d` |
| **通知服务** | `notify_d/` | 多渠道通知推送（邮件/Slack/Discord） | `notify_d` |
| **观测服务** | `observe_d/` | OpenTelemetry 可观测性数据采集 | `observe_d` |
| **公共服务** | `common/` | 共享工具库与兼容层（18+ 组件） | `svc_common` |

## 架构总览

```
+-------------------------------------------------------------------+
|                        外部客户端/Agent                              |
+-------------------------------------------------------------------+
|   gateway_d (API 网关)                                              |
|   HTTP/WS/Stdio/MCP/A2A/OpenAI API → JSON-RPC 2.0 → 服务路由       |
+---+---------------+---------------+---------------+---------------+
    |               |               |               |
|  +-----------+  +-----------+  +-----------+  +-----------+      |
|  |  llm_d    |  |  tool_d   |  |  sched_d  |  | market_d  |      |
|  | LLM 服务   |  | 工具执行   |  | 任务调度   |  | 应用市场   |      |
|  +-----------+  +-----------+  +-----------+  +-----------+      |
|               |               |               |               |
|  +-----------+  +-----------+  +-----------+  +-----------+      |
|  |  monit_d  |  | channel_d |  |  info_d   |  | notify_d  |      |
|  | 监控告警   |  | 通道服务   |  | 信息服务   |  | 通知服务   |      |
|  +-----------+  +-----------+  +-----------+  +-----------+      |
|               |                                                   |
|  +-----------+  +--------------------------------------+         |
|  | observe_d |  |  common (公共服务库 — 18+ 组件)       |         |
|  | 观测服务   |  |  兼容层/工具库/日志/配置/安全/IPC/指标 |         |
|  +-----------+  +--------------------------------------+         |
+-------------------------------------------------------------------+
|                         Atom 内核层                                |
+-------------------------------------------------------------------+
```

## 通信方式

所有守护进程通过统一的 IPC 服务总线（`ipc_service_bus`）通信，支持多协议消息传递：

| 通信方式 | 适用场景 | 延迟 | 协议 |
|----------|----------|------|------|
| Unix Socket | 同机守护进程 | < 100μs | JSON-RPC 2.0 |
| TCP | 跨机守护进程 | < 1ms | JSON-RPC 2.0 |
| 共享内存 | 高性能数据交换 | < 10μs | 自定义 |

### IPC 服务总线协议类型

| 协议 | 说明 |
|------|------|
| `IPC_BUS_PROTO_JSON_RPC` | 标准 JSON-RPC 2.0 |
| `IPC_BUS_PROTO_MCP` | Model Context Protocol |
| `IPC_BUS_PROTO_A2A` | Agent-to-Agent Protocol |
| `IPC_BUS_PROTO_OPENAI` | OpenAI API 兼容协议 |
| `IPC_BUS_PROTO_AUTO` | 自动协议检测 |

### IPC 消息类型

| 类型 | 说明 |
|------|------|
| `IPC_BUS_MSG_REQUEST` | 请求消息 |
| `IPC_BUS_MSG_RESPONSE` | 响应消息 |
| `IPC_BUS_MSG_NOTIFICATION` | 通知消息 |
| `IPC_BUS_MSG_BROADCAST` | 广播消息 |
| `IPC_BUS_MSG_HEARTBEAT` | 心跳消息 |
| `IPC_BUS_MSG_DISCOVERY` | 服务发现消息 |
| `IPC_BUS_MSG_CONTROL` | 控制消息 |

## 守护进程生命周期

```
INIT → CONFIG_LOAD → SERVICE_REGISTER → IDLE → BUSY → SHUTDOWN
  ↓        ↓             ↓               ↓      ↓        ↓
初始化   加载配置    注册到服务发现     等待    处理    优雅关闭
```

服务状态枚举（`agentrt_svc_state_t`）：

| 状态 | 说明 |
|------|------|
| `AGENTRT_SVC_STATE_NONE` | 未初始化 |
| `AGENTRT_SVC_STATE_CREATED` | 已创建 |
| `AGENTRT_SVC_STATE_INITIALIZING` | 初始化中 |
| `AGENTRT_SVC_STATE_READY` | 就绪 |
| `AGENTRT_SVC_STATE_RUNNING` | 运行中 |
| `AGENTRT_SVC_STATE_PAUSED` | 已暂停 |
| `AGENTRT_SVC_STATE_STOPPING` | 停止中 |
| `AGENTRT_SVC_STATE_STOPPED` | 已停止 |
| `AGENTRT_SVC_STATE_ZOMBIE` | 僵尸状态 |
| `AGENTRT_SVC_STATE_ERROR` | 错误状态 |

## 依赖关系

```
common ← gateway_d ← 外部客户端
common ← llm_d     ← gateway_d
common ← tool_d    ← gateway_d, llm_d
common ← sched_d   ← gateway_d
common ← market_d  ← gateway_d
common ← monit_d   ← 所有守护进程（指标上报）
common ← channel_d ← gateway_d
common ← info_d    ← gateway_d
common ← notify_d  ← monit_d（告警通知）
common ← observe_d ← monit_d（可观测性）
```

## 构建说明

### 前置依赖

- CMake 3.16+
- C11 编译器（GCC/Clang/MSVC）
- cJSON 库
- GTest（可选，用于单元测试）
- lcov/genhtml（可选，用于覆盖率报告）

### 构建命令

```bash
# 标准构建
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 启用测试
cmake -B build -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build

# 启用覆盖率
cmake -B build -DBUILD_COVERAGE=ON
cmake --build build
cmake --build build --target coverage

# 跨平台构建
cmake -B build -DBUILD_ALL_PLATFORMS=ON
```

### 构建选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `BUILD_TESTS` | ON | 构建单元测试 |
| `BUILD_COVERAGE` | OFF | 启用代码覆盖率 |
| `BUILD_ALL_PLATFORMS` | OFF | 跨平台编译 |

## 启动方式

```bash
# 启动单个守护进程
./gateway_d --config gateway_config.json

# 使用管理器启动所有守护进程
./daemon_manager --start-all

# 查看守护进程状态
./daemon_manager --status
```

## CI/CD 脚本

| 脚本 | 用途 |
|------|------|
| `scripts/ci.sh` | CI 流水线构建脚本 |
| `scripts/local-ci.sh` | 本地 CI 模拟脚本 |
| `scripts/static-analysis.sh` | 静态代码分析 |
| `scripts/verify-coverage.sh` | 覆盖率验证 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
