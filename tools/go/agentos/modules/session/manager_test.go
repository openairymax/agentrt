// AgentOS Go SDK - 会话管理模块单元测试
// Version: 3.0.0

package session

import (
	"context"
	"errors"
	"testing"

	"github.com/spharx/agentos/tools/go/agentos/client"
	"github.com/spharx/agentos/tools/go/agentos/types"
)

func TestNewSessionManager(t *testing.T) {
	sm := NewSessionManager(nil)
	if sm == nil {
		t.Fatal("SessionManager 不应为 nil")
	}
}

func TestSessionManager_Create(t *testing.T) {
	mock := &client.MockAPIClient{
		PostFn: func(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{Success: true, Data: map[string]interface{}{"session_id": "s1"}}, nil
		},
	}
	sm := NewSessionManager(mock)
	sess, err := sm.Create(context.Background(), "user1")
	if err != nil {
		t.Fatalf("Create error = %v", err)
	}
	if sess.ID != "s1" {
		t.Errorf("sess.ID = %q", sess.ID)
	}
	if sess.UserID != "user1" {
		t.Errorf("sess.UserID = %q", sess.UserID)
	}
	if sess.Status != types.SessionStatusActive {
		t.Errorf("status = %q", sess.Status)
	}
}

func TestSessionManager_CreateWithOptions(t *testing.T) {
	mock := &client.MockAPIClient{
		PostFn: func(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{Success: true, Data: map[string]interface{}{"session_id": "s2"}}, nil
		},
	}
	sm := NewSessionManager(mock)
	sess, err := sm.CreateWithOptions(context.Background(), "user1", map[string]interface{}{"env": "test"})
	if err != nil {
		t.Fatalf("CreateWithOptions error = %v", err)
	}
	if sess.ID != "s2" {
		t.Errorf("sess.ID = %q", sess.ID)
	}
}

func TestSessionManager_Create_APIError(t *testing.T) {
	mock := &client.MockAPIClient{
		PostFn: func(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
			return nil, errors.New("network error")
		},
	}
	sm := NewSessionManager(mock)
	_, err := sm.Create(context.Background(), "user1")
	if err == nil {
		t.Error("API 错误应传播")
	}
}

func TestSessionManager_Get(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data:    map[string]interface{}{"session_id": "s1", "user_id": "u1", "status": "active"},
			}, nil
		},
	}
	sm := NewSessionManager(mock)
	sess, err := sm.Get(context.Background(), "s1")
	if err != nil {
		t.Fatalf("Get error = %v", err)
	}
	if sess.ID != "s1" {
		t.Errorf("sess.ID = %q", sess.ID)
	}
}

func TestSessionManager_Get_EmptyID(t *testing.T) {
	sm := NewSessionManager(nil)
	_, err := sm.Get(context.Background(), "")
	if err == nil {
		t.Error("空 ID 应返回错误")
	}
}

func TestSessionManager_SetContext(t *testing.T) {
	mock := &client.MockAPIClient{
		PostFn: func(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{Success: true, Data: nil}, nil
		},
	}
	sm := NewSessionManager(mock)
	err := sm.SetContext(context.Background(), "s1", "key1", "value1")
	if err != nil {
		t.Fatalf("SetContext error = %v", err)
	}
}

func TestSessionManager_SetContext_EmptySessionID(t *testing.T) {
	sm := NewSessionManager(nil)
	err := sm.SetContext(context.Background(), "", "key", "val")
	if err == nil {
		t.Error("空 sessionID 应返回错误")
	}
}

func TestSessionManager_SetContext_EmptyKey(t *testing.T) {
	sm := NewSessionManager(nil)
	err := sm.SetContext(context.Background(), "s1", "", "val")
	if err == nil {
		t.Error("空 key 应返回错误")
	}
}

func TestSessionManager_GetContext(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data:    map[string]interface{}{"value": "result_value"},
			}, nil
		},
	}
	sm := NewSessionManager(mock)
	val, err := sm.GetContext(context.Background(), "s1", "key1")
	if err != nil {
		t.Fatalf("GetContext error = %v", err)
	}
	if val != "result_value" {
		t.Errorf("value = %v", val)
	}
}

func TestSessionManager_GetContext_EmptyID(t *testing.T) {
	sm := NewSessionManager(nil)
	_, err := sm.GetContext(context.Background(), "", "key")
	if err == nil {
		t.Error("空 ID 应返回错误")
	}
}

func TestSessionManager_GetAllContext(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data:    map[string]interface{}{"context": map[string]interface{}{"k1": "v1", "k2": "v2"}},
			}, nil
		},
	}
	sm := NewSessionManager(mock)
	ctx, err := sm.GetAllContext(context.Background(), "s1")
	if err != nil {
		t.Fatalf("GetAllContext error = %v", err)
	}
	if len(ctx) != 2 {
		t.Errorf("len(ctx) = %d", len(ctx))
	}
}

func TestSessionManager_DeleteContext(t *testing.T) {
	mock := &client.MockAPIClient{
		DelFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{Success: true, Data: nil}, nil
		},
	}
	sm := NewSessionManager(mock)
	err := sm.DeleteContext(context.Background(), "s1", "key1")
	if err != nil {
		t.Fatalf("DeleteContext error = %v", err)
	}
}

func TestSessionManager_Close(t *testing.T) {
	mock := &client.MockAPIClient{
		DelFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{Success: true, Data: nil}, nil
		},
	}
	sm := NewSessionManager(mock)
	err := sm.Close(context.Background(), "s1")
	if err != nil {
		t.Fatalf("Close error = %v", err)
	}
}

func TestSessionManager_Close_EmptyID(t *testing.T) {
	sm := NewSessionManager(nil)
	err := sm.Close(context.Background(), "")
	if err == nil {
		t.Error("空 ID 应返回错误")
	}
}

func TestSessionManager_List(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data: map[string]interface{}{
					"sessions": []interface{}{
						map[string]interface{}{"session_id": "s1", "user_id": "u1", "status": "active"},
					},
				},
			}, nil
		},
	}
	sm := NewSessionManager(mock)
	sessions, err := sm.List(context.Background(), nil)
	if err != nil {
		t.Fatalf("List error = %v", err)
	}
	if len(sessions) != 1 {
		t.Errorf("len(sessions) = %d", len(sessions))
	}
}

func TestSessionManager_ListByUser(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data: map[string]interface{}{
					"sessions": []interface{}{
						map[string]interface{}{"session_id": "s1", "user_id": "u1"},
						map[string]interface{}{"session_id": "s2", "user_id": "u1"},
					},
				},
			}, nil
		},
	}
	sm := NewSessionManager(mock)
	sessions, err := sm.ListByUser(context.Background(), "u1", nil)
	if err != nil {
		t.Fatalf("ListByUser error = %v", err)
	}
	if len(sessions) != 2 {
		t.Errorf("len(sessions) = %d", len(sessions))
	}
}

func TestSessionManager_ListActive(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data: map[string]interface{}{
					"sessions": []interface{}{
						map[string]interface{}{"session_id": "s1", "status": "active"},
					},
				},
			}, nil
		},
	}
	sm := NewSessionManager(mock)
	sessions, err := sm.ListActive(context.Background())
	if err != nil {
		t.Fatalf("ListActive error = %v", err)
	}
	if len(sessions) != 1 {
		t.Errorf("len(sessions) = %d", len(sessions))
	}
}

func TestSessionManager_Update(t *testing.T) {
	mock := &client.MockAPIClient{
		PutFn: func(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data:    map[string]interface{}{"session_id": "s1", "metadata": map[string]interface{}{"updated": true}},
			}, nil
		},
	}
	sm := NewSessionManager(mock)
	sess, err := sm.Update(context.Background(), "s1", map[string]interface{}{"updated": true})
	if err != nil {
		t.Fatalf("Update error = %v", err)
	}
	if sess.ID != "s1" {
		t.Errorf("sess.ID = %q", sess.ID)
	}
}

func TestSessionManager_Update_EmptyID(t *testing.T) {
	sm := NewSessionManager(nil)
	_, err := sm.Update(context.Background(), "", nil)
	if err == nil {
		t.Error("空 ID 应返回错误")
	}
}

func TestSessionManager_Refresh(t *testing.T) {
	mock := &client.MockAPIClient{
		PostFn: func(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{Success: true, Data: nil}, nil
		},
	}
	sm := NewSessionManager(mock)
	err := sm.Refresh(context.Background(), "s1")
	if err != nil {
		t.Fatalf("Refresh error = %v", err)
	}
}

func TestSessionManager_Refresh_EmptyID(t *testing.T) {
	sm := NewSessionManager(nil)
	err := sm.Refresh(context.Background(), "")
	if err == nil {
		t.Error("空 ID 应返回错误")
	}
}

func TestSessionManager_IsExpired(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data:    map[string]interface{}{"session_id": "s1", "status": "expired"},
			}, nil
		},
	}
	sm := NewSessionManager(mock)
	expired, err := sm.IsExpired(context.Background(), "s1")
	if err != nil {
		t.Fatalf("IsExpired error = %v", err)
	}
	if !expired {
		t.Error("应返回 expired = true")
	}
}

func TestSessionManager_IsExpired_NotExpired(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data:    map[string]interface{}{"session_id": "s1", "status": "active"},
			}, nil
		},
	}
	sm := NewSessionManager(mock)
	expired, err := sm.IsExpired(context.Background(), "s1")
	if err != nil {
		t.Fatalf("IsExpired error = %v", err)
	}
	if expired {
		t.Error("活跃会话不应返回 expired")
	}
}

func TestSessionManager_Count(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{Success: true, Data: map[string]interface{}{"count": float64(15)}}, nil
		},
	}
	sm := NewSessionManager(mock)
	count, err := sm.Count(context.Background())
	if err != nil {
		t.Fatalf("Count error = %v", err)
	}
	if count != 15 {
		t.Errorf("count = %d", count)
	}
}

func TestSessionManager_CountActive(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{Success: true, Data: map[string]interface{}{"count": float64(8)}}, nil
		},
	}
	sm := NewSessionManager(mock)
	count, err := sm.CountActive(context.Background())
	if err != nil {
		t.Fatalf("CountActive error = %v", err)
	}
	if count != 8 {
		t.Errorf("count = %d", count)
	}
}

func TestSessionManager_CleanExpired(t *testing.T) {
	mock := &client.MockAPIClient{
		PostFn: func(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{Success: true, Data: map[string]interface{}{"cleaned": float64(3)}}, nil
		},
	}
	sm := NewSessionManager(mock)
	cleaned, err := sm.CleanExpired(context.Background())
	if err != nil {
		t.Fatalf("CleanExpired error = %v", err)
	}
	if cleaned != 3 {
		t.Errorf("cleaned = %d", cleaned)
	}
}

func TestParseSessionFromMap(t *testing.T) {
	data := map[string]interface{}{
		"session_id": "s1", "user_id": "u1", "status": "active",
		"context":  map[string]interface{}{"k": "v"},
		"metadata": map[string]interface{}{"env": "prod"},
	}
	sess := parseSessionFromMap(data)
	if sess.ID != "s1" || sess.UserID != "u1" || sess.Status != types.SessionStatusActive {
		t.Errorf("parseSessionFromMap = %+v", sess)
	}
}

func TestParseSessionList_InvalidData(t *testing.T) {
	_, err := parseSessionList(&types.APIResponse{Success: true, Data: "not a map"})
	if err == nil {
		t.Error("无效数据应返回错误")
	}
}

func TestParseSessionList_Empty(t *testing.T) {
	sessions, err := parseSessionList(&types.APIResponse{
		Success: true,
		Data:    map[string]interface{}{"sessions": []interface{}{}},
	})
	if err != nil {
		t.Fatalf("error = %v", err)
	}
	if len(sessions) != 0 {
		t.Errorf("len(sessions) = %d", len(sessions))
	}
}
