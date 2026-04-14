// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: MIT
/**
 * @file protocol.go
 * @brief AgentOS Go SDK — Protocol Client Module
 *
 * 提供多协议客户端支持，允许 SDK 用户通过统一接口与不同协议后端交互：
 * - JSON-RPC 2.0 (默认)
 * - MCP (Model Context Protocol) v1.0
 * - A2A (Agent-to-Agent) v0.3
 * - OpenAI API 兼容
 *
 * @since 3.0.0
 */

package agentos

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strings"
	"time"
)

// ProtocolType represents the supported protocol types
type ProtocolType int

const (
	ProtocolJSONRPC ProtocolType = iota
	ProtocolMCP
	ProtocolA2A
	ProtocolOpenAI
	ProtocolAutoDetect // Auto-detect from endpoint/content
)

func (p ProtocolType) String() string {
	switch p {
	case ProtocolJSONRPC:
		return "jsonrpc"
	case ProtocolMCP:
		return "mcp"
	case ProtocolA2A:
		return "a2a"
	case ProtocolOpenAI:
		return "openai"
	default:
		return "auto"
	}
}

// ProtocolConfig holds configuration for a protocol client
type ProtocolConfig struct {
	Type         ProtocolType
	Endpoint     string
	APIKey       string
	Timeout      time.Duration
	RetryCount   int
	RetryDelay   time.Duration
	EnableStream bool
	Headers      map[string]string
}

// DefaultProtocolConfig returns sensible defaults for protocol clients
func DefaultProtocolConfig() *ProtocolConfig {
	return &ProtocolConfig{
		Type:        ProtocolJSONRPC,
		Endpoint:    "http://localhost:18789",
		Timeout:     30 * time.Second,
		RetryCount:  3,
		RetryDelay:  1 * time.Second,
		Headers:     make(map[string]string),
	}
}

// NewProtocolConfigFromEnv creates config from environment variables
func NewProtocolConfigFromEnv() *ProtocolConfig {
	cfg := DefaultProtocolConfig()
	if ep := getEnv("AGENTOS_ENDPOINT", ""); ep != "" {
		cfg.Endpoint = ep
	}
	if key := getEnv("AGENTOS_API_KEY", ""); key != "" {
		cfg.APIKey = key
	}
	return cfg
}

// ProtocolClient provides unified multi-protocol client interface
type ProtocolClient struct {
	config    *ProtocolConfig
	httpClient *http.Client
}

// NewProtocolClient creates a new protocol client with given configuration
func NewProtocolClient(config *ProtocolConfig) (*ProtocolClient, error) {
	if config == nil {
		config = DefaultProtocolConfig()
	}
	client := &ProtocolClient{
		config: config,
		httpClient: &http.Client{
			Timeout: config.Timeout,
			Transport: &http.Transport{
				MaxIdleConns:        10,
				IdleConnTimeout:     90 * time.Second,
				DisableCompression: false,
			},
		},
	}
	return client, nil
}

