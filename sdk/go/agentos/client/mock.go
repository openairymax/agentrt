// AgentOS Go SDK - Mock 客户端实现
// Version: 0.1.0
//
// 提供 APIClient 接口的 Mock 实现，仅供单元测试使用。
// 警告：此文件为测试工具，不应在生产代码中使用。

package client

import (
	"context"

	"github.com/spharx/agentos/sdk/go/agentos"
	"github.com/spharx/agentos/sdk/go/agentos/types"
)

// MockAPIClient 是 APIClient 接口的 Mock 实现，支持通过函数字段自定义行为
// 仅用于测试，不应在生产代码中引用
type MockAPIClient struct {
	GetFn  func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error)
	PostFn func(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error)
	PutFn  func(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error)
	DelFn  func(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error)
}

// Get 实现 APIClient.Get 接口
func (m *MockAPIClient) Get(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
	if m.GetFn != nil {
		return m.GetFn(ctx, path, opts...)
	}
	return nil, agentos.NewError(agentos.CodeNotSupported, "Mock GET handler not configured", nil)
}

// Post 实现 APIClient.Post 接口
func (m *MockAPIClient) Post(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
	if m.PostFn != nil {
		return m.PostFn(ctx, path, body, opts...)
	}
	return nil, agentos.NewError(agentos.CodeNotSupported, "Mock POST handler not configured", nil)
}

// Put 实现 APIClient.Put 接口
func (m *MockAPIClient) Put(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
	if m.PutFn != nil {
		return m.PutFn(ctx, path, body, opts...)
	}
	return nil, agentos.NewError(agentos.CodeNotSupported, "Mock PUT handler not configured", nil)
}

// Delete 实现 APIClient.Delete 接口
func (m *MockAPIClient) Delete(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
	if m.DelFn != nil {
		return m.DelFn(ctx, path, opts...)
	}
	return nil, agentos.NewError(agentos.CodeNotSupported, "Mock DELETE handler not configured", nil)
}

