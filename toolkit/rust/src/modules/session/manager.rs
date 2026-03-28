// AgentOS Rust SDK - 会话管理器实现
// Version: 3.0.0
// Last updated: 2026-03-24
//
// 提供会话的创建、查询、更新、删除及上下文管理功能。
// 对应 Go SDK: modules/session/manager.go

use std::collections::HashMap;
use std::sync::Arc;

use crate::client::APIClient;
use crate::error::{AgentOSError, CODE_MISSING_PARAMETER};
use crate::types::{Session, SessionStatus, ListOptions, APIResponse};
use crate::utils::{extract_data_map, get_string, get_map, get_interface_slice, build_url};

/// SessionManager 管理会话完整生命周期
pub struct SessionManager {
    api: Arc<dyn APIClient>,
}

impl SessionManager {
    /// 创建新的会话管理器实例
    ///
    /// # 参数
    /// - `api`: API 客户端
    pub fn new(api: Arc<dyn APIClient>) -> Self {
        SessionManager { api }
    }

    /// 创建新会话
    ///
    /// # 返回
    /// 返回创建的会话对象
    pub async fn create(&self) -> Result<Session, AgentOSError> {
        let resp = self.api.post("/api/v1/sessions", None, None).await?;

        let data = extract_data_map(&resp)
            .ok_or_else(|| AgentOSError::invalid_response("会话创建响应格式异常"))?;

        Ok(self.parse_session_from_map(data))
    }

    /// 创建带用户 ID 的新会话
    ///
    /// # 参数
    /// - `user_id`: 用户 ID
    ///
    /// # 返回
    /// 返回创建的会话对象
    pub async fn create_with_user(&self, user_id: &str) -> Result<Session, AgentOSError> {
        let body = serde_json::json!({ "user_id": user_id });
        let resp = self.api.post("/api/v1/sessions", Some(&body), None).await?;

        let data = extract_data_map(&resp)
            .ok_or_else(|| AgentOSError::invalid_response("会话创建响应格式异常"))?;

        Ok(self.parse_session_from_map(data))
    }

    /// 获取指定会话的详细信息
    ///
    /// # 参数
    /// - `session_id`: 会话 ID
    ///
    /// # 返回
    /// 返回会话对象
    pub async fn get(&self, session_id: &str) -> Result<Session, AgentOSError> {
        if session_id.is_empty() {
            return Err(AgentOSError::with_code(CODE_MISSING_PARAMETER, "会话ID不能为空"));
        }

        let path = format!("/api/v1/sessions/{}", session_id);
        let resp = self.api.get(&path, None).await?;

        let data = extract_data_map(&resp)
            .ok_or_else(|| AgentOSError::invalid_response("会话详情响应格式异常"))?;

        Ok(self.parse_session_from_map(data))
    }

    /// 设置会话上下文值
    ///
    /// # 参数
    /// - `session_id`: 会话 ID
    /// - `key`: 上下文键
    /// - `value`: 上下文值
    pub async fn set_context(
        &self,
        session_id: &str,
        key: &str,
        value: serde_json::Value,
    ) -> Result<(), AgentOSError> {
        if session_id.is_empty() {
            return Err(AgentOSError::with_code(CODE_MISSING_PARAMETER, "会话ID不能为空"));
        }

        let body = serde_json::json!({ "key": key, "value": value });
        let path = format!("/api/v1/sessions/{}/context", session_id);
        self.api.post(&path, Some(&body), None).await?;
        Ok(())
    }

    /// 获取会话上下文值
    ///
    /// # 参数
    /// - `session_id`: 会话 ID
    /// - `key`: 上下文键
    ///
    /// # 返回
    /// 返回上下文值
    pub async fn get_context(
        &self,
        session_id: &str,
        key: &str,
    ) -> Result<serde_json::Value, AgentOSError> {
        if session_id.is_empty() {
            return Err(AgentOSError::with_code(CODE_MISSING_PARAMETER, "会话ID不能为空"));
        }

        let path = format!("/api/v1/sessions/{}/context/{}", session_id, key);
        let resp = self.api.get(&path, None).await?;

        let data = extract_data_map(&resp)
            .ok_or_else(|| AgentOSError::invalid_response("上下文响应格式异常"))?;

        data.get("value")
            .cloned()
            .ok_or_else(|| AgentOSError::invalid_response("缺少 value 字段"))
    }

