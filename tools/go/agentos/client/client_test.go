// AgentOS Go SDK - 客户端模块单元测试
// Version: 3.0.0

package client

import (
	"context"
	"encoding/json"
	"io"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"github.com/spharx/agentos/tools/go/agentos"
	"github.com/spharx/agentos/tools/go/agentos/types"
)

func newTestServer(t *testing.T, handler http.HandlerFunc) *httptest.Server {
	t.Helper()
	return httptest.NewServer(handler)
}

func TestNewClient(t *testing.T) {
	c, err := NewClient()
	if err != nil {
		t.Fatalf("NewClient error = %v", err)
	}
	defer c.Close()
	if c == nil {
		t.Fatal("Client 不应为 nil")
	}
}

func TestNewClient_InvalidConfig(t *testing.T) {
	cfg := &agentos.Config{Endpoint: "", Timeout: 30 * time.Second, MaxConnections: 10}
	_, err := NewClientWithConfig(cfg)
	if err == nil {
		t.Error("空端点应返回错误")
	}
	if !agentos.IsErrorCode(err, agentos.CodeInvalidEndpoint) {
		t.Errorf("期望 CodeInvalidEndpoint, got %v", err)
	}
}

func TestClient_Get(t *testing.T) {
	ts := newTestServer(t, func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodGet {
			t.Errorf("期望 GET, got %s", r.Method)
		}
		if !strings.Contains(r.Header.Get("User-Agent"), "AgentOS") {
			t.Error("应包含 User-Agent")
		}
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]interface{}{
			"success": true,
			"data":    map[string]interface{}{"key": "value"},
		})
	})
	defer ts.Close()

	c, err := NewClient(agentos.WithEndpoint(ts.URL))
	if err != nil {
		t.Fatal(err)
	}
	defer c.Close()

	resp, err := c.Get(context.Background(), "/test")
	if err != nil {
		t.Fatalf("Get error = %v", err)
	}
	if !resp.Success {
		t.Error("响应应成功")
	}
}

func TestClient_Post(t *testing.T) {
	ts := newTestServer(t, func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodPost {
			t.Errorf("期望 POST, got %s", r.Method)
		}
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]interface{}{
			"success": true, "data": map[string]interface{}{"id": "1"},
		})
	})
	defer ts.Close()

	c, err := NewClient(agentos.WithEndpoint(ts.URL))
	if err != nil {
		t.Fatal(err)
	}
	defer c.Close()

	resp, err := c.Post(context.Background(), "/test", map[string]string{"key": "val"})
	if err != nil {
		t.Fatalf("Post error = %v", err)
	}
	if !resp.Success {
		t.Error("响应应成功")
	}
}

func TestClient_Put(t *testing.T) {
	ts := newTestServer(t, func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodPut {
			t.Errorf("期望 PUT, got %s", r.Method)
		}
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]interface{}{"success": true, "data": nil})
	})
	defer ts.Close()

	c, _ := NewClient(agentos.WithEndpoint(ts.URL))
	defer c.Close()

	resp, err := c.Put(context.Background(), "/test", nil)
	if err != nil {
		t.Fatalf("Put error = %v", err)
	}
	if !resp.Success {
		t.Error("响应应成功")
	}
}

func TestClient_Delete(t *testing.T) {
	ts := newTestServer(t, func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodDelete {
			t.Errorf("期望 DELETE, got %s", r.Method)
		}
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]interface{}{"success": true, "data": nil})
	})
	defer ts.Close()

	c, _ := NewClient(agentos.WithEndpoint(ts.URL))
	defer c.Close()

	resp, err := c.Delete(context.Background(), "/test")
	if err != nil {
		t.Fatalf("Delete error = %v", err)
	}
	if !resp.Success {
		t.Error("响应应成功")
	}
}

func TestClient_4xxNoRetry(t *testing.T) {
	callCount := 0
	ts := newTestServer(t, func(w http.ResponseWriter, r *http.Request) {
		callCount++
		w.WriteHeader(http.StatusBadRequest)
		w.Write([]byte(`{"success": false, "message": "bad request"}`))
	})
	defer ts.Close()

	c, _ := NewClient(agentos.WithEndpoint(ts.URL), agentos.WithMaxRetries(3))
	defer c.Close()

	_, err := c.Get(context.Background(), "/test")
	if err == nil {
		t.Fatal("400 应返回错误")
	}
	if callCount != 1 {
		t.Errorf("4xx 不应重试, 但调用了 %d 次", callCount)
	}
}

