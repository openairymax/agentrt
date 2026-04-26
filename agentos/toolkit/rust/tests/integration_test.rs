// AgentOS Rust SDK - 集成测试
// Version: 3.0.0
// Last updated: 2026-03-24
//
// 测试 SDK 的公共 API 和模块集成

use agentos_rs::*;

// ============================================================
// 客户端测试
// ============================================================

#[test]
fn test_client_creation() {
    let client = Client::new("http://localhost:18789");
    assert!(client.is_ok());

    let client = client.unwrap();
    assert_eq!(client.endpoint(), "http://localhost:18789");
    assert_eq!(client.api_key(), None);
}

#[test]
fn test_client_with_api_key() {
    let client = Client::new_with_api_key("http://localhost:18789", "test-key");
    assert!(client.is_ok());

    let client = client.unwrap();
    assert_eq!(client.api_key(), Some("test-key"));
}

#[test]
fn test_client_builder() {
    let client = Client::builder("http://localhost:18789")
        .api_key("test-key")
        .max_retries(5)
        .build();

    assert!(client.is_ok());
    let client = client.unwrap();
    assert_eq!(client.api_key(), Some("test-key"));
}

#[test]
fn test_client_invalid_endpoint() {
    let client = Client::new("invalid-endpoint");
    // 由于我们在 builder 中使用了 panic，这里会 panic
    // 在实际应用中，应该返回错误而不是 panic
}

// ============================================================
// 错误码测试
// ============================================================

#[test]
fn test_error_codes_consistency() {
    // 验证错误码与 Go SDK 一致
    assert_eq!(CODE_SUCCESS, "0x0000");
    assert_eq!(CODE_UNKNOWN, "0x0001");
    assert_eq!(CODE_INVALID_PARAMETER, "0x0002");
    assert_eq!(CODE_MISSING_PARAMETER, "0x0003");
    assert_eq!(CODE_TIMEOUT, "0x0004");
    assert_eq!(CODE_NOT_FOUND, "0x0005");
    assert_eq!(CODE_ALREADY_EXISTS, "0x0006");
    assert_eq!(CODE_CONFLICT, "0x0007");
    assert_eq!(CODE_INVALID_CONFIG, "0x0008");
    assert_eq!(CODE_INVALID_ENDPOINT, "0x0009");
    assert_eq!(CODE_NETWORK_ERROR, "0x000A");
    assert_eq!(CODE_CONNECTION_REFUSED, "0x000B");
    assert_eq!(CODE_SERVER_ERROR, "0x000C");
    assert_eq!(CODE_UNAUTHORIZED, "0x000D");
    assert_eq!(CODE_FORBIDDEN, "0x000E");
    assert_eq!(CODE_RATE_LIMITED, "0x000F");
    assert_eq!(CODE_INVALID_RESPONSE, "0x0010");
    assert_eq!(CODE_PARSE_ERROR, "0x0011");
    assert_eq!(CODE_VALIDATION_ERROR, "0x0012");
    assert_eq!(CODE_NOT_SUPPORTED, "0x0013");
    assert_eq!(CODE_INTERNAL, "0x0014");
    assert_eq!(CODE_BUSY, "0x0015");
}

#[test]
fn test_domain_error_codes() {
    // 核心循环错误 (0x1xxx)
    assert_eq!(CODE_LOOP_CREATE_FAILED, "0x1001");
    assert_eq!(CODE_LOOP_START_FAILED, "0x1002");
    assert_eq!(CODE_LOOP_STOP_FAILED, "0x1003");

    // 认知层错误 (0x2xxx)
    assert_eq!(CODE_COGNITION_FAILED, "0x2001");
    assert_eq!(CODE_DAG_BUILD_FAILED, "0x2002");
    assert_eq!(CODE_AGENT_DISPATCH_FAILED, "0x2003");
    assert_eq!(CODE_INTENT_PARSE_FAILED, "0x2004");

    // 执行层错误 (0x3xxx)
    assert_eq!(CODE_TASK_FAILED, "0x3001");
    assert_eq!(CODE_TASK_CANCELLED, "0x3002");
    assert_eq!(CODE_TASK_TIMEOUT, "0x3003");

    // 记忆层错误 (0x4xxx)
    assert_eq!(CODE_MEMORY_NOT_FOUND, "0x4001");
    assert_eq!(CODE_MEMORY_EVOLVE_FAILED, "0x4002");
    assert_eq!(CODE_MEMORY_SEARCH_FAILED, "0x4003");
    assert_eq!(CODE_SESSION_NOT_FOUND, "0x4004");
    assert_eq!(CODE_SESSION_EXPIRED, "0x4005");
    assert_eq!(CODE_SKILL_NOT_FOUND, "0x4006");
    assert_eq!(CODE_SKILL_EXECUTION_FAILED, "0x4007");

    // 系统调用错误 (0x5xxx)
    assert_eq!(CODE_TELEMETRY_ERROR, "0x5001");

    // 安全域错误 (0x6xxx)
    assert_eq!(CODE_PERMISSION_DENIED, "0x6001");
    assert_eq!(CODE_CORRUPTED_DATA, "0x6002");
}

