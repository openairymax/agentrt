package task

import (
	"context"
	"testing"
	"time"

	"github.com/spharx/agentos/toolkit/go/agentos/types"
)

func BenchmarkTaskSubmit(b *testing.B) {
	client := newMockClient()
	tm := NewTaskManager(client)
	ctx := context.Background()

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_, _ = tm.Submit(ctx, "benchmark task")
	}
}

func BenchmarkTaskGet(b *testing.B) {
	client := newMockClient()
	tm := NewTaskManager(client)
	ctx := context.Background()

	_, _ = tm.Submit(ctx, "benchmark task")

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_, _ = tm.Get(ctx, "test-task-id")
	}
}

func BenchmarkTaskQuery(b *testing.B) {
	client := newMockClient()
	tm := NewTaskManager(client)
	ctx := context.Background()

	_, _ = tm.Submit(ctx, "benchmark task")

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_, _ = tm.Query(ctx, "test-task-id")
	}
}

func BenchmarkTaskList(b *testing.B) {
	client := newMockClient()
	tm := NewTaskManager(client)
	ctx := context.Background()

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_, _ = tm.List(ctx, nil)
	}
}

func BenchmarkTaskCancel(b *testing.B) {
	client := newMockClient()
	tm := NewTaskManager(client)
	ctx := context.Background()

	_, _ = tm.Submit(ctx, "benchmark task")

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_ = tm.Cancel(ctx, "test-task-id")
	}
}

func BenchmarkTaskWait(b *testing.B) {
	client := newMockClient()
	tm := NewTaskManager(client)
	ctx := context.Background()

	_, _ = tm.Submit(ctx, "benchmark task")

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_, _ = tm.Wait(ctx, "test-task-id", 5*time.Second)
	}
}

func BenchmarkConcurrentTaskSubmits(b *testing.B) {
	client := newMockClient()
	tm := NewTaskManager(client)
	ctx := context.Background()

	b.ResetTimer()
	b.RunParallel(func(pb *testing.PB) {
		i := 0
		for pb.Next() {
			_, _ = tm.Submit(ctx, "concurrent task")
			i++
		}
	})
}

func newMockClient() MockAPIClient {
	return MockAPIClient{}
}

type MockAPIClient struct{}

func (m MockAPIClient) Get(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
	return &types.APIResponse{
		Success: true,
		Message: "success",
		Data: map[string]interface{}{
			"task_id":     "test-task-id",
			"status":     "pending",
			"description": "test task",
		},
	}, nil
}

func (m MockAPIClient) Post(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
	return &types.APIResponse{
		Success: true,
		Message: "success",
		Data: map[string]interface{}{
			"task_id":     "test-task-id",
			"status":     "pending",
			"description": "test task",
		},
	}, nil
}

func (m MockAPIClient) Delete(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
	return &types.APIResponse{
		Success: true,
		Message: "success",
	}, nil
}

func (m MockAPIClient) Put(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
	return &types.APIResponse{
		Success: true,
		Message: "success",
	}, nil
}