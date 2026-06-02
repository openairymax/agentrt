# Toolkit Rust — AgentOS Rust SDK

**模块路径**: `agentos/toolkit/rust/`
**版本**: v0.1.0 (SDK v0.1.0)

## 概述

AgentOS Rust SDK 提供基于 Rust 语言的 AgentOS 系统编程接口，利用 Rust 的内存安全特性和零成本抽象，适用于对性能和安全要求较高的场景。SDK 包含客户端层、业务模块层（Task/Memory/Session/Skill）、系统调用绑定、遥测、插件系统和宏定义，与 Python/Go/TypeScript SDK 保持 API 一致性。

## 目录结构

```
rust/
├── src/
│   ├── lib.rs                  # 模块入口，导出所有公共 API
│   ├── agent.rs                # Agent 模块
│   ├── error.rs                # AgentOSError/ErrorCode 与错误码常量
│   ├── protocol.rs             # 协议处理
│   ├── syscall.rs              # 系统调用绑定
│   ├── telemetry.rs            # OpenTelemetry 遥测
│   ├── plugin.rs               # Plugin 系统
│   ├── macros.rs               # 宏定义
│   ├── task.rs                 # Task 领域模型（deprecated，使用 modules::task）
│   ├── memory.rs               # Memory 领域模型（deprecated，使用 modules::memory）
│   ├── session.rs              # Session 领域模型（deprecated，使用 modules::session）
│   ├── skill.rs                # Skill 领域模型（deprecated，使用 modules::skill）
│   ├── client/
│   │   ├── mod.rs              # Client 模块入口
│   │   └── client.rs           # Client/APIClient 实现
│   ├── modules/
│   │   ├── mod.rs              # 模块导出
│   │   ├── task/
│   │   │   ├── mod.rs
│   │   │   └── manager.rs      # TaskManager
│   │   ├── memory/
│   │   │   ├── mod.rs
│   │   │   └── manager.rs      # MemoryManager
│   │   ├── session/
│   │   │   ├── mod.rs
│   │   │   └── manager.rs      # SessionManager
│   │   └── skill/
│   │       ├── mod.rs
│   │       └── manager.rs      # SkillManager
│   ├── types/
│   │   ├── mod.rs              # 类型模块入口
│   │   └── types.rs            # 类型定义
│   └── utils/
│       ├── mod.rs
│       └── helpers.rs          # 工具函数
├── tests/                      # 集成测试
│   ├── integration_test.rs     # 集成测试
│   ├── managers_test.rs        # Manager 测试
│   ├── module_structure_test.rs # 模块结构测试
│   ├── error_test.rs           # 错误测试
│   ├── client_async_test.rs    # 异步客户端测试
│   ├── plugin_test.rs          # 插件测试
│   └── benchmark_test.rs       # 性能基准测试
├── Cargo.toml                  # Rust 项目配置
└── README.md                   # 本文件
```

## 核心组件

### 客户端层

| 类型 | 说明 |
|------|------|
| `Client` | HTTP 客户端，支持 API Key 认证和超时配置 |
| `APIClient` | 高级 API 客户端，封装所有业务模块 |

```rust
use agentos_rs::{new_client, new_client_with_api_key};

let client = new_client("http://localhost:18789")?;
let client = new_client_with_api_key("http://localhost:18789", "api-key")?;
```

### 业务模块层

| 管理器 | 说明 | 核心方法 |
|--------|------|----------|
| `TaskManager` | 任务管理 | submit/get/cancel/list/wait |
| `MemoryManager` | 记忆管理 | write/read/search/delete/list |
| `SessionManager` | 会话管理 | create/get/close/list |
| `SkillManager` | 技能管理 | load/execute/unload/list |

### 类型系统

| 类型 | 说明 |
|------|------|
| `Task` / `TaskResult` | 任务与结果 |
| `Memory` / `MemorySearchResult` | 记忆与搜索结果 |
| `Session` | 会话 |
| `Skill` / `SkillResult` / `SkillInfo` | 技能与执行结果 |
| `TaskStatus` | 任务状态枚举（is_terminal()） |
| `MemoryLayer` | 记忆层级（L1-L4，is_valid()） |
| `SessionStatus` | 会话状态枚举 |
| `SkillStatus` | 技能状态枚举 |
| `SpanStatus` | 遥测 Span 状态 |

