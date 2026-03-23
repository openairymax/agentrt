// AgentOS Rust SDK Client
// Version: 2.0.0
// Last updated: 2026-03-23

use reqwest::{Client as ReqwestClient, RequestBuilder};
use serde_json::{json, Value};
use std::collections::HashMap;
use std::time::Duration;

use crate::{AgentOSError, Memory, Session, Skill, Task};

type Result<T> = std::result::Result<T, AgentOSError>;
type JsonValue = serde_json::Value;

/// AgentOS 客户端
#[derive(Debug, Clone)]
pub struct Client {
    endpoint: String,
    client: ReqwestClient,
    api_key: Option<String>,
}

impl Client {
    /// 创建新的 AgentOS 客户端（默认超时 30s）
    pub fn new(endpoint: &str) -> Result<Self> {
        Self::new_with_options(endpoint, None, Duration::from_secs(30))
    }

    /// 创建带自定义超时的 AgentOS 客户端
    pub fn new_with_timeout(endpoint: &str, timeout: Duration) -> Result<Self> {
        Self::new_with_options(endpoint, None, timeout)
    }

    /// 创建带 API Key 的 AgentOS 客户端
    pub fn new_with_api_key(endpoint: &str, api_key: &str) -> Result<Self> {
        Self::new_with_options(endpoint, Some(api_key), Duration::from_secs(30))
    }

    /// 创建带完整配置的 AgentOS 客户端
    pub fn new_with_options(endpoint: &str, api_key: Option<&str>, timeout: Duration) -> Result<Self> {
        let endpoint = if endpoint.is_empty() {
            "http://localhost:18789".to_string()
        } else {
            let trimmed = endpoint.trim_end_matches('/').to_string();
            if !trimmed.starts_with("http://") && !trimmed.starts_with("https://") {
                return Err(AgentOSError::Config(
                    "Endpoint must start with http:// or https://".to_string(),
                ));
            }
            trimmed
        };

        let client = ReqwestClient::builder()
            .timeout(timeout)
            .build()?;

        Ok(Client { 
            endpoint, 
            client,
            api_key: api_key.map(|s| s.to_string()),
        })
    }

    /// 获取客户端端点地址
    pub fn endpoint(&self) -> &str {
        &self.endpoint
    }

    /// 获取 API Key
    pub fn api_key(&self) -> Option<&str> {
        self.api_key.as_deref()
    }

    /// 向 AgentOS 服务端发送 HTTP 请求
    pub(crate) async fn request(
        &self,
        method: &str,
        path: &str,
        data: Option<&JsonValue>,
    ) -> Result<serde_json::Map<String, JsonValue>> {
        let url = format!("{}{}", self.endpoint, path);

        let mut builder: RequestBuilder = match method {
            "GET" => self.client.get(&url),
            "POST" => self.client.post(&url),
            "PUT" => self.client.put(&url),
            "DELETE" => self.client.delete(&url),
            _ => {
                return Err(AgentOSError::Other(format!(
                    "Unsupported method: {}",
                    method
                )))
            }
        };

        // 添加 API Key 认证头
        if let Some(ref api_key) = self.api_key {
            builder = builder.bearer_auth(api_key);
        }

        if let Some(data) = data {
            builder = builder.json(data);
        }

        let response = builder.send().await?;
        let status = response.status();

        if !status.is_success() {
            return Err(AgentOSError::Http(format!(
                "Server returned error: {}",
                status
            )));
        }

        let body = response.json::<serde_json::Map<String, JsonValue>>().await?;
        Ok(body)
    }

    /// 提交任务�?AgentOS 系统
    pub async fn submit_task(&self, description: &str) -> Result<Task> {
        let data = json!({"description": description});
        let response = self.request("POST", "/api/v1/tasks", Some(&data)).await?;

        let task_id = response
            .get("task_id")
            .and_then(|v| v.as_str())
            .ok_or_else(|| {
                AgentOSError::InvalidResponse("Missing task_id".to_string())
            })?;

        Ok(Task::new(self.clone(), task_id.to_string()))
    }

    /// 写入记忆�?AgentOS 系统
    pub async fn write_memory(
        &self,
        content: &str,
        metadata: Option<HashMap<String, Value>>,
    ) -> Result<String> {
        let metadata = metadata.unwrap_or_default();
        let data = json!({"content": content, "metadata": metadata});
        let response = self.request("POST", "/api/v1/memories", Some(&data)).await?;

        let memory_id = response
            .get("memory_id")
            .and_then(|v| v.as_str())
            .ok_or_else(|| {
                AgentOSError::InvalidResponse("Missing memory_id".to_string())
            })?;

        Ok(memory_id.to_string())
    }

    /// 搜索记忆（修�?URL 注入：使�?query 参数而非字符串拼接）
    pub async fn search_memory(&self, query: &str, top_k: u32) -> Result<Vec<Memory>> {
        let mut params = HashMap::new();
        params.insert("query", query);
        params.insert("top_k", &top_k.to_string());

        let client = self
            .client
            .get(format!("{}/api/v1/memories/search", self.endpoint))
            .query(&params);

        let response = client.send().await?;
        if !response.status().is_success() {
            return Err(AgentOSError::Http(format!(
                "Server returned error: {}",
                response.status()
            )));
        }

        let body: serde_json::Map<String, JsonValue> = response.json().await?;
        let memories_data = body
            .get("memories")
            .and_then(|v| v.as_array())
            .ok_or_else(|| {
                AgentOSError::InvalidResponse("Missing memories".to_string())
            })?;

        let mut memories = Vec::new();
        for mem_data in memories_data {
            if let Some(mem_map) = mem_data.as_object() {
                let memory = Memory::from_json(mem_map)?;
                memories.push(memory);
            }
        }

        Ok(memories)
    }

    /// 根据 ID 获取记忆
    pub async fn get_memory(&self, memory_id: &str) -> Result<Memory> {
        let path = format!("/api/v1/memories/{}", memory_id);
        let response = self.request("GET", &path, None).await?;

        let memory = Memory::from_json(&response)?;
        Ok(memory)
    }

    /// 根据 ID 删除记忆
    pub async fn delete_memory(&self, memory_id: &str) -> Result<bool> {
        let path = format!("/api/v1/memories/{}", memory_id);
        let response = self.request("DELETE", &path, None).await?;

        let success = response
            .get("success")
            .and_then(|v| v.as_bool())
            .ok_or_else(|| {
                AgentOSError::InvalidResponse("Missing success".to_string())
            })?;

        Ok(success)
    }

    /// 创建新会�?    pub async fn create_session(&self) -> Result<Session> {
        let response = self.request("POST", "/api/v1/sessions", None).await?;

        let session_id = response
            .get("session_id")
            .and_then(|v| v.as_str())
            .ok_or_else(|| {
                AgentOSError::InvalidResponse("Missing session_id".to_string())
            })?;

        Ok(Session::new(self.clone(), session_id.to_string()))
    }

    /// 加载技�?    pub async fn load_skill(&self, skill_name: &str) -> Result<Skill> {
        Ok(Skill::new(self.clone(), skill_name.to_string()))
    }

    /// 健康检�?    pub async fn health(&self) -> Result<bool> {
        match self.request("GET", "/api/v1/health", None).await {
            Ok(_) => Ok(true),
            Err(_) => Ok(false),
        }
    }
}
