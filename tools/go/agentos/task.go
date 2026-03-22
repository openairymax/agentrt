// AgentOS Go SDK Task
// Version: 1.0.0.5
// Last updated: 2026-03-21

package agentos

import (
	"context"
	"errors"
	"fmt"
	"time"
)

// TaskStatus represents the status of a task
type TaskStatus string

const (
	TaskStatusPending   TaskStatus = "pending"
	TaskStatusRunning   TaskStatus = "running"
	TaskStatusCompleted TaskStatus = "completed"
	TaskStatusFailed    TaskStatus = "failed"
	TaskStatusCancelled TaskStatus = "cancelled"
)

// Task represents a task in the AgentOS system
type Task struct {
	client *Client
	ID     string
}

// NewTask creates a new Task object
func NewTask(client *Client, taskID string) *Task {
	return &Task{
		client: client,
		ID:     taskID,
	}
}

// Query queries the task status
func (t *Task) Query(ctx context.Context) (TaskStatus, error) {
	path := fmt.Sprintf("/api/tasks/%s", t.ID)
	
	response, err := t.client.request(ctx, "GET", path, nil)
	if err != nil {
		return "", err
	}
	
	statusStr, ok := response["status"].(string)
	if !ok {
		return "", errors.New("invalid response: missing status")
	}
	
	return TaskStatus(statusStr), nil
}

// Wait waits for the task to complete
func (t *Task) Wait(ctx context.Context, timeout time.Duration) (TaskResult, error) {
	start := time.Now()
	for {
		status, err := t.Query(ctx)
		if err != nil {
			return TaskResult{}, err
		}
		
		if status == TaskStatusCompleted || status == TaskStatusFailed || status == TaskStatusCancelled {
			path := fmt.Sprintf("/api/tasks/%s", t.ID)
			response, err := t.client.request(ctx, "GET", path, nil)
			if err != nil {
				return TaskResult{}, err
			}
			
			output, _ := response["output"].(string)
			error, _ := response["error"].(string)
			
			return TaskResult{
				ID:     t.ID,
				Status: status,
				Output: output,
				Error:  error,
			}, nil
		}
		
		if timeout > 0 && time.Since(start) > timeout {
			return TaskResult{}, fmt.Errorf("task timed out after %v", timeout)
		}
		
		time.Sleep(500 * time.Millisecond)
	}
}

// Cancel cancels the task
func (t *Task) Cancel(ctx context.Context) (bool, error) {
	path := fmt.Sprintf("/api/tasks/%s/cancel", t.ID)
	
	response, err := t.client.request(ctx, "POST", path, nil)
	if err != nil {
		return false, err
	}
	
	success, ok := response["success"].(bool)
	if !ok {
		return false, errors.New("invalid response: missing success")
	}
	
	return success, nil
}

// TaskResult represents the result of a task
type TaskResult struct {
	ID     string
	Status TaskStatus
	Output string
	Error  string
}