### 插件系统

| 类型 | 说明 |
|------|------|
| `BasePlugin` | 插件基类 |
| `PluginFactory` | 插件工厂 |
| `PluginRegistry` | 插件注册表 |
| `PluginManager` | 插件管理器 |
| `PluginManifest` | 插件清单 |
| `PluginState` | 插件状态 |

### 系统调用绑定

| 类型 | 说明 |
|------|------|
| `SyscallBinding` | 系统调用绑定 trait |
| `HttpSyscallBinding` | HTTP 系统调用实现 |
| `SyscallNamespace` | 命名空间 |
| `TaskSyscall` / `MemorySyscall` / `SessionSyscall` / `SkillSyscall` / `AgentSyscall` | 各模块系统调用 |

### 错误码体系

| 常量 | 值 | 说明 |
|------|-----|------|
| `CODE_SUCCESS` | `0x0000` | 成功 |
| `CODE_INVALID_PARAMETER` | `0x0002` | 参数无效 |
| `CODE_TIMEOUT` | `0x0004` | 超时 |
| `CODE_NOT_FOUND` | `0x0005` | 未找到 |
| `CODE_NETWORK_ERROR` | `0x000A` | 网络错误 |
| `CODE_TASK_FAILED` | `0x3001` | 任务失败 |
| `CODE_MEMORY_NOT_FOUND` | `0x4001` | 记忆未找到 |
| `CODE_SESSION_EXPIRED` | `0x4005` | 会话过期 |
| `CODE_SKILL_NOT_FOUND` | `0x4006` | 技能未找到 |

## 接口说明

### 便捷函数

```rust
use agentos_rs::{new_client, new_client_with_api_key, VERSION, AUTHOR, LICENSE};

let client = new_client("http://localhost:18789")?;
let client = new_client_with_api_key("http://localhost:18789", "key")?;
```

### TaskManager

```rust
let task_mgr = client.task_manager();

let task = task_mgr.submit("analyze data").await?;
let result = task_mgr.wait(&task.id, Duration::from_secs(30)).await?;
let tasks = task_mgr.list(&ListOptions::default()).await?;
task_mgr.cancel(&task_id).await?;
```

### MemoryManager

```rust
let memory_mgr = client.memory_manager();

let memory_id = memory_mgr.write("content", None).await?;
let memories = memory_mgr.search("query", 5).await?;
let memory = memory_mgr.read(&memory_id).await?;
memory_mgr.delete(&memory_id).await?;
```

### 工具函数

```rust
use agentos_rs::{
    extract_data_map, get_string, get_i64, get_f64, get_bool,
    build_url, generate_id, validate_endpoint,
    merge_maps, parse_time_from_map,
};
```

## 依赖关系

- **Rust Edition**: 2021
- **核心依赖**: tokio (async runtime), serde + serde_json (序列化), reqwest (HTTP), tracing (日志)
- **错误处理**: anyhow, thiserror
- **测试**: 内置 #[cfg(test)]

## 构建说明

```bash
# 构建
cargo build --release

# 运行测试
cargo test

# 运行基准测试
cargo test --bench

# 运行文档生成
cargo doc --open

# 检查代码
cargo clippy
```

## 使用示例

```rust
use agentos_rs::{new_client, TaskManager, MemoryManager};

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let client = new_client("http://localhost:18789")?;

    let task_mgr = client.task_manager();
    let task = task_mgr.submit("Analyze sales data").await?;
    let result = task_mgr.wait(&task.id, std::time::Duration::from_secs(60)).await?;
    println!("Task result: {:?}", result.output);

    let memory_mgr = client.memory_manager();
    let memory_id = memory_mgr.write("Analysis result stored", None).await?;
    println!("Memory saved: {}", memory_id);

    Ok(())
}
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
