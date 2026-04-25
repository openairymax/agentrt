// AgentOS Rust SDK - 统一错误体系
// Version: 3.0.0
// Last updated: 2026-04-05
//
// 定义 SDK 的完整错误类型层级、错误码枚举、哨兵错误和 HTTP 状态码映射。
// 所有异常继承自 AgentOSError，支持错误链追踪。
// 对应 Go SDK: errors.go

use std::fmt;
use thiserror::Error;

// ============================================================
// 错误码常量
// 基于 ErrorCodeReference.md 规范，采用十六进制分类体系：
//   0x0xxx 通用错误 (General)
//   0x1xxx 核心循环错误 (CoreLoop)
//   0x2xxx 认知层错误 (Cognition)
//   0x3xxx 执行层错误 (Execution)
//   0x4xxx 记忆层错误 (Memory)
//   0x5xxx 系统调用错误 (Syscall)
//   0x6xxx 安全域错误 (Security)
//   0x7xxx 动态模块错误 (gateway)
// ============================================================

/// 通用错误码 (0x0xxx)
pub const CODE_SUCCESS: &str = "0x0000";
pub const CODE_UNKNOWN: &str = "0x0001";
pub const CODE_INVALID_PARAMETER: &str = "0x0002";
pub const CODE_MISSING_PARAMETER: &str = "0x0003";
pub const CODE_TIMEOUT: &str = "0x0004";
pub const CODE_NOT_FOUND: &str = "0x0005";
pub const CODE_ALREADY_EXISTS: &str = "0x0006";
pub const CODE_CONFLICT: &str = "0x0007";
pub const CODE_INVALID_CONFIG: &str = "0x0008";
pub const CODE_INVALID_ENDPOINT: &str = "0x0009";
pub const CODE_NETWORK_ERROR: &str = "0x000A";
pub const CODE_CONNECTION_REFUSED: &str = "0x000B";
pub const CODE_SERVER_ERROR: &str = "0x000C";
pub const CODE_UNAUTHORIZED: &str = "0x000D";
pub const CODE_FORBIDDEN: &str = "0x000E";
pub const CODE_RATE_LIMITED: &str = "0x000F";
pub const CODE_INVALID_RESPONSE: &str = "0x0010";
pub const CODE_PARSE_ERROR: &str = "0x0011";
pub const CODE_VALIDATION_ERROR: &str = "0x0012";
pub const CODE_NOT_SUPPORTED: &str = "0x0013";
pub const CODE_INTERNAL: &str = "0x0014";
pub const CODE_BUSY: &str = "0x0015";

/// 核心循环错误码 (0x1xxx)
pub const CODE_LOOP_CREATE_FAILED: &str = "0x1001";
pub const CODE_LOOP_START_FAILED: &str = "0x1002";
pub const CODE_LOOP_STOP_FAILED: &str = "0x1003";

/// 认知层错误码 (0x2xxx)
pub const CODE_COGNITION_FAILED: &str = "0x2001";
pub const CODE_DAG_BUILD_FAILED: &str = "0x2002";
pub const CODE_AGENT_DISPATCH_FAILED: &str = "0x2003";
pub const CODE_INTENT_PARSE_FAILED: &str = "0x2004";

/// 执行层错误码 (0x3xxx)
pub const CODE_TASK_FAILED: &str = "0x3001";
pub const CODE_TASK_CANCELLED: &str = "0x3002";
pub const CODE_TASK_TIMEOUT: &str = "0x3003";

/// 记忆层错误码 (0x4xxx)
pub const CODE_MEMORY_NOT_FOUND: &str = "0x4001";
pub const CODE_MEMORY_EVOLVE_FAILED: &str = "0x4002";
pub const CODE_MEMORY_SEARCH_FAILED: &str = "0x4003";
pub const CODE_SESSION_NOT_FOUND: &str = "0x4004";
pub const CODE_SESSION_EXPIRED: &str = "0x4005";
pub const CODE_SKILL_NOT_FOUND: &str = "0x4006";
pub const CODE_SKILL_EXECUTION_FAILED: &str = "0x4007";

/// 系统调用错误码 (0x5xxx)
pub const CODE_TELEMETRY_ERROR: &str = "0x5001";

/// 安全域错误码 (0x6xxx)
pub const CODE_PERMISSION_DENIED: &str = "0x6001";
pub const CODE_CORRUPTED_DATA: &str = "0x6002";

// ============================================================
// 错误类型定义
// ============================================================

