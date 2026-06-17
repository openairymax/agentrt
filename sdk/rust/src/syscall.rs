use serde::{Deserialize, Serialize};
use serde_json::Value;
use std::collections::HashMap;

use crate::AgentOSError;

type Result<T> = std::result::Result<T, AgentOSError>;

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum SyscallNamespace {
    #[serde(rename = "task")]
    Task,
    #[serde(rename = "memory")]
    Memory,
    #[serde(rename = "session")]
    Session,
    #[serde(rename = "skill")]
    Skill,
    #[serde(rename = "agent")]
    Agent,
    #[serde(rename = "telemetry")]
    Telemetry,
}

impl std::fmt::Display for SyscallNamespace {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            SyscallNamespace::Task => write!(f, "task"),
            SyscallNamespace::Memory => write!(f, "memory"),
            SyscallNamespace::Session => write!(f, "session"),
            SyscallNamespace::Skill => write!(f, "skill"),
            SyscallNamespace::Agent => write!(f, "agent"),
            SyscallNamespace::Telemetry => write!(f, "telemetry"),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SyscallRequest {
    pub namespace: SyscallNamespace,
    pub operation: String,
    pub params: HashMap<String, Value>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SyscallResponse {
    pub success: bool,
    pub data: Option<Value>,
    pub error: Option<String>,
}

pub trait SyscallBinding {
    fn invoke(&self, request: SyscallRequest) -> Result<SyscallResponse>;
}

pub struct HttpSyscallBinding {
    base_url: String,
    client: reqwest::blocking::Client,
}

impl HttpSyscallBinding {
    pub fn new(base_url: &str) -> Self {
        HttpSyscallBinding {
            base_url: base_url.trim_end_matches('/').to_string(),
            client: reqwest::blocking::Client::new(),
        }
    }

    pub fn with_timeout(base_url: &str, timeout: std::time::Duration) -> Self {
        HttpSyscallBinding {
            base_url: base_url.trim_end_matches('/').to_string(),
            client: reqwest::blocking::Client::builder()
                .timeout(timeout)
                .build()
                .unwrap_or_else(|_| reqwest::blocking::Client::new()),
        }
    }
}

impl SyscallBinding for HttpSyscallBinding {
    fn invoke(&self, request: SyscallRequest) -> Result<SyscallResponse> {
        let url = format!("{}/api/v1/syscall/{}", self.base_url, request.namespace);
        let body = serde_json::to_value(&request)
            .map_err(|e| AgentOSError::json(&e.to_string()))?;

        let resp = self.client
            .post(&url)
            .json(&body)
            .send()
            .map_err(|e| AgentOSError::network(&e.to_string()))?;

        let status = resp.status();
        if !status.is_success() {
            return Err(AgentOSError::http(&format!("Syscall HTTP {}: {}", status, request.operation)));
        }

        let syscall_resp: SyscallResponse = resp
            .json()
            .map_err(|e| AgentOSError::json(&e.to_string()))?;

        Ok(syscall_resp)
    }
}

pub struct TaskSyscall<B: SyscallBinding> {
    binding: B,
}

impl<B: SyscallBinding> TaskSyscall<B> {
    pub fn new(binding: B) -> Self {
        TaskSyscall { binding }
    }

    pub fn submit(&self, description: &str) -> Result<SyscallResponse> {
        let mut params = HashMap::new();
        params.insert("description".to_string(), Value::String(description.to_string()));
        self.binding.invoke(SyscallRequest {
            namespace: SyscallNamespace::Task,
            operation: "submit".to_string(),
            params,
        })
    }

    pub fn query(&self, task_id: &str) -> Result<SyscallResponse> {
        let mut params = HashMap::new();
        params.insert("task_id".to_string(), Value::String(task_id.to_string()));
        self.binding.invoke(SyscallRequest {
            namespace: SyscallNamespace::Task,
            operation: "query".to_string(),
            params,
        })
    }

    pub fn wait(&self, task_id: &str, timeout_ms: u32) -> Result<SyscallResponse> {
        let mut params = HashMap::new();
        params.insert("task_id".to_string(), Value::String(task_id.to_string()));
        params.insert("timeout_ms".to_string(), Value::Number(timeout_ms.into()));
        self.binding.invoke(SyscallRequest {
            namespace: SyscallNamespace::Task,
            operation: "wait".to_string(),
            params,
        })
    }

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

pub struct MemorySyscall<B: SyscallBinding> {
    binding: B,
}

impl<B: SyscallBinding> MemorySyscall<B> {
    pub fn new(binding: B) -> Self {
        MemorySyscall { binding }
    }

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

    pub fn read(&self, memory_id: &str) -> Result<SyscallResponse> {
        let mut params = HashMap::new();
        params.insert("memory_id".to_string(), Value::String(memory_id.to_string()));
        self.binding.invoke(SyscallRequest {
            namespace: SyscallNamespace::Memory,
            operation: "read".to_string(),
            params,
        })
    }

    pub fn get(&self, memory_id: &str) -> Result<SyscallResponse> {
        let mut params = HashMap::new();
        params.insert("memory_id".to_string(), Value::String(memory_id.to_string()));
        self.binding.invoke(SyscallRequest {
            namespace: SyscallNamespace::Memory,
            operation: "get".to_string(),
            params,
        })
    }

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

pub struct SessionSyscall<B: SyscallBinding> {
    binding: B,
}

impl<B: SyscallBinding> SessionSyscall<B> {
    pub fn new(binding: B) -> Self {
        SessionSyscall { binding }
    }

    pub fn create(&self) -> Result<SyscallResponse> {
        self.binding.invoke(SyscallRequest {
            namespace: SyscallNamespace::Session,
            operation: "create".to_string(),
            params: HashMap::new(),
        })
    }

    pub fn get(&self, session_id: &str) -> Result<SyscallResponse> {
        let mut params = HashMap::new();
        params.insert("session_id".to_string(), Value::String(session_id.to_string()));
        self.binding.invoke(SyscallRequest {
            namespace: SyscallNamespace::Session,
            operation: "get".to_string(),
            params,
        })
    }

    pub fn close(&self, session_id: &str) -> Result<SyscallResponse> {
        let mut params = HashMap::new();
        params.insert("session_id".to_string(), Value::String(session_id.to_string()));
        self.binding.invoke(SyscallRequest {
            namespace: SyscallNamespace::Session,
            operation: "close".to_string(),
            params,
        })
    }

    pub fn list(&self) -> Result<SyscallResponse> {
        self.binding.invoke(SyscallRequest {
            namespace: SyscallNamespace::Session,
            operation: "list".to_string(),
            params: HashMap::new(),
        })
    }

    pub fn set_context(&self, session_id: &str, key: &str, value: &str) -> Result<SyscallResponse> {
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

    pub fn get_context(&self, session_id: &str, key: &str) -> Result<SyscallResponse> {
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

pub struct SkillSyscall<B: SyscallBinding> {
    binding: B,
}

impl<B: SyscallBinding> SkillSyscall<B> {
    pub fn new(binding: B) -> Self {
        SkillSyscall { binding }
    }

    pub fn load(&self, skill_name: &str) -> Result<SyscallResponse> {
        let mut params = HashMap::new();
        params.insert("skill_name".to_string(), Value::String(skill_name.to_string()));
        self.binding.invoke(SyscallRequest {
            namespace: SyscallNamespace::Skill,
            operation: "load".to_string(),
            params,
        })
    }

    pub fn execute(&self, skill_id: &str, params_value: Value) -> Result<SyscallResponse> {
        let mut params = HashMap::new();
        params.insert("skill_id".to_string(), Value::String(skill_id.to_string()));
        params.insert("params".to_string(), params_value);
        self.binding.invoke(SyscallRequest {
            namespace: SyscallNamespace::Skill,
            operation: "execute".to_string(),
            params,
        })
    }

    pub fn unload(&self, skill_id: &str) -> Result<SyscallResponse> {
        let mut params = HashMap::new();
        params.insert("skill_id".to_string(), Value::String(skill_id.to_string()));
        self.binding.invoke(SyscallRequest {
            namespace: SyscallNamespace::Skill,
            operation: "unload".to_string(),
            params,
        })
    }

    pub fn list(&self) -> Result<SyscallResponse> {
        self.binding.invoke(SyscallRequest {
            namespace: SyscallNamespace::Skill,
            operation: "list".to_string(),
            params: HashMap::new(),
        })
    }
}

pub struct AgentSyscall<B: SyscallBinding> {
    binding: B,
}

impl<B: SyscallBinding> AgentSyscall<B> {
    pub fn new(binding: B) -> Self {
        AgentSyscall { binding }
    }

    pub fn spawn(&self, spec: Value) -> Result<SyscallResponse> {
        let mut params = HashMap::new();
        params.insert("spec".to_string(), spec);
        self.binding.invoke(SyscallRequest {
            namespace: SyscallNamespace::Agent,
            operation: "spawn".to_string(),
            params,
        })
    }

    pub fn terminate(&self, agent_id: &str) -> Result<SyscallResponse> {
        let mut params = HashMap::new();
        params.insert("agent_id".to_string(), Value::String(agent_id.to_string()));
        self.binding.invoke(SyscallRequest {
            namespace: SyscallNamespace::Agent,
            operation: "terminate".to_string(),
            params,
        })
    }

    pub fn invoke(&self, agent_id: &str, method: &str, args: Value) -> Result<SyscallResponse> {
        let mut params = HashMap::new();
        params.insert("agent_id".to_string(), Value::String(agent_id.to_string()));
        params.insert("method".to_string(), Value::String(method.to_string()));
        params.insert("args".to_string(), args);
        self.binding.invoke(SyscallRequest {
            namespace: SyscallNamespace::Agent,
            operation: "invoke".to_string(),
            params,
        })
    }
}
