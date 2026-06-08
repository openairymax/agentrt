# Toolkit — 多语言 SDK 工具包

**版本**: v0.0.5

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
│   │   ├── client/             # 客户端层
│   │   │   ├── client.py       # APIClient/ClientConfig
│   │   │   └── mock.py         # MockClient 测试客户端
│   │   ├── modules/            # 业务模块层
│   │   │   ├── base_manager.py # BaseManager 基类
│   │   │   ├── task/           # TaskManager + Checkpoint
│   │   │   ├── memory/         # MemoryManager
│   │   │   ├── session/        # SessionManager
│   │   │   └── skill/          # SkillManager
│   │   ├── framework/          # 应用框架层
│   │   │   ├── application.py  # Application 基类
│   │   │   ├── plugin.py       # Plugin 系统
│   │   │   ├── lifecycle.py    # 生命周期管理
│   │   │   ├── config.py       # 配置管理
│   │   │   ├── event.py        # 事件系统
│   │   │   ├── state.py        # 状态管理
│   │   │   ├── task.py         # 框架任务
│   │   │   ├── skill.py        # 框架技能
│   │   │   ├── errors.py       # 框架错误
│   │   │   └── plugins/        # 内置插件（metrics/logger）
│   │   ├── types/              # 类型定义（common.py）
│   │   └── utils/              # 工具函数
│   │       ├── helpers.py      # 通用工具
│   │       ├── api_helpers.py  # API 辅助
│   │       ├── event_emitter.py# EventEmitter
│   │       ├── core.py         # 核心工具（向后兼容）
│   │       └── token_optimizer.py # Token 优化（LRU 缓存）
│   ├── tests/                  # 测试套件
│   ├── examples/               # 使用示例
│   ├── setup.py                # 包配置
│   ├── pyproject.toml          # PEP 621 项目配置
│   ├── requirements.txt        # 依赖清单
│   ├── restructure_sdk.py      # SDK 重构脚本
│   └── README.md
├── go/                         # Go SDK
│   ├── agentos/                # 核心包
│   │   ├── agentos.go          # 版本信息
│   │   ├── config.go           # 配置管理
│   │   ├── protocol.go         # 协议处理
│   │   ├── errors.go           # 错误定义
│   │   ├── client/             # 客户端层
│   │   │   ├── client.go       # Client/APIClient/ClientConfig
│   │   │   └── mock.go         # MockClient
│   │   ├── modules/            # 业务模块层
│   │   │   ├── modules.go      # 模块导出
│   │   │   ├── base_manager.go # BaseManager 基类
│   │   │   ├── task/manager.go # TaskManager
│   │   │   ├── memory/manager.go # MemoryManager
│   │   │   ├── session/manager.go # SessionManager
│   │   │   └── skill/manager.go # SkillManager
│   │   ├── plugin/plugin.go    # Plugin 系统
│   │   ├── syscall/syscall.go  # 系统调用绑定
│   │   ├── telemetry/telemetry.go # OpenTelemetry 遥测
│   │   ├── types/types.go      # 类型定义
│   │   └── utils/helpers.go    # 工具函数
│   ├── go.mod                  # Go 模块配置
│   └── README.md
├── rust/                       # Rust SDK
│   ├── src/
│   │   ├── lib.rs              # 模块入口
│   │   ├── agent.rs            # Agent 模块
│   │   ├── error.rs            # AgentOSError/ErrorCode
│   │   ├── protocol.rs         # 协议处理
│   │   ├── syscall.rs          # 系统调用绑定
│   │   ├── telemetry.rs        # 遥测
│   │   ├── plugin.rs           # Plugin 系统
│   │   ├── macros.rs           # 宏定义
│   │   ├── task.rs             # Task（deprecated）
│   │   ├── memory.rs           # Memory（deprecated）
│   │   ├── session.rs          # Session（deprecated）
│   │   ├── skill.rs            # Skill（deprecated）
│   │   ├── client/             # 客户端层
│   │   │   ├── mod.rs
│   │   │   └── client.rs       # Client/APIClient
│   │   ├── modules/            # 业务模块层
│   │   │   ├── task/manager.rs # TaskManager
│   │   │   ├── memory/manager.rs # MemoryManager
│   │   │   ├── session/manager.rs # SessionManager
│   │   │   └── skill/manager.rs # SkillManager
│   │   ├── types/              # 类型定义
│   │   └── utils/              # 工具函数
│   ├── tests/                  # 集成测试
│   ├── Cargo.toml              # Rust 项目配置
│   ├── Cargo.lock              # 依赖锁定
│   └── README.md
├── typescript/                 # TypeScript SDK
│   ├── src/
│   │   ├── index.ts            # 模块入口
│   │   ├── agentos.ts          # AgentOS 主类
│   │   ├── agent.ts            # Agent 模型
│   │   ├── manager.ts          # 配置管理
│   │   ├── config.ts           # 配置类型
│   │   ├── errors.ts           # 错误定义
│   │   ├── protocol.ts         # 协议处理
│   │   ├── syscall.ts          # 系统调用绑定
│   │   ├── telemetry.ts        # 遥测
│   │   ├── plugin.ts           # Plugin 系统
│   │   ├── task.ts             # Task 领域模型
│   │   ├── memory.ts           # Memory 领域模型
│   │   ├── session.ts          # Session 领域模型
│   │   ├── skill.ts            # Skill 领域模型
│   │   ├── client/             # 客户端层
│   │   │   ├── index.ts
│   │   │   ├── client.ts       # Client/APIClient
│   │   │   └── mock.ts         # MockClient
│   │   ├── modules/            # 业务模块层
│   │   │   ├── base_manager.ts # BaseManager
│   │   │   ├── task.ts         # TaskManager
│   │   │   ├── memory.ts       # MemoryManager
│   │   │   ├── session.ts      # SessionManager
│   │   │   └── skill.ts        # SkillManager
│   │   ├── types/              # 类型定义（enums/models/requests）
│   │   └── utils/              # 工具函数（helpers/logger）
│   ├── tests/                  # 测试套件
│   ├── package.json            # NPM 配置
│   ├── tsconfig.json           # TypeScript 配置
│   ├── jest.config.js          # Jest 配置
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
| Python | requests, aiohttp | pip / setup.py |
| Go | 标准库 | go mod |
| Rust | tokio, serde, reqwest | Cargo |
| TypeScript | axios, ws | npm / tsc |

---

© 2026 SPHARX Ltd. All Rights Reserved.
