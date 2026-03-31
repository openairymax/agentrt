# AgentOS Rust SDK

**版本 3.0.0 | MIT License**

AgentOS Rust SDK 提供了类型安全、内存安全的接口，用于与 AgentOS 系统进行交互。该 SDK 支持任务管理、记忆操作、会话管理和技能加载等功能，并利用 Rust 的所有权系统和生命周期机制确保内存安全。

## 概述

AgentOS Rust SDK 是 AgentOS 多语言 SDK 集合的一部分，旨在为 Rust 开发者提供：

- ✅ **内存安全**：利用 Rust 所有权系统和生命周期，无需垃圾回收
- ✅ **类型安全**：完整的类型定义，编译时检查
- ✅ **零成本抽象**：高性能，没有运行时开销
- ✅ **异步支持**：基于 Tokio 异步运行时
- ✅ **并发安全**：无数据竞争，线程安全

## 安装

### 使用 Cargo

在您的 `Cargo.toml` 中添加依赖：

```toml
[dependencies]
agentos-rs = "3.0.0"
tokio = { version = "1", features = ["full"] }
serde = { version = "1", features = ["derive"] }
serde_json = "1.0"
```

### 异步运行时

推荐使用 Tokio 作为异步运行时：

```toml
[dependencies]
tokio = { version = "1", features = ["full"] }
```

## 快速开始

### 基本用法

```rust
use agentos_rs::{Config, Client};

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    // 初始化配置
    let config = Config::builder()
        .base_url("http://localhost:8080")
        .api_key("your-api-key")
        .timeout(30)
        .build()?;

    // 创建客户端
    let client = Client::new(config)?;

    // 提交任务
    let task = client.submit_task(
        "分析销售数据",
        Some("high"),
        Some(vec!["数据分析".to_string()]),
    ).await?;

    println!("Task ID: {}", task.id);

    // 等待完成
    let result = task.wait(Some(30000)).await?;
    println!("Result: {:?}", result);

    // 写入记忆
    let memory_id = client.write_memory(
        "用户偏好使用 Rust 编程",
        "L2",
        Some(vec!["Rust".to_string(), "编程语言".to_string()]),
        None,
    ).await?;

    println!("Memory ID: {}", memory_id);

    // 搜索记忆
    let memories = client.search_memory(
        "Rust 编程",
        Some("L2"),
        Some(5),
    ).await?;

    for memory in &memories {
        println!("- {}", memory.content);
    }

    Ok(())
}
```

### 高级用法

```rust
use agentos_rs::{Config, Client};
use agentos_rs::modules::{SessionManager, SkillManager};

#[tokio::main]
async fn advanced_example() -> Result<(), Box<dyn std::error::Error>> {
    let config = Config::builder()
        .base_url("http://localhost:8080")
        .api_key("your-api-key")
        .timeout(60)
        .build()?;

    let client = Client::new(config)?;

    // 创建会话
    let session_mgr = SessionManager::new(client.config().clone());
    let session = session_mgr.create(
        "user_12345",
        Some(serde_json::json!({
            "preferences": {
                "language": "Rust",
                "theme": "dark"
            }
        })),
    ).await?;

    println!("Session ID: {}", session.id);

    // 设置会话上下文
    session_mgr.set_context(&session.id, "user_id", "12345").await?;
    session_mgr.set_context(&session.id, "preferences", serde_json::json!({
        "language": "Rust"
    })).await?;

    // 获取会话上下文
    let user_id = session_mgr.get_context(&session.id, "user_id").await?;
    println!("User ID: {:?}", user_id);

    // 加载技能
    let skill_mgr = SkillManager::new(client.config().clone());
    let skill = skill_mgr.load("browser_skill", None).await?;

    println!("Skill: {}", skill.name);

    // 获取技能信息
    let skill_info = skill.get_info().await?;
    println!("Skill version: {}", skill_info.version);
    println!("Skill description: {}", skill_info.description);

    // 执行技能
    let skill_result = skill.execute(Some(serde_json::json!({
        "url": "https://example.com",
        "timeout": 10000
    }))).await?;

    if skill_result.success {
        println!("Skill executed successfully: {:?}", skill_result.output);
    } else {
        println!("Skill execution failed: {:?}", skill_result.error);
    }

    // 关闭会话
    session_mgr.close(&session.id).await?;
    println!("Session closed");

    Ok(())
}
```

## 核心模块

### Config

配置管理模块，支持构建器模式：

