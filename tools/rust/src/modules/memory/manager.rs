// AgentOS Rust SDK - 记忆管理器实现
// Version: 3.0.0
// Last updated: 2026-03-24
//
// 提供记忆的写入、搜索、更新、删除及统计功能。
// 对应 Go SDK: modules/memory/manager.go

use std::collections::HashMap;
use std::sync::Arc;

use crate::client::APIClient;
use crate::error::{AgentOSError, CODE_MISSING_PARAMETER};
use crate::types::{Memory, MemoryLayer, MemorySearchResult, ListOptions, APIResponse};
use crate::utils::{extract_data_map, get_string, get_i64, get_f64, get_map, get_interface_slice, build_url};

/// MemoryWriteItem 批量写入时的单条记忆项
#[derive(Debug, Clone)]
pub struct MemoryWriteItem {
    /// 记忆内容
    pub content: String,
    /// 记忆层级
    pub layer: MemoryLayer,
    /// 元数据
    pub metadata: Option<HashMap<String, serde_json::Value>>,
}

/// MemoryManager 管理记忆完整生命周期
pub struct MemoryManager {
    api: Arc<dyn APIClient>,
}

impl MemoryManager {
    /// 创建新的记忆管理器实例
    ///
    /// # 参数
    /// - `api`: API 客户端
    pub fn new(api: Arc<dyn APIClient>) -> Self {
        MemoryManager { api }
    }

    /// 写入一条新记忆到指定层级
    ///
    /// # 参数
    /// - `content`: 记忆内容
    /// - `layer`: 记忆层级
    ///
    /// # 返回
    /// 返回创建的记忆对象
    pub async fn write(&self, content: &str, layer: MemoryLayer) -> Result<Memory, AgentOSError> {
        self.write_with_options(content, layer, None).await
    }

    /// 使用元数据选项写入新记忆
    ///
    /// # 参数
    /// - `content`: 记忆内容
    /// - `layer`: 记忆层级
    /// - `metadata`: 元数据
    ///
    /// # 返回
    /// 返回创建的记忆对象
    pub async fn write_with_options(
        &self,
        content: &str,
        layer: MemoryLayer,
        metadata: Option<HashMap<String, serde_json::Value>>,
    ) -> Result<Memory, AgentOSError> {
        if content.is_empty() {
            return Err(AgentOSError::with_code(CODE_MISSING_PARAMETER, "记忆内容不能为空"));
        }

        let mut body = serde_json::json!({
            "content": content,
            "layer": layer.as_str()
        });

        if let Some(meta) = metadata {
            body["metadata"] = serde_json::to_value(meta).map_err(|e| {
                AgentOSError::parse_error(&format!("序列化元数据失败: {}", e))
            })?;
        }

        let resp = self.api.post("/api/v1/memories", Some(&body), None).await?;

        let data = extract_data_map(&resp)
            .ok_or_else(|| AgentOSError::invalid_response("记忆写入响应格式异常"))?;

        Ok(self.parse_memory_from_map(data))
    }

    /// 获取指定记忆的详细信息
    ///
    /// # 参数
    /// - `memory_id`: 记忆 ID
    ///
    /// # 返回
    /// 返回记忆对象
    pub async fn get(&self, memory_id: &str) -> Result<Memory, AgentOSError> {
        if memory_id.is_empty() {
            return Err(AgentOSError::with_code(CODE_MISSING_PARAMETER, "记忆ID不能为空"));
        }

        let path = format!("/api/v1/memories/{}", memory_id);
        let resp = self.api.get(&path, None).await?;

        let data = extract_data_map(&resp)
            .ok_or_else(|| AgentOSError::invalid_response("记忆详情响应格式异常"))?;

        Ok(self.parse_memory_from_map(data))
    }

