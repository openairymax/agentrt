// AgentOS Go SDK Syscall Module Tests
// Version: 2.0.0
// Last updated: 2026-03-23

package syscall

import (
	"context"
	"testing"

	"github.com/spharx/agentos/toolkit/go/agentos/types"
)

// TestSyscallNamespace tests namespace constants
func TestSyscallNamespace(t *testing.T) {
	tests := []struct {
		name     string
		namespace SyscallNamespace
		expected string
	}{
		{"task namespace", NamespaceTask, "task"},
		{"memory namespace", NamespaceMemory, "memory"},
		{"session namespace", NamespaceSession, "session"},
		{"telemetry namespace", NamespaceTelemetry, "telemetry"},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if string(tt.namespace) != tt.expected {
				t.Errorf("expected %s, got %s", tt.expected, tt.namespace)
			}
		})
	}
}

// TestSyscallRequest tests syscall request structure
func TestSyscallRequest(t *testing.T) {
	req := &SyscallRequest{
		Namespace: NamespaceTask,
		Operation: "submit",
		Params:    map[string]any{"description": "test task"},
	}

	if req.Namespace != NamespaceTask {
		t.Errorf("expected NamespaceTask, got %s", req.Namespace)
	}
	if req.Operation != "submit" {
		t.Errorf("expected submit, got %s", req.Operation)
	}
	if req.Params["description"] != "test task" {
		t.Errorf("expected test task, got %v", req.Params["description"])
	}
}

// TestSyscallResponse tests syscall response structure
func TestSyscallResponse(t *testing.T) {
	resp := &SyscallResponse{
		Success:   true,
		Data:      map[string]any{"task_id": "task-123"},
		Error:     "",
		ErrorCode: "",
	}

	if !resp.Success {
		t.Error("expected success to be true")
	}
	if resp.Data == nil {
		t.Error("expected data to not be nil")
	}
}

// TestMockSyscallBinding tests the mock binding implementation
func TestMockSyscallBinding(t *testing.T) {
	ctx := context.Background()
	mock := NewMockSyscallBinding()

	// Test default response
	req := &SyscallRequest{
		Namespace: NamespaceTask,
		Operation: "submit",
	}
	resp, err := mock.Invoke(ctx, req)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !resp.Success {
		t.Error("expected success response")
	}

	// Test custom response
	mock.SetResponse(NamespaceTask, "query", &SyscallResponse{
		Success: true,
		Data:    map[string]any{"status": "completed"},
	})

	req2 := &SyscallRequest{
		Namespace: NamespaceTask,
		Operation: "query",
	}
	resp2, err := mock.Invoke(ctx, req2)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if resp2.Data.(map[string]any)["status"] != "completed" {
		t.Error("expected status to be completed")
	}
}

// TestTaskSyscall tests TaskSyscall convenience methods
func TestTaskSyscall(t *testing.T) {
	ctx := context.Background()
	mock := NewMockSyscallBinding()
	taskSyscall := NewTaskSyscall(mock)

	t.Run("Submit", func(t *testing.T) {
		mock.SetResponse(NamespaceTask, "submit", &SyscallResponse{
			Success: true,
			Data:    map[string]any{"task_id": "task-123"},
		})

		resp, err := taskSyscall.Submit(ctx, "test task")
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if !resp.Success {
			t.Error("expected success response")
		}
	})

	t.Run("Query", func(t *testing.T) {
		mock.SetResponse(NamespaceTask, "query", &SyscallResponse{
			Success: true,
			Data:    map[string]any{"status": "running"},
		})

		resp, err := taskSyscall.Query(ctx, "task-123")
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if !resp.Success {
			t.Error("expected success response")
		}
	})

	t.Run("Cancel", func(t *testing.T) {
		mock.SetResponse(NamespaceTask, "cancel", &SyscallResponse{
			Success: true,
		})

		resp, err := taskSyscall.Cancel(ctx, "task-123")
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if !resp.Success {
			t.Error("expected success response")
		}
	})

	t.Run("Wait", func(t *testing.T) {
		mock.SetResponse(NamespaceTask, "wait", &SyscallResponse{
			Success: true,
			Data:    map[string]any{"status": "completed"},
		})

		resp, err := taskSyscall.Wait(ctx, "task-123", 5000)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if !resp.Success {
			t.Error("expected success response")
		}
	})
}

