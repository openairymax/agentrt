// AgentOS Rust SDK - 插件框架
// Version: 3.0.0
// Last updated: 2026-04-26
//
// 插件化框架，提供运行时的动态功能扩展能力。
// 与 Python SDK: framework/plugin.py 保持一致。

use std::collections::HashMap;
use std::sync::{Arc, Mutex};

/// 插件状态枚举
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub enum PluginState {
    Discovered,
    Loaded,
    Activating,
    Active,
    Deactivating,
    Inactive,
    Error,
    Unloaded,
}

impl std::fmt::Display for PluginState {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            PluginState::Discovered => write!(f, "discovered"),
            PluginState::Loaded => write!(f, "loaded"),
            PluginState::Activating => write!(f, "activating"),
            PluginState::Active => write!(f, "active"),
            PluginState::Deactivating => write!(f, "deactivating"),
            PluginState::Inactive => write!(f, "inactive"),
            PluginState::Error => write!(f, "error"),
            PluginState::Unloaded => write!(f, "unloaded"),
        }
    }
}

/// 插件依赖
#[derive(Debug, Clone)]
pub struct PluginDependency {
    pub plugin_id: String,
    pub version_range: String,
    pub optional: bool,
}

/// 插件清单
#[derive(Debug, Clone)]
pub struct PluginManifest {
    pub plugin_id: String,
    pub name: String,
    pub version: String,
    pub description: String,
    pub author: String,
    pub entry_point: String,
    pub entry_class: String,
    pub dependencies: Vec<PluginDependency>,
    pub capabilities: Vec<String>,
    pub permissions: Vec<String>,
    pub max_memory_mb: i32,
    pub max_cpu_percent: f64,
    pub max_threads: i32,
    pub timeout_seconds: f64,
    pub tags: Vec<String>,
}

impl PluginManifest {
    pub fn new(plugin_id: &str, name: &str) -> Self {
        Self {
            plugin_id: plugin_id.to_string(),
            name: name.to_string(),
            version: "1.0.0".to_string(),
            description: String::new(),
            author: String::new(),
            entry_point: String::new(),
            entry_class: String::new(),
            dependencies: Vec::new(),
            capabilities: Vec::new(),
            permissions: Vec::new(),
            max_memory_mb: 128,
            max_cpu_percent: 50.0,
            max_threads: 4,
            timeout_seconds: 30.0,
            tags: Vec::new(),
        }
    }
}

/// 插件运行时信息
#[derive(Debug, Clone)]
pub struct PluginInfo {
    pub manifest: PluginManifest,
    pub state: PluginState,
    pub loaded_at: Option<String>,
    pub activated_at: Option<String>,
    pub error_message: Option<String>,
    pub activation_count: i32,
    pub total_active_time_seconds: f64,
}

/// 插件基类 trait
pub trait BasePlugin: Send + Sync {
    fn plugin_id(&self) -> &str;
    fn set_plugin_id(&mut self, id: &str);
    fn on_load(&mut self, context: &HashMap<String, serde_json::Value>) -> Result<(), String>;
    fn on_activate(&mut self, context: &HashMap<String, serde_json::Value>) -> Result<(), String>;
    fn on_deactivate(&mut self) -> Result<(), String>;
    fn on_unload(&mut self) -> Result<(), String>;
    fn on_error(&mut self, error: &str);
    fn get_capabilities(&self) -> Vec<String>;
}

/// 插件工厂函数类型
pub type PluginFactory = Box<dyn Fn() -> Box<dyn BasePlugin> + Send + Sync>;

/// 插件注册表
pub struct PluginRegistry {
    factories: HashMap<String, PluginFactory>,
    instances: HashMap<String, Box<dyn BasePlugin>>,
    manifests: HashMap<String, PluginManifest>,
    states: HashMap<String, PluginState>,
}

impl PluginRegistry {
    /// 创建新的插件注册表
    pub fn new() -> Self {
        Self {
            factories: HashMap::new(),
            instances: HashMap::new(),
            manifests: HashMap::new(),
            states: HashMap::new(),
        }
    }

