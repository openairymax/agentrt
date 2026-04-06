// AgentOS Rust SDK - 宏定义
// Version: 3.0.0
// Last updated: 2026-04-05
//
// 提供宏来减少 Manager 模块的重复代码。
// 使用 Rust 宏系统实现代码生成。

/// 实现基础 Manager 结构
#[macro_export]
macro_rules! impl_base_manager {
    ($name:ident, $resource:ident, $resource_type:expr) => {
        pub struct $name {
            api: Arc<dyn APIClient>,
        }

        impl $name {
            /// 创建新的管理器实例
            pub fn new(api: Arc<dyn APIClient>) -> Self {
                Self { api }
            }

            /// 获取 API 客户端
            pub fn get_api(&self) -> Arc<dyn APIClient> {
                self.api.clone()
            }

            /// 执行 GET 请求
            async fn execute_get(&self, path: &str, error_msg: &str) -> Result<$resource, AgentOSError> {
                let resp = self.api.get(path, None, None).await?;
                let data = extract_data_map(&resp)
                    .ok_or_else(|| AgentOSError::invalid_response(error_msg))?;
                self.parse_resource_from_map(&data, error_msg)
            }

            /// 执行 POST 请求
            async fn execute_post(
                &self,
                path: &str,
                body: serde_json::Value,
                error_msg: &str,
            ) -> Result<$resource, AgentOSError> {
                let resp = self.api.post(path, Some(&body), None).await?;
                let data = extract_data_map(&resp)
                    .ok_or_else(|| AgentOSError::invalid_response(error_msg))?;
                self.parse_resource_from_map(&data, error_msg)
            }

            /// 执行 DELETE 请求
            async fn execute_delete(&self, path: &str, error_msg: &str) -> Result<(), AgentOSError> {
                let resp = self.api.delete(path, None).await?;
                if let Some(data) = extract_data_map(&resp) {
                    if let Some(success) = data.get("success").and_then(|v| v.as_bool()) {
                        if !success {
                            return Err(AgentOSError::internal(error_msg));
                        }
                    }
                }
                Ok(())
            }

            /// 记录操作日志
            fn log_operation(&self, operation: &str, resource_id: &str) {
                log::debug!("[{}] {}: ID={}", $resource_type, operation, resource_id);
            }

            /// 记录错误日志
            fn log_error(&self, operation: &str, err: &AgentOSError) {
                log::error!("[{}] {} failed: {}", $resource_type, operation, err);
            }
        }
    };
}

/// 实现 Task 解析方法
#[macro_export]
macro_rules! impl_task_parser {
    ($name:ident) => {
        impl $name {
            /// 从 map 解析 Task
            fn parse_task_from_map(&self, data: &serde_json::Value, context: &str) -> Result<Task, AgentOSError> {
                Ok(Task {
                    id: get_string(data, "task_id")
                        .ok_or_else(|| AgentOSError::invalid_response(&format!("{}: 缺少 task_id", context)))?,
                    description: get_string(data, "description").unwrap_or_default(),
                    status: TaskStatus::from_str(&get_string(data, "status").unwrap_or_else(|| "pending".to_string())),
                    created_at: parse_time_from_map(data, "created_at"),
                    updated_at: parse_time_from_map(data, "updated_at"),
                })
            }

            /// 从 map 解析 Task 列表
            fn parse_task_list(&self, data: &serde_json::Value) -> Vec<Task> {
                get_interface_slice(data, "tasks")
                    .map(|items| {
                        items
                            .iter()
                            .filter_map(|item| {
                                if let Some(map) = item.as_object() {
                                    let json_value = serde_json::to_value(map).ok()?;
                                    self.parse_task_from_map(&json_value, "任务列表解析").ok()
                                } else {
                                    None
                                }
                            })
                            .collect()
                    })
                    .unwrap_or_default()
            }
        }
    };
}

/// 实现 Memory 解析方法
#[macro_export]
macro_rules! impl_memory_parser {
    ($name:ident) => {
        impl $name {
            /// 从 map 解析 Memory
            fn parse_memory_from_map(&self, data: &serde_json::Value, context: &str) -> Result<Memory, AgentOSError> {
                Ok(Memory {
                    id: get_string(data, "memory_id")
                        .ok_or_else(|| AgentOSError::invalid_response(&format!("{}: 缺少 memory_id", context)))?,
                    content: get_string(data, "content").unwrap_or_default(),
                    layer: MemoryLayer::from_str(&get_string(data, "layer").unwrap_or_else(|| "L1".to_string())),
                    score: get_f64(data, "score").unwrap_or(1.0),
                    metadata: get_map(data, "metadata").unwrap_or_default(),
                    created_at: parse_time_from_map(data, "created_at"),
                    updated_at: parse_time_from_map(data, "updated_at"),
                })
            }

            /// 从 map 解析 Memory 列表
            fn parse_memory_list(&self, data: &serde_json::Value) -> Vec<Memory> {
                get_interface_slice(data, "memories")
                    .map(|items| {
                        items
                            .iter()
                            .filter_map(|item| {
                                if let Some(map) = item.as_object() {
                                    let json_value = serde_json::to_value(map).ok()?;
                                    self.parse_memory_from_map(&json_value, "记忆列表解析").ok()
                                } else {
                                    None
                                }
                            })
                            .collect()
                    })
                    .unwrap_or_default()
            }
        }
    };
}

