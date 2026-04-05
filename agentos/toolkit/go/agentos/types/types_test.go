// AgentOS Go SDK - 类型定义模块单元测试
// Version: 3.0.0

package types

import (
	"testing"
	"time"
)

func TestTaskStatus_String(t *testing.T) {
	tests := []struct {
		status TaskStatus
		want   string
	}{
		{TaskStatusPending, "pending"},
		{TaskStatusRunning, "running"},
		{TaskStatusCompleted, "completed"},
		{TaskStatusFailed, "failed"},
		{TaskStatusCancelled, "cancelled"},
	}
	for _, tt := range tests {
		if got := tt.status.String(); got != tt.want {
			t.Errorf("%s.String() = %q, want %q", tt.status, got, tt.want)
		}
	}
}

func TestTaskStatus_IsTerminal(t *testing.T) {
	tests := []struct {
		status  TaskStatus
		isTerm  bool
	}{
		{TaskStatusPending, false},
		{TaskStatusRunning, false},
		{TaskStatusCompleted, true},
		{TaskStatusFailed, true},
		{TaskStatusCancelled, true},
	}
	for _, tt := range tests {
		if got := tt.status.IsTerminal(); got != tt.isTerm {
			t.Errorf("%s.IsTerminal() = %v, want %v", tt.status, got, tt.isTerm)
		}
	}
}

func TestMemoryLayer_String(t *testing.T) {
	if MemoryLayerL1.String() != "L1" {
		t.Errorf("L1.String() = %q", MemoryLayerL1.String())
	}
}

func TestMemoryLayer_IsValid(t *testing.T) {
	if !MemoryLayerL1.IsValid() || !MemoryLayerL4.IsValid() {
		t.Error("L1/L4 应合法")
	}
	if MemoryLayer("L5").IsValid() {
		t.Error("L5 应不合法")
	}
	if MemoryLayer("").IsValid() {
		t.Error("空层级应不合法")
	}
}

func TestSessionStatus_String(t *testing.T) {
	if SessionStatusActive.String() != "active" {
		t.Errorf("active.String() = %q", SessionStatusActive.String())
	}
}

func TestSkillStatus_String(t *testing.T) {
	if SkillStatusActive.String() != "active" {
		t.Errorf("active.String() = %q", SkillStatusActive.String())
	}
}

func TestDefaultPaginationOptions(t *testing.T) {
	p := DefaultPaginationOptions()
	if p.Page != 1 || p.PageSize != 20 {
		t.Errorf("默认分页 = %+v", p)
	}
}

func TestPaginationOptions_BuildQueryParams(t *testing.T) {
	p := &PaginationOptions{Page: 2, PageSize: 50}
	params := p.BuildQueryParams()
	if params["page"] != "2" {
		t.Errorf("page = %q", params["page"])
	}
	if params["page_size"] != "50" {
		t.Errorf("page_size = %q", params["page_size"])
	}
}

func TestPaginationOptions_BuildQueryParams_Zero(t *testing.T) {
	p := &PaginationOptions{}
	params := p.BuildQueryParams()
	if len(params) != 0 {
		t.Errorf("零值不应产生参数, got %v", params)
	}
}

func TestListOptions_ToQueryParams(t *testing.T) {
	opts := &ListOptions{
		Pagination: &PaginationOptions{Page: 1, PageSize: 10},
		Sort:       &SortOptions{Field: "created_at", Order: "desc"},
		Filter:     &FilterOptions{Key: "status", Value: "active"},
	}
	params := opts.ToQueryParams()
	if params["sort_by"] != "created_at" {
		t.Errorf("sort_by = %q", params["sort_by"])
	}
	if params["filter_key"] != "status" {
		t.Errorf("filter_key = %q", params["filter_key"])
	}
}

func TestRequestOptionFunctions(t *testing.T) {
	opts := &RequestOptions{}
	WithRequestTimeout(5 * time.Second)(opts)
	WithHeader("X-Custom", "value")(opts)
	WithQueryParam("key", "val")(opts)

	if opts.Timeout != 5*time.Second {
		t.Errorf("Timeout = %v, want 5s", opts.Timeout)
	}
	if opts.Headers["X-Custom"] != "value" {
		t.Errorf("Header = %q", opts.Headers["X-Custom"])
	}
	if opts.QueryParams["key"] != "val" {
		t.Errorf("QueryParam = %q", opts.QueryParams["key"])
	}
}
