// AgentRT Rust SDK - Hook Trait
// Version: 0.1.0
// Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
//
// Hook trait for implementing lifecycle event interceptors.
// Corresponds to ecosystem/hooks/__init__.py in the Python ecosystem.

use std::collections::HashMap;
use serde::{Serialize, Deserialize};

/// Context passed to hook callbacks.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HookContext {
    pub event: String,
    #[serde(default)]
    pub agent_id: String,
    #[serde(default)]
    pub session_id: String,
    #[serde(default)]
    pub timestamp: String,
    #[serde(default)]
    pub metadata: HashMap<String, serde_json::Value>,
}

impl HookContext {
    pub fn new(event: &str) -> Self {
        Self {
            event: event.to_string(),
            agent_id: String::new(),
            session_id: String::new(),
            timestamp: chrono::Utc::now().to_rfc3339(),
            metadata: HashMap::new(),
        }
    }
}

/// Result returned by a hook callback.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HookResult {
    #[serde(default = "default_true")]
    pub allowed: bool,
    pub modified_data: Option<serde_json::Value>,
    #[serde(default)]
    pub messages: Vec<String>,
    #[serde(default)]
    pub metadata: HashMap<String, serde_json::Value>,
}

fn default_true() -> bool { true }

impl HookResult {
    pub fn allow() -> Self {
        Self {
            allowed: true,
            modified_data: None,
            messages: Vec::new(),
            metadata: HashMap::new(),
        }
    }

    pub fn deny(reason: &str) -> Self {
        Self {
            allowed: false,
            modified_data: None,
            messages: vec![reason.to_string()],
            metadata: HashMap::new(),
        }
    }

    pub fn with_modified_data(data: serde_json::Value) -> Self {
        Self {
            allowed: true,
            modified_data: Some(data),
            messages: Vec::new(),
            metadata: HashMap::new(),
        }
    }
}

/// Hook trait for implementing lifecycle event interceptors.
///
/// Implement this trait to create custom hooks that intercept
/// AgentRT runtime events. Each method corresponds to a specific
/// lifecycle event.
///
/// # Example
///
/// ```rust
/// use agentrt_rs::hook::{Hook, HookContext, HookResult};
///
/// struct AuditHook;
///
/// impl Hook for AuditHook {
///     fn name(&self) -> &str { "audit" }
///     fn priority(&self) -> u8 { 100 }
///
///     fn on_tool_call(
///         &mut self,
///         ctx: &HookContext,
///         tool_name: &str,
///         tool_input: Option<&serde_json::Value>,
///     ) -> HookResult {
///         println!("Tool called: {}", tool_name);
///         HookResult::allow()
///     }
/// }
/// ```
pub trait Hook: Send + Sync {
    /// Unique name for this hook.
    fn name(&self) -> &str;

    /// Hook version.
    fn version(&self) -> &str {
        "0.1.0"
    }

    /// Priority (0=lowest, 100=highest). Higher priority hooks run first.
    fn priority(&self) -> u8 {
        50
    }

    /// Whether this hook is enabled.
    fn enabled(&self) -> bool {
        true
    }

    /// Return the list of event names this hook listens to.
    fn events(&self) -> Vec<String> {
        vec![]
    }

    // ── Agent lifecycle ────────────────────────────────

    /// Called when an agent starts execution.
    fn on_agent_start(
        &mut self,
        _ctx: &HookContext,
        _data: Option<&serde_json::Value>,
    ) -> HookResult {
        HookResult::allow()
    }

    /// Called when an agent finishes execution.
    fn on_agent_end(
        &mut self,
        _ctx: &HookContext,
        _data: Option<&serde_json::Value>,
    ) -> HookResult {
        HookResult::allow()
    }

    // ── Tool events ────────────────────────────────────

    /// Called before a tool is invoked.
    fn on_tool_call(
        &mut self,
        _ctx: &HookContext,
        _tool_name: &str,
        _tool_input: Option<&serde_json::Value>,
    ) -> HookResult {
        HookResult::allow()
    }

    /// Called after a tool returns a result.
    fn on_tool_result(
        &mut self,
        _ctx: &HookContext,
        _tool_name: &str,
        _result: Option<&serde_json::Value>,
    ) -> HookResult {
        HookResult::allow()
    }

    // ── LLM events ─────────────────────────────────────

    /// Called before sending a request to LLM.
    fn on_llm_request(
        &mut self,
        _ctx: &HookContext,
        _messages: Option<&serde_json::Value>,
        _model: &str,
    ) -> HookResult {
        HookResult::allow()
    }

    /// Called after receiving a response from LLM.
    fn on_llm_response(
        &mut self,
        _ctx: &HookContext,
        _response: Option<&serde_json::Value>,
        _usage: Option<&HashMap<String, u64>>,
    ) -> HookResult {
        HookResult::allow()
    }

    // ── Memory events ──────────────────────────────────

    /// Called when reading from memory.
    fn on_memory_read(
        &mut self,
        _ctx: &HookContext,
        _key: &str,
        _layer: &str,
    ) -> HookResult {
        HookResult::allow()
    }

    /// Called when writing to memory.
    fn on_memory_write(
        &mut self,
        _ctx: &HookContext,
        _key: &str,
        _data: Option<&serde_json::Value>,
    ) -> HookResult {
        HookResult::allow()
    }

    /// Get hook metadata for registration.
    fn get_hook_info(&self) -> HashMap<String, serde_json::Value> {
        let mut info = HashMap::new();
        info.insert("name".to_string(), serde_json::Value::String(self.name().to_string()));
        info.insert("version".to_string(), serde_json::Value::String(self.version().to_string()));
        info.insert("priority".to_string(), serde_json::Value::Number(self.priority().into()));
        info.insert("enabled".to_string(), serde_json::Value::Bool(self.enabled()));
        info
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    struct TestHook {
        called: bool,
    }

    impl Hook for TestHook {
        fn name(&self) -> &str {
            "test_hook"
        }

        fn on_tool_call(
            &mut self,
            _ctx: &HookContext,
            tool_name: &str,
            _tool_input: Option<&serde_json::Value>,
        ) -> HookResult {
            self.called = true;
            if tool_name == "dangerous_tool" {
                HookResult::deny("Blocked for testing")
            } else {
                HookResult::allow()
            }
        }
    }

    #[test]
    fn test_hook_default_behavior() {
        let mut hook = TestHook { called: false };
        let ctx = HookContext::new("test_event");

        let result = hook.on_agent_start(&ctx, None);
        assert!(result.allowed);

        // should not have called on_tool_call
        assert!(!hook.called);
    }

    #[test]
    fn test_hook_block_tool() {
        let mut hook = TestHook { called: false };
        let ctx = HookContext::new("on_tool_call");

        let result = hook.on_tool_call(&ctx, "dangerous_tool", None);
        assert!(!result.allowed);
        assert_eq!(result.messages, vec!["Blocked for testing"]);
        assert!(hook.called);
    }

    #[test]
    fn test_hook_allow_tool() {
        let mut hook = TestHook { called: false };
        let ctx = HookContext::new("on_tool_call");

        let result = hook.on_tool_call(&ctx, "safe_tool", None);
        assert!(result.allowed);
        assert!(hook.called);
    }

    #[test]
    fn test_hook_info() {
        let hook = TestHook { called: false };
        let info = hook.get_hook_info();
        assert_eq!(info["name"], "test_hook");
        assert_eq!(info["version"], "0.1.0");
    }
}