// AgentOS Go SDK - 客户端边界条件测试
// Version: 3.0.0
// Last updated: 2026-04-05
//
// 测试客户端在各种边界条件下的行为

package client

import (
	"context"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"github.com/spharx/agentos/toolkit/go/agentos"
)

func TestClient_TimeoutBoundary(t *testing.T) {
	tests := []struct {
		name     string
		timeout  time.Duration
		wantErr  bool
		errCode  agentos.ErrorCode
	}{
		{
			name:    "最小超时_1ms",
			timeout: 1 * time.Millisecond,
			wantErr: true,
			errCode: agentos.CodeTimeout,
		},
		{
			name:    "正常超时_30s",
			timeout: 30 * time.Second,
			wantErr: false,
		},
		{
			name:    "最大超时_1h",
			timeout: 1 * time.Hour,
			wantErr: false,
		},
		{
			name:    "零值超时",
			timeout: 0,
			wantErr: true,
			errCode: agentos.CodeInvalidConfig,
		},
		{
			name:    "负数超时",
			timeout: -1 * time.Second,
			wantErr: true,
			errCode: agentos.CodeInvalidConfig,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			ts := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
				if tt.timeout == 1*time.Millisecond {
					time.Sleep(10 * time.Millisecond)
				}
				w.WriteHeader(http.StatusOK)
			}))
			defer ts.Close()

			cfg := &agentos.Config{
				Endpoint:  ts.URL,
				Timeout:   tt.timeout,
				MaxConnections: 10,
			}

			c, err := NewClientWithConfig(cfg)
			if err != nil {
				if tt.wantErr && agentos.IsErrorCode(err, tt.errCode) {
					return
				}
				t.Fatalf("NewClientWithConfig error = %v", err)
			}
			defer c.Close()

			_, err = c.Get(context.Background(), "/test")
			if tt.wantErr {
				if err == nil {
					t.Errorf("%s: 期望错误但未返回", tt.name)
				} else if !agentos.IsErrorCode(err, tt.errCode) {
					t.Errorf("%s: 期望错误码 %s, got %v", tt.name, tt.errCode, err)
				}
			} else {
				if err != nil {
					t.Errorf("%s: 不期望错误但返回: %v", tt.name, err)
				}
			}
		})
	}
}

func TestClient_MaxConnectionsBoundary(t *testing.T) {
	tests := []struct {
		name          string
		maxConn       int
		wantErr       bool
		errCode       agentos.ErrorCode
	}{
		{
			name:    "最小连接数_1",
			maxConn: 1,
			wantErr: false,
		},
		{
			name:    "正常连接数_100",
			maxConn: 100,
			wantErr: false,
		},
		{
			name:    "最大连接数_10000",
			maxConn: 10000,
			wantErr: false,
		},
		{
			name:    "零值连接数",
			maxConn: 0,
			wantErr: true,
			errCode: agentos.CodeInvalidConfig,
		},
		{
			name:    "负数连接数",
			maxConn: -1,
			wantErr: true,
			errCode: agentos.CodeInvalidConfig,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			ts := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
				w.WriteHeader(http.StatusOK)
			}))
			defer ts.Close()

			cfg := &agentos.Config{
				Endpoint:  ts.URL,
				Timeout:   30 * time.Second,
				MaxConnections: tt.maxConn,
			}

			c, err := NewClientWithConfig(cfg)
			if tt.wantErr {
				if err == nil {
					t.Errorf("%s: 期望错误但未返回", tt.name)
				} else if !agentos.IsErrorCode(err, tt.errCode) {
					t.Errorf("%s: 期望错误码 %s, got %v", tt.name, tt.errCode, err)
				}
				return
			}

			if err != nil {
				t.Fatalf("NewClientWithConfig error = %v", err)
			}
			defer c.Close()
		})
	}
}

func TestClient_EmptyEndpoint(t *testing.T) {
	cfg := &agentos.Config{
		Endpoint:  "",
		Timeout:   30 * time.Second,
		MaxConnections: 10,
	}

	_, err := NewClientWithConfig(cfg)
	if err == nil {
		t.Error("空端点应返回错误")
	}
	if !agentos.IsErrorCode(err, agentos.CodeInvalidEndpoint) {
		t.Errorf("期望 CodeInvalidEndpoint, got %v", err)
	}
}

func TestClient_InvalidEndpoint(t *testing.T) {
	tests := []struct {
		name     string
		endpoint string
		wantErr  bool
	}{
		{
			name:     "缺少协议",
			endpoint: "localhost:18789",
			wantErr:  true,
		},
		{
			name:     "无效协议",
			endpoint: "ftp://localhost:18789",
			wantErr:  true,
		},
		{
			name:     "缺少端口",
			endpoint: "http://localhost",
			wantErr:  false,
		},
		{
			name:     "有效HTTP端点",
			endpoint: "http://localhost:18789",
			wantErr:  false,
		},
		{
			name:     "有效HTTPS端点",
			endpoint: "https://localhost:18789",
			wantErr:  false,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			cfg := &agentos.Config{
				Endpoint:  tt.endpoint,
				Timeout:   30 * time.Second,
				MaxConnections: 10,
			}

			c, err := NewClientWithConfig(cfg)
			if tt.wantErr {
				if err == nil {
					t.Errorf("%s: 期望错误但未返回", tt.name)
				}
				return
			}

			if err != nil {
				t.Errorf("%s: 不期望错误但返回: %v", tt.name, err)
			} else {
				c.Close()
			}
		})
	}
}

func TestClient_RequestSizeBoundary(t *testing.T) {
	ts := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusOK)
	}))
	defer ts.Close()

	c, err := NewClient(agentos.WithEndpoint(ts.URL))
	if err != nil {
		t.Fatal(err)
	}
	defer c.Close()

	tests := []struct {
		name    string
		body    []byte
		wantErr bool
	}{
		{
			name:    "空请求体",
			body:    []byte{},
			wantErr: false,
		},
		{
			name:    "小请求体_1KB",
			body:    make([]byte, 1024),
			wantErr: false,
		},
		{
			name:    "中等请求体_1MB",
			body:    make([]byte, 1024*1024),
			wantErr: false,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			_, err := c.Post(context.Background(), "/test", tt.body)
			if tt.wantErr && err == nil {
				t.Errorf("%s: 期望错误但未返回", tt.name)
			}
			if !tt.wantErr && err != nil {
				t.Errorf("%s: 不期望错误但返回: %v", tt.name, err)
			}
		})
	}
}

func TestClient_ContextCancellation(t *testing.T) {
	ts := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		time.Sleep(2 * time.Second)
		w.WriteHeader(http.StatusOK)
	}))
	defer ts.Close()

	c, err := NewClient(agentos.WithEndpoint(ts.URL))
	if err != nil {
		t.Fatal(err)
	}
	defer c.Close()

	ctx, cancel := context.WithTimeout(context.Background(), 100*time.Millisecond)
	defer cancel()

	_, err = c.Get(ctx, "/test")
	if err == nil {
		t.Error("期望超时错误但未返回")
	}
	if !agentos.IsErrorCode(err, agentos.CodeTimeout) {
		t.Errorf("期望 CodeTimeout, got %v", err)
	}
}
