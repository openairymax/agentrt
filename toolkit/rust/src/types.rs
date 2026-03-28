// AgentOS Rust SDK Types
// Version: 1.0.0.5
// Last updated: 2026-03-21

use serde::{Deserialize, Serialize};
use std::collections::HashMap;

/// Memory information
#[derive(Debug, Serialize, Deserialize)]
pub struct MemoryInfo {
    /// Memory ID
    pub memory_id: String,
    /// Memory content
    pub content: String,
    /// Creation timestamp
    pub created_at: String,
    /// Metadata
    pub metadata: HashMap<String, serde_json::Value>,
}
