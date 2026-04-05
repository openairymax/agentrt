// AgentOS Rust SDK - HTTP 客户端实现
// Version: 3.0.0
// Last updated: 2026-03-24
//
// 提供 HTTP 通信层、APIClient 接口定义和依赖倒转抽象。
// 对应 Go SDK: client/client.go

use reqwest::{Client as ReqwestClient, RequestBuilder, Response, StatusCode};
use serde_json::{json, Value};
use std::collections::HashMap;
use std::time::Duration;

use crate::error::{AgentOSError, http_status_to_code};
use crate::types::{APIResponse, HealthStatus, Metrics, RequestOptions, RequestOption};

/// APIClient 定义所有 Manager 共同依赖的 HTTP 通信接口
#[async_trait::async_trait]
pub trait APIClient: Send + Sync {
    /// 执行 HTTP GET 请求
    async fn get(&self, path: &str, opts: Option<Vec<RequestOption>>) -> Result<APIResponse, AgentOSError>;

    /// 执行 HTTP POST 请求
    async fn post(&self, path: &str, body: Option<&Value>, opts: Option<Vec<RequestOption>>) -> Result<APIResponse, AgentOSError>;

    /// 执行 HTTP PUT 请求
    async fn put(&self, path: &str, body: Option<&Value>, opts: Option<Vec<RequestOption>>) -> Result<APIResponse, AgentOSError>;

    /// 执行 HTTP DELETE 请求
    async fn delete(&self, path: &str, opts: Option<Vec<RequestOption>>) -> Result<APIResponse, AgentOSError>;
}

/// Client 是 AgentOS Rust SDK 的核心 HTTP 客户端
#[derive(Debug, Clone)]
pub struct Client {
    endpoint: String,
    http_client: ReqwestClient,
    api_key: Option<String>,
    user_agent: String,
    timeout: Duration,
    max_retries: u32,
    retry_delay: Duration,
}

/// 响应体最大允许大小（10MB）
const MAX_RESPONSE_BODY_SIZE: usize = 10 * 1024 * 1024;

impl Client {
    /// 创建新的 AgentOS 客户端（使用默认配置）
    ///
    /// # 参数
    /// - `endpoint`: AgentOS 服务端点地址，如 "http://localhost:18789"
    ///
    /// # 返回
    /// 返回 Result<Client, AgentOSError>
    ///
    /// # 示例
    /// ```rust
    /// use agentos::client::Client;
    ///
    /// let client = Client::new("http://localhost:18789")?;
    /// ```
    pub fn new(endpoint: &str) -> Result<Self, AgentOSError> {
        Self::builder(endpoint).build()
    }

    /// 创建客户端构建器
    ///
    /// # 参数
    /// - `endpoint`: AgentOS 服务端点地址
    ///
    /// # 返回
    /// 返回 ClientBuilder 实例
    pub fn builder(endpoint: &str) -> ClientBuilder {
        ClientBuilder::new(endpoint)
    }

    /// 创建带自定义超时的 AgentOS 客户端
    ///
    /// # 参数
    /// - `endpoint`: AgentOS 服务端点地址
    /// - `timeout`: 请求超时时间
    pub fn new_with_timeout(endpoint: &str, timeout: Duration) -> Result<Self, AgentOSError> {
        Self::builder(endpoint)
            .timeout(timeout)
            .build()
    }

    /// 创建带 API Key 的 AgentOS 客户端
    ///
    /// # 参数
    /// - `endpoint`: AgentOS 服务端点地址
    /// - `api_key`: API 密钥
    pub fn new_with_api_key(endpoint: &str, api_key: &str) -> Result<Self, AgentOSError> {
        Self::builder(endpoint)
            .api_key(api_key)
            .build()
    }

    /// 获取客户端端点地址
    pub fn endpoint(&self) -> &str {
        &self.endpoint
    }

    /// 获取 API Key
    pub fn api_key(&self) -> Option<&str> {
        self.api_key.as_deref()
    }

    /// 健康检查 AgentOS 服务状态
    ///
    /// # 返回
    /// 返回健康状态信息
    pub async fn health(&self) -> Result<HealthStatus, AgentOSError> {
        let resp = self.get("/health", None).await?;
        let data = resp.data.as_object()
            .ok_or_else(|| AgentOSError::invalid_response("健康检查响应格式异常"))?;

        Ok(HealthStatus {
            status: data.get("status")
                .and_then(|v| v.as_str())
                .unwrap_or("unknown")
                .to_string(),
            version: data.get("version")
                .and_then(|v| v.as_str())
                .unwrap_or("unknown")
                .to_string(),
            uptime: data.get("uptime")
                .and_then(|v| v.as_i64())
                .unwrap_or(0),
            checks: data.get("checks")
                .and_then(|v| v.as_object())
                .map(|obj| obj.iter()
                    .map(|(k, v)| (k.clone(), v.as_str().unwrap_or("").to_string()))
                    .collect())
                .unwrap_or_default(),
        })
    }