/// ErrorCode 表示 AgentOS SDK 的错误码类型
pub type ErrorCode = &'static str;

/// AgentOSError 是 SDK 所有错误的统一基类
#[derive(Error, Debug, Clone)]
pub enum AgentOSError {
    /// 带错误码的错误
    #[error("[{code}] {message}")]
    WithCode {
        code: String,
        message: String,
    },

    /// 网络错误
    #[error("[{}] {}", CODE_NETWORK_ERROR, .0)]
    Network(String),

    /// HTTP 错误
    #[error("[{}] {}", CODE_SERVER_ERROR, .0)]
    Http(String),

    /// JSON 解析错误
    #[error("[{}] {}", CODE_PARSE_ERROR, .0)]
    Json(String),

    /// 任务错误
    #[error("[{}] {}", CODE_TASK_FAILED, .0)]
    Task(String),

    /// 记忆错误
    #[error("[{}] {}", CODE_MEMORY_NOT_FOUND, .0)]
    Memory(String),

    /// 会话错误
    #[error("[{}] {}", CODE_SESSION_NOT_FOUND, .0)]
    Session(String),

    /// 技能错误
    #[error("[{}] {}", CODE_SKILL_NOT_FOUND, .0)]
    Skill(String),

    /// 超时错误
    #[error("[{}] {}", CODE_TIMEOUT, .0)]
    Timeout(String),

    /// 无效响应错误
    #[error("[{}] {}", CODE_INVALID_RESPONSE, .0)]
    InvalidResponse(String),

    /// 配置错误
    #[error("[{}] {}", CODE_INVALID_CONFIG, .0)]
    Config(String),

    /// 连接被拒绝错误
    #[error("[{}] {}", CODE_CONNECTION_REFUSED, .0)]
    ConnectionRefused(String),

    /// 未授权错误
    #[error("[{}] {}", CODE_UNAUTHORIZED, .0)]
    Unauthorized(String),

    /// 禁止访问错误
    #[error("[{}] {}", CODE_FORBIDDEN, .0)]
    Forbidden(String),

    /// 未找到错误
    #[error("[{}] {}", CODE_NOT_FOUND, .0)]
    NotFound(String),

    /// 参数无效错误
    #[error("[{}] {}", CODE_INVALID_PARAMETER, .0)]
    InvalidParameter(String),

    /// 缺少参数错误
    #[error("[{}] {}", CODE_MISSING_PARAMETER, .0)]
    MissingParameter(String),

    /// 其他错误
    #[error("[{}] {}", CODE_UNKNOWN, .0)]
    Other(String),
}

impl AgentOSError {
    /// 创建带错误码的错误
    ///
    /// # 参数
    /// - `code`: 错误码
    /// - `message`: 错误消息
    ///
    /// # 返回
    /// 返回 AgentOSError::WithCode 变体
    pub fn with_code(code: &str, message: &str) -> Self {
        AgentOSError::WithCode {
            code: code.to_string(),
            message: message.to_string(),
        }
    }

    /// 创建网络错误
    pub fn network(message: &str) -> Self {
        AgentOSError::Network(message.to_string())
    }

    /// 创建 HTTP 错误
    pub fn http(message: &str) -> Self {
        AgentOSError::Http(message.to_string())
    }

    /// 创建 JSON 解析错误
    pub fn json(message: &str) -> Self {
        AgentOSError::Json(message.to_string())
    }

    /// 创建任务错误
    pub fn task(message: &str) -> Self {
        AgentOSError::Task(message.to_string())
    }

    /// 创建记忆错误
    pub fn memory(message: &str) -> Self {
        AgentOSError::Memory(message.to_string())
    }

    /// 创建会话错误
    pub fn session(message: &str) -> Self {
        AgentOSError::Session(message.to_string())
    }

    /// 创建技能错误
    pub fn skill(message: &str) -> Self {
        AgentOSError::Skill(message.to_string())
    }

    /// 创建超时错误
    pub fn timeout(message: &str) -> Self {
        AgentOSError::Timeout(message.to_string())
    }

    /// 创建无效响应错误
    pub fn invalid_response(message: &str) -> Self {
        AgentOSError::InvalidResponse(message.to_string())
    }

    /// 创建配置错误
    pub fn config(message: &str) -> Self {
        AgentOSError::Config(message.to_string())
    }

    /// 创建连接被拒绝错误
    pub fn connection_refused(message: &str) -> Self {
        AgentOSError::ConnectionRefused(message.to_string())
    }

