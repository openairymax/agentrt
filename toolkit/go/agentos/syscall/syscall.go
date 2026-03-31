// AgentOS Go SDK Syscall Module
// Version: 2.0.0
// Last updated: 2026-03-23
//
// This module provides system call bindings for AgentOS, following the design
// from syscall.md specification.
//
// The SyscallBinding interface can be implemented to:
//   - HTTP-based binding (uses APIClient)
//   - FFI-based binding (uses CGO)
//   - Mock binding (for testing)
//
// The Convenience classes (TaskSyscall, MemorySyscall, SessionSyscall, TelemetrySyscall)
// provide higher-level APIs for commons operations.

//
// Design Philosophy:
// - Interface-based: Allows different transport mechanisms
// - Namespace-aware: Maps to syscall.md specification
// - Error-transparent: Preserves error codes from responses
// - Context-aware: Supports cancellation and timeouts

package syscall

import (
	"context"
	"encoding/json"
	"fmt"
	"sync"

	"github.com/spharx/agentos/toolkit/go/agentos/client"
	"github.com/spharx/agentos/toolkit/go/agentos/types"
)

// SyscallNamespace defines the namespace for system calls
type SyscallNamespace string

const (
	NamespaceTask      SyscallNamespace = "task"
	NamespaceMemory    SyscallNamespace = "memory"
	NamespaceSession   SyscallNamespace = "session"
	NamespaceTelemetry SyscallNamespace = "telemetry"
)

// SyscallRequest represents a system call request
type SyscallRequest struct {
	Namespace SyscallNamespace `json:"namespace"`
	Operation string                 `json:"operation"`
	Params    map[string]any    `json:"params,omitempty"`
}

// SyscallResponse represents a system call response
type SyscallResponse struct {
	Success   bool           `json:"success"`
	Data      any            `json:"data,omitempty"`
	Error     string        `json:"error,omitempty"`
	ErrorCode string        `json:"error_code,omitempty"`
}

// SyscallBinding defines the interface for system call execution
type SyscallBinding interface {
	// Invoke executes a system call
	Invoke(ctx context.Context, request *SyscallRequest) (*SyscallResponse, error)
}

// HTTPSyscallBinding implements SyscallBinding using HTTP API client
type HTTPSyscallBinding struct {
	apiClient client.APIClient
}

// NewHTTPSyscallBinding creates a new HTTP-based syscall binding
func NewHTTPSyscallBinding(apiClient client.APIClient) *HTTPSyscallBinding {
	return &HTTPSyscallBinding{apiClient: apiClient}
}

// Invoke executes a system call via HTTP API
func (b *HTTPSyscallBinding) Invoke(ctx context.Context, request *SyscallRequest) (*SyscallResponse, error) {
	// Build the path based on namespace and operation
	path := fmt.Sprintf("/api/v1/syscall/%s/%s", request.Namespace, request.Operation)

	// Prepare request body
	body := map[string]any{}
	if request.Params != nil {
		body = request.Params
	}

	// Execute HTTP request
	resp, err := b.apiClient.Post(ctx, path, body)
	if err != nil {
		return nil, err
	}

	// Parse response data into SyscallResponse
	var result SyscallResponse
	if resp.Data != nil {
		if dataBytes, jsonErr := json.Marshal(resp.Data); jsonErr == nil {
			if parseErr := json.Unmarshal(dataBytes, &result); parseErr != nil {
				return nil, fmt.Errorf("failed to parse syscall response: %w", parseErr)
			}
		}
	}

	return &result, nil
}

// TaskSyscall provides convenient methods for task-related system calls
type TaskSyscall struct {
	binding SyscallBinding
}

// NewTaskSyscall creates a new TaskSyscall
func NewTaskSyscall(binding SyscallBinding) *TaskSyscall {
	return &TaskSyscall{binding: binding}
}

// Submit submits a new task
func (t *TaskSyscall) Submit(ctx context.Context, description string) (*SyscallResponse, error) {
	return t.binding.Invoke(ctx, &SyscallRequest{
		Namespace: NamespaceTask,
		Operation: "submit",
		Params: map[string]any{"description": description},
	})
}

// Query queries task status
func (t *TaskSyscall) Query(ctx context.Context, taskID string) (*SyscallResponse, error) {
	return t.binding.Invoke(ctx, &SyscallRequest{
		Namespace: NamespaceTask,
		Operation: "query",
		Params: map[string]any{"task_id": taskID},
	})
}

// Cancel cancels a task
func (t *TaskSyscall) Cancel(ctx context.Context, taskID string) (*SyscallResponse, error) {
	return t.binding.Invoke(ctx, &SyscallRequest{
		Namespace: NamespaceTask,
		Operation: "cancel",
		Params: map[string]any{"task_id": taskID},
	 })
}

// Wait waits for task completion
func (t *TaskSyscall) Wait(ctx context.Context, taskID string, timeoutMs int) (*SyscallResponse, error) {
	return t.binding.Invoke(ctx, &SyscallRequest{
		Namespace: NamespaceTask,
		Operation: "wait",
		Params: map[string]any{
			"task_id":    taskID,
			"timeout_ms": timeoutMs,
		},
	 })
}

// MemorySyscall provides convenient methods for memory-related system calls
type MemorySyscall struct {
	binding SyscallBinding
}

// NewMemorySyscall creates a new MemorySyscall
func NewMemorySyscall(binding SyscallBinding) *MemorySyscall {
	return &MemorySyscall{binding: binding}
}

// Write writes data to memory
func (m *MemorySyscall) Write(ctx context.Context, content string, metadata map[string]any) (*SyscallResponse, error) {
	return m.binding.Invoke(ctx, &SyscallRequest{
		Namespace: NamespaceMemory,
		Operation: "write",
		Params: map[string]any{
			"content":  content,
			"metadata": metadata,
		},
    })
}

// Search searches memory
func (m *MemorySyscall) Search(ctx context.Context, query string, topK int) (*SyscallResponse, error) {
	return m.binding.Invoke(ctx, &SyscallRequest{
		Namespace: NamespaceMemory,
		Operation: "search",
		Params: map[string]any{
			"query": query,
			"top_k": topK,
		},
    })
}

// Get retrieves memory by ID
func (m *MemorySyscall) Get(ctx context.Context, memoryID string) (*SyscallResponse, error) {
	return m.binding.Invoke(ctx, &SyscallRequest{
		Namespace: NamespaceMemory,
		Operation: "get",
		Params: map[string]any{"memory_id": memoryID},
    })
}

// Delete deletes memory by ID
func (m *MemorySyscall) Delete(ctx context.Context, memoryID string) (*SyscallResponse, error) {
	return m.binding.Invoke(ctx, &SyscallRequest{
		Namespace: NamespaceMemory,
		Operation: "delete",
		Params: map[string]any{"memory_id": memoryID},
    })
}

// Evolve evolves memory to a higher layer
func (m *MemorySyscall) Evolve(ctx context.Context, memoryID string, targetLayer types.MemoryLayer) (*SyscallResponse, error) {
	return m.binding.Invoke(ctx, &SyscallRequest{
		Namespace: NamespaceMemory,
		Operation: "evolve",
		Params: map[string]any{
			"memory_id":  memoryID,
			"target_layer": string(targetLayer),
		},
    })
}

// SessionSyscall provides convenient methods for session-related system calls
type SessionSyscall struct {
	binding SyscallBinding
}

// NewSessionSyscall creates a new SessionSyscall
func NewSessionSyscall(binding SyscallBinding) *SessionSyscall {
	return &SessionSyscall{binding: binding}
}

// Create creates a new session
func (s *SessionSyscall) Create(ctx context.Context, metadata map[string]any) (*SyscallResponse, error) {
	params := map[string]any{}
	if metadata != nil {
		params["metadata"] = metadata
	}
	return s.binding.Invoke(ctx, &SyscallRequest{
		Namespace: NamespaceSession,
		Operation: "create",
		Params:    params,
	})
}

// SetContext sets a context value
func (s *SessionSyscall) SetContext(ctx context.Context, sessionID, key string, value any) (*SyscallResponse, error) {
	return s.binding.Invoke(ctx, &SyscallRequest{
		Namespace: NamespaceSession,
		Operation: "set_context",
		Params: map[string]any{
			"session_id": sessionID,
			"key":       key,
			"value":    value,
		},
    })
}

// GetContext gets a context value
func (s *SessionSyscall) GetContext(ctx context.Context, sessionID, key string) (*SyscallResponse, error) {
	return s.binding.Invoke(ctx, &SyscallRequest{
		Namespace: NamespaceSession,
		Operation: "get_context",
		Params: map[string]any{
			"session_id": sessionID,
			"key":       key,
		},
    })
}

