package client

import (
	"bytes"
	"context"
	"crypto/rand"
	"encoding/json"
	"fmt"
	"io"
	"math"
	"math/big"
	"net/http"
	"strings"
	"time"

	"github.com/spharx/agentos/toolkit/go/agentos"
	"github.com/spharx/agentos/toolkit/go/agentos/types"
	"github.com/spharx/agentos/toolkit/go/agentos/utils"
)

const MaxResponseBodySize = 10 * 1024 * 1024

type APIClient interface {
	Get(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error)
	Post(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error)
	Put(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error)
	Delete(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error)
}

type Client struct {
	config     *agentos.Config
	httpClient *http.Client
}

var _ APIClient = (*Client)(nil)

func NewClient(opts ...agentos.ConfigOption) (*Client, error) {
	config := agentos.NewConfig(opts...)
	if err := config.Validate(); err != nil {
		return nil, err
	}
	return newClientWithConfig(config)
}

func NewClientWithConfig(config *agentos.Config) (*Client, error) {
	if config == nil {
		config = agentos.DefaultConfig()
	}
	if err := config.Validate(); err != nil {
		return nil, err
	}
	return newClientWithConfig(config)
}

func newClientWithConfig(config *agentos.Config) (*Client, error) {
	return &Client{
		config: config,
		httpClient: &http.Client{
			Timeout: config.Timeout,
			Transport: &http.Transport{
				MaxIdleConns:       config.MaxConnections,
				IdleConnTimeout:    config.IdleConnTimeout,
				DisableCompression: false,
			},
		},
	}, nil
}

func (c *Client) GetConfig() *agentos.Config {
	return c.config.Clone()
}

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

func (c *Client) Close() error {
	if c.httpClient != nil {
		c.httpClient.CloseIdleConnections()
	}
	return nil
}

func (c *Client) String() string {
	return fmt.Sprintf("AgentOS Client[endpoint=%s, timeout=%v]", c.config.Endpoint, c.config.Timeout)
}

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
	req.Header.Set("X-Request-ID", generateRequestID())
	if c.config.APIKey != "" {
		req.Header.Set("Authorization", "Bearer "+c.config.APIKey)
	}
	for k, v := range options.Headers {
		req.Header.Set(k, v)
	}

	var lastErr error
	for attempt := 0; attempt <= c.config.MaxRetries; attempt++ {
		if attempt > 0 {
			delay := calculateBackoff(c.config.RetryDelay, attempt)

			select {
			case <-ctx.Done():
				return nil, agentos.WrapError(agentos.CodeTimeout, "请求在重试等待中被取消", ctx.Err())
			case <-time.After(delay):
			}

			if seeker, ok := req.Body.(io.Seeker); ok && req.Body != nil {
				if _, seekErr := seeker.Seek(0, io.SeekStart); seekErr != nil {
					return nil, agentos.WrapError(agentos.CodeNetworkError, "重置请求体失败", seekErr)
				}
			}
		}

		resp, err := c.httpClient.Do(req)
		if err != nil {
			lastErr = agentos.WrapError(agentos.CodeNetworkError, "请求执行失败", err)
			if ctx.Err() != nil {
				return nil, agentos.WrapError(agentos.CodeTimeout, "请求被取消", ctx.Err())
			}
			continue
		}

		respBody, readErr := io.ReadAll(io.LimitReader(resp.Body, MaxResponseBodySize))
		resp.Body.Close()

		if readErr != nil {
			lastErr = agentos.WrapError(agentos.CodeParseError, "读取响应失败", readErr)
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

func generateRequestID() string {
	timestamp := time.Now().UnixNano()
	randomNum, _ := rand.Int(rand.Reader, big.NewInt(1000000))
	return fmt.Sprintf("req-%d-%06d", timestamp, randomNum.Int64())
}

func calculateBackoff(baseDelay time.Duration, attempt int) time.Duration {
	backoff := float64(baseDelay) * math.Pow(2, float64(attempt-1))

	jitterBig, _ := rand.Int(rand.Reader, big.NewInt(int64(backoff)))
	jitter := time.Duration(jitterBig.Int64())

	return time.Duration(backoff) + jitter
}

func shouldRetry(statusCode int) bool {
	return statusCode >= 500 || statusCode == http.StatusTooManyRequests
}

func (c *Client) Get(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
	return c.request(ctx, http.MethodGet, path, nil, opts...)
}

func (c *Client) Post(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
	return c.request(ctx, http.MethodPost, path, body, opts...)
}

func (c *Client) Put(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
	return c.request(ctx, http.MethodPut, path, body, opts...)
}

func (c *Client) Delete(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
	return c.request(ctx, http.MethodDelete, path, nil, opts...)
}
