// AgentOS Rust SDK - 类型定义
// Version: 3.0.0
// Last updated: 2026-03-24
//
// 定义 SDK 中使用的所有枚举类型、领域模型和请求/响应结构。
// 对应 Go SDK: types/types.go

use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::time::Duration;

// ============================================================
// 枚举类型
// ============================================================

/// 任务状态枚举
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum TaskStatus {
    /// 待执行
    Pending,
    /// 执行中
    Running,
    /// 已完成
    Completed,
    /// 已失败
    Failed,
    /// 已取消
    Cancelled,
}

impl TaskStatus {
    /// 转换为字符串表示
    pub fn as_str(&self) -> &'static str {
        match self {
            TaskStatus::Pending => "pending",
            TaskStatus::Running => "running",
            TaskStatus::Completed => "completed",
            TaskStatus::Failed => "failed",
            TaskStatus::Cancelled => "cancelled",
        }
    }

    /// 从字符串解析任务状态
    pub fn from_str(s: &str) -> Option<Self> {
        match s {
            "pending" => Some(TaskStatus::Pending),
            "running" => Some(TaskStatus::Running),
            "completed" => Some(TaskStatus::Completed),
            "failed" => Some(TaskStatus::Failed),
            "cancelled" => Some(TaskStatus::Cancelled),
            _ => None,
        }
    }

    /// 判断任务是否处于终态
    pub fn is_terminal(&self) -> bool {
        matches!(self, TaskStatus::Completed | TaskStatus::Failed | TaskStatus::Cancelled)
    }
}

impl std::fmt::Display for TaskStatus {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.as_str())
    }
}

/// 记忆层级枚举（对应认知深度分层）
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum MemoryLayer {
    /// L1 - 工作记忆
    L1,
    /// L2 - 情景记忆
    L2,
    /// L3 - 语义记忆
    L3,
    /// L4 - 程序记忆
    L4,
}

impl MemoryLayer {
    /// 转换为字符串表示
    pub fn as_str(&self) -> &'static str {
        match self {
            MemoryLayer::L1 => "L1",
            MemoryLayer::L2 => "L2",
            MemoryLayer::L3 => "L3",
            MemoryLayer::L4 => "L4",
        }
    }

    /// 从字符串解析记忆层级
    pub fn from_str(s: &str) -> Option<Self> {
        match s.to_uppercase().as_str() {
            "L1" => Some(MemoryLayer::L1),
            "L2" => Some(MemoryLayer::L2),
            "L3" => Some(MemoryLayer::L3),
            "L4" => Some(MemoryLayer::L4),
            _ => None,
        }
    }

    /// 判断记忆层级是否合法
    pub fn is_valid(&self) -> bool {
        matches!(self, MemoryLayer::L1 | MemoryLayer::L2 | MemoryLayer::L3 | MemoryLayer::L4)
    }
}

impl std::fmt::Display for MemoryLayer {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.as_str())
    }
}

/// 会话状态枚举
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum SessionStatus {
    /// 活跃
    Active,
    /// 不活跃
    Inactive,
    /// 已过期
    Expired,
}

impl SessionStatus {
    /// 转换为字符串表示
    pub fn as_str(&self) -> &'static str {
        match self {
            SessionStatus::Active => "active",
            SessionStatus::Inactive => "inactive",
            SessionStatus::Expired => "expired",
        }
    }
}

impl std::fmt::Display for SessionStatus {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.as_str())
    }
}

/// 技能状态枚举
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum SkillStatus {
    /// 活跃
    Active,
    /// 不活跃
    Inactive,
    /// 已弃用
    Deprecated,
}

impl SkillStatus {
    /// 转换为字符串表示
    pub fn as_str(&self) -> &'static str {
        match self {
            SkillStatus::Active => "active",
            SkillStatus::Inactive => "inactive",
            SkillStatus::Deprecated => "deprecated",
        }
    }
}

impl std::fmt::Display for SkillStatus {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.as_str())
    }
}

/// 遥测 Span 状态枚举
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum SpanStatus {
    /// 正常
    Ok,
    /// 错误
    Error,
    /// 未设置
    Unset,
}

impl SpanStatus {
    /// 转换为字符串表示
    pub fn as_str(&self) -> &'static str {
        match self {
            SpanStatus::Ok => "ok",
            SpanStatus::Error => "error",
            SpanStatus::Unset => "unset",
        }
    }
}

impl std::fmt::Display for SpanStatus {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.as_str())
    }
}

// ============================================================
// 领域模型
// ============================================================

