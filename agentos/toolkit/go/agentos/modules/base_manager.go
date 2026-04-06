// AgentOS Go SDK - 基础管理器模块
// Version: 3.0.0
// Last updated: 2026-04-05
//
// 提供泛型 BaseManager 实现，减少各 Manager 模块的重复代码。
// 使用 Go 1.18+ 泛型特性。

package modules

import (
	"context"
	"fmt"
	"time"

	"github.com/spharx/agentos/toolkit/go/agentos"
	"github.com/spharx/agentos/toolkit/go/agentos/client"
	"github.com/spharx/agentos/toolkit/go/agentos/types"
	"github.com/spharx/agentos/toolkit/go/agentos/utils"
)

// ResourceConverter 定义资源类型转换接口
type ResourceConverter[T any] interface {
	// Convert 从 map 转换为具体资源类型
	Convert(data map[string]interface{}) (*T, error)
}

// BaseManager 泛型基础管理器
type BaseManager[T any] struct {
	api             client.APIClient
	resourceType    string
	converter       ResourceConverter[T]
}

// NewBaseManager 创建泛型基础管理器
func NewBaseManager[T any](api client.APIClient, resourceType string, converter ResourceConverter[T]) *BaseManager[T] {
	return &BaseManager[T]{
		api:          api,
		resourceType: resourceType,
		converter:    converter,
	}
}

// GetAPI 获取 API 客户端
func (bm *BaseManager[T]) GetAPI() client.APIClient {
	return bm.api
}

// GetResourceType 获取资源类型
func (bm *BaseManager[T]) GetResourceType() string {
	return bm.resourceType
}

// ExecuteGet 执行通用 GET 请求
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

// ExecutePost 执行通用 POST 请求
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

// ExecuteDelete 执行通用 DELETE 请求
func (bm *BaseManager[T]) ExecuteDelete(ctx context.Context, path string, errorMsg string) error {
	resp, err := bm.api.Delete(ctx, path)
	if err != nil {
		return err
	}

	if success, ok := resp.Data.(map[string]interface{})["success"]; ok {
		if !success.(bool) {
			return agentos.NewError(agentos.CodeInternal, errorMsg, nil)
		}
	}
	return nil
}

// ValidateAndExtract 验证并提取数据
func (bm *BaseManager[T]) ValidateAndExtract(resp *types.APIResponse, errorMsg string) (*T, error) {
	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return nil, agentos.NewError(agentos.CodeInvalidResponse, errorMsg, nil)
	}
	return bm.converter.Convert(data)
}

// BuildListOptions 构建列表查询参数
func (bm *BaseManager[T]) BuildListOptions(opts *types.ListOptions) []types.RequestOption {
	var requestOpts []types.RequestOption
	if opts != nil {
		requestOpts = opts.ToQueryParams()
	}
	return requestOpts
}

// LogOperation 记录操作日志
func (bm *BaseManager[T]) LogOperation(operation string, resourceID string) {
	agentos.GetLogger().Debug(fmt.Sprintf("[%s] %s: ID=%s", bm.resourceType, operation, resourceID))
}

// LogError 记录错误日志
func (bm *BaseManager[T]) LogError(operation string, err error) {
	agentos.GetLogger().Error(fmt.Sprintf("[%s] %s failed: %v", bm.resourceType, operation, err))
}

// ===== 预定义转换器 =====

// TaskConverter 任务类型转换器
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

// MemoryConverter 记忆类型转换器
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

// SessionConverter 会话类型转换器
type SessionConverter struct{}

func (c *SessionConverter) Convert(data map[string]interface{}) (*types.Session, error) {
	return &types.Session{
		ID:          utils.GetString(data, "session_id"),
		Status:      types.SessionStatus(utils.GetString(data, "status")),
		Context:     utils.GetMap(data, "context"),
		CreatedAt:   utils.GetTime(data, "created_at", time.Now()),
		UpdatedAt:   utils.GetTime(data, "updated_at", time.Now()),
		ExpiresAt:   utils.GetTime(data, "expires_at", time.Time{}),
	}, nil
}

// SkillConverter 技能类型转换器
type SkillConverter struct{}

func (c *SkillConverter) Convert(data map[string]interface{}) (*types.Skill, error) {
	return &types.Skill{
		Name:        utils.GetString(data, "skill_name"),
		Description: utils.GetString(data, "description"),
		Status:      types.SkillStatus(utils.GetString(data, "status")),
		Loaded:      utils.GetBool(data, "loaded", false),
		CreatedAt:   utils.GetTime(data, "created_at", time.Now()),
		UpdatedAt:   utils.GetTime(data, "updated_at", time.Now()),
	}, nil
}
