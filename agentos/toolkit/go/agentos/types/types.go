// AgentOS Go SDK - 类型定义模块
// Version: 3.0.0
// Last updated: 2026-03-22
//
// 定义 SDK 中使用的所有枚举类型、领域模型和请求/响应结构。
// 对应 Python SDK: types.py + types/__init__.py

package types

import (
	"fmt"
	"time"
)

// ============================================================
// 枚举类型
// ============================================================

// TaskStatus 任务状态枚举
type TaskStatus string

const (
	TaskStatusPending   TaskStatus = "pending"
	TaskStatusRunning   TaskStatus = "running"
	TaskStatusCompleted TaskStatus = "completed"
	TaskStatusFailed    TaskStatus = "failed"
	TaskStatusCancelled TaskStatus = "cancelled"
)

// String 返回状态的可读字符串
func (s TaskStatus) String() string {
	return string(s)
}

// IsTerminal 判断任务是否处于终态
func (s TaskStatus) IsTerminal() bool {
	return s == TaskStatusCompleted || s == TaskStatusFailed || s == TaskStatusCancelled
}

// MemoryLayer 记忆层级枚举（对应认知深度分层）
type MemoryLayer string

const (
	MemoryLayerL1 MemoryLayer = "L1"
	MemoryLayerL2 MemoryLayer = "L2"
	MemoryLayerL3 MemoryLayer = "L3"
	MemoryLayerL4 MemoryLayer = "L4"
)

// String 返回层级的可读字符串
func (l MemoryLayer) String() string {
	return string(l)
}

// IsValid 判断记忆层级是否合法
func (l MemoryLayer) IsValid() bool {
	switch l {
	case MemoryLayerL1, MemoryLayerL2, MemoryLayerL3, MemoryLayerL4:
		return true
	}
	return false
}

// SessionStatus 会话状态枚举
type SessionStatus string

const (
	SessionStatusActive   SessionStatus = "active"
	SessionStatusInactive SessionStatus = "inactive"
	SessionStatusExpired  SessionStatus = "expired"
)

// String 返回状态的可读字符串
func (s SessionStatus) String() string {
	return string(s)
}

// SkillStatus 技能状态枚举
type SkillStatus string

const (
	SkillStatusActive    SkillStatus = "active"
	SkillStatusInactive  SkillStatus = "inactive"
	SkillStatusDeprecated SkillStatus = "deprecated"
)

// String 返回状态的可读字符串
func (s SkillStatus) String() string {
	return string(s)
}

// SpanStatus 遥测 Span 状态枚举
type SpanStatus string

const (
	SpanStatusOK       SpanStatus = "ok"
	SpanStatusError    SpanStatus = "error"
	SpanStatusUnset    SpanStatus = "unset"
)

// String 返回状态的可读字符串
func (s SpanStatus) String() string {
	return string(s)
}

// ============================================================
// 领域模型
// ============================================================

// Task 表示 AgentOS 系统中的一个执行任务
type Task struct {
	ID          string                 `json:"task_id"`
	Description string                 `json:"description"`
	Status      TaskStatus             `json:"status"`
	Priority    int                    `json:"priority"`
	Output      string                 `json:"output"`
	Error       string                 `json:"error"`
	Metadata    map[string]interface{} `json:"metadata"`
	CreatedAt   time.Time              `json:"created_at"`
	UpdatedAt   time.Time              `json:"updated_at"`
}

// TaskResult 表示已完成任务的结果快照
type TaskResult struct {
	ID        string     `json:"id"`
	Status    TaskStatus `json:"status"`
	Output    string     `json:"output"`
	Error     string     `json:"error"`
	StartTime time.Time  `json:"start_time"`
	EndTime   time.Time  `json:"end_time"`
	Duration  float64    `json:"duration"`
}

// Memory 表示 AgentOS 系统中的一条记忆记录
type Memory struct {
	ID        string                 `json:"memory_id"`
	Content   string                 `json:"content"`
	Layer     MemoryLayer            `json:"layer"`
	Score     float64                `json:"score"`
	Metadata  map[string]interface{} `json:"metadata"`
	CreatedAt time.Time              `json:"created_at"`
	UpdatedAt time.Time              `json:"updated_at"`
}

// MemorySearchResult 表示记忆搜索的聚合结果
type MemorySearchResult struct {
	Memories []Memory `json:"memories"`
	Total    int      `json:"total"`
	Query    string   `json:"query"`
	TopK     int      `json:"top_k"`
}

// Session 表示用户与 Agent 交互的有状态通道
type Session struct {
	ID           string                 `json:"session_id"`
	UserID       string                 `json:"user_id"`
	Status       SessionStatus          `json:"status"`
	Context      map[string]interface{} `json:"context"`
	Metadata     map[string]interface{} `json:"metadata"`
	CreatedAt    time.Time              `json:"created_at"`
	LastActivity time.Time              `json:"last_activity"`
}

