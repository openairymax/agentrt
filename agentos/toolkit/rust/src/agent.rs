// AgentOS Rust SDK Agent
// Version: 2.0.0
// Last updated: 2026-03-23

use crate::Client;

/// AgentOS 代理入口
#[derive(Debug, Clone)]
pub struct Agent {
    client: Client,
}

impl Agent {
    /// 创建新的 AgentOS 代理
    pub fn new(client: Client) -> Self {
        Agent { client }
    }

    /// 获取底层客户端引用
    pub fn client(&self) -> &Client {
        &self.client
    }

    /// 获取客户端克隆
    pub fn into_client(self) -> Client {
        self.client
    }

    /// 健康检查
    pub async fn health(&self) -> bool {
        self.client.health().await.is_ok()
    }

    /// 获取端点地址
    pub fn endpoint(&self) -> &str {
        self.client.endpoint()
    }
}