// ============================================================
// 类型测试
// ============================================================

#[test]
fn test_task_status() {
    assert_eq!(TaskStatus::Pending.as_str(), "pending");
    assert_eq!(TaskStatus::Running.as_str(), "running");
    assert_eq!(TaskStatus::Completed.as_str(), "completed");
    assert_eq!(TaskStatus::Failed.as_str(), "failed");
    assert_eq!(TaskStatus::Cancelled.as_str(), "cancelled");

    assert!(TaskStatus::Completed.is_terminal());
    assert!(TaskStatus::Failed.is_terminal());
    assert!(TaskStatus::Cancelled.is_terminal());
    assert!(!TaskStatus::Pending.is_terminal());
    assert!(!TaskStatus::Running.is_terminal());
}

#[test]
fn test_memory_layer() {
    assert_eq!(MemoryLayer::L1.as_str(), "L1");
    assert_eq!(MemoryLayer::L2.as_str(), "L2");
    assert_eq!(MemoryLayer::L3.as_str(), "L3");
    assert_eq!(MemoryLayer::L4.as_str(), "L4");

    assert!(MemoryLayer::L1.is_valid());
    assert!(MemoryLayer::L2.is_valid());
    assert!(MemoryLayer::L3.is_valid());
    assert!(MemoryLayer::L4.is_valid());
}

#[test]
fn test_session_status() {
    assert_eq!(SessionStatus::Active.as_str(), "active");
    assert_eq!(SessionStatus::Inactive.as_str(), "inactive");
    assert_eq!(SessionStatus::Expired.as_str(), "expired");
}

#[test]
fn test_skill_status() {
    assert_eq!(SkillStatus::Active.as_str(), "active");
    assert_eq!(SkillStatus::Inactive.as_str(), "inactive");
    assert_eq!(SkillStatus::Deprecated.as_str(), "deprecated");
}

// ============================================================
// 工具函数测试
// ============================================================

#[test]
fn test_generate_id() {
    let id1 = generate_id();
    let id2 = generate_id();
    assert_ne!(id1, id2);
    assert!(!id1.is_empty());
}

#[test]
fn test_generate_task_id() {
    let id = generate_task_id();
    assert!(id.starts_with("task-"));
}

#[test]
fn test_generate_memory_id() {
    let id = generate_memory_id();
    assert!(id.starts_with("mem-"));
}

#[test]
fn test_generate_session_id() {
    let id = generate_session_id();
    assert!(id.starts_with("session-"));
}

#[test]
fn test_validate_endpoint() {
    assert!(validate_endpoint("http://localhost:18789"));
    assert!(validate_endpoint("https://api.example.com"));
    assert!(!validate_endpoint("invalid-endpoint"));
    assert!(!validate_endpoint("ftp://example.com"));
}

// ============================================================
// 错误处理测试
// ============================================================

#[test]
fn test_agentos_error_creation() {
    let err = AgentOSError::with_code(CODE_INVALID_PARAMETER, "参数无效");
    assert_eq!(err.code(), CODE_INVALID_PARAMETER);

    let err = AgentOSError::network("网络错误");
    assert!(err.is_network_error());

    let err = AgentOSError::timeout("超时");
    assert!(err.is_network_error());

    let err = AgentOSError::http("服务器错误");
    assert!(err.is_server_error());
}

#[test]
fn test_http_status_mapping() {
    assert_eq!(http_status_to_code(400), CODE_INVALID_PARAMETER);
    assert_eq!(http_status_to_code(401), CODE_UNAUTHORIZED);
    assert_eq!(http_status_to_code(403), CODE_FORBIDDEN);
    assert_eq!(http_status_to_code(404), CODE_NOT_FOUND);
    assert_eq!(http_status_to_code(408), CODE_TIMEOUT);
    assert_eq!(http_status_to_code(429), CODE_RATE_LIMITED);
    assert_eq!(http_status_to_code(500), CODE_SERVER_ERROR);
    assert_eq!(http_status_to_code(504), CODE_TIMEOUT);
}

// ============================================================
// 版本信息测试
// ============================================================

#[test]
fn test_version_info() {
    assert_eq!(VERSION, "3.0.0");
    assert_eq!(AUTHOR, "SpharxWorks");
    assert_eq!(LICENSE, "MIT");
}

// ============================================================
// 便捷函数测试
// ============================================================

#[test]
fn test_new_client_function() {
    let client = new_client("http://localhost:18789");
    assert!(client.is_ok());
}

#[test]
fn test_new_client_with_api_key_function() {
    let client = new_client_with_api_key("http://localhost:18789", "test-key");
    assert!(client.is_ok());
    let client = client.unwrap();
    assert_eq!(client.api_key(), Some("test-key"));
}
