// AgentOS Go SDK - 客户端模块
// Version: 3.0.0
// Last updated: 2026-03-22
//
// 提供 HTTP 通信层、APIClient 接口定义和依赖倒转抽象。
// 对应 Python SDK: client/__init__.py + agent.py

package client

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strings"
	"time"

	"github.com/spharx/agentos/tools/go/agentos"
	"github.com/spharx/agentos/tools/go/agentos/types"
	"github.com/spharx/agentos/tools/go/agentos/utils"
)

// APIClient 定义所有 Manager 共同依赖的 HTTP 通信接口
type APIClient interface {
	Get(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error)
	Post(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error)
	Put(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error)
	Delete(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error)
}

// Client 是 AgentOS Go SDK 的核心 HTTP 客户端
type Client struct {
	config     *agentos.Config
	httpClient *http.Client
}

// 确保 Client 实现了 APIClient 接口（编译期检查）
var _ APIClient = (*Client)(nil)

// NewClient 使用函数式选项创建新的 HTTP 客户端
func NewClient(opts ...agentos.ConfigOption) (*Client, error) {
	config := agentos.NewConfig(opts...)
	if err := config.Validate(); err != nil {
		return nil, err
	}
	return newClientWithConfig(config)
}

// NewClientWithConfig 使用预构建的 Config 对象创建客户端
func NewClientWithConfig(config *agentos.Config) (*Client, error) {
	if config == nil {
		config = agentos.DefaultConfig()
	}
	if err := config.Validate(); err != nil {
		return nil, err
	}
	return newClientWithConfig(config)
}

// newClientWithConfig 内部构造函数
func newClientWithConfig(config *agentos.Config) (*Client, error) {
	return &Client{
		config: config,
		httpClient: &http.Client{
			Timeout: config.Timeout,
			Transport: &http.Transport{
				MaxIdleConns:        config.MaxConnections,
				IdleConnTimeout:     config.IdleConnTimeout,
				DisableCompression:  false,
			},
		},
	}, nil
}

// GetConfig 返回当前客户端的配置副本
func (c *Client) GetConfig() *agentos.Config {
	return c.config.Clone()
}

// Health 检查 AgentOS 服务的健康状态
func (c *Client) Health(ctx context.Context) (*types.HealthStatus, error) {
	resp, err := c.Get(ctx, "/health")
	if err != nil {
		return nil, err
	}

	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return nil, agentos.NewError(agentos.CodeInvalidResponse, "健康检查响应格式异常", nil)
	}

	return &types.HealthStatus{
		Status:    utils.GetString(data, "status"),
		Version:   utils.GetString(data, "version"),
		Uptime:    utils.GetInt64(data, "uptime"),
		Checks:    utils.GetStringMap(data, "checks"),
		Timestamp: time.Now(),
	}, nil
}

// Metrics 获取 AgentOS 系统运行指标
func (c *Client) Metrics(ctx context.Context) (*types.Metrics, error) {
	resp, err := c.Get(ctx, "/metrics")
	if err != nil {
		return nil, err
	}

	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return nil, agentos.NewError(agentos.CodeInvalidResponse, "指标响应格式异常", nil)
	}

	return &types.Metrics{
		TasksTotal:       utils.GetInt64(data, "tasks_total"),
		TasksCompleted:   utils.GetInt64(data, "tasks_completed"),
		TasksFailed:      utils.GetInt64(data, "tasks_failed"),
		MemoriesTotal:    utils.GetInt64(data, "memories_total"),
		SessionsActive:   utils.GetInt64(data, "sessions_active"),
		SkillsLoaded:     utils.GetInt64(data, "skills_loaded"),
		CPUUsage:         utils.GetFloat64(data, "cpu_usage"),
		MemoryUsage:      utils.GetFloat64(data, "memory_usage"),
		RequestCount:     utils.GetInt64(data, "request_count"),
		AverageLatencyMs: utils.GetFloat64(data, "average_latency_ms"),
	}, nil
}

// Close 关闭客户端，释放 HTTP 连接池资源
func (c *Client) Close() error {
	if c.httpClient != nil {
		c.httpClient.CloseIdleConnections()
	}
	return nil
}

// String 返回客户端的可读描述
func (c *Client) String() string {
	return fmt.Sprintf("AgentOS Client[endpoint=%s, timeout=%v]", c.config.Endpoint, c.config.Timeout)
}

// ============================================================
// HTTP 通信实现 (APIClient 接口)
// ============================================================

// request 执行底层 HTTP 请求，包含序列化、重试和响应解析逻辑
func (c *Client) request(ctx context.Context, method, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
	options := &types.RequestOptions{}
	for _, opt := range opts {
		opt(options)
	}

	fullURL := strings.TrimRight(c.config.Endpoint, "/") + utils.BuildURL(path, options.QueryParams)

	var reqBody io.Reader
	if body != nil {
		jsonData, err := json.Marshal(body)
		if err != nil {
			return nil, agentos.WrapError(agentos.CodeParseError, "序列化请求体失败", err)
		}
		reqBody = bytes.NewReader(jsonData)
	}

	req, err := http.NewRequestWithContext(ctx, method, fullURL, reqBody)
	if err != nil {
		return nil, agentos.WrapError(agentos.CodeNetworkError, "创建请求失败", err)
	}

	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("User-Agent", c.config.UserAgent)
	if c.config.APIKey != "" {
		req.Header.Set("Authorization", "Bearer "+c.config.APIKey)
	}
	for k, v := range options.Headers {
		req.Header.Set(k, v)
	}

	var lastErr error
	for attempt := 0; attempt <= c.config.MaxRetries; attempt++ {
		if attempt > 0 {
			time.Sleep(c.config.RetryDelay)
		}

		resp, err := c.httpClient.Do(req)
		if err != nil {
			lastErr = agentos.WrapError(agentos.CodeNetworkError, "请求执行失败", err)
			continue
		}
		defer resp.Body.Close()

		respBody, err := io.ReadAll(resp.Body)
		if err != nil {
			lastErr = agentos.WrapError(agentos.CodeParseError, "读取响应失败", err)
			continue
		}

		if resp.StatusCode >= 400 {
			lastErr = agentos.HTTPStatusToError(resp.StatusCode, string(respBody))
			if !shouldRetry(resp.StatusCode) {
				return nil, lastErr
			}
			continue
		}

		var apiResp types.APIResponse
		if err := json.Unmarshal(respBody, &apiResp); err != nil {
			return nil, agentos.WrapError(agentos.CodeParseError, "解析响应失败", err)
		}

		return &apiResp, nil
	}

	return nil, lastErr
}

// shouldRetry 根据 HTTP 状态码判断是否应进行重试
func shouldRetry(statusCode int) bool {
	return statusCode >= 500 || statusCode == http.StatusTooManyRequests
}

// Get 执行 HTTP GET 请求
func (c *Client) Get(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
	return c.request(ctx, http.MethodGet, path, nil, opts...)
}

// Post 执行 HTTP POST 请求
func (c *Client) Post(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
	return c.request(ctx, http.MethodPost, path, body, opts...)
}

// Put 执行 HTTP PUT 请求
func (c *Client) Put(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
	return c.request(ctx, http.MethodPut, path, body, opts...)
}

// Delete 执行 HTTP DELETE 请求
func (c *Client) Delete(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
	return c.request(ctx, http.MethodDelete, path, nil, opts...)
}
