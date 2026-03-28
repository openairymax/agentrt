Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# AgentOS Go SDK

**版本**: Doc V1.5  
**最后更新**: 2026-03-23  
**状态**: 🟢 生产就绪  
**Go 版本**: 1.21+

---

## 🎯 概述

AgentOS Go SDK 提供对 AgentOS 系统调用 API 的惯用 Go 封装。SDK 遵循 Go 语言习惯，支持 context 传递、错误处理和并发模式，同时保持与底层 C API 的完整功能对应。

---

## 📦 安装

```bash
go get github.com/spharx/agentos-go
```

---

## 🚀 快速开始

### 创建 Agent 并执行任务

```go
package main

import (
    "context"
    "fmt"
    "log"
    
    "github.com/spharx/agentos-go"
)

func main() {
    ctx := context.Background()
    
    // 创建 Agent 配置
    manager := &agentos.AgentConfig{
        Name:                "my_agent",
        Description:         "My first AgentOS agent",
        Type:                agentos.AgentTypeChat,
        MaxConcurrentTasks:  8,
        TaskQueueDepth:      64,
    }
    
    // 创建 Agent
    agent, err := agentos.NewAgent(ctx, manager)
    if err != nil {
        log.Fatalf("创建 Agent 失败: %v", err)
    }
    defer agent.Destroy()
    
    // 启动 Agent
    if err := agent.Start(ctx); err != nil {
        log.Fatalf("启动 Agent 失败: %v", err)
    }
    
    // 提交任务
    taskID, err := agent.SubmitTask(ctx, &agentos.TaskRequest{
        Description: "分析用户反馈数据",
        Priority:    agentos.PriorityNormal,
        InputParams: map[string]interface{}{
            "data_source": "feedback.csv",
        },
        TimeoutMs: 30000,
    })
    if err != nil {
        log.Fatalf("提交任务失败: %v", err)
    }
    
    fmt.Printf("任务已提交: %s\n", taskID)
    
    // 等待任务完成
    result, err := agent.WaitTask(ctx, taskID, 35000)
    if err != nil {
        log.Fatalf("等待任务失败: %v", err)
    }
    
    fmt.Printf("任务结果: %s\n", result)
}
```

### 记忆系统操作

```go
package main

import (
    "context"
    "fmt"
    "log"
    
    "github.com/spharx/agentos-go"
)

func main() {
    ctx := context.Background()
    client := agentos.NewMemoryClient()
    
    // 写入记忆
    recordID, err := client.Write(ctx, &agentos.MemoryWriteRequest{
        Content:    "用户对新产品功能表示满意，特别是实时协作功能",
        Metadata:   map[string]interface{}{"type": "user_feedback", "rating": 5},
        Importance: 0.8,
        Tags:       []string{"feedback", "positive", "collaboration"},
    })
    if err != nil {
        log.Fatalf("写入记忆失败: %v", err)
    }
    
    fmt.Printf("记忆写入成功: %s\n", recordID)
    
    // 语义检索
    results, err := client.Query(ctx, &agentos.MemoryQuery{
        Text:              "用户对协作功能的反馈",
        Limit:             10,
        SimilarityThreshold: 0.6,
    })
    if err != nil {
        log.Fatalf("查询记忆失败: %v", err)
    }
    
    for _, result := range results {
        fmt.Printf("[相似度: %.2f] %s\n", result.Similarity, result.Content)
    }
}
```

---

## 📖 API 参考

### Agent 模块

#### `AgentConfig`

```go
// Agent 配置
type AgentConfig struct {
    Name                 string
    Description          string
    Type                 AgentType
    MaxConcurrentTasks   uint32
    TaskQueueDepth       uint32
    DefaultTaskTimeoutMs uint32
    System1MaxDescLen    uint32
    System1MaxPriority   uint8
    CognitionConfig      map[string]interface{}
    MemoryConfig         map[string]interface{}
    SecurityConfig       map[string]interface{}
    ResourceLimits       *ResourceLimits
    RetryConfig          *RetryConfig
}

// Agent 类型
type AgentType int

const (
    AgentTypeGeneric AgentType = iota
    AgentTypeChat
    AgentTypeTask
    AgentTypeAnalysis
    AgentTypeMonitor
)
```

#### `Agent`

```go
// Agent 实例
type Agent struct { ... }

// 创建新 Agent
func NewAgent(ctx context.Context, manager *AgentConfig) (*Agent, error)

// 销毁 Agent
func (a *Agent) Destroy() error

// 获取 Agent ID
func (a *Agent) ID() string

// 获取 Agent 状态
func (a *Agent) State() AgentState

// 启动 Agent
func (a *Agent) Start(ctx context.Context) error

// 暂停 Agent
func (a *Agent) Pause(ctx context.Context) error

// 停止 Agent
func (a *Agent) Stop(ctx context.Context, force bool) error

// 绑定 Skill
func (a *Agent) BindSkill(ctx context.Context, name string, manager map[string]interface{}) error

// 解绑 Skill
func (a *Agent) UnbindSkill(ctx context.Context, name string) error

// 提交任务
func (a *Agent) SubmitTask(ctx context.Context, req *TaskRequest) (string, error)

// 查询任务
func (a *Agent) QueryTask(ctx context.Context, taskID string) (*TaskDescriptor, error)

// 取消任务
func (a *Agent) CancelTask(ctx context.Context, taskID string, force bool) error

// 等待任务完成
func (a *Agent) WaitTask(ctx context.Context, taskID string, timeoutMs uint32) (map[string]interface{}, error)

// 列出任务
func (a *Agent) ListTasks(ctx context.Context, filter *TaskFilter) ([]*TaskDescriptor, error)
```