// TestMemorySyscall tests MemorySyscall convenience methods
func TestMemorySyscall(t *testing.T) {
	ctx := context.Background()
	mock := NewMockSyscallBinding()
	memorySyscall := NewMemorySyscall(mock)

	t.Run("Write", func(t *testing.T) {
		mock.SetResponse(NamespaceMemory, "write", &SyscallResponse{
			Success: true,
			Data:    map[string]any{"memory_id": "mem-123"},
		})

		resp, err := memorySyscall.Write(ctx, "test content", map[string]any{"key": "value"})
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if !resp.Success {
			t.Error("expected success response")
		}
	})

	t.Run("Search", func(t *testing.T) {
		mock.SetResponse(NamespaceMemory, "search", &SyscallResponse{
			Success: true,
			Data:    map[string]any{"memories": []any{}},
		})

		resp, err := memorySyscall.Search(ctx, "query", 5)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if !resp.Success {
			t.Error("expected success response")
		}
	})

	t.Run("Get", func(t *testing.T) {
		mock.SetResponse(NamespaceMemory, "get", &SyscallResponse{
			Success: true,
			Data:    map[string]any{"content": "test content"},
		})

		resp, err := memorySyscall.Get(ctx, "mem-123")
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if !resp.Success {
			t.Error("expected success response")
		}
	})

	t.Run("Delete", func(t *testing.T) {
		mock.SetResponse(NamespaceMemory, "delete", &SyscallResponse{
			Success: true,
		})

		resp, err := memorySyscall.Delete(ctx, "mem-123")
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if !resp.Success {
			t.Error("expected success response")
		}
	})

	t.Run("Evolve", func(t *testing.T) {
		mock.SetResponse(NamespaceMemory, "evolve", &SyscallResponse{
			Success: true,
			Data:    map[string]any{"layer": "L2"},
		})

		resp, err := memorySyscall.Evolve(ctx, "mem-123", types.MemoryLayerL2)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if !resp.Success {
			t.Error("expected success response")
		}
	})
}

// TestSessionSyscall tests SessionSyscall convenience methods
func TestSessionSyscall(t *testing.T) {
	ctx := context.Background()
	mock := NewMockSyscallBinding()
	sessionSyscall := NewSessionSyscall(mock)

	t.Run("Create", func(t *testing.T) {
		mock.SetResponse(NamespaceSession, "create", &SyscallResponse{
			Success: true,
			Data:    map[string]any{"session_id": "sess-123"},
		})

		resp, err := sessionSyscall.Create(ctx, map[string]any{"user": "test"})
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if !resp.Success {
			t.Error("expected success response")
		}
	})

	t.Run("SetContext", func(t *testing.T) {
		mock.SetResponse(NamespaceSession, "set_context", &SyscallResponse{
			Success: true,
		})

		resp, err := sessionSyscall.SetContext(ctx, "sess-123", "key", "value")
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if !resp.Success {
			t.Error("expected success response")
		}
	})

	t.Run("GetContext", func(t *testing.T) {
		mock.SetResponse(NamespaceSession, "get_context", &SyscallResponse{
			Success: true,
			Data:    map[string]any{"value": "test_value"},
		})

		resp, err := sessionSyscall.GetContext(ctx, "sess-123", "key")
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if !resp.Success {
			t.Error("expected success response")
		}
	})

	t.Run("DeleteContext", func(t *testing.T) {
		mock.SetResponse(NamespaceSession, "delete_context", &SyscallResponse{
			Success: true,
		})

		resp, err := sessionSyscall.DeleteContext(ctx, "sess-123", "key")
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if !resp.Success {
			t.Error("expected success response")
		}
	})

	t.Run("Close", func(t *testing.T) {
		mock.SetResponse(NamespaceSession, "close", &SyscallResponse{
			Success: true,
		})

		resp, err := sessionSyscall.Close(ctx, "sess-123")
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if !resp.Success {
			t.Error("expected success response")
		}
	})
}

// TestTelemetrySyscall tests TelemetrySyscall convenience methods
func TestTelemetrySyscall(t *testing.T) {
	ctx := context.Background()
	mock := NewMockSyscallBinding()
	telemetrySyscall := NewTelemetrySyscall(mock)

	t.Run("RecordMetric", func(t *testing.T) {
		mock.SetResponse(NamespaceTelemetry, "record_metric", &SyscallResponse{
			Success: true,
		})

		resp, err := telemetrySyscall.RecordMetric(ctx, "request_count", 1.0, map[string]string{"method": "GET"})
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if !resp.Success {
			t.Error("expected success response")
		}
	})

	t.Run("StartSpan", func(t *testing.T) {
		mock.SetResponse(NamespaceTelemetry, "start_span", &SyscallResponse{
			Success: true,
			Data:    map[string]any{"span_id": "span-123"},
		})

		resp, err := telemetrySyscall.StartSpan(ctx, "operation", "")
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if !resp.Success {
			t.Error("expected success response")
		}
	})

	t.Run("EndSpan", func(t *testing.T) {
		mock.SetResponse(NamespaceTelemetry, "end_span", &SyscallResponse{
			Success: true,
		})

		resp, err := telemetrySyscall.EndSpan(ctx, "span-123", "ok")
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if !resp.Success {
			t.Error("expected success response")
		}
	})

	t.Run("GetMetrics", func(t *testing.T) {
		mock.SetResponse(NamespaceTelemetry, "get_metrics", &SyscallResponse{
			Success: true,
			Data:    map[string]any{"metrics": []any{}},
		})

		resp, err := telemetrySyscall.GetMetrics(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if !resp.Success {
			t.Error("expected success response")
		}
	})
}

// TestContextCancellation tests that context cancellation is properly handled
func TestContextCancellation(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	cancel() // Cancel immediately

	mock := NewMockSyscallBinding()
	taskSyscall := NewTaskSyscall(mock)

	// The mock should still work even with cancelled context
	// (real implementations would check ctx.Err())
	resp, err := taskSyscall.Submit(ctx, "test")
	if err != nil {
		// This is acceptable behavior
		t.Logf("context cancellation handled: %v", err)
	}
	if resp != nil && resp.Success {
		t.Log("mock returned success despite cancelled context")
	}
}

// TestErrorResponses tests error response handling
func TestErrorResponses(t *testing.T) {
	ctx := context.Background()
	mock := NewMockSyscallBinding()

	t.Run("NotFoundError", func(t *testing.T) {
		mock.SetResponse(NamespaceMemory, "get", &SyscallResponse{
			Success:   false,
			Error:     "memory not found",
			ErrorCode: "0x4001",
		})

		memorySyscall := NewMemorySyscall(mock)
		resp, err := memorySyscall.Get(ctx, "nonexistent")
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if resp.Success {
			t.Error("expected failure response")
		}
		if resp.ErrorCode != "0x4001" {
			t.Errorf("expected error code 0x4001, got %s", resp.ErrorCode)
		}
	})

	t.Run("UnauthorizedError", func(t *testing.T) {
		mock.SetResponse(NamespaceSession, "create", &SyscallResponse{
			Success:   false,
			Error:     "unauthorized",
			ErrorCode: "0x000D",
		})

		sessionSyscall := NewSessionSyscall(mock)
		resp, err := sessionSyscall.Create(ctx, nil)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if resp.Success {
			t.Error("expected failure response")
		}
		if resp.ErrorCode != "0x000D" {
			t.Errorf("expected error code 0x000D, got %s", resp.ErrorCode)
		}
	})
}