// DeleteContext deletes a context value
func (s *SessionSyscall) DeleteContext(ctx context.Context, sessionID, key string) (*SyscallResponse, error) {
	return s.binding.Invoke(ctx, &SyscallRequest{
		Namespace: NamespaceSession,
		Operation: "delete_context",
		Params: map[string]any{
			"session_id": sessionID,
			"key":       key,
		},
    })
}

// Close closes a session
func (s *SessionSyscall) Close(ctx context.Context, sessionID string) (*SyscallResponse, error) {
	return s.binding.Invoke(ctx, &SyscallRequest{
		Namespace: NamespaceSession,
		Operation: "close",
		Params: map[string]any{"session_id": sessionID},
    })
}

// TelemetrySyscall provides convenient methods for telemetry-related system calls
type TelemetrySyscall struct {
	binding SyscallBinding
}

// NewTelemetrySyscall creates a new TelemetrySyscall
func NewTelemetrySyscall(binding SyscallBinding) *TelemetrySyscall {
	return &TelemetrySyscall{binding: binding}
}

// RecordMetric records a metric
func (t *TelemetrySyscall) RecordMetric(ctx context.Context, name string, value float64, tags map[string]string) (*SyscallResponse, error) {
	return t.binding.Invoke(ctx, &SyscallRequest{
		Namespace: NamespaceTelemetry,
		Operation: "record_metric",
		Params: map[string]any{
			"name":  name,
			"value": value,
			"tags":  tags,
		},
    })
}

// StartSpan starts a new span
func (t *TelemetrySyscall) StartSpan(ctx context.Context, name string, parentSpanID string) (*SyscallResponse, error) {
	return t.binding.Invoke(ctx, &SyscallRequest{
		Namespace: NamespaceTelemetry,
		Operation: "start_span",
		Params: map[string]any{
			"name":            name,
			"parent_span_id": parentSpanID,
		},
    })
}

// EndSpan ends a span
func (t *TelemetrySyscall) EndSpan(ctx context.Context, spanID string, status string) (*SyscallResponse, error) {
	return t.binding.Invoke(ctx, &SyscallRequest{
		Namespace: NamespaceTelemetry,
		Operation: "end_span",
		Params: map[string]any{
			"span_id": spanID,
			"status": status,
		},
    })
}

// GetMetrics retrieves current metrics
func (t *TelemetrySyscall) GetMetrics(ctx context.Context) (*SyscallResponse, error) {
	return t.binding.Invoke(ctx, &SyscallRequest{
		Namespace: NamespaceTelemetry,
		Operation: "get_metrics",
	})
}

// MockSyscallBinding implements SyscallBinding for testing
type MockSyscallBinding struct {
	mu        sync.RWMutex
	responses map[string]*SyscallResponse
	callCount map[string]int
}

// NewMockSyscallBinding creates a new mock binding
func NewMockSyscallBinding() *MockSyscallBinding {
	return &MockSyscallBinding{
		responses: make(map[string]*SyscallResponse),
		callCount: make(map[string]int),
	}
}

// SetResponse sets a mock response for a specific syscall
func (m *MockSyscallBinding) SetResponse(namespace SyscallNamespace, operation string, response *SyscallResponse) {
	m.mu.Lock()
	defer m.mu.Unlock()
	key := fmt.Sprintf("%s.%s", namespace, operation)
	m.responses[key] = response
}

// GetCallCount returns the number of times a syscall was invoked
func (m *MockSyscallBinding) GetCallCount(namespace SyscallNamespace, operation string) int {
	m.mu.RLock()
	defer m.mu.RUnlock()
	key := fmt.Sprintf("%s.%s", namespace, operation)
	return m.callCount[key]
}

// Invoke executes a mock system call
func (m *MockSyscallBinding) Invoke(ctx context.Context, request *SyscallRequest) (*SyscallResponse, error) {
	m.mu.Lock()
	defer m.mu.Unlock()

	key := fmt.Sprintf("%s.%s", request.Namespace, request.Operation)
	m.callCount[key]++

	if resp, ok := m.responses[key]; ok {
		return resp, nil
	}
	// Default success response
	return &SyscallResponse{
		Success: true,
		Data:    map[string]any{},
	}, nil
}

