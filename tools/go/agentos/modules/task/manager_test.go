// AgentOS Go SDK - 任务管理模块单元测试
// Version: 3.0.0

package task

import (
	"context"
	"errors"
	"testing"
	"time"

	"github.com/spharx/agentos/tools/go/agentos"
	"github.com/spharx/agentos/tools/go/agentos/client"
	"github.com/spharx/agentos/tools/go/agentos/types"
)

func TestNewTaskManager(t *testing.T) {
	tm := NewTaskManager(nil)
	if tm == nil {
		t.Fatal("TaskManager 不应为 nil")
	}
}

func TestTaskManager_Submit_Success(t *testing.T) {
	mock := &client.MockAPIClient{
		PostFn: func(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{Success: true, Data: map[string]interface{}{"task_id": "t1"}}, nil
		},
	}
	tm := NewTaskManager(mock)
	task, err := tm.Submit(context.Background(), "测试任务")
	if err != nil {
		t.Fatalf("Submit error = %v", err)
	}
	if task.ID != "t1" {
		t.Errorf("task.ID = %q", task.ID)
	}
}

func TestTaskManager_Submit_Empty(t *testing.T) {
	tm := NewTaskManager(nil)
	_, err := tm.Submit(context.Background(), "")
	if !agentos.IsErrorCode(err, agentos.CodeMissingParameter) {
		t.Errorf("期望 CodeMissingParameter, got %v", err)
	}
}

func TestTaskManager_Submit_APIError(t *testing.T) {
	mock := &client.MockAPIClient{
		PostFn: func(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
			return nil, errors.New("network error")
		},
	}
	tm := NewTaskManager(mock)
	_, err := tm.Submit(context.Background(), "test")
	if err == nil {
		t.Error("API 错误应传播")
	}
}

func TestTaskManager_SubmitWithOptions(t *testing.T) {
	mock := &client.MockAPIClient{
		PostFn: func(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{Success: true, Data: map[string]interface{}{"task_id": "t2"}}, nil
		},
	}
	tm := NewTaskManager(mock)
	task, err := tm.SubmitWithOptions(context.Background(), "带选项任务", 8, map[string]interface{}{"env": "test"})
	if err != nil {
		t.Fatalf("SubmitWithOptions error = %v", err)
	}
	if task.Priority != 8 {
		t.Errorf("priority = %d, want 8", task.Priority)
	}
}

func TestTaskManager_Get(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data:    map[string]interface{}{"task_id": "t1", "description": "test", "status": "running"},
			}, nil
		},
	}
	tm := NewTaskManager(mock)
	task, err := tm.Get(context.Background(), "t1")
	if err != nil {
		t.Fatalf("Get error = %v", err)
	}
	if task.ID != "t1" {
		t.Errorf("task.ID = %q", task.ID)
	}
}

func TestTaskManager_Get_EmptyID(t *testing.T) {
	tm := NewTaskManager(nil)
	_, err := tm.Get(context.Background(), "")
	if err == nil {
		t.Error("空 ID 应返回错误")
	}
}

func TestTaskManager_Query(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data:    map[string]interface{}{"task_id": "t1", "status": "completed"},
			}, nil
		},
	}
	tm := NewTaskManager(mock)
	status, err := tm.Query(context.Background(), "t1")
	if err != nil {
		t.Fatalf("Query error = %v", err)
	}
	if status != types.TaskStatusCompleted {
		t.Errorf("status = %q", status)
	}
}

func TestTaskManager_Cancel(t *testing.T) {
	mock := &client.MockAPIClient{
		PostFn: func(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{Success: true, Data: nil}, nil
		},
	}
	tm := NewTaskManager(mock)
	err := tm.Cancel(context.Background(), "t1")
	if err != nil {
		t.Fatalf("Cancel error = %v", err)
	}
}

func TestTaskManager_Cancel_EmptyID(t *testing.T) {
	tm := NewTaskManager(nil)
	err := tm.Cancel(context.Background(), "")
	if err == nil {
		t.Error("空 ID 应返回错误")
	}
}

func TestTaskManager_List(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data: map[string]interface{}{
					"tasks": []interface{}{
						map[string]interface{}{"task_id": "t1", "status": "completed"},
					},
					"total": float64(1),
				},
			}, nil
		},
	}
	tm := NewTaskManager(mock)
	tasks, err := tm.List(context.Background(), nil)
	if err != nil {
		t.Fatalf("List error = %v", err)
	}
	if len(tasks) != 1 {
		t.Errorf("len(tasks) = %d", len(tasks))
	}
}

func TestTaskManager_Delete(t *testing.T) {
	mock := &client.MockAPIClient{
		DelFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{Success: true, Data: nil}, nil
		},
	}
	tm := NewTaskManager(mock)
	err := tm.Delete(context.Background(), "t1")
	if err != nil {
		t.Fatalf("Delete error = %v", err)
	}
}

