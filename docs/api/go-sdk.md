Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# AgentOS Go SDK

**版本**: 1.0.0
**最后更新**: 2026-04-06
**状态**: 生产就绪
**Go 版本**: 1.16+
**核心文档**: [Go SDK 规范](../../agentos/manuals/api/toolkit/go/README.md)

---

## 📦 安装

### 获取依赖

```bash
go get github.com/spharx/agentos-go-sdk
```

### 导入包

```go
package main

import (
    "fmt"
    agentos "github.com/spharx/agentos-go-sdk"
)
```

---

## 🚀 快速开始

### 1. 初始化客户端

```go
package main

import (
    "context"
    "log"
    "time"

    agentos "github.com/spharx/agentos-go-sdk"
)

func main() {
    // 创建客户端配置
    cfg := &agentos.Config{
        BaseURL:   "http://localhost:8080",
        APIKey:    "your-api-key",
        Timeout:   30 * time.Second,
        MaxRetries: 3,
    }

    // 创建客户端
    client, err := agentos.NewClient(cfg)
    if err != nil {
        log.Fatalf("Failed to create client: %v", err)
    }
    defer client.Close()

    // 健康检查
    ctx := context.Background()
    health, err := client.HealthCheck(ctx)
    if err != nil {
        log.Fatalf("Health check failed: %v", err)
    }
    fmt.Printf("System status: %s\n", health.Status)
}
```

### 2. 创建 Agent

```go
func createAgentExample(client *agentos.Client) {
    ctx := context.Background()

    // 配置 Agent
    cfg := &agentos.AgentConfig{
        Name:        "my-assistant",
        Description: "A helpful AI assistant",
        Model:       "gpt-4-turbo",
        MaxTokens:   4096,
        Temperature: 0.7,
        Tools:       []string{"web-search", "code-executor"},
    }

    // 创建 Agent
    agent, err := client.CreateAgent(ctx, cfg)
    if err != nil {
        log.Fatalf("Failed to create agent: %v", err)
    }

    fmt.Printf("Agent created with ID: %s\n", agent.ID)
}
```

### 3. 与 Agent 对话

```go
func chatExample(agent *agentos.Agent) {
    ctx := context.Background()

    // 同步对话
    resp, err := agent.Chat(ctx, &agentos.ChatRequest{
        Message: "Hello, how are you?",
    })
    if err != nil {
        log.Fatalf("Chat failed: %v", err)
    }
    fmt.Printf("Response: %s\n", resp.Content)

    // 流式对话
    stream, err := agent.ChatStream(ctx, &agentos.ChatRequest{
        Message: "Tell me a long story",
    })
    if err != nil {
        log.Fatalf("Stream failed: %v", err)
    }

    for {
        chunk, err := stream.Recv()
        if err == io.EOF {
            break
        }
        if err != nil {
            log.Fatalf("Stream error: %v", err)
        }
        fmt.Print(chunk.Content)
    }
}
```

---

## 📚 核心 API 参考

### Client - 客户端结构体

```go
package agentos

type Config struct {
    BaseURL    string        // 内核服务地址
    APIKey     string        // API 密钥
    Timeout    time.Duration // 请求超时
    MaxRetries int           // 最大重试次数
    Logger     Logger        // 自定义日志器
}

type Client struct {
    config     *Config
    httpClient *http.Client
    memory     *MemoryClient
    task       *TaskClient
}

// NewClient 创建新的客户端实例
func NewClient(cfg *Config) (*Client, error)

// Close 关闭客户端连接
func (c *Client) Close() error

// HealthCheck 系统健康检查
func (c *Client) HealthCheck(ctx context.Context) (*HealthStatus, error)

// CreateAgent 创建新 Agent
func (c *Client) CreateAgent(ctx context.Context, cfg *AgentConfig) (*Agent, error)

// GetAgent 获取已存在的 Agent
func (c *Client) GetAgent(ctx context.Context, agentID string) (*Agent, error)

// ListAgents 列出所有 Agent
func (c *Client) ListAgents(ctx context.Context, opts *ListOptions) ([]*Agent, error)

// Memory 返回记忆操作客户端
func (c *Client) Memory() *MemoryClient

// Task 返回任务管理客户端
func (c *Client) Task() *TaskClient
```

