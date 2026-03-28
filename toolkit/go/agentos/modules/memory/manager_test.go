// AgentOS Go SDK - 记忆管理模块单元测试
// Version: 3.0.0

package memory

import (
	"context"
	"errors"
	"testing"

	"github.com/spharx/agentos/toolkit/go/agentos"
	"github.com/spharx/agentos/toolkit/go/agentos/client"
	"github.com/spharx/agentos/toolkit/go/agentos/types"
)

func TestNewMemoryManager(t *testing.T) {
	mm := NewMemoryManager(nil)
	if mm == nil {
		t.Fatal("MemoryManager 不应为 nil")
	}
}

func TestMemoryManager_Write_Success(t *testing.T) {
	mock := &client.MockAPIClient{
		PostFn: func(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{Success: true, Data: map[string]interface{}{"memory_id": "m1"}}, nil
		},
	}
	mm := NewMemoryManager(mock)
	mem, err := mm.Write(context.Background(), "测试记忆", types.MemoryLayerL1)
	if err != nil {
		t.Fatalf("Write error = %v", err)
	}
	if mem.ID != "m1" {
		t.Errorf("mem.ID = %q", mem.ID)
	}
	if mem.Content != "测试记忆" {
		t.Errorf("mem.Content = %q", mem.Content)
	}
}

func TestMemoryManager_Write_Empty(t *testing.T) {
	mm := NewMemoryManager(nil)
	_, err := mm.Write(context.Background(), "", types.MemoryLayerL1)
	if !agentos.IsErrorCode(err, agentos.CodeMissingParameter) {
		t.Errorf("期望 CodeMissingParameter, got %v", err)
	}
}

func TestMemoryManager_WriteWithOptions(t *testing.T) {
	mock := &client.MockAPIClient{
		PostFn: func(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{Success: true, Data: map[string]interface{}{"memory_id": "m2"}}, nil
		},
	}
	mm := NewMemoryManager(mock)
	mem, err := mm.WriteWithOptions(context.Background(), "带元数据记忆", types.MemoryLayerL2, map[string]interface{}{"source": "test"})
	if err != nil {
		t.Fatalf("WriteWithOptions error = %v", err)
	}
	if mem.Layer != types.MemoryLayerL2 {
		t.Errorf("layer = %q", mem.Layer)
	}
}

func TestMemoryManager_Write_APIError(t *testing.T) {
	mock := &client.MockAPIClient{
		PostFn: func(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
			return nil, errors.New("network error")
		},
	}
	mm := NewMemoryManager(mock)
	_, err := mm.Write(context.Background(), "test", types.MemoryLayerL1)
	if err == nil {
		t.Error("API 错误应传播")
	}
}

func TestMemoryManager_Get(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data:    map[string]interface{}{"memory_id": "m1", "content": "hello", "layer": "L1", "score": float64(0.95)},
			}, nil
		},
	}
	mm := NewMemoryManager(mock)
	mem, err := mm.Get(context.Background(), "m1")
	if err != nil {
		t.Fatalf("Get error = %v", err)
	}
	if mem.Content != "hello" {
		t.Errorf("content = %q", mem.Content)
	}
}

func TestMemoryManager_Get_EmptyID(t *testing.T) {
	mm := NewMemoryManager(nil)
	_, err := mm.Get(context.Background(), "")
	if err == nil {
		t.Error("空 ID 应返回错误")
	}
}

func TestMemoryManager_Search(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data: map[string]interface{}{
					"memories": []interface{}{
						map[string]interface{}{"memory_id": "m1", "content": "test", "layer": "L1"},
					},
					"total": float64(1),
				},
			}, nil
		},
	}
	mm := NewMemoryManager(mock)
	result, err := mm.Search(context.Background(), "test query", 5)
	if err != nil {
		t.Fatalf("Search error = %v", err)
	}
	if result.Total != 1 {
		t.Errorf("total = %d", result.Total)
	}
	if result.Query != "test query" {
		t.Errorf("query = %q", result.Query)
	}
	if result.TopK != 5 {
		t.Errorf("topK = %d", result.TopK)
	}
}

