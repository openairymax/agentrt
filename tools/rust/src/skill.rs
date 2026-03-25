// AgentOS Rust SDK Skill
// Version: 1.0.0.5
// Last updated: 2026-03-21

use serde_json::{json, Value};
use std::collections::HashMap;

use crate::{AgentOSError, Client};

/// Skill result
#[derive(Debug)]
pub struct SkillResult {
    pub success: bool,
    pub output: Option<Value>,
    pub error: Option<String>,
}

/// Skill info
#[derive(Debug)]
pub struct SkillInfo {
    pub name: String,
    pub description: String,
    pub version: String,
    pub parameters: HashMap<String, Value>,
}

/// Skill
type Result<T> = std::result::Result<T, AgentOSError>;

/// Skill
type JsonObject = serde_json::Map<String, Value>;

/// Skill
#[derive(Debug, Clone)]
pub struct Skill {
    client: Client,
    name: String,
}

impl Skill {
    /// Create a new Skill object
    pub fn new(client: Client, name: String) -> Self {
        Skill {
            client,
            name,
        }
    }
    
    /// Execute the skill with the given parameters
    pub async fn execute(&self, parameters: Option<HashMap<String, Value>>) -> Result<SkillResult> {
        let path = format!("/api/v1/skills/{}/execute", self.name);
        let parameters = parameters.unwrap_or_default();
        let data = json!({"parameters": parameters});
        let response = self.client.request("POST", &path, Some(&data)).await?;
        
        let success = response.get("success")
            .and_then(|v| v.as_bool())
            .ok_or_else(|| AgentOSError::InvalidResponse("Missing success".to_string()))?;
        
        let output = response.get("output").cloned();
        let error = response.get("error").and_then(|v| v.as_str()).map(|s| s.to_string());
        
        Ok(SkillResult {
            success,
            output,
            error,
        })
    }
    
    /// Get information about the skill
    pub async fn get_info(&self) -> Result<SkillInfo> {
        let path = format!("/api/v1/skills/{}", self.name);
        let response = self.client.request("GET", &path, None).await?;
        
        let description = response.get("description")
            .and_then(|v| v.as_str())
            .unwrap_or("")
            .to_string();
        
        let version = response.get("version")
            .and_then(|v| v.as_str())
            .unwrap_or("")
            .to_string();
        
        let parameters = response.get("parameters")
            .and_then(|v| v.as_object())
            .map(|obj| {
                obj.iter()
                    .map(|(k, v)| (k.clone(), v.clone()))
                    .collect()
            })
            .unwrap_or_default();
        
        Ok(SkillInfo {
            name: self.name.clone(),
            description,
            version,
            parameters,
        })
    }
}
