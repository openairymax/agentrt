// AgentOS Rust SDK Task
// Version: 2.0.0
// Last updated: 2026-03-23

use serde_json::{json, Value};
use std::time::{Duration, Instant};

use crate::{AgentOSError, Client};

/// ﻛﭨﭨﮒ۰ﻝﭘﮔﮔﻛﺕ?#[derive(Debug, Clone, PartialEq, Eq)]
pub enum TaskStatus {
    Pending,
    Running,
    Completed,
    Failed,
    Cancelled,
}

impl TaskStatus {
    /// ﻛﭨﮒ­ﻝ؛۵ﻛﺕﺎﻟ۶۲ﮔﻛﭨﭨﮒ۰ﻝﭘﮔ?    pub fn from_str(s: &str) -> Option<Self> {
        match s {
            "pending" => Some(TaskStatus::Pending),
            "running" => Some(TaskStatus::Running),
            "completed" => Some(TaskStatus::Completed),
            "failed" => Some(TaskStatus::Failed),
            "cancelled" => Some(TaskStatus::Cancelled),
            _ => None,
        }
    }

    /// ﻟﺛ؛ﮔ۱ﻛﺕﭦﮒ­ﻝ؛۵ﻛﺕﺎﻟ۰۷ﻝ۳ﭦ
    pub fn as_str(&self) -> &'static str {
        match self {
            TaskStatus::Pending => "pending",
            TaskStatus::Running => "running",
            TaskStatus::Completed => "completed",
            TaskStatus::Failed => "failed",
            TaskStatus::Cancelled => "cancelled",
        }
    }
}

impl std::fmt::Display for TaskStatus {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.as_str())
    }
}

/// ﻛﭨﭨﮒ۰ﻝﭨﮔ
#[derive(Debug)]
pub struct TaskResult {
    pub id: String,
    pub status: String,
    pub output: Option<String>,
    pub error: Option<String>,
}

type Result<T> = std::result::Result<T, AgentOSError>;

/// AgentOS ﻛﭨﭨﮒ۰
#[derive(Debug, Clone)]
pub struct Task {
    client: Client,
    id: String,
}

impl Task {
    /// ﮒﮒﭨﭦﮔﺍﻝ Task ﮒﺁﺗﻟﺎ۰
    pub fn new(client: Client, id: String) -> Self {
        Task { client, id }
    }

    /// ﻟﺓﮒﻛﭨﭨﮒ۰ ID
    pub fn task_id(&self) -> &str {
        &self.id
    }

    /// ﮔ۴ﻟﺁ۱ﻛﭨﭨﮒ۰ﻝﭘﮔ?    pub async fn query(&self) -> Result<TaskStatus> {
        let path = format!("/api/v1/tasks/{}", self.id);
        let response = self.client.request("GET", &path, None).await?;

        let status = response
            .get("status")
            .and_then(|v| v.as_str())
            .ok_or_else(|| {
                AgentOSError::InvalidResponse("Missing status".to_string())
            })?;

        TaskStatus::from_str(status).ok_or_else(|| {
            AgentOSError::InvalidResponse(format!("Unknown status: {}", status))
        })
    }

    /// ﻝ­ﮒﺝﻛﭨﭨﮒ۰ﮒ؟ﮔ
    pub async fn wait(&self, timeout: Option<Duration>) -> Result<TaskResult> {
        let start = Instant::now();

        loop {
            let status = self.query().await?;

            match status {
                TaskStatus::Completed
                | TaskStatus::Failed
                | TaskStatus::Cancelled => {
                    let path = format!("/api/v1/tasks/{}", self.id);
                    let response = self.client.request("GET", &path, None).await?;

                    let output = response
                        .get("output")
                        .and_then(|v| v.as_str())
                        .map(|s| s.to_string());
                    let error = response
                        .get("error")
                        .and_then(|v| v.as_str())
                        .map(|s| s.to_string());

                    return Ok(TaskResult {
                        id: self.id.clone(),
                        status: status.as_str().to_string(),
                        output,
                        error,
                    });
                }
                _ => {
                    if let Some(timeout) = timeout {
                        if start.elapsed() > timeout {
                            return Err(AgentOSError::Timeout(
                                "Task timed out".to_string(),
                            ));
                        }
                    }

                    tokio::time::sleep(Duration::from_millis(500)).await;
                }
            }
        }
    }

    /// ﮒﮔﭘﻛﭨﭨﮒ۰
    pub async fn cancel(&self) -> Result<bool> {
        let path = format!("/api/v1/tasks/{}/cancel", self.id);
        let response = self.client.request("POST", &path, None).await?;

        let success = response
            .get("success")
            .and_then(|v| v.as_bool())
            .ok_or_else(|| {
                AgentOSError::InvalidResponse("Missing success".to_string())
            })?;

        Ok(success)
    }
}