```rust
use agentos_rs::Config;

let config = Config::builder()
    .base_url("http://localhost:8080")
    .api_key("your-api-key")
    .timeout(30)
    .max_retries(3)
    .build()?;
```

### Client

HTTP 客户端实现，负责与 AgentOS 服务器通信：

```rust
use agentos_rs::{Config, Client};

let client = Client::new(config)?;
```

### TaskManager

任务管理模块，提供任务提交、查询、等待、取消等功能：

```rust
use agentos_rs::modules::TaskManager;

let task_mgr = TaskManager::new(config.clone());

// 提交任务
let task = task_mgr.submit_task(
    "分析数据",
    Some("high"),
    Some(vec!["数据分析".to_string()]),
).await?;

// 查询任务状态
let status = task_mgr.get_status(&task.id).await?;

// 等待任务完成
let result = task_mgr.wait(&task.id, Some(30000)).await?;

// 取消任务
let cancelled = task_mgr.cancel(&task.id).await?;

// 批量提交
let tasks = task_mgr.batch_submit(
    vec!["任务1", "任务2", "任务3"],
    None,
).await?;
```

### MemoryManager

记忆管理模块，支持四层记忆系统：

```rust
use agentos_rs::modules::MemoryManager;

let memory_mgr = MemoryManager::new(config.clone());

// 写入记忆
let memory_id = memory_mgr.write(
    "用户喜欢 Rust 编程",
    "L2",
    Some(vec!["Rust".to_string()]),
    None,
).await?;

// 搜索记忆
let memories = memory_mgr.search(
    "Rust 编程",
    Some("L2"),
    Some(10),
).await?;

// 按层级搜索
let layer_memories = memory_mgr.list_by_layer("L2", Some(20)).await?;

// 更新记忆
memory_mgr.update(
    &memory_id,
    "更新后的内容",
    Some(vec!["更新".to_string()]),
).await?;

// 删除记忆
memory_mgr.delete(&memory_id).await?;
```

### SessionManager

会话管理模块，处理会话生命周期：

```rust
use agentos_rs::modules::SessionManager;

let session_mgr = SessionManager::new(config.clone());

// 创建会话
let session = session_mgr.create(
    "user_id",
    Some(metadata),
).await?;

// 获取会话
let session = session_mgr.get(&session.id).await?;

// 设置上下文
session_mgr.set_context(&session.id, "key", "value").await?;

// 获取上下文
let value = session_mgr.get_context(&session.id, "key").await?;

// 关闭会话
session_mgr.close(&session.id).await?;
```

### SkillManager

技能管理模块，提供技能加载和执行：

```rust
use agentos_rs::modules::SkillManager;

let skill_mgr = SkillManager::new(config.clone());

// 加载技能
let skill = skill_mgr.load("browser_skill", None).await?;

// 获取技能信息
let info = skill.get_info().await?;
println!("Skill: {} v{}", info.name, info.version);

// 执行技能
let result = skill.execute(Some(params)).await?;

// 注册技能
skill_mgr.register("custom_skill", "1.0.0", Some(schema)).await?;

// 更新技能
skill_mgr.update("custom_skill", "1.0.1").await?;

// 卸载技能
skill_mgr.unload("custom_skill").await?;

// 列出所有技能
let skills = skill_mgr.list().await?;
```

## 四层记忆系统

AgentOS Rust SDK 支持统一的四层记忆模型：

| 层级 | 名称 | 描述 | 示例 |
|------|------|------|------|
| **L1** | 原始卷 | 原始感官数据 | 图像像素、音频波形 |
| **L2** | 特征层 | 提取的特征向量 | SIFT 特征、MFCC |
| **L3** | 结构层 | 结构化知识 | 概念图、关系网 |
| **L4** | 模式层 | 抽象模式 | 因果链、数学结构 |

```rust
// L1: 原始数据
memory_mgr.write("image_pixels_data", "L1", None, None).await?;

// L2: 特征向量
memory_mgr.write("sift_features", "L2", Some(vec!["特征".to_string()]), None).await?;

// L3: 结构化知识
memory_mgr.write("concept_graph", "L3", Some(vec!["知识图谱".to_string()]), None).await?;

// L4: 抽象模式
memory_mgr.write("causal_chain", "L4", Some(vec!["因果推理".to_string()]), None).await?;
```

## 错误处理

SDK 提供了完整的错误类型体系：