func TestClient_5xxRetry(t *testing.T) {
	callCount := 0
	ts := newTestServer(t, func(w http.ResponseWriter, r *http.Request) {
		callCount++
		if callCount < 3 {
			w.WriteHeader(http.StatusInternalServerError)
			w.Write([]byte("internal error"))
			return
		}
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]interface{}{"success": true, "data": nil})
	})
	defer ts.Close()

	c, _ := NewClient(agentos.WithEndpoint(ts.URL), agentos.WithMaxRetries(3), agentos.WithRetryDelay(10*time.Millisecond))
	defer c.Close()

	resp, err := c.Get(context.Background(), "/test")
	if err != nil {
		t.Fatalf("5xx 重试后应成功, error = %v", err)
	}
	if !resp.Success {
		t.Error("重试后响应应成功")
	}
	if callCount != 3 {
		t.Errorf("期望重试 3 次, got %d", callCount)
	}
}

func TestClient_ContextCancel(t *testing.T) {
	ts := newTestServer(t, func(w http.ResponseWriter, r *http.Request) {
		time.Sleep(2 * time.Second)
		w.WriteHeader(http.StatusOK)
	})
	defer ts.Close()

	c, _ := NewClient(agentos.WithEndpoint(ts.URL), agentos.WithTimeout(5*time.Second))
	defer c.Close()

	ctx, cancel := context.WithTimeout(context.Background(), 50*time.Millisecond)
	defer cancel()

	_, err := c.Get(ctx, "/test")
	if err == nil {
		t.Error("超时上下文应返回错误")
	}
}

func TestClient_Health(t *testing.T) {
	ts := newTestServer(t, func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]interface{}{
			"success": true,
			"data":    map[string]interface{}{"status": "ok", "version": "1.0", "checks": map[string]string{"db": "ok"}},
		})
	})
	defer ts.Close()

	c, _ := NewClient(agentos.WithEndpoint(ts.URL))
	defer c.Close()

	health, err := c.Health(context.Background())
	if err != nil {
		t.Fatalf("Health error = %v", err)
	}
	if health.Status != "ok" {
		t.Errorf("status = %q", health.Status)
	}
}

func TestClient_Metrics(t *testing.T) {
	ts := newTestServer(t, func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]interface{}{
			"success": true,
			"data":    map[string]interface{}{"tasks_total": float64(100), "cpu_usage": 0.75},
		})
	})
	defer ts.Close()

	c, _ := NewClient(agentos.WithEndpoint(ts.URL))
	defer c.Close()

	metrics, err := c.Metrics(context.Background())
	if err != nil {
		t.Fatalf("Metrics error = %v", err)
	}
	if metrics.TasksTotal != 100 {
		t.Errorf("tasks_total = %d", metrics.TasksTotal)
	}
}

func TestClient_Close(t *testing.T) {
	c, _ := NewClient()
	if err := c.Close(); err != nil {
		t.Errorf("Close error = %v", err)
	}
}

func TestClient_String(t *testing.T) {
	c, _ := NewClient()
	defer c.Close()
	s := c.String()
	if s == "" {
		t.Error("String() 不应返回空")
	}
}

func TestClient_GetConfig(t *testing.T) {
	c, _ := NewClient(agentos.WithAPIKey("test-key"))
	defer c.Close()
	cfg := c.GetConfig()
	if cfg.APIKey != "test-key" {
		t.Error("GetConfig 应返回配置副本")
	}
}

func TestNewClientWithConfig_NilConfig(t *testing.T) {
	c, err := NewClientWithConfig(nil)
	if err != nil {
		t.Fatalf("NewClientWithConfig(nil) error = %v", err)
	}
	defer c.Close()
	if c == nil {
		t.Fatal("Client 不应为 nil")
	}
}

func TestClient_Health_InvalidFormat(t *testing.T) {
	ts := newTestServer(t, func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]interface{}{"success": true, "data": "not a map"})
	})
	defer ts.Close()

	c, _ := NewClient(agentos.WithEndpoint(ts.URL))
	defer c.Close()

	_, err := c.Health(context.Background())
	if err == nil {
		t.Error("无效格式应返回错误")
	}
}

func TestClient_Metrics_InvalidFormat(t *testing.T) {
	ts := newTestServer(t, func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]interface{}{"success": true, "data": 42})
	})
	defer ts.Close()

	c, _ := NewClient(agentos.WithEndpoint(ts.URL))
	defer c.Close()

	_, err := c.Metrics(context.Background())
	if err == nil {
		t.Error("无效格式应返回错误")
	}
}

func TestClient_WithAPIKey_Header(t *testing.T) {
	ts := newTestServer(t, func(w http.ResponseWriter, r *http.Request) {
		auth := r.Header.Get("Authorization")
		if auth != "Bearer test-api-key" {
			t.Errorf("Authorization = %q, want Bearer test-api-key", auth)
		}
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]interface{}{"success": true, "data": nil})
	})
	defer ts.Close()

	c, _ := NewClient(agentos.WithEndpoint(ts.URL), agentos.WithAPIKey("test-api-key"))
	defer c.Close()

	_, err := c.Post(context.Background(), "/secure", map[string]string{"k": "v"})
	if err != nil {
		t.Fatalf("Post error = %v", err)
	}
}