    /// 注册插件工厂
    pub fn register(
        &mut self,
        factory: PluginFactory,
        manifest: Option<PluginManifest>,
    ) -> Result<String, String> {
        let temp = factory();
        let plugin_id = temp.plugin_id().to_string();

        let plugin_id = if plugin_id.is_empty() {
            if let Some(ref m) = manifest {
                m.plugin_id.clone()
            } else {
                return Err("plugin ID cannot be empty".to_string());
            }
        } else {
            plugin_id
        };

        if self.factories.contains_key(&plugin_id) {
            return Err(format!("plugin '{}' already registered", plugin_id));
        }

        let final_manifest = match manifest {
            Some(m) => {
                let mut m = m;
                if m.plugin_id.is_empty() {
                    m.plugin_id = plugin_id.clone();
                }
                if m.capabilities.is_empty() {
                    m.capabilities = temp.get_capabilities();
                }
                m
            }
            None => PluginManifest {
                plugin_id: plugin_id.clone(),
                name: plugin_id.clone(),
                capabilities: temp.get_capabilities(),
                ..PluginManifest::new(&plugin_id, &plugin_id)
            },
        };

        self.factories.insert(plugin_id.clone(), factory);
        self.manifests.insert(plugin_id.clone(), final_manifest);
        self.states.insert(plugin_id.clone(), PluginState::Discovered);

        Ok(plugin_id)
    }

    /// 注销插件
    pub fn unregister(&mut self, plugin_id: &str) -> bool {
        if !self.factories.contains_key(plugin_id) {
            return false;
        }

        self.instances.remove(plugin_id);
        self.factories.remove(plugin_id);
        self.manifests.remove(plugin_id);
        self.states.remove(plugin_id);

        true
    }

    /// 发现已注册的插件
    pub fn discover(&self, capability: Option<&str>) -> Vec<&PluginManifest> {
        self.manifests
            .values()
            .filter(|m| {
                if let Some(cap) = capability {
                    m.capabilities.iter().any(|c| c == cap)
                } else {
                    true
                }
            })
            .collect()
    }

    /// 加载插件实例
    pub fn load(&mut self, plugin_id: &str) -> Result<&dyn BasePlugin, String> {
        if !self.factories.contains_key(plugin_id) {
            return Err(format!("plugin '{}' not found", plugin_id));
        }

        if self.instances.contains_key(plugin_id) {
            return Ok(self.instances[plugin_id].as_ref());
        }

        let factory = self.factories.get(plugin_id).unwrap();
        let mut instance = factory();
        instance.set_plugin_id(plugin_id);

        self.instances.insert(plugin_id.to_string(), instance);
        self.states.insert(plugin_id.to_string(), PluginState::Loaded);

        Ok(self.instances[plugin_id].as_ref())
    }

    /// 卸载插件实例
    pub fn unload(&mut self, plugin_id: &str) -> bool {
        if !self.instances.contains_key(plugin_id) {
            return false;
        }

        self.instances.remove(plugin_id);
        self.states.insert(plugin_id.to_string(), PluginState::Unloaded);

        true
    }

    /// 获取已加载的插件实例
    pub fn get(&self, plugin_id: &str) -> Option<&dyn BasePlugin> {
        self.instances.get(plugin_id).map(|b| b.as_ref())
    }

    /// 获取插件清单
    pub fn get_manifest(&self, plugin_id: &str) -> Option<&PluginManifest> {
        self.manifests.get(plugin_id)
    }

    /// 获取插件状态
    pub fn get_state(&self, plugin_id: &str) -> Option<&PluginState> {
        self.states.get(plugin_id)
    }

    /// 列出所有已注册的插件ID
    pub fn list_plugins(&self) -> Vec<&str> {
        self.factories.keys().map(|s| s.as_str()).collect()
    }

    /// 按能力查找插件
    pub fn find_by_capability(&self, capability: &str) -> Vec<&str> {
        self.manifests
            .iter()
            .filter(|(_, m)| m.capabilities.iter().any(|c| c == capability))
            .map(|(pid, _)| pid.as_str())
            .collect()
    }
}

/// 插件管理器
pub struct PluginManager {
    plugins: HashMap<String, PluginInfo>,
    sandbox_enabled: bool,
    auto_discover: bool,
    plugin_directories: Vec<String>,
}