    /// 获取 AgentOS 系统运行指标
    ///
    /// # 返回
    /// 返回系统指标快照
    pub async fn metrics(&self) -> Result<Metrics, AgentOSError> {
        let resp = self.get("/metrics", None).await?;
        let data = resp.data.as_object()
            .ok_or_else(|| AgentOSError::invalid_response("指标响应格式异常"))?;

        Ok(Metrics {
            tasks_total: data.get("tasks_total").and_then(|v| v.as_i64()).unwrap_or(0),
            tasks_completed: data.get("tasks_completed").and_then(|v| v.as_i64()).unwrap_or(0),
            tasks_failed: data.get("tasks_failed").and_then(|v| v.as_i64()).unwrap_or(0),
            memories_total: data.get("memories_total").and_then(|v| v.as_i64()).unwrap_or(0),
            sessions_active: data.get("sessions_active").and_then(|v| v.as_i64()).unwrap_or(0),
            skills_loaded: data.get("skills_loaded").and_then(|v| v.as_i64()).unwrap_or(0),
            cpu_usage: data.get("cpu_usage").and_then(|v| v.as_f64()).unwrap_or(0.0),
            memory_usage: data.get("memory_usage").and_then(|v| v.as_f64()).unwrap_or(0.0),
            request_count: data.get("request_count").and_then(|v| v.as_i64()).unwrap_or(0),
            average_latency_ms: data.get("average_latency_ms").and_then(|v| v.as_f64()).unwrap_or(0.0),
        })
    }

    /// 执行底层 HTTP 请求，包含序列化、重试和响应解析逻辑
    async fn request_internal(
        &self,
        method: reqwest::Method,
        path: &str,
        body: Option<&Value>,
        opts: Option<Vec<RequestOption>>,
    ) -> Result<APIResponse, AgentOSError> {
        let mut options = RequestOptions::default();
        if let Some(opts_vec) = opts {
            for opt in opts_vec {
                opt(&mut options);
            }
        }

        // 构建完整 URL
        let mut url = format!("{}{}", self.endpoint, path);
        if !options.query_params.is_empty() {
            let query_string: Vec<String> = options.query_params
                .iter()
                .map(|(k, v)| format!("{}={}", k, v))
                .collect();
            url = format!("{}?{}", url, query_string.join("&"));
        }

        // 构建请求
        let mut builder: RequestBuilder = self.http_client.request(method, &url);

        // 生成请求 ID
        let request_id = Self::generate_request_id();

        // 添加请求头
        builder = builder
            .header("Content-Type", "application/json")
            .header("User-Agent", &self.user_agent)
            .header("X-Request-ID", &request_id);

        // 添加 API Key 认证头
        if let Some(ref api_key) = self.api_key {
            builder = builder.bearer_auth(api_key);
        }

        // 添加自定义请求头
        for (key, value) in &options.headers {
            builder = builder.header(key, value);
        }

        // 添加请求体
        if let Some(data) = body {
            builder = builder.json(data);
        }

        // 设置超时
        if let Some(timeout) = options.timeout {
            builder = builder.timeout(timeout);
        }

        // 执行请求（带重试）
        let mut last_error: Option<AgentOSError> = None;
        for attempt in 0..=self.max_retries {
            if attempt > 0 {
                let delay = self.calculate_backoff(attempt);
                tokio::time::sleep(delay).await;
            }

            let result = builder.try_clone()
                .ok_or_else(|| AgentOSError::internal("无法克隆请求"))?
                .send()
                .await;

            match result {
                Ok(response) => {
                    let status = response.status();

                    // 检查响应状态码
                    if !status.is_success() {
                        let error_body = response.text().await.unwrap_or_default();
                        let error = AgentOSError::with_code(
                            http_status_to_code(status.as_u16()),
                            &error_body,
                        );

                        // 判断是否应该重试
                        if self.should_retry(status) {
                            last_error = Some(error);
                            continue;
                        }

                        return Err(error);
                    }

                    // 解析响应体
                    let response_text = response.text().await
                        .map_err(|e| AgentOSError::parse_error(&format!("读取响应失败: {}", e)))?;

                    let api_response: APIResponse = serde_json::from_str(&response_text)
                        .map_err(|e| AgentOSError::parse_error(&format!("解析响应失败: {}", e)))?;

                    return Ok(api_response);
                }
                Err(e) => {
                    let error = if e.is_timeout() {
                        AgentOSError::timeout(&format!("请求超时: {}", e))
                    } else if e.is_connect() {
                        AgentOSError::connection_refused(&format!("连接被拒绝: {}", e))
                    } else {
                        AgentOSError::network(&format!("网络错误: {}", e))
                    };

                    last_error = Some(error);

                    // 如果是超时或连接错误，继续重试
                    if e.is_timeout() || e.is_connect() {
                        continue;
                    }

                    return Err(error);
                }
            }
        }

        // 所有重试都失败
        Err(last_error.unwrap_or_else(|| AgentOSError::network("未知网络错误")))
    }

    /// 生成唯一的请求 ID
    fn generate_request_id() -> String {
        use std::time::{SystemTime, UNIX_EPOCH};
        use rand::Rng;

        let timestamp = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .map(|d| d.as_micros())
            .unwrap_or(0);
        let random: u32 = rand::thread_rng().gen_range(0..999999);

        format!("req-{}-{:06}", timestamp, random)
    }

