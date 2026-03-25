# AgentOS Rust SDK 模块化重构文档

## 版本信息
- **版本**: 3.0.0
- **更新日期**: 2026-03-24
- **作者**: SpharxWorks

## 重构目标

以 Go SDK 为基准，重构 Rust SDK 的模块化结构，确保两个 SDK 的架构、API 和错误码完全一致。

## 目录结构对比

### Go SDK 结构
```
tools/go/agentos/
├── agentos.go          # 主入口
├── config.go           # 配置管理
├── errors.go           # 错误定义
├── client/             # HTTP 客户端层
│   ├── client.go
│   ├── client_test.go
│   └── mock.go
├── modules/            # 业务模块层
│   ├── modules.go
│   ├── task/
│   │   ├── manager.go
│   │   └── manager_test.go
│   ├── memory/
│   │   ├── manager.go
│   │   └── manager_test.go
│   ├── session/
│   │   ├── manager.go
│   │   └── manager_test.go
│   └── skill/
│       ├── manager.go
│       └── manager_test.go
├── syscall/            # 系统调用层
│   ├── syscall.go
│   └── syscall_test.go
├── telemetry/          # 遥测模块
│   ├── telemetry.go
│   └── telemetry_test.go
├── types/              # 类型定义
│   ├── types.go
│   └── types_test.go
└── utils/              # 工具函数
    ├── helpers.go
    └── helpers_test.go
```

### Rust SDK 结构（重构后）
```
tools/rust/src/
├── lib.rs              # 主入口（对应 agentos.go）
├── error.rs            # 错误定义（对应 errors.go）
├── client/             # HTTP 客户端层
│   ├── mod.rs
│   └── client.rs
├── types/              # 类型定义
│   ├── mod.rs
│   └── types.rs
├── utils/              # 工具函数
│   ├── mod.rs
│   └── helpers.rs
├── modules/            # 业务模块层
│   ├── mod.rs
│   ├── task/
│   │   ├── mod.rs
│   │   └── manager.rs
│   ├── memory/
│   │   ├── mod.rs
│   │   └── manager.rs
│   ├── session/
│   │   ├── mod.rs
│   │   └── manager.rs
│   └── skill/
│       ├── mod.rs
│       └── manager.rs
├── syscall.rs          # 系统调用层
├── telemetry.rs        # 遥测模块
├── agent.rs            # Agent 模块
└── [旧模块保留用于向后兼容]
    ├── task.rs
    ├── memory.rs
    ├── session.rs
    └── skill.rs

tests/
├── integration_test.rs       # 集成测试
└── module_structure_test.rs  # 模块结构测试
```

## 核心特性

