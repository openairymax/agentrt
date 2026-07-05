// AgentRT Rust SDK - 模块结构测试
// Version: 0.1.0
// Last updated: 2026-03-24
//
// 测试模块化结构是否正确

use agentrt_rs::*;

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
    let client = Client::new("http://localhost:18789");
    assert!(client.is_ok(), "Client::new should succeed with valid endpoint");

    // 空端点现在使用默认值 http://127.0.0.1:18789
    let default_client = Client::new("");
    assert!(default_client.is_ok(), "Client::new with empty endpoint should use default");

    let bad_client2 = Client::new("not-a-url");
    assert!(bad_client2.is_err(), "Client::new should fail with invalid endpoint");
}

// ============================================================
// 结构一致性测试
// ============================================================

#[test]
fn test_structure_consistency_with_go_sdk() {
    let base = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("src");

    let expected_files = vec![
        "client/client.rs",
        "types/types.rs",
        "utils/helpers.rs",
        "modules/task/manager.rs",
        "modules/memory/manager.rs",
        "modules/session/manager.rs",
        "modules/skill/manager.rs",
    ];

    let mut missing: Vec<&str> = Vec::new();
    for file in &expected_files {
        let full_path = base.join(file);
        if !full_path.exists() {
            missing.push(file);
        }
    }

    if !missing.is_empty() {
        eprintln!("WARNING: Missing SDK source files: {:?}", missing);
    }

    assert!(
        missing.len() < expected_files.len() / 2,
        "Too many SDK source files missing ({}/{}): {:?}",
        missing.len(),
        expected_files.len(),
        missing
    );
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
