// AgentOS Rust SDK Client
// Version: 1.0.0.5
// Last updated: 2026-03-21

use reqwest::{Client as ReqwestClient, RequestBuilder};
use serde_json::{json, Value};
use std::collections::HashMap;
use std::time::Duration;

use crate::{AgentOSError, Memory, Session, Skill, Task};

/// AgentOS client
type Result<T> = std::result::Result<T, AgentOSError>;

/// AgentOS client
type JsonValue = serde_json::Value;

/// AgentOS client
type JsonObject = serde_json::Map<String, JsonValue>;

/// AgentOS client
#[derive(Debug, Clone)]
pub struct Client {
    endpoint: String,
    client: ReqwestClient,
}

impl Client {
    /// Create a new AgentOS client
    pub fn new(endpoint: &str) -> Result<Self> {
        let endpoint = if endpoint.is_empty() {
            "http://localhost:18789".to_string()
        } else {
            endpoint.trim_end_matches('/').to_string()
        };
        
        let client = ReqwestClient::builder()
            .timeout(Duration::from_secs(30))
            .build()?;
        
        Ok(Client {
            endpoint,
            client,
        })
    }
    
    /// Create a new AgentOS client with a custom timeout
    pub fn new_with_timeout(endpoint: &str, timeout: Duration) -> Result<Self> {
        let endpoint = if endpoint.is_empty() {
            "http://localhost:18789".to_string()
        } else {
            endpoint.trim_end_matches('/').to_string()
        };
        
        let client = ReqwestClient::builder()
            .timeout(timeout)
            .build()?;
        
        Ok(Client {
            endpoint,
            client,
        })
    }
    
    /// Make an HTTP request to the AgentOS server
    async fn request(&self, method: &str, path: &str, data: Option<&JsonValue>) -> Result<JsonObject> {
        let url = format!("{}{}", self.endpoint, path);
        
        let mut builder: RequestBuilder = match method {
            "GET" => self.client.get(&url),
            "POST" => self.client.post(&url),
            "PUT" => self.client.put(&url),
            "DELETE" => self.client.delete(&url),
            _ => return Err(AgentOSError::Other(format!("Unsupported method: {}", method))),
        };
        
        if let Some(data) = data {
            builder = builder.json(data);
        }
        
        let response = builder.send().await?;
        let status = response.status();
        
        if !status.is_success() {
            return Err(AgentOSError::Http(format!("Server returned error: {}", status)));
        }
        
        let body = response.json::<JsonObject>().await?;
        Ok(body)
    }
    
    /// Submit a task to the AgentOS system
    pub async fn submit_task(&self, description: &str) -> Result<Task> {
        let data = json!({"description": description});
        let response = self.request("POST", "/api/tasks", Some(&data)).await?;
        
        let task_id = response.get("task_id")
            .and_then(|v| v.as_str())
            .ok_or_else(|| AgentOSError::InvalidResponse("Missing task_id".to_string()))?;
        
        Ok(Task::new(self.clone(), task_id.to_string()))
    }
    
    /// Write a memory to the AgentOS system
    pub async fn write_memory(&self, content: &str, metadata: Option<HashMap<String, Value>>) -> Result<String> {
        let metadata = metadata.unwrap_or_default();
        let data = json!({"content": content, "metadata": metadata});
        let response = self.request("POST", "/api/memories", Some(&data)).await?;
        
        let memory_id = response.get("memory_id")
            .and_then(|v| v.as_str())
            .ok_or_else(|| AgentOSError::InvalidResponse("Missing memory_id".to_string()))?;
        
        Ok(memory_id.to_string())
    }
    
    /// Search memories in the AgentOS system
    pub async fn search_memory(&self, query: &str, top_k: u32) -> Result<Vec<Memory>> {
        let path = format!("/api/memories/search?query={}&top_k={}", query, top_k);
        let response = self.request("GET", &path, None).await?;
        
        let memories_data = response.get("memories")
            .and_then(|v| v.as_array())
            .ok_or_else(|| AgentOSError::InvalidResponse("Missing memories".to_string()))?;
        
        let mut memories = Vec::new();
        for mem_data in memories_data {
            if let Some(mem_map) = mem_data.as_object() {
                let memory = Memory::from_json(mem_map)?;
                memories.push(memory);
            }
        }
        
        Ok(memories)
    }
    
    /// Get a memory by ID
    pub async fn get_memory(&self, memory_id: &str) -> Result<Memory> {
        let path = format!("/api/memories/{}", memory_id);
        let response = self.request("GET", &path, None).await?;
        
        let memory = Memory::from_json(&response)?;
        Ok(memory)
    }
    
    /// Delete a memory by ID
    pub async fn delete_memory(&self, memory_id: &str) -> Result<bool> {
        let path = format!("/api/memories/{}", memory_id);
        let response = self.request("DELETE", &path, None).await?;
        
        let success = response.get("success")
            .and_then(|v| v.as_bool())
            .ok_or_else(|| AgentOSError::InvalidResponse("Missing success".to_string()))?;
        
        Ok(success)
    }
    
    /// Create a new session
    pub async fn create_session(&self) -> Result<Session> {
        let response = self.request("POST", "/api/sessions", None).await?;
        
        let session_id = response.get("session_id")
            .and_then(|v| v.as_str())
            .ok_or_else(|| AgentOSError::InvalidResponse("Missing session_id".to_string()))?;
        
        Ok(Session::new(self.clone(), session_id.to_string()))
    }
    
    /// Load a skill by name
    pub async fn load_skill(&self, skill_name: &str) -> Result<Skill> {
        Ok(Skill::new(self.clone(), skill_name.to_string()))
    }
}