// DetectProtocol auto-detects the appropriate protocol type based on context
func (c *ProtocolClient) DetectProtocol(ctx context.Context) (ProtocolType, float64, error) {
	req, err := http.NewRequestWithContext(ctx, http.MethodGet,
		c.config.Endpoint+"/api/v1/protocols", nil)
	if err != nil {
		return ProtocolJSONRPC, 0, fmt.Errorf("create request: %w", err)
	}
	c.setAuthHeaders(req)

	resp, err := c.httpClient.Do(req)
	if err != nil {
		return ProtocolJSONRPC, 50.0, err
	}
	defer resp.Body.Close()

	body, _ := io.ReadAll(resp.Body)

	contentType := resp.Header.Get("Content-Type")
	confidence := 50.0
	detected := ProtocolJSONRPC

	switch {
	case strings.Contains(contentType, "application/json"):
		confidence += 20.0
		if bytes.Contains(body, []byte("\"tools/call\"")) ||
			bytes.Contains(body, []byte("\"method\":\"tools/list\"")) {
			detected = ProtocolMCP
			confidence += 30.0
		} else if bytes.Contains(body, []byte("\"task/delegate\"")) ||
			bytes.Contains(body, []byte("\"agent/discover\"")) {
			detected = ProtocolA2A
			confidence += 30.0
		} else if bytes.Contains(body, []byte("\"model\"")) &&
			bytes.Contains(body, []byte("\"choices\"")) {
			detected = ProtocolOpenAI
			confidence += 25.0
		} else if bytes.Contains(body, []byte("\"jsonrpc\")) {
			detected = ProtocolJSONRPC
			confidence += 40.0
		}
	case strings.Contains(contentType, "text/event-stream"):
		confidence += 15.0
		detected = ProtocolOpenAI
	}

	if confidence > 100.0 {
		confidence = 100.0
	}
	return detected, confidence, nil
}

// SendRequest sends a unified request through the configured protocol
func (c *ProtocolClient) SendRequest(ctx context.Context,
	method string, params map[string]interface{}) ([]byte, error) {

	var payload []byte
	var reqPath string

	switch c.config.Type {
	case ProtocolOpenAI:
		reqPath = "/v1/chat/completions"
		payload = c.buildOpenAIRequest(method, params)
	case ProtocolMCP:
		reqPath = "/api/v1/invoke"
		payload = c.buildMCPRequest(method, params)
	case ProtocolA2A:
		reqPath = "/api/v1/invoke"
		payload = c.buildA2ARequest(method, params)
	default:
		reqPath = "/rpc"
		payload = c.buildJSONRPCRequest(method, params)
	}

	url := fmt.Sprintf("%s%s", c.config.Endpoint, reqPath)
	var lastErr error

	for attempt := 0; attempt <= c.config.RetryCount; attempt++ {
		if attempt > 0 {
			select {
			case <-ctx.Done():
				return nil, ctx.Err()
			case <-time.After(c.config.RetryDelay):
				c.config.RetryDelay *= 2
			}
		}

		result, err := c.doHTTPRequest(ctx, url, payload)
		if err == nil {
			return result, nil
		}
		lastErr = err

		var apiErr Error
		if errors.As(err, &apiErr) && !IsRetryableError(apiErr) {
			break
		}
	}

	return nil, lastErr
}

// StreamRequest sends a streaming request and delivers chunks via callback
func (c *ProtocolClient) StreamRequest(ctx context.Context,
	method string, params map[string]interface{},
	onChunk func([]byte) error) error {

	if !c.config.EnableStream {
		data, err := c.SendRequest(ctx, method, params)
		if err != nil {
			return err
		}
		return onChunk(data)
	}

	var url string
	var payload []byte

	switch c.config.Type {
	case ProtocolOpenAI:
		url = fmt.Sprintf("%s/v1/chat/completions", c.config.Endpoint)
		payload = c.buildOpenAIRequest(method, params)
	default:
		url = fmt.Sprintf("%s/rpc", c.config.Endpoint)
		payload = c.buildJSONRPCRequest(method, params)
	}

	req, err := http.NewRequestWithContext(ctx, http.MethodPost, url,
		bytes.NewReader(payload))
	if err != nil {
		return fmt.Errorf("create stream request: %w", err)
	}
	c.setAuthHeaders(req)
	req.Header.Set("Accept", "text/event-stream")
	req.Header.Set("Cache-Control", "no-cache")

	resp, err := c.httpClient.Do(req)
	if err != nil {
		return fmt.Errorf("stream request failed: %w", err)
	}
	defer resp.Body.Close()

	buf := make([]byte, 4096)
	for {
		n, err := resp.Body.Read(buf)
		if n > 0 {
			chunk := make([]byte, n)
			copy(chunk, buf[:n])
			if err := onChunk(chunk); err != nil {
				return err
			}
		}
		if err == io.EOF {
			break
		}
		if err != nil {
			return fmt.Errorf("read stream chunk: %w", err)
		}
		select {
		case <-ctx.Done():
			return ctx.Err()
		default:
		}
	}
	return nil
}

// ListProtocols queries available protocols from the gateway
func (c *ProtocolClient) ListProtocols(ctx context.Context) ([]string, error) {
	req, err := http.NewRequestWithContext(ctx, http.MethodGet,
		fmt.Sprintf("%s/api/v1/protocols", c.config.Endpoint), nil)
	if err != nil {
		return nil, err
	}
	c.setAuthHeaders(req)

	resp, err := c.httpClient.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	body, _ := io.ReadAll(resp.Body)
	var result struct {
		Protocols []string `json:"protocols"`
	}
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, fmt.Errorf("parse protocols list: %w", err)
	}
	return result.Protocols, nil
}

// TestConnection tests connectivity to each registered protocol
func (c *ProtocolClient) TestConnection(ctx context.Context,
	protocolName string) (map[string]interface{}, error) {

	path := fmt.Sprintf("/api/v1/protocols/%s/test", protocolName)
	url := fmt.Sprintf("%s%s", c.config.Endpoint, path)

	req, err := http.NewRequestWithContext(ctx, http.MethodGet, url, nil)
	if err != nil {
		return nil, err
	}
	c.setAuthHeaders(req)

	start := time.Now()
	resp, err := c.httpClient.Do(req)
	latency := time.Since(start)
	if err != nil {
		return map[string]interface{}{
			"protocol":  protocolName,
			"status":    "error",
			"latency_ms": latency.Milliseconds(),
			"error":     err.Error(),
		}, nil
	}
	defer resp.Body.Close()

	body, _ := io.ReadAll(resp.Body)
	var result map[string]interface{}
	json.Unmarshal(body, &result)

	if result == nil {
		result = make(map[string]interface{})
	}
	result["protocol"] = protocolName
	result["status_code"] = resp.StatusCode
	result["latency_ms"] = latency.Milliseconds()

	return result, nil
}

