// AgentOS Go SDK - 插件框架
// Version: 3.0.0
// Last updated: 2026-04-26
//
// 插件化框架，提供运行时的动态功能扩展能力。
// 与 Python SDK: framework/plugin.py 保持一致。

package plugin

import (
	"fmt"
	"sync"
	"time"
)

// PluginState 插件状态枚举
type PluginState string

const (
	StateDiscovered  PluginState = "discovered"
	StateLoaded      PluginState = "loaded"
	StateActivating  PluginState = "activating"
	StateActive      PluginState = "active"
	StateDeactivating PluginState = "deactivating"
	StateInactive    PluginState = "inactive"
	StateError       PluginState = "error"
	StateUnloaded    PluginState = "unloaded"
)

// PluginDependency 插件依赖
type PluginDependency struct {
	PluginID     string `json:"plugin_id"`
	VersionRange string `json:"version_range"`
	Optional     bool   `json:"optional"`
}

// PluginManifest 插件清单
type PluginManifest struct {
	PluginID       string             `json:"plugin_id"`
	Name           string             `json:"name"`
	Version        string             `json:"version"`
	Description    string             `json:"description"`
	Author         string             `json:"author"`
	EntryPoint     string             `json:"entry_point"`
	EntryClass     string             `json:"entry_class"`
	Dependencies   []PluginDependency `json:"dependencies"`
	Capabilities   []string           `json:"capabilities"`
	Permissions    []string           `json:"permissions"`
	MaxMemoryMB    int                `json:"max_memory_mb"`
	MaxCPUPercent  float64            `json:"max_cpu_percent"`
	MaxThreads     int                `json:"max_threads"`
	TimeoutSeconds float64            `json:"timeout_seconds"`
	Tags           []string           `json:"tags"`
}

// PluginInfo 插件运行时信息
type PluginInfo struct {
	Manifest              PluginManifest
	State                 PluginState
	LoadedAt              *time.Time
	ActivatedAt           *time.Time
	ErrorMessage          string
	ActivationCount       int
	TotalActiveTimeSeconds float64
}

// BasePlugin 插件基类接口
type BasePlugin interface {
	GetPluginID() string
	SetPluginID(id string)
	OnLoad(context map[string]interface{}) error
	OnActivate(context map[string]interface{}) error
	OnDeactivate() error
	OnUnload() error
	OnError(err error)
	GetCapabilities() []string
}

// BasePluginImpl 插件基类默认实现
type BasePluginImpl struct {
	pluginID string
}

func (p *BasePluginImpl) GetPluginID() string {
	return p.pluginID
}

func (p *BasePluginImpl) SetPluginID(id string) {
	p.pluginID = id
}

func (p *BasePluginImpl) OnLoad(context map[string]interface{}) error {
	return nil
}

func (p *BasePluginImpl) OnActivate(context map[string]interface{}) error {
	return nil
}

func (p *BasePluginImpl) OnDeactivate() error {
	return nil
}

func (p *BasePluginImpl) OnUnload() error {
	return nil
}

func (p *BasePluginImpl) OnError(err error) {}

func (p *BasePluginImpl) GetCapabilities() []string {
	return []string{}
}

// PluginFactory 插件工厂函数类型
type PluginFactory func() BasePlugin

// PluginRegistry 插件注册表
type PluginRegistry struct {
	mu            sync.RWMutex
	factories     map[string]PluginFactory
	instances     map[string]BasePlugin
	manifests     map[string]PluginManifest
	states        map[string]PluginState
}

// NewPluginRegistry 创建新的插件注册表
func NewPluginRegistry() *PluginRegistry {
	return &PluginRegistry{
		factories: make(map[string]PluginFactory),
		instances: make(map[string]BasePlugin),
		manifests: make(map[string]PluginManifest),
		states:    make(map[string]PluginState),
	}
}

