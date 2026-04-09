// AgentOS Go SDK - 记忆管理模块
// Version: 3.0.0
// Last updated: 2026-03-22
//
// 提供记忆的写入、搜索、更新、删除及统计功能。
// 对应 Python SDK: modules/memory/__init__.py + memory.py

package memory

import (
	"context"
	"fmt"
	"time"

	"github.com/spharx/agentos/toolkit/go/agentos"
	"github.com/spharx/agentos/toolkit/go/agentos/client"
	"github.com/spharx/agentos/toolkit/go/agentos/types"
	"github.com/spharx/agentos/toolkit/go/agentos/utils"
)

// MemoryWriteItem 批量写入时的单条记忆项
type MemoryWriteItem struct {
	Content  string
	Layer    types.MemoryLayer
	Metadata map[string]interface{}
}

// MemoryManager 管理记忆完整生命周期
type MemoryManager struct {
	api client.APIClient
}

// NewMemoryManager 创建新的记忆管理器实例
func NewMemoryManager(api client.APIClient) *MemoryManager {
	return &MemoryManager{api: api}
}

// Write 写入一条新记忆到指定层级
func (mm *MemoryManager) Write(ctx context.Context, content string, layer types.MemoryLayer) (*types.Memory, error) {
	return mm.WriteWithOptions(ctx, content, layer, nil)
}

// WriteWithOptions 使用元数据选项写入新记忆
func (mm *MemoryManager) WriteWithOptions(ctx context.Context, content string, layer types.MemoryLayer, metadata map[string]interface{}) (*types.Memory, error) {
	if content == "" {
		return nil, agentos.NewError(agentos.CodeMissingParameter, "记忆内容不能为空", nil)
	}

	body := map[string]interface{}{
		"content": content,
		"layer":   layer,
	}
	if metadata != nil {
		body["metadata"] = metadata
	}

	resp, err := mm.api.Post(ctx, "/api/v1/memories", body)
	if err != nil {
		return nil, err
	}

	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return nil, agentos.NewError(agentos.CodeInvalidResponse, "记忆写入响应格式异常", nil)
	}

	return &types.Memory{
		ID:        utils.GetString(data, "memory_id"),
		Content:   content,
		Layer:     layer,
		Score:     1.0,
		Metadata:  metadata,
		CreatedAt: time.Now(),
		UpdatedAt: time.Now(),
	}, nil
}

// Get 获取指定记忆的详细信息
func (mm *MemoryManager) Get(ctx context.Context, memoryID string) (*types.Memory, error) {
	if memoryID == "" {
		return nil, agentos.NewError(agentos.CodeMissingParameter, "记忆ID不能为空", nil)
	}

	resp, err := mm.api.Get(ctx, fmt.Sprintf("/api/v1/memories/%s", memoryID))
	if err != nil {
		return nil, err
	}

	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return nil, agentos.NewError(agentos.CodeInvalidResponse, "记忆详情响应格式异常", nil)
	}

	return parseMemoryFromMap(data), nil
}

// Search 搜索记忆，返回按相关度排序的结果
func (mm *MemoryManager) Search(ctx context.Context, query string, topK int) (*types.MemorySearchResult, error) {
	if query == "" {
		return nil, agentos.NewError(agentos.CodeMissingParameter, "搜索查询不能为空", nil)
	}
	if topK <= 0 {
		topK = 10
	}

	path := utils.BuildURL("/api/v1/memories/search", map[string]string{
		"query": query,
		"top_k": fmt.Sprintf("%d", topK),
	})

	resp, err := mm.api.Get(ctx, path)
	if err != nil {
		return nil, err
	}

	return parseMemorySearchResult(resp, query, topK)
}

// SearchByLayer 在指定层级内搜索记忆
func (mm *MemoryManager) SearchByLayer(ctx context.Context, query string, layer types.MemoryLayer, topK int) (*types.MemorySearchResult, error) {
	if query == "" {
		return nil, agentos.NewError(agentos.CodeMissingParameter, "搜索查询不能为空", nil)
	}
	if topK <= 0 {
		topK = 10
	}

	path := utils.BuildURL("/api/v1/memories/search", map[string]string{
		"query": query,
		"layer": layer.String(),
		"top_k": fmt.Sprintf("%d", topK),
	})

	resp, err := mm.api.Get(ctx, path)
	if err != nil {
		return nil, err
	}

	return parseMemorySearchResult(resp, query, topK)
}

// Update 更新指定记忆的内容
func (mm *MemoryManager) Update(ctx context.Context, memoryID string, content string) (*types.Memory, error) {
	if memoryID == "" {
		return nil, agentos.NewError(agentos.CodeMissingParameter, "记忆ID不能为空", nil)
	}

	resp, err := mm.api.Put(ctx, fmt.Sprintf("/api/v1/memories/%s", memoryID), map[string]interface{}{
		"content": content,
	})
	if err != nil {
		return nil, err
	}

	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return nil, agentos.NewError(agentos.CodeInvalidResponse, "记忆更新响应格式异常", nil)
	}

	return parseMemoryFromMap(data), nil
}

// Delete 删除指定记忆
func (mm *MemoryManager) Delete(ctx context.Context, memoryID string) error {
	if memoryID == "" {
		return agentos.NewError(agentos.CodeMissingParameter, "记忆ID不能为空", nil)
	}
	_, err := mm.api.Delete(ctx, fmt.Sprintf("/api/v1/memories/%s", memoryID))
	return err
}

// List 列出记忆，支持分页和过滤
func (mm *MemoryManager) List(ctx context.Context, opts *types.ListOptions) ([]types.Memory, error) {
	path := "/api/v1/memories"
	if opts != nil {
		path = utils.BuildURL(path, opts.ToQueryParams())
	}

	resp, err := mm.api.Get(ctx, path)
	if err != nil {
		return nil, err
	}

	return parseMemoryList(resp)
}

// ListByLayer 按层级列出记忆
func (mm *MemoryManager) ListByLayer(ctx context.Context, layer types.MemoryLayer, opts *types.ListOptions) ([]types.Memory, error) {
	params := map[string]string{"layer": layer.String()}
	if opts != nil {
		for k, v := range opts.ToQueryParams() {
			params[k] = v
		}
	}

	resp, err := mm.api.Get(ctx, utils.BuildURL("/api/v1/memories", params))
	if err != nil {
		return nil, err
	}

	return parseMemoryList(resp)
}

// Count 获取记忆总数
func (mm *MemoryManager) Count(ctx context.Context) (int64, error) {
	resp, err := mm.api.Get(ctx, "/api/v1/memories/count")
	if err != nil {
		return 0, err
	}
	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return 0, nil
	}
	return utils.GetInt64(data, "count"), nil
}

// Clear 清空所有记忆数据
func (mm *MemoryManager) Clear(ctx context.Context) error {
	_, err := mm.api.Delete(ctx, "/api/v1/memories")
	return err
}

// BatchWrite 批量写入多条记忆
func (mm *MemoryManager) BatchWrite(ctx context.Context, memories []MemoryWriteItem) ([]types.Memory, error) {
	results := make([]types.Memory, 0, len(memories))
	for _, m := range memories {
		mem, err := mm.WriteWithOptions(ctx, m.Content, m.Layer, m.Metadata)
		if err != nil {
			return results, err
		}
		results = append(results, *mem)
	}
	return results, nil
}

// Evolve 触发记忆演化过程（L1→L2→L3→L4 层级升华）
func (mm *MemoryManager) Evolve(ctx context.Context) error {
	_, err := mm.api.Post(ctx, "/api/v1/memories/evolve", nil)
	return err
}

// GetStats 获取各层级的记忆统计数据
func (mm *MemoryManager) GetStats(ctx context.Context) (map[string]int64, error) {
	resp, err := mm.api.Get(ctx, "/api/v1/memories/stats")
	if err != nil {
		return nil, err
	}
	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return make(map[string]int64), nil
	}
	return utils.ExtractInt64Stats(data), nil
}

// parseMemoryFromMap 从 map 解析 Memory 结构
func parseMemoryFromMap(data map[string]interface{}) *types.Memory {
	return &types.Memory{
		ID:        utils.GetString(data, "memory_id"),
		Content:   utils.GetString(data, "content"),
		Layer:     types.MemoryLayer(utils.GetString(data, "layer")),
		Score:     utils.GetFloat64(data, "score"),
		Metadata:  utils.GetMap(data, "metadata"),
		CreatedAt: utils.ParseTimeFromMap(data, "created_at"),
		UpdatedAt: utils.ParseTimeFromMap(data, "updated_at"),
	}
}

// parseMemoryList 从 APIResponse 解析 Memory 列表
func parseMemoryList(resp *types.APIResponse) ([]types.Memory, error) {
	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return nil, agentos.NewError(agentos.CodeInvalidResponse, "记忆列表响应格式异常", nil)
	}

	items := utils.GetInterfaceSlice(data, "memories")
	memories := make([]types.Memory, 0, len(items))
	for _, item := range items {
		if m, ok := item.(map[string]interface{}); ok {
			memories = append(memories, *parseMemoryFromMap(m))
		}
	}
	return memories, nil
}

// parseMemorySearchResult 从 APIResponse 解析记忆搜索结果
func parseMemorySearchResult(resp *types.APIResponse, query string, topK int) (*types.MemorySearchResult, error) {
	data, ok := utils.ExtractDataMap(resp)
	if !ok {
		return nil, agentos.NewError(agentos.CodeInvalidResponse, "记忆搜索响应格式异常", nil)
	}

	memories, err := parseMemoryList(resp)
	if err != nil {
		return nil, err
	}

	return &types.MemorySearchResult{
		Memories: memories,
		Total:    int(utils.GetInt64(data, "total")),
		Query:    query,
		TopK:     topK,
	}, nil
}

