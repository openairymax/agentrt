// AgentOS Rust SDK - 任务管理器实现
// Version: 3.0.0
// Last updated: 2026-03-24
//
// 提供任务的提交、查询、等待、取消、列表等生命周期管理功能。
// 对应 Go SDK: modules/task/manager.go

use std::collections::HashMap;
use std::sync::Arc;
use std::time::{Duration, Instant};

use crate::client::APIClient;
use crate::error::{AgentOSError, CODE_MISSING_PARAMETER, CODE_TASK_TIMEOUT, CODE_INVALID_PARAMETER};
use crate::types::{Task, TaskResult, TaskStatus, ListOptions, APIResponse};
use crate::utils::{extract_data_map, get_string, get_i64, get_map, parse_time_from_map, get_interface_slice, build_url};

/// TaskManager 管理任务完整生命周期
pub struct TaskManager {
    api: Arc<dyn APIClient>,
}

impl TaskManager {
    /// 创建新的任务管理器实例
    ///
    /// # 参数
    /// - `api`: API 客户端
    pub fn new(api: Arc<dyn APIClient>) -> Self {
        TaskManager { api }
    }

    /// 提交新的执行任务
    ///
    /// # 参数
    /// - `description`: 任务描述
    ///
    /// # 返回
    /// 返回创建的任务对象
    pub async fn submit(&self, description: &str) -> Result<Task, AgentOSError> {
        if description.is_empty() {
            return Err(AgentOSError::with_code(CODE_MISSING_PARAMETER, "任务描述不能为空"));
        }

        let body = serde_json::json!({ "description": description });
        let resp = self.api.post("/api/v1/tasks", Some(&body), None).await?;

        let data = extract_data_map(&resp)
            .ok_or_else(|| AgentOSError::invalid_response("任务创建响应格式异常"))?;

        Ok(self.parse_task_from_map(data))
    }

    /// 使用扩展选项提交任务
    ///
    /// # 参数
    /// - `description`: 任务描述
    /// - `priority`: 任务优先级
    /// - `metadata`: 任务元数据
    ///
    /// # 返回
    /// 返回创建的任务对象
    pub async fn submit_with_options(
        &self,
        description: &str,
        priority: i32,
        metadata: Option<HashMap<String, serde_json::Value>>,
    ) -> Result<Task, AgentOSError> {
        if description.is_empty() {
            return Err(AgentOSError::with_code(CODE_MISSING_PARAMETER, "任务描述不能为空"));
        }

        let mut body = serde_json::json!({
            "description": description,
            "priority": priority
        });

        if let Some(meta) = metadata {
            body["metadata"] = serde_json::to_value(meta).map_err(|e| {
                AgentOSError::parse_error(&format!("序列化元数据失败: {}", e))
            })?;
        }

        let resp = self.api.post("/api/v1/tasks", Some(&body), None).await?;

        let data = extract_data_map(&resp)
            .ok_or_else(|| AgentOSError::invalid_response("任务创建响应格式异常"))?;

        Ok(self.parse_task_from_map(data))
    }

    /// 获取指定任务的详细信息
    ///
    /// # 参数
    /// - `task_id`: 任务 ID
    ///
    /// # 返回
    /// 返回任务对象
    pub async fn get(&self, task_id: &str) -> Result<Task, AgentOSError> {
        if task_id.is_empty() {
            return Err(AgentOSError::with_code(CODE_MISSING_PARAMETER, "任务ID不能为空"));
        }

        let path = format!("/api/v1/tasks/{}", task_id);
        let resp = self.api.get(&path, None).await?;

        let data = extract_data_map(&resp)
            .ok_or_else(|| AgentOSError::invalid_response("任务详情响应格式异常"))?;

        Ok(self.parse_task_from_map(data))
    }

    /// 查询任务的当前状态
    ///
    /// # 参数
    /// - `task_id`: 任务 ID
    ///
    /// # 返回
    /// 返回任务状态
    pub async fn query(&self, task_id: &str) -> Result<TaskStatus, AgentOSError> {
        let task = self.get(task_id).await?;
        Ok(task.status)
    }

    /// 阻塞等待任务到达终态，支持超时控制
    ///
    /// # 参数
    /// - `task_id`: 任务 ID
    /// - `timeout`: 超时时间
    ///
    /// # 返回
    /// 返回任务结果
    pub async fn wait(&self, task_id: &str, timeout: Duration) -> Result<TaskResult, AgentOSError> {
        let start = Instant::now();

        loop {
            let status = self.query(task_id).await?;

            if status.is_terminal() {
                let task = self.get(task_id).await?;
                let end = Instant::now();

                return Ok(TaskResult {
                    id: task.id,
                    status: task.status,
                    output: task.output,
                    error: task.error,
                    start_time: task.created_at,
                    end_time: task.updated_at,
                    duration: end.duration_since(start).as_secs_f64(),
                });
            }

            if start.elapsed() > timeout {
                return Err(AgentOSError::with_code(
                    CODE_TASK_TIMEOUT,
                    &format!("任务 {} 超时", task_id),
                ));
            }

            tokio::time::sleep(Duration::from_millis(500)).await;
        }
    }

