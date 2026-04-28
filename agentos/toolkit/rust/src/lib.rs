// AgentOS Rust SDK - 主入口
// Version: 3.0.0
// Last updated: 2026-03-24
//
// SDK 顶层统一入口，提供版本信息并导出所有公共 API。
// 对应 Go SDK: agentos.go

// ============================================================
// 模块声明
// ============================================================

pub mod client;
pub mod error;
pub mod types;
pub mod utils;
pub mod modules;

// 保留旧模块以保持向后兼容（标记为 deprecated）
#[deprecated(since = "3.0.0", note = "请使用 modules::task::TaskManager")]
pub mod task;
#[deprecated(since = "3.0.0", note = "请使用 modules::memory::MemoryManager")]
pub mod memory;
#[deprecated(since = "3.0.0", note = "请使用 modules::session::SessionManager")]
pub mod session;
#[deprecated(since = "3.0.0", note = "请使用 modules::skill::SkillManager")]
pub mod skill;

// 其他模块
pub mod syscall;
pub mod telemetry;
pub mod agent;
pub mod plugin;

// ============================================================
// 版本信息
// ============================================================

/// SDK 版本号
pub const VERSION: &str = "3.0.0";

/// SDK 作者
pub const AUTHOR: &str = "SpharxWorks";

/// SDK 许可证
pub const LICENSE: &str = "MIT";

// ============================================================
// 公共 API 导出
// ============================================================

// 客户端层
pub use client::{APIClient, Client};

// 错误类型
pub use error::{AgentOSError, ErrorCode};

// Syscall 绑定
pub use syscall::{
    SyscallBinding, HttpSyscallBinding,
    SyscallNamespace, SyscallRequest, SyscallResponse,
    TaskSyscall, MemorySyscall, SessionSyscall, SkillSyscall, AgentSyscall,
};

// 类型定义
pub use types::{
    // 枚举类型
    TaskStatus, MemoryLayer, SessionStatus, SkillStatus, SpanStatus,
    // 领域模型
    Task, TaskResult, Memory, MemorySearchResult, Session, Skill, SkillResult, SkillInfo,
    // 请求/响应结构
    RequestOptions, RequestOption, APIResponse, HealthStatus, Metrics,
    // 列表查询选项
    PaginationOptions, SortOptions, FilterOptions, ListOptions,
    // 请求选项函数
    with_request_timeout, with_header, with_query_param,
};

// 插件框架
pub use plugin::{
    PluginState, PluginManifest, PluginDependency, PluginInfo,
    BasePlugin, PluginFactory, PluginRegistry, PluginManager,
    get_plugin_registry,
};

// 业务模块管理器
pub use modules::{
    TaskManager, MemoryManager, SessionManager, SkillManager,
};

// 工具函数
pub use utils::{
    extract_data_map, get_string, get_i64, get_f64, get_bool, get_map,
    get_string_map, get_array, get_interface_slice, parse_time_from_map,
    extract_int64_stats, build_url, generate_id, generate_task_id,
    generate_memory_id, generate_session_id, validate_endpoint,
    format_time, parse_time, deep_clone_value, merge_maps,
};

// 错误码常量（与 Go SDK 保持一致）
pub use error::{
    // 通用错误 (0x0xxx)
    CODE_SUCCESS, CODE_UNKNOWN, CODE_INVALID_PARAMETER, CODE_MISSING_PARAMETER,
    CODE_TIMEOUT, CODE_NOT_FOUND, CODE_ALREADY_EXISTS, CODE_CONFLICT,
    CODE_INVALID_CONFIG, CODE_INVALID_ENDPOINT, CODE_NETWORK_ERROR,
    CODE_CONNECTION_REFUSED, CODE_SERVER_ERROR, CODE_UNAUTHORIZED,
    CODE_FORBIDDEN, CODE_RATE_LIMITED, CODE_INVALID_RESPONSE,
    CODE_PARSE_ERROR, CODE_VALIDATION_ERROR, CODE_NOT_SUPPORTED,
    CODE_INTERNAL, CODE_BUSY,

    // 核心循环错误 (0x1xxx)
    CODE_LOOP_CREATE_FAILED, CODE_LOOP_START_FAILED, CODE_LOOP_STOP_FAILED,

    // 认知层错误 (0x2xxx)
    CODE_COGNITION_FAILED, CODE_DAG_BUILD_FAILED, CODE_AGENT_DISPATCH_FAILED,
    CODE_INTENT_PARSE_FAILED,

    // 执行层错误 (0x3xxx)
    CODE_TASK_FAILED, CODE_TASK_CANCELLED, CODE_TASK_TIMEOUT,

    // 记忆层错误 (0x4xxx)
    CODE_MEMORY_NOT_FOUND, CODE_MEMORY_EVOLVE_FAILED, CODE_MEMORY_SEARCH_FAILED,
    CODE_SESSION_NOT_FOUND, CODE_SESSION_EXPIRED,
    CODE_SKILL_NOT_FOUND, CODE_SKILL_EXECUTION_FAILED,

    // 系统调用错误 (0x5xxx)
    CODE_TELEMETRY_ERROR,

    // 安全域错误 (0x6xxx)
    CODE_PERMISSION_DENIED, CODE_CORRUPTED_DATA,

    // 工具函数
    http_status_to_code,
};

// ============================================================
// 便捷函数
// ============================================================

/// 创建新的 AgentOS 客户端
///
/// # 参数
/// - `endpoint`: AgentOS 服务端点地址
///
/// # 返回
/// 返回 Result<Client, AgentOSError>
///
/// # 示例
/// ```rust
/// use agentos_rs::new_client;
///
/// let client = new_client("http://localhost:18789");
/// ```
pub fn new_client(endpoint: &str) -> Result<Client, AgentOSError> {
    Client::new(endpoint)
}

/// 创建带 API Key 的 AgentOS 客户端
///
/// # 参数
/// - `endpoint`: AgentOS 服务端点地址
/// - `api_key`: API 密钥
///
/// # 返回
/// 返回 Result<Client, AgentOSError>
pub fn new_client_with_api_key(endpoint: &str, api_key: &str) -> Result<Client, AgentOSError> {
    Client::new_with_api_key(endpoint, api_key)
}

// ============================================================
// 测试模块
// ============================================================

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_version() {
        assert_eq!(VERSION, "3.0.0");
    }

    #[test]
    fn test_author() {
        assert_eq!(AUTHOR, "SpharxWorks");
    }

    #[test]
    fn test_license() {
        assert_eq!(LICENSE, "MIT");
    }

    #[test]
    fn test_error_codes_general() {
        assert_eq!(CODE_SUCCESS, "0x0000");
        assert_eq!(CODE_UNKNOWN, "0x0001");
        assert_eq!(CODE_INVALID_PARAMETER, "0x0002");
        assert_eq!(CODE_TIMEOUT, "0x0004");
        assert_eq!(CODE_NOT_FOUND, "0x0005");
        assert_eq!(CODE_NETWORK_ERROR, "0x000A");
        assert_eq!(CODE_SERVER_ERROR, "0x000C");
    }

    #[test]
    fn test_error_codes_domain() {
        assert_eq!(CODE_TASK_FAILED, "0x3001");
        assert_eq!(CODE_MEMORY_NOT_FOUND, "0x4001");
        assert_eq!(CODE_SESSION_NOT_FOUND, "0x4004");
        assert_eq!(CODE_SKILL_NOT_FOUND, "0x4006");
    }

    #[test]
    fn test_http_status_to_code() {
        assert_eq!(http_status_to_code(400), CODE_INVALID_PARAMETER);
        assert_eq!(http_status_to_code(401), CODE_UNAUTHORIZED);
        assert_eq!(http_status_to_code(403), CODE_FORBIDDEN);
        assert_eq!(http_status_to_code(404), CODE_NOT_FOUND);
        assert_eq!(http_status_to_code(408), CODE_TIMEOUT);
        assert_eq!(http_status_to_code(429), CODE_RATE_LIMITED);
        assert_eq!(http_status_to_code(500), CODE_SERVER_ERROR);
        assert_eq!(http_status_to_code(504), CODE_TIMEOUT);
        assert_eq!(http_status_to_code(418), CODE_UNKNOWN);
    }

    #[test]
    fn test_new_client() {
        let client = new_client("http://localhost:18789");
        assert!(client.is_ok());
    }

    #[test]
    fn test_new_client_with_api_key() {
        let client = new_client_with_api_key("http://localhost:18789", "test-api-key");
        assert!(client.is_ok());
        let client = client.unwrap();
        assert_eq!(client.api_key(), Some("test-api-key"));
    }

    #[test]
    fn test_task_status() {
        assert!(TaskStatus::Completed.is_terminal());
        assert!(TaskStatus::Failed.is_terminal());
        assert!(TaskStatus::Cancelled.is_terminal());
        assert!(!TaskStatus::Pending.is_terminal());
        assert!(!TaskStatus::Running.is_terminal());
    }

    #[test]
    fn test_memory_layer() {
        assert!(MemoryLayer::L1.is_valid());
        assert!(MemoryLayer::L2.is_valid());
        assert!(MemoryLayer::L3.is_valid());
        assert!(MemoryLayer::L4.is_valid());
    }
}
