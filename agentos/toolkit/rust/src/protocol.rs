// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: MIT
//! @file protocol.rs
//! @brief AgentOS Rust SDK — Protocol Client Module
//!
//! 提供多协议客户端支持，允许通过统一接口与不同协议后端交互：
//! - JSON-RPC 2.0 (默认)
//! - MCP (Model Context Protocol) v1.0
//! - A2A (Agent-to-Agent) v0.3
//! - OpenAI API 兼容

use crate::error::{Error, ErrorCode};
use crate::types::Client;
use async_trait::async_trait;
use reqwest::{header, Client as ReqwestClient, Response, StatusCode};
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::sync::Arc;
use std::time::Duration;

/// Supported protocol types for AgentOS communication
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum ProtocolType {
    JsonRpc,
    Mcp,
    A2a,
    OpenAi,
    AutoDetect,
}

impl std::fmt::Display for ProtocolType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            ProtocolType::JsonRpc => write!(f, "jsonrpc"),
            ProtocolType::Mcp => write!(f, "mcp"),
            ProtocolType::A2a => write!(f, "a2a"),
            ProtocolType::OpenAi => write!(f, "openai"),
            ProtocolType::AutoDetect => write!(f, "auto"),
        }
    }
}

/// Configuration for a protocol client
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProtocolConfig {
    pub protocol_type: ProtocolType,
    pub endpoint: String,
    pub api_key: Option<String>,
    pub timeout: Duration,
    pub retry_count: u32,
    pub retry_delay: Duration,
    pub enable_streaming: bool,
    #[serde(skip)]
    pub extra_headers: HashMap<String, String>,
}

impl Default for ProtocolConfig {
    fn default() -> Self {
        Self {
            protocol_type: ProtocolType::JsonRpc,
            endpoint: "http://localhost:18789".to_string(),
            api_key: None,
            timeout: Duration::from_secs(30),
            retry_count: 3,
            retry_delay: Duration::from_secs(1),
            enable_streaming: false,
            extra_headers: HashMap::new(),
        }
    }
}

/// Result of automatic protocol detection
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DetectionResult {
    pub detected_type: ProtocolType,
    pub type_name: String,
    pub confidence: f64,
    pub is_streaming: bool,
    pub has_binary_payload: bool,
}

/// Result of a connection test to a protocol endpoint
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ConnectionTestResult {
    pub protocol: String,
    pub status: String,
    pub status_code: Option<u16>,
    pub latency_ms: Option<f64>,
    pub error: Option<String>,
}

/// Unified multi-protocol client for AgentOS
pub struct ProtocolClient {
    config: ProtocolConfig,
    http_client: ReqwestClient,
    stats: Arc<std::sync::Mutex<ProtocolStats>>,
}

#[derive(Debug, Default)]
struct ProtocolStats {
    requests_sent: u64,
    transformations: u64,
}

impl ProtocolClient {
    /// Create a new protocol client with given configuration
    pub fn new(config: ProtocolConfig) -> Result<Self, Error> {
        let http_client = ReqwestClient::builder()
            .timeout(config.timeout)
            .build()
            .map_err(|e| Error::NetworkError(e.to_string()))?;

        Ok(Self {
            config,
            http_client,
            stats: Arc::new(std::sync::Mutex::new(ProtocolStats::default())),
        })
    }

    /// Create from an existing AgentOS client reference
    pub fn from_client(client: &Client) -> Result<Self, Error> {
        let config = ProtocolConfig {
            endpoint: client.endpoint().to_string(),
            api_key: None,
            ..Default::default()
        };
        Self::new(config)
    }

    /// Auto-detect the appropriate protocol by querying the gateway
    pub async fn detect_protocol(&self) -> Result<DetectionResult, Error> {
        let url = format!("{}/api/v1/protocols", self.config.endpoint);
        let resp = self.http_client.get(&url)
            .send()
            .await
            .map_err(|e| Error::NetworkError(format!("detect request failed: {}", e)))?;

        let content_type = resp.headers()
            .get(header::CONTENT_TYPE)
            .and_then(|v| v.to_str().ok())
            .unwrap_or("")
            .to_string();

        let body = resp.text().await.unwrap_or_default();
        let mut confidence = 50.0_f64;
        let detected = ProtocolType::JsonRpc;

        if content_type.contains("application/json") {
            confidence += 20.0;
            if body.contains("\"tools/call\"") || body.contains("\"method\":\"tools/list\"") {
                detected = ProtocolType::Mcp;
                confidence += 30.0;
            } else if body.contains("\"task/delegate\"") || body.contains("\"agent/discover\"") {
                detected = ProtocolType::A2a;
                confidence += 30.0;
            } else if body.contains("\"model\"") && body.contains("\"choices\"") {
                detected = ProtocolType::OpenAi;
                confidence += 25.0;
            } else if body.contains("\"jsonrpc\"") {
                detected = ProtocolType::JsonRpc;
                confidence += 40.0;
            }
        }

        Ok(DetectionResult {
            detected_type: detected,
            type_name: format!("{}", detected),
            confidence: confidence.min(100.0),
            is_streaming: content_type.contains("event-stream"),
            has_binary_payload: false,
        })
    }