    /// 计算指数退避延迟（含抖动）
    fn calculate_backoff(&self, attempt: u32) -> Duration {
        use rand::Rng;

        let base_ms = self.retry_delay.as_millis() as f64;
        let backoff_ms = base_ms * 2_f64.powi(attempt as i32 - 1);

        // 添加随机抖动（0-50%）
        let jitter = rand::thread_rng().gen_range(0.0..0.5) * backoff_ms;

        Duration::from_millis((backoff_ms + jitter) as u64)
    }

    /// 根据 HTTP 状态码判断是否应进行重试
    fn should_retry(&self, status: StatusCode) -> bool {
        status.is_server_error() || status == StatusCode::TOO_MANY_REQUESTS
    }
}

/// 实现 APIClient trait
#[async_trait::async_trait]
impl APIClient for Client {
    async fn get(&self, path: &str, opts: Option<Vec<RequestOption>>) -> Result<APIResponse, AgentOSError> {
        self.request_internal(reqwest::Method::GET, path, None, opts).await
    }

    async fn post(&self, path: &str, body: Option<&Value>, opts: Option<Vec<RequestOption>>) -> Result<APIResponse, AgentOSError> {
        self.request_internal(reqwest::Method::POST, path, body, opts).await
    }

    async fn put(&self, path: &str, body: Option<&Value>, opts: Option<Vec<RequestOption>>) -> Result<APIResponse, AgentOSError> {
        self.request_internal(reqwest::Method::PUT, path, body, opts).await
    }

    async fn delete(&self, path: &str, opts: Option<Vec<RequestOption>>) -> Result<APIResponse, AgentOSError> {
        self.request_internal(reqwest::Method::DELETE, path, None, opts).await
    }
}

/// 客户端构建器
pub struct ClientBuilder {
    endpoint: String,
    timeout: Duration,
    max_retries: u32,
    retry_delay: Duration,
    api_key: Option<String>,
    user_agent: String,
    max_connections: usize,
    idle_conn_timeout: Duration,
}

impl ClientBuilder {
    /// 创建新的构建器
    pub fn new(endpoint: &str) -> Self {
        let endpoint = if endpoint.is_empty() {
            "http://localhost:18789".to_string()
        } else {
            let trimmed = endpoint.trim_end_matches('/');
            // 不再使用 panic，而是在 build() 时返回错误
            trimmed.to_string()
        };

        Self {
            endpoint,
            timeout: Duration::from_secs(30),
            max_retries: 3,
            retry_delay: Duration::from_secs(1),
            api_key: None,
            user_agent: "AgentOS-Rust-tools/3.0.0".to_string(),
            max_connections: 10,
            idle_conn_timeout: Duration::from_secs(90),
        }
    }

    /// 设置请求超时时间
    pub fn timeout(mut self, timeout: Duration) -> Self {
        self.timeout = timeout;
        self
    }

    /// 设置最大重试次数
    pub fn max_retries(mut self, max_retries: u32) -> Self {
        self.max_retries = max_retries;
        self
    }

    /// 设置重试间隔
    pub fn retry_delay(mut self, retry_delay: Duration) -> Self {
        self.retry_delay = retry_delay;
        self
    }

    /// 设置 API 密钥
    pub fn api_key(mut self, api_key: &str) -> Self {
        self.api_key = Some(api_key.to_string());
        self
    }

    /// 设置 User-Agent
    pub fn user_agent(mut self, user_agent: &str) -> Self {
        self.user_agent = user_agent.to_string();
        self
    }

    /// 设置最大连接数
    pub fn max_connections(mut self, max_connections: usize) -> Self {
        self.max_connections = max_connections;
        self
    }

    /// 设置空闲连接超时时间
    pub fn idle_conn_timeout(mut self, idle_conn_timeout: Duration) -> Self {
        self.idle_conn_timeout = idle_conn_timeout;
        self
    }

    /// 构建客户端
    /// 
    /// # 错误
    /// 如果端点地址格式无效（不以 http:// 或 https:// 开头），返回错误
    pub fn build(self) -> Result<Client, AgentOSError> {
        // 验证端点格式
        if !self.endpoint.starts_with("http://") && !self.endpoint.starts_with("https://") {
            return Err(AgentOSError::manager(
                "端点地址必须以 http:// 或 https:// 开头"
            ));
        }

        let http_client = ReqwestClient::builder()
            .timeout(self.timeout)
            .pool_max_idle_per_host(self.max_connections)
            .pool_idle_timeout(Some(self.idle_conn_timeout))
            .build()
            .map_err(|e| AgentOSError::manager(&format!("创建 HTTP 客户端失败: {}", e)))?;

        Ok(Client {
            endpoint: self.endpoint,
            http_client,
            api_key: self.api_key,
            user_agent: self.user_agent,
            timeout: self.timeout,
            max_retries: self.max_retries,
            retry_delay: self.retry_delay,
        })
    }
}