### Memory 模块

#### `MemoryClient`

```go
// 记忆系统客户端
type MemoryClient struct { ... }

// 创建客户端
func NewMemoryClient() *MemoryClient

// 写入记忆
func (c *MemoryClient) Write(ctx context.Context, req *MemoryWriteRequest) (string, error)

// 语义检索
func (c *MemoryClient) Query(ctx context.Context, query *MemoryQuery) ([]*MemoryResult, error)

// 获取指定记录
func (c *MemoryClient) Get(ctx context.Context, recordID string) (*MemoryRecord, error)

// 主动遗忘
func (c *MemoryClient) Forget(ctx context.Context, policy *ForgetPolicy) (uint64, error)

// 触发记忆进化
func (c *MemoryClient) Evolve(ctx context.Context, manager map[string]interface{}) (*EvolutionStats, error)

// 获取统计信息
func (c *MemoryClient) Stats(ctx context.Context) (*MemoryStatistics, error)
```

#### `MemoryQuery`

```go
// 记忆查询条件
type MemoryQuery struct {
    Text               string
    Vector             []float32
    Tags               []string
    TimeStart          uint64
    TimeEnd            uint64
    ImportanceThreshold float32
    SourceAgent        string
    Layer              uint8
    Limit              uint32
    SimilarityThreshold float32
    SortBy             MemorySortBy
}

// 排序方式
type MemorySortBy int

const (
    MemorySortByTimeDesc MemorySortBy = iota
    MemorySortByTimeAsc
    MemorySortByImportance
    MemorySortBySimilarity
    MemorySortByAccessCount
)
```

### Session 模块

#### `SessionClient`

```go
// 会话管理客户端
type SessionClient struct { ... }

// 创建客户端
func NewSessionClient() *SessionClient

// 创建会话
func (c *SessionClient) Create(ctx context.Context, req *SessionCreateRequest) (string, error)

// 获取会话信息
func (c *SessionClient) Get(ctx context.Context, sessionID string) (*SessionDescriptor, error)

// 关闭会话
func (c *SessionClient) Close(ctx context.Context, sessionID string, archive bool, summary string) error

// 添加消息
func (c *SessionClient) AddMessage(ctx context.Context, sessionID string, req *MessageAddRequest) (string, error)

// 列出消息
func (c *SessionClient) ListMessages(ctx context.Context, sessionID string, offset, limit uint32) ([]*SessionMessage, error)

// 获取会话上下文
func (c *SessionClient) GetContext(ctx context.Context, sessionID string) (*SessionContext, error)

// 设置会话上下文
func (c *SessionClient) SetContext(ctx context.Context, sessionID string, ctx *SessionContext) error
```

### Telemetry 模块

#### `TelemetryClient`

```go
// 可观测性客户端
type TelemetryClient struct { ... }

// 创建客户端
func NewTelemetryClient() *TelemetryClient

// 写入日志
func (c *TelemetryClient) Log(level LogLevel, module, message string, fields map[string]interface{})

// 记录指标
func (c *TelemetryClient) Metric(name string, value float64, ty MetricType, labels map[string]interface{})

// 创建追踪 span
func (c *TelemetryClient) Trace(ctx context.Context, operation string, kind SpanKind, attrs map[string]interface{}) (context.Context, func())

// 获取统计信息
func (c *TelemetryClient) Stats(ctx context.Context) (*TelemetryStatistics, error)
```

---

## 🔄 Context 传递

所有 API 都支持 `context.Context` 用于超时控制和取消：

```go
ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
defer cancel()

taskID, err := agent.SubmitTask(ctx, req)
if err != nil {
    if errors.Is(err, context.DeadlineExceeded) {
        log.Println("请求超时")
    }
}
```

---

## ⚠️ 错误处理

```go
import "github.com/spharx/agentos-go"

taskID, err := agent.SubmitTask(ctx, req)
if err != nil {
    var agentErr *agentos.Error
    if errors.As(err, &agentErr) {
        log.Printf("错误码: 0x%04X", agentErr.Code)
        log.Printf("错误信息: %s", agentErr.Message)
        log.Printf("trace_id: %s", agentErr.TraceID)
        
        switch agentErr.Code {
        case agentos.ErrInvalidArgument:
            log.Println("参数无效")
        case agentos.ErrResourceLimit:
            log.Println("资源限制超出")
        }
    }
}
```

---

## 🧪 测试

```go
package main

import (
    "context"
    "testing"
    
    "github.com/spharx/agentos-go"
    "github.com/spharx/agentos-go/testing"
)

func TestAgentSubmitTask(t *testing.T) {
    mockAgent := testing.NewMockAgent()
    ctx := context.Background()
    
    taskID, err := mockAgent.SubmitTask(ctx, &agentos.TaskRequest{
        Description: "test task",
    })
    
    if err != nil {
        t.Fatalf("提交任务失败: %v", err)
    }
    
    if taskID == "" {
        t.Error("taskID 不应为空")
    }
    
    if !mockAgent.SubmitTaskCalled() {
        t.Error("SubmitTask 未被调用")
    }
}
```

---

## 📚 相关文档

- [系统调用 API](../../syscalls/) - 底层 C API 参考
- [Python SDK](../python/README.md) - Python SDK 文档
- [Rust SDK](../rust/README.md) - Rust SDK 文档
- [开发指南：创建 Agent](../../../guides/folder/create_agent.md) - Agent 开发教程

---

© 2026 SPHARX Ltd. All Rights Reserved.
*"From data intelligence emerges."*