// AgentOS Go SDK - 客户端并发场景测试
// Version: 3.0.0
// Last updated: 2026-04-05
//
// 测试客户端在高并发场景下的安全性和性能

package client

import (
	"context"
	"fmt"
	"net/http"
	"net/http/httptest"
	"sync"
	"sync/atomic"
	"testing"
	"time"

	"github.com/spharx/agentos/toolkit/go/agentos"
)

func TestClient_ConcurrentRequests(t *testing.T) {
	var requestCount int64
	ts := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		atomic.AddInt64(&requestCount, 1)
		w.WriteHeader(http.StatusOK)
		w.Write([]byte(`{"success":true}`))
	}))
	defer ts.Close()

	cfg := &agentos.manager{
		Endpoint:  ts.URL,
		Timeout:   30 * time.Second,
		MaxConnections: 100,
	}

	c, err := NewClientWithConfig(cfg)
	if err != nil {
		t.Fatal(err)
	}
	defer c.Close()

	const goroutines = 100
	const requestsPerGoroutine = 10

	var wg sync.WaitGroup
	errors := make(chan error, goroutines*requestsPerGoroutine)

	for i := 0; i < goroutines; i++ {
		wg.Add(1)
		go func(id int) {
			defer wg.Done()
			for j := 0; j < requestsPerGoroutine; j++ {
				_, err := c.Get(context.Background(), fmt.Sprintf("/test/%d/%d", id, j))
				if err != nil {
					errors <- err
				}
			}
		}(i)
	}

	wg.Wait()
	close(errors)

	var errorCount int
	for err := range errors {
		t.Logf("请求错误: %v", err)
		errorCount++
	}

	totalRequests := goroutines * requestsPerGoroutine
	errorRate := float64(errorCount) / float64(totalRequests) * 100

	t.Logf("总请求数: %d", totalRequests)
	t.Logf("成功请求数: %d", atomic.LoadInt64(&requestCount))
	t.Logf("错误数: %d", errorCount)
	t.Logf("错误率: %.2f%%", errorRate)

	if errorRate > 5.0 {
		t.Errorf("错误率过高: %.2f%% (期望 < 5%%)", errorRate)
	}
}

func TestClient_ConcurrentReadWrite(t *testing.T) {
	ts := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.Method == http.MethodGet {
			w.WriteHeader(http.StatusOK)
			w.Write([]byte(`{"success":true,"data":{"key":"value"}}`))
		} else if r.Method == http.MethodPost {
			w.WriteHeader(http.StatusCreated)
			w.Write([]byte(`{"success":true,"data":{"id":"123"}}`))
		}
	}))
	defer ts.Close()

	c, err := NewClient(agentos.WithEndpoint(ts.URL))
	if err != nil {
		t.Fatal(err)
	}
	defer c.Close()

	const goroutines = 50
	var wg sync.WaitGroup

	for i := 0; i < goroutines; i++ {
		wg.Add(2)

		go func(id int) {
			defer wg.Done()
			for j := 0; j < 5; j++ {
				_, err := c.Get(context.Background(), fmt.Sprintf("/read/%d/%d", id, j))
				if err != nil {
					t.Errorf("并发读错误: %v", err)
				}
			}
		}(i)

		go func(id int) {
			defer wg.Done()
			for j := 0; j < 5; j++ {
				_, err := c.Post(context.Background(), fmt.Sprintf("/write/%d/%d", id, j), map[string]int{"id": id})
				if err != nil {
					t.Errorf("并发写错误: %v", err)
				}
			}
		}(i)
	}

	wg.Wait()
}

func TestClient_ConnectionPoolExhaustion(t *testing.T) {
	var activeConns int64
	var maxConns int64

	ts := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		current := atomic.AddInt64(&activeConns, 1)
		defer atomic.AddInt64(&activeConns, -1)

		for {
			if max := atomic.LoadInt64(&maxConns); current > max {
				if atomic.CompareAndSwapInt64(&maxConns, max, current) {
					break
				}
			} else {
				break
			}
		}

		time.Sleep(50 * time.Millisecond)
		w.WriteHeader(http.StatusOK)
	}))
	defer ts.Close()

	const maxConnections = 10
	cfg := &agentos.manager{
		Endpoint:  ts.URL,
		Timeout:   30 * time.Second,
		MaxConnections: maxConnections,
	}

	c, err := NewClientWithConfig(cfg)
	if err != nil {
		t.Fatal(err)
	}
	defer c.Close()

	const goroutines = 50
	var wg sync.WaitGroup

	for i := 0; i < goroutines; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			c.Get(context.Background(), "/test")
		}()
	}

	wg.Wait()

	maxObserved := atomic.LoadInt64(&maxConns)
	t.Logf("最大并发连接数: %d (限制: %d)", maxObserved, maxConnections)

	if maxObserved > int64(maxConnections*2) {
		t.Errorf("连接池限制未生效: 最大连接数 %d 超过限制 %d 的2倍", maxObserved, maxConnections)
	}
}

