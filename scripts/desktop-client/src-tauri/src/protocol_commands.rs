use serde::{Deserialize, Serialize};
use tauri::State;
use crate::backend_client::{BackendClient, ProtocolAdapter, ProtocolTestResult};
use crate::commands::AppState;

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct ProtocolInfo {
    pub id: String,
    pub name: String,
    pub description: String,
    pub version: String,
    pub status: String,
    pub endpoint: String,
    pub capabilities: Vec<String>,
    pub color: String,
    pub icon: String,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct ProtocolConnectionTest {
    pub protocol_id: String,
    pub endpoint: String,
    pub success: bool,
    pub latency_ms: u64,
    pub message: String,
    pub details: Option<serde_json::Value>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct ProtocolMessage {
    pub protocol: String,
    pub method: String,
    pub params: serde_json::Value,
    pub id: Option<String>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct ProtocolResponse {
    pub protocol: String,
    pub success: bool,
    pub data: serde_json::Value,
    pub error: Option<String>,
    pub latency_ms: u64,
}

#[tauri::command]
pub async fn list_protocols(
    state: State<'_, AppState>,
) -> Result<Vec<ProtocolInfo>, String> {
    let client = get_backend_client(&state)?;

    match client.list_protocol_adapters().await {
        Ok(adapters) => {
            let protocols = adapters.into_iter().map(|a| adapter_to_info(&a)).collect();
            Ok(protocols)
        }
        Err(_) => {
            Ok(get_default_protocols())
        }
    }
}

#[tauri::command]
pub async fn test_protocol_connection(
    protocol_id: String,
    endpoint: String,
    state: State<'_, AppState>,
) -> Result<ProtocolConnectionTest, String> {
    let client = get_backend_client(&state)?;

    match client.test_protocol_connection(&protocol_id, &endpoint).await {
        Ok(result) => Ok(ProtocolConnectionTest {
            protocol_id: result.protocol,
            endpoint: result.endpoint,
            success: result.success,
            latency_ms: result.latency_ms,
            message: result.message,
            details: result.details,
        }),
        Err(e) => {
            let start = std::time::Instant::now();
            let success = test_local_connection(&endpoint).await;
            let latency = start.elapsed().as_millis() as u64;

            Ok(ProtocolConnectionTest {
                protocol_id,
                endpoint,
                success,
                latency_ms: latency,
                message: if success {
                    "Connection successful (local test)".to_string()
                } else {
                    format!("Connection failed: {}", e)
                },
                details: None,
            })
        }
    }
}

#[tauri::command]
pub async fn send_protocol_message(
    message: ProtocolMessage,
    state: State<'_, AppState>,
) -> Result<ProtocolResponse, String> {
    let client = get_backend_client(&state)?;
    let start = std::time::Instant::now();

    let result = match message.protocol.as_str() {
        "jsonrpc" | "json-rpc" => {
            client.send_jsonrpc(&message.method, message.params).await
        }
        "mcp" => {
            let params = serde_json::json!({
                "protocol": "mcp",
                "method": message.method,
                "params": message.params
            });
            client.send_jsonrpc("protocol.translate", params).await
        }
        "a2a" => {
            let params = serde_json::json!({
                "protocol": "a2a",
                "method": message.method,
                "params": message.params
            });
            client.send_jsonrpc("protocol.translate", params).await
        }
        "openai" => {
            let params = serde_json::json!({
                "protocol": "openai",
                "method": message.method,
                "params": message.params
            });
            client.send_jsonrpc("protocol.translate", params).await
        }
        _ => Err(format!("Unsupported protocol: {}", message.protocol)),
    };

    let latency = start.elapsed().as_millis() as u64;

    match result {
        Ok(data) => Ok(ProtocolResponse {
            protocol: message.protocol,
            success: true,
            data,
            error: None,
            latency_ms: latency,
        }),
        Err(e) => Ok(ProtocolResponse {
            protocol: message.protocol,
            success: false,
            data: serde_json::Value::Null,
            error: Some(e),
            latency_ms: latency,
        }),
    }
}

#[tauri::command]
pub async fn get_protocol_capabilities(
    protocol_id: String,
    _state: State<'_, AppState>,
) -> Result<Vec<serde_json::Value>, String> {
    let capabilities = match protocol_id.as_str() {
        "jsonrpc" | "json-rpc" => vec![
            serde_json::json!({"name": "agent.list", "description": "List registered agents", "params": []}),
            serde_json::json!({"name": "agent.register", "description": "Register new agent", "params": ["name", "type"]}),
            serde_json::json!({"name": "task.submit", "description": "Submit task to agent", "params": ["agent_id", "description"]}),
            serde_json::json!({"name": "task.list", "description": "List tasks", "params": []}),
            serde_json::json!({"name": "service.status", "description": "Get service status", "params": []}),
            serde_json::json!({"name": "config.get", "description": "Get configuration", "params": ["key"]}),
            serde_json::json!({"name": "config.set", "description": "Set configuration", "params": ["key", "value"]}),
            serde_json::json!({"name": "memory.store", "description": "Store memory entry", "params": ["type", "content"]}),
            serde_json::json!({"name": "memory.search", "description": "Search memory", "params": ["query"]}),
        ],
        "mcp" => vec![
            serde_json::json!({"name": "tools/list", "description": "List available MCP tools", "params": []}),
            serde_json::json!({"name": "tools/call", "description": "Call MCP tool", "params": ["name", "arguments"]}),
            serde_json::json!({"name": "resources/list", "description": "List MCP resources", "params": []}),
            serde_json::json!({"name": "resources/read", "description": "Read MCP resource", "params": ["uri"]}),
            serde_json::json!({"name": "prompts/list", "description": "List MCP prompts", "params": []}),
            serde_json::json!({"name": "prompts/get", "description": "Get MCP prompt", "params": ["name", "arguments"]}),
            serde_json::json!({"name": "completion/complete", "description": "Get completions", "params": ["ref", "argument"]}),
            serde_json::json!({"name": "sampling/createMessage", "description": "Create LLM message", "params": ["messages", "modelPreferences"]}),
        ],
        "a2a" => vec![
            serde_json::json!({"name": "agent/discover", "description": "Discover A2A agents", "params": ["filter"]}),
            serde_json::json!({"name": "agent/describe", "description": "Get agent card", "params": ["agent_id"]}),
            serde_json::json!({"name": "task/create", "description": "Create A2A task", "params": ["agent_id", "message"]}),
            serde_json::json!({"name": "task/get", "description": "Get task status", "params": ["task_id"]}),
            serde_json::json!({"name": "task/cancel", "description": "Cancel task", "params": ["task_id"]}),
            serde_json::json!({"name": "task/list", "description": "List agent tasks", "params": ["agent_id"]}),
            serde_json::json!({"name": "message/send", "description": "Send message to agent", "params": ["agent_id", "message"]}),
            serde_json::json!({"name": "message/stream", "description": "Stream message to agent", "params": ["agent_id", "message"]}),
        ],
        "openai" => vec![
            serde_json::json!({"name": "chat.completions.create", "description": "Create chat completion", "params": ["model", "messages"]}),
            serde_json::json!({"name": "models.list", "description": "List available models", "params": []}),
            serde_json::json!({"name": "models.retrieve", "description": "Get model details", "params": ["model"]}),
            serde_json::json!({"name": "embeddings.create", "description": "Create embeddings", "params": ["model", "input"]}),
            serde_json::json!({"name": "fine_tuning.jobs.create", "description": "Create fine-tuning job", "params": ["model", "training_file"]}),
            serde_json::json!({"name": "assistants.create", "description": "Create assistant", "params": ["model", "instructions"]}),
            serde_json::json!({"name": "threads.create", "description": "Create thread", "params": []}),
            serde_json::json!({"name": "runs.create", "description": "Create run", "params": ["thread_id", "assistant_id"]}),
        ],
        _ => vec![],
    };

    Ok(capabilities)
}

fn get_backend_client(state: &AppState) -> Result<BackendClient, String> {
    let config = state.config.lock().map_err(|e| e.to_string())?;
    Ok(BackendClient::new(crate::backend_client::BackendConfig {
        gateway_url: config.gateway_url.clone().unwrap_or_else(|| "http://localhost:18789".to_string()),
        timeout_seconds: config.timeout_seconds,
        api_key: config.api_key.clone(),
    }))
}

fn adapter_to_info(adapter: &ProtocolAdapter) -> ProtocolInfo {
    let (color, icon) = match adapter.protocol.as_str() {
        "mcp" => ("#4CAF50", "🔌"),
        "a2a" => ("#2196F3", "🤝"),
        "openai" => ("#FF9800", "🧠"),
        _ => ("#9E9E9E", "📡"),
    };

    ProtocolInfo {
        id: adapter.name.clone(),
        name: format!("{} v{}", adapter.protocol.to_uppercase(), adapter.version),
        description: format!("{} protocol adapter", adapter.protocol.to_uppercase()),
        version: adapter.version.clone(),
        status: adapter.status.clone(),
        endpoint: adapter.endpoint.clone(),
        capabilities: adapter.capabilities.clone(),
        color: color.to_string(),
        icon: icon.to_string(),
    }
}

fn get_default_protocols() -> Vec<ProtocolInfo> {
    vec![
        ProtocolInfo {
            id: "jsonrpc".to_string(),
            name: "JSON-RPC 2.0".to_string(),
            description: "AgentOS native JSON-RPC protocol".to_string(),
            version: "2.0".to_string(),
            status: "active".to_string(),
            endpoint: "/jsonrpc".to_string(),
            capabilities: vec!["agent.list".to_string(), "task.submit".to_string(), "config.get".to_string()],
            color: "#4CAF50".to_string(),
            icon: "⚡".to_string(),
        },
        ProtocolInfo {
            id: "mcp".to_string(),
            name: "MCP v1.0".to_string(),
            description: "Model Context Protocol - tool & resource integration".to_string(),
            version: "1.0".to_string(),
            status: "active".to_string(),
            endpoint: "/mcp".to_string(),
            capabilities: vec!["tools/list".to_string(), "tools/call".to_string(), "resources/list".to_string()],
            color: "#4CAF50".to_string(),
            icon: "🔌".to_string(),
        },
        ProtocolInfo {
            id: "a2a".to_string(),
            name: "A2A v0.3".to_string(),
            description: "Agent-to-Agent Protocol - inter-agent communication".to_string(),
            version: "0.3.0".to_string(),
            status: "active".to_string(),
            endpoint: "/a2a".to_string(),
            capabilities: vec!["agent/discover".to_string(), "task/create".to_string(), "message/send".to_string()],
            color: "#2196F3".to_string(),
            icon: "🤝".to_string(),
        },
        ProtocolInfo {
            id: "openai".to_string(),
            name: "OpenAI API v1".to_string(),
            description: "OpenAI-compatible API - chat completions & models".to_string(),
            version: "1.0".to_string(),
            status: "active".to_string(),
            endpoint: "/v1".to_string(),
            capabilities: vec!["chat.completions.create".to_string(), "models.list".to_string()],
            color: "#FF9800".to_string(),
            icon: "🧠".to_string(),
        },
    ]
}

async fn test_local_connection(endpoint: &str) -> bool {
    let url = if endpoint.starts_with("http") {
        endpoint.to_string()
    } else {
        format!("http://localhost{}", endpoint)
    };

    reqwest::Client::new()
        .get(&url)
        .timeout(std::time::Duration::from_secs(5))
        .send()
        .await
        .is_ok()
}
