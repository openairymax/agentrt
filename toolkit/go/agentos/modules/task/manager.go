// AgentOS Go SDK - 任务管理模块
// Version: 3.0.0
// Last updated: 2026-03-22
//
// 提供任务的提交、查询、等待、取消、列表等生命周期管理功能。
// 对应 Python SDK: modules/task/__init__.py + task.py

package task

import (
	"context"
	"fmt"
	"sync"
	"time"

	"github.com/spharx/agentos/toolkit/go/agentos"
	"github.com/spharx/agentos/toolkit/go/agentos/client"
	"github.com/spharx/agentos/toolkit/go/agentos/types"
	"github.com/spharx/agentos/toolkit/go/agentos/utils"
)

// TaskManager 管理任务完整生命周期
type TaskManager struct {
	api client.APIClient
}

// NewTaskManager 创建新的任务管理器实例
func NewTaskManager(api client.APIClient) *TaskManager {
	return &TaskManager{api: api}
}

// Submit 提交新的执行任务
func (tm *TaskManager) Submit(ctx context.Context, description string) (*types.Task, error) {
	if description == "" {
		return nil, agentos.NewError(agentos.CodeMissingParameter, "任务描述不能为空", nil)
	}

	resp, err := tm.api.Post(ctx, "/api/v1/tasks", map[string]interface{}{
		"description": description,
	})
	if err != nil {
		return nil, err
	}

	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return nil, agentos.NewError(agentos.CodeInvalidResponse, "任务创建响应格式异常", nil)
	}

	return &types.Task{
		ID:          utils.GetString(data, "task_id"),
		Description: description,
		Status:      types.TaskStatusPending,
		CreatedAt:   time.Now(),
		UpdatedAt:   time.Now(),
	}, nil
}

// SubmitWithOptions 使用扩展选项提交任务
func (tm *TaskManager) SubmitWithOptions(ctx context.Context, description string, priority int, metadata map[string]interface{}) (*types.Task, error) {
	if description == "" {
		return nil, agentos.NewError(agentos.CodeMissingParameter, "任务描述不能为空", nil)
	}

	body := map[string]interface{}{
		"description": description,
		"priority":    priority,
	}
	if metadata != nil {
		body["metadata"] = metadata
	}

	resp, err := tm.api.Post(ctx, "/api/v1/tasks", body)
	if err != nil {
		return nil, err
	}

	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return nil, agentos.NewError(agentos.CodeInvalidResponse, "任务创建响应格式异常", nil)
	}

	return &types.Task{
		ID:          utils.GetString(data, "task_id"),
		Description: description,
		Priority:    priority,
		Status:      types.TaskStatusPending,
		Metadata:    metadata,
		CreatedAt:   time.Now(),
		UpdatedAt:   time.Now(),
	}, nil
}

// Get 获取指定任务的详细信息
func (tm *TaskManager) Get(ctx context.Context, taskID string) (*types.Task, error) {
	if taskID == "" {
		return nil, agentos.NewError(agentos.CodeMissingParameter, "任务ID不能为空", nil)
	}

	resp, err := tm.api.Get(ctx, fmt.Sprintf("/api/v1/tasks/%s", taskID))
	if err != nil {
		return nil, err
	}

	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return nil, agentos.NewError(agentos.CodeInvalidResponse, "任务详情响应格式异常", nil)
	}

	return parseTaskFromMap(data), nil
}

// Query 查询任务的当前状态
func (tm *TaskManager) Query(ctx context.Context, taskID string) (types.TaskStatus, error) {
	task, err := tm.Get(ctx, taskID)
	if err != nil {
		return "", err
	}
	return task.Status, nil
}

// Wait 阻塞等待任务到达终态，支持超时控制和上下文取消
func (tm *TaskManager) Wait(ctx context.Context, taskID string, timeout time.Duration) (*types.TaskResult, error) {
	start := time.Now()

	for {
		status, err := tm.Query(ctx, taskID)
		if err != nil {
			return nil, err
		}

		if status.IsTerminal() {
			task, err := tm.Get(ctx, taskID)
			if err != nil {
				return nil, err
			}
			return &types.TaskResult{
				ID:        task.ID,
				Status:    task.Status,
				Output:    task.Output,
				Error:     task.Error,
				StartTime: start,
				EndTime:   time.Now(),
				Duration:  time.Since(start).Seconds(),
			}, nil
		}

		if timeout > 0 && time.Since(start) > timeout {
			return nil, agentos.NewErrorf(agentos.CodeTaskTimeout, "任务 %s 超时", taskID)
		}

		select {
		case <-ctx.Done():
			return nil, ctx.Err()
		case <-time.After(500 * time.Millisecond):
		}
	}
}

// Cancel 取消正在执行的任务
func (tm *TaskManager) Cancel(ctx context.Context, taskID string) error {
	if taskID == "" {
		return agentos.NewError(agentos.CodeMissingParameter, "任务ID不能为空", nil)
	}
	_, err := tm.api.Post(ctx, fmt.Sprintf("/api/v1/tasks/%s/cancel", taskID), nil)
	return err
}

// List 列出任务，支持分页和过滤
func (tm *TaskManager) List(ctx context.Context, opts *types.ListOptions) ([]types.Task, error) {
	path := "/api/v1/tasks"
	if opts != nil {
		path = utils.BuildURL(path, opts.ToQueryParams())
	}

	resp, err := tm.api.Get(ctx, path)
	if err != nil {
		return nil, err
	}

	return parseTaskList(resp)
}

