// AgentOS Rust SDK - 技能管理器实现
// Version: 3.0.0
// Last updated: 2026-03-24
//
// 提供技能的加载、执行、查询等功能。
// 对应 Go SDK: modules/skill/manager.go

use std::collections::HashMap;
use std::sync::Arc;

use crate::client::APIClient;
use crate::error::{AgentOSError, CODE_MISSING_PARAMETER};
use crate::types::{Skill, SkillInfo, SkillResult, SkillStatus, ListOptions, APIResponse};
use crate::utils::{extract_data_map, get_string, get_map, get_interface_slice, build_url};

/// SkillManager 管理技能完整生命周期
pub struct SkillManager {
    api: Arc<dyn APIClient>,
}

impl SkillManager {
    /// 创建新的技能管理器实例
    ///
    /// # 参数
    /// - `api`: API 客户端
    pub fn new(api: Arc<dyn APIClient>) -> Self {
        SkillManager { api }
    }

    /// 加载技能
    ///
    /// # 参数
    /// - `skill_name`: 技能名称
    ///
    /// # 返回
    /// 返回技能信息
    pub async fn load(&self, skill_name: &str) -> Result<SkillInfo, AgentOSError> {
        if skill_name.is_empty() {
            return Err(AgentOSError::with_code(CODE_MISSING_PARAMETER, "技能名称不能为空"));
        }

        let path = format!("/api/v1/skills/{}", skill_name);
        let resp = self.api.get(&path, None).await?;

        let data = extract_data_map(&resp)
            .ok_or_else(|| AgentOSError::invalid_response("技能加载响应格式异常"))?;

        Ok(self.parse_skill_info_from_map(data, skill_name))
    }

    /// 执行技能
    ///
    /// # 参数
    /// - `skill_name`: 技能名称
    /// - `parameters`: 执行参数
    ///
    /// # 返回
    /// 返回执行结果
    pub async fn execute(
        &self,
        skill_name: &str,
        parameters: Option<HashMap<String, serde_json::Value>>,
    ) -> Result<SkillResult, AgentOSError> {
        if skill_name.is_empty() {
            return Err(AgentOSError::with_code(CODE_MISSING_PARAMETER, "技能名称不能为空"));
        }

        let params = parameters.unwrap_or_default();
        let body = serde_json::json!({ "parameters": params });

        let path = format!("/api/v1/skills/{}/execute", skill_name);
        let resp = self.api.post(&path, Some(&body), None).await?;

        let data = extract_data_map(&resp)
            .ok_or_else(|| AgentOSError::invalid_response("技能执行响应格式异常"))?;

        Ok(SkillResult {
            success: data.get("success")
                .and_then(|v| v.as_bool())
                .unwrap_or(false),
            output: data.get("output").cloned(),
            error: data.get("error")
                .and_then(|v| v.as_str())
                .map(|s| s.to_string()),
        })
    }

    /// 获取技能详细信息
    ///
    /// # 参数
    /// - `skill_name`: 技能名称
    ///
    /// # 返回
    /// 返回技能对象
    pub async fn get(&self, skill_name: &str) -> Result<Skill, AgentOSError> {
        if skill_name.is_empty() {
            return Err(AgentOSError::with_code(CODE_MISSING_PARAMETER, "技能名称不能为空"));
        }

        let path = format!("/api/v1/skills/{}", skill_name);
        let resp = self.api.get(&path, None).await?;

        let data = extract_data_map(&resp)
            .ok_or_else(|| AgentOSError::invalid_response("技能详情响应格式异常"))?;

        Ok(self.parse_skill_from_map(data))
    }

    /// 列出已加载的技能
    ///
    /// # 参数
    /// - `opts`: 列表查询选项
    ///
    /// # 返回
    /// 返回技能列表
    pub async fn list(&self, opts: Option<&ListOptions>) -> Result<Vec<Skill>, AgentOSError> {
        let path = if let Some(options) = opts {
            build_url("/api/v1/skills", options.to_query_params())
        } else {
            "/api/v1/skills".to_string()
        };

        let resp = self.api.get(&path, None).await?;
        self.parse_skill_list(&resp)
    }

    /// 卸载技能
    ///
    /// # 参数
    /// - `skill_name`: 技能名称
    pub async fn unload(&self, skill_name: &str) -> Result<(), AgentOSError> {
        if skill_name.is_empty() {
            return Err(AgentOSError::with_code(CODE_MISSING_PARAMETER, "技能名称不能为空"));
        }

        let path = format!("/api/v1/skills/{}", skill_name);
        self.api.delete(&path, None).await?;
        Ok(())
    }

    /// 获取已加载技能数量
    ///
    /// # 返回
    /// 返回技能数量
    pub async fn count(&self) -> Result<i64, AgentOSError> {
        let resp = self.api.get("/api/v1/skills/count", None).await?;

        let data = extract_data_map(&resp)
            .ok_or_else(|| AgentOSError::invalid_response("技能计数响应格式异常"))?;

        Ok(data.get("count")
            .and_then(|v| v.as_i64())
            .unwrap_or(0))
    }

    /// 从 map 解析 Skill 结构
    fn parse_skill_from_map(&self, data: HashMap<String, serde_json::Value>) -> Skill {
        Skill {
            id: get_string(&data, "skill_id"),
            name: get_string(&data, "name"),
            version: {
                let version = get_string(&data, "version");
                if version.is_empty() { None } else { Some(version) }
            },
            description: {
                let desc = get_string(&data, "description");
                if desc.is_empty() { None } else { Some(desc) }
            },
            status: SkillStatus::from_str(&get_string(&data, "status"))
                .unwrap_or(SkillStatus::Active),
            parameters: get_map(&data, "parameters"),
            metadata: get_map(&data, "metadata"),
            created_at: get_string(&data, "created_at"),
        }
    }

    /// 从 map 解析 SkillInfo 结构
    fn parse_skill_info_from_map(&self, data: HashMap<String, serde_json::Value>, skill_name: &str) -> SkillInfo {
        SkillInfo {
            name: skill_name.to_string(),
            description: {
                let desc = get_string(&data, "description");
                if desc.is_empty() { None } else { Some(desc) }
            },
            version: {
                let version = get_string(&data, "version");
                if version.is_empty() { None } else { Some(version) }
            },
            parameters: get_map(&data, "parameters"),
        }
    }

    /// 从 APIResponse 解析 Skill 列表
    fn parse_skill_list(&self, resp: &APIResponse) -> Result<Vec<Skill>, AgentOSError> {
        let data = extract_data_map(resp)
            .ok_or_else(|| AgentOSError::invalid_response("技能列表响应格式异常"))?;

        let items = get_interface_slice(&data, "skills");
        let mut skills = Vec::with_capacity(items.len());

        for item in items {
            if let Some(obj) = item.as_object() {
                let data: HashMap<String, serde_json::Value> = obj
                    .iter()
                    .map(|(k, v)| (k.clone(), v.clone()))
                    .collect();
                skills.push(self.parse_skill_from_map(data));
            }
        }

        Ok(skills)
    }
}

impl SkillStatus {
    /// 从字符串解析技能状态
    pub fn from_str(s: &str) -> Option<Self> {
        match s {
            "active" => Some(SkillStatus::Active),
            "inactive" => Some(SkillStatus::Inactive),
            "deprecated" => Some(SkillStatus::Deprecated),
            _ => None,
        }
    }
}
