// AgentOS Rust SDK Task
// Version: 2.0.0
// Last updated: 2026-03-23

use std::time::{Duration, Instant};

use crate::{AgentOSError, Client, client::APIClient};

/// 任务状态枚举
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum TaskStatus {
    Pending,
    Running,
    Completed,
    Failed,
    Cancelled,
}

impl TaskStatus {
    /// 从字符串解析任务状态
    pub fn from_str(s: &str) -> Option<Self> {
        match s {
            "pending" => Some(TaskStatus::Pending),
            "running" => Some(TaskStatus::Running),
            "completed" => Some(TaskStatus::Completed),
            "failed" => Some(TaskStatus::Failed),
            "cancelled" => Some(TaskStatus::Cancelled),
            _ => None,
        }
    }

    /// 转换为字符串表示
    pub fn as_str(&self) -> &'static str {
        match self {
            TaskStatus::Pending => "pending",
            TaskStatus::Running => "running",
            TaskStatus::Completed => "completed",
            TaskStatus::Failed => "failed",
            TaskStatus::Cancelled => "cancelled",
        }
    }

    /// 是否为终态
    pub fn is_terminal(&self) -> bool {
        matches!(self, TaskStatus::Completed | TaskStatus::Failed | TaskStatus::Cancelled)
    }
}

impl std::fmt::Display for TaskStatus {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.as_str())
    }
}

/// 任务结果
#[derive(Debug)]
pub struct TaskResult {
    pub id: String,
    pub status: String,
    pub output: Option<String>,
    pub error: Option<String>,
}

type Result<T> = std::result::Result<T, AgentOSError>;

/// AgentOS 任务
#[derive(Debug, Clone)]
pub struct Task {
    client: Client,
    id: String,
}

impl Task {
    /// 创建新的 Task 对象
    pub fn new(client: Client, id: String) -> Self {
        Task { client, id }
    }

    /// 获取任务 ID
    pub fn task_id(&self) -> &str {
        &self.id
    }

    /// 查询任务状态
    pub async fn query(&self) -> Result<TaskStatus> {
        let path = format!("/api/v1/tasks/{}", self.id);
        let response = self.client.get(&path, None).await?;

        let status = response.data
            .get("status")
            .and_then(|v| v.as_str())
            .ok_or_else(|| {
                AgentOSError::InvalidResponse("Missing status".to_string())
            })?;

        TaskStatus::from_str(status).ok_or_else(|| {
            AgentOSError::InvalidResponse(format!("Unknown status: {}", status))
        })
    }

    /// 等待任务完成
    pub async fn wait(&self, timeout: Option<Duration>) -> Result<TaskResult> {
        let start = Instant::now();

        loop {
            let status = self.query().await?;

            match status {
                TaskStatus::Completed
                | TaskStatus::Failed
                | TaskStatus::Cancelled => {
                    let path = format!("/api/v1/tasks/{}", self.id);
                    let response = self.client.get(&path, None).await?;

                    let output = response.data
                        .get("output")
                        .and_then(|v| v.as_str())
                        .map(|s| s.to_string());
                    let error = response.data
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

    /// 取消任务
    pub async fn cancel(&self) -> Result<bool> {
        let path = format!("/api/v1/tasks/{}/cancel", self.id);
        let response = self.client.post(&path, None, None).await?;

        let success = response.data
            .get("success")
            .and_then(|v| v.as_bool())
            .ok_or_else(|| {
                AgentOSError::InvalidResponse("Missing success".to_string())
            })?;

        Ok(success)
    }
}
