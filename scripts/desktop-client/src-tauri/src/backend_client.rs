use serde::{Deserialize, Serialize};
use std::sync::Arc;
use tokio::sync::RwLock;

#[derive(Debug, Clone)]
pub struct BackendConfig {
    pub gateway_url: String,
    pub timeout_seconds: u64,
    pub api_key: Option<String>,
}

impl Default for BackendConfig {
    fn default() -> Self {
        Self {
            gateway_url: "http://localhost:18789".to_string(),
            timeout_seconds: 30,
            api_key: None,
        }
    }
}

#[derive(Debug, Clone)]
pub struct BackendClient {
    config: Arc<RwLock<BackendConfig>>,
    http: reqwest::Client,
}

impl BackendClient {
    pub fn new(config: BackendConfig) -> Self {
        let http = reqwest::Client::builder()
            .timeout(std::time::Duration::from_secs(config.timeout_seconds))
            .build()
            .unwrap_or_default();

        Self {
            config: Arc::new(RwLock::new(config)),
            http,
        }
    }

    pub async fn update_config(&self, config: BackendConfig) {
        let mut current = self.config.write().await;
        *current = config;
    }

    pub async fn get_gateway_url(&self) -> String {
        self.config.read().await.gateway_url.clone()
    }

    pub async fn health_check(&self) -> Result<HealthResponse, String> {
        let url = format!("{}/health", self.get_gateway_url().await);
        let resp = self.http.get(&url)
            .send()
            .await
            .map_err(|e| format!("Health check failed: {}", e))?;

        if !resp.status().is_success() {
            return Err(format!("Health check returned status {}", resp.status()));
        }

        resp.json::<HealthResponse>()
            .await
            .map_err(|e| format!("Failed to parse health response: {}", e))
    }

    pub async fn list_services(&self) -> Result<Vec<ServiceEntry>, String> {
        let url = format!("{}/api/v1/services", self.get_gateway_url().await);
        let resp = self.send_authenticated_request(reqwest::Method::GET, &url, None).await?;

        let result: ServicesResponse = resp.json()
            .await
            .map_err(|e| format!("Failed to parse services response: {}", e))?;

        Ok(result.services)
    }

    pub async fn get_service(&self, name: &str) -> Result<ServiceEntry, String> {
        let url = format!("{}/api/v1/services/{}", self.get_gateway_url().await, name);
        let resp = self.send_authenticated_request(reqwest::Method::GET, &url, None).await?;

        resp.json::<ServiceEntry>()
            .await
            .map_err(|e| format!("Failed to parse service response: {}", e))
    }

    pub async fn list_agents(&self) -> Result<Vec<AgentEntry>, String> {
        let url = format!("{}/api/v1/agents", self.get_gateway_url().await);
        let resp = self.send_authenticated_request(reqwest::Method::GET, &url, None).await?;

        let result: AgentsResponse = resp.json()
            .await
            .map_err(|e| format!("Failed to parse agents response: {}", e))?;

        Ok(result.agents)
    }

    pub async fn register_agent(&self, config: &AgentRegistration) -> Result<AgentEntry, String> {
        let url = format!("{}/api/v1/agents", self.get_gateway_url().await);
        let body = serde_json::to_value(config)
            .map_err(|e| format!("Failed to serialize agent config: {}", e))?;
        let resp = self.send_authenticated_request(reqwest::Method::POST, &url, Some(body)).await?;

        resp.json::<AgentEntry>()
            .await
            .map_err(|e| format!("Failed to parse agent registration response: {}", e))
    }

    pub async fn submit_task(&self, request: &TaskSubmission) -> Result<TaskEntry, String> {
        let url = format!("{}/api/v1/tasks", self.get_gateway_url().await);
        let body = serde_json::to_value(request)
            .map_err(|e| format!("Failed to serialize task request: {}", e))?;
        let resp = self.send_authenticated_request(reqwest::Method::POST, &url, Some(body)).await?;

        resp.json::<TaskEntry>()
            .await
            .map_err(|e| format!("Failed to parse task response: {}", e))
    }

