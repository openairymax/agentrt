// AgentOS Go SDK - 技能管理模块单元测试
// Version: 3.0.0

package skill

import (
	"context"
	"errors"
	"testing"

	"github.com/spharx/agentos/tools/go/agentos"
	"github.com/spharx/agentos/tools/go/agentos/client"
	"github.com/spharx/agentos/tools/go/agentos/types"
)

func TestNewSkillManager(t *testing.T) {
	sm := NewSkillManager(nil)
	if sm == nil {
		t.Fatal("SkillManager 不应为 nil")
	}
}

func TestSkillManager_Load(t *testing.T) {
	mock := &client.MockAPIClient{
		PostFn: func(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data:    map[string]interface{}{"name": "test-skill", "version": "1.0"},
			}, nil
		},
	}
	sm := NewSkillManager(mock)
	sk, err := sm.Load(context.Background(), "skill1")
	if err != nil {
		t.Fatalf("Load error = %v", err)
	}
	if sk.ID != "skill1" {
		t.Errorf("sk.ID = %q", sk.ID)
	}
	if sk.Status != types.SkillStatusActive {
		t.Errorf("status = %q", sk.Status)
	}
}

func TestSkillManager_Load_EmptyID(t *testing.T) {
	sm := NewSkillManager(nil)
	_, err := sm.Load(context.Background(), "")
	if !agentos.IsErrorCode(err, agentos.CodeMissingParameter) {
		t.Errorf("期望 CodeMissingParameter, got %v", err)
	}
}

func TestSkillManager_Get(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data:    map[string]interface{}{"skill_id": "sk1", "name": "test", "status": "active"},
			}, nil
		},
	}
	sm := NewSkillManager(mock)
	sk, err := sm.Get(context.Background(), "sk1")
	if err != nil {
		t.Fatalf("Get error = %v", err)
	}
	if sk.ID != "sk1" {
		t.Errorf("sk.ID = %q", sk.ID)
	}
}

func TestSkillManager_Get_EmptyID(t *testing.T) {
	sm := NewSkillManager(nil)
	_, err := sm.Get(context.Background(), "")
	if err == nil {
		t.Error("空 ID 应返回错误")
	}
}

func TestSkillManager_Execute(t *testing.T) {
	mock := &client.MockAPIClient{
		PostFn: func(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data:    map[string]interface{}{"success": true, "output": "executed", "error": ""},
			}, nil
		},
	}
	sm := NewSkillManager(mock)
	result, err := sm.Execute(context.Background(), "sk1", map[string]interface{}{"key": "val"})
	if err != nil {
		t.Fatalf("Execute error = %v", err)
	}
	if !result.Success {
		t.Error("应返回 success = true")
	}
	if result.Output != "executed" {
		t.Errorf("output = %v", result.Output)
	}
}

func TestSkillManager_Execute_EmptyID(t *testing.T) {
	sm := NewSkillManager(nil)
	_, err := sm.Execute(context.Background(), "", nil)
	if !agentos.IsErrorCode(err, agentos.CodeMissingParameter) {
		t.Errorf("期望 CodeMissingParameter, got %v", err)
	}
}

func TestSkillManager_ExecuteWithContext(t *testing.T) {
	mock := &client.MockAPIClient{
		PostFn: func(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data:    map[string]interface{}{"success": true, "output": "done"},
			}, nil
		},
	}
	sm := NewSkillManager(mock)
	result, err := sm.ExecuteWithContext(context.Background(), "sk1", nil, "session1")
	if err != nil {
		t.Fatalf("ExecuteWithContext error = %v", err)
	}
	if !result.Success {
		t.Error("应返回 success")
	}
}

func TestSkillManager_Unload(t *testing.T) {
	mock := &client.MockAPIClient{
		PostFn: func(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{Success: true, Data: nil}, nil
		},
	}
	sm := NewSkillManager(mock)
	err := sm.Unload(context.Background(), "sk1")
	if err != nil {
		t.Fatalf("Unload error = %v", err)
	}
}

func TestSkillManager_Unload_EmptyID(t *testing.T) {
	sm := NewSkillManager(nil)
	err := sm.Unload(context.Background(), "")
	if err == nil {
		t.Error("空 ID 应返回错误")
	}
}

func TestSkillManager_List(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data: map[string]interface{}{
					"skills": []interface{}{
						map[string]interface{}{"skill_id": "sk1", "name": "a"},
						map[string]interface{}{"skill_id": "sk2", "name": "b"},
					},
				},
			}, nil
		},
	}
	sm := NewSkillManager(mock)
	skills, err := sm.List(context.Background(), nil)
	if err != nil {
		t.Fatalf("List error = %v", err)
	}
	if len(skills) != 2 {
		t.Errorf("len(skills) = %d", len(skills))
	}
}

func TestSkillManager_ListLoaded(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data: map[string]interface{}{
					"skills": []interface{}{
						map[string]interface{}{"skill_id": "sk1", "status": "loaded"},
					},
				},
			}, nil
		},
	}
	sm := NewSkillManager(mock)
	skills, err := sm.ListLoaded(context.Background())
	if err != nil {
		t.Fatalf("ListLoaded error = %v", err)
	}
	if len(skills) != 1 {
		t.Errorf("len(skills) = %d", len(skills))
	}
}

func TestSkillManager_Register(t *testing.T) {
	mock := &client.MockAPIClient{
		PostFn: func(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data:    map[string]interface{}{"skill_id": "new-sk1"},
			}, nil
		},
	}
	sm := NewSkillManager(mock)
	sk, err := sm.Register(context.Background(), "new-skill", "测试技能", map[string]interface{}{"p1": "v1"})
	if err != nil {
		t.Fatalf("Register error = %v", err)
	}
	if sk.Name != "new-skill" {
		t.Errorf("name = %q", sk.Name)
	}
}

func TestSkillManager_Register_EmptyName(t *testing.T) {
	sm := NewSkillManager(nil)
	_, err := sm.Register(context.Background(), "", "desc", nil)
	if !agentos.IsErrorCode(err, agentos.CodeMissingParameter) {
		t.Errorf("期望 CodeMissingParameter, got %v", err)
	}
}

func TestSkillManager_Update(t *testing.T) {
	mock := &client.MockAPIClient{
		PutFn: func(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data:    map[string]interface{}{"skill_id": "sk1", "description": "updated"},
			}, nil
		},
	}
	sm := NewSkillManager(mock)
	sk, err := sm.Update(context.Background(), "sk1", "updated desc", nil)
	if err != nil {
		t.Fatalf("Update error = %v", err)
	}
	if sk.ID != "sk1" {
		t.Errorf("sk.ID = %q", sk.ID)
	}
}

func TestSkillManager_Delete(t *testing.T) {
	mock := &client.MockAPIClient{
		DelFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{Success: true, Data: nil}, nil
		},
	}
	sm := NewSkillManager(mock)
	err := sm.Delete(context.Background(), "sk1")
	if err != nil {
		t.Fatalf("Delete error = %v", err)
	}
}

func TestSkillManager_Delete_EmptyID(t *testing.T) {
	sm := NewSkillManager(nil)
	err := sm.Delete(context.Background(), "")
	if err == nil {
		t.Error("空 ID 应返回错误")
	}
}

func TestSkillManager_GetInfo(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data:    map[string]interface{}{"skill_name": "info-skill", "description": "desc", "version": "2.0"},
			}, nil
		},
	}
	sm := NewSkillManager(mock)
	info, err := sm.GetInfo(context.Background(), "sk1")
	if err != nil {
		t.Fatalf("GetInfo error = %v", err)
	}
	if info.Name != "info-skill" || info.Version != "2.0" {
		t.Errorf("GetInfo = %+v", info)
	}
}

func TestSkillManager_GetInfo_EmptyID(t *testing.T) {
	sm := NewSkillManager(nil)
	_, err := sm.GetInfo(context.Background(), "")
	if err == nil {
		t.Error("空 ID 应返回错误")
	}
}

func TestSkillManager_Validate(t *testing.T) {
	mock := &client.MockAPIClient{
		PostFn: func(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data:    map[string]interface{}{"valid": true, "errors": []interface{}{}},
			}, nil
		},
	}
	sm := NewSkillManager(mock)
	valid, errs, err := sm.Validate(context.Background(), "sk1", map[string]interface{}{"p": "v"})
	if err != nil {
		t.Fatalf("Validate error = %v", err)
	}
	if !valid {
		t.Error("应返回 valid = true")
	}
	if len(errs) != 0 {
		t.Errorf("errors = %v", errs)
	}
}

func TestSkillManager_Validate_Invalid(t *testing.T) {
	mock := &client.MockAPIClient{
		PostFn: func(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data:    map[string]interface{}{"valid": false, "errors": []interface{}{"param missing"}},
			}, nil
		},
	}
	sm := NewSkillManager(mock)
	valid, errs, err := sm.Validate(context.Background(), "sk1", nil)
	if err != nil {
		t.Fatalf("Validate error = %v", err)
	}
	if valid {
		t.Error("不应返回 valid")
	}
	if len(errs) != 1 {
		t.Errorf("len(errs) = %d", len(errs))
	}
}

func TestSkillManager_Count(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{Success: true, Data: map[string]interface{}{"count": float64(25)}}, nil
		},
	}
	sm := NewSkillManager(mock)
	count, err := sm.Count(context.Background())
	if err != nil {
		t.Fatalf("Count error = %v", err)
	}
	if count != 25 {
		t.Errorf("count = %d", count)
	}
}

func TestSkillManager_CountLoaded(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{Success: true, Data: map[string]interface{}{"count": float64(10)}}, nil
		},
	}
	sm := NewSkillManager(mock)
	count, err := sm.CountLoaded(context.Background())
	if err != nil {
		t.Fatalf("CountLoaded error = %v", err)
	}
	if count != 10 {
		t.Errorf("count = %d", count)
	}
}

func TestSkillManager_Search(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data: map[string]interface{}{
					"skills": []interface{}{
						map[string]interface{}{"skill_id": "sk1", "name": "search-result"},
					},
				},
			}, nil
		},
	}
	sm := NewSkillManager(mock)
	skills, err := sm.Search(context.Background(), "test", 5)
	if err != nil {
		t.Fatalf("Search error = %v", err)
	}
	if len(skills) != 1 {
		t.Errorf("len(skills) = %d", len(skills))
	}
}

func TestSkillManager_Search_EmptyQuery(t *testing.T) {
	sm := NewSkillManager(nil)
	_, err := sm.Search(context.Background(), "", 10)
	if !agentos.IsErrorCode(err, agentos.CodeMissingParameter) {
		t.Errorf("期望 CodeMissingParameter, got %v", err)
	}
}

func TestSkillManager_BatchExecute(t *testing.T) {
	mock := &client.MockAPIClient{
		PostFn: func(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data:    map[string]interface{}{"success": true, "output": "done"},
			}, nil
		},
	}
	sm := NewSkillManager(mock)
	requests := []SkillExecuteRequest{
		{SkillID: "sk1", Parameters: nil},
		{SkillID: "sk2", Parameters: map[string]interface{}{"k": "v"}},
	}
	results, err := sm.BatchExecute(context.Background(), requests)
	if err != nil {
		t.Fatalf("BatchExecute error = %v", err)
	}
	if len(results) != 2 {
		t.Errorf("len(results) = %d", len(results))
	}
}

func TestSkillManager_BatchExecute_PartialFailure(t *testing.T) {
	callCount := 0
	mock := &client.MockAPIClient{
		PostFn: func(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
			callCount++
			if callCount > 1 {
				return nil, errors.New("exec error")
			}
			return &types.APIResponse{
				Success: true,
				Data:    map[string]interface{}{"success": true, "output": "ok"},
			}, nil
		},
	}
	sm := NewSkillManager(mock)
	requests := []SkillExecuteRequest{
		{SkillID: "sk1"},
		{SkillID: "sk2"},
	}
	results, err := sm.BatchExecute(context.Background(), requests)
	if err == nil {
		t.Error("部分失败应返回错误")
	}
	if len(results) != 1 {
		t.Errorf("应返回 1 条, got %d", len(results))
	}
}

func TestSkillManager_GetStats(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data:    map[string]interface{}{"executions": float64(100), "failures": int(5)},
			}, nil
		},
	}
	sm := NewSkillManager(mock)
	stats, err := sm.GetStats(context.Background(), "sk1")
	if err != nil {
		t.Fatalf("GetStats error = %v", err)
	}
	if stats["executions"] != 100 || stats["failures"] != 5 {
		t.Errorf("stats = %v", stats)
	}
}

func TestSkillManager_GetStats_EmptyID(t *testing.T) {
	sm := NewSkillManager(nil)
	_, err := sm.GetStats(context.Background(), "")
	if err == nil {
		t.Error("空 ID 应返回错误")
	}
}

func TestParseSkillFromMap(t *testing.T) {
	data := map[string]interface{}{
		"skill_id": "sk1", "name": "test", "version": "1.0",
		"description": "desc", "status": "active",
		"parameters": map[string]interface{}{"p": "v"},
	}
	sk := parseSkillFromMap(data, "")
	if sk.ID != "sk1" || sk.Name != "test" || sk.Version != "1.0" {
		t.Errorf("parseSkillFromMap = %+v", sk)
	}
}

func TestParseSkillFromMap_FallbackID(t *testing.T) {
	data := map[string]interface{}{"name": "test"}
	sk := parseSkillFromMap(data, "fallback-id")
	if sk.ID != "fallback-id" {
		t.Errorf("应使用 fallback ID, got %q", sk.ID)
	}
}

func TestParseSkillList_InvalidData(t *testing.T) {
	_, err := parseSkillList(&types.APIResponse{Success: true, Data: "not a map"})
	if err == nil {
		t.Error("无效数据应返回错误")
	}
}

func TestParseSkillResult(t *testing.T) {
	resp := &types.APIResponse{
		Success: true,
		Data:    map[string]interface{}{"success": true, "output": "result-output", "error": ""},
	}
	result, err := parseSkillResult(resp)
	if err != nil {
		t.Fatalf("parseSkillResult error = %v", err)
	}
	if !result.Success || result.Output != "result-output" {
		t.Errorf("parseSkillResult = %+v", result)
	}
}

func TestParseSkillResult_InvalidData(t *testing.T) {
	_, err := parseSkillResult(&types.APIResponse{Success: true, Data: "bad"})
	if err == nil {
		t.Error("无效数据应返回错误")
	}
}