    /// Send a unified request through the configured protocol
    pub async fn send_request(
        &self,
        method: &str,
        params: &serde_json::Value,
    ) -> Result<serde_json::Value, Error> {
        let payload = self.build_request_payload(method, params);
        let url_path = self.get_url_path();
        let url = format!("{}{}", self.config.endpoint, url_path);

        let mut req_builder = self.http_client.post(&url).json(&payload);
        self.apply_headers(&mut req_builder);

        let mut last_error = None;
        let mut delay = self.config.retry_delay;

        for attempt in 0..=self.config.retry_count {
            if attempt > 0 {
                tokio::time::sleep(delay).await;
                delay *= 2;
            }

            match req_builder.try_clone().unwrap().send().await {
                Ok(resp) => {
                    if !resp.status().is_success() {
                        let status = resp.status();
                        let body = resp.text().await.unwrap_or_default();
                        return Err(Error::HttpError(status.as_u16(), body));
                    }
                    let body: serde_json::Value =
                        resp.json().await.map_err(|e| Error::ParseError(e.to_string()))?;
                    {
                        let mut stats = self.stats.lock().unwrap();
                        stats.requests_sent += 1;
                        if self.config.protocol_type != ProtocolType::JsonRpc {
                            stats.transformations += 1;
                        }
                    }
                    return Ok(body);
                }
                Err(e) => {
                    if e.is_timeout() || e.is_connect() {
                        last_error = Some(Error::TimeoutError(e.to_string()));
                    } else {
                        last_error = Some(Error::NetworkError(e.to_string()));
                        break;
                    }
                }
            }
        }

        Err(last_error.unwrap_or(Error::UnknownError("request failed".into())))
    }

    /// Send a streaming request and deliver chunks via callback
    pub async fn stream_request<F>(
        &self,
        method: &str,
        params: &serde_json::Value,
        mut on_chunk: F,
    ) -> Result<(), Error>
    where
        F: FnMut(bytes::Vec) + Send + 'static,
    {
        if !self.config.enable_streaming {
            let result = self.send_request(method, params).await?;
            on_chunk(serde_json::to_vec(&result).unwrap_or_default());
            return Ok(());
        }

        let payload = self.build_request_payload(method, params);
        let url = format!("{}/rpc", self.config.endpoint);

        let mut req_builder = self.http_client.post(&url)
            .json(&payload);
        self.apply_headers(&mut req_builder);

        let resp = req_builder.send().await
            .map_err(|e| Error::NetworkError(format!("stream request: {}", e)))?;

        use futures_util::StreamExt;
        let mut byte_stream = resp.bytes_stream();

        while let Some(chunk_result) = byte_stream.next().await {
            match chunk_result {
                Ok(chunk) => on_chunk(chunk.to_vec()),
                Err(e) => return Err(Error::StreamError(e.to_string())),
            }
        }

        Ok(())
    }

    /// List available protocols from the gateway
    pub async fn list_protocols(&self) -> Result<Vec<String>, Error> {
        let url = format!("{}/api/v1/protocols", self.config.endpoint);
        let resp = self.http_client.get(&url)
            .send().await
            .map_err(|e| Error::NetworkError(e.to_string()))?;

        let data: serde_json::Value = resp.json().await
            .map_err(|e| Error::ParseError(e.to_string()))?;

        let protocols = data["protocols"].as_array()
            .map(|arr| arr.iter().filter_map(|v| v.as_str()).map(String::from).collect())
            .unwrap_or_default();

        Ok(protocols)
    }