    pub async fn list_tasks(&self) -> Result<Vec<TaskEntry>, String> {
        let url = format!("{}/api/v1/tasks", self.get_gateway_url().await);
        let resp = self.send_authenticated_request(reqwest::Method::GET, &url, None).await?;

        let result: TasksResponse = resp.json()
            .await
            .map_err(|e| format!("Failed to parse tasks response: {}", e))?;

        Ok(result.tasks)
    }

    pub async fn get_task(&self, task_id: &str) -> Result<TaskEntry, String> {
        let url = format!("{}/api/v1/tasks/{}", self.get_gateway_url().await, task_id);
        let resp = self.send_authenticated_request(reqwest::Method::GET, &url, None).await?;

        resp.json::<TaskEntry>()
            .await
            .map_err(|e| format!("Failed to parse task response: {}", e))
    }

    pub async fn cancel_task(&self, task_id: &str) -> Result<(), String> {
        let url = format!("{}/api/v1/tasks/{}/cancel", self.get_gateway_url().await, task_id);
        let _resp = self.send_authenticated_request(reqwest::Method::POST, &url, None).await?;
        Ok(())
    }

    pub async fn get_metrics(&self) -> Result<serde_json::Value, String> {
        let url = format!("{}/metrics", self.get_gateway_url().await);
        let resp = self.http.get(&url)
            .send()
            .await
            .map_err(|e| format!("Metrics request failed: {}", e))?;

        let text = resp.text().await.map_err(|e| format!("Failed to read metrics: {}", e))?;
        serde_json::from_str(&text).map_err(|e| format!("Failed to parse metrics: {}", e))
    }

    pub async fn get_config(&self, key: &str) -> Result<ConfigEntry, String> {
        let url = format!("{}/api/v1/config/{}", self.get_gateway_url().await, key);
        let resp = self.send_authenticated_request(reqwest::Method::GET, &url, None).await?;

        resp.json::<ConfigEntry>()
            .await
            .map_err(|e| format!("Failed to parse config response: {}", e))
    }

    pub async fn set_config(&self, key: &str, value: &str) -> Result<ConfigEntry, String> {
        let url = format!("{}/api/v1/config/{}", self.get_gateway_url().await, key);
        let body = serde_json::json!({"value": value});
        let resp = self.send_authenticated_request(reqwest::Method::PUT, &url, Some(body)).await?;

        resp.json::<ConfigEntry>()
            .await
            .map_err(|e| format!("Failed to parse config response: {}", e))
    }

    pub async fn test_protocol_connection(&self, protocol: &str, endpoint: &str) -> Result<ProtocolTestResult, String> {
        let url = format!("{}/api/v1/protocols/test", self.get_gateway_url().await);
        let body = serde_json::json!({
            "protocol": protocol,
            "endpoint": endpoint
        });
        let resp = self.send_authenticated_request(reqwest::Method::POST, &url, Some(body)).await?;

        resp.json::<ProtocolTestResult>()
            .await
            .map_err(|e| format!("Failed to parse protocol test response: {}", e))
    }

    pub async fn list_protocol_adapters(&self) -> Result<Vec<ProtocolAdapter>, String> {
        let url = format!("{}/api/v1/protocols/adapters", self.get_gateway_url().await);
        let resp = self.send_authenticated_request(reqwest::Method::GET, &url, None).await?;

        let result: ProtocolAdaptersResponse = resp.json()
            .await
            .map_err(|e| format!("Failed to parse adapters response: {}", e))?;

        Ok(result.adapters)
    }

    pub async fn send_jsonrpc(&self, method: &str, params: serde_json::Value) -> Result<serde_json::Value, String> {
        let url = format!("{}/jsonrpc", self.get_gateway_url().await);
        let body = serde_json::json!({
            "jsonrpc": "2.0",
            "id": uuid::Uuid::new_v4().to_string(),
            "method": method,
            "params": params
        });

        let resp = self.send_authenticated_request(reqwest::Method::POST, &url, Some(body)).await?;
        let result: serde_json::Value = resp.json()
            .await
            .map_err(|e| format!("Failed to parse JSON-RPC response: {}", e))?;

        if let Some(error) = result.get("error") {
            return Err(format!("JSON-RPC error: {}", error));
        }

        Ok(result.get("result").cloned().unwrap_or(serde_json::Value::Null))
    }