impl PluginManager {
    /// 创建新的插件管理器
    pub fn new() -> Self {
        Self {
            plugins: HashMap::new(),
            sandbox_enabled: true,
            auto_discover: true,
            plugin_directories: Vec::new(),
        }
    }

    /// 禁用沙箱
    pub fn with_sandbox_disabled(mut self) -> Self {
        self.sandbox_enabled = false;
        self
    }

    /// 设置插件目录
    pub fn with_plugin_directories(mut self, dirs: Vec<String>) -> Self {
        self.plugin_directories = dirs;
        self
    }

    /// 加载插件
    pub fn load_plugin(&mut self, plugin_id: &str, manifest: PluginManifest) -> Result<&PluginInfo, String> {
        if self.plugins.contains_key(plugin_id) {
            return Ok(&self.plugins[plugin_id]);
        }

        let now = chrono::Utc::now().to_rfc3339();
        let info = PluginInfo {
            manifest,
            state: PluginState::Loaded,
            loaded_at: Some(now),
            activated_at: None,
            error_message: None,
            activation_count: 0,
            total_active_time_seconds: 0.0,
        };

        self.plugins.insert(plugin_id.to_string(), info);
        Ok(&self.plugins[plugin_id])
    }

    /// 卸载插件
    pub fn unload_plugin(&mut self, plugin_id: &str) -> bool {
        if !self.plugins.contains_key(plugin_id) {
            return false;
        }

        let info = self.plugins.get(plugin_id).unwrap();
        if info.state == PluginState::Active {
            self.deactivate_plugin(plugin_id);
        }

        self.plugins.remove(plugin_id);
        true
    }

    /// 激活插件
    pub fn activate_plugin(&mut self, plugin_id: &str) -> bool {
        let info = match self.plugins.get_mut(plugin_id) {
            Some(i) => i,
            None => return false,
        };

        if info.state != PluginState::Loaded && info.state != PluginState::Inactive {
            return false;
        }

        let now = chrono::Utc::now().to_rfc3339();
        info.state = PluginState::Active;
        info.activated_at = Some(now);
        info.activation_count += 1;
        true
    }

    /// 停用插件
    pub fn deactivate_plugin(&mut self, plugin_id: &str) -> bool {
        let info = match self.plugins.get_mut(plugin_id) {
            Some(i) => i,
            None => return false,
        };

        if info.state != PluginState::Active {
            return false;
        }

        info.state = PluginState::Inactive;
        true
    }

    /// 获取插件信息
    pub fn get_plugin_info(&self, plugin_id: &str) -> Option<&PluginInfo> {
        self.plugins.get(plugin_id)
    }

    /// 列出插件
    pub fn list_plugins(&self, state_filter: Option<&PluginState>) -> Vec<&PluginInfo> {
        self.plugins
            .values()
            .filter(|p| {
                if let Some(sf) = state_filter {
                    &p.state == sf
                } else {
                    true
                }
            })
            .collect()
    }

    /// 获取统计信息
    pub fn get_stats(&self) -> HashMap<String, serde_json::Value> {
        let mut state_counts = HashMap::new();
        for info in self.plugins.values() {
            let key = info.state.to_string();
            *state_counts.entry(key).or_insert(0i32) += 1;
        }

        let mut stats = HashMap::new();
        stats.insert(
            "total_plugins".to_string(),
            serde_json::Value::Number(self.plugins.len().into()),
        );
        stats.insert(
            "sandbox_enabled".to_string(),
            serde_json::Value::Bool(self.sandbox_enabled),
        );

        stats
    }
}

use once_cell::sync::Lazy;
use std::sync::MutexGuard;

static GLOBAL_REGISTRY: Lazy<Mutex<PluginRegistry>> = Lazy::new(|| Mutex::new(PluginRegistry::new()));

/// 获取全局插件注册表
pub fn get_plugin_registry() -> MutexGuard<'static, PluginRegistry> {
    GLOBAL_REGISTRY.lock().unwrap()
}

#[cfg(test)]
mod tests {
    use super::*;

    struct TestPlugin {
        plugin_id: String,
        loaded: bool,
        activated: bool,
    }

