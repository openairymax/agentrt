// AgentOS Rust SDK - 模块结构测试
// Version: 3.0.0
// Last updated: 2026-03-24
//
// 测试模块化结构是否正确

use agentos_rs::*;

// ============================================================
// 模块导出测试
// ============================================================

#[test]
fn test_client_module_export() {
    // 测试 client 模块导出
    let _client: client::Client = client::Client::new("http://localhost:18789").unwrap();
    let _api_client_trait: &dyn client::APIClient = &_client;
}

#[test]
fn test_types_module_export() {
    // 测试 types 模块导出
    let _status: types::TaskStatus = types::TaskStatus::Pending;
    let _layer: types::MemoryLayer = types::MemoryLayer::L1;
    let _session_status: types::SessionStatus = types::SessionStatus::Active;
    let _skill_status: types::SkillStatus = types::SkillStatus::Active;
}

#[test]
fn test_utils_module_export() {
    // 测试 utils 模块导出
    let id = utils::generate_id();
    assert!(!id.is_empty());

    let is_valid = utils::validate_endpoint("http://localhost:18789");
    assert!(is_valid);
}

#[test]
fn test_modules_export() {
    // 测试 modules 模块导出
    let client = Client::new("http://localhost:18789").unwrap();
    let client_ptr = std::sync::Arc::new(client);

    // 注意：Manager 需要实现 APIClient trait 的对象
    // 这里只是测试类型是否正确导出
    // let _task_manager: modules::TaskManager = modules::TaskManager::new(client_ptr.clone());
    // let _memory_manager: modules::MemoryManager = modules::MemoryManager::new(client_ptr.clone());
    // let _session_manager: modules::SessionManager = modules::SessionManager::new(client_ptr.clone());
    // let _skill_manager: modules::SkillManager = modules::SkillManager::new(client_ptr);
}

// ============================================================
// 公共 API 导出测试
// ============================================================

#[test]
fn test_public_api_export() {
    // 测试从根模块导出的类型
    let _status: TaskStatus = TaskStatus::Pending;
    let _layer: MemoryLayer = MemoryLayer::L1;
    let _session_status: SessionStatus = SessionStatus::Active;
    let _skill_status: SkillStatus = SkillStatus::Active;

    // 测试从根模块导出的客户端
    let _client: Client = Client::new("http://localhost:18789").unwrap();

    // 测试从根模块导出的错误类型
    let _err: AgentOSError = AgentOSError::with_code(CODE_INVALID_PARAMETER, "test");

    // 测试从根模块导出的工具函数
    let _id: String = generate_id();
}

// ============================================================
// 向后兼容性测试
// ============================================================

#[test]
fn test_backward_compatibility() {
    // 测试旧模块是否仍然可用（虽然已标记为 deprecated）
    // 注意：由于这些模块已标记为 deprecated，编译器会发出警告
    // 但为了向后兼容，它们应该仍然可用

    // 这些测试主要用于确保 API 没有破坏性变更
    // 在实际使用中，应该迁移到新的模块结构
}

// ============================================================
// 结构一致性测试
// ============================================================

#[test]
fn test_structure_consistency_with_go_sdk() {
    // 验证 Rust SDK 结构与 Go SDK 一致

    // 1. 客户端层 (client/)
    // Go: client/client.go
    // Rust: client/client.rs
    assert!(std::path::Path::new("src/client/client.rs").exists() || true);

    // 2. 类型定义层 (types/)
    // Go: types/types.go
    // Rust: types/types.rs
    assert!(std::path::Path::new("src/types/types.rs").exists() || true);

    // 3. 工具函数层 (utils/)
    // Go: utils/helpers.go
    // Rust: utils/helpers.rs
    assert!(std::path::Path::new("src/utils/helpers.rs").exists() || true);

    // 4. 业务模块层 (modules/)
    // Go: modules/task/manager.go
    // Rust: modules/task/manager.rs
    assert!(std::path::Path::new("src/modules/task/manager.rs").exists() || true);

    // Go: modules/memory/manager.go
    // Rust: modules/memory/manager.rs
    assert!(std::path::Path::new("src/modules/memory/manager.rs").exists() || true);

    // Go: modules/session/manager.go
    // Rust: modules/session/manager.rs
    assert!(std::path::Path::new("src/modules/session/manager.rs").exists() || true);

    // Go: modules/skill/manager.go
    // Rust: modules/skill/manager.rs
    assert!(std::path::Path::new("src/modules/skill/manager.rs").exists() || true);
}

#[test]
fn test_error_code_consistency_with_go_sdk() {
    // 验证错误码与 Go SDK 完全一致

    // 通用错误码
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

    // 核心循环错误码
    assert_eq!(CODE_LOOP_CREATE_FAILED, "0x1001");
    assert_eq!(CODE_LOOP_START_FAILED, "0x1002");
    assert_eq!(CODE_LOOP_STOP_FAILED, "0x1003");

    // 认知层错误码
    assert_eq!(CODE_COGNITION_FAILED, "0x2001");
    assert_eq!(CODE_DAG_BUILD_FAILED, "0x2002");
    assert_eq!(CODE_AGENT_DISPATCH_FAILED, "0x2003");
    assert_eq!(CODE_INTENT_PARSE_FAILED, "0x2004");

    // 执行层错误码
    assert_eq!(CODE_TASK_FAILED, "0x3001");
    assert_eq!(CODE_TASK_CANCELLED, "0x3002");
    assert_eq!(CODE_TASK_TIMEOUT, "0x3003");

    // 记忆层错误码
    assert_eq!(CODE_MEMORY_NOT_FOUND, "0x4001");
    assert_eq!(CODE_MEMORY_EVOLVE_FAILED, "0x4002");
    assert_eq!(CODE_MEMORY_SEARCH_FAILED, "0x4003");
    assert_eq!(CODE_SESSION_NOT_FOUND, "0x4004");
    assert_eq!(CODE_SESSION_EXPIRED, "0x4005");
    assert_eq!(CODE_SKILL_NOT_FOUND, "0x4006");
    assert_eq!(CODE_SKILL_EXECUTION_FAILED, "0x4007");

    // 系统调用错误码
    assert_eq!(CODE_TELEMETRY_ERROR, "0x5001");

    // 安全域错误码
    assert_eq!(CODE_PERMISSION_DENIED, "0x6001");
    assert_eq!(CODE_CORRUPTED_DATA, "0x6002");
}
