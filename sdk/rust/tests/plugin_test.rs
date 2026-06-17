use agentos_rs::plugin::{
    PluginState, PluginType, PluginManifest, BasePlugin, PluginRegistry, PluginManager,
};
use agentos_rs::hook::{Hook, HookContext, HookResult};

use std::collections::HashMap;

// ─── Test Plugin ─────────────────────────────────────────

struct TestPlugin {
    plugin_id: String,
    loaded: bool,
    activated: bool,
}

impl TestPlugin {
    fn new(name: &str) -> Self {
        Self {
            plugin_id: name.to_string(),
            loaded: false,
            activated: false,
        }
    }
}

impl BasePlugin for TestPlugin {
    fn plugin_id(&self) -> &str {
        &self.plugin_id
    }

    fn set_plugin_id(&mut self, id: &str) {
        self.plugin_id = id.to_string();
    }

    fn on_load(&mut self, _context: &HashMap<String, serde_json::Value>) -> Result<(), String> {
        self.loaded = true;
        Ok(())
    }

    fn on_activate(&mut self, _context: &HashMap<String, serde_json::Value>) -> Result<(), String> {
        self.activated = true;
        Ok(())
    }

    fn on_deactivate(&mut self) -> Result<(), String> {
        Ok(())
    }

    fn on_unload(&mut self) -> Result<(), String> {
        Ok(())
    }

    fn on_error(&mut self, _error: &str) {}

    fn get_capabilities(&self) -> Vec<String> {
        vec!["test".to_string(), "benchmark".to_string()]
    }
}

// ─── Plugin Registry Tests ───────────────────────────────

#[cfg(test)]
mod plugin_registry_tests {
    use super::*;

    #[test]
    fn test_plugin_registry_create() {
        let registry = PluginRegistry::new();
        assert!(registry.list_plugins().is_empty());
    }

    #[test]
    fn test_plugin_register_and_discover() {
        let mut registry = PluginRegistry::new();
        let pid = registry.register(
            Box::new(|| Box::new(TestPlugin::new("test-plugin"))),
            Some(PluginManifest {
                plugin_id: "test-plugin".to_string(),
                ..PluginManifest::new("test-plugin", "Test Plugin")
            }),
        ).unwrap();
        assert_eq!(pid, "test-plugin");
        assert_eq!(registry.list_plugins().len(), 1);

        let discovered = registry.discover(None);
        assert_eq!(discovered.len(), 1);
    }

    #[test]
    fn test_plugin_register_duplicate() {
        let mut registry = PluginRegistry::new();
        registry.register(
            Box::new(|| Box::new(TestPlugin::new("dup-plugin"))),
            Some(PluginManifest::new("dup-plugin", "Dup Plugin")),
        ).unwrap();
        let result = registry.register(
            Box::new(|| Box::new(TestPlugin::new("dup-plugin"))),
            Some(PluginManifest::new("dup-plugin", "Dup Plugin")),
        );
        assert!(result.is_err(), "duplicate registration should fail");
    }

    #[test]
    fn test_plugin_discover_by_capability() {
        let mut registry = PluginRegistry::new();
        registry.register(
            Box::new(|| Box::new(TestPlugin::new("plugin-a"))),
            Some(PluginManifest {
                plugin_id: "plugin-a".to_string(),
                capabilities: vec!["test".to_string()],
                ..PluginManifest::new("plugin-a", "Plugin A")
            }),
        ).unwrap();
        registry.register(
            Box::new(|| Box::new(TestPlugin::new("plugin-b"))),
            Some(PluginManifest {
                plugin_id: "plugin-b".to_string(),
                capabilities: vec!["test".to_string()],
                ..PluginManifest::new("plugin-b", "Plugin B")
            }),
        ).unwrap();

        let found = registry.discover(Some("test"));
        assert_eq!(found.len(), 2);
    }

    #[test]
    fn test_plugin_discover_no_match() {
        let mut registry = PluginRegistry::new();
        registry.register(
            Box::new(|| Box::new(TestPlugin::new("test-plugin"))),
            Some(PluginManifest {
                plugin_id: "test-plugin".to_string(),
                capabilities: vec!["test".to_string()],
                ..PluginManifest::new("test-plugin", "Test Plugin")
            }),
        ).unwrap();

        let found = registry.discover(Some("nonexistent"));
        assert_eq!(found.len(), 0);
    }

    #[test]
    fn test_plugin_lifecycle() {
        let mut registry = PluginRegistry::new();
        registry.register(
            Box::new(|| Box::new(TestPlugin::new("lifecycle-plugin"))),
            Some(PluginManifest::new("lifecycle-plugin", "Lifecycle Plugin")),
        ).unwrap();

        registry.load("lifecycle-plugin").unwrap();
        assert_eq!(registry.get_state("lifecycle-plugin"), Some(&PluginState::Loaded));

        let manifest = registry.get_manifest("lifecycle-plugin");
        assert!(manifest.is_some());
        assert_eq!(manifest.unwrap().plugin_id, "lifecycle-plugin");
    }

    #[test]
    fn test_plugin_unregister() {
        let mut registry = PluginRegistry::new();
        registry.register(
            Box::new(|| Box::new(TestPlugin::new("temp-plugin"))),
            Some(PluginManifest::new("temp-plugin", "Temp Plugin")),
        ).unwrap();
        assert_eq!(registry.list_plugins().len(), 1);

        assert!(registry.unregister("temp-plugin"));
        assert!(registry.list_plugins().is_empty());
    }