    /// 创建未授权错误
    pub fn unauthorized(message: &str) -> Self {
        AgentOSError::Unauthorized(message.to_string())
    }

    /// 创建禁止访问错误
    pub fn forbidden(message: &str) -> Self {
        AgentOSError::Forbidden(message.to_string())
    }

    /// 创建未找到错误
    pub fn not_found(message: &str) -> Self {
        AgentOSError::NotFound(message.to_string())
    }

    /// 创建参数无效错误
    pub fn invalid_parameter(message: &str) -> Self {
        AgentOSError::InvalidParameter(message.to_string())
    }

    /// 创建缺少参数错误
    pub fn missing_parameter(message: &str) -> Self {
        AgentOSError::MissingParameter(message.to_string())
    }

    /// 创建解析错误
    pub fn parse_error(message: &str) -> Self {
        AgentOSError::Json(message.to_string())
    }

    /// 创建内部错误
    pub fn internal(message: &str) -> Self {
        AgentOSError::with_code(CODE_INTERNAL, message)
    }

    /// 获取错误码
    pub fn code(&self) -> &str {
        match self {
            AgentOSError::WithCode { code, .. } => code,
            AgentOSError::Network(_) => CODE_NETWORK_ERROR,
            AgentOSError::Http(_) => CODE_SERVER_ERROR,
            AgentOSError::Json(_) => CODE_PARSE_ERROR,
            AgentOSError::Task(_) => CODE_TASK_FAILED,
            AgentOSError::Memory(_) => CODE_MEMORY_NOT_FOUND,
            AgentOSError::Session(_) => CODE_SESSION_NOT_FOUND,
            AgentOSError::Skill(_) => CODE_SKILL_NOT_FOUND,
            AgentOSError::Timeout(_) => CODE_TIMEOUT,
            AgentOSError::InvalidResponse(_) => CODE_INVALID_RESPONSE,
            AgentOSError::Config(_) => CODE_INVALID_CONFIG,
            AgentOSError::ConnectionRefused(_) => CODE_CONNECTION_REFUSED,
            AgentOSError::Unauthorized(_) => CODE_UNAUTHORIZED,
            AgentOSError::Forbidden(_) => CODE_FORBIDDEN,
            AgentOSError::NotFound(_) => CODE_NOT_FOUND,
            AgentOSError::InvalidParameter(_) => CODE_INVALID_PARAMETER,
            AgentOSError::MissingParameter(_) => CODE_MISSING_PARAMETER,
            AgentOSError::Other(_) => CODE_UNKNOWN,
        }
    }

    /// 判断是否为网络相关错误
    pub fn is_network_error(&self) -> bool {
        matches!(
            self,
            AgentOSError::Network(_)
                | AgentOSError::Timeout(_)
                | AgentOSError::ConnectionRefused(_)
        )
    }

    /// 判断是否为服务端错误
    pub fn is_server_error(&self) -> bool {
        matches!(
            self,
            AgentOSError::Http(_)
                | AgentOSError::Task(_)
                | AgentOSError::Skill(_)
        )
    }
}

// ============================================================
// HTTP 状态码映射
// ============================================================

/// 将 HTTP 状态码映射为 SDK 错误码
///
/// # 参数
/// - `status`: HTTP 状态码
///
/// # 返回
/// 返回对应的错误码
pub fn http_status_to_code(status: u16) -> ErrorCode {
    match status {
        400 => CODE_INVALID_PARAMETER,
        401 => CODE_UNAUTHORIZED,
        403 => CODE_FORBIDDEN,
        404 => CODE_NOT_FOUND,
        408 => CODE_TIMEOUT,
        409 => CODE_CONFLICT,
        422 => CODE_VALIDATION_ERROR,
        429 => CODE_RATE_LIMITED,
        500 | 502 | 503 => CODE_SERVER_ERROR,
        504 => CODE_TIMEOUT,
        _ => CODE_UNKNOWN,
    }
}

/// 将 HTTP 状态码转换为 AgentOSError
///
/// # 参数
/// - `status`: HTTP 状态码
/// - `message`: 错误消息
///
/// # 返回
/// 返回对应的 AgentOSError
pub fn http_status_to_error(status: u16, message: &str) -> AgentOSError {
    let code = http_status_to_code(status);

    match status {
        400 => AgentOSError::invalid_parameter(message),
        401 => AgentOSError::unauthorized(message),
        403 => AgentOSError::forbidden(message),
        404 => AgentOSError::not_found(message),
        408 => AgentOSError::timeout(message),
        429 => AgentOSError::with_code(CODE_RATE_LIMITED, message),
        500..=599 => AgentOSError::http(message),
        _ => AgentOSError::with_code(code, message),
    }
}

