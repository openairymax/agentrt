// AgentOS Rust SDK Task
// Version: 1.0.0.5
// Last updated: 2026-03-21

use serde_json::{json, Value};
use std::time::{Duration, Instant};

use crate::{AgentOSError, Client};

/// Task status
enum TaskStatus {
    Pending,
    Running,
    Completed,
    Failed,
    Cancelled,
}

impl TaskStatus {
    fn from_str(s: &str) -> Option<Self> {
        match s {
            "pending" => Some(TaskStatus::Pending),
            "running" => Some(TaskStatus::Running),
            "completed" => Some(TaskStatus::Completed),
            "failed" => Some(TaskStatus::Failed),
            "cancelled" => Some(TaskStatus::Cancelled),
            _ => None,
        }
    }
}

/// Task result
#[derive(Debug)]
pub struct TaskResult {
    pub id: String,
    pub status: String,
    pub output: Option<String>,
    pub error: Option<String>,
}

/// Task
type Result<T> = std::result::Result<T, AgentOSError>;

/// Task
type JsonObject = serde_json::Map<String, Value>;

/// Task
#[derive(Debug, Clone)]
pub struct Task {
    client: Client,
    id: String,
}

impl Task {
    /// Create a new Task object
    pub fn new(client: Client, id: String) -> Self {
        Task {
            client,
            id,
        }
    }
    
    /// Query the task status
    pub async fn query(&self) -> Result<String> {
        let path = format!("/api/tasks/{}", self.id);
        let response = self.client.request("GET", &path, None).await?;
        
        let status = response.get("status")
            .and_then(|v| v.as_str())
            .ok_or_else(|| AgentOSError::InvalidResponse("Missing status".to_string()))?;
        
        Ok(status.to_string())
    }
    
    /// Wait for the task to complete
    pub async fn wait(&self, timeout: Option<Duration>) -> Result<TaskResult> {
        let start = Instant::now();
        
        loop {
            let status_str = self.query().await?;
            
            match TaskStatus::from_str(&status_str) {
                Some(TaskStatus::Completed) | Some(TaskStatus::Failed) | Some(TaskStatus::Cancelled) => {
                    let path = format!("/api/tasks/{}", self.id);
                    let response = self.client.request("GET", &path, None).await?;
                    
                    let output = response.get("output").and_then(|v| v.as_str()).map(|s| s.to_string());
                    let error = response.get("error").and_then(|v| v.as_str()).map(|s| s.to_string());
                    
                    return Ok(TaskResult {
                        id: self.id.clone(),
                        status: status_str,
                        output,
                        error,
                    });
                }
                _ => {
                    if let Some(timeout) = timeout {
                        if start.elapsed() > timeout {
                            return Err(AgentOSError::Timeout("Task timed out".to_string()));
                        }
                    }
                    
                    tokio::time::sleep(Duration::from_millis(500)).await;
                }
            }
        }
    }
    
    /// Cancel the task
    pub async fn cancel(&self) -> Result<bool> {
        let path = format!("/api/tasks/{}/cancel", self.id);
        let response = self.client.request("POST", &path, None).await?;
        
        let success = response.get("success")
            .and_then(|v| v.as_bool())
            .ok_or_else(|| AgentOSError::InvalidResponse("Missing success".to_string()))?;
        
        Ok(success)
    }
}