    /// Test connectivity to a specific protocol endpoint
    pub async fn test_connection(
        &self,
        protocol_name: &str,
    ) -> Result<ConnectionTestResult, Error> {
        let url = format!(
            "{}/api/v1/protocols/{}/test",
            self.config.endpoint, protocol_name
        );
        let start = std::time::Instant::now();

        let result = self.http_client.get(&url)
            .send()
            .await;

        let latency_ms = start.elapsed().as_secs_f64() * 1000.0;

        match result {
            Ok(resp) => Ok(ConnectionTestResult {
                protocol: protocol_name.to_string(),
                status: if resp.status().is_success() { "ok".into() } else { "error".into() },
                status_code: Some(resp.status().as_u16()),
                latency_ms: Some(latency_ms.round(2)),
                error: None,
            }),
            Err(e) => Ok(ConnectionTestResult {
                protocol: protocol_name.to_string(),
                status: "error".into(),
                latency_ms: Some(latency_ms.round(2)),
                error: Some(e.to_string()),
            }),
        }
    }

    /// Get capabilities of a specific protocol adapter
    pub async fn get_capabilities(
        &self,
        protocol_name: &str,
    ) -> Result<serde_json::Value, Error> {
        let url = format!(
            "{}/api/v1/protocols/{}/capabilities",
            self.config.endpoint, protocol_name
        );
        let resp = self.http_client.get(&url)
            .send().await
            .map_err(|e| Error::NetworkError(e.to_string()))?;

        resp.json().await
            .map_err(|e| Error::ParseError(e.to_string()))
    }

    /// Get internal statistics about this client instance
    pub fn get_stats(&self) -> ProtocolStats {
        self.stats.lock().unwrap().clone()
    }

    // ======================================================================
    // Internal helpers
    // ======================================================================

    fn apply_headers<'a>(&self, builder: &'a mut reqwest::RequestBuilder) {
        let headers = builder.headers_mut().unwrap();
        headers.insert(header::CONTENT_TYPE, "application/json".parse().unwrap());
        headers.insert(header::ACCEPT, "application/json".parse().unwrap());

        if let Some(ref key) = self.config.api_key {
            headers.insert(
                header::AUTHORIZATION,
                format!("Bearer {}", key).parse().unwrap(),
            );
        }

        for (k, v) in &self.config.extra_headers {
            if let (Ok(name), Ok(value)) = (
                header::HeaderName::from_bytes(k.as_bytes()),
                header::HeaderValue::from_bytes(v.as_bytes()),
            ) {
                headers.insert(name, value);
            }
        }
    }

    fn get_url_path(&self) -> &'static str {
        match self.config.protocol_type {
            ProtocolType::OpenAi => "/v1/chat/completions",
            ProtocolType::Mcp | ProtocolType::A2a => "/api/v1/invoke",
            _ => "/rpc",
        }
    }

    fn build_request_payload(
        &self,
        method: &str,
        params: &serde_json::Value,
    ) -> serde_json::Value {
        match self.config.protocol_type {
            ProtocolType::OpenAi => {
                let model = params.get("model")
                    .and_then(|v| v.as_str())
                    .unwrap_or("gpt-4o");
                let messages = params.get("messages").cloned().unwrap_or(serde_json::Value::Array(vec![]));
                let temperature = params.get("temperature").and_then(|v| v.as_f64()).unwrap_or(0.7);
                let max_tokens = params.get("max_tokens").and_then(|v| v.as_i64()).unwrap_or(2048);

                json!({
                    "model": model,
                    "messages": messages,
                    "temperature": temperature,
                    "max_tokens": max_tokens,
                    "stream": self.config.enable_streaming,
                })
            }
            ProtocolType::Mcp => {
                let mcp_method = method.rsplit('.').next().unwrap_or(method);
                json!({
                    "protocol": "mcp",
                    "version": "1.0",
                    "method": mcp_method,
                    "params": params,
                })
            }
            ProtocolType::A2a => {
                let mapping: &[(&str, &str)] = &[
                    ("agent.list", "agent/discover"),
                    ("task.create", "task/delegate"),
                ];
                let a2a_method = mapping.iter()
                    .find(|(k, _)| *k == method)
                    .map(|(_, v)| *v)
                    .unwrap_or(method);
                json!({
                    "protocol": "a2a",
                    "version": "0.3",
                    "method": a2a_method,
                    "params": params,
                })
            }
            _ => json!({
                "jsonrpc": "2.0",
                "id": format!("req_{}", chrono::Utc::now().timestamp_millis()),
                "method": method,
                "params": params,
            }),
        }
    }
}
