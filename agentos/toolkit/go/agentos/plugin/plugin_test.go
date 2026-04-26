// AgentOS Go SDK - 插件框架测试
// Version: 3.0.0
// Last updated: 2026-04-26

package plugin

import (
	"sync"
	"testing"
)

type testPlugin struct {
	BasePluginImpl
	loaded     bool
	activated  bool
	capabilities []string
}

func newTestPlugin() BasePlugin {
	return &testPlugin{
		capabilities: []string{"test", "simple"},
	}
}

func (p *testPlugin) OnLoad(context map[string]interface{}) error {
	p.loaded = true
	return nil
}

func (p *testPlugin) OnActivate(context map[string]interface{}) error {
	p.activated = true
	return nil
}

func (p *testPlugin) GetCapabilities() []string {
	return p.capabilities
}

type loggerPlugin struct {
	BasePluginImpl
	logs []string
	mu   sync.Mutex
}

func newLoggerPlugin() BasePlugin {
	return &loggerPlugin{}
}

func (p *loggerPlugin) GetCapabilities() []string {
	return []string{"logging", "structured_logs", "log_query"}
}

func (p *loggerPlugin) Log(level, message string) {
	p.mu.Lock()
	defer p.mu.Unlock()
	p.logs = append(p.logs, level+":"+message)
}

func (p *loggerPlugin) Count() int {
	p.mu.Lock()
	defer p.mu.Unlock()
	return len(p.logs)
}

type metricsPlugin struct {
	BasePluginImpl
	counters map[string]float64
	mu       sync.Mutex
}

func newMetricsPlugin() BasePlugin {
	return &metricsPlugin{
		counters: make(map[string]float64),
	}
}

func (p *metricsPlugin) GetCapabilities() []string {
	return []string{"metrics", "counters", "gauges", "timers"}
}

func (p *metricsPlugin) Increment(name string, value float64) float64 {
	p.mu.Lock()
	defer p.mu.Unlock()
	p.counters[name] += value
	return p.counters[name]
}

func (p *metricsPlugin) GetCounter(name string) float64 {
	p.mu.Lock()
	defer p.mu.Unlock()
	return p.counters[name]
}

func TestRegistry_Register(t *testing.T) {
	registry := NewPluginRegistry()

	manifest := &PluginManifest{PluginID: "testPlugin", Name: "Test Plugin", Version: "1.0.0"}
	pid, err := registry.Register(newTestPlugin, manifest)
	if err != nil {
		t.Fatalf("Register failed: %v", err)
	}
	if pid != "testPlugin" {
		t.Errorf("Expected plugin ID 'testPlugin', got '%s'", pid)
	}
}

func TestRegistry_RegisterDuplicate(t *testing.T) {
	registry := NewPluginRegistry()

	manifest := &PluginManifest{PluginID: "testPlugin", Name: "Test Plugin", Version: "1.0.0"}
	_, err := registry.Register(newTestPlugin, manifest)
	if err != nil {
		t.Fatalf("First register failed: %v", err)
	}

	_, err = registry.Register(newTestPlugin, manifest)
	if err == nil {
		t.Error("Expected error for duplicate registration")
	}
}

func TestRegistry_Discover(t *testing.T) {
	registry := NewPluginRegistry()

	registry.Register(newLoggerPlugin, &PluginManifest{PluginID: "loggerPlugin", Name: "Logger", Capabilities: []string{"logging", "structured_logs", "log_query"}})
	registry.Register(newMetricsPlugin, &PluginManifest{PluginID: "metricsPlugin", Name: "Metrics", Capabilities: []string{"metrics", "counters", "gauges", "timers"}})

	all := registry.Discover("")
	if len(all) != 2 {
		t.Errorf("Expected 2 plugins, got %d", len(all))
	}

	logging := registry.Discover("logging")
	if len(logging) != 1 {
		t.Errorf("Expected 1 logging plugin, got %d", len(logging))
	}
}

func TestRegistry_Load(t *testing.T) {
	registry := NewPluginRegistry()

	registry.Register(newTestPlugin, &PluginManifest{PluginID: "testPlugin", Name: "Test Plugin"})

	instance, err := registry.Load("testPlugin")
	if err != nil {
		t.Fatalf("Load failed: %v", err)
	}
	if instance == nil {
		t.Fatal("Expected non-nil instance")
	}

	state, _ := registry.GetState("testPlugin")
	if state != StateLoaded {
		t.Errorf("Expected state LOADED, got %s", state)
	}
}

func TestRegistry_LoadSameInstance(t *testing.T) {
	registry := NewPluginRegistry()

	registry.Register(newTestPlugin, &PluginManifest{PluginID: "testPlugin", Name: "Test Plugin"})

	inst1, _ := registry.Load("testPlugin")
	inst2, _ := registry.Load("testPlugin")

	if inst1 != inst2 {
		t.Error("Expected same instance on repeated load")
	}
}