    #[test]
    fn test_plugin_manager_create() {
        let pm = PluginManager::new();
        let stats = pm.get_stats();
        assert_eq!(stats.get("total_plugins").unwrap().as_u64(), Some(0));
    }

    #[test]
    fn test_plugin_manager_load_and_activate() {
        let mut pm = PluginManager::new().with_sandbox_disabled();
        let manifest = PluginManifest::new("mgr-plugin", "Manager Plugin");

        let info = pm.load_plugin("mgr-plugin", manifest).unwrap();
        assert_eq!(info.state, PluginState::Loaded);

        assert!(pm.activate_plugin("mgr-plugin"));
        let info = pm.get_plugin_info("mgr-plugin").unwrap();
        assert_eq!(info.state, PluginState::Active);
    }

    #[test]
    fn test_plugin_manager_deactivate() {
        let mut pm = PluginManager::new().with_sandbox_disabled();
        pm.load_plugin("deact-plugin", PluginManifest::new("deact-plugin", "Deact Plugin")).unwrap();
        pm.activate_plugin("deact-plugin");

        assert!(pm.deactivate_plugin("deact-plugin"));
        let info = pm.get_plugin_info("deact-plugin").unwrap();
        assert_eq!(info.state, PluginState::Inactive);
    }

    #[test]
    fn test_plugin_manager_unload() {
        let mut pm = PluginManager::new().with_sandbox_disabled();
        pm.load_plugin("unload-plugin", PluginManifest::new("unload-plugin", "Unload Plugin")).unwrap();
        assert!(pm.unload_plugin("unload-plugin"));
        assert!(pm.get_plugin_info("unload-plugin").is_none());
    }

    #[test]
    fn test_plugin_type_enum() {
        assert_eq!(PluginType::Agent.to_string(), "agent");
        assert_eq!(PluginType::Tool.to_string(), "tool");
        assert_eq!(PluginType::Hook.to_string(), "hook");
        assert_eq!(PluginType::Skill.to_string(), "skill");
    }
}

// ─── Hook Tests ──────────────────────────────────────────

#[cfg(test)]
mod hook_tests {
    use super::*;

    struct AuditorHook;
    impl Hook for AuditorHook {
        fn name(&self) -> &str { "auditor" }
        fn priority(&self) -> u8 { 100 }
    }

    struct BlockDangerousHook;
    impl Hook for BlockDangerousHook {
        fn name(&self) -> &str { "block_dangerous" }
        fn priority(&self) -> u8 { 90 }

        fn on_tool_call(
            &mut self,
            _ctx: &HookContext,
            tool_name: &str,
            _tool_input: Option<&serde_json::Value>,
        ) -> HookResult {
            if tool_name == "rm_rf" || tool_name == "format_disk" {
                HookResult::deny("Dangerous tool blocked")
            } else {
                HookResult::allow()
            }
        }
    }

    #[test]
    fn test_hook_context_new() {
        let ctx = HookContext::new("on_agent_start");
        assert_eq!(ctx.event, "on_agent_start");
        assert!(ctx.agent_id.is_empty());
        assert!(!ctx.timestamp.is_empty());
    }

    #[test]
    fn test_hook_result_allow() {
        let result = HookResult::allow();
        assert!(result.allowed);
        assert!(result.modified_data.is_none());
    }

    #[test]
    fn test_hook_result_deny() {
        let result = HookResult::deny("blocked for security");
        assert!(!result.allowed);
        assert_eq!(result.messages, vec!["blocked for security"]);
    }

    #[test]
    fn test_hook_result_with_data() {
        let data = serde_json::json!({"key": "value"});
        let result = HookResult::with_modified_data(data.clone());
        assert!(result.allowed);
        assert_eq!(result.modified_data, Some(data));
    }

    #[test]
    fn test_hook_default_info() {
        let hook = AuditorHook;
        assert_eq!(hook.name(), "auditor");
        assert_eq!(hook.version(), "0.1.0");
        assert_eq!(hook.priority(), 100);
        assert!(hook.enabled());
    }

    #[test]
    fn test_hook_get_info() {
        let hook = AuditorHook;
        let info = hook.get_hook_info();
        assert_eq!(info["name"], "auditor");
        assert_eq!(info["version"], "0.1.0");
    }

    #[test]
    fn test_hook_block_dangerous_tool() {
        let mut hook = BlockDangerousHook;
        let ctx = HookContext::new("on_tool_call");

        let result = hook.on_tool_call(&ctx, "rm_rf", None);
        assert!(!result.allowed);
        assert_eq!(result.messages, vec!["Dangerous tool blocked"]);

        let result = hook.on_tool_call(&ctx, "read_file", None);
        assert!(result.allowed);
    }

    #[test]
    fn test_hook_default_methods_return_allow() {
        let mut hook = AuditorHook;
        let ctx = HookContext::new("test");

        assert!(hook.on_agent_start(&ctx, None).allowed);
        assert!(hook.on_agent_end(&ctx, None).allowed);
        assert!(hook.on_llm_request(&ctx, None, "gpt-4").allowed);
        assert!(hook.on_llm_response(&ctx, None, None).allowed);
        assert!(hook.on_memory_read(&ctx, "key", "L1").allowed);
        assert!(hook.on_memory_write(&ctx, "key", None).allowed);
    }
}