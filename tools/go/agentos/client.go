// AgentOS Go SDK Client
// Version: 1.0.0.5
// Last updated: 2026-03-21

package agentos

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"net/http"
	"strconv"
	"time"
)

// Client represents the AgentOS client
type Client struct {
	endpoint string
	timeout  time.Duration
	client   *http.Client
}

// NewClient creates a new AgentOS client
func NewClient(endpoint string) (*Client, error) {
	if endpoint == "" {
		endpoint = "http://localhost:18789"
	}
	
	return &Client{
		endpoint: endpoint,
		timeout:  30 * time.Second,
		client: &http.Client{
			Timeout: 30 * time.Second,
		},
	}, nil
}

// NewClientWithTimeout creates a new AgentOS client with a custom timeout
func NewClientWithTimeout(endpoint string, timeout time.Duration) (*Client, error) {
	if endpoint == "" {
		endpoint = "http://localhost:18789"
	}
	
	return &Client{
		endpoint: endpoint,
		timeout:  timeout,
		client: &http.Client{
			Timeout: timeout,
		},
	}, nil
}

// request makes an HTTP request to the AgentOS server
func (c *Client) request(ctx context.Context, method, path string, data interface{}) (map[string]interface{}, error) {
	url := c.endpoint + path
	
	var body []byte
	var err error
	if data != nil {
		body, err = json.Marshal(data)
		if err != nil {
			return nil, fmt.Errorf("error marshaling data: %w", err)
		}
	}
	
	req, err := http.NewRequestWithContext(ctx, method, url, nil)
	if err != nil {
		return nil, fmt.Errorf("error creating request: %w", err)
	}
	
	req.Header.Set("Content-Type", "application/json")
	if body != nil {
		req.Body = jsonReader(body)
	}
	
	resp, err := c.client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("error making request: %w", err)
	}
	defer resp.Body.Close()
	
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("server returned error: %s", resp.Status)
	}
	
	var result map[string]interface{}
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return nil, fmt.Errorf("error decoding response: %w", err)
	}
	
	return result, nil
}

// SubmitTask submits a task to the AgentOS system
func (c *Client) SubmitTask(ctx context.Context, description string) (string, error) {
	data := map[string]interface{}{
		"description": description,
	}
	
	response, err := c.request(ctx, "POST", "/api/tasks", data)
	if err != nil {
		return "", err
	}
	
	taskID, ok := response["task_id"].(string)
	if !ok {
		return "", errors.New("invalid response: missing task_id")
	}
	
	return taskID, nil
}

// WriteMemory writes a memory to the AgentOS system
func (c *Client) WriteMemory(ctx context.Context, content string, metadata map[string]interface{}) (string, error) {
	if metadata == nil {
		metadata = make(map[string]interface{})
	}
	
	data := map[string]interface{}{
		"content":  content,
		"metadata": metadata,
	}
	
	response, err := c.request(ctx, "POST", "/api/memories", data)
	if err != nil {
		return "", err
	}
	
	memoryID, ok := response["memory_id"].(string)
	if !ok {
		return "", errors.New("invalid response: missing memory_id")
	}
	
	return memoryID, nil
}

// SearchMemory searches memories in the AgentOS system
func (c *Client) SearchMemory(ctx context.Context, query string, topK int) ([]Memory, error) {
	path := fmt.Sprintf("/api/memories/search?query=%s&top_k=%d", query, topK)
	
	response, err := c.request(ctx, "GET", path, nil)
	if err != nil {
		return nil, err
	}
	
	memoriesData, ok := response["memories"].([]interface{})
	if !ok {
		return nil, errors.New("invalid response: missing memories")
	}
	
	memories := make([]Memory, 0, len(memoriesData))
	for _, memData := range memoriesData {
		memMap, ok := memData.(map[string]interface{})
		if !ok {
			continue
		}
		
		memory := Memory{
			ID:        getString(memMap, "memory_id"),
			Content:   getString(memMap, "content"),
			CreatedAt: getString(memMap, "created_at"),
			Metadata:  getMap(memMap, "metadata"),
		}
		memories = append(memories, memory)
	}
	
	return memories, nil
}

// GetMemory gets a memory by ID
func (c *Client) GetMemory(ctx context.Context, memoryID string) (Memory, error) {
	path := fmt.Sprintf("/api/memories/%s", memoryID)
	
	response, err := c.request(ctx, "GET", path, nil)
	if err != nil {
		return Memory{}, err
	}
	
	memory := Memory{
		ID:        getString(response, "memory_id"),
		Content:   getString(response, "content"),
		CreatedAt: getString(response, "created_at"),
		Metadata:  getMap(response, "metadata"),
	}
	
	return memory, nil
}

// DeleteMemory deletes a memory by ID
func (c *Client) DeleteMemory(ctx context.Context, memoryID string) (bool, error) {
	path := fmt.Sprintf("/api/memories/%s", memoryID)
	
	response, err := c.request(ctx, "DELETE", path, nil)
	if err != nil {
		return false, err
	}
	
	success, ok := response["success"].(bool)
	if !ok {
		return false, errors.New("invalid response: missing success")
	}
	
	return success, nil
}

// CreateSession creates a new session
func (c *Client) CreateSession(ctx context.Context) (string, error) {
	response, err := c.request(ctx, "POST", "/api/sessions", nil)
	if err != nil {
		return "", err
	}
	
	sessionID, ok := response["session_id"].(string)
	if !ok {
		return "", errors.New("invalid response: missing session_id")
	}
	
	return sessionID, nil
}

// LoadSkill loads a skill by name
func (c *Client) LoadSkill(ctx context.Context, skillName string) (*Skill, error) {
	return &Skill{client: c, name: skillName}, nil
}

// Helper functions
func getString(m map[string]interface{}, key string) string {
	if v, ok := m[key]; ok {
		if s, ok := v.(string); ok {
			return s
		}
	}
	return ""
}

func getMap(m map[string]interface{}, key string) map[string]interface{} {
	if v, ok := m[key]; ok {
		if m, ok := v.(map[string]interface{}); ok {
			return m
		}
	}
	return make(map[string]interface{})
}

// jsonReader is a helper to create a reader from a byte slice
func jsonReader(data []byte) *jsonReaderImpl {
	return &jsonReaderImpl{
		data: data,
		pos:  0,
	}
}

type jsonReaderImpl struct {
	data []byte
	pos  int
}

func (r *jsonReaderImpl) Read(p []byte) (n int, err error) {
	if r.pos >= len(r.data) {
		return 0, nil
	}
	
	n = copy(p, r.data[r.pos:])
	r.pos += n
	return n, nil
}

func (r *jsonReaderImpl) Close() error {
	return nil
}
