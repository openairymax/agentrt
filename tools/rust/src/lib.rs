// AgentOS Rust SDK
// Version: 2.0.0
// Last updated: 2026-03-23

pub mod agent;
pub mod client;
pub mod error;
pub mod memory;
pub mod session;
pub mod skill;
pub mod syscall;
pub mod task;
pub mod telemetry;
pub mod types;

pub use client::Client;
pub use error::AgentOSError;
pub use memory::Memory;
pub use session::Session;
pub use skill::{Skill, SkillInfo, SkillResult};
pub use task::{Task, TaskResult, TaskStatus};

// Re-export error codes for convenience
pub use error::{
    CODE_SUCCESS, CODE_UNKNOWN, CODE_INVALID_PARAMETER, CODE_MISSING_PARAMETER,
    CODE_TIMEOUT, CODE_NOT_FOUND, CODE_ALREADY_EXISTS, CODE_CONFLICT,
    CODE_INVALID_CONFIG, CODE_INVALID_ENDPOINT, CODE_NETWORK_ERROR,
    CODE_CONNECTION_REFUSED, CODE_SERVER_ERROR, CODE_UNAUTHORIZED,
    CODE_FORBIDDEN, CODE_RATE_LIMITED, CODE_INVALID_RESPONSE,
    CODE_PARSE_ERROR, CODE_VALIDATION_ERROR, CODE_NOT_SUPPORTED,
    CODE_INTERNAL, CODE_BUSY,
    CODE_LOOP_CREATE_FAILED, CODE_LOOP_START_FAILED, CODE_LOOP_STOP_FAILED,
    CODE_COGNITION_FAILED, CODE_DAG_BUILD_FAILED, CODE_AGENT_DISPATCH_FAILED,
    CODE_INTENT_PARSE_FAILED,
    CODE_TASK_FAILED, CODE_TASK_CANCELLED, CODE_TASK_TIMEOUT,
    CODE_MEMORY_NOT_FOUND, CODE_MEMORY_EVOLVE_FAILED, CODE_MEMORY_SEARCH_FAILED,
    CODE_SESSION_NOT_FOUND, CODE_SESSION_EXPIRED,
    CODE_SKILL_NOT_FOUND, CODE_SKILL_EXECUTION_FAILED,
    CODE_TELEMETRY_ERROR,
    CODE_PERMISSION_DENIED, CODE_CORRUPTED_DATA,
    http_status_to_code,
};

#[cfg(test)]
mod tests {
    use super::*;

    // ===== Error Code Tests =====

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

    // ===== AgentOSError Tests =====

    #[test]
    fn test_agentos_error_with_code() {
        let err = AgentOSError::with_code("0x0001", "Test error");
        let msg = format!("{}", err);
        assert!(msg.contains("0x0001"));
        assert!(msg.contains("Test error"));
    }

    #[test]
    fn test_agentos_error_network() {
        let err = AgentOSError::Network("Connection refused".to_string());
        let msg = format!("{}", err);
        assert!(msg.contains("Network error"));
        assert!(msg.contains("Connection refused"));
    }

    #[test]
    fn test_agentos_error_timeout() {
        let err = AgentOSError::Timeout("Operation timed out".to_string());
        let msg = format!("{}", err);
        assert!(msg.contains("Timeout"));
    }

    #[test]
    fn test_agentos_error_task() {
        let err = AgentOSError::Task("Task execution failed".to_string());
        let msg = format!("{}", err);
        assert!(msg.contains("Task error"));
    }

    #[test]
    fn test_agentos_error_memory() {
        let err = AgentOSError::Memory("Memory not found".to_string());
        let msg = format!("{}", err);
        assert!(msg.contains("Memory error"));
    }

    #[test]
    fn test_agentos_error_session() {
        let err = AgentOSError::Session("Session expired".to_string());
        let msg = format!("{}", err);
        assert!(msg.contains("Session error"));
    }

    #[test]
    fn test_agentos_error_config() {
        let err = AgentOSError::Config("Invalid endpoint".to_string());
        let msg = format!("{}", err);
        assert!(msg.contains("Config error"));
    }

    // ===== Client Tests =====

    #[test]
    fn test_client_new_default() {
        let client = Client::new("");
        assert!(client.is_ok());
        let client = client.unwrap();
        assert_eq!(client.endpoint(), "http://localhost:18789");
    }

    #[test]
    fn test_client_new_with_endpoint() {
        let client = Client::new("http://localhost:9999");
        assert!(client.is_ok());
        let client = client.unwrap();
        assert_eq!(client.endpoint(), "http://localhost:9999");
    }

    #[test]
    fn test_client_new_trailing_slash() {
        let client = Client::new("http://localhost:9999/");
        assert!(client.is_ok());
        let client = client.unwrap();
        assert_eq!(client.endpoint(), "http://localhost:9999");
    }

    #[test]
    fn test_client_new_invalid_endpoint() {
        let result = Client::new("invalid-endpoint");
        assert!(result.is_err());
        match result {
            Err(AgentOSError::Config(msg)) => {
                assert!(msg.contains("http://") || msg.contains("https://"));
            }
            _ => panic!("Expected Config error"),
        }
    }

    #[test]
    fn test_client_new_https() {
        let client = Client::new("https://api.example.com");
        assert!(client.is_ok());
        let client = client.unwrap();
        assert_eq!(client.endpoint(), "https://api.example.com");
    }

    // ===== Task Tests =====

    #[test]
    fn test_task_status_from_str() {
        assert_eq!(TaskStatus::from_str("pending"), Some(TaskStatus::Pending));
        assert_eq!(TaskStatus::from_str("running"), Some(TaskStatus::Running));
        assert_eq!(TaskStatus::from_str("completed"), Some(TaskStatus::Completed));
        assert_eq!(TaskStatus::from_str("failed"), Some(TaskStatus::Failed));
        assert_eq!(TaskStatus::from_str("cancelled"), Some(TaskStatus::Cancelled));
        assert_eq!(TaskStatus::from_str("unknown"), None);
    }

    #[test]
    fn test_task_status_as_str() {
        assert_eq!(TaskStatus::Pending.as_str(), "pending");
        assert_eq!(TaskStatus::Running.as_str(), "running");
        assert_eq!(TaskStatus::Completed.as_str(), "completed");
        assert_eq!(TaskStatus::Failed.as_str(), "failed");
        assert_eq!(TaskStatus::Cancelled.as_str(), "cancelled");
    }

    #[test]
    fn test_task_new() {
        let client = Client::new("http://localhost:18789").unwrap();
        let task = Task::new(client, "task-123".to_string());
        assert_eq!(task.task_id(), "task-123");
    }

    // ===== Memory Tests =====

    #[test]
    fn test_memory_new() {
        let mut metadata = std::collections::HashMap::new();
        metadata.insert("key".to_string(), serde_json::json!("value"));
        
        let memory = Memory::new(
            "mem-123".to_string(),
            "Test content".to_string(),
            "2026-03-23T10:00:00Z".to_string(),
            metadata.clone(),
        );
        
        assert_eq!(memory.id, "mem-123");
        assert_eq!(memory.content, "Test content");
        assert_eq!(memory.created_at, "2026-03-23T10:00:00Z");
    }

    #[test]
    fn test_memory_from_json() {
        let mut json = serde_json::Map::new();
        json.insert("memory_id".to_string(), serde_json::json!("mem-123"));
        json.insert("content".to_string(), serde_json::json!("Test content"));
        json.insert("created_at".to_string(), serde_json::json!("2026-03-23T10:00:00Z"));
        json.insert("metadata".to_string(), serde_json::json!({}));

        let memory = Memory::from_json(&json);
        assert!(memory.is_ok());
        let memory = memory.unwrap();
        assert_eq!(memory.id, "mem-123");
        assert_eq!(memory.content, "Test content");
    }

    #[test]
    fn test_memory_from_json_missing_id() {
        let mut json = serde_json::Map::new();
        json.insert("content".to_string(), serde_json::json!("Test content"));

        let result = Memory::from_json(&json);
        assert!(result.is_err());
    }

    // ===== Session Tests =====

    #[test]
    fn test_session_new() {
        let client = Client::new("http://localhost:18789").unwrap();
        let session = Session::new(client, "session-456".to_string());
        assert_eq!(session.id, "session-456");
    }

    // ===== Skill Tests =====

    #[test]
    fn test_skill_new() {
        let client = Client::new("http://localhost:18789").unwrap();
        let skill = Skill::new(client, "browser_skill".to_string());
        assert_eq!(skill.name, "browser_skill");
    }

    // ===== Async Tests =====

    #[tokio::test]
    async fn test_client_health_check_offline() {
        let client = Client::new("http://localhost:59999").unwrap();
        let result = client.health().await;
        assert!(result.is_ok());
        assert_eq!(result.unwrap(), false);
    }

    #[tokio::test]
    async fn test_client_submit_task_offline() {
        let client = Client::new("http://localhost:59999").unwrap();
        let result = client.submit_task("test task").await;
        assert!(result.is_err());
    }

    #[tokio::test]
    async fn test_client_create_session_offline() {
        let client = Client::new("http://localhost:59999").unwrap();
        let result = client.create_session().await;
        assert!(result.is_err());
    }

    #[tokio::test]
    async fn test_client_write_memory_offline() {
        let client = Client::new("http://localhost:59999").unwrap();
        let result = client.write_memory("test content", None).await;
        assert!(result.is_err());
    }

    #[tokio::test]
    async fn test_client_search_memory_offline() {
        let client = Client::new("http://localhost:59999").unwrap();
        let result = client.search_memory("test query", 5).await;
        assert!(result.is_err());
    }

    #[tokio::test]
    async fn test_client_get_memory_offline() {
        let client = Client::new("http://localhost:59999").unwrap();
        let result = client.get_memory("mem-123").await;
        assert!(result.is_err());
    }

    #[tokio::test]
    async fn test_client_delete_memory_offline() {
        let client = Client::new("http://localhost:59999").unwrap();
        let result = client.delete_memory("mem-123").await;
        assert!(result.is_err());
    }

    #[tokio::test]
    async fn test_client_load_skill() {
        let client = Client::new("http://localhost:59999").unwrap();
        let result = client.load_skill("test_skill").await;
        assert!(result.is_ok());
        let skill = result.unwrap();
        assert_eq!(skill.name, "test_skill");
    }
}