    impl TestPlugin {
        fn new() -> Self {
            Self {
                plugin_id: String::new(),
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
            vec!["test".to_string(), "simple".to_string()]
        }
    }

    struct LoggerPlugin {
        plugin_id: String,
        logs: Vec<String>,
    }

    impl LoggerPlugin {
        fn new() -> Self {
            Self {
                plugin_id: String::new(),
                logs: Vec::new(),
            }
        }

        fn log(&mut self, level: &str, message: &str) {
            self.logs.push(format!("{}:{}", level, message));
        }

        fn count(&self) -> usize {
            self.logs.len()
        }
    }

    impl BasePlugin for LoggerPlugin {
        fn plugin_id(&self) -> &str {
            &self.plugin_id
        }

        fn set_plugin_id(&mut self, id: &str) {
            self.plugin_id = id.to_string();
        }

        fn on_load(&mut self, _context: &HashMap<String, serde_json::Value>) -> Result<(), String> {
            Ok(())
        }

        fn on_activate(&mut self, _context: &HashMap<String, serde_json::Value>) -> Result<(), String> {
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
            vec!["logging".to_string(), "structured_logs".to_string()]
        }
    }

    struct MetricsPlugin {
        plugin_id: String,
        counters: HashMap<String, f64>,
    }

    impl MetricsPlugin {
        fn new() -> Self {
            Self {
                plugin_id: String::new(),
                counters: HashMap::new(),
            }
        }

        fn increment(&mut self, name: &str, value: f64) -> f64 {
            let entry = self.counters.entry(name.to_string()).or_insert(0.0);
            *entry += value;
            *entry
        }

        fn get_counter(&self, name: &str) -> f64 {
            *self.counters.get(name).unwrap_or(&0.0)
        }
    }

    impl BasePlugin for MetricsPlugin {
        fn plugin_id(&self) -> &str {
            &self.plugin_id
        }

        fn set_plugin_id(&mut self, id: &str) {
            self.plugin_id = id.to_string();
        }

        fn on_load(&mut self, _context: &HashMap<String, serde_json::Value>) -> Result<(), String> {
            Ok(())
        }

        fn on_activate(&mut self, _context: &HashMap<String, serde_json::Value>) -> Result<(), String> {
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
            vec!["metrics".to_string(), "counters".to_string()]
        }
    }

    #[test]
    fn test_register() {
        let mut registry = PluginRegistry::new();
        let pid = registry.register(
            Box::new(|| Box::new(TestPlugin::new())),
            Some(PluginManifest::new("testPlugin", "Test Plugin")),
        ).unwrap();
        assert_eq!(pid, "testPlugin");
    }

    #[test]
    fn test_register_duplicate() {
        let mut registry = PluginRegistry::new();
        let factory: PluginFactory = Box::new(|| Box::new(TestPlugin::new()));
        let manifest = PluginManifest::new("testPlugin", "Test Plugin");
        registry.register(factory, Some(manifest)).unwrap();

        let factory2: PluginFactory = Box::new(|| Box::new(TestPlugin::new()));
        let manifest2 = PluginManifest::new("testPlugin", "Test Plugin");
        let result = registry.register(factory2, Some(manifest2));
        assert!(result.is_err());
    }

    #[test]
    fn test_discover() {
        let mut registry = PluginRegistry::new();
        registry.register(
            Box::new(|| Box::new(LoggerPlugin::new())),
            Some(PluginManifest {
                plugin_id: "loggerPlugin".to_string(),
                capabilities: vec!["logging".to_string()],
                ..PluginManifest::new("loggerPlugin", "Logger")
            }),
        ).unwrap();
        registry.register(
            Box::new(|| Box::new(MetricsPlugin::new())),
            Some(PluginManifest {
                plugin_id: "metricsPlugin".to_string(),
                capabilities: vec!["metrics".to_string()],
                ..PluginManifest::new("metricsPlugin", "Metrics")
            }),
        ).unwrap();

        let all = registry.discover(None);
        assert_eq!(all.len(), 2);

        let logging = registry.discover(Some("logging"));
        assert_eq!(logging.len(), 1);
    }