/// 实现 Session 解析方法
#[macro_export]
macro_rules! impl_session_parser {
    ($name:ident) => {
        impl $name {
            /// 从 map 解析 Session
            fn parse_session_from_map(&self, data: &serde_json::Value, context: &str) -> Result<Session, AgentOSError> {
                Ok(Session {
                    id: get_string(data, "session_id")
                        .ok_or_else(|| AgentOSError::invalid_response(&format!("{}: 缺少 session_id", context)))?,
                    status: SessionStatus::from_str(&get_string(data, "status").unwrap_or_else(|| "active".to_string())),
                    context: get_map(data, "context").unwrap_or_default(),
                    created_at: parse_time_from_map(data, "created_at"),
                    updated_at: parse_time_from_map(data, "updated_at"),
                    expires_at: parse_time_from_map(data, "expires_at"),
                })
            }

            /// 从 map 解析 Session 列表
            fn parse_session_list(&self, data: &serde_json::Value) -> Vec<Session> {
                get_interface_slice(data, "sessions")
                    .map(|items| {
                        items
                            .iter()
                            .filter_map(|item| {
                                if let Some(map) = item.as_object() {
                                    let json_value = serde_json::to_value(map).ok()?;
                                    self.parse_session_from_map(&json_value, "会话列表解析").ok()
                                } else {
                                    None
                                }
                            })
                            .collect()
                    })
                    .unwrap_or_default()
            }
        }
    };
}

/// 实现 Skill 解析方法
#[macro_export]
macro_rules! impl_skill_parser {
    ($name:ident) => {
        impl $name {
            /// 从 map 解析 Skill
            fn parse_skill_from_map(&self, data: &serde_json::Value, context: &str) -> Result<Skill, AgentOSError> {
                Ok(Skill {
                    name: get_string(data, "skill_name")
                        .ok_or_else(|| AgentOSError::invalid_response(&format!("{}: 缺少 skill_name", context)))?,
                    description: get_string(data, "description").unwrap_or_default(),
                    status: SkillStatus::from_str(&get_string(data, "status").unwrap_or_else(|| "available".to_string())),
                    loaded: get_bool(data, "loaded").unwrap_or(false),
                    created_at: parse_time_from_map(data, "created_at"),
                    updated_at: parse_time_from_map(data, "updated_at"),
                })
            }

            /// 从 map 解析 Skill 列表
            fn parse_skill_list(&self, data: &serde_json::Value) -> Vec<Skill> {
                get_interface_slice(data, "skills")
                    .map(|items| {
                        items
                            .iter()
                            .filter_map(|item| {
                                if let Some(map) = item.as_object() {
                                    let json_value = serde_json::to_value(map).ok()?;
                                    self.parse_skill_from_map(&json_value, "技能列表解析").ok()
                                } else {
                                    None
                                }
                            })
                            .collect()
                    })
                    .unwrap_or_default()
            }
        }
    };
}

/// 完整实现一个 Manager（结构 + 解析器）
#[macro_export]
macro_rules! impl_manager {
    (TaskManager, Task) => {
        impl_base_manager!(TaskManager, Task, "Task");
        impl_task_parser!(TaskManager);
    };
    (MemoryManager, Memory) => {
        impl_base_manager!(MemoryManager, Memory, "Memory");
        impl_memory_parser!(MemoryManager);
    };
    (SessionManager, Session) => {
        impl_base_manager!(SessionManager, Session, "Session");
        impl_session_parser!(SessionManager);
    };
    (SkillManager, Skill) => {
        impl_base_manager!(SkillManager, Skill, "Skill");
        impl_skill_parser!(SkillManager);
    };
}

/// 实现资源转换器 trait
#[macro_export]
macro_rules! impl_resource_converter {
    ($resource:ident, $converter:ident) => {
        pub struct $converter;

        impl ResourceConverter for $converter {
            type Resource = $resource;

            fn convert(&self, data: &serde_json::Value) -> Result<$resource, AgentOSError> {
                // 默认实现，具体转换器需要重写
                Err(AgentOSError::internal("转换器未实现"))
            }
        }
    };
}
