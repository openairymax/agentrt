use agentos::plugin::{Plugin, PluginRegistry, PluginManager, PluginMetadata};

struct TestPlugin {
    name: String,
    initialized: bool,
}

impl TestPlugin {
    fn new(name: &str) -> Self {
        Self {
            name: name.to_string(),
            initialized: false,
        }
    }
}

impl Plugin for TestPlugin {
    fn name(&self) -> &str {
        &self.name
    }

    fn initialize(&mut self) -> Result<(), Box<dyn std::error::Error>> {
        self.initialized = true;
        Ok(())
    }

    fn shutdown(&mut self) -> Result<(), Box<dyn std::error::Error>> {
        self.initialized = false;
        Ok(())
    }

    fn metadata(&self) -> PluginMetadata {
        PluginMetadata {
            name: self.name.clone(),
            version: "1.0.0".to_string(),
            description: "Test plugin for benchmarking".to_string(),
            capabilities: vec!["test".to_string(), "benchmark".to_string()],
        }
    }
}

#[cfg(test)]
mod plugin_tests {
    use super::*;

    #[test]
    fn test_plugin_registry_create() {
        let registry = PluginRegistry::new();
        assert_eq!(registry.count(), 0);
    }

    #[test]
    fn test_plugin_register_and_discover() {
        let mut registry = PluginRegistry::new();
        let plugin = TestPlugin::new("test-plugin");
        registry.register(Box::new(plugin)).expect("register should succeed");
        assert_eq!(registry.count(), 1);

        let discovered = registry.discover("test");
        assert_eq!(discovered.len(), 1);
    }

    #[test]
    fn test_plugin_register_duplicate() {
        let mut registry = PluginRegistry::new();
        let p1 = TestPlugin::new("dup-plugin");
        let p2 = TestPlugin::new("dup-plugin");
        registry.register(Box::new(p1)).expect("first register should succeed");
        let result = registry.register(Box::new(p2));
        assert!(result.is_err(), "duplicate registration should fail");
    }

    #[test]
    fn test_plugin_discover_by_capability() {
        let mut registry = PluginRegistry::new();
        registry.register(Box::new(TestPlugin::new("plugin-a"))).unwrap();
        registry.register(Box::new(TestPlugin::new("plugin-b"))).unwrap();

        let found = registry.discover("benchmark");
        assert_eq!(found.len(), 2);
    }

    #[test]
    fn test_plugin_discover_no_match() {
        let mut registry = PluginRegistry::new();
        registry.register(Box::new(TestPlugin::new("test-plugin"))).unwrap();

        let found = registry.discover("nonexistent");
        assert_eq!(found.len(), 0);
    }

    #[test]
    fn test_plugin_lifecycle() {
        let mut registry = PluginRegistry::new();
        let plugin = TestPlugin::new("lifecycle-plugin");
        registry.register(Box::new(plugin)).unwrap();

        registry.initialize("lifecycle-plugin").expect("init should succeed");

        let meta = registry.get_metadata("lifecycle-plugin").expect("should find plugin");
        assert_eq!(meta.name, "lifecycle-plugin");

        registry.shutdown("lifecycle-plugin").expect("shutdown should succeed");
    }

    #[test]
    fn test_plugin_unregister() {
        let mut registry = PluginRegistry::new();
        registry.register(Box::new(TestPlugin::new("temp-plugin"))).unwrap();
        assert_eq!(registry.count(), 1);

        registry.unregister("temp-plugin").expect("unregister should succeed");
        assert_eq!(registry.count(), 0);
    }

    #[test]
    fn test_plugin_manager_create() {
        let manager = PluginManager::new();
        assert_eq!(manager.count(), 0);
    }

    #[test]
    fn test_plugin_manager_load_and_activate() {
        let mut manager = PluginManager::new();
        let plugin = TestPlugin::new("mgr-plugin");
        manager.load(Box::new(plugin)).expect("load should succeed");
        manager.activate("mgr-plugin").expect("activate should succeed");
        assert!(manager.is_active("mgr-plugin"));
    }

    #[test]
    fn test_plugin_manager_deactivate() {
        let mut manager = PluginManager::new();
        manager.load(Box::new(TestPlugin::new("deact-plugin"))).unwrap();
        manager.activate("deact-plugin").unwrap();
        manager.deactivate("deact-plugin").expect("deactivate should succeed");
        assert!(!manager.is_active("deact-plugin"));
    }

    #[test]
    fn test_plugin_manager_unload() {
        let mut manager = PluginManager::new();
        manager.load(Box::new(TestPlugin::new("unload-plugin"))).unwrap();
        manager.unload("unload-plugin").expect("unload should succeed");
        assert_eq!(manager.count(), 0);
    }

    #[test]
    fn test_plugin_metadata() {
        let plugin = TestPlugin::new("meta-plugin");
        let meta = plugin.metadata();
        assert_eq!(meta.name, "meta-plugin");
        assert_eq!(meta.version, "1.0.0");
        assert_eq!(meta.capabilities.len(), 2);
        assert!(meta.capabilities.contains(&"test".to_string()));
    }

    #[test]
    fn test_plugin_multiple_capabilities() {
        let mut registry = PluginRegistry::new();
        registry.register(Box::new(TestPlugin::new("multi-cap"))).unwrap();

        let test_plugins = registry.discover("test");
        let bench_plugins = registry.discover("benchmark");
        assert_eq!(test_plugins.len(), 1);
        assert_eq!(bench_plugins.len(), 1);
    }

    #[test]
    fn test_plugin_initialize_nonexistent() {
        let mut registry = PluginRegistry::new();
        let result = registry.initialize("nonexistent");
        assert!(result.is_err());
    }

    #[test]
    fn test_plugin_shutdown_nonexistent() {
        let mut registry = PluginRegistry::new();
        let result = registry.shutdown("nonexistent");
        assert!(result.is_err());
    }
}