// Register 注册插件工厂
func (r *PluginRegistry) Register(factory PluginFactory, manifest *PluginManifest) (string, error) {
	r.mu.Lock()
	defer r.mu.Unlock()

	temp := factory()
	pluginID := temp.GetPluginID()

	if pluginID == "" {
		pluginID = manifest.PluginID
	}
	if pluginID == "" {
		return "", fmt.Errorf("plugin ID cannot be empty, provide manifest with plugin_id or implement GetPluginID()")
	}

	if _, exists := r.factories[pluginID]; exists {
		return "", fmt.Errorf("plugin '%s' already registered", pluginID)
	}

	if manifest == nil {
		manifest = &PluginManifest{
			PluginID:     pluginID,
			Name:         pluginID,
			Version:      "1.0.0",
			Capabilities: temp.GetCapabilities(),
		}
	} else {
		if manifest.PluginID == "" {
			manifest.PluginID = pluginID
		}
		if manifest.Capabilities == nil {
			manifest.Capabilities = temp.GetCapabilities()
		}
	}

	r.factories[pluginID] = factory
	r.manifests[pluginID] = *manifest
	r.states[pluginID] = StateDiscovered

	return pluginID, nil
}

// Unregister 注销插件
func (r *PluginRegistry) Unregister(pluginID string) bool {
	r.mu.Lock()
	defer r.mu.Unlock()

	if _, exists := r.factories[pluginID]; !exists {
		return false
	}

	if _, loaded := r.instances[pluginID]; loaded {
		r.Unload(pluginID)
	}

	delete(r.factories, pluginID)
	delete(r.manifests, pluginID)
	delete(r.states, pluginID)

	return true
}

// Discover 发现已注册的插件
func (r *PluginRegistry) Discover(capability string) []PluginManifest {
	r.mu.RLock()
	defer r.mu.RUnlock()

	var result []PluginManifest
	for _, m := range r.manifests {
		if capability == "" {
			result = append(result, m)
		} else {
			for _, c := range m.Capabilities {
				if c == capability {
					result = append(result, m)
					break
				}
			}
		}
	}
	return result
}

// Load 加载插件实例
func (r *PluginRegistry) Load(pluginID string) (BasePlugin, error) {
	r.mu.Lock()
	defer r.mu.Unlock()

	factory, exists := r.factories[pluginID]
	if !exists {
		return nil, fmt.Errorf("plugin '%s' not found", pluginID)
	}

	if instance, loaded := r.instances[pluginID]; loaded {
		return instance, nil
	}

	instance := factory()
	instance.SetPluginID(pluginID)

	r.instances[pluginID] = instance
	r.states[pluginID] = StateLoaded

	return instance, nil
}

// Unload 卸载插件实例
func (r *PluginRegistry) Unload(pluginID string) bool {
	r.mu.Lock()
	defer r.mu.Unlock()

	if _, exists := r.instances[pluginID]; !exists {
		return false
	}

	delete(r.instances, pluginID)
	r.states[pluginID] = StateUnloaded

	return true
}

// Get 获取已加载的插件实例
func (r *PluginRegistry) Get(pluginID string) (BasePlugin, bool) {
	r.mu.RLock()
	defer r.mu.RUnlock()

	instance, exists := r.instances[pluginID]
	return instance, exists
}

// GetManifest 获取插件清单
func (r *PluginRegistry) GetManifest(pluginID string) (PluginManifest, bool) {
	r.mu.RLock()
	defer r.mu.RUnlock()

	m, exists := r.manifests[pluginID]
	return m, exists
}

// GetState 获取插件状态
func (r *PluginRegistry) GetState(pluginID string) (PluginState, bool) {
	r.mu.RLock()
	defer r.mu.RUnlock()

	s, exists := r.states[pluginID]
	return s, exists
}

// ListPlugins 列出所有已注册的插件ID
func (r *PluginRegistry) ListPlugins() []string {
	r.mu.RLock()
	defer r.mu.RUnlock()

	result := make([]string, 0, len(r.factories))
	for id := range r.factories {
		result = append(result, id)
	}
	return result
}

// FindByCapability 按能力查找插件
func (r *PluginRegistry) FindByCapability(capability string) []string {
	r.mu.RLock()
	defer r.mu.RUnlock()

	var result []string
	for pid, m := range r.manifests {
		for _, c := range m.Capabilities {
			if c == capability {
				result = append(result, pid)
				break
			}
		}
	}
	return result
}

// PluginManager 插件管理器
type PluginManager struct {
	mu               sync.RWMutex
	plugins          map[string]*PluginInfo
	sandboxEnabled   bool
	autoDiscover     bool
	pluginDirectories []string
}

