// AgentOS Go SDK - 遥测模块
// Version: 3.0.0
// Last updated: 2026-03-22
//
// 提供可观测性功能，包括指标收集、分布式追踪和运行时监控。
// 对应 Python SDK: telemetry.py + telemetry/__init__.py

package telemetry

import (
	"fmt"
	"sync"
	"time"

	"github.com/spharx/agentos/toolkit/go/agentos/types"
)

// DefaultMaxDataPoints 每个指标名称的最大数据点数
const DefaultMaxDataPoints = 1000

// DefaultMaxSpans 最大追踪区间数
const DefaultMaxSpans = 500

// MetricPoint 表示一个单一的数据点
type MetricPoint struct {
	Name      string                 `json:"name"`
	Value     float64                `json:"value"`
	Timestamp time.Time              `json:"timestamp"`
	Tags      map[string]string      `json:"tags"`
}

// Meter 负责多维度指标收集（内置数据点上限控制）
type Meter struct {
	mu           sync.RWMutex
	metrics      map[string][]MetricPoint
	maxDataPoints int
}

// NewMeter 创建新的指标收集器
func NewMeter() *Meter {
	return &Meter{
		metrics:      make(map[string][]MetricPoint),
		maxDataPoints: DefaultMaxDataPoints,
	}
}

// Record 记录一个指标数据点（超出上限时淘汰最早的数据点）
func (m *Meter) Record(name string, value float64, tags map[string]string) {
	m.mu.Lock()
	defer m.mu.Unlock()

	point := MetricPoint{
		Name:      name,
		Value:     value,
		Timestamp: time.Now(),
		Tags:      tags,
	}
	m.metrics[name] = append(m.metrics[name], point)

	if len(m.metrics[name]) > m.maxDataPoints {
		m.metrics[name] = m.metrics[name][len(m.metrics[name])-m.maxDataPoints:]
	}
}

// Get 获取指定指标的最近数据点
func (m *Meter) Get(name string) (*MetricPoint, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()

	points, ok := m.metrics[name]
	if !ok || len(points) == 0 {
		return nil, fmt.Errorf("指标 %s 不存在", name)
	}
	return &points[len(points)-1], nil
}

// GetAll 获取所有指标名称
func (m *Meter) GetAll() []string {
	m.mu.RLock()
	defer m.mu.RUnlock()

	names := make([]string, 0, len(m.metrics))
	for name := range m.metrics {
		names = append(names, name)
	}
	return names
}

// Reset 清空所有已记录的指标数据
func (m *Meter) Reset() {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.metrics = make(map[string][]MetricPoint)
}

// Span 表示一个追踪区间
type Span struct {
	Name      string            `json:"name"`
	Status    types.SpanStatus  `json:"status"`
	StartTime time.Time         `json:"start_time"`
	EndTime   time.Time         `json:"end_time"`
	Duration  float64           `json:"duration"`
	Tags      map[string]string `json:"tags"`
}

// Tracer 负责分布式追踪（内置跨度上限控制）
type Tracer struct {
	mu        sync.RWMutex
	spans     []Span
	maxSpans  int
}

// NewTracer 创建新的追踪器
func NewTracer() *Tracer {
	return &Tracer{
		spans:    make([]Span, 0),
		maxSpans: DefaultMaxSpans,
	}
}

// StartSpan 开始一个新的追踪区间
func (t *Tracer) StartSpan(name string) *Span {
	return &Span{
		Name:      name,
		Status:    types.SpanStatusOK,
		StartTime: time.Now(),
		Tags:      make(map[string]string),
	}
}

// FinishSpan 完成一个追踪区间并记录（超出上限时淘汰最早的区间）
func (t *Tracer) FinishSpan(span *Span) {
	span.EndTime = time.Now()
	span.Duration = span.EndTime.Sub(span.StartTime).Seconds()

	t.mu.Lock()
	defer t.mu.Unlock()
	t.spans = append(t.spans, *span)

	if len(t.spans) > t.maxSpans {
		t.spans = t.spans[len(t.spans)-t.maxSpans:]
	}
}

// GetSpans 获取所有已完成的追踪区间
func (t *Tracer) GetSpans() []Span {
	t.mu.RLock()
	defer t.mu.RUnlock()

	result := make([]Span, len(t.spans))
	copy(result, t.spans)
	return result
}

// Reset 清空所有已记录的追踪区间
func (t *Tracer) Reset() {
	t.mu.Lock()
	defer t.mu.Unlock()
	t.spans = make([]Span, 0)
}

// Telemetry 聚合指标收集和分布式追踪
type Telemetry struct {
	Meter  *Meter
	Tracer *Tracer
}

// NewTelemetry 创建新的遥测实例
func NewTelemetry() *Telemetry {
	return &Telemetry{
		Meter:  NewMeter(),
		Tracer: NewTracer(),
	}
}

