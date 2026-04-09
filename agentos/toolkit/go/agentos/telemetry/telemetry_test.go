// AgentOS Go SDK - 遥测模块单元测试
// Version: 3.0.0

package telemetry

import (
	"sync"
	"testing"
	"time"

	"github.com/spharx/agentos/toolkit/go/agentos/types"
)

func TestNewMeter(t *testing.T) {
	m := NewMeter()
	if m == nil {
		t.Fatal("Meter 不应为 nil")
	}
}

func TestMeter_RecordAndGet(t *testing.T) {
	m := NewMeter()
	m.Record("cpu_usage", 75.5, map[string]string{"host": "server1"})

	point, err := m.Get("cpu_usage")
	if err != nil {
		t.Fatalf("Get error = %v", err)
	}
	if point.Name != "cpu_usage" {
		t.Errorf("name = %q", point.Name)
	}
	if point.Value != 75.5 {
		t.Errorf("value = %f", point.Value)
	}
	if point.Tags["host"] != "server1" {
		t.Errorf("tag = %v", point.Tags)
	}
}

func TestMeter_Get_NotFound(t *testing.T) {
	m := NewMeter()
	_, err := m.Get("nonexistent")
	if err == nil {
		t.Error("不存在的指标应返回错误")
	}
}

func TestMeter_GetAll(t *testing.T) {
	m := NewMeter()
	m.Record("metric_a", 1.0, nil)
	m.Record("metric_b", 2.0, nil)

	names := m.GetAll()
	if len(names) != 2 {
		t.Errorf("len(names) = %d", len(names))
	}
}

func TestMeter_GetAll_Empty(t *testing.T) {
	m := NewMeter()
	names := m.GetAll()
	if len(names) != 0 {
		t.Errorf("空 meter 应返回空列表, got %d", len(names))
	}
}

func TestMeter_Record_MultiplePoints(t *testing.T) {
	m := NewMeter()
	m.Record("counter", 1.0, nil)
	m.Record("counter", 2.0, nil)
	m.Record("counter", 3.0, nil)

	point, err := m.Get("counter")
	if err != nil {
		t.Fatalf("Get error = %v", err)
	}
	if point.Value != 3.0 {
		t.Errorf("应返回最新值 3.0, got %f", point.Value)
	}
}

func TestMeter_ConcurrentAccess(t *testing.T) {
	m := NewMeter()
	var wg sync.WaitGroup

	for i := 0; i < 100; i++ {
		wg.Add(1)
		go func(idx int) {
			defer wg.Done()
			m.Record("concurrent_metric", float64(idx), nil)
		}(i)
	}

	wg.Wait()

	point, err := m.Get("concurrent_metric")
	if err != nil {
		t.Fatalf("Get error = %v", err)
	}
	if point.Value == 0 {
		t.Error("应至少记录一个数据点")
	}
}

func TestNewTracer(t *testing.T) {
	tr := NewTracer()
	if tr == nil {
		t.Fatal("Tracer 不应为 nil")
	}
}

func TestTracer_StartAndFinishSpan(t *testing.T) {
	tr := NewTracer()
	span := tr.StartSpan("test-operation")

	if span.Name != "test-operation" {
		t.Errorf("name = %q", span.Name)
	}
	if span.Status != types.SpanStatusOK {
		t.Errorf("status = %q", span.Status)
	}

	time.Sleep(10 * time.Millisecond)
	tr.FinishSpan(span)

	if span.Duration == 0 {
		t.Error("完成后的 span 应有 duration > 0")
	}

	spans := tr.GetSpans()
	if len(spans) != 1 {
		t.Fatalf("len(spans) = %d", len(spans))
	}
	if spans[0].Name != "test-operation" {
		t.Errorf("span name = %q", spans[0].Name)
	}
}

func TestTracer_GetSpans_Empty(t *testing.T) {
	tr := NewTracer()
	spans := tr.GetSpans()
	if len(spans) != 0 {
		t.Errorf("空 tracer 应返回空列表, got %d", len(spans))
	}
}