func TestMemoryManager_Search_EmptyQuery(t *testing.T) {
	mm := NewMemoryManager(nil)
	_, err := mm.Search(context.Background(), "", 10)
	if !agentos.IsErrorCode(err, agentos.CodeMissingParameter) {
		t.Errorf("期望 CodeMissingParameter, got %v", err)
	}
}

func TestMemoryManager_Search_DefaultTopK(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data:    map[string]interface{}{"memories": []interface{}{}, "total": float64(0)},
			}, nil
		},
	}
	mm := NewMemoryManager(mock)
	result, err := mm.Search(context.Background(), "test", 0)
	if err != nil {
		t.Fatalf("Search error = %v", err)
	}
	if result.TopK != 10 {
		t.Errorf("默认 topK 应为 10, got %d", result.TopK)
	}
}

func TestMemoryManager_SearchByLayer(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data: map[string]interface{}{
					"memories": []interface{}{
						map[string]interface{}{"memory_id": "m1", "content": "test", "layer": "L2"},
					},
					"total": float64(1),
				},
			}, nil
		},
	}
	mm := NewMemoryManager(mock)
	result, err := mm.SearchByLayer(context.Background(), "test", types.MemoryLayerL2, 5)
	if err != nil {
		t.Fatalf("SearchByLayer error = %v", err)
	}
	if len(result.Memories) != 1 {
		t.Errorf("len(memories) = %d", len(result.Memories))
	}
}

func TestMemoryManager_Update(t *testing.T) {
	mock := &client.MockAPIClient{
		PutFn: func(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data:    map[string]interface{}{"memory_id": "m1", "content": "updated"},
			}, nil
		},
	}
	mm := NewMemoryManager(mock)
	mem, err := mm.Update(context.Background(), "m1", "updated content")
	if err != nil {
		t.Fatalf("Update error = %v", err)
	}
	if mem.ID != "m1" {
		t.Errorf("mem.ID = %q", mem.ID)
	}
}

func TestMemoryManager_Update_EmptyID(t *testing.T) {
	mm := NewMemoryManager(nil)
	_, err := mm.Update(context.Background(), "", "content")
	if err == nil {
		t.Error("空 ID 应返回错误")
	}
}

func TestMemoryManager_Delete(t *testing.T) {
	mock := &client.MockAPIClient{
		DelFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{Success: true, Data: nil}, nil
		},
	}
	mm := NewMemoryManager(mock)
	err := mm.Delete(context.Background(), "m1")
	if err != nil {
		t.Fatalf("Delete error = %v", err)
	}
}

func TestMemoryManager_Delete_EmptyID(t *testing.T) {
	mm := NewMemoryManager(nil)
	err := mm.Delete(context.Background(), "")
	if err == nil {
		t.Error("空 ID 应返回错误")
	}
}

func TestMemoryManager_List(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data: map[string]interface{}{
					"memories": []interface{}{
						map[string]interface{}{"memory_id": "m1", "content": "a"},
						map[string]interface{}{"memory_id": "m2", "content": "b"},
					},
				},
			}, nil
		},
	}
	mm := NewMemoryManager(mock)
	memories, err := mm.List(context.Background(), nil)
	if err != nil {
		t.Fatalf("List error = %v", err)
	}
	if len(memories) != 2 {
		t.Errorf("len(memories) = %d", len(memories))
	}
}

func TestMemoryManager_ListByLayer(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data: map[string]interface{}{
					"memories": []interface{}{
						map[string]interface{}{"memory_id": "m1", "layer": "L3"},
					},
				},
			}, nil
		},
	}
	mm := NewMemoryManager(mock)
	memories, err := mm.ListByLayer(context.Background(), types.MemoryLayerL3, nil)
	if err != nil {
		t.Fatalf("ListByLayer error = %v", err)
	}
	if len(memories) != 1 {
		t.Errorf("len(memories) = %d", len(memories))
	}
}

func TestMemoryManager_Count(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{Success: true, Data: map[string]interface{}{"count": float64(42)}}, nil
		},
	}
	mm := NewMemoryManager(mock)
	count, err := mm.Count(context.Background())
	if err != nil {
		t.Fatalf("Count error = %v", err)
	}
	if count != 42 {
		t.Errorf("count = %d", count)
	}
}

