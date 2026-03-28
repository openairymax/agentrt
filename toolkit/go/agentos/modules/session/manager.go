// AgentOS Go SDK - 会话管理模块
// Version: 3.0.0
// Last updated: 2026-03-22
//
// 提供会话的创建、查询、上下文管理、清理等生命周期管理功能。
// 对应 Python SDK: modules/session/__init__.py + session.py

package session

import (
	"context"
	"fmt"
	"time"

	"github.com/spharx/agentos/toolkit/go/agentos"
	"github.com/spharx/agentos/toolkit/go/agentos/client"
	"github.com/spharx/agentos/toolkit/go/agentos/types"
	"github.com/spharx/agentos/toolkit/go/agentos/utils"
)

// SessionManager 管理会话完整生命周期
type SessionManager struct {
	api client.APIClient
}

// NewSessionManager 创建新的会话管理器实例
func NewSessionManager(api client.APIClient) *SessionManager {
	return &SessionManager{api: api}
}

// Create 创建新的用户会话
func (sm *SessionManager) Create(ctx context.Context, userID string) (*types.Session, error) {
	return sm.CreateWithOptions(ctx, userID, nil)
}

// CreateWithOptions 使用元数据选项创建新会话
func (sm *SessionManager) CreateWithOptions(ctx context.Context, userID string, metadata map[string]interface{}) (*types.Session, error) {
	body := map[string]interface{}{"user_id": userID}
	if metadata != nil {
		body["metadata"] = metadata
	}

	resp, err := sm.api.Post(ctx, "/api/v1/sessions", body)
	if err != nil {
		return nil, err
	}

	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return nil, agentos.NewError(agentos.CodeInvalidResponse, "会话创建响应格式异常", nil)
	}

	return &types.Session{
		ID:           utils.GetString(data, "session_id"),
		UserID:       userID,
		Status:       types.SessionStatusActive,
		Context:      make(map[string]interface{}),
		Metadata:     metadata,
		CreatedAt:    time.Now(),
		LastActivity: time.Now(),
	}, nil
}

// Get 获取指定会话的详细信息
func (sm *SessionManager) Get(ctx context.Context, sessionID string) (*types.Session, error) {
	if sessionID == "" {
		return nil, agentos.NewError(agentos.CodeMissingParameter, "会话ID不能为空", nil)
	}

	resp, err := sm.api.Get(ctx, fmt.Sprintf("/api/v1/sessions/%s", sessionID))
	if err != nil {
		return nil, err
	}

	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return nil, agentos.NewError(agentos.CodeInvalidResponse, "会话详情响应格式异常", nil)
	}

	return parseSessionFromMap(data), nil
}

// SetContext 设置会话上下文中的指定键值对
func (sm *SessionManager) SetContext(ctx context.Context, sessionID string, key string, value interface{}) error {
	if sessionID == "" {
		return agentos.NewError(agentos.CodeMissingParameter, "会话ID不能为空", nil)
	}
	if key == "" {
		return agentos.NewError(agentos.CodeMissingParameter, "上下文键不能为空", nil)
	}

	_, err := sm.api.Post(ctx, fmt.Sprintf("/api/v1/sessions/%s/context", sessionID), map[string]interface{}{
		"key":   key,
		"value": value,
	})
	return err
}

// GetContext 获取会话上下文中指定键的值
func (sm *SessionManager) GetContext(ctx context.Context, sessionID string, key string) (interface{}, error) {
	if sessionID == "" {
		return nil, agentos.NewError(agentos.CodeMissingParameter, "会话ID不能为空", nil)
	}

	resp, err := sm.api.Get(ctx, fmt.Sprintf("/api/v1/sessions/%s/context/%s", sessionID, key))
	if err != nil {
		return nil, err
	}

	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return nil, nil
	}
	return data["value"], nil
}

// GetAllContext 获取会话的全部上下文数据
func (sm *SessionManager) GetAllContext(ctx context.Context, sessionID string) (map[string]interface{}, error) {
	if sessionID == "" {
		return nil, agentos.NewError(agentos.CodeMissingParameter, "会话ID不能为空", nil)
	}

	resp, err := sm.api.Get(ctx, fmt.Sprintf("/api/v1/sessions/%s/context", sessionID))
	if err != nil {
		return nil, err
	}

	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return make(map[string]interface{}), nil
	}
	return utils.GetMap(data, "context"), nil
}

// DeleteContext 删除会话上下文中的指定键
func (sm *SessionManager) DeleteContext(ctx context.Context, sessionID string, key string) error {
	if sessionID == "" {
		return agentos.NewError(agentos.CodeMissingParameter, "会话ID不能为空", nil)
	}
	_, err := sm.api.Delete(ctx, fmt.Sprintf("/api/v1/sessions/%s/context/%s", sessionID, key))
	return err
}

// Close 关闭指定会话
func (sm *SessionManager) Close(ctx context.Context, sessionID string) error {
	if sessionID == "" {
		return agentos.NewError(agentos.CodeMissingParameter, "会话ID不能为空", nil)
	}
	_, err := sm.api.Delete(ctx, fmt.Sprintf("/api/v1/sessions/%s", sessionID))
	return err
}

// List 列出会话，支持分页和过滤
func (sm *SessionManager) List(ctx context.Context, opts *types.ListOptions) ([]types.Session, error) {
	path := "/api/v1/sessions"
	if opts != nil {
		path = utils.BuildURL(path, opts.ToQueryParams())
	}

	resp, err := sm.api.Get(ctx, path)
	if err != nil {
		return nil, err
	}

	return parseSessionList(resp)
}

// ListByUser 列出指定用户的所有会话
func (sm *SessionManager) ListByUser(ctx context.Context, userID string, opts *types.ListOptions) ([]types.Session, error) {
	params := map[string]string{"user_id": userID}
	if opts != nil {
		for k, v := range opts.ToQueryParams() {
			params[k] = v
		}
	}

	resp, err := sm.api.Get(ctx, utils.BuildURL("/api/v1/sessions", params))
	if err != nil {
		return nil, err
	}

	return parseSessionList(resp)
}

// ListActive 列出当前所有活跃会话
func (sm *SessionManager) ListActive(ctx context.Context) ([]types.Session, error) {
	resp, err := sm.api.Get(ctx, "/api/v1/sessions?status=active")
	if err != nil {
		return nil, err
	}

	return parseSessionList(resp)
}

// Update 更新会话的元数据
func (sm *SessionManager) Update(ctx context.Context, sessionID string, metadata map[string]interface{}) (*types.Session, error) {
	if sessionID == "" {
		return nil, agentos.NewError(agentos.CodeMissingParameter, "会话ID不能为空", nil)
	}

	resp, err := sm.api.Put(ctx, fmt.Sprintf("/api/v1/sessions/%s", sessionID), map[string]interface{}{
		"metadata": metadata,
	})
	if err != nil {
		return nil, err
	}

	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return nil, agentos.NewError(agentos.CodeInvalidResponse, "会话更新响应格式异常", nil)
	}

	return parseSessionFromMap(data), nil
}

// Refresh 刷新会话的活跃时间，防止过期
func (sm *SessionManager) Refresh(ctx context.Context, sessionID string) error {
	if sessionID == "" {
		return agentos.NewError(agentos.CodeMissingParameter, "会话ID不能为空", nil)
	}
	_, err := sm.api.Post(ctx, fmt.Sprintf("/api/v1/sessions/%s/refresh", sessionID), nil)
	return err
}

// IsExpired 检查指定会话是否已过期
func (sm *SessionManager) IsExpired(ctx context.Context, sessionID string) (bool, error) {
	sess, err := sm.Get(ctx, sessionID)
	if err != nil {
		return false, err
	}
	return sess.Status == types.SessionStatusExpired, nil
}

// Count 获取会话总数
func (sm *SessionManager) Count(ctx context.Context) (int64, error) {
	resp, err := sm.api.Get(ctx, "/api/v1/sessions/count")
	if err != nil {
		return 0, err
	}
	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return 0, nil
	}
	return utils.GetInt64(data, "count"), nil
}

// CountActive 获取活跃会话数
func (sm *SessionManager) CountActive(ctx context.Context) (int64, error) {
	resp, err := sm.api.Get(ctx, "/api/v1/sessions/count?status=active")
	if err != nil {
		return 0, err
	}
	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return 0, nil
	}
	return utils.GetInt64(data, "count"), nil
}

// CleanExpired 清理所有已过期的会话，返回清理数量
func (sm *SessionManager) CleanExpired(ctx context.Context) (int64, error) {
	resp, err := sm.api.Post(ctx, "/api/v1/sessions/clean-expired", nil)
	if err != nil {
		return 0, err
	}
	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return 0, nil
	}
	return utils.GetInt64(data, "cleaned"), nil
}

// parseSessionFromMap 从 map 解析 Session 结构
func parseSessionFromMap(data map[string]interface{}) *types.Session {
	return &types.Session{
		ID:           utils.GetString(data, "session_id"),
		UserID:       utils.GetString(data, "user_id"),
		Status:       types.SessionStatus(utils.GetString(data, "status")),
		Context:      utils.GetMap(data, "context"),
		Metadata:     utils.GetMap(data, "metadata"),
		CreatedAt:    utils.ParseTimeFromMap(data, "created_at"),
		LastActivity: utils.ParseTimeFromMap(data, "last_activity"),
	}
}

// parseSessionList 从 APIResponse 解析 Session 列表
func parseSessionList(resp *types.APIResponse) ([]types.Session, error) {
	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return nil, agentos.NewError(agentos.CodeInvalidResponse, "会话列表响应格式异常", nil)
	}

	items := utils.GetInterfaceSlice(data, "sessions")
	sessions := make([]types.Session, 0, len(items))
	for _, item := range items {
		if m, ok := item.(map[string]interface{}); ok {
			sessions = append(sessions, *parseSessionFromMap(m))
		}
	}
	return sessions, nil
}