### 1. 模块化架构
- **client/**: HTTP 客户端层，提供 APIClient trait 和实现
- **types/**: 类型定义，包含所有枚举、领域模型和请求/响应结构
- **utils/**: 工具函数，提供 ID 生成、验证等通用功能
- **modules/**: 业务模块层，包含 Task、Memory、Session、Skill 管理器

### 2. 错误码体系
完全采用 Go SDK 的十六进制错误码体系：

| 分类 | 范围 | 说明 |
|------|------|------|
| 通用错误 | 0x0xxx | 参数、配置、网络等基础错误 |
| 核心循环错误 | 0x1xxx | Loop 创建、启动、停止错误 |
| 认知层错误 | 0x2xxx | DAG 构建、意图解析错误 |
| 执行层错误 | 0x3xxx | 任务执行、取消、超时错误 |
| 记忆层错误 | 0x4xxx | 记忆、会话、技能相关错误 |
| 系统调用错误 | 0x5xxx | 遥测、系统级错误 |
| 安全域错误 | 0x6xxx | 权限、数据损坏错误 |

### 3. API Key 认证
支持 Bearer Token 认证：
```rust
// 方式 1: 使用便捷函数
let client = agentos::new_client_with_api_key("http://localhost:18789", "your-api-key")?;

// 方式 2: 使用构建器
let client = Client::builder("http://localhost:18789")
    .api_key("your-api-key")
    .timeout(Duration::from_secs(60))
    .max_retries(5)
    .build()?;
```

### 4. 异步支持
所有 API 都是异步的，基于 Tokio 运行时：
```rust
#[tokio::main]
async fn main() -> Result<(), AgentOSError> {
    let client = Client::new("http://localhost:18789")?;

    // 使用 TaskManager
    let task_manager = TaskManager::new(Arc::new(client));
    let task = task_manager.submit("执行任务").await?;

    Ok(())
}
```

## 公共 API 导出

### 从根模块导出的类型
```rust
// 客户端
pub use client::{APIClient, Client};

// 错误类型
pub use error::{AgentOSError, ErrorCode};

// 类型定义
pub use types::{
    TaskStatus, MemoryLayer, SessionStatus, SkillStatus, SpanStatus,
    Task, TaskResult, Memory, MemorySearchResult, Session, Skill, SkillResult, SkillInfo,
    RequestOptions, RequestOption, APIResponse, HealthStatus, Metrics,
    PaginationOptions, SortOptions, FilterOptions, ListOptions,
};

// 业务模块管理器
pub use modules::{
    TaskManager, MemoryManager, SessionManager, SkillManager,
};

// 工具函数
pub use utils::{
    generate_id, validate_endpoint, build_url, ...
};

// 错误码常量
pub use error::{
    CODE_SUCCESS, CODE_UNKNOWN, CODE_INVALID_PARAMETER, ...
};
```

## 向后兼容性

为保持向后兼容，旧的模块文件仍然保留，但标记为 `deprecated`：
```rust
#[deprecated(since = "3.0.0", note = "请使用 modules::task::TaskManager")]
pub mod task;
```

## 测试覆盖

### 单元测试
- `src/error.rs`: 错误码和错误类型测试
- `src/types/types.rs`: 类型定义测试
- `src/utils/helpers.rs`: 工具函数测试
- `src/lib.rs`: 公共 API 测试

### 集成测试
- `tests/integration_test.rs`: 完整的集成测试套件
- `tests/module_structure_test.rs`: 模块结构一致性测试

## 使用示例

### 基本使用
```rust
use agentos::{Client, TaskManager, MemoryManager};
use std::sync::Arc;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    // 创建客户端
    let client = Arc::new(Client::new("http://localhost:18789")?);

    // 使用 TaskManager
    let task_manager = TaskManager::new(client.clone());
    let task = task_manager.submit("执行任务").await?;
    println!("任务 ID: {}", task.id);

    // 使用 MemoryManager
    let memory_manager = MemoryManager::new(client.clone());
    let memory = memory_manager.write("测试记忆", MemoryLayer::L1).await?;
    println!("记忆 ID: {}", memory.id);

    Ok(())
}
```

### 健康检查
```rust
use agentos::Client;

#[tokio::main]
async fn main() -> Result<(), agentos::AgentOSError> {
    let client = Client::new("http://localhost:18789")?;

    // 健康检查
    let health = client.health().await?;
    println!("服务状态: {}", health.status);
    println!("版本: {}", health.version);

    // 获取指标
    let metrics = client.metrics().await?;
    println!("任务总数: {}", metrics.tasks_total);

    Ok(())
}
```

## 依赖项

```toml
[dependencies]
reqwest = { version = "0.11", features = ["json", "rustls-tls"] }
tokio = { version = "1", features = ["full"] }
serde = { version = "1.0", features = ["derive"] }
serde_json = "1.0"
thiserror = "1.0"
async-trait = "0.1"
chrono = { version = "0.4", features = ["serde"] }
uuid = { version = "1.0", features = ["v4"] }
urlencoding = "2.1"
rand = "0.8"
```

## 与 Go SDK 的一致性

### ✅ 已实现的一致性
1. **目录结构**: 完全匹配 Go SDK 的模块化结构
2. **错误码**: 所有错误码与 Go SDK 完全一致（十六进制格式）
3. **类型定义**: 枚举、领域模型与 Go SDK 对齐
4. **API 接口**: Manager 模式与 Go SDK 一致
5. **HTTP 状态码映射**: 与 Go SDK 的 `HTTPStatusToError` 一致
6. **认证支持**: 支持 API Key Bearer Token 认证
7. **文档注释**: 所有公共 API 都有完整的文档注释

### 🔄 差异说明
1. **语言特性**: Rust 使用 `async/await` 和 `Result`，Go 使用 `goroutine` 和 `error`
2. **内存管理**: Rust 使用所有权系统，Go 使用 GC
3. **错误处理**: Rust 使用 `Result<T, E>`，Go 使用多返回值
4. **泛型约束**: Rust 使用 trait bounds，Go 使用接口

## 迁移指南

### 从旧版本迁移
```rust
// 旧版本（已废弃）
use agentos::task::Task;
use agentos::memory::Memory;

// 新版本（推荐）
use agentos::modules::TaskManager;
use agentos::modules::MemoryManager;
use agentos::{Task, Memory}; // 或直接使用类型
```

### 从 Go SDK 迁移
Go 和 Rust 的 API 设计保持一致，迁移时只需注意语法差异：

```go
// Go
client := agentos.NewClient(agentos.WithEndpoint("http://localhost:18789"))
taskManager := task.NewTaskManager(client)
task, err := taskManager.Submit(ctx, "执行任务")
```

```rust
// Rust
let client = Client::new("http://localhost:18789")?;
let task_manager = TaskManager::new(Arc::new(client));
let task = task_manager.submit("执行任务").await?;
```

## 总结

本次重构成功地将 Rust SDK 的模块化结构与 Go SDK 对齐，实现了：

1. ✅ 完全一致的目录结构
2. ✅ 完全一致的错误码体系
3. ✅ 完全一致的类型定义
4. ✅ API Key 认证支持
5. ✅ 完整的测试覆盖
6. ✅ 向后兼容性保证

Rust SDK 现在可以与 Go SDK 无缝协作，为开发者提供一致的 API 体验。