    /// 搜索记忆，返回按相关度排序的结果
    ///
    /// # 参数
    /// - `query`: 搜索查询
    /// - `top_k`: 返回数量
    ///
    /// # 返回
    /// 返回记忆搜索结果
    pub async fn search(&self, query: &str, top_k: i32) -> Result<MemorySearchResult, AgentOSError> {
        if query.is_empty() {
            return Err(AgentOSError::with_code(CODE_MISSING_PARAMETER, "搜索查询不能为空"));
        }

        let top_k = if top_k <= 0 { 10 } else { top_k };

        let params = HashMap::from([
            ("query".to_string(), query.to_string()),
            ("top_k".to_string(), top_k.to_string()),
        ]);

        let path = build_url("/api/v1/memories/search", params);
        let resp = self.api.get(&path, None).await?;

        self.parse_memory_search_result(&resp, query, top_k)
    }

    /// 在指定层级内搜索记忆
    ///
    /// # 参数
    /// - `query`: 搜索查询
    /// - `layer`: 记忆层级
    /// - `top_k`: 返回数量
    ///
    /// # 返回
    /// 返回记忆搜索结果
    pub async fn search_by_layer(
        &self,
        query: &str,
        layer: MemoryLayer,
        top_k: i32,
    ) -> Result<MemorySearchResult, AgentOSError> {
        if query.is_empty() {
            return Err(AgentOSError::with_code(CODE_MISSING_PARAMETER, "搜索查询不能为空"));
        }

        let top_k = if top_k <= 0 { 10 } else { top_k };

        let params = HashMap::from([
            ("query".to_string(), query.to_string()),
            ("layer".to_string(), layer.as_str().to_string()),
            ("top_k".to_string(), top_k.to_string()),
        ]);

        let path = build_url("/api/v1/memories/search", params);
        let resp = self.api.get(&path, None).await?;

        self.parse_memory_search_result(&resp, query, top_k)
    }

    /// 更新指定记忆的内容
    ///
    /// # 参数
    /// - `memory_id`: 记忆 ID
    /// - `content`: 新内容
    ///
    /// # 返回
    /// 返回更新后的记忆对象
    pub async fn update(&self, memory_id: &str, content: &str) -> Result<Memory, AgentOSError> {
        if memory_id.is_empty() {
            return Err(AgentOSError::with_code(CODE_MISSING_PARAMETER, "记忆ID不能为空"));
        }

        let body = serde_json::json!({ "content": content });
        let path = format!("/api/v1/memories/{}", memory_id);
        let resp = self.api.put(&path, Some(&body), None).await?;

        let data = extract_data_map(&resp)
            .ok_or_else(|| AgentOSError::invalid_response("记忆更新响应格式异常"))?;

        Ok(self.parse_memory_from_map(data))
    }

    /// 删除指定记忆
    ///
    /// # 参数
    /// - `memory_id`: 记忆 ID
    pub async fn delete(&self, memory_id: &str) -> Result<(), AgentOSError> {
        if memory_id.is_empty() {
            return Err(AgentOSError::with_code(CODE_MISSING_PARAMETER, "记忆ID不能为空"));
        }

        let path = format!("/api/v1/memories/{}", memory_id);
        self.api.delete(&path, None).await?;
        Ok(())
    }

    /// 列出记忆，支持分页和过滤
    ///
    /// # 参数
    /// - `opts`: 列表查询选项
    ///
    /// # 返回
    /// 返回记忆列表
    pub async fn list(&self, opts: Option<&ListOptions>) -> Result<Vec<Memory>, AgentOSError> {
        let path = if let Some(options) = opts {
            build_url("/api/v1/memories", options.to_query_params())
        } else {
            "/api/v1/memories".to_string()
        };

        let resp = self.api.get(&path, None).await?;
        self.parse_memory_list(&resp)
    }

    /// 按层级列出记忆
    ///
    /// # 参数
    /// - `layer`: 记忆层级
    /// - `opts`: 列表查询选项
    ///
    /// # 返回
    /// 返回记忆列表
    pub async fn list_by_layer(
        &self,
        layer: MemoryLayer,
        opts: Option<&ListOptions>,
    ) -> Result<Vec<Memory>, AgentOSError> {
        let mut params = HashMap::new();
        params.insert("layer".to_string(), layer.as_str().to_string());

        if let Some(options) = opts {
            for (k, v) in options.to_query_params() {
                params.insert(k, v);
            }
        }

        let path = build_url("/api/v1/memories", params);
        let resp = self.api.get(&path, None).await?;
        self.parse_memory_list(&resp)
    }