func TestMemoryManager_Clear(t *testing.T) {
	mock := &client.MockAPIClient{
		DelFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{Success: true, Data: nil}, nil
		},
	}
	mm := NewMemoryManager(mock)
	err := mm.Clear(context.Background())
	if err != nil {
		t.Fatalf("Clear error = %v", err)
	}
}

func TestMemoryManager_BatchWrite(t *testing.T) {
	mock := &client.MockAPIClient{
		PostFn: func(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{Success: true, Data: map[string]interface{}{"memory_id": "mx"}}, nil
		},
	}
	mm := NewMemoryManager(mock)
	items := []MemoryWriteItem{
		{Content: "记忆1", Layer: types.MemoryLayerL1},
		{Content: "记忆2", Layer: types.MemoryLayerL2},
	}
	memories, err := mm.BatchWrite(context.Background(), items)
	if err != nil {
		t.Fatalf("BatchWrite error = %v", err)
	}
	if len(memories) != 2 {
		t.Errorf("len(memories) = %d", len(memories))
	}
}

func TestMemoryManager_BatchWrite_PartialFailure(t *testing.T) {
	callCount := 0
	mock := &client.MockAPIClient{
		PostFn: func(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
			callCount++
			if callCount > 1 {
				return nil, errors.New("write error")
			}
			return &types.APIResponse{Success: true, Data: map[string]interface{}{"memory_id": "mx"}}, nil
		},
	}
	mm := NewMemoryManager(mock)
	items := []MemoryWriteItem{
		{Content: "ok", Layer: types.MemoryLayerL1},
		{Content: "fail", Layer: types.MemoryLayerL2},
	}
	memories, err := mm.BatchWrite(context.Background(), items)
	if err == nil {
		t.Error("部分失败应返回错误")
	}
	if len(memories) != 1 {
		t.Errorf("应返回 1 条, got %d", len(memories))
	}
}

func TestMemoryManager_Evolve(t *testing.T) {
	mock := &client.MockAPIClient{
		PostFn: func(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{Success: true, Data: nil}, nil
		},
	}
	mm := NewMemoryManager(mock)
	err := mm.Evolve(context.Background())
	if err != nil {
		t.Fatalf("Evolve error = %v", err)
	}
}

func TestMemoryManager_GetStats(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{
				Success: true,
				Data:    map[string]interface{}{"L1": float64(10), "L2": float64(20), "L3": int(5)},
			}, nil
		},
	}
	mm := NewMemoryManager(mock)
	stats, err := mm.GetStats(context.Background())
	if err != nil {
		t.Fatalf("GetStats error = %v", err)
	}
	if stats["L1"] != 10 || stats["L2"] != 20 || stats["L3"] != 5 {
		t.Errorf("stats = %v", stats)
	}
}

func TestMemoryManager_GetStats_EmptyData(t *testing.T) {
	mock := &client.MockAPIClient{
		GetFn: func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
			return &types.APIResponse{Success: true, Data: nil}, nil
		},
	}
	mm := NewMemoryManager(mock)
	stats, err := mm.GetStats(context.Background())
	if err != nil {
		t.Fatalf("GetStats error = %v", err)
	}
	if len(stats) != 0 {
		t.Errorf("空数据应返回空 map, got %v", stats)
	}
}

func TestParseMemoryFromMap(t *testing.T) {
	data := map[string]interface{}{
		"memory_id": "m1", "content": "hello", "layer": "L2",
		"score": float64(0.88), "metadata": map[string]interface{}{"key": "val"},
	}
	mem := parseMemoryFromMap(data)
	if mem.ID != "m1" || mem.Layer != types.MemoryLayerL2 || mem.Score != 0.88 {
		t.Errorf("parseMemoryFromMap = %+v", mem)
	}
}

func TestParseMemoryList_InvalidData(t *testing.T) {
	_, err := parseMemoryList(&types.APIResponse{Success: true, Data: "not a map"})
	if err == nil {
		t.Error("无效数据应返回错误")
	}
}

func TestParseMemoryList_Empty(t *testing.T) {
	memories, err := parseMemoryList(&types.APIResponse{
		Success: true,
		Data:    map[string]interface{}{"memories": []interface{}{}},
	})
	if err != nil {
		t.Fatalf("error = %v", err)
	}
	if len(memories) != 0 {
		t.Errorf("len(memories) = %d", len(memories))
	}
}