// NewPluginManager 创建新的插件管理器
func NewPluginManager(options ...func(*PluginManager)) *PluginManager {
	pm := &PluginManager{
		plugins:        make(map[string]*PluginInfo),
		sandboxEnabled: true,
		autoDiscover:   true,
	}
	for _, opt := range options {
		opt(pm)
	}
	return pm
}

// WithSandboxDisabled 禁用沙箱选项
func WithSandboxDisabled() func(*PluginManager) {
	return func(pm *PluginManager) {
		pm.sandboxEnabled = false
	}
}

// WithPluginDirectories 设置插件目录选项
func WithPluginDirectories(dirs []string) func(*PluginManager) {
	return func(pm *PluginManager) {
		pm.pluginDirectories = dirs
	}
}

// LoadPlugin 加载插件
func (pm *PluginManager) LoadPlugin(pluginID string, manifest PluginManifest) (*PluginInfo, error) {
	pm.mu.Lock()
	defer pm.mu.Unlock()

	if _, exists := pm.plugins[pluginID]; exists {
		return pm.plugins[pluginID], nil
	}

	now := time.Now()
	info := &PluginInfo{
		Manifest:        manifest,
		State:           StateLoaded,
		LoadedAt:        &now,
		ActivationCount: 0,
	}

	pm.plugins[pluginID] = info
	return info, nil
}

// UnloadPlugin 卸载插件
func (pm *PluginManager) UnloadPlugin(pluginID string) bool {
	pm.mu.Lock()
	defer pm.mu.Unlock()

	info, exists := pm.plugins[pluginID]
	if !exists {
		return false
	}

	if info.State == StateActive {
		pm.DeactivatePlugin(pluginID)
	}

	info.State = StateUnloaded
	delete(pm.plugins, pluginID)
	return true
}

// ActivatePlugin 激活插件
func (pm *PluginManager) ActivatePlugin(pluginID string) bool {
	pm.mu.Lock()
	defer pm.mu.Unlock()

	info, exists := pm.plugins[pluginID]
	if !exists {
		return false
	}

	if info.State != StateLoaded && info.State != StateInactive {
		return false
	}

	now := time.Now()
	info.State = StateActive
	info.ActivatedAt = &now
	info.ActivationCount++
	return true
}

// DeactivatePlugin 停用插件
func (pm *PluginManager) DeactivatePlugin(pluginID string) bool {
	pm.mu.Lock()
	defer pm.mu.Unlock()

	info, exists := pm.plugins[pluginID]
	if !exists || info.State != StateActive {
		return false
	}

	if info.ActivatedAt != nil {
		activeTime := time.Since(*info.ActivatedAt).Seconds()
		info.TotalActiveTimeSeconds += activeTime
	}

	info.State = StateInactive
	return true
}

// GetPluginInfo 获取插件信息
func (pm *PluginManager) GetPluginInfo(pluginID string) (*PluginInfo, bool) {
	pm.mu.RLock()
	defer pm.mu.RUnlock()

	info, exists := pm.plugins[pluginID]
	return info, exists
}

// ListPlugins 列出插件
func (pm *PluginManager) ListPlugins(stateFilter ...PluginState) []*PluginInfo {
	pm.mu.RLock()
	defer pm.mu.RUnlock()

	var result []*PluginInfo
	for _, info := range pm.plugins {
		if len(stateFilter) > 0 && info.State != stateFilter[0] {
			continue
		}
		result = append(result, info)
	}
	return result
}

// GetStats 获取统计信息
func (pm *PluginManager) GetStats() map[string]interface{} {
	pm.mu.RLock()
	defer pm.mu.RUnlock()

	stateCounts := make(map[string]int)
	for _, info := range pm.plugins {
		stateCounts[string(info.State)]++
	}

	return map[string]interface{}{
		"total_plugins":      len(pm.plugins),
		"state_distribution": stateCounts,
		"sandbox_enabled":    pm.sandboxEnabled,
		"plugin_directories": pm.pluginDirectories,
	}
}

var globalRegistry *PluginRegistry
var globalRegistryOnce sync.Once

// GetPluginRegistry 获取全局插件注册表单例
func GetPluginRegistry() *PluginRegistry {
	globalRegistryOnce.Do(func() {
		globalRegistry = NewPluginRegistry()
	})
	return globalRegistry
}
