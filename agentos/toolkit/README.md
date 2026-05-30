# Toolkit — 多语言 SDK 工具包

**模块路径**: `agentos/toolkit/`
**版本**: v0.0.5 (SDK v3.0.0)

## 概述

Toolkit 是 AgentOS 的多语言 SDK 工具包，为开发者提供统一的编程接口来与 AgentOS 系统交互。当前支持 Python、Go、Rust、TypeScript 四种语言，所有 SDK 共享统一的 API 设计、类型系统和错误码规范，确保跨语言开发体验的一致性。遵循 ARCHITECTURAL_PRINCIPLES.md 五维正交设计体系（K-1 内核最小化、K-2 接口契约化、E-3 资源确定性、A-1 简约至上、S-1 安全默认）。

## 目录结构

```
toolkit/
├── python/                     # Python SDK
│   ├── agentos/                # 核心包
│   │   ├── __init__.py         # 模块入口（导出所有公共 API）
│   │   ├── agent.py            # AgentOS/AsyncAgentOS 客户端
│   │   ├── task.py             # Task 领域模型
│   │   ├── memory.py           # Memory 领域模型
│   │   ├── session.py          # Session 领域模型
│   │   ├── skill.py            # Skill 领域模型
│   │   ├── protocol.py         # 协议处理
│   │   ├── syscall.py          # 系统调用绑定
│   │   ├── telemetry.py        # OpenTelemetry 遥测
│   │   ├── exceptions.py       # 异常层级与错误码
│   │   ├── types.py            # 类型定义
│   │   ├── utils.py            # 工具函数
│   │   ├── client/             # 客户端层
│   │   │   ├── client.py       # APIClient/ClientConfig
│   │   │   └── mock.py         # MockClient 测试客户端
│   │   ├── modules/            # 业务模块层
│   │   │   ├── base_manager.py # BaseManager 基类
│   │   │   ├── task/           # TaskManager
│   │   │   ├── memory/         # MemoryManager
│   │   │   ├── session/        # SessionManager
│   │   │   └── skill/          # SkillManager
│   │   ├── framework/          # 应用框架层
│   │   │   ├── application.py  # Application 基类
│   │   │   ├── plugin.py       # Plugin 系统
│   │   │   ├── lifecycle.py    # 生命周期管理
│   │   │   ├── config.py       # 配置管理
│   │   │   ├── event.py        # 事件系统
│   │   │   └── plugins/        # 内置插件
│   │   └── utils/              # 工具函数
│   ├── tests/                  # 测试套件
│   ├── examples/               # 使用示例
│   ├── setup.py                # 包配置
│   └── README.md
├── go/                         # Go SDK
│   ├── agentos/                # 核心包
│   │   ├── agentos.go          # 版本信息
│   │   ├── config.go           # 配置管理
│   │   ├── protocol.go         # 协议处理
│   │   ├── errors.go           # 错误定义
│   │   ├── client/             # 客户端层
│   │   ├── modules/            # 业务模块层
│   │   ├── plugin/             # 插件系统
│   │   ├── syscall/            # 系统调用绑定
│   │   ├── telemetry/          # 遥测
│   │   ├── types/              # 类型定义
│   │   └── utils/              # 工具函数
│   └── README.md
├── rust/                       # Rust SDK
│   ├── src/
│   │   ├── lib.rs              # 模块入口
│   │   ├── agent.rs            # Agent 模块
│   │   ├── error.rs            # 错误定义
│   │   ├── protocol.rs         # 协议处理
│   │   ├── syscall.rs          # 系统调用绑定
│   │   ├── telemetry.rs        # 遥测
│   │   ├── plugin.rs           # 插件系统
│   │   ├── macros.rs           # 宏定义
│   │   ├── client/             # 客户端层
│   │   ├── modules/            # 业务模块层
│   │   ├── types/              # 类型定义
│   │   └── utils/              # 工具函数
│   ├── tests/                  # 集成测试
│   ├── Cargo.toml              # Rust 项目配置
│   └── README.md
├── typescript/                 # TypeScript SDK
│   ├── src/
│   │   ├── index.ts            # 模块入口
│   │   ├── agentos.ts          # AgentOS 主类
│   │   ├── manager.ts          # 配置管理
│   │   ├── errors.ts           # 错误定义
│   │   ├── config.ts           # 配置类型
│   │   ├── protocol.ts         # 协议处理
│   │   ├── syscall.ts          # 系统调用绑定
│   │   ├── telemetry.ts        # 遥测
│   │   ├── plugin.ts           # 插件系统
│   │   ├── client/             # 客户端层
│   │   ├── modules/            # 业务模块层
│   │   ├── types/              # 类型定义
│   │   └── utils/              # 工具函数
│   ├── tests/                  # 测试套件
│   ├── package.json            # NPM 配置
│   ├── tsconfig.json           # TypeScript 配置
│   └── README.md
└── README.md                   # 本文件
```

## 统一 API 设计

所有语言 SDK 共享以下统一 API：

### 核心模块

