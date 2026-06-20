// Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
//
// HTTP client for AgentRT gateway communication.
// All runtime commands go through the gateway REST API.

use anyhow::{Context, Result};
use reqwest::Client as HttpClient;
use serde::{Deserialize, Serialize};
use std::time::Duration;

/// Gateway API client for communicating with the AgentRT runtime.
pub struct GatewayClient {
    base_url: String,
    http: HttpClient,
}

impl GatewayClient {
    /// Create a new gateway client.
    pub fn new(base_url: &str) -> Result<Self> {
        let http = HttpClient::builder()
            .timeout(Duration::from_secs(30))
            .user_agent(format!("agentrt-cli/{}", env!("CARGO_PKG_VERSION")))
            .build()
            .context("Failed to create HTTP client")?;

        Ok(Self {
            base_url: base_url.trim_end_matches('/').to_string(),
            http,
        })
    }

    /// GET request with JSON response.
    pub async fn get<T: for<'de> Deserialize<'de>>(&self, path: &str) -> Result<T> {
        let url = format!("{}{}", self.base_url, path);
        let resp = self
            .http
            .get(&url)
            .send()
            .await
            .with_context(|| format!("GET {} failed", url))?;

        let status = resp.status();
        let body = resp.text().await.with_context(|| "Failed to read response body")?;

        if !status.is_success() {
            anyhow::bail!(
                "Gateway returned {} for {}: {}",
                status,
                url,
                truncate_body(&body)
            );
        }

        serde_json::from_str(&body)
            .with_context(|| format!("Failed to parse JSON from {}: {}", url, truncate_body(&body)))
    }

    /// POST request with JSON body and response.
    pub async fn post<T: for<'de> Deserialize<'de>>(
        &self,
        path: &str,
        body: &impl Serialize,
    ) -> Result<T> {
        let url = format!("{}{}", self.base_url, path);
        let resp = self
            .http
            .post(&url)
            .json(body)
            .send()
            .await
            .with_context(|| format!("POST {} failed", url))?;

        let status = resp.status();
        let text = resp.text().await.with_context(|| "Failed to read response body")?;

        if !status.is_success() {
            anyhow::bail!(
                "Gateway returned {} for {}: {}",
                status,
                url,
                truncate_body(&text)
            );
        }

        serde_json::from_str(&text)
            .with_context(|| format!("Failed to parse JSON from {}: {}", url, truncate_body(&text)))
    }

    /// Check if the gateway is reachable.
    pub async fn health_check(&self) -> Result<HealthResponse> {
        self.get("/api/v1/health").await
    }
}

fn truncate_body(body: &str) -> String {
    if body.len() > 200 {
        format!("{}... (truncated)", &body[..200])
    } else {
        body.to_string()
    }
}

// ─── API Response Types ────────────────────────────────────────────

#[derive(Debug, Deserialize)]
#[allow(dead_code)]
pub struct HealthResponse {
    pub status: String,
    pub version: Option<String>,
    pub uptime_seconds: Option<u64>,
    pub daemons: Option<Vec<DaemonStatus>>,
}

/// Status of a single daemon process.
#[derive(Debug, Deserialize)]
#[allow(dead_code)]
pub struct DaemonStatus {
    pub name: String,
    pub status: String,
    pub pid: Option<u32>,
}#[derive(Debug, Deserialize)]
pub struct LlmProvider {
    pub name: String,
    pub models: Vec<String>,
    pub status: String,
}

#[derive(Debug, Serialize)]
pub struct RunRequest {
    pub prompt: Option<String>,
    pub agent_file: String,
    pub model: Option<String>,
    pub interactive: bool,
}

#[derive(Debug, Deserialize)]
pub struct RunResponse {
    pub session_id: String,
    pub response: String,
    pub tokens_used: Option<u64>,
    pub cost_usd: Option<f64>,
}

#[derive(Debug, Serialize)]
#[allow(dead_code)]
pub struct ConfigSetRequest {
    pub key: String,
    pub value: serde_json::Value,
}

#[derive(Debug, Serialize)]
#[allow(dead_code)]
pub struct MarketSearchRequest {
    pub keyword: String,
}

#[derive(Debug, Deserialize)]
pub struct MarketSearchResult {
    pub name: String,
    pub version: String,
    pub description: String,
    pub author: String,
    pub downloads: u64,
}

#[derive(Debug, Serialize)]
pub struct MarketInstallRequest {
    pub package: String,
}

#[derive(Debug, Deserialize)]
#[allow(dead_code)]
pub struct MarketInstallResult {
    pub status: String,
    pub message: String,
    pub installed_path: Option<String>,
}

#[derive(Debug, Deserialize)]
pub struct DeployStatusResponse {
    pub status: String,
    pub version: String,
    pub daemons: Vec<DaemonInfo>,
    pub memory_usage_mb: Option<f64>,
    pub cpu_percent: Option<f64>,
}

#[derive(Debug, Deserialize)]
pub struct DaemonInfo {
    pub name: String,
    pub status: String,
    pub pid: u32,
    pub port: Option<u16>,
    pub uptime_seconds: Option<u64>,
}

#[derive(Debug, Deserialize)]
pub struct LogEntry {
    pub timestamp: String,
    pub level: String,
    pub message: String,
    pub daemon: Option<String>,
}

#[derive(Debug, Deserialize)]
pub struct CostResponse {
    pub total_tokens: u64,
    pub total_cost_usd: f64,
    pub by_provider: Option<Vec<ProviderCost>>,
}

#[derive(Debug, Deserialize)]
pub struct ProviderCost {
    pub provider: String,
    pub tokens: u64,
    pub cost_usd: f64,
}

#[derive(Debug, Serialize)]
pub struct LlmTestRequest {
    pub provider: String,
}