// AgentOS Rust SDK Session
// Version: 1.0.0.5
// Last updated: 2026-03-21

use serde_json::{json, Value};

use crate::{AgentOSError, Client};

/// Session
type Result<T> = std::result::Result<T, AgentOSError>;

/// Session
type JsonObject = serde_json::Map<String, Value>;

/// Session
#[derive(Debug, Clone)]
pub struct Session {
    client: Client,
    id: String,
}

impl Session {
    /// Create a new Session object
    pub fn new(client: Client, id: String) -> Self {
        Session {
            client,
            id,
        }
    }
    
    /// Set a context value for the session
    pub async fn set_context(&self, key: &str, value: Value) -> Result<bool> {
        let path = format!("/api/sessions/{}/context", self.id);
        let data = json!({"key": key, "value": value});
        let response = self.client.request("POST", &path, Some(&data)).await?;
        
        let success = response.get("success")
            .and_then(|v| v.as_bool())
            .ok_or_else(|| AgentOSError::InvalidResponse("Missing success".to_string()))?;
        
        Ok(success)
    }
    
    /// Get a context value from the session
    pub async fn get_context(&self, key: &str) -> Result<Value> {
        let path = format!("/api/sessions/{}/context/{}", self.id, key);
        let response = self.client.request("GET", &path, None).await?;
        
        let value = response.get("value")
            .ok_or_else(|| AgentOSError::InvalidResponse("Missing value".to_string()))?;
        
        Ok(value.clone())
    }
    
    /// Close the session
    pub async fn close(&self) -> Result<bool> {
        let path = format!("/api/sessions/{}", self.id);
        let response = self.client.request("DELETE", &path, None).await?;
        
        let success = response.get("success")
            .and_then(|v| v.as_bool())
            .ok_or_else(|| AgentOSError::InvalidResponse("Missing success".to_string()))?;
        
        Ok(success)
    }
}
