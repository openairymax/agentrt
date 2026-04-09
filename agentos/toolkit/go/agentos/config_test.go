// AgentOS Go SDK - 配置管理模块单元测试
// Version: 3.0.0

package agentos

import (
	"os"
	"testing"
	"time"
)

func TestDefaultConfig(t *testing.T) {
	cfg := DefaultConfig()
	if cfg.Endpoint != "http://localhost:18789" {
		t.Errorf("默认端点 = %q", cfg.Endpoint)
	}
	if cfg.Timeout != 30*time.Second {
		t.Errorf("默认超时 = %v", cfg.Timeout)
	}
	if cfg.MaxRetries != 3 {
		t.Errorf("默认重试次数 = %d", cfg.MaxRetries)
	}
}

func TestWithEndpoint(t *testing.T) {
	cfg := NewConfig(WithEndpoint("http://custom:8080"))
	if cfg.Endpoint != "http://custom:8080" {
		t.Errorf("端点 = %q", cfg.Endpoint)
	}
}

func TestWithEndpoint_IgnoresEmpty(t *testing.T) {
	cfg := NewConfig(WithEndpoint(""))
	if cfg.Endpoint != "http://localhost:18789" {
		t.Errorf("空端点不应覆盖默认值, got %q", cfg.Endpoint)
	}
}

func TestWithTimeout_IgnoresZero(t *testing.T) {
	cfg := NewConfig(WithTimeout(0))
	if cfg.Timeout != 30*time.Second {
		t.Errorf("零超时不应覆盖默认值, got %v", cfg.Timeout)
	}
}

func TestWithMaxRetries(t *testing.T) {
	cfg := NewConfig(WithMaxRetries(5))
	if cfg.MaxRetries != 5 {
		t.Errorf("重试次数 = %d", cfg.MaxRetries)
	}
}

func TestWithAPIKey(t *testing.T) {
	cfg := NewConfig(WithAPIKey("test-key"))
	if cfg.APIKey != "test-key" {
		t.Errorf("API Key = %q", cfg.APIKey)
	}
}

func TestWithDebug(t *testing.T) {
	cfg := NewConfig(WithDebug(true))
	if !cfg.Debug {
		t.Error("Debug 应为 true")
	}
}

func TestNewConfigFromEnv(t *testing.T) {
	os.Setenv("AGENTOS_ENDPOINT", "http://env:9999")
	os.Setenv("AGENTOS_TIMEOUT", "45s")
	os.Setenv("AGENTOS_MAX_RETRIES", "5")
	os.Setenv("AGENTOS_API_KEY", "env-key")
	os.Setenv("AGENTOS_DEBUG", "true")
	os.Setenv("AGENTOS_LOG_LEVEL", "debug")
	defer func() {
		os.Unsetenv("AGENTOS_ENDPOINT")
		os.Unsetenv("AGENTOS_TIMEOUT")
		os.Unsetenv("AGENTOS_MAX_RETRIES")
		os.Unsetenv("AGENTOS_API_KEY")
		os.Unsetenv("AGENTOS_DEBUG")
		os.Unsetenv("AGENTOS_LOG_LEVEL")
	}()

	cfg, err := NewConfigFromEnv()
	if err != nil {
		t.Fatalf("NewConfigFromEnv error = %v", err)
	}
	if cfg.Endpoint != "http://env:9999" {
		t.Errorf("环境变量端点 = %q", cfg.Endpoint)
	}
	if cfg.Timeout != 45*time.Second {
		t.Errorf("环境变量超时 = %v", cfg.Timeout)
	}
	if cfg.MaxRetries != 5 {
		t.Errorf("环境变量重试 = %d", cfg.MaxRetries)
	}
	if !cfg.Debug {
		t.Error("环境变量 Debug 应为 true")
	}
	if cfg.LogLevel != "debug" {
		t.Errorf("LogLevel = %q, want debug", cfg.LogLevel)
	}
}

func TestValidate_Valid(t *testing.T) {
	cfg := DefaultConfig()
	if err := cfg.Validate(); err != nil {
		t.Errorf("默认配置验证应通过, got %v", err)
	}
}

func TestValidate_EmptyEndpoint(t *testing.T) {
	cfg := &manager{Endpoint: "", Timeout: 30 * time.Second, MaxConnections: 10}
	err := cfg.Validate()
	if err == nil {
		t.Error("空端点应返回错误")
	}
	if !IsErrorCode(err, CodeInvalidEndpoint) {
		t.Errorf("期望 CodeInvalidEndpoint, got %v", err)
	}
}

func TestValidate_ZeroTimeout(t *testing.T) {
	cfg := &manager{Endpoint: "http://localhost:18789", Timeout: 0, MaxConnections: 10}
	err := cfg.Validate()
	if err == nil {
		t.Error("零超时应返回错误")
	}
}

func TestValidate_ZeroMaxConnections(t *testing.T) {
	cfg := &manager{Endpoint: "http://localhost:18789", Timeout: 30 * time.Second, MaxConnections: 0}
	err := cfg.Validate()
	if err == nil {
		t.Error("零连接数应返回错误")
	}
}

func TestClone(t *testing.T) {
	cfg := NewConfig(WithEndpoint("http://test:8080"), WithAPIKey("key"))
	cloned := cfg.Clone()

	cloned.Endpoint = "http://changed:8080"
	cloned.APIKey = "changed-key"

	if cfg.Endpoint == "http://changed:8080" {
		t.Error("Clone 应创建独立副本")
	}
	if cfg.APIKey == "changed-key" {
		t.Error("Clone 修改不应影响原始配置")
	}
}

func TestMerge_Nil(t *testing.T) {
	base := DefaultConfig()
	merged := base.Merge(nil)
	if merged.Endpoint != base.Endpoint {
		t.Error("nil override 不应改变配置")
	}
}

func TestMerge_NonZero(t *testing.T) {
	base := DefaultConfig()
	override := &manager{Endpoint: "http://override:8080", APIKey: "new-key"}
	merged := base.Merge(override)

	if merged.Endpoint != "http://override:8080" {
		t.Error("非零值应覆盖")
	}
	if merged.APIKey != "new-key" {
		t.Error("非零值应覆盖")
	}
}

func TestMerge_ZeroValuePreserved(t *testing.T) {
	base := NewConfig(WithTimeout(60 * time.Second))
	override := &manager{Endpoint: "http://override:8080"}
	merged := base.Merge(override)

	if merged.Timeout != 60*time.Second {
		t.Errorf("零值应保留 base, got %v", merged.Timeout)
	}
}

func TestMerge_DebugAlwaysOverride(t *testing.T) {
	base := NewConfig(WithDebug(false))
	override := &manager{Debug: true}
	merged := base.Merge(override)
	if !merged.Debug {
		t.Error("Debug 应始终取 override 的值")
	}
}

func TestConfigString(t *testing.T) {
	cfg := DefaultConfig()
	s := cfg.String()
	if s == "" {
		t.Error("String() 不应返回空")
	}
}
