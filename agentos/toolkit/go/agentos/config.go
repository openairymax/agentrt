// AgentOS Go SDK - 配置管理模块
// Version: 3.0.0
// Last updated: 2026-03-22
//
// 提供客户端配置的定义、创建、验证和合并功能。
// 支持函数式选项模式和环境变量注入，零值保护守卫。
// 对应 Python SDK: 隐含在 client 模块中

package agentos

import (
	"fmt"
	"net/url"
	"os"
	"strconv"
	"time"
)

// manager 定义 AgentOS 客户端的完整配置
type manager struct {
	Endpoint        string
	Timeout         time.Duration
	MaxRetries      int
	RetryDelay      time.Duration
	APIKey          string
	UserAgent       string
	Debug           bool
	LogLevel        string
	MaxConnections  int
	IdleConnTimeout time.Duration
}

// ConfigOption 定义配置选项的函数签名
type ConfigOption func(*manager)

// DefaultConfig 返回默认配置
func DefaultConfig() *manager {
	return &manager{
		Endpoint:        "http://localhost:18789",
		Timeout:         30 * time.Second,
		MaxRetries:      3,
		RetryDelay:      1 * time.Second,
		UserAgent:       "AgentOS-Go-tools/3.0.0",
		LogLevel:        "info",
		MaxConnections:  10,
		IdleConnTimeout: 90 * time.Second,
	}
}

// WithEndpoint 设置服务端点地址（空值不覆盖默认值）
func WithEndpoint(endpoint string) ConfigOption {
	return func(c *manager) {
		if endpoint != "" {
			c.Endpoint = endpoint
		}
	}
}

// WithTimeout 设置请求超时时间
func WithTimeout(timeout time.Duration) ConfigOption {
	return func(c *manager) {
		if timeout > 0 {
			c.Timeout = timeout
		}
	}
}

// WithMaxRetries 设置最大重试次数
func WithMaxRetries(maxRetries int) ConfigOption {
	return func(c *manager) {
		if maxRetries >= 0 {
			c.MaxRetries = maxRetries
		}
	}
}

// WithRetryDelay 设置重试间隔
func WithRetryDelay(delay time.Duration) ConfigOption {
	return func(c *manager) {
		if delay > 0 {
			c.RetryDelay = delay
		}
	}
}

// WithAPIKey 设置 API 密钥
func WithAPIKey(apiKey string) ConfigOption {
	return func(c *manager) {
		c.APIKey = apiKey
	}
}

// WithUserAgent 设置 User-Agent 头
func WithUserAgent(userAgent string) ConfigOption {
	return func(c *manager) {
		if userAgent != "" {
			c.UserAgent = userAgent
		}
	}
}

// WithDebug 设置调试模式
func WithDebug(debug bool) ConfigOption {
	return func(c *manager) {
		c.Debug = debug
	}
}

// WithLogLevel 设置日志级别
func WithLogLevel(level string) ConfigOption {
	return func(c *manager) {
		if level != "" {
			c.LogLevel = level
		}
	}
}

// WithMaxConnections 设置最大连接数
func WithMaxConnections(maxConn int) ConfigOption {
	return func(c *manager) {
		if maxConn > 0 {
			c.MaxConnections = maxConn
		}
	}
}

// NewConfig 使用函数式选项创建新配置
func NewConfig(opts ...ConfigOption) *manager {
	cfg := DefaultConfig()
	for _, opt := range opts {
		opt(cfg)
	}
	return cfg
}

// NewConfigFromEnv 从环境变量创建配置（自动验证）
func NewConfigFromEnv() (*manager, error) {
	cfg := DefaultConfig()

	if v := os.Getenv("AGENTOS_ENDPOINT"); v != "" {
		cfg.Endpoint = v
	}
	if v := os.Getenv("AGENTOS_TIMEOUT"); v != "" {
		if d, err := time.ParseDuration(v); err == nil {
			cfg.Timeout = d
		}
	}
	if v := os.Getenv("AGENTOS_MAX_RETRIES"); v != "" {
		if n, err := strconv.Atoi(v); err == nil && n >= 0 {
			cfg.MaxRetries = n
		}
	}
	if v := os.Getenv("AGENTOS_RETRY_DELAY"); v != "" {
		if d, err := time.ParseDuration(v); err == nil {
			cfg.RetryDelay = d
		}
	}
	if v := os.Getenv("AGENTOS_API_KEY"); v != "" {
		cfg.APIKey = v
	}
	if v := os.Getenv("AGENTOS_DEBUG"); v != "" {
		cfg.Debug = v == "true" || v == "1"
	}
	if v := os.Getenv("AGENTOS_LOG_LEVEL"); v != "" {
		cfg.LogLevel = v
	}
	if v := os.Getenv("AGENTOS_MAX_CONNECTIONS"); v != "" {
		if n, err := strconv.Atoi(v); err == nil && n > 0 {
			cfg.MaxConnections = n
		}
	}
	if v := os.Getenv("AGENTOS_USER_AGENT"); v != "" {
		cfg.UserAgent = v
	}

	if err := cfg.Validate(); err != nil {
		return nil, err
	}
	return cfg, nil
}

// Validate 验证配置的合法性
func (c *manager) Validate() error {
	if c.Endpoint == "" {
		return ErrInvalidEndpoint
	}
	if parsed, err := url.Parse(c.Endpoint); err != nil || (parsed.Scheme != "http" && parsed.Scheme != "https") {
		return NewError(CodeInvalidEndpoint, "端点地址必须以 http:// 或 https:// 开头", nil)
	}
	if c.Timeout <= 0 {
		return NewError(CodeInvalidConfig, "超时时间必须大于零", nil)
	}
	if c.MaxConnections <= 0 {
		return NewError(CodeInvalidConfig, "最大连接数必须大于零", nil)
	}
	return nil
}

// Clone 创建配置的深拷贝
func (c *manager) Clone() *manager {
	return &manager{
		Endpoint:        c.Endpoint,
		Timeout:         c.Timeout,
		MaxRetries:      c.MaxRetries,
		RetryDelay:      c.RetryDelay,
		APIKey:          c.APIKey,
		UserAgent:       c.UserAgent,
		Debug:           c.Debug,
		LogLevel:        c.LogLevel,
		MaxConnections:  c.MaxConnections,
		IdleConnTimeout: c.IdleConnTimeout,
	}
}

// Merge 将 override 的非零值合并到当前配置（Debug 除外，始终取 override 值）
func (c *manager) Merge(override *manager) *manager {
	result := c.Clone()
	if override == nil {
		return result
	}

	if override.Endpoint != "" {
		result.Endpoint = override.Endpoint
	}
	if override.Timeout > 0 {
		result.Timeout = override.Timeout
	}
	if override.MaxRetries >= 0 {
		result.MaxRetries = override.MaxRetries
	}
	if override.RetryDelay > 0 {
		result.RetryDelay = override.RetryDelay
	}
	if override.APIKey != "" {
		result.APIKey = override.APIKey
	}
	if override.UserAgent != "" {
		result.UserAgent = override.UserAgent
	}
	result.Debug = override.Debug
	if override.LogLevel != "" {
		result.LogLevel = override.LogLevel
	}
	if override.MaxConnections > 0 {
		result.MaxConnections = override.MaxConnections
	}
	if override.IdleConnTimeout > 0 {
		result.IdleConnTimeout = override.IdleConnTimeout
	}

	return result
}

// String 返回配置的可读描述
func (c *manager) String() string {
	return fmt.Sprintf("manager[endpoint=%s, timeout=%v, retries=%d]", c.Endpoint, c.Timeout, c.MaxRetries)
}