func TestClient_RaceCondition(t *testing.T) {
	ts := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusOK)
	}))
	defer ts.Close()

	c, err := NewClient(agentos.WithEndpoint(ts.URL))
	if err != nil {
		t.Fatal(err)
	}
	defer c.Close()

	const iterations = 100
	var wg sync.WaitGroup

	for i := 0; i < iterations; i++ {
		wg.Add(3)

		go func() {
			defer wg.Done()
			c.Get(context.Background(), "/test1")
		}()

		go func() {
			defer wg.Done()
			c.Post(context.Background(), "/test2", map[string]string{"key": "value"})
		}()

		go func() {
			defer wg.Done()
			c.Close()
		}()
	}

	wg.Wait()
}

func TestClient_ConcurrentWithTimeout(t *testing.T) {
	ts := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		time.Sleep(200 * time.Millisecond)
		w.WriteHeader(http.StatusOK)
	}))
	defer ts.Close()

	c, err := NewClient(agentos.WithEndpoint(ts.URL))
	if err != nil {
		t.Fatal(err)
	}
	defer c.Close()

	const goroutines = 20
	var wg sync.WaitGroup
	var timeoutCount int64

	for i := 0; i < goroutines; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			ctx, cancel := context.WithTimeout(context.Background(), 100*time.Millisecond)
			defer cancel()

			_, err := c.Get(ctx, "/test")
			if err != nil && agentos.IsErrorCode(err, agentos.CodeTimeout) {
				atomic.AddInt64(&timeoutCount, 1)
			}
		}()
	}

	wg.Wait()

	t.Logf("超时请求数: %d / %d", timeoutCount, goroutines)
	if timeoutCount < int64(goroutines*8/10) {
		t.Errorf("期望至少80%%的请求超时，实际: %d%%", timeoutCount*100/int64(goroutines))
	}
}

func TestClient_StressTest(t *testing.T) {
	if testing.Short() {
		t.Skip("跳过压力测试")
	}

	var successCount int64
	var failureCount int64

	ts := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if time.Now().UnixNano()%10 == 0 {
			w.WriteHeader(http.StatusInternalServerError)
			return
		}
		w.WriteHeader(http.StatusOK)
		w.Write([]byte(`{"success":true}`))
	}))
	defer ts.Close()

	c, err := NewClient(
		agentos.WithEndpoint(ts.URL),
		agentos.WithMaxConnections(200),
	)
	if err != nil {
		t.Fatal(err)
	}
	defer c.Close()

	const duration = 5 * time.Second
	const workers = 100

	var wg sync.WaitGroup
	stopCh := make(chan struct{})

	for i := 0; i < workers; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for {
				select {
				case <-stopCh:
					return
				default:
					_, err := c.Get(context.Background(), "/test")
					if err == nil {
						atomic.AddInt64(&successCount, 1)
					} else {
						atomic.AddInt64(&failureCount, 1)
					}
				}
			}
		}()
	}

	time.Sleep(duration)
	close(stopCh)
	wg.Wait()

	total := successCount + failureCount
	t.Logf("压力测试结果 (%v):", duration)
	t.Logf("  总请求数: %d", total)
	t.Logf("  成功: %d (%.2f%%)", successCount, float64(successCount)/float64(total)*100)
	t.Logf("  失败: %d (%.2f%%)", failureCount, float64(failureCount)/float64(total)*100)
	t.Logf("  QPS: %.2f", float64(total)/duration.Seconds())

	if total < 1000 {
		t.Errorf("QPS过低: 期望 > 200, 实际 %.2f", float64(total)/duration.Seconds())
	}
}
