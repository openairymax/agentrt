// AgentOS Rust SDK - 工具函数实现
// Version: 3.0.0
// Last updated: 2026-03-24
//
// 提供通用工具函数、ID 生成、验证等功能。
// 对应 Go SDK: utils/helpers.go

use serde_json::Value;
use std::collections::HashMap;
use chrono::{DateTime, Utc};
use uuid::Uuid;

/// 从 APIResponse 中提取 data 字段作为 map
///
/// # 参数
/// - `response`: API 响应对象
///
/// # 返回
/// 返回 Option<HashMap<String, Value>>
pub fn extract_data_map(response: &crate::types::APIResponse) -> Option<HashMap<String, Value>> {
    response.data.as_object().map(|obj| {
        obj.iter()
            .map(|(k, v)| (k.clone(), v.clone()))
            .collect()
    })
}

/// 从 map 中获取字符串值
///
/// # 参数
/// - `data`: 数据 map
/// - `key`: 键名
///
/// # 返回
/// 返回字符串值，如果不存在则返回空字符串
pub fn get_string(data: &HashMap<String, Value>, key: &str) -> String {
    data.get(key)
        .and_then(|v| v.as_str())
        .unwrap_or("")
        .to_string()
}

/// 从 map 中获取 i64 值
///
/// # 参数
/// - `data`: 数据 map
/// - `key`: 键名
///
/// # 返回
/// 返回 i64 值，如果不存在则返回 0
pub fn get_i64(data: &HashMap<String, Value>, key: &str) -> i64 {
    data.get(key)
        .and_then(|v| v.as_i64())
        .unwrap_or(0)
}

/// 从 map 中获取 f64 值
///
/// # 参数
/// - `data`: 数据 map
/// - `key`: 键名
///
/// # 返回
/// 返回 f64 值，如果不存在则返回 0.0
pub fn get_f64(data: &HashMap<String, Value>, key: &str) -> f64 {
    data.get(key)
        .and_then(|v| v.as_f64())
        .unwrap_or(0.0)
}

/// 从 map 中获取 bool 值
///
/// # 参数
/// - `data`: 数据 map
/// - `key`: 键名
///
/// # 返回
/// 返回 bool 值，如果不存在则返回 false
pub fn get_bool(data: &HashMap<String, Value>, key: &str) -> bool {
    data.get(key)
        .and_then(|v| v.as_bool())
        .unwrap_or(false)
}

/// 从 map 中获取嵌套 map 值
///
/// # 参数
/// - `data`: 数据 map
/// - `key`: 键名
///
/// # 返回
/// 返回 HashMap，如果不存在则返回空 map
pub fn get_map(data: &HashMap<String, Value>, key: &str) -> HashMap<String, Value> {
    data.get(key)
        .and_then(|v| v.as_object())
        .map(|obj| {
            obj.iter()
                .map(|(k, v)| (k.clone(), v.clone()))
                .collect()
        })
        .unwrap_or_default()
}

/// 从 map 中获取字符串 map 值
///
/// # 参数
/// - `data`: 数据 map
/// - `key`: 键名
///
/// # 返回
/// 返回 HashMap<String, String>，如果不存在则返回空 map
pub fn get_string_map(data: &HashMap<String, Value>, key: &str) -> HashMap<String, String> {
    data.get(key)
        .and_then(|v| v.as_object())
        .map(|obj| {
            obj.iter()
                .filter_map(|(k, v)| {
                    v.as_str().map(|s| (k.clone(), s.to_string()))
                })
                .collect()
        })
        .unwrap_or_default()
}

/// 从 map 中获取数组值
///
/// # 参数
/// - `data`: 数据 map
/// - `key`: 键名
///
/// # 返回
/// 返回 Vec<Value>，如果不存在则返回空数组
pub fn get_array(data: &HashMap<String, Value>, key: &str) -> Vec<Value> {
    data.get(key)
        .and_then(|v| v.as_array())
        .map(|arr| arr.clone())
        .unwrap_or_default()
}

/// 从 map 中获取接口切片
///
/// # 参数
/// - `data`: 数据 map
/// - `key`: 键名
///
/// # 返回
/// 返回 Vec<Value>，如果不存在则返回空数组
pub fn get_interface_slice(data: &HashMap<String, Value>, key: &str) -> Vec<Value> {
    get_array(data, key)
}

/// 从 map 中解析时间字段
///
/// # 参数
/// - `data`: 数据 map
/// - `key`: 键名
///
/// # 返回
/// 返回 DateTime<Utc>，如果解析失败则返回当前时间
pub fn parse_time_from_map(data: &HashMap<String, Value>, key: &str) -> DateTime<Utc> {
    let time_str = get_string(data, key);
    DateTime::parse_from_rfc3339(&time_str)
        .map(|dt| dt.with_timezone(&Utc))
        .unwrap_or_else(|_| Utc::now())
}

/// 提取 int64 统计数据
///
/// # 参数
/// - `data`: 数据 map
///
/// # 返回
/// 返回 HashMap<String, i64>
pub fn extract_int64_stats(data: &HashMap<String, Value>) -> HashMap<String, i64> {
    data.iter()
        .filter_map(|(k, v)| {
            v.as_i64().map(|i| (k.clone(), i))
        })
        .collect()
}

