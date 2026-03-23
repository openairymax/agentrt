// AgentOS Rust SDK Error
// Version: 2.0.0
// Last updated: 2026-03-23
// �?Go SDK errors.go 保持一致的十六进制错误码体�?
use thiserror::Error;
use reqwest;

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

pub const CODE_LOOP_CREATE_FAILED: &str = "0x1001";
pub const CODE_LOOP_START_FAILED: &str = "0x1002";
pub const CODE_LOOP_STOP_FAILED: &str = "0x1003";

pub const CODE_COGNITION_FAILED: &str = "0x2001";
pub const CODE_DAG_BUILD_FAILED: &str = "0x2002";
pub const CODE_AGENT_DISPATCH_FAILED: &str = "0x2003";
pub const CODE_INTENT_PARSE_FAILED: &str = "0x2004";

pub const CODE_TASK_FAILED: &str = "0x3001";
pub const CODE_TASK_CANCELLED: &str = "0x3002";
pub const CODE_TASK_TIMEOUT: &str = "0x3003";

pub const CODE_MEMORY_NOT_FOUND: &str = "0x4001";
pub const CODE_MEMORY_EVOLVE_FAILED: &str = "0x4002";
pub const CODE_MEMORY_SEARCH_FAILED: &str = "0x4003";
pub const CODE_SESSION_NOT_FOUND: &str = "0x4004";
pub const CODE_SESSION_EXPIRED: &str = "0x4005";
pub const CODE_SKILL_NOT_FOUND: &str = "0x4006";
pub const CODE_SKILL_EXECUTION_FAILED: &str = "0x4007";

pub const CODE_TELEMETRY_ERROR: &str = "0x5001";

pub const CODE_PERMISSION_DENIED: &str = "0x6001";
pub const CODE_CORRUPTED_DATA: &str = "0x6002";

/** HTTP 状态码到错误码的映射，�?Go SDK HTTPStatusToError 一�?*/
pub fn http_status_to_code(status: u16) -> &'static str {
    match status {
        400 => CODE_INVALID_PARAMETER,
        401 => CODE_UNAUTHORIZED,
        403 => CODE_FORBIDDEN,
        404 => CODE_NOT_FOUND,
        408 => CODE_TIMEOUT,
        409 => CODE_CONFLICT,
        429 => CODE_RATE_LIMITED,
        422 => CODE_VALIDATION_ERROR,
        500 | 502 | 503 => CODE_SERVER_ERROR,
        504 => CODE_TIMEOUT,
        _ => CODE_UNKNOWN,
    }
}

#[derive(Error, Debug)]
pub enum AgentOSError {
    #[error("[{code}] {message}")]
    WithCode { code: String, message: String },

    #[error("Network error: {0}")]
    Network(String),

    #[error("HTTP error: {0}")]
    Http(String),

    #[error("JSON error: {0}")]
    Json(String),

    #[error("Task error: {0}")]
    Task(String),

    #[error("Memory error: {0}")]
    Memory(String),

    #[error("Session error: {0}")]
    Session(String),

    #[error("Skill error: {0}")]
    Skill(String),

    #[error("Timeout error: {0}")]
    Timeout(String),

    #[error("Invalid response: {0}")]
    InvalidResponse(String),

    #[error("Config error: {0}")]
    Config(String),

    #[error("Other error: {0}")]
    Other(String),
}

impl AgentOSError {
    /** 创建带错误码的错�?*/
    pub fn with_code(code: &str, message: &str) -> Self {
        AgentOSError::WithCode {
            code: code.to_string(),
            message: message.to_string(),
        }
    }
}

impl From<reqwest::Error> for AgentOSError {
    fn from(err: reqwest::Error) -> Self {
        if err.is_timeout() {
            AgentOSError::Timeout(err.to_string())
        } else if err.is_request() {
            AgentOSError::Network(err.to_string())
        } else if err.is_response() {
            AgentOSError::Http(err.to_string())
        } else {
            AgentOSError::Other(err.to_string())
        }
    }
}

impl From<serde_json::Error> for AgentOSError {
    fn from(err: serde_json::Error) -> Self {
        AgentOSError::Json(err.to_string())
    }
}