```rust
use agentos_rs::error::{AgentOSError, ErrorCode};

match result {
    Ok(value) => println!("Success: {:?}", value),
    Err(e) => {
        match e.code {
            ErrorCode::INVALID_PARAMS => {
                println!("Invalid parameters: {}", e.message);
            }
            ErrorCode::NETWORK_ERROR => {
                println!("Network error: {}", e.message);
            }
            ErrorCode::TIMEOUT => {
                println!("Request timeout: {}", e.message);
            }
            _ => {
                println!("Unknown error: {}", e.message);
            }
        }
    }
}
```

## 类型定义

### TaskStatus

```rust
pub enum TaskStatus {
    Pending,
    Running,
    Completed,
    Failed,
    Cancelled,
}
```

### Memory

```rust
pub struct Memory {
    pub id: String,
    pub content: String,
    pub level: String,
    pub tags: Option<Vec<String>>,
    pub metadata: Option<serde_json::Value>,
    pub created_at: String,
}
```

### Session

```rust
pub struct Session {
    pub id: String,
    pub user_id: String,
    pub context: serde_json::Value,
    pub created_at: String,
    pub updated_at: String,
}
```

### SkillInfo

```rust
pub struct SkillInfo {
    pub name: String,
    pub version: String,
    pub description: String,
    pub parameters: Option<serde_json::Value>,
}
```

## 完整示例

```rust
use agentos_rs::{Config, Client, modules::{TaskManager, MemoryManager, SessionManager, SkillManager}};
use agentos_rs::types::TaskStatus;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    // 初始化客户端
    let config = Config::builder()
        .base_url("http://localhost:8080")
        .api_key("your-api-key")
        .timeout(60)
        .build()?;

    let client = Client::new(config)?;

    // 任务管理示例
    println!("=== Task Management ===");
    let task_mgr = TaskManager::new(client.config().clone());
    let task = task_mgr.submit_task(
        "分析2026年第一季度销售数据",
        Some("high"),
        Some(vec!["数据分析".to_string(), "销售".to_string()]),
    ).await?;
    println!("Task ID: {}", task.id);

    let result = task_mgr.wait(&task.id, Some(60000)).await?;
    match result.status {
        TaskStatus::Completed => println!("Task completed: {:?}", result.output),
        TaskStatus::Failed => println!("Task failed: {:?}", result.error),
        TaskStatus::Cancelled => println!("Task was cancelled"),
        _ => println!("Task status: {:?}", result.status),
    }

    // 记忆管理示例
    println!("\n=== Memory Management ===");
    let memory_mgr = MemoryManager::new(client.config().clone());
    let memory_id = memory_mgr.write(
        "用户喜欢使用 Rust 进行系统编程",
        "L2",
        Some(vec!["Rust".to_string(), "系统编程".to_string()]),
        None,
    ).await?;
    println!("Created memory: {}", memory_id);

    let memories = memory_mgr.search("Rust 编程", Some("L2"), Some(5)).await?;
    println!("Found {} memories", memories.len());

    // 会话管理示例
    println!("\n=== Session Management ===");
    let session_mgr = SessionManager::new(client.config().clone());
    let session = session_mgr.create(
        "user_67890",
        Some(serde_json::json!({"language": "Rust"})),
    ).await?;
    println!("Session ID: {}", session.id);

    session_mgr.set_context(&session.id, "preferences", serde_json::json!({
        "theme": "dark"
    })).await?;

    session_mgr.close(&session.id).await?;
    println!("Session closed");

    // 技能管理示例
    println!("\n=== Skill Management ===");
    let skill_mgr = SkillManager::new(client.config().clone());
    let skill = skill_mgr.load("browser_skill", None).await?;
    println!("Loaded skill: {}", skill.name);

    let skill_info = skill.get_info().await?;
    println!("Skill version: {}", skill_info.version);

    let skill_result = skill.execute(Some(serde_json::json!({
        "url": "https://example.com"
    }))).await?;

    if skill_result.success {
        println!("Skill executed successfully");
    } else {
        println!("Skill execution failed");
    }

    Ok(())
}
```

## 性能测试

运行性能基准测试：

```bash
cargo test --release -- features bench
```

## 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](../../LICENSE) 文件

## 版本历史

- **v3.0.0** (当前版本)
  - 统一版本号为 3.0.0
  - 优化错误处理机制
  - 提升异步性能

- **v2.0.0**
  - 添加会话管理功能
  - 实现四层记忆系统
  - 完善类型定义

- **v1.0.0**
  - 初始版本
  - 基础任务和记忆管理功能

## 相关文档

- [AgentOS 主文档](../../README.md)
- [DESIGN.md](../../DESIGN.md)
- [Rust 语言官网](https://www.rust-lang.org/)
- [Tokio 异步运行时](https://tokio.rs/)
