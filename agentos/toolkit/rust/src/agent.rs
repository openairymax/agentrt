// AgentOS Rust SDK Agent
// Version: 2.0.0
// Last updated: 2026-03-23

use crate::Client;

/// AgentOS ﻛﭨ۲ﻝﮒ۴ﮒ۲
#[derive(Debug, Clone)]
pub struct Agent {
    client: Client,
}

impl Agent {
    /// ﮒﮒﭨﭦﮔﺍﻝ AgentOS ﻛﭨ۲ﻝ
    pub fn new(client: Client) -> Self {
        Agent { client }
    }

    /// ﻟﺓﮒﮒﭦﮒﺎﮒ؟۱ﮔﺓﻝ،ﺁﮒﺙﻝ?    pub fn client(&self) -> &Client {
        &self.client
    }

    /// ﻟﺓﮒﮒ؟۱ﮔﺓﻝ،ﺁﮒﻠ?    pub fn into_client(self) -> Client {
        self.client
    }

    /// ﮒ۴ﮒﭦﺓﮔ۲ﮔ?    pub async fn health(&self) -> bool {
        self.client.health().await.unwrap_or(false)
    }

    /// ﻟﺓﮒﻝ،ﺁﻝﺗﮒﺍﮒ
    pub fn endpoint(&self) -> &str {
        self.client.endpoint()
    }
}
