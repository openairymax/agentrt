Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# AgentOS Rust SDK

**版本**: v1.0.0.6  
**最后更新**: 2026-03-23  
**状态**: 🟢 生产就绪  
**Rust 版本**: 1.70+

---

## 🎯 概述

AgentOS Rust SDK 提供对 AgentOS 系统调用 API 的安全 Rust 封装。SDK 遵循 Rust 最佳实践，提供零成本抽象、内存安全和线程安全保证，同时保持与底层 C API 的完整功能对应。

---

## 📦 安装

```toml
# Cargo.toml
[dependencies]
agentos = "1.0"
tokio = { version = "1", features = ["full"] }
```

---

## 🚀 快速开始

### 创建 Agent 并执行任务

```rust
use agentos::{Agent, AgentConfig, TaskPriority, Result};

#[tokio::main]
async fn main() -> Result<()> {
    // 创建 Agent 配置
    let config = AgentConfig::builder()
        .name("my_agent")
        .description("My first AgentOS agent")
        .agent_type("chat")
        .max_concurrent_tasks(8)
        .task_queue_depth(64)
        .build()?;
    
    // 创建 Agent
    let agent = Agent::new(config).await?;
    
    // 提交任务
    let task_id = agent.submit_task()
        .description("分析用户反馈数据")
        .priority(TaskPriority::Normal)
        .input_params(json!({"data_source": "feedback.csv"}))
        .timeout_ms(30000)
        .send()
        .await?;
    
    println!("任务已提交: {}", task_id);
    
    // 等待任务完成
    let result = agent.wait_task(&task_id)
        .timeout_ms(35000)
        .await?;
    
    println!("任务结果: {}", result);
    
    Ok(())
}
```

### 记忆系统操作

```rust
use agentos::{MemoryClient, MemoryQuery, Result};

#[tokio::main]
async fn main() -> Result<()> {
    let client = MemoryClient::new()?;
    
    // 写入记忆
    let record_id = client.write()
        .content("用户对新产品功能表示满意，特别是实时协作功能")
        .metadata(json!({"type": "user_feedback", "rating": 5}))
        .importance(0.8)
        .tags(["feedback", "positive", "collaboration"])
        .send()
        .await?;
    
    println!("记忆写入成功: {}", record_id);
    
    // 语义检索
    let query = MemoryQuery::builder()
        .text("用户对协作功能的反馈")
        .limit(10)
        .similarity_threshold(0.6)
        .build();
    
    let results = client.query(&query).await?;
    
    for result in results {
        println!("[相似度: {:.2}] {}", result.similarity, result.content);
    }
    
    Ok(())
}
```

---

## 📖 API 参考

### Agent 模块

#### `AgentConfig`

```rust
/// Agent 配置构建器
pub struct AgentConfig {
    pub name: String,
    pub description: String,
    pub agent_type: AgentType,
    pub max_concurrent_tasks: u32,
    pub task_queue_depth: u32,
    pub default_task_timeout_ms: u32,
    pub system1_max_desc_len: u32,
    pub system1_max_priority: u8,
    pub cognition_config: Option<serde_json::Value>,
    pub memory_config: Option<serde_json::Value>,
    pub security_config: Option<serde_json::Value>,
    pub resource_limits: Option<ResourceLimits>,
    pub retry_config: Option<RetryConfig>,
}

impl AgentConfig {
    pub fn builder() -> AgentConfigBuilder { ... }
}

/// Agent 配置构建器
pub struct AgentConfigBuilder { ... }

impl AgentConfigBuilder {
    pub fn name(mut self, name: impl Into<String>) -> Self;
    pub fn description(mut self, desc: impl Into<String>) -> Self;
    pub fn agent_type(mut self, ty: impl Into<String>) -> Self;
    pub fn max_concurrent_tasks(mut self, n: u32) -> Self;
    pub fn task_queue_depth(mut self, depth: u32) -> Self;
    pub fn build(self) -> Result<AgentConfig>;
}
```

#### `Agent`

```rust
/// Agent 实例
pub struct Agent { ... }

impl Agent {
    /// 创建新 Agent
    pub async fn new(config: AgentConfig) -> Result<Self>;
    
    /// 获取 Agent ID
    pub fn id(&self) -> &str;
    
    /// 获取 Agent 状态
    pub fn state(&self) -> AgentState;
    
    /// 启动 Agent
    pub async fn start(&self) -> Result<()>;
    
    /// 暂停 Agent
    pub async fn pause(&self) -> Result<()>;
    
    /// 停止 Agent
    pub async fn stop(&self, force: bool) -> Result<()>;
    
    /// 绑定 Skill
    pub async fn bind_skill(&self, name: &str, config: Option<Value>) -> Result<()>;
    
    /// 解绑 Skill
    pub async fn unbind_skill(&self, name: &str) -> Result<()>;
    
    /// 提交任务（返回构建器）
    pub fn submit_task(&self) -> TaskSubmitBuilder<'_>;
    
    /// 查询任务
    pub async fn query_task(&self, task_id: &str) -> Result<TaskDescriptor>;
    
    /// 取消任务
    pub async fn cancel_task(&self, task_id: &str, force: bool) -> Result<()>;
    
    /// 等待任务完成
    pub async fn wait_task(&self, task_id: &str) -> TaskWaitBuilder<'_>;
    
    /// 列出任务
    pub async fn list_tasks(&self, filter: Option<TaskFilter>) -> Result<Vec<TaskDescriptor>>;
}

impl Drop for Agent {
    fn drop(&mut self);
}
```

### Memory 模块

#### `MemoryClient`

```rust
/// 记忆系统客户端
pub struct MemoryClient { ... }

impl MemoryClient {
    pub fn new() -> Result<Self>;
    
    /// 写入记忆（返回构建器）
    pub fn write(&self) -> MemoryWriteBuilder<'_>;
    
    /// 语义检索
    pub async fn query(&self, query: &MemoryQuery) -> Result<Vec<MemoryResult>>;
    
    /// 获取指定记录
    pub async fn get(&self, record_id: &str) -> Result<MemoryRecord>;
    
    /// 主动遗忘
    pub async fn forget(&self, policy: Option<ForgetPolicy>) -> Result<u64>;
    
    /// 触发记忆进化
    pub async fn evolve(&self, config: Option<Value>) -> Result<EvolutionStats>;
    
    /// 获取统计信息
    pub async fn stats(&self) -> Result<MemoryStatistics>;
}
```

#### `MemoryQuery`

```rust
/// 记忆查询条件
#[derive(Debug, Clone, Builder)]
pub struct MemoryQuery {
    pub text: Option<String>,
    pub vector: Option<Vec<f32>>,
    pub tags: Option<Vec<String>>,
    pub time_start: Option<u64>,
    pub time_end: Option<u64>,
    pub importance_threshold: f32,
    pub source_agent: Option<String>,
    pub layer: u8,
    pub limit: u32,
    pub similarity_threshold: f32,
    pub sort_by: MemorySortBy,
}

#[derive(Debug, Clone, Copy)]
pub enum MemorySortBy {
    TimeDesc,
    TimeAsc,
    Importance,
    Similarity,
    AccessCount,
}
```

### Session 模块

#### `SessionClient`

```rust
/// 会话管理客户端
pub struct SessionClient { ... }

impl SessionClient {
    pub fn new() -> Result<Self>;
    
    /// 创建会话
    pub async fn create(
        &self,
        agent_id: &str,
        session_type: SessionType,
        title: Option<&str>,
        metadata: Option<Value>,
    ) -> Result<String>;
    
    /// 获取会话信息
    pub async fn get(&self, session_id: &str) -> Result<SessionDescriptor>;
    
    /// 关闭会话
    pub async fn close(
        &self,
        session_id: &str,
        archive: bool,
        summary: Option<&str>,
    ) -> Result<()>;
    
    /// 添加消息
    pub async fn add_message(
        &self,
        session_id: &str,
        role: MessageRole,
        content: &str,
        metadata: Option<Value>,
    ) -> Result<String>;
    
    /// 列出消息
    pub async fn list_messages(
        &self,
        session_id: &str,
        offset: u32,
        limit: u32,
    ) -> Result<Vec<SessionMessage>>;
    
    /// 获取会话上下文
    pub async fn get_context(&self, session_id: &str) -> Result<SessionContext>;
    
    /// 设置会话上下文
    pub async fn set_context(&self, session_id: &str, ctx: &SessionContext) -> Result<()>;
}
```

### Telemetry 模块

#### `TelemetryClient`

```rust
/// 可观测性客户端
pub struct TelemetryClient { ... }

impl TelemetryClient {
    pub fn new() -> Result<Self>;
    
    /// 写入日志
    pub fn log(&self, level: LogLevel, module: &str, message: &str, fields: Option<Value>);
    
    /// 记录指标
    pub fn metric(&self, name: &str, value: f64, ty: MetricType, labels: Option<Value>);
    
    /// 创建追踪 span
    pub fn trace(&self, operation: &str, kind: SpanKind, attrs: Option<Value>) -> TraceGuard<'_>;
    
    /// 获取统计信息
    pub async fn stats(&self) -> Result<TelemetryStatistics>;
}

/// 追踪 Guard（RAII）
pub struct TraceGuard<'a> { ... }

impl<'a> Drop for TraceGuard<'a> {
    fn drop(&mut self);
}
```

---

## 🔒 安全保证

### 内存安全

- 所有 FFI 调用都通过安全封装
- 自动管理 C 侧资源的生命周期
- 使用 `Drop` trait 确保资源释放

### 线程安全

- 所有类型都实现 `Send` 和 `Sync`
- 内部使用 `Arc<Mutex<>>` 保护共享状态
- 异步运行时兼容 `tokio` 和 `async-std`

### 错误处理

```rust
use agentos::{Error, ErrorCode};

match agent.submit_task().send().await {
    Ok(task_id) => println!("任务提交成功: {}", task_id),
    Err(Error::AgentOSError { code, message, trace_id }) => {
        eprintln!("错误码: 0x{:04X}", code);
        eprintln!("错误信息: {}", message);
        eprintln!("trace_id: {}", trace_id);
        
        match code {
            ErrorCode::InvalidArgument => eprintln!("参数无效"),
            ErrorCode::ResourceLimit => eprintln!("资源限制超出"),
            _ => {}
        }
    }
    Err(e) => eprintln!("其他错误: {}", e),
}
```

---

## 🧪 测试

```rust
#[cfg(test)]
mod tests {
    use agentos::{Agent, AgentConfig};
    use agentos::testing::{MockAgent, MockMemoryClient};
    
    #[tokio::test]
    async fn test_agent_submit_task() {
        let mock_agent = MockAgent::new();
        
        let task_id = mock_agent.submit_task()
            .description("test task")
            .send()
            .await
            .unwrap();
        
        assert!(!task_id.is_empty());
        assert!(mock_agent.submit_task_called());
    }
}
```

---

## 📚 相关文档

- [系统调用 API](../../syscalls/) - 底层 C API 参考
- [Python SDK](../python/README.md) - Python SDK 文档
- [Go SDK](../go/README.md) - Go SDK 文档
- [开发指南：创建 Agent](../../../guides/folder/create_agent.md) - Agent 开发教程

---

© 2026 SPHARX Ltd. All Rights Reserved.
*"From data intelligence emerges."*