// AgentOS Go SDK - 技能管理模块
// Version: 3.0.0
// Last updated: 2026-03-22
//
// 提供技能的注册、加载、执行、卸载及验证功能。
// 对应 Python SDK: modules/skill/__init__.py + skill.py

package skill

import (
	"context"
	"fmt"

	"github.com/spharx/agentos/toolkit/go/agentos"
	"github.com/spharx/agentos/toolkit/go/agentos/client"
	"github.com/spharx/agentos/toolkit/go/agentos/types"
	"github.com/spharx/agentos/toolkit/go/agentos/utils"
)

// SkillExecuteRequest 批量执行时的单条请求
type SkillExecuteRequest struct {
	SkillID    string
	Parameters map[string]interface{}
}

// SkillManager 管理技能完整生命周期
type SkillManager struct {
	api client.APIClient
}

// NewSkillManager 创建新的技能管理器实例
func NewSkillManager(api client.APIClient) *SkillManager {
	return &SkillManager{api: api}
}

// Load 加载指定技能到运行时
func (sm *SkillManager) Load(ctx context.Context, skillID string) (*types.Skill, error) {
	if skillID == "" {
		return nil, agentos.NewError(agentos.CodeMissingParameter, "技能ID不能为空", nil)
	}

	resp, err := sm.api.Post(ctx, fmt.Sprintf("/api/v1/skills/%s/load", skillID), nil)
	if err != nil {
		return nil, err
	}

	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return nil, agentos.NewError(agentos.CodeInvalidResponse, "技能加载响应格式异常", nil)
	}

	return &types.Skill{
		ID:         skillID,
		Name:       utils.GetString(data, "name"),
		Version:    utils.GetString(data, "version"),
		Status:     types.SkillStatusActive,
		Parameters: utils.GetMap(data, "parameters"),
		Metadata:   utils.GetMap(data, "metadata"),
	}, nil
}

// Get 获取指定技能的详细信息
func (sm *SkillManager) Get(ctx context.Context, skillID string) (*types.Skill, error) {
	if skillID == "" {
		return nil, agentos.NewError(agentos.CodeMissingParameter, "技能ID不能为空", nil)
	}

	resp, err := sm.api.Get(ctx, fmt.Sprintf("/api/v1/skills/%s", skillID))
	if err != nil {
		return nil, err
	}

	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return nil, agentos.NewError(agentos.CodeInvalidResponse, "技能详情响应格式异常", nil)
	}

	return parseSkillFromMap(data, skillID), nil
}

// Execute 执行指定技能
func (sm *SkillManager) Execute(ctx context.Context, skillID string, parameters map[string]interface{}) (*types.SkillResult, error) {
	if skillID == "" {
		return nil, agentos.NewError(agentos.CodeMissingParameter, "技能ID不能为空", nil)
	}

	resp, err := sm.api.Post(ctx, fmt.Sprintf("/api/v1/skills/%s/execute", skillID), map[string]interface{}{
		"parameters": parameters,
	})
	if err != nil {
		return nil, err
	}

	return parseSkillResult(resp)
}

// ExecuteWithContext 在指定会话上下文中执行技能
func (sm *SkillManager) ExecuteWithContext(ctx context.Context, skillID string, parameters map[string]interface{}, sessionID string) (*types.SkillResult, error) {
	if skillID == "" {
		return nil, agentos.NewError(agentos.CodeMissingParameter, "技能ID不能为空", nil)
	}

	resp, err := sm.api.Post(ctx, fmt.Sprintf("/api/v1/skills/%s/execute", skillID), map[string]interface{}{
		"parameters": parameters,
		"session_id": sessionID,
	})
	if err != nil {
		return nil, err
	}

	return parseSkillResult(resp)
}

// Unload 卸载指定技能，释放运行时资源
func (sm *SkillManager) Unload(ctx context.Context, skillID string) error {
	if skillID == "" {
		return agentos.NewError(agentos.CodeMissingParameter, "技能ID不能为空", nil)
	}
	_, err := sm.api.Post(ctx, fmt.Sprintf("/api/v1/skills/%s/unload", skillID), nil)
	return err
}

// List 列出技能，支持分页和过滤
func (sm *SkillManager) List(ctx context.Context, opts *types.ListOptions) ([]types.Skill, error) {
	path := "/api/v1/skills"
	if opts != nil {
		path = utils.BuildURL(path, opts.ToQueryParams())
	}

	resp, err := sm.api.Get(ctx, path)
	if err != nil {
		return nil, err
	}

	return parseSkillList(resp)
}

// ListLoaded 列出当前已加载的技能
func (sm *SkillManager) ListLoaded(ctx context.Context) ([]types.Skill, error) {
	resp, err := sm.api.Get(ctx, "/api/v1/skills?status=loaded")
	if err != nil {
		return nil, err
	}

	return parseSkillList(resp)
}

// Register 注册新的技能
func (sm *SkillManager) Register(ctx context.Context, name string, description string, parameters map[string]interface{}) (*types.Skill, error) {
	if name == "" {
		return nil, agentos.NewError(agentos.CodeMissingParameter, "技能名称不能为空", nil)
	}

	resp, err := sm.api.Post(ctx, "/api/v1/skills", map[string]interface{}{
		"name":        name,
		"description": description,
		"parameters":  parameters,
	})
	if err != nil {
		return nil, err
	}

	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return nil, agentos.NewError(agentos.CodeInvalidResponse, "技能注册响应格式异常", nil)
	}

	return &types.Skill{
		ID:          utils.GetString(data, "skill_id"),
		Name:        name,
		Description: description,
		Parameters:  parameters,
		Status:      types.SkillStatusActive,
		Metadata:    utils.GetMap(data, "metadata"),
	}, nil
}

// Update 更新指定技能的描述和参数
func (sm *SkillManager) Update(ctx context.Context, skillID string, description string, parameters map[string]interface{}) (*types.Skill, error) {
	if skillID == "" {
		return nil, agentos.NewError(agentos.CodeMissingParameter, "技能ID不能为空", nil)
	}

	resp, err := sm.api.Put(ctx, fmt.Sprintf("/api/v1/skills/%s", skillID), map[string]interface{}{
		"description": description,
		"parameters":  parameters,
	})
	if err != nil {
		return nil, err
	}

	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return nil, agentos.NewError(agentos.CodeInvalidResponse, "技能更新响应格式异常", nil)
	}

	return parseSkillFromMap(data, skillID), nil
}

// Delete 删除指定技能
func (sm *SkillManager) Delete(ctx context.Context, skillID string) error {
	if skillID == "" {
		return agentos.NewError(agentos.CodeMissingParameter, "技能ID不能为空", nil)
	}
	_, err := sm.api.Delete(ctx, fmt.Sprintf("/api/v1/skills/%s", skillID))
	return err
}

// GetInfo 获取指定技能的只读元信息
func (sm *SkillManager) GetInfo(ctx context.Context, skillID string) (*types.SkillInfo, error) {
	if skillID == "" {
		return nil, agentos.NewError(agentos.CodeMissingParameter, "技能ID不能为空", nil)
	}

	resp, err := sm.api.Get(ctx, fmt.Sprintf("/api/v1/skills/%s/info", skillID))
	if err != nil {
		return nil, err
	}

	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return nil, agentos.NewError(agentos.CodeInvalidResponse, "技能信息响应格式异常", nil)
	}

	return &types.SkillInfo{
		Name:        utils.GetString(data, "skill_name"),
		Description: utils.GetString(data, "description"),
		Version:     utils.GetString(data, "version"),
		Parameters:  utils.GetMap(data, "parameters"),
	}, nil
}

// Validate 验证技能参数是否合法
func (sm *SkillManager) Validate(ctx context.Context, skillID string, parameters map[string]interface{}) (bool, []string, error) {
	if skillID == "" {
		return false, nil, agentos.NewError(agentos.CodeMissingParameter, "技能ID不能为空", nil)
	}

	resp, err := sm.api.Post(ctx, fmt.Sprintf("/api/v1/skills/%s/validate", skillID), map[string]interface{}{
		"parameters": parameters,
	})
	if err != nil {
		return false, nil, err
	}

	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return false, nil, agentos.NewError(agentos.CodeInvalidResponse, "技能验证响应格式异常", nil)
	}

	valid := utils.GetBool(data, "valid")
	var validationErrors []string
	if errs := utils.GetInterfaceSlice(data, "errors"); errs != nil {
		for _, e := range errs {
			if errStr, ok := e.(string); ok {
				validationErrors = append(validationErrors, errStr)
			}
		}
	}

	return valid, validationErrors, nil
}

// Count 获取技能总数
func (sm *SkillManager) Count(ctx context.Context) (int64, error) {
	resp, err := sm.api.Get(ctx, "/api/v1/skills/count")
	if err != nil {
		return 0, err
	}
	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return 0, nil
	}
	return utils.GetInt64(data, "count"), nil
}

// CountLoaded 获取已加载技能数
func (sm *SkillManager) CountLoaded(ctx context.Context) (int64, error) {
	resp, err := sm.api.Get(ctx, "/api/v1/skills/count?status=loaded")
	if err != nil {
		return 0, err
	}
	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return 0, nil
	}
	return utils.GetInt64(data, "count"), nil
}

// Search 搜索技能
func (sm *SkillManager) Search(ctx context.Context, query string, topK int) ([]types.Skill, error) {
	if query == "" {
		return nil, agentos.NewError(agentos.CodeMissingParameter, "搜索查询不能为空", nil)
	}
	if topK <= 0 {
		topK = 10
	}

	path := utils.BuildURL("/api/v1/skills/search", map[string]string{
		"query": query,
		"top_k": fmt.Sprintf("%d", topK),
	})

	resp, err := sm.api.Get(ctx, path)
	if err != nil {
		return nil, err
	}

	return parseSkillList(resp)
}

// BatchExecute 批量执行多个技能
func (sm *SkillManager) BatchExecute(ctx context.Context, requests []SkillExecuteRequest) ([]types.SkillResult, error) {
	results := make([]types.SkillResult, 0, len(requests))
	for _, req := range requests {
		result, err := sm.Execute(ctx, req.SkillID, req.Parameters)
		if err != nil {
			return results, err
		}
		results = append(results, *result)
	}
	return results, nil
}

// GetStats 获取指定技能的执行统计数据
func (sm *SkillManager) GetStats(ctx context.Context, skillID string) (map[string]int64, error) {
	if skillID == "" {
		return nil, agentos.NewError(agentos.CodeMissingParameter, "技能ID不能为空", nil)
	}

	resp, err := sm.api.Get(ctx, fmt.Sprintf("/api/v1/skills/%s/stats", skillID))
	if err != nil {
		return nil, err
	}
	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return make(map[string]int64), nil
	}
	return utils.ExtractInt64Stats(data), nil
}

// parseSkillFromMap 从 map 解析 Skill 结构
func parseSkillFromMap(data map[string]interface{}, fallbackID string) *types.Skill {
	id := utils.GetString(data, "skill_id")
	if id == "" {
		id = fallbackID
	}
	return &types.Skill{
		ID:          id,
		Name:        utils.GetString(data, "name"),
		Version:     utils.GetString(data, "version"),
		Description: utils.GetString(data, "description"),
		Status:      types.SkillStatus(utils.GetString(data, "status")),
		Parameters:  utils.GetMap(data, "parameters"),
		Metadata:    utils.GetMap(data, "metadata"),
		CreatedAt:   utils.ParseTimeFromMap(data, "created_at"),
	}
}

// parseSkillList 从 APIResponse 解析 Skill 列表
func parseSkillList(resp *types.APIResponse) ([]types.Skill, error) {
	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return nil, agentos.NewError(agentos.CodeInvalidResponse, "技能列表响应格式异常", nil)
	}

	items := utils.GetInterfaceSlice(data, "skills")
	skills := make([]types.Skill, 0, len(items))
	for _, item := range items {
		if m, ok := item.(map[string]interface{}); ok {
			skills = append(skills, *parseSkillFromMap(m, ""))
		}
	}
	return skills, nil
}

// parseSkillResult 从 APIResponse 解析 SkillResult
func parseSkillResult(resp *types.APIResponse) (*types.SkillResult, error) {
	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return nil, agentos.NewError(agentos.CodeInvalidResponse, "技能执行响应格式异常", nil)
	}

	return &types.SkillResult{
		Success: utils.GetBool(data, "success"),
		Output:  data["output"],
		Error:   utils.GetString(data, "error"),
	}, nil
}