func TestRegistry_LoadNonExistent(t *testing.T) {
	registry := NewPluginRegistry()

	_, err := registry.Load("nonexistent")
	if err == nil {
		t.Error("Expected error for non-existent plugin")
	}
}

func TestRegistry_Unload(t *testing.T) {
	registry := NewPluginRegistry()

	registry.Register(newTestPlugin, &PluginManifest{PluginID: "testPlugin", Name: "Test Plugin"})
	registry.Load("testPlugin")

	result := registry.Unload("testPlugin")
	if !result {
		t.Error("Unload should return true")
	}

	_, exists := registry.Get("testPlugin")
	if exists {
		t.Error("Plugin should not exist after unload")
	}

	state, _ := registry.GetState("testPlugin")
	if state != StateUnloaded {
		t.Errorf("Expected state UNLOADED, got %s", state)
	}
}

func TestRegistry_Unregister(t *testing.T) {
	registry := NewPluginRegistry()

	registry.Register(newTestPlugin, &PluginManifest{PluginID: "testPlugin", Name: "Test Plugin"})
	registry.Load("testPlugin")

	result := registry.Unregister("testPlugin")
	if !result {
		t.Error("Unregister should return true")
	}

	plugins := registry.ListPlugins()
	for _, p := range plugins {
		if p == "testPlugin" {
			t.Error("Plugin should not be listed after unregister")
		}
	}
}

func TestRegistry_FindByCapability(t *testing.T) {
	registry := NewPluginRegistry()

	registry.Register(newLoggerPlugin, &PluginManifest{PluginID: "loggerPlugin", Name: "Logger", Capabilities: []string{"logging"}})
	registry.Register(newMetricsPlugin, &PluginManifest{PluginID: "metricsPlugin", Name: "Metrics", Capabilities: []string{"metrics"}})

	result := registry.FindByCapability("metrics")
	found := false
	for _, pid := range result {
		if pid == "metricsPlugin" {
			found = true
		}
	}
	if !found {
		t.Error("Expected to find metricsPlugin by capability 'metrics'")
	}
}

func TestRegistry_FullLifecycle(t *testing.T) {
	registry := NewPluginRegistry()

	// 1. Register
	pid, err := registry.Register(newLoggerPlugin, &PluginManifest{PluginID: "loggerPlugin", Name: "Logger", Capabilities: []string{"logging"}})
	if err != nil {
		t.Fatalf("Register failed: %v", err)
	}
	if pid != "loggerPlugin" {
		t.Errorf("Expected 'loggerPlugin', got '%s'", pid)
	}

	// 2. Discover
	discovered := registry.Discover("")
	if len(discovered) != 1 {
		t.Errorf("Expected 1 discovered plugin, got %d", len(discovered))
	}

	// 3. Load
	instance, err := registry.Load("loggerPlugin")
	if err != nil {
		t.Fatalf("Load failed: %v", err)
	}

	// 4. Call
	lp, ok := instance.(*loggerPlugin)
	if !ok {
		t.Fatal("Expected *loggerPlugin type")
	}
	lp.Log("INFO", "test message")
	if lp.Count() != 1 {
		t.Errorf("Expected 1 log entry, got %d", lp.Count())
	}

	// 5. Unload
	result := registry.Unload("loggerPlugin")
	if !result {
		t.Error("Unload should return true")
	}

	state, _ := registry.GetState("loggerPlugin")
	if state != StateUnloaded {
		t.Errorf("Expected state UNLOADED, got %s", state)
	}
}

func TestManager_LoadAndActivate(t *testing.T) {
	pm := NewPluginManager(WithSandboxDisabled())

	manifest := PluginManifest{
		PluginID: "test_plugin",
		Name:     "Test Plugin",
		Version:  "1.0.0",
	}

	info, err := pm.LoadPlugin("test_plugin", manifest)
	if err != nil {
		t.Fatalf("LoadPlugin failed: %v", err)
	}
	if info.State != StateLoaded {
		t.Errorf("Expected state LOADED, got %s", info.State)
	}

	pm.ActivatePlugin("test_plugin")

	info, _ = pm.GetPluginInfo("test_plugin")
	if info.State != StateActive {
		t.Errorf("Expected state ACTIVE, got %s", info.State)
	}
}

func TestManager_GetStats(t *testing.T) {
	pm := NewPluginManager()

	stats := pm.GetStats()
	totalPlugins, ok := stats["total_plugins"].(int)
	if !ok {
		t.Fatal("total_plugins not found or wrong type")
	}
	if totalPlugins != 0 {
		t.Errorf("Expected 0 plugins, got %d", totalPlugins)
	}
}

func TestGetPluginRegistry_Singleton(t *testing.T) {
	r1 := GetPluginRegistry()
	r2 := GetPluginRegistry()
	if r1 != r2 {
		t.Error("Expected same registry instance")
	}
}
