// Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
//
// HTTP client for AgentRT gateway communication.
// Simplified version for the TUI.

use anyhow::{Context, Result};
use reqwest::Client as HttpClient;
use serde::{Deserialize, Serialize};
use std::time::Duration;

/// Gateway API client for the TUI application.
pub struct GatewayClient {
    base_url: String,
    http: HttpClient,
}

impl GatewayClient {
    pub fn new(base_url: &str) -> Result<Self> {
        let http = HttpClient::builder()
            .timeout(Duration::from_secs(60))
            .user_agent(format!("agentrt-tui/{}", env!("CARGO_PKG_VERSION")))
            .build()
            .context("Failed to create HTTP client")?;

        Ok(Self {
            base_url: base_url.trim_end_matches('/').to_string(),
            http,
        })
    }

    pub async fn health_check(&self) -> Result<HealthResponse> {
        let url = format!("{}/api/v1/health", self.base_url);
        let resp = self.http.get(&url).send().await?;
        let body = resp.text().await?;
        serde_json::from_str(&body).context("Failed to parse health response")
    }

    pub async fn send_message(
        &self,
        prompt: &str,
        agent_file: &str,
    ) -> Result<RunResponse> {
        let url = format!("{}/api/v1/agent/run", self.base_url);
        let request = RunRequest {
            prompt: Some(prompt.to_string()),
            agent_file: agent_file.to_string(),
            model: None,
            interactive: true,
        };

        let resp = self.http.post(&url).json(&request).send().await?;
        let status = resp.status();
        let body = resp.text().await?;

        if !status.is_success() {
            anyhow::bail!("Gateway error: {}", body);
        }

        serde_json::from_str(&body).context("Failed to parse run response")
    }

    #[allow(dead_code)]
    pub async fn get_logs(&self, lines: u32) -> Result<Vec<LogEntry>> {
        let url = format!("{}/api/v1/logs?lines={}", self.base_url, lines);
        let resp = self.http.get(&url).send().await?;
        let body = resp.text().await?;
        serde_json::from_str(&body).context("Failed to parse logs")
    }

    #[allow(dead_code)]
    pub async fn get_memory_stats(&self) -> Result<String> {
        let url = format!("{}/api/v1/memory/stats", self.base_url);
        let resp = self.http.get(&url).send().await?;
        Ok(resp.text().await.unwrap_or_default())
    }

    #[allow(dead_code)]
    pub async fn get_plugins(&self) -> Result<String> {
        let url = format!("{}/api/v1/plugins", self.base_url);
        let resp = self.http.get(&url).send().await?;
        Ok(resp.text().await.unwrap_or_default())
    }
}

#[derive(Debug, Deserialize)]
#[allow(dead_code)]
pub struct HealthResponse {
    pub status: String,
    pub version: Option<String>,
    pub uptime_seconds: Option<u64>,
}

#[derive(Debug, Serialize)]
pub struct RunRequest {
    pub prompt: Option<String>,
    pub agent_file: String,
    pub model: Option<String>,
    pub interactive: bool,
}

#[derive(Debug, Deserialize)]
#[allow(dead_code)]
pub struct RunResponse {
    pub session_id: String,
    pub response: String,
    pub tokens_used: Option<u64>,
    pub cost_usd: Option<f64>,
}

#[derive(Debug, Deserialize)]
#[allow(dead_code)]
pub struct LogEntry {
    pub timestamp: String,
    pub level: String,
    pub message: String,
    pub daemon: Option<String>,
}