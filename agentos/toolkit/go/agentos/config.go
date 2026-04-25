package agentos

import (
	"fmt"
	"net/url"
	"os"
	"strconv"
	"time"
)

type Config struct {
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

type ConfigOption func(*Config)

func DefaultConfig() *Config {
	return &Config{
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

func WithEndpoint(endpoint string) ConfigOption {
	return func(c *Config) {
		if endpoint != "" {
			c.Endpoint = endpoint
		}
	}
}

func WithTimeout(timeout time.Duration) ConfigOption {
	return func(c *Config) {
		if timeout > 0 {
			c.Timeout = timeout
		}
	}
}

func WithMaxRetries(maxRetries int) ConfigOption {
	return func(c *Config) {
		if maxRetries >= 0 {
			c.MaxRetries = maxRetries
		}
	}
}

func WithRetryDelay(delay time.Duration) ConfigOption {
	return func(c *Config) {
		if delay > 0 {
			c.RetryDelay = delay
		}
	}
}

func WithAPIKey(apiKey string) ConfigOption {
	return func(c *Config) {
		c.APIKey = apiKey
	}
}

func WithUserAgent(userAgent string) ConfigOption {
	return func(c *Config) {
		if userAgent != "" {
			c.UserAgent = userAgent
		}
	}
}

func WithDebug(debug bool) ConfigOption {
	return func(c *Config) {
		c.Debug = debug
	}
}

func WithLogLevel(level string) ConfigOption {
	return func(c *Config) {
		if level != "" {
			c.LogLevel = level
		}
	}
}

func WithMaxConnections(maxConn int) ConfigOption {
	return func(c *Config) {
		if maxConn > 0 {
			c.MaxConnections = maxConn
		}
	}
}

func NewConfig(opts ...ConfigOption) *Config {
	cfg := DefaultConfig()
	for _, opt := range opts {
		opt(cfg)
	}
	return cfg
}

func NewConfigFromEnv() (*Config, error) {
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

func (c *Config) Validate() error {
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

func (c *Config) Clone() *Config {
	return &Config{
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

func (c *Config) Merge(override *Config) *Config {
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

func (c *Config) String() string {
	return fmt.Sprintf("Config[endpoint=%s, timeout=%v, retries=%d]", c.Endpoint, c.Timeout, c.MaxRetries)
}
