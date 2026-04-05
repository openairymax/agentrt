// AgentOS Rust SDK Memory
// Version: 1.0.0.5
// Last updated: 2026-03-21

use serde_json::Value;
use std::collections::HashMap;

use crate::AgentOSError;

/// Memory
type Result<T> = std::result::Result<T, AgentOSError>;

/// Memory
type JsonObject = serde_json::Map<String, Value>;

/// Memory
#[derive(Debug, Clone)]
pub struct Memory {
    pub id: String,
    pub content: String,
    pub created_at: String,
    pub metadata: HashMap<String, Value>,
}

impl Memory {
    /// Create a new Memory object
    pub fn new(id: String, content: String, created_at: String, metadata: HashMap<String, Value>) -> Self {
        Memory {
            id,
            content,
            created_at,
            metadata,
        }
    }
    
    /// Create a Memory object from a JSON object
    pub fn from_json(json: &JsonObject) -> Result<Self> {
        let id = json.get("memory_id")
            .and_then(|v| v.as_str())
            .ok_or_else(|| AgentOSError::InvalidResponse("Missing memory_id".to_string()))?;
        
        let content = json.get("content")
            .and_then(|v| v.as_str())
            .ok_or_else(|| AgentOSError::InvalidResponse("Missing content".to_string()))?;
        
        let created_at = json.get("created_at")
            .and_then(|v| v.as_str())
            .ok_or_else(|| AgentOSError::InvalidResponse("Missing created_at".to_string()))?;
        
        let metadata = json.get("metadata")
            .and_then(|v| v.as_object())
            .map(|obj| {
                obj.iter()
                    .map(|(k, v)| (k.clone(), v.clone()))
                    .collect()
            })
            .unwrap_or_default();
        
        Ok(Memory {
            id: id.to_string(),
            content: content.to_string(),
            created_at: created_at.to_string(),
            metadata,
        })
    }
}