| 模块 | 说明 | Python | Go | Rust | TypeScript |
|------|------|--------|-----|------|------------|
| **Agent** | Agent 管理 | `AgentOS` / `AsyncAgentOS` | `Client` | `Client` | `AgentOS` |
| **Task** | 任务管理 | `TaskManager` | `TaskManager` | `TaskManager` | `TaskManager` |
| **Memory** | 记忆管理 | `MemoryManager` | `MemoryManager` | `MemoryManager` | `MemoryManager` |
| **Session** | 会话管理 | `SessionManager` | `SessionManager` | `SessionManager` | `SessionManager` |
| **Skill** | 技能管理 | `SkillManager` | `SkillManager` | `SkillManager` | `SkillManager` |
| **Syscall** | 系统调用 | `SyscallBinding` | `SyscallBinding` | `SyscallBinding` | `SyscallBinding` |
| **Telemetry** | 遥测 | `TelemetryManager` | `Telemetry` | `Telemetry` | `Telemetry` |
| **Plugin** | 插件系统 | `Plugin` | `Plugin` | `Plugin` | `Plugin` |

### 统一类型系统

| 类型 | 说明 |
|------|------|
| `Task` / `TaskResult` | 任务与结果 |
| `Memory` / `MemorySearchResult` | 记忆与搜索结果 |
| `Session` | 会话 |
| `Skill` / `SkillResult` / `SkillInfo` | 技能与执行结果 |
| `TaskStatus` | 任务状态枚举 |
| `MemoryLayer` | 记忆层级（L1-L4） |
| `SessionStatus` | 会话状态枚举 |
| `SkillStatus` | 技能状态枚举 |
| `SpanStatus` | 遥测 Span 状态 |

### 统一错误码

| 范围 | 类别 | 示例 |
|------|------|------|
| `0x0xxx` | 通用错误 | SUCCESS/UNKNOWN/INVALID_PARAMETER/TIMEOUT/NOT_FOUND |
| `0x1xxx` | 核心循环错误 | LOOP_CREATE_FAILED/LOOP_START_FAILED/LOOP_STOP_FAILED |
| `0x2xxx` | 认知层错误 | COGNITION_FAILED/DAG_BUILD_FAILED/AGENT_DISPATCH_FAILED |
| `0x3xxx` | 执行层错误 | TASK_FAILED/TASK_CANCELLED/TASK_TIMEOUT |
| `0x4xxx` | 记忆层错误 | MEMORY_NOT_FOUND/SESSION_EXPIRED/SKILL_NOT_FOUND |
| `0x5xxx` | 系统调用错误 | TELEMETRY_ERROR |
| `0x6xxx` | 安全域错误 | PERMISSION_DENIED/CORRUPTED_DATA |

## SDK 架构

```
┌──────────────────────────────────────────────────────────────┐
│                    Application Layer                          │
│  (用户代码 / OpenLab Apps / Contrib Skills)                   │
├──────────────────────────────────────────────────────────────┤
│                    SDK API Layer                              │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐       │
│  │  Agent   │ │   Task   │ │  Memory  │ │ Session  │       │
│  │ Manager  │ │ Manager  │ │ Manager  │ │ Manager  │       │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘       │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐       │
│  │  Skill   │ │  Syscall │ │Telemetry │ │  Plugin  │       │
│  │ Manager  │ │ Binding  │ │          │ │          │       │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘       │
├──────────────────────────────────────────────────────────────┤
│                    Client Layer                               │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │  APIClient   │  │  MockClient  │  │  ClientConfig│      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
├──────────────────────────────────────────────────────────────┤
│                    Transport Layer                            │
│  HTTP/HTTPS (JSON-RPC 2.0)  │  WebSocket  │  gRPC           │
├──────────────────────────────────────────────────────────────┤
│                    AgentOS Core Runtime                       │
└──────────────────────────────────────────────────────────────┘
```

## 语言特性对比

| 特性 | Python | Go | Rust | TypeScript |
|------|--------|-----|------|------------|
| 异步支持 | ✅ asyncio | ✅ goroutine | ✅ tokio | ✅ Promise |
| 类型安全 | ✅ 类型注解 | ✅ 静态类型 | ✅ 所有权系统 | ✅ 静态类型 |
| 插件系统 | ✅ | ✅ | ✅ | ✅ |
| Mock 客户端 | ✅ | ✅ | ✅ | ✅ |
| Checkpoint | ✅ | — | — | — |
| OpenTelemetry | ✅ | ✅ | ✅ | ✅ |
| 事件系统 | ✅ EventEmitter | — | — | — |
| 基准测试 | ✅ | ✅ | ✅ | ✅ |

## 依赖关系

| SDK | 核心依赖 | 构建工具 |
|-----|----------|----------|
| Python | requests, aiohttp, pydantic | pip / setup.py |
| Go | 标准库 + testify | go mod |
| Rust | tokio, serde, reqwest, tracing | Cargo |
| TypeScript | node-fetch, events | npm / tsc |

---

© 2026 SPHARX Ltd. All Rights Reserved.
