// AgentOS Rust SDK Syscall
// Version: 2.0.0
// Last updated: 2026-03-23

use serde::{Deserialize, Serialize};
use serde_json::Value;
use std::collections::HashMap;

use crate::AgentOSError;

type Result<T> = std::result::Result<T, AgentOSError>;

/// ﻝﺏﭨﻝﭨﻟﺍﻝ۷ﮒﺛﮒﻝ۸ﭦﻠﺑ
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum SyscallNamespace {
    #[serde(rename = "task")]
    Task,
    #[serde(rename = "memory")]
    Memory,
    #[serde(rename = "session")]
    Session,
    #[serde(rename = "telemetry")]
    Telemetry,
}

impl std::fmt::Display for SyscallNamespace {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            SyscallNamespace::Task => write!(f, "task"),
            SyscallNamespace::Memory => write!(f, "memory"),
            SyscallNamespace::Session => write!(f, "session"),
            SyscallNamespace::Telemetry => write!(f, "telemetry"),
        }
    }
}

/// ﻝﺏﭨﻝﭨﻟﺍﻝ۷ﻟﺁﺓﮔﺎ
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SyscallRequest {
    pub namespace: SyscallNamespace,
    pub operation: String,
    pub params: HashMap<String, Value>,
}

/// ﻝﺏﭨﻝﭨﻟﺍﻝ۷ﮒﮒﭦ
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SyscallResponse {
    pub success: bool,
    pub data: Option<Value>,
    pub error: Option<String>,
}

/// ﻝﺏﭨﻝﭨﻟﺍﻝ۷ﻝﭨﮒ؟ﮔﺛﻟﺎ۰ trait
pub trait SyscallBinding {
    /// ﮔ۶ﻟ۰ﻝﺏﭨﻝﭨﻟﺍﻝ۷
    fn invoke(&self, request: SyscallRequest) -> Result<SyscallResponse>;
}

/// ﻛﭨﭨﮒ۰ﻝﺏﭨﻝﭨﻟﺍﻝ۷
pub struct TaskSyscall<B: SyscallBinding> {
    binding: B,
}

impl<B: SyscallBinding> TaskSyscall<B> {
    /// ﮒﮒﭨﭦﻛﭨﭨﮒ۰ﻝﺏﭨﻝﭨﻟﺍﻝ۷
    pub fn new(binding: B) -> Self {
        TaskSyscall { binding }
    }

    /// ﮔﻛﭦ۳ﻛﭨﭨﮒ۰
    pub fn submit(&self, description: &str) -> Result<SyscallResponse> {
        let mut params = HashMap::new();
        params.insert("description".to_string(), Value::String(description.to_string()));
        self.binding.invoke(SyscallRequest {
            namespace: SyscallNamespace::Task,
            operation: "submit".to_string(),
            params,
        })
    }

    /// ﮔ۴ﻟﺁ۱ﻛﭨﭨﮒ۰
    pub fn query(&self, task_id: &str) -> Result<SyscallResponse> {
        let mut params = HashMap::new();
        params.insert("task_id".to_string(), Value::String(task_id.to_string()));
        self.binding.invoke(SyscallRequest {
            namespace: SyscallNamespace::Task,
            operation: "query".to_string(),
            params,
        })
    }

    /// ﮒﮔﭘﻛﭨﭨﮒ۰
    pub fn cancel(&self, task_id: &str) -> Result<SyscallResponse> {
        let mut params = HashMap::new();
        params.insert("task_id".to_string(), Value::String(task_id.to_string()));
        self.binding.invoke(SyscallRequest {
            namespace: SyscallNamespace::Task,
            operation: "cancel".to_string(),
            params,
        })
    }
}

/// ﻟ؟ﺍﮒﺟﻝﺏﭨﻝﭨﻟﺍﻝ۷
pub struct MemorySyscall<B: SyscallBinding> {
    binding: B,
}

impl<B: SyscallBinding> MemorySyscall<B> {
    /// ﮒﮒﭨﭦﻟ؟ﺍﮒﺟﻝﺏﭨﻝﭨﻟﺍﻝ۷
    pub fn new(binding: B) -> Self {
        MemorySyscall { binding }
    }

    /// ﮒﮒ۴ﻟ؟ﺍﮒﺟ
    pub fn write(&self, content: &str, metadata: Option<Value>) -> Result<SyscallResponse> {
        let mut params = HashMap::new();
        params.insert("content".to_string(), Value::String(content.to_string()));
        if let Some(meta) = metadata {
            params.insert("metadata".to_string(), meta);
        }
        self.binding.invoke(SyscallRequest {
            namespace: SyscallNamespace::Memory,
            operation: "write".to_string(),
            params,
        })
    }

    /// ﮔﻝﺑ۱ﻟ؟ﺍﮒﺟ
    pub fn search(&self, query: &str, top_k: u32) -> Result<SyscallResponse> {
        let mut params = HashMap::new();
        params.insert("query".to_string(), Value::String(query.to_string()));
        params.insert("top_k".to_string(), Value::Number(top_k.into()));
        self.binding.invoke(SyscallRequest {
            namespace: SyscallNamespace::Memory,
            operation: "search".to_string(),
            params,
        })
    }

    /// ﮒ ﻠ۳ﻟ؟ﺍﮒﺟ
    pub fn delete(&self, memory_id: &str) -> Result<SyscallResponse> {
        let mut params = HashMap::new();
        params.insert("memory_id".to_string(), Value::String(memory_id.to_string()));
        self.binding.invoke(SyscallRequest {
            namespace: SyscallNamespace::Memory,
            operation: "delete".to_string(),
            params,
        })
    }
}

/// ﻛﺙﻟﺁﻝﺏﭨﻝﭨﻟﺍﻝ۷
pub struct SessionSyscall<B: SyscallBinding> {
    binding: B,
}

impl<B: SyscallBinding> SessionSyscall<B> {
    /// ﮒﮒﭨﭦﻛﺙﻟﺁﻝﺏﭨﻝﭨﻟﺍﻝ۷
    pub fn new(binding: B) -> Self {
        SessionSyscall { binding }
    }

    /// ﮒﮒﭨﭦﻛﺙﻟﺁ
    pub fn create(&self) -> Result<SyscallResponse> {
        self.binding.invoke(SyscallRequest {
            namespace: SyscallNamespace::Session,
            operation: "create".to_string(),
            params: HashMap::new(),
        })
    }

    /// ﻟ؟ﺝﻝﺛ؟ﻛﺕﻛﺕﮔ?    pub fn set_context(&self, session_id: &str, key: &str, value: &str) -> Result<SyscallResponse> {
        let mut params = HashMap::new();
        params.insert("session_id".to_string(), Value::String(session_id.to_string()));
        params.insert("key".to_string(), Value::String(key.to_string()));
        params.insert("value".to_string(), Value::String(value.to_string()));
        self.binding.invoke(SyscallRequest {
            namespace: SyscallNamespace::Session,
            operation: "set_context".to_string(),
            params,
        })
    }

    /// ﻟﺓﮒﻛﺕﻛﺕﮔ?    pub fn get_context(&self, session_id: &str, key: &str) -> Result<SyscallResponse> {
        let mut params = HashMap::new();
        params.insert("session_id".to_string(), Value::String(session_id.to_string()));
        params.insert("key".to_string(), Value::String(key.to_string()));
        self.binding.invoke(SyscallRequest {
            namespace: SyscallNamespace::Session,
            operation: "get_context".to_string(),
            params,
        })
    }
}
