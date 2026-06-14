package modules

import (
	"context"
	"time"

	"github.com/spharx/agentos/toolkit/go/agentos"
	"github.com/spharx/agentos/toolkit/go/agentos/client"
	"github.com/spharx/agentos/toolkit/go/agentos/types"
	"github.com/spharx/agentos/toolkit/go/agentos/utils"
)

type ResourceConverter[T any] interface {
	Convert(data map[string]interface{}) (*T, error)
}

type BaseManager[T any] struct {
	api          client.APIClient
	resourceType string
	converter    ResourceConverter[T]
}

func NewBaseManager[T any](api client.APIClient, resourceType string, converter ResourceConverter[T]) *BaseManager[T] {
	return &BaseManager[T]{
		api:          api,
		resourceType: resourceType,
		converter:    converter,
	}
}

func (bm *BaseManager[T]) GetAPI() client.APIClient {
	return bm.api
}

func (bm *BaseManager[T]) GetResourceType() string {
	return bm.resourceType
}

func (bm *BaseManager[T]) ExecuteGet(ctx context.Context, path string, errorMsg string) (*T, error) {
	resp, err := bm.api.Get(ctx, path)
	if err != nil {
		return nil, err
	}

	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return nil, agentos.NewError(agentos.CodeInvalidResponse, errorMsg, nil)
	}

	return bm.converter.Convert(data)
}

func (bm *BaseManager[T]) ExecutePost(ctx context.Context, path string, body map[string]interface{}, errorMsg string) (*T, error) {
	resp, err := bm.api.Post(ctx, path, body)
	if err != nil {
		return nil, err
	}

	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return nil, agentos.NewError(agentos.CodeInvalidResponse, errorMsg, nil)
	}

	return bm.converter.Convert(data)
}

func (bm *BaseManager[T]) ExecuteDelete(ctx context.Context, path string, errorMsg string) error {
	resp, err := bm.api.Delete(ctx, path)
	if err != nil {
		return err
	}

	if dataMap, ok := resp.Data.(map[string]interface{}); ok {
		if success, ok := dataMap["success"]; ok {
			if b, ok := success.(bool); ok && !b {
				return agentos.NewError(agentos.CodeInternal, errorMsg, nil)
			}
		}
	}
	return nil
}

func (bm *BaseManager[T]) ValidateAndExtract(resp *types.APIResponse, errorMsg string) (*T, error) {
	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return nil, agentos.NewError(agentos.CodeInvalidResponse, errorMsg, nil)
	}
	return bm.converter.Convert(data)
}

func (bm *BaseManager[T]) BuildListOptions(opts *types.ListOptions) []types.RequestOption {
	var requestOpts []types.RequestOption
	if opts != nil {
		for k, v := range opts.ToQueryParams() {
			requestOpts = append(requestOpts, types.WithQueryParam(k, v))
		}
	}
	return requestOpts
}

func (bm *BaseManager[T]) LogOperation(operation string, resourceID string) {
	agentos.GetLogger().Printf("[DEBUG] [%s] %s: ID=%s", bm.resourceType, operation, resourceID)
}

func (bm *BaseManager[T]) LogError(operation string, err error) {
	agentos.GetLogger().Printf("[ERROR] [%s] %s failed: %v", bm.resourceType, operation, err)
}

type TaskConverter struct{}

func (c *TaskConverter) Convert(data map[string]interface{}) (*types.Task, error) {
	return &types.Task{
		ID:          utils.GetString(data, "task_id"),
		Description: utils.GetString(data, "description"),
		Status:      types.TaskStatus(utils.GetString(data, "status")),
		CreatedAt:   utils.GetTime(data, "created_at", time.Now()),
		UpdatedAt:   utils.GetTime(data, "updated_at", time.Now()),
	}, nil
}

type MemoryConverter struct{}

func (c *MemoryConverter) Convert(data map[string]interface{}) (*types.Memory, error) {
	return &types.Memory{
		ID:        utils.GetString(data, "memory_id"),
		Content:   utils.GetString(data, "content"),
		Layer:     types.MemoryLayer(utils.GetString(data, "layer")),
		Score:     utils.GetFloat(data, "score", 1.0),
		Metadata:  utils.GetMap(data, "metadata"),
		CreatedAt: utils.GetTime(data, "created_at", time.Now()),
		UpdatedAt: utils.GetTime(data, "updated_at", time.Now()),
	}, nil
}

type SessionConverter struct{}

func (c *SessionConverter) Convert(data map[string]interface{}) (*types.Session, error) {
	return &types.Session{
		ID:           utils.GetString(data, "session_id"),
		Status:       types.SessionStatus(utils.GetString(data, "status")),
		Context:      utils.GetMap(data, "context"),
		CreatedAt:    utils.GetTime(data, "created_at", time.Now()),
		LastActivity: utils.GetTime(data, "last_activity", time.Now()),
	}, nil
}

type SkillConverter struct{}

func (c *SkillConverter) Convert(data map[string]interface{}) (*types.Skill, error) {
	return &types.Skill{
		ID:          utils.GetString(data, "skill_id"),
		Name:        utils.GetString(data, "name"),
		Description: utils.GetString(data, "description"),
		Status:      types.SkillStatus(utils.GetString(data, "status")),
		CreatedAt:   utils.GetTime(data, "created_at", time.Now()),
	}, nil
}