    /// 获取记忆总数
    ///
    /// # 返回
    /// 返回记忆总数
    pub async fn count(&self) -> Result<i64, AgentOSError> {
        let resp = self.api.get("/api/v1/memories/count", None).await?;

        let data = extract_data_map(&resp)
            .ok_or_else(|| AgentOSError::invalid_response("记忆计数响应格式异常"))?;

        Ok(get_i64(&data, "count"))
    }

    /// 清空所有记忆数据
    pub async fn clear(&self) -> Result<(), AgentOSError> {
        self.api.delete("/api/v1/memories", None).await?;
        Ok(())
    }

    /// 批量写入多条记忆
    ///
    /// # 参数
    /// - `memories`: 记忆项列表
    ///
    /// # 返回
    /// 返回创建的记忆列表
    pub async fn batch_write(&self, memories: &[MemoryWriteItem]) -> Result<Vec<Memory>, AgentOSError> {
        let mut results = Vec::with_capacity(memories.len());

        for item in memories {
            let memory = self.write_with_options(&item.content, item.layer, item.metadata.clone()).await?;
            results.push(memory);
        }

        Ok(results)
    }

    /// 触发记忆演化过程（L1→L2→L3→L4 层级升华）
    pub async fn evolve(&self) -> Result<(), AgentOSError> {
        self.api.post("/api/v1/memories/evolve", None, None).await?;
        Ok(())
    }

    /// 获取各层级的记忆统计数据
    ///
    /// # 返回
    /// 返回统计数据 map
    pub async fn get_stats(&self) -> Result<HashMap<String, i64>, AgentOSError> {
        let resp = self.api.get("/api/v1/memories/stats", None).await?;

        let data = extract_data_map(&resp)
            .ok_or_else(|| AgentOSError::invalid_response("记忆统计响应格式异常"))?;

        Ok(data.iter()
            .filter_map(|(k, v)| v.as_i64().map(|i| (k.clone(), i)))
            .collect())
    }

    /// 从 map 解析 Memory 结构
    fn parse_memory_from_map(&self, data: HashMap<String, serde_json::Value>) -> Memory {
        Memory {
            id: get_string(&data, "memory_id"),
            content: get_string(&data, "content"),
            layer: MemoryLayer::from_str(&get_string(&data, "layer"))
                .unwrap_or(MemoryLayer::L1),
            score: get_f64(&data, "score"),
            metadata: get_map(&data, "metadata"),
            created_at: get_string(&data, "created_at"),
            updated_at: get_string(&data, "updated_at"),
        }
    }

    /// 从 APIResponse 解析 Memory 列表
    fn parse_memory_list(&self, resp: &APIResponse) -> Result<Vec<Memory>, AgentOSError> {
        let data = extract_data_map(resp)
            .ok_or_else(|| AgentOSError::invalid_response("记忆列表响应格式异常"))?;

        let items = get_interface_slice(&data, "memories");
        let mut memories = Vec::with_capacity(items.len());

        for item in items {
            if let Some(obj) = item.as_object() {
                let data: HashMap<String, serde_json::Value> = obj
                    .iter()
                    .map(|(k, v)| (k.clone(), v.clone()))
                    .collect();
                memories.push(self.parse_memory_from_map(data));
            }
        }

        Ok(memories)
    }

    /// 从 APIResponse 解析记忆搜索结果
    fn parse_memory_search_result(
        &self,
        resp: &APIResponse,
        query: &str,
        top_k: i32,
    ) -> Result<MemorySearchResult, AgentOSError> {
        let data = extract_data_map(resp)
            .ok_or_else(|| AgentOSError::invalid_response("记忆搜索响应格式异常"))?;

        let memories = self.parse_memory_list(resp)?;

        Ok(MemorySearchResult {
            memories,
            total: get_i64(&data, "total"),
            query: query.to_string(),
            top_k,
        })
    }
}
