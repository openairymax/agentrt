// AgentOS Go SDK - Mock 客户端实现
// Version: 3.0.0
//
// 提供 APIClient 接口的 Mock 实现，供各模块单元测试使用。
// 注意：此文件为非测试源文件，可被其他包正常导入。

package client

import (
	"context"
	"errors"

	"github.com/spharx/agentos/tools/go/agentos/types"
)

// MockAPIClient 是 APIClient 接口的 Mock 实现，支持通过函数字段自定义行为
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
	return nil, errors.New("not implemented")
}

// Post 实现 APIClient.Post 接口
func (m *MockAPIClient) Post(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
	if m.PostFn != nil {
		return m.PostFn(ctx, path, body, opts...)
	}
	return nil, errors.New("not implemented")
}

// Put 实现 APIClient.Put 接口
func (m *MockAPIClient) Put(ctx context.Context, path string, body interface{}, opts ...types.RequestOption) (*types.APIResponse, error) {
	if m.PutFn != nil {
		return m.PutFn(ctx, path, body, opts...)
	}
	return nil, errors.New("not implemented")
}

// Delete 实现 APIClient.Delete 接口
func (m *MockAPIClient) Delete(ctx context.Context, path string, opts ...types.RequestOption) (*types.APIResponse, error) {
	if m.DelFn != nil {
		return m.DelFn(ctx, path, opts...)
	}
	return nil, errors.New("not implemented")
}