func TestTracer_MultipleSpans(t *testing.T) {
	tr := NewTracer()

	span1 := tr.StartSpan("op1")
	time.Sleep(5 * time.Millisecond)
	tr.FinishSpan(span1)

	span2 := tr.StartSpan("op2")
	time.Sleep(5 * time.Millisecond)
	tr.FinishSpan(span2)

	span3 := tr.StartSpan("op3")
	time.Sleep(5 * time.Millisecond)
	tr.FinishSpan(span3)

	spans := tr.GetSpans()
	if len(spans) != 3 {
		t.Fatalf("len(spans) = %d", len(spans))
	}
	if spans[0].Name != "op1" || spans[1].Name != "op2" || spans[2].Name != "op3" {
		t.Error("span 顺序不正确")
	}
}

func TestTracer_SpanTags(t *testing.T) {
	tr := NewTracer()
	span := tr.StartSpan("tagged-op")
	span.Tags["user_id"] = "u1"
	span.Tags["trace_id"] = "t1"
	tr.FinishSpan(span)

	spans := tr.GetSpans()
	if spans[0].Tags["user_id"] != "u1" {
		t.Errorf("tag user_id = %v", spans[0].Tags["user_id"])
	}
}

func TestTracer_ConcurrentFinish(t *testing.T) {
	tr := NewTracer()
	var wg sync.WaitGroup

	for i := 0; i < 50; i++ {
		wg.Add(1)
		go func(idx int) {
			defer wg.Done()
			span := tr.StartSpan("concurrent-op")
			time.Sleep(time.Millisecond)
			span.Tags["idx"] = string(rune('0' + idx%10))
			tr.FinishSpan(span)
		}(i)
	}

	wg.Wait()

	spans := tr.GetSpans()
	if len(spans) != 50 {
		t.Errorf("len(spans) = %d, want 50", len(spans))
	}
}

func TestTracer_GetSpans_ReturnsCopy(t *testing.T) {
	tr := NewTracer()
	span := tr.StartSpan("copy-test")
	tr.FinishSpan(span)

	spans := tr.GetSpans()
	spans[0].Name = "modified"

	spans2 := tr.GetSpans()
	if spans2[0].Name == "modified" {
		t.Error("GetSpans 应返回副本，修改不应影响原始数据")
	}
}

func TestNewTelemetry(t *testing.T) {
	tel := NewTelemetry()
	if tel == nil {
		t.Fatal("Telemetry 不应为 nil")
	}
	if tel.Meter == nil {
		t.Error("Meter 不应为 nil")
	}
	if tel.Tracer == nil {
		t.Error("Tracer 不应为 nil")
	}
}

func TestTelemetry_Integration(t *testing.T) {
	tel := NewTelemetry()

	tel.Meter.Record("request_count", 42.0, map[string]string{"endpoint": "/api"})
	tel.Meter.Record("latency_ms", 12.5, map[string]string{"endpoint": "/api"})

	span := tel.Tracer.StartSpan("api_request")
	span.Tags["method"] = "GET"
	time.Sleep(5 * time.Millisecond)
	tel.Tracer.FinishSpan(span)

	names := tel.Meter.GetAll()
	if len(names) != 2 {
		t.Errorf("metrics count = %d", len(names))
	}

	point, err := tel.Meter.Get("request_count")
	if err != nil {
		t.Fatalf("Get error = %v", err)
	}
	if point.Value != 42.0 {
		t.Errorf("value = %f", point.Value)
	}

	spans := tel.Tracer.GetSpans()
	if len(spans) != 1 {
		t.Errorf("spans count = %d", len(spans))
	}
	if spans[0].Tags["method"] != "GET" {
		t.Errorf("span tag = %v", spans[0].Tags)
	}
}

func TestMetricPoint_Timestamp(t *testing.T) {
	m := NewMeter()
	before := time.Now()
	m.Record("ts_test", 1.0, nil)
	after := time.Now()

	point, _ := m.Get("ts_test")
	if point.Timestamp.Before(before) || point.Timestamp.After(after) {
		t.Error("时间戳应在记录时间范围内")
	}
}

func TestSpan_StatusDefault(t *testing.T) {
	tr := NewTracer()
	span := tr.StartSpan("status-test")
	if span.Status != types.SpanStatusOK {
		t.Errorf("默认状态应为 ok, got %q", span.Status)
	}
}