// ============================================================
// 外部错误类型转换
// ============================================================

impl From<reqwest::Error> for AgentOSError {
    fn from(err: reqwest::Error) -> Self {
        if err.is_timeout() {
            AgentOSError::timeout(&err.to_string())
        } else if err.is_connect() {
            AgentOSError::connection_refused(&err.to_string())
        } else if err.is_request() {
            AgentOSError::network(&err.to_string())
        } else if err.is_response() {
            AgentOSError::http(&err.to_string())
        } else if err.is_body() || err.is_decode() {
            AgentOSError::json(&err.to_string())
        } else {
            AgentOSError::Other(err.to_string())
        }
    }
}

impl From<serde_json::Error> for AgentOSError {
    fn from(err: serde_json::Error) -> Self {
        AgentOSError::json(&err.to_string())
    }
}

impl From<std::io::Error> for AgentOSError {
    fn from(err: std::io::Error) -> Self {
        AgentOSError::with_code(CODE_INTERNAL, &err.to_string())
    }
}

// ============================================================
// 测试模块
// ============================================================

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_error_codes_format() {
        // 验证错误码格式为十六进制
        assert!(CODE_SUCCESS.starts_with("0x"));
        assert!(CODE_UNKNOWN.starts_with("0x"));
        assert!(CODE_TASK_FAILED.starts_with("0x"));
        assert!(CODE_MEMORY_NOT_FOUND.starts_with("0x"));
    }

    #[test]
    fn test_error_with_code() {
        let err = AgentOSError::with_code(CODE_INVALID_PARAMETER, "参数无效");
        assert_eq!(err.code(), CODE_INVALID_PARAMETER);

        let msg = format!("{}", err);
        assert!(msg.contains("0x0002"));
        assert!(msg.contains("参数无效"));
    }

    #[test]
    fn test_error_network() {
        let err = AgentOSError::network("连接失败");
        assert!(err.is_network_error());
        assert_eq!(err.code(), CODE_NETWORK_ERROR);
    }

    #[test]
    fn test_error_timeout() {
        let err = AgentOSError::timeout("操作超时");
        assert!(err.is_network_error());
        assert_eq!(err.code(), CODE_TIMEOUT);
    }

    #[test]
    fn test_error_server() {
        let err = AgentOSError::http("服务器错误");
        assert!(err.is_server_error());
        assert_eq!(err.code(), CODE_SERVER_ERROR);
    }

    #[test]
    fn test_http_status_to_code() {
        assert_eq!(http_status_to_code(400), CODE_INVALID_PARAMETER);
        assert_eq!(http_status_to_code(401), CODE_UNAUTHORIZED);
        assert_eq!(http_status_to_code(403), CODE_FORBIDDEN);
        assert_eq!(http_status_to_code(404), CODE_NOT_FOUND);
        assert_eq!(http_status_to_code(408), CODE_TIMEOUT);
        assert_eq!(http_status_to_code(409), CODE_CONFLICT);
        assert_eq!(http_status_to_code(422), CODE_VALIDATION_ERROR);
        assert_eq!(http_status_to_code(429), CODE_RATE_LIMITED);
        assert_eq!(http_status_to_code(500), CODE_SERVER_ERROR);
        assert_eq!(http_status_to_code(502), CODE_SERVER_ERROR);
        assert_eq!(http_status_to_code(503), CODE_SERVER_ERROR);
        assert_eq!(http_status_to_code(504), CODE_TIMEOUT);
        assert_eq!(http_status_to_code(418), CODE_UNKNOWN);
    }

    #[test]
    fn test_http_status_to_error() {
        let err = http_status_to_error(404, "资源未找到");
        assert_eq!(err.code(), CODE_NOT_FOUND);

        let err = http_status_to_error(500, "内部错误");
        assert_eq!(err.code(), CODE_SERVER_ERROR);
    }

    #[test]
    fn test_error_classification() {
        let network_err = AgentOSError::network("网络错误");
        assert!(network_err.is_network_error());
        assert!(!network_err.is_server_error());

        let server_err = AgentOSError::http("服务器错误");
        assert!(!server_err.is_network_error());
        assert!(server_err.is_server_error());

        let timeout_err = AgentOSError::timeout("超时");
        assert!(timeout_err.is_network_error());
        assert!(!timeout_err.is_server_error());
    }
}