// Delete 删除指定任务
func (tm *TaskManager) Delete(ctx context.Context, taskID string) error {
	if taskID == "" {
		return agentos.NewError(agentos.CodeMissingParameter, "任务ID不能为空", nil)
	}
	_, err := tm.api.Delete(ctx, fmt.Sprintf("/api/v1/tasks/%s", taskID))
	return err
}

// GetResult 获取已完成任务的结果
func (tm *TaskManager) GetResult(ctx context.Context, taskID string) (*types.TaskResult, error) {
	task, err := tm.Get(ctx, taskID)
	if err != nil {
		return nil, err
	}
	if !task.Status.IsTerminal() {
		return nil, agentos.NewError(agentos.CodeInvalidParameter, "任务尚未完成", nil)
	}
	return &types.TaskResult{
		ID:     task.ID,
		Status: task.Status,
		Output: task.Output,
		Error:  task.Error,
	}, nil
}

// BatchSubmit 批量提交多个任务
func (tm *TaskManager) BatchSubmit(ctx context.Context, descriptions []string) ([]types.Task, error) {
	tasks := make([]types.Task, 0, len(descriptions))
	for _, desc := range descriptions {
		task, err := tm.Submit(ctx, desc)
		if err != nil {
			return tasks, err
		}
		tasks = append(tasks, *task)
	}
	return tasks, nil
}

// Count 获取任务总数
func (tm *TaskManager) Count(ctx context.Context) (int64, error) {
	resp, err := tm.api.Get(ctx, "/api/v1/tasks/count")
	if err != nil {
		return 0, err
	}
	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return 0, agentos.NewError(agentos.CodeInvalidResponse, "任务计数响应格式异常", nil)
	}
	return utils.GetInt64(data, "count"), nil
}

// WaitForAny 并发等待任一任务完成，返回最先到达终态的结果
func (tm *TaskManager) WaitForAny(ctx context.Context, taskIDs []string, timeout time.Duration) (*types.TaskResult, error) {
	if len(taskIDs) == 0 {
		return nil, agentos.NewError(agentos.CodeMissingParameter, "任务ID列表不能为空", nil)
	}

	ctx, cancel := context.WithTimeout(ctx, timeout)
	defer cancel()

	resultCh := make(chan *types.TaskResult, len(taskIDs))
	errCh := make(chan error, len(taskIDs))

	var wg sync.WaitGroup
	for _, id := range taskIDs {
		wg.Add(1)
		go func(taskID string) {
			defer wg.Done()
			result, err := tm.Wait(ctx, taskID, timeout)
			if err != nil {
				errCh <- err
				return
			}
			resultCh <- result
		}(id)
	}

	done := make(chan struct{})
	go func() {
		wg.Wait()
		close(done)
	}()

	select {
	case result := <-resultCh:
		return result, nil
	case err := <-errCh:
		return nil, err
	case <-ctx.Done():
		return nil, agentos.NewError(agentos.CodeTaskTimeout, "等待任务超时", ctx.Err())
	case <-done:
		return nil, agentos.NewError(agentos.CodeTaskFailed, "所有任务已完成但无结果", nil)
	}
}

// WaitForAll 并发等待多个任务全部完成
func (tm *TaskManager) WaitForAll(ctx context.Context, taskIDs []string, timeout time.Duration) ([]types.TaskResult, error) {
	if len(taskIDs) == 0 {
		return nil, nil
	}

	ctx, cancel := context.WithTimeout(ctx, timeout)
	defer cancel()

	type indexedResult struct {
		index  int
		result *types.TaskResult
		err    error
	}

	resultCh := make(chan indexedResult, len(taskIDs))
	var wg sync.WaitGroup

	for i, id := range taskIDs {
		wg.Add(1)
		go func(idx int, taskID string) {
			defer wg.Done()
			result, err := tm.Wait(ctx, taskID, timeout)
			resultCh <- indexedResult{index: idx, result: result, err: err}
		}(i, id)
	}

	go func() {
		wg.Wait()
		close(resultCh)
	}()

	results := make([]types.TaskResult, len(taskIDs))
	var firstErr error
	for ir := range resultCh {
		if ir.err != nil && firstErr == nil {
			firstErr = ir.err
		}
		if ir.result != nil {
			results[ir.index] = *ir.result
		}
	}

	if firstErr != nil {
		return results, firstErr
	}
	return results, nil
}

// parseTaskFromMap 从 map 解析 Task 结构
func parseTaskFromMap(data map[string]interface{}) *types.Task {
	return &types.Task{
		ID:          utils.GetString(data, "task_id"),
		Description: utils.GetString(data, "description"),
		Status:      types.TaskStatus(utils.GetString(data, "status")),
		Priority:    int(utils.GetInt64(data, "priority")),
		Output:      utils.GetString(data, "output"),
		Error:       utils.GetString(data, "error"),
		Metadata:    utils.GetMap(data, "metadata"),
		CreatedAt:   utils.ParseTimeFromMap(data, "created_at"),
		UpdatedAt:   utils.ParseTimeFromMap(data, "updated_at"),
	}
}

// parseTaskList 从 APIResponse 解析 Task 列表
func parseTaskList(resp *types.APIResponse) ([]types.Task, error) {
	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return nil, agentos.NewError(agentos.CodeInvalidResponse, "任务列表响应格式异常", nil)
	}

	items := utils.GetInterfaceSlice(data, "tasks")
	tasks := make([]types.Task, 0, len(items))
	for _, item := range items {
		if m, ok := item.(map[string]interface{}); ok {
			tasks = append(tasks, *parseTaskFromMap(m))
		}
	}
	return tasks, nil
}