func TestTaskManager_Delete_EmptyID(t *testing.T) {
	tm := NewTaskManager(nil)
	err := tm.Delete(context.Background(), "")
	if err == nil {
		t.Error("空 ID 应返回错误")
	}
}

func TestTaskManager_GetResult_NotTerminal(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data:    map[string]interface{}{"task_id": "t1", "status": "running"},
			}, nil
		},
	}
	tm := NewTaskManager(mock)
	_, err := tm.GetResult(context.Background(), "t1")
	if err == nil {
		t.Error("非终态应返回错误")
	}
}

func TestTaskManager_GetResult_Completed(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data:    map[string]interface{}{"task_id": "t1", "status": "completed", "output": "result"},
			}, nil
		},
	}
	tm := NewTaskManager(mock)
	result, err := tm.GetResult(context.Background(), "t1")
	if err != nil {
		t.Fatalf("GetResult error = %v", err)
	}
	if result.Output != "result" {
		t.Errorf("output = %q", result.Output)
	}
}

func TestTaskManager_BatchSubmit(t *testing.T) {
	mock := &client.MockAPIClient{
		PostFn: func(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{Success: true, Data: map[string]interface{}{"task_id": "t1"}}, nil
		},
	}
	tm := NewTaskManager(mock)
	tasks, err := tm.BatchSubmit(context.Background(), []string{"a", "b"})
	if err != nil {
		t.Fatalf("BatchSubmit error = %v", err)
	}
	if len(tasks) != 2 {
		t.Errorf("len(tasks) = %d", len(tasks))
	}
}

func TestTaskManager_BatchSubmit_PartialFailure(t *testing.T) {
	callCount := 0
	mock := &client.MockAPIClient{
		PostFn: func(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
			callCount++
			if callCount > 1 {
				return nil, errors.New("error")
			}
			return &types.APIResponse{Success: true, Data: map[string]interface{}{"task_id": "t1"}}, nil
		},
	}
	tm := NewTaskManager(mock)
	tasks, err := tm.BatchSubmit(context.Background(), []string{"a", "b"})
	if err == nil {
		t.Error("部分失败应返回错误")
	}
	if len(tasks) != 1 {
		t.Errorf("部分失败应返回已成功的, got %d", len(tasks))
	}
}

func TestTaskManager_Wait_Completed(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data:    map[string]interface{}{"task_id": "t1", "status": "completed", "output": "done"},
			}, nil
		},
	}
	tm := NewTaskManager(mock)
	result, err := tm.Wait(context.Background(), "t1", 0)
	if err != nil {
		t.Fatalf("Wait error = %v", err)
	}
	if result.Output != "done" {
		t.Errorf("output = %q", result.Output)
	}
}

func TestTaskManager_Wait_Timeout(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data:    map[string]interface{}{"task_id": "t1", "status": "running"},
			}, nil
		},
	}
	tm := NewTaskManager(mock)
	_, err := tm.Wait(context.Background(), "t1", 200*time.Millisecond)
	if err == nil {
		t.Error("超时应返回错误")
	}
	if !agentos.IsErrorCode(err, agentos.CodeTaskTimeout) {
		t.Errorf("期望 CodeTaskTimeout, got %v", err)
	}
}

func TestTaskManager_Wait_ContextCancel(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data:    map[string]interface{}{"task_id": "t1", "status": "running"},
			}, nil
		},
	}
	tm := NewTaskManager(mock)
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	_, err := tm.Wait(ctx, "t1", 0)
	if err == nil {
		t.Error("上下文取消应返回错误")
	}
}

func TestTaskManager_WaitForAll_Success(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data:    map[string]interface{}{"task_id": "t1", "status": "completed", "output": "ok"},
			}, nil
		},
	}
	tm := NewTaskManager(mock)
	results, err := tm.WaitForAll(context.Background(), []string{"t1", "t2"}, 0)
	if err != nil {
		t.Fatalf("WaitForAll error = %v", err)
	}
	if len(results) != 2 {
		t.Errorf("len(results) = %d", len(results))
	}
}

func TestParseTaskFromMap(t *testing.T) {
	data := map[string]interface{}{
		"task_id": "t1", "description": "test", "status": "completed",
		"priority": float64(5), "output": "done", "error": "",
	}
	task := parseTaskFromMap(data)
	if task.ID != "t1" || task.Status != types.TaskStatusCompleted || task.Priority != 5 {
		t.Errorf("parseTaskFromMap = %+v", task)
	}
}

func TestParseTaskList_InvalidData(t *testing.T) {
	_, err := parseTaskList(&types.APIResponse{Success: true, Data: "not a map"})
	if err == nil {
		t.Error("无效数据应返回错误")
	}
}

func TestParseTaskList_EmptyTasks(t *testing.T) {
	tasks, err := parseTaskList(&types.APIResponse{
		Success: true,
		Data:    map[string]interface{}{"tasks": []interface{}{}},
	})
	if err != nil {
		t.Fatalf("error = %v", err)
	}
	if len(tasks) != 0 {
		t.Errorf("len(tasks) = %d", len(tasks))
	}
}