### Agent - Agent 结构体

```go
type Agent struct {
    ID          string
    Name        string
    Description string
    Status      AgentStatus
    client      *Client
}

// Start 启动 Agent
func (a *Agent) Start(ctx context.Context) error

// Stop 停止 Agent
func (a *Agent) Stop(ctx context.Context) error

// Chat 同步对话
func (a *Agent) Chat(ctx context.Context, req *ChatRequest) (*ChatResponse, error)

// ChatStream 流式对话
func (a *Agent) ChatStream(ctx context.Context, req *ChatRequest) (*ChatStream, error)

// ExecuteTask 执行任务
func (a *Agent) ExecuteTask(ctx context.Context, taskName string, params map[string]interface{}) (*TaskResult, error)

// GetSession 获取当前会话
func (a *Agent) GetSession(ctx context.Context) (*Session, error)
```

### ChatRequest / ChatResponse

```go
type ChatRequest struct {
    Message      string                 // 用户消息
    SystemPrompt string                 // 系统提示词
    Context      map[string]interface{} // 上下文
    Temperature  float64                // 温度 [0, 2]
    MaxTokens    int                    // 最大 Token 数
    Stream       bool                   // 是否流式输出
}

type ChatResponse struct {
    ID         string    `json:"id"`
    Content    string    `json:"content"`
    Role       string    `json:"role"`
    FinishReason string  `json:"finish_reason"`
    Usage      *Usage    `json:"usage"`
    CreatedAt  time.Time `json:"created_at"`
}

type Usage struct {
    PromptTokens     int `json:"prompt_tokens"`
    CompletionTokens int `json:"completion_tokens"`
    TotalTokens      int `json:"total_tokens"`
}
```

### MemoryClient - 记忆操作

```go
type MemoryClient struct {
    client *Client
}

// Store 存储数据到记忆系统
func (m *MemoryClient) Store(ctx context.Context, req *StoreRequest) (*StoreResponse, error)

// Search 语义搜索记忆
func (m *MemoryClient) Search(ctx context.Context, req *SearchRequest) (*SearchResponse, error)

// GetByID 根据 ID 获取记录
func (m *MemoryClient) GetByID(ctx context.Context, id string) (*MemoryRecord, error)

// Delete 删除记录
func (m *MemoryClient) Delete(ctx context.Context, id string) error

// Count 统计记录数
func (m *MemoryClient) Count(ctx context.Context, filters map[string]string) (int64, error)

// BatchStore 批量存储
func (m *MemoryClient) BatchStore(ctx context.Context, records []*StoreRequest) ([]string, error)

// BatchSearch 批量搜索
func (m *MemoryClient) BatchSearch(ctx context.Context, queries []string, topK int) ([][]*MemoryRecord, error)
```

### TaskClient - 任务管理

```go
type TaskClient struct {
    client *Client
}

// Create 创建任务
func (t *TaskClient) Create(ctx context.Context, req *CreateTaskRequest) (*Task, error)

// GetStatus 查询任务状态
func (t *TaskClient) GetStatus(ctx context.Context, taskID string) (*TaskStatus, error)

// Cancel 取消任务
func (t *TaskClient) Cancel(ctx context.Context, taskID string) error

// ListTasks 列出任务
func (t *TaskClient) ListTasks(ctx context.Context, opts *ListTaskOptions) ([]*Task, error)
```

---

## 🔧 高级用法

### Context 超时控制

```go
func withTimeoutExample(client *agentos.Client) {
    // 设置 5 秒超时
    ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
    defer cancel()

    _, err := client.HealthCheck(ctx)
    if err == context.DeadlineExceeded {
        fmt.Println("Request timed out")
    }
}
```

### 错误处理

```go
package main

import (
    "errors"
    "fmt"

    agentos "github.com/spharx/agentos-go-sdk"
)

var (
    ErrAuthFailed      = errors.New("authentication failed")
    ErrRateLimited     = errors.New("rate limit exceeded")
    ErrNotFound        = errors.New("resource not found")
    ErrTimeout         = errors.New("request timeout")
    ErrInternal        = errors.New("internal server error")
)

func handleError(err error) {
    var agentErr *agentos.Error

    if errors.As(err, &agentErr) {
        switch agentErr.Code {
        case 401:
            fmt.Println("认证失败，请检查 API Key")
        case 429:
            fmt.Printf("请求频率限制，请等待 %d 秒\n", agentErr.RetryAfter)
        case 404:
            fmt.Println("资源不存在")
        default:
            fmt.Printf("错误 [%d]: %s\n", agentErr.Code, agentErr.Message)
        }
    } else {
        fmt.Printf("未知错误: %v\n", err)
    }
}
```

### 自定义 HTTP Transport

```go
import (
    "net/http"
    "time"
)

func customTransport() {
    transport := &http.Transport{
        DialContext: (&net.Dialer{
            Timeout:   10 * time.Second,
            KeepAlive: 30 * time.Second,
        }).DialContext,
        MaxIdleConns:          100,
        IdleConnTimeout:       90 * time.Second,
        TLSHandshakeTimeout:  10 * time.Second,
    }

    httpClient := &http.Client{
        Transport: transport,
        Timeout:   30 * time.Second,
    }

    cfg := &agentos.Config{
        BaseURL:     "http://localhost:8080",
        APIKey:      "your-key",
        HTTPClient:  httpClient,  // 使用自定义 HTTP 客户端
    }

    client, _ := agentos.NewClient(cfg)
}
```

### 并发请求

```go
func concurrentRequests(client *agentos.Client) {
    ctx := context.Background()
    agent, _ := client.GetAgent(ctx, "agent-id")

    var wg sync.WaitGroup
    results := make(chan *agentos.ChatResponse, 10)

    messages := []string{
        "Question 1",
        "Question 2",
        "Question 3",
    }

    for _, msg := range messages {
        wg.Add(1)
        go func(m string) {
            defer wg.Done()
            resp, err := agent.Chat(ctx, &agentos.ChatRequest{Message: m})
            if err != nil {
                log.Printf("Error: %v", err)
                return
            }
            results <- resp
        }(msg)
    }

    go func() {
        wg.Wait()
        close(results)
    }()

    for resp := range results {
        fmt.Printf("Response: %s\n", resp.Content)
    }
}
```

---

## ⚙️ 配置选项

### 环境变量

```bash
# 必需配置
export AGENTOS_BASE_URL=http://localhost:8080
export AGENTOS_API_KEY=your-api-key-here

# 可选配置
export AGENTOS_TIMEOUT=30s             # 请求超时
export AGENTOS_MAX_RETRIES=3           # 最大重试次数
export AGENTOS_LOG_LEVEL=info          # 日志级别 (debug, info, warn, error)
export AGENTOS_ENABLE_METRICS=true     # Prometheus 指标
```

### 结构化配置文件

```yaml
# config.yaml
agentos:
  base_url: "http://localhost:8080"
  api_key: "${AGENTOS_API_KEY}"  # 支持环境变量引用
  timeout: 30s
  max_retries: 3

logging:
  level: "info"
  format: "json"  # json | text

metrics:
  enabled: true
  endpoint: ":9091"
```

加载配置：

```go
func loadConfig() (*agentos.Config, error) {
    data, err := os.ReadFile("config.yaml")
    if err != nil {
        return nil, err
    }

    var cfg struct {
        AgentOS agentos.Config `yaml:"agentos"`
    }

    if err := yaml.Unmarshal(data, &cfg); err != nil {
        return nil, err
    }

    return &cfg.AgentOS, nil
}
```

---

## 🧪 测试

### 单元测试

```bash
# 运行所有测试
go test ./... -v

# 运行特定包的测试
go test ./pkg/client -v
go test ./pkg/memory -v

# 运行基准测试
go test ./... -bench=. -benchmem
```

### Mock 测试示例

```go
package agentos_test

import (
    "context"
    "testing"

    "github.com/stretchr/testify/assert"
    "github.com/stretchr/testify/require"
    agentos "github.com/spharx/agentos-go-sdk"
)

func TestChatWithMockServer(t *testing.T) {
    // 创建 mock HTTP server
    server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        // 验证请求
        assert.Equal(t, "/api/v1/chat", r.URL.Path)
        assert.Equal(t, "Bearer test-token", r.Header.Get("Authorization"))

        // 返回模拟响应
        w.WriteHeader(http.StatusOK)
        w.Write([]byte(`{
            "jsonrpc": "2.0",
            "result": {
                "choices": [{
                    "message": {"role": "assistant", "content": "Mock response"}
                }]
            },
            "id": "test"
        }`))
    }))
    defer server.Close()

    // 使用 mock server URL 创建客户端
    cfg := &agentos.Config{
        BaseURL: server.URL,
        APIKey:  "test-token",
    }

    client, err := agentos.NewClient(cfg)
    require.NoError(t, err)

    agent, err := client.GetAgent(context.Background(), "test-agent")
    require.NoError(t, err)

    resp, err := agent.Chat(context.Background(), &agentos.ChatRequest{
        Message: "Hello!",
    })
    require.NoError(t, err)
    assert.Equal(t, "Mock response", resp.Content)
}
```

### 基准测试

```go
func BenchmarkChat(b *testing.B) {
    client, _ := agentos.NewClient(&agentos.Config{
        BaseURL: "http://localhost:8080",
        APIKey:  "test-key",
    })
    agent, _ := client.GetAgent(context.Background(), "bench-agent")

    b.ResetTimer()
    for i := 0; i < b.N; i++ {
        _, err := agent.Chat(context.Background(), &agentos.ChatRequest{
            Message: "Benchmark message",
        })
        if err != nil {
            b.Fatal(err)
        }
    }
}
```

---

## 📊 性能优化建议

### 1. 连接复用

SDK 默认使用连接池，无需额外配置。如需调整：

```go
transport := &http.Transport{
    MaxIdleConnsPerHost: 100,
    MaxConnsPerHost:     0,  // 无限制
}
```

### 2. 并发控制

使用 semaphore 或 worker pool 限制并发数：

```go
import "golang.org/x/sync/semaphore"

sem := semaphore.NewWeighted(10) // 最大 10 个并发

for _, req := range requests {
    sem.Acquire(ctx, 1)
    go func(r string) {
        defer sem.Release(1)
        agent.Chat(ctx, &agentos.ChatRequest{Message: r})
    }(req)
}
```

### 3. 响应缓存

对于重复性查询，可添加缓存层：

```go
type CachedClient struct {
    client *agentos.Client
    cache  *lru.Cache
}

func (c *CachedClient) GetAgent(ctx context.Context, id string) (*agentos.Agent, error) {
    if cached, ok := c.cache.Get(id); ok {
        return cached.(*agentos.Agent), nil
    }

    agent, err := c.client.GetAgent(ctx, id)
    if err == nil {
        c.cache.Add(id, agent)
    }
    return agent, err
}
```

---

## 📚 相关文档

- **[内核 API](kernel-api.md)** — Syscall 接口参考
- **[守护进程 API](daemon-api.md)** — Daemon 服务接口
- **[Python SDK](python-sdk.md)** — Python 语言绑定
- **[错误码手册](error-codes.md)** — 错误码定义与处理
- **[Go SDK 规范](../../agentos/manuals/api/toolkit/go/README.md)** — 完整规范

---

## 🔗 快速链接

- **GoDoc**: https://pkg.go.dev/github.com/spharx/agentos-go-sdk
- **GitHub Issues**: https://github.com/SpharxTeam/AgentOS/issues
- **示例代码**: https://github.com/SpharxTeam/agentos-go-examples

---

**© 2026 SPHARX Ltd. All Rights Reserved.**

*"From data intelligence emerges."*
