// AgentOS Rust SDK Telemetry
// Version: 2.0.0
// Last updated: 2026-03-23

use std::collections::HashMap;
use std::sync::{Arc, Mutex};
use std::time::Instant;

const DEFAULT_MAX_DATA_POINTS: usize = 1000;
const DEFAULT_MAX_SPANS: usize = 500;

/// Span 状态
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum SpanStatus {
    OK,
    Error,
    Unset,
}

impl std::fmt::Display for SpanStatus {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            SpanStatus::OK => write!(f, "ok"),
            SpanStatus::Error => write!(f, "error"),
            SpanStatus::Unset => write!(f, "unset"),
        }
    }
}

/// 指标数据点
#[derive(Debug, Clone)]
pub struct MetricPoint {
    pub name: String,
    pub value: f64,
    pub timestamp: u128,
    pub tags: HashMap<String, String>,
}

/// Span 追踪
#[derive(Debug, Clone)]
pub struct Span {
    pub name: String,
    pub status: SpanStatus,
    pub start_time: u128,
    pub end_time: Option<u128>,
    pub duration: Option<f64>,
    pub tags: HashMap<String, String>,
}

impl Span {
    /// 创建新的 Span
    pub fn new(name: &str) -> Self {
        Span {
            name: name.to_string(),
            status: SpanStatus::OK,
            start_time: Self::now_nanos(),
            end_time: None,
            duration: None,
            tags: HashMap::new(),
        }
    }

    /// 结束 Span 并计算持续时间
    pub fn finish(&mut self) {
        self.end_time = Some(Self::now_nanos());
        self.duration = self.end_time.map(|end| {
            (end - self.start_time) as f64 / 1_000_000_000.0
        });
    }

    fn now_nanos() -> u128 {
        std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .map(|d| d.as_nanos())
            .unwrap_or(0)
    }
}

/// 指标收集器
#[derive(Debug)]
pub struct Meter {
    data_points: Vec<MetricPoint>,
    max_data_points: usize,
}

impl Meter {
    /// 创建新的 Meter
    pub fn new() -> Self {
        Meter {
            data_points: Vec::new(),
            max_data_points: DEFAULT_MAX_DATA_POINTS,
        }
    }

    /// 创建带自定义上限的 Meter
    pub fn with_max(max_data_points: usize) -> Self {
        Meter {
            data_points: Vec::new(),
            max_data_points,
        }
    }

    /// 记录指标数据点
    pub fn record(&mut self, name: &str, value: f64, tags: Option<HashMap<String, String>>) {
        let point = MetricPoint {
            name: name.to_string(),
            value,
            timestamp: std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .map(|d| d.as_millis())
                .unwrap_or(0),
            tags: tags.unwrap_or_default(),
        };

        if self.data_points.len() >= self.max_data_points {
            self.data_points.remove(0);
        }
        self.data_points.push(point);
    }

    /// 获取指定名称的指标数据点
    pub fn get(&self, name: &str) -> Vec<&MetricPoint> {
        self.data_points
            .iter()
            .filter(|p| p.name == name)
            .collect()
    }

    /// 获取所有指标名称
    pub fn get_all_names(&self) -> Vec<String> {
        let mut names: Vec<String> = self
            .data_points
            .iter()
            .map(|p| p.name.clone())
            .collect();
        names.sort();
        names.dedup();
        names
    }

    /// 获取所有数据点
    pub fn get_all(&self) -> &[MetricPoint] {
        &self.data_points
    }

    /// 重置所有数据
    pub fn reset(&mut self) {
        self.data_points.clear();
    }
}

/// 追踪器
#[derive(Debug)]
pub struct Tracer {
    spans: Vec<Span>,
    max_spans: usize,
}

impl Tracer {
    /// 创建新的 Tracer
    pub fn new() -> Self {
        Tracer {
            spans: Vec::new(),
            max_spans: DEFAULT_MAX_SPANS,
        }
    }

    /// 创建带自定义上限的 Tracer
    pub fn with_max(max_spans: usize) -> Self {
        Tracer {
            spans: Vec::new(),
            max_spans,
        }
    }

    /// 开始新的 Span
    pub fn start_span(&mut self, name: &str) -> Span {
        if self.spans.len() >= self.max_spans {
            self.spans.remove(0);
        }
        let span = Span::new(name);
        self.spans.push(span.clone());
        span
    }

    /// 结束 Span
    pub fn finish_span(&mut self, span: &mut Span) {
        span.finish();
    }

    /// 获取所有 Span
    pub fn get_spans(&self) -> &[Span] {
        &self.spans
    }

    /// 重置所有 Span
    pub fn reset(&mut self) {
        self.spans.clear();
    }
}

/// 遥测聚合器
#[derive(Debug)]
pub struct Telemetry {
    service_name: String,
    meter: Arc<Mutex<Meter>>,
    tracer: Arc<Mutex<Tracer>>,
}

impl Telemetry {
    /// 创建新的遥测聚合器
    pub fn new(service_name: &str) -> Self {
        Telemetry {
            service_name: service_name.to_string(),
            meter: Arc::new(Mutex::new(Meter::new())),
            tracer: Arc::new(Mutex::new(Tracer::new())),
        }
    }

    /// 创建带默认服务名的遥测聚合器
    pub fn default_telemetry() -> Self {
        Telemetry::new("agentos-sdk")
    }

    /// 获取服务名
    pub fn service_name(&self) -> &str {
        &self.service_name
    }

    /// 获取 Meter 的克隆引用
    pub fn meter(&self) -> Arc<Mutex<Meter>> {
        Arc::clone(&self.meter)
    }

    /// 获取 Tracer 的克隆引用
    pub fn tracer(&self) -> Arc<Mutex<Tracer>> {
        Arc::clone(&self.tracer)
    }

    /// 重置所有遥测数据
    pub fn reset(&self) {
        if let Ok(mut meter) = self.meter.lock() {
            meter.reset();
        }
        if let Ok(mut tracer) = self.tracer.lock() {
            tracer.reset();
        }
    }
}