    /// 删除会话上下文值
    ///
    /// # 参数
    /// - `session_id`: 会话 ID
    /// - `key`: 上下文键
    pub async fn delete_context(&self, session_id: &str, key: &str) -> Result<(), AgentOSError> {
        if session_id.is_empty() {
            return Err(AgentOSError::with_code(CODE_MISSING_PARAMETER, "会话ID不能为空"));
        }

        let path = format!("/api/v1/sessions/{}/context/{}", session_id, key);
        self.api.delete(&path, None).await?;
        Ok(())
    }

    /// 关闭会话
    ///
    /// # 参数
    /// - `session_id`: 会话 ID
    pub async fn close(&self, session_id: &str) -> Result<(), AgentOSError> {
        if session_id.is_empty() {
            return Err(AgentOSError::with_code(CODE_MISSING_PARAMETER, "会话ID不能为空"));
        }

        let path = format!("/api/v1/sessions/{}", session_id);
        self.api.delete(&path, None).await?;
        Ok(())
    }

    /// 列出会话，支持分页和过滤
    ///
    /// # 参数
    /// - `opts`: 列表查询选项
    ///
    /// # 返回
    /// 返回会话列表
    pub async fn list(&self, opts: Option<&ListOptions>) -> Result<Vec<Session>, AgentOSError> {
        let path = if let Some(options) = opts {
            build_url("/api/v1/sessions", options.to_query_params())
        } else {
            "/api/v1/sessions".to_string()
        };

        let resp = self.api.get(&path, None).await?;
        self.parse_session_list(&resp)
    }

    /// 获取活跃会话数量
    ///
    /// # 返回
    /// 返回活跃会话数量
    pub async fn count_active(&self) -> Result<i64, AgentOSError> {
        let resp = self.api.get("/api/v1/sessions/count", None).await?;

        let data = extract_data_map(&resp)
            .ok_or_else(|| AgentOSError::invalid_response("会话计数响应格式异常"))?;

        Ok(data.get("count")
            .and_then(|v| v.as_i64())
            .unwrap_or(0))
    }

    /// 从 map 解析 Session 结构
    fn parse_session_from_map(&self, data: HashMap<String, serde_json::Value>) -> Session {
        Session {
            id: get_string(&data, "session_id"),
            user_id: {
                let user_id = get_string(&data, "user_id");
                if user_id.is_empty() { None } else { Some(user_id) }
            },
            status: SessionStatus::from_str(&get_string(&data, "status"))
                .unwrap_or(SessionStatus::Active),
            context: get_map(&data, "context"),
            metadata: get_map(&data, "metadata"),
            created_at: get_string(&data, "created_at"),
            last_activity: get_string(&data, "last_activity"),
        }
    }

    /// 从 APIResponse 解析 Session 列表
    fn parse_session_list(&self, resp: &APIResponse) -> Result<Vec<Session>, AgentOSError> {
        let data = extract_data_map(resp)
            .ok_or_else(|| AgentOSError::invalid_response("会话列表响应格式异常"))?;

        let items = get_interface_slice(&data, "sessions");
        let mut sessions = Vec::with_capacity(items.len());

        for item in items {
            if let Some(obj) = item.as_object() {
                let data: HashMap<String, serde_json::Value> = obj
                    .iter()
                    .map(|(k, v)| (k.clone(), v.clone()))
                    .collect();
                sessions.push(self.parse_session_from_map(data));
            }
        }

        Ok(sessions)
    }
}

impl SessionStatus {
    /// 从字符串解析会话状态
    pub fn from_str(s: &str) -> Option<Self> {
        match s {
            "active" => Some(SessionStatus::Active),
            "inactive" => Some(SessionStatus::Inactive),
            "expired" => Some(SessionStatus::Expired),
            _ => None,
        }
    }
}