/// Task 表示 AgentOS 系统中的一个执行任务
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Task {
    /// 任务 ID
    #[serde(rename = "task_id")]
    pub id: String,
    /// 任务描述
    pub description: String,
    /// 任务状态
    pub status: TaskStatus,
    /// 任务优先级
    #[serde(default)]
    pub priority: i32,
    /// 任务输出
    #[serde(skip_serializing_if = "Option::is_none")]
    pub output: Option<String>,
    /// 错误信息
    #[serde(skip_serializing_if = "Option::is_none")]
    pub error: Option<String>,
    /// 元数据
    #[serde(default)]
    pub metadata: HashMap<String, serde_json::Value>,
    /// 创建时间
    pub created_at: String,
    /// 更新时间
    pub updated_at: String,
}

/// TaskResult 表示已完成任务的结果快照
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TaskResult {
    /// 任务 ID
    pub id: String,
    /// 任务状态
    pub status: TaskStatus,
    /// 任务输出
    #[serde(skip_serializing_if = "Option::is_none")]
    pub output: Option<String>,
    /// 错误信息
    #[serde(skip_serializing_if = "Option::is_none")]
    pub error: Option<String>,
    /// 开始时间
    pub start_time: String,
    /// 结束时间
    pub end_time: String,
    /// 执行时长（秒）
    pub duration: f64,
}

/// Memory 表示 AgentOS 系统中的一条记忆记录
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Memory {
    /// 记忆 ID
    #[serde(rename = "memory_id")]
    pub id: String,
    /// 记忆内容
    pub content: String,
    /// 记忆层级
    pub layer: MemoryLayer,
    /// 相关度分数
    #[serde(default)]
    pub score: f64,
    /// 元数据
    #[serde(default)]
    pub metadata: HashMap<String, serde_json::Value>,
    /// 创建时间
    pub created_at: String,
    /// 更新时间
    pub updated_at: String,
}

/// MemorySearchResult 表示记忆搜索的聚合结果
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MemorySearchResult {
    /// 记忆列表
    pub memories: Vec<Memory>,
    /// 总数
    pub total: i64,
    /// 查询字符串
    pub query: String,
    /// TopK 参数
    #[serde(rename = "top_k")]
    pub top_k: i32,
}

/// Session 表示用户与 Agent 交互的有状态通道
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Session {
    /// 会话 ID
    #[serde(rename = "session_id")]
    pub id: String,
    /// 用户 ID
    #[serde(skip_serializing_if = "Option::is_none")]
    pub user_id: Option<String>,
    /// 会话状态
    pub status: SessionStatus,
    /// 上下文数据
    #[serde(default)]
    pub context: HashMap<String, serde_json::Value>,
    /// 元数据
    #[serde(default)]
    pub metadata: HashMap<String, serde_json::Value>,
    /// 创建时间
    pub created_at: String,
    /// 最后活动时间
    pub last_activity: String,
}

/// Skill 表示 AgentOS 系统中的可插拔能力单元
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Skill {
    /// 技能 ID
    #[serde(rename = "skill_id")]
    pub id: String,
    /// 技能名称
    pub name: String,
    /// 版本号
    #[serde(skip_serializing_if = "Option::is_none")]
    pub version: Option<String>,
    /// 描述
    #[serde(skip_serializing_if = "Option::is_none")]
    pub description: Option<String>,
    /// 状态
    pub status: SkillStatus,
    /// 参数定义
    #[serde(default)]
    pub parameters: HashMap<String, serde_json::Value>,
    /// 元数据
    #[serde(default)]
    pub metadata: HashMap<String, serde_json::Value>,
    /// 创建时间
    pub created_at: String,
}

/// SkillResult 表示技能执行的结果
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SkillResult {
    /// 是否成功
    pub success: bool,
    /// 输出结果
    #[serde(skip_serializing_if = "Option::is_none")]
    pub output: Option<serde_json::Value>,
    /// 错误信息
    #[serde(skip_serializing_if = "Option::is_none")]
    pub error: Option<String>,
}

/// SkillInfo 表示技能的只读元信息
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SkillInfo {
    /// 技能名称
    pub name: String,
    /// 描述
    #[serde(skip_serializing_if = "Option::is_none")]
    pub description: Option<String>,
    /// 版本号
    #[serde(skip_serializing_if = "Option::is_none")]
    pub version: Option<String>,
    /// 参数定义
    #[serde(default)]
    pub parameters: HashMap<String, serde_json::Value>,
}

// ============================================================
// 请求/响应结构
// ============================================================