// GetCapabilities returns capabilities of a specific protocol adapter
func (c *ProtocolClient) GetCapabilities(ctx context.Context,
	protocolName string) (map[string]interface{}, error) {

	path := fmt.Sprintf("/api/v1/protocols/%s/capabilities", protocolName)
	url := fmt.Sprintf("%s%s", c.config.Endpoint, path)

	req, err := http.NewRequestWithContext(ctx, http.MethodGet, url, nil)
	if err != nil {
		return nil, err
	}
	c.setAuthHeaders(req)

	resp, err := c.httpClient.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	body, _ := io.ReadAll(resp.Body)
	var result map[string]interface{}
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, fmt.Errorf("parse capabilities: %w", err)
	}
	return result, nil
}

// ============================================================================
// Internal helpers
// ============================================================================

func (c *ProtocolClient) setAuthHeaders(req *http.Request) {
	for k, v := range c.config.Headers {
		req.Header.Set(k, v)
	}
	if c.config.APIKey != "" {
		req.Header.Set("Authorization", "Bearer "+c.config.APIKey)
	}
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Accept", "application/json")
}

func (c *ProtocolClient) doHTTPRequest(ctx context.Context, url string,
	payload []byte) ([]byte, error) {

	req, err := http.NewRequestWithContext(ctx, http.MethodPost, url,
		bytes.NewReader(payload))
	if err != nil {
		return nil, fmt.Errorf("create request: %w", err)
	}
	c.setAuthHeaders(req)

	resp, err := c.httpClient.Do(req)
	if err != nil {
		return nil, NewError(ErrorCodeNetwork, fmt.Sprintf("HTTP request failed: %v", err))
	}
	defer resp.Body.Close()

	body, _ := io.ReadAll(resp.Body)

	if resp.StatusCode >= 400 {
		var apiErrResp struct {
			Error struct {
				Code    int    `json:"code"`
				Message string `json:"message"`
			} `json:"error"`
		}
		json.Unmarshal(body, &apiErrResp)

		errCode := ErrorCodeServer
		msg := fmt.Sprintf("HTTP %d", resp.StatusCode)
		if apiErrResp.Error.Message != "" {
			msg = apiErrResp.Error.Message
		}
		if resp.StatusCode >= 400 && resp.StatusCode < 500 {
			errCode = ErrorCodeInvalidParameter
		}

		return nil, NewError(errCode, msg)
	}
	return body, nil
}

func (c *ProtocolClient) buildJSONRPCRequest(
	method string, params map[string]interface{}) []byte {

	req := map[string]interface{}{
		"jsonrpc": "2.0",
		"id":      generateID(),
		"method":  method,
		"params":  params,
	}
	data, _ := json.Marshal(req)
	return data
}

func (c *ProtocolClient) buildMCPRequest(
	method string, params map[string]interface{}) []byte {

	mcpMethod := method
	parts := strings.SplitN(method, ".", 2)
	if len(parts) == 2 {
		mcpMethod = parts[1]
	}

	req := map[string]interface{}{
		"protocol": "mcp",
		"version":  "1.0",
		"method":   mcpMethod,
		"params":   params,
	}
	data, _ := json.Marshal(req)
	return data
}

func (c *ProtocolClient) buildA2ARequest(
	method string, params map[string]interface{}) []byte {

	a2aMethod := method
	switch method {
	case "agent.list":
		a2aMethod = "agent/discover"
	case "task.create":
		a2aMethod = "task/delegate"
	}

	req := map[string]interface{}{
		"protocol": "a2a",
		"version":  "0.3",
		"method":   a2aMethod,
		"params":   params,
	}
	data, _ := json.Marshal(req)
	return data
}

func (c *ProtocolClient) buildOpenAIRequest(
	method string, params map[string]interface{}) []byte {

	messagesRaw, _ := params["messages"]
	model, _ := params["model"].(string)
	if model == "" {
		model = "gpt-4o"
	}

	temperature, _ := params["temperature"].(float64)
	maxTokens, _ := params["max_tokens"].(int)

	req := map[string]interface{}{
		"model":       model,
		"messages":    messagesRaw,
		"temperature": temperature,
		"max_tokens":  maxTokens,
		"stream":     c.config.EnableStream,
	}
	data, _ := json.Marshal(req)
	return data
}

func getEnv(key, fallback string) string {
	if val := os.Getenv(key); val != "" {
		return val
	}
	return fallback
}