func TestClient_InvalidJSON(t *testing.T) {
	ts := newTestServer(t, func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		w.Write([]byte("not valid json"))
	})
	defer ts.Close()

	c, _ := NewClient(agentos.WithEndpoint(ts.URL))
	defer c.Close()

	_, err := c.Get(context.Background(), "/bad-json")
	if err == nil {
		t.Error("无效 JSON 应返回错误")
	}
}

func TestClient_TooManyRequests(t *testing.T) {
	callCount := 0
	ts := newTestServer(t, func(w http.ResponseWriter, r *http.Request) {
		callCount++
		if callCount < 3 {
			w.WriteHeader(http.StatusTooManyRequests)
			w.Write([]byte("rate limited"))
			return
		}
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]interface{}{"success": true, "data": nil})
	})
	defer ts.Close()

	c, _ := NewClient(agentos.WithEndpoint(ts.URL), agentos.WithMaxRetries(3), agentos.WithRetryDelay(10*time.Millisecond))
	defer c.Close()

	resp, err := c.Get(context.Background(), "/rate-limit")
	if err != nil {
		t.Fatalf("429 重试后应成功, error = %v", err)
	}
	if !resp.Success {
		t.Error("重试后响应应成功")
	}
	if callCount != 3 {
		t.Errorf("期望重试 3 次, got %d", callCount)
	}
}

func TestClient_QueryParams(t *testing.T) {
	ts := newTestServer(t, func(w http.ResponseWriter, r *http.Request) {
		q := r.URL.Query()
		if q.Get("page") != "2" || q.Get("size") != "10" {
			t.Errorf("query params = %v", q)
		}
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]interface{}{"success": true, "data": nil})
	})
	defer ts.Close()

	c, _ := NewClient(agentos.WithEndpoint(ts.URL))
	defer c.Close()

	_, err := c.Get(context.Background(), "/items",
		types.WithQueryParam("page", "2"),
		types.WithQueryParam("size", "10"))
	if err != nil {
		t.Fatalf("Get error = %v", err)
	}
}

func TestNewClient_WithRetryOptions(t *testing.T) {
	c, err := NewClient(
		agentos.WithEndpoint("http://localhost:9999"),
		agentos.WithMaxRetries(5),
		agentos.WithRetryDelay(100*time.Millisecond),
	)
	if err != nil {
		t.Fatalf("NewClient error = %v", err)
	}
	defer c.Close()

	cfg := c.GetConfig()
	if cfg.MaxRetries != 5 {
		t.Errorf("MaxRetries = %d, want 5", cfg.MaxRetries)
	}
	if cfg.RetryDelay != 100*time.Millisecond {
		t.Errorf("RetryDelay = %v, want 100ms", cfg.RetryDelay)
	}
}

func TestClient_CustomHeader(t *testing.T) {
	ts := newTestServer(t, func(w http.ResponseWriter, r *http.Request) {
		custom := r.Header.Get("X-Custom")
		if custom != "test-value" {
			t.Errorf("X-Custom = %q", custom)
		}
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]interface{}{"success": true, "data": nil})
	})
	defer ts.Close()

	c, _ := NewClient(agentos.WithEndpoint(ts.URL))
	defer c.Close()

	_, err := c.Get(context.Background(), "/headers", types.WithHeader("X-Custom", "test-value"))
	if err != nil {
		t.Fatalf("Get error = %v", err)
	}
}

func TestClient_AllRetriesExhausted(t *testing.T) {
	ts := newTestServer(t, func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusServiceUnavailable)
		w.Write([]byte("unavailable"))
	})
	defer ts.Close()

	c, _ := NewClient(agentos.WithEndpoint(ts.URL), agentos.WithMaxRetries(2), agentos.WithRetryDelay(10*time.Millisecond))
	defer c.Close()

	_, err := c.Get(context.Background(), "/unavailable")
	if err == nil {
		t.Error("重试耗尽应返回错误")
	}
}

func TestClient_Post_NilBody(t *testing.T) {
	ts := newTestServer(t, func(w http.ResponseWriter, r *http.Request) {
		body, _ := io.ReadAll(r.Body)
		if len(body) != 0 {
			t.Errorf("nil body 应发送空请求体, got %s", string(body))
		}
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]interface{}{"success": true, "data": nil})
	})
	defer ts.Close()

	c, _ := NewClient(agentos.WithEndpoint(ts.URL))
	defer c.Close()

	_, err := c.Post(context.Background(), "/nil-body", nil)
	if err != nil {
		t.Fatalf("Post nil body error = %v", err)
	}
}