    /// 取消正在执行的任务
    ///
    /// # 参数
    /// - `task_id`: 任务 ID
    pub async fn cancel(&self, task_id: &str) -> Result<(), AgentOSError> {
        if task_id.is_empty() {
            return Err(AgentOSError::with_code(CODE_MISSING_PARAMETER, "任务ID不能为空"));
        }

        let path = format!("/api/v1/tasks/{}/cancel", task_id);
        self.api.post(&path, None, None).await?;
        Ok(())
    }

    /// 列出任务，支持分页和过滤
    ///
    /// # 参数
    /// - `opts`: 列表查询选项
    ///
    /// # 返回
    /// 返回任务列表
    pub async fn list(&self, opts: Option<&ListOptions>) -> Result<Vec<Task>, AgentOSError> {
        let path = if let Some(options) = opts {
            build_url("/api/v1/tasks", options.to_query_params())
        } else {
            "/api/v1/tasks".to_string()
        };

        let resp = self.api.get(&path, None).await?;
        self.parse_task_list(&resp)
    }

    /// 删除指定任务
    ///
    /// # 参数
    /// - `task_id`: 任务 ID
    pub async fn delete(&self, task_id: &str) -> Result<(), AgentOSError> {
        if task_id.is_empty() {
            return Err(AgentOSError::with_code(CODE_MISSING_PARAMETER, "任务ID不能为空"));
        }

        let path = format!("/api/v1/tasks/{}", task_id);
        self.api.delete(&path, None).await?;
        Ok(())
    }

    /// 获取已完成任务的结果
    ///
    /// # 参数
    /// - `task_id`: 任务 ID
    ///
    /// # 返回
    /// 返回任务结果
    pub async fn get_result(&self, task_id: &str) -> Result<TaskResult, AgentOSError> {
        let task = self.get(task_id).await?;

        if !task.status.is_terminal() {
            return Err(AgentOSError::with_code(
                CODE_INVALID_PARAMETER,
                "任务尚未完成",
            ));
        }

        Ok(TaskResult {
            id: task.id,
            status: task.status,
            output: task.output,
            error: task.error,
            start_time: task.created_at,
            end_time: task.updated_at,
            duration: 0.0,
        })
    }

    /// 批量提交多个任务
    ///
    /// # 参数
    /// - `descriptions`: 任务描述列表
    ///
    /// # 返回
    /// 返回创建的任务列表
    pub async fn batch_submit(&self, descriptions: &[&str]) -> Result<Vec<Task>, AgentOSError> {
        let mut tasks = Vec::with_capacity(descriptions.len());

        for desc in descriptions {
            let task = self.submit(desc).await?;
            tasks.push(task);
        }

        Ok(tasks)
    }

    /// 获取任务总数
    ///
    /// # 返回
    /// 返回任务总数
    pub async fn count(&self) -> Result<i64, AgentOSError> {
        let resp = self.api.get("/api/v1/tasks/count", None).await?;

        let data = extract_data_map(&resp)
            .ok_or_else(|| AgentOSError::invalid_response("任务计数响应格式异常"))?;

        Ok(get_i64(&data, "count"))
    }

    /// 从 map 解析 Task 结构
    fn parse_task_from_map(&self, data: HashMap<String, serde_json::Value>) -> Task {
        Task {
            id: get_string(&data, "task_id"),
            description: get_string(&data, "description"),
            status: TaskStatus::from_str(&get_string(&data, "status"))
                .unwrap_or(TaskStatus::Pending),
            priority: get_i64(&data, "priority") as i32,
            output: {
                let output = get_string(&data, "output");
                if output.is_empty() { None } else { Some(output) }
            },
            error: {
                let error = get_string(&data, "error");
                if error.is_empty() { None } else { Some(error) }
            },
            metadata: get_map(&data, "metadata"),
            created_at: get_string(&data, "created_at"),
            updated_at: get_string(&data, "updated_at"),
        }
    }

    /// 从 APIResponse 解析 Task 列表
    fn parse_task_list(&self, resp: &APIResponse) -> Result<Vec<Task>, AgentOSError> {
        let data = extract_data_map(resp)
            .ok_or_else(|| AgentOSError::invalid_response("任务列表响应格式异常"))?;

        let items = get_interface_slice(&data, "tasks");
        let mut tasks = Vec::with_capacity(items.len());

        for item in items {
            if let Some(obj) = item.as_object() {
                let data: HashMap<String, serde_json::Value> = obj
                    .iter()
                    .map(|(k, v)| (k.clone(), v.clone()))
                    .collect();
                tasks.push(self.parse_task_from_map(data));
            }
        }

        Ok(tasks)
    }
}