// Skill 表示 AgentOS 系统中的可插拔能力单元
type Skill struct {
	ID          string                 `json:"skill_id"`
	Name        string                 `json:"name"`
	Version     string                 `json:"version"`
	Description string                 `json:"description"`
	Status      SkillStatus            `json:"status"`
	Parameters  map[string]interface{} `json:"parameters"`
	Metadata    map[string]interface{} `json:"metadata"`
	CreatedAt   time.Time              `json:"created_at"`
}

// SkillResult 表示技能执行的结果
type SkillResult struct {
	Success bool        `json:"success"`
	Output  interface{} `json:"output"`
	Error   string      `json:"error"`
}

// SkillInfo 表示技能的只读元信息
type SkillInfo struct {
	Name        string                 `json:"name"`
	Description string                 `json:"description"`
	Version     string                 `json:"version"`
	Parameters  map[string]interface{} `json:"parameters"`
}

// ============================================================
// 请求/响应结构
// ============================================================

// RequestOptions 单次请求的可选参数
type RequestOptions struct {
	Timeout     time.Duration
	Headers     map[string]string
	QueryParams map[string]string
}

// RequestOption 请求选项函数签名
type RequestOption func(*RequestOptions)

// WithRequestTimeout 设置单次请求超时
func WithRequestTimeout(timeout time.Duration) RequestOption {
	return func(o *RequestOptions) {
		if timeout > 0 {
			o.Timeout = timeout
		}
	}
}

// WithHeader 添加自定义请求头
func WithHeader(key, value string) RequestOption {
	return func(o *RequestOptions) {
		if o.Headers == nil {
			o.Headers = make(map[string]string)
		}
		o.Headers[key] = value
	}
}

// WithQueryParam 添加查询参数
func WithQueryParam(key, value string) RequestOption {
	return func(o *RequestOptions) {
		if o.QueryParams == nil {
			o.QueryParams = make(map[string]string)
		}
		o.QueryParams[key] = value
	}
}

// APIResponse 通用 API 响应结构
type APIResponse struct {
	Success bool        `json:"success"`
	Data    interface{} `json:"data"`
	Message string      `json:"message"`
}

// HealthStatus 健康检查返回状态
type HealthStatus struct {
	Status    string            `json:"status"`
	Version   string            `json:"version"`
	Uptime    int64             `json:"uptime"`
	Checks    map[string]string `json:"checks"`
	Timestamp time.Time         `json:"timestamp"`
}

// Metrics 系统运行指标快照
type Metrics struct {
	TasksTotal       int64   `json:"tasks_total"`
	TasksCompleted   int64   `json:"tasks_completed"`
	TasksFailed      int64   `json:"tasks_failed"`
	MemoriesTotal    int64   `json:"memories_total"`
	SessionsActive   int64   `json:"sessions_active"`
	SkillsLoaded     int64   `json:"skills_loaded"`
	CPUUsage         float64 `json:"cpu_usage"`
	MemoryUsage      float64 `json:"memory_usage"`
	RequestCount     int64   `json:"request_count"`
	AverageLatencyMs float64 `json:"average_latency_ms"`
}

// ============================================================
// 列表查询选项
// ============================================================

// PaginationOptions 分页选项
type PaginationOptions struct {
	Page     int `json:"page"`
	PageSize int `json:"page_size"`
}

// DefaultPaginationOptions 返回默认分页选项
func DefaultPaginationOptions() *PaginationOptions {
	return &PaginationOptions{Page: 1, PageSize: 20}
}

// BuildQueryParams 将分页参数转换为查询参数 map
func (p *PaginationOptions) BuildQueryParams() map[string]string {
	params := make(map[string]string)
	if p.Page > 0 {
		params["page"] = fmt.Sprintf("%d", p.Page)
	}
	if p.PageSize > 0 {
		params["page_size"] = fmt.Sprintf("%d", p.PageSize)
	}
	return params
}

// SortOptions 排序选项
type SortOptions struct {
	Field string `json:"field"`
	Order string `json:"order"`
}

// FilterOptions 过滤选项
type FilterOptions struct {
	Key   string      `json:"key"`
	Value interface{} `json:"value"`
}

// ListOptions 列表查询的复合选项
type ListOptions struct {
	Pagination *PaginationOptions
	Sort       *SortOptions
	Filter     *FilterOptions
}

// ToQueryParams 将列表选项转换为查询参数 map
func (o *ListOptions) ToQueryParams() map[string]string {
	params := make(map[string]string)
	if o.Pagination != nil {
		for k, v := range o.Pagination.BuildQueryParams() {
			params[k] = v
		}
	}
	if o.Sort != nil {
		if o.Sort.Field != "" {
			params["sort_by"] = o.Sort.Field
		}
		if o.Sort.Order != "" {
			params["sort_order"] = o.Sort.Order
		}
	}
	if o.Filter != nil {
		if o.Filter.Key != "" {
			params["filter_key"] = o.Filter.Key
			params["filter_value"] = fmt.Sprintf("%v", o.Filter.Value)
		}
	}
	return params
}