/// 构建 URL（带查询参数）
///
/// # 参数
/// - `path`: 基础路径
/// - `params`: 查询参数
///
/// # 返回
/// 返回完整的 URL 路径
pub fn build_url(path: &str, params: HashMap<String, String>) -> String {
    if params.is_empty() {
        return path.to_string();
    }

    let query_string: Vec<String> = params
        .iter()
        .map(|(k, v)| format!("{}={}", urlencoding::encode(k), urlencoding::encode(v)))
        .collect();

    format!("{}?{}", path, query_string.join("&"))
}

/// 生成唯一 ID（UUID v4）
///
/// # 返回
/// 返回 UUID 字符串
pub fn generate_id() -> String {
    Uuid::new_v4().to_string()
}

/// 生成任务 ID
///
/// # 返回
/// 返回格式为 "task-{uuid}" 的 ID
pub fn generate_task_id() -> String {
    format!("task-{}", generate_id())
}

/// 生成记忆 ID
///
/// # 返回
/// 返回格式为 "mem-{uuid}" 的 ID
pub fn generate_memory_id() -> String {
    format!("mem-{}", generate_id())
}

/// 生成会话 ID
///
/// # 返回
/// 返回格式为 "session-{uuid}" 的 ID
pub fn generate_session_id() -> String {
    format!("session-{}", generate_id())
}

/// 验证端点 URL 格式
///
/// # 参数
/// - `endpoint`: 端点 URL
///
/// # 返回
/// 如果格式正确返回 true，否则返回 false
pub fn validate_endpoint(endpoint: &str) -> bool {
    endpoint.starts_with("http://") || endpoint.starts_with("https://")
}

/// 格式化时间为 ISO 8601 字符串
///
/// # 参数
/// - `datetime`: 时间对象
///
/// # 返回
/// 返回 ISO 8601 格式的字符串
pub fn format_time(datetime: &DateTime<Utc>) -> String {
    datetime.to_rfc3339()
}

/// 解析 ISO 8601 时间字符串
///
/// # 参数
/// - `time_str`: 时间字符串
///
/// # 返回
/// 返回 DateTime<Utc>
pub fn parse_time(time_str: &str) -> Result<DateTime<Utc>, chrono::ParseError> {
    DateTime::parse_from_rfc3339(time_str)
        .map(|dt| dt.with_timezone(&Utc))
}

/// 深拷贝 JSON Value
///
/// # 参数
/// - `value`: JSON 值
///
/// # 返回
/// 返回深拷贝后的值
pub fn deep_clone_value(value: &Value) -> Value {
    value.clone()
}

/// 合并两个 HashMap（override 覆盖 base）
///
/// # 参数
/// - `base`: 基础 map
/// - `override`: 覆盖 map
///
/// # 返回
/// 返回合并后的 map
pub fn merge_maps(base: &HashMap<String, Value>, override_map: &HashMap<String, Value>) -> HashMap<String, Value> {
    let mut result = base.clone();
    for (k, v) in override_map {
        result.insert(k.clone(), v.clone());
    }
    result
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;

    #[test]
    fn test_get_string() {
        let mut data = HashMap::new();
        data.insert("name".to_string(), json!("test"));
        assert_eq!(get_string(&data, "name"), "test");
        assert_eq!(get_string(&data, "missing"), "");
    }

    #[test]
    fn test_get_i64() {
        let mut data = HashMap::new();
        data.insert("count".to_string(), json!(42));
        assert_eq!(get_i64(&data, "count"), 42);
        assert_eq!(get_i64(&data, "missing"), 0);
    }

    #[test]
    fn test_get_f64() {
        let mut data = HashMap::new();
        data.insert("score".to_string(), json!(3.14));
        assert_eq!(get_f64(&data, "score"), 3.14);
        assert_eq!(get_f64(&data, "missing"), 0.0);
    }

    #[test]
    fn test_get_bool() {
        let mut data = HashMap::new();
        data.insert("active".to_string(), json!(true));
        assert_eq!(get_bool(&data, "active"), true);
        assert_eq!(get_bool(&data, "missing"), false);
    }

    #[test]
    fn test_build_url() {
        let mut params = HashMap::new();
        params.insert("query".to_string(), "test".to_string());
        params.insert("limit".to_string(), "10".to_string());

        let url = build_url("/api/v1/search", params);
        assert!(url.contains("/api/v1/search?"));
        assert!(url.contains("query=test"));
        assert!(url.contains("limit=10"));
    }

    #[test]
    fn test_generate_id() {
        let id1 = generate_id();
        let id2 = generate_id();
        assert_ne!(id1, id2);
        assert!(id1.len() > 0);
    }

    #[test]
    fn test_generate_task_id() {
        let id = generate_task_id();
        assert!(id.starts_with("task-"));
    }

    #[test]
    fn test_generate_memory_id() {
        let id = generate_memory_id();
        assert!(id.starts_with("mem-"));
    }

    #[test]
    fn test_validate_endpoint() {
        assert!(validate_endpoint("http://localhost:18789"));
        assert!(validate_endpoint("https://api.example.com"));
        assert!(!validate_endpoint("invalid-endpoint"));
        assert!(!validate_endpoint("ftp://example.com"));
    }
}