    async fn send_authenticated_request(
        &self,
        method: reqwest::Method,
        url: &str,
        body: Option<serde_json::Value>,
    ) -> Result<reqwest::Response, String> {
        let config = self.config.read().await;
        let mut req = self.http.request(method, url);

        if let Some(ref api_key) = config.api_key {
            req = req.header("Authorization", format!("Bearer {}", api_key));
        }

        if let Some(body) = body {
            req = req.json(&body);
        }

        let resp = req.send().await.map_err(|e| format!("Request failed: {}", e))?;

        if resp.status() == reqwest::StatusCode::UNAUTHORIZED {
            return Err("Authentication required. Please configure API key.".to_string());
        }

        if !resp.status().is_success() {
            let status = resp.status();
            let text = resp.text().await.unwrap_or_default();
            return Err(format!("Request failed with status {}: {}", status, text));
        }

        Ok(resp)
    }
}

#[derive(Debug, Serialize, Deserialize)]
pub struct HealthResponse {
    pub status: String,
    pub version: String,
    pub uptime_seconds: u64,
    pub services: serde_json::Value,
}

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct ServiceEntry {
    pub name: String,
    pub status: String,
    pub healthy: bool,
    pub port: Option<u16>,
    pub uptime_seconds: Option<u64>,
    pub metadata: Option<serde_json::Value>,
}

#[derive(Debug, Serialize, Deserialize)]
struct ServicesResponse {
    services: Vec<ServiceEntry>,
}

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct AgentEntry {
    pub id: String,
    pub name: String,
    #[serde(rename = "type")]
    pub agent_type: Option<String>,
    pub status: String,
    pub task_count: Option<u32>,
    pub last_active: Option<String>,
    pub description: Option<String>,
    pub capabilities: Option<Vec<String>>,
    pub config: Option<serde_json::Value>,
    pub created_at: Option<String>,
}

#[derive(Debug, Serialize, Deserialize)]
struct AgentsResponse {
    agents: Vec<AgentEntry>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct AgentRegistration {
    pub name: String,
    #[serde(rename = "type")]
    pub agent_type: String,
    pub description: Option<String>,
    pub model: Option<String>,
    pub system_prompt: Option<String>,
    pub tools: Option<Vec<String>>,
}

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct TaskEntry {
    pub id: String,
    pub agent_id: Option<String>,
    pub name: Option<String>,
    #[serde(rename = "type")]
    pub type_: Option<String>,
    pub status: String,
    pub progress: f32,
    pub created_at: String,
    pub updated_at: Option<String>,
    pub result: Option<serde_json::Value>,
    pub error: Option<String>,
}

#[derive(Debug, Serialize, Deserialize)]
struct TasksResponse {
    tasks: Vec<TaskEntry>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct TaskSubmission {
    pub agent_id: String,
    pub description: String,
    pub priority: Option<String>,
    pub parameters: Option<serde_json::Value>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct ConfigEntry {
    pub key: String,
    pub value: String,
    pub namespace: Option<String>,
    pub version: Option<u64>,
    pub source: Option<String>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct ProtocolTestResult {
    pub protocol: String,
    pub endpoint: String,
    pub success: bool,
    pub latency_ms: u64,
    pub message: String,
    pub details: Option<serde_json::Value>,
}

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct ProtocolAdapter {
    pub name: String,
    pub protocol: String,
    pub version: String,
    pub status: String,
    pub endpoint: String,
    pub capabilities: Vec<String>,
}

#[derive(Debug, Serialize, Deserialize)]
struct ProtocolAdaptersResponse {
    adapters: Vec<ProtocolAdapter>,
}