    #[test]
    fn test_load() {
        let mut registry = PluginRegistry::new();
        registry.register(
            Box::new(|| Box::new(TestPlugin::new())),
            Some(PluginManifest::new("testPlugin", "Test Plugin")),
        ).unwrap();

        let result = registry.load("testPlugin");
        assert!(result.is_ok());

        let state = registry.get_state("testPlugin");
        assert_eq!(state, Some(&PluginState::Loaded));
    }

    #[test]
    fn test_load_nonexistent() {
        let mut registry = PluginRegistry::new();
        let result = registry.load("nonexistent");
        assert!(result.is_err());
    }

    #[test]
    fn test_unload() {
        let mut registry = PluginRegistry::new();
        registry.register(
            Box::new(|| Box::new(TestPlugin::new())),
            Some(PluginManifest::new("testPlugin", "Test Plugin")),
        ).unwrap();
        registry.load("testPlugin").unwrap();

        let result = registry.unload("testPlugin");
        assert!(result);

        let state = registry.get_state("testPlugin");
        assert_eq!(state, Some(&PluginState::Unloaded));
    }

    #[test]
    fn test_unregister() {
        let mut registry = PluginRegistry::new();
        registry.register(
            Box::new(|| Box::new(TestPlugin::new())),
            Some(PluginManifest::new("testPlugin", "Test Plugin")),
        ).unwrap();
        registry.load("testPlugin").unwrap();

        let result = registry.unregister("testPlugin");
        assert!(result);

        let plugins = registry.list_plugins();
        assert!(!plugins.contains(&"testPlugin"));
    }

    #[test]
    fn test_find_by_capability() {
        let mut registry = PluginRegistry::new();
        registry.register(
            Box::new(|| Box::new(LoggerPlugin::new())),
            Some(PluginManifest {
                plugin_id: "loggerPlugin".to_string(),
                capabilities: vec!["logging".to_string()],
                ..PluginManifest::new("loggerPlugin", "Logger")
            }),
        ).unwrap();
        registry.register(
            Box::new(|| Box::new(MetricsPlugin::new())),
            Some(PluginManifest {
                plugin_id: "metricsPlugin".to_string(),
                capabilities: vec!["metrics".to_string()],
                ..PluginManifest::new("metricsPlugin", "Metrics")
            }),
        ).unwrap();

        let result = registry.find_by_capability("metrics");
        assert!(result.contains(&"metricsPlugin"));
        assert!(!result.contains(&"loggerPlugin"));
    }

    #[test]
    fn test_full_lifecycle() {
        let mut registry = PluginRegistry::new();

        // 1. Register
        let pid = registry.register(
            Box::new(|| Box::new(LoggerPlugin::new())),
            Some(PluginManifest {
                plugin_id: "loggerPlugin".to_string(),
                capabilities: vec!["logging".to_string()],
                ..PluginManifest::new("loggerPlugin", "Logger")
            }),
        ).unwrap();
        assert_eq!(pid, "loggerPlugin");

        // 2. Discover
        let discovered = registry.discover(None);
        assert_eq!(discovered.len(), 1);

        // 3. Load
        let result = registry.load("loggerPlugin");
        assert!(result.is_ok());

        // 4. Verify state
        let state = registry.get_state("loggerPlugin");
        assert_eq!(state, Some(&PluginState::Loaded));

        // 5. Unload
        let result = registry.unload("loggerPlugin");
        assert!(result);

        let state = registry.get_state("loggerPlugin");
        assert_eq!(state, Some(&PluginState::Unloaded));
    }

    #[test]
    fn test_manager_load_and_activate() {
        let mut pm = PluginManager::new().with_sandbox_disabled();
        let manifest = PluginManifest::new("test_plugin", "Test Plugin");

        let info = pm.load_plugin("test_plugin", manifest).unwrap();
        assert_eq!(info.state, PluginState::Loaded);

        pm.activate_plugin("test_plugin");
        let info = pm.get_plugin_info("test_plugin").unwrap();
        assert_eq!(info.state, PluginState::Active);
    }

    #[test]
    fn test_manager_get_stats() {
        let pm = PluginManager::new();
        let stats = pm.get_stats();
        assert_eq!(stats.get("total_plugins").unwrap().as_u64(), Some(0));
    }
}