/// RequestOptions 单次请求的可选参数
#[derive(Debug, Clone, Default)]
pub struct RequestOptions {
    /// 请求超时
    pub timeout: Option<Duration>,
    /// 自定义请求头
    pub headers: HashMap<String, String>,
    /// 查询参数
    pub query_params: HashMap<String, String>,
}

/// RequestOption 请求选项函数签名
pub type RequestOption = Box<dyn Fn(&mut RequestOptions) + Send + Sync>;

/// 设置单次请求超时
pub fn with_request_timeout(timeout: Duration) -> RequestOption {
    Box::new(move |opts: &mut RequestOptions| {
        opts.timeout = Some(timeout);
    })
}

/// 添加自定义请求头
pub fn with_header(key: String, value: String) -> RequestOption {
    Box::new(move |opts: &mut RequestOptions| {
        opts.headers.insert(key.clone(), value.clone());
    })
}

/// 添加查询参数
pub fn with_query_param(key: String, value: String) -> RequestOption {
    Box::new(move |opts: &mut RequestOptions| {
        opts.query_params.insert(key.clone(), value.clone());
    })
}

/// APIResponse 通用 API 响应结构
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct APIResponse {
    /// 是否成功
    pub success: bool,
    /// 响应数据
    pub data: serde_json::Value,
    /// 消息
    #[serde(skip_serializing_if = "Option::is_none")]
    pub message: Option<String>,
}

/// HealthStatus 健康检查返回状态
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HealthStatus {
    /// 状态
    pub status: String,
    /// 版本
    pub version: String,
    /// 运行时长（秒）
    pub uptime: i64,
    /// 检查项
    pub checks: HashMap<String, String>,
}

/// Metrics 系统运行指标快照
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Metrics {
    /// 任务总数
    pub tasks_total: i64,
    /// 已完成任务数
    pub tasks_completed: i64,
    /// 失败任务数
    pub tasks_failed: i64,
    /// 记忆总数
    pub memories_total: i64,
    /// 活跃会话数
    pub sessions_active: i64,
    /// 已加载技能数
    pub skills_loaded: i64,
    /// CPU 使用率
    pub cpu_usage: f64,
    /// 内存使用率
    pub memory_usage: f64,
    /// 请求总数
    pub request_count: i64,
    /// 平均延迟（毫秒）
    pub average_latency_ms: f64,
}

// ============================================================
// 列表查询选项
// ============================================================

/// PaginationOptions 分页选项
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PaginationOptions {
    /// 页码
    pub page: i32,
    /// 每页大小
    pub page_size: i32,
}

impl Default for PaginationOptions {
    fn default() -> Self {
        Self {
            page: 1,
            page_size: 20,
        }
    }
}

impl PaginationOptions {
    /// 将分页参数转换为查询参数 map
    pub fn to_query_params(&self) -> HashMap<String, String> {
        let mut params = HashMap::new();
        if self.page > 0 {
            params.insert("page".to_string(), self.page.to_string());
        }
        if self.page_size > 0 {
            params.insert("page_size".to_string(), self.page_size.to_string());
        }
        params
    }
}

/// SortOptions 排序选项
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SortOptions {
    /// 排序字段
    pub field: String,
    /// 排序顺序
    pub order: String,
}

/// FilterOptions 过滤选项
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FilterOptions {
    /// 过滤键
    pub key: String,
    /// 过滤值
    pub value: serde_json::Value,
}

/// ListOptions 列表查询的复合选项
#[derive(Debug, Clone, Default)]
pub struct ListOptions {
    /// 分页选项
    pub pagination: Option<PaginationOptions>,
    /// 排序选项
    pub sort: Option<SortOptions>,
    /// 过滤选项
    pub filter: Option<FilterOptions>,
}

impl ListOptions {
    /// 将列表选项转换为查询参数 map
    pub fn to_query_params(&self) -> HashMap<String, String> {
        let mut params = HashMap::new();

        if let Some(ref pagination) = self.pagination {
            for (k, v) in pagination.to_query_params() {
                params.insert(k, v);
            }
        }

        if let Some(ref sort) = self.sort {
            if !sort.field.is_empty() {
                params.insert("sort_by".to_string(), sort.field.clone());
            }
            if !sort.order.is_empty() {
                params.insert("sort_order".to_string(), sort.order.clone());
            }
        }

        if let Some(ref filter) = self.filter {
            if !filter.key.is_empty() {
                params.insert("filter_key".to_string(), filter.key.clone());
                params.insert("filter_value".to_string(), filter.value.to_string());
            }
        }

        params
    }
}
