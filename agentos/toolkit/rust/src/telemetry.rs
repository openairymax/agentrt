// AgentOS Rust SDK Telemetry
// Version: 2.0.0
// Last updated: 2026-03-23

use std::collections::HashMap;
use std::sync::{Arc, Mutex};
use std::time::Instant;

const DEFAULT_MAX_DATA_POINTS: usize = 1000;
const DEFAULT_MAX_SPANS: usize = 500;

/// Span ﻝﭘﮔ?#[derive(Debug, Clone, PartialEq, Eq)]
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

/// ﮔﮔ ﮔﺍﮔ؟ﻝ?#[derive(Debug, Clone)]
pub struct MetricPoint {
    pub name: String,
    pub value: f64,
    pub timestamp: u128,
    pub tags: HashMap<String, String>,
}

/// Span ﻟﺟﺛﻟﺕ۹
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
    /// ﮒﮒﭨﭦﮔﺍﻝ Span
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

    /// ﻝﭨﮔ Span ﮒﺗﭘﻟ؟۰ﻝ؟ﮔﻝﭨ­ﮔﭘﻠ?    pub fn finish(&mut self) {
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

/// ﮔﮔ ﮔﭘﻠﮒ?#[derive(Debug)]
pub struct Meter {
    data_points: Vec<MetricPoint>,
    max_data_points: usize,
}

impl Meter {
    /// ﮒﮒﭨﭦﮔﺍﻝ Meter
    pub fn new() -> Self {
        Meter {
            data_points: Vec::new(),
            max_data_points: DEFAULT_MAX_DATA_POINTS,
        }
    }

    /// ﮒﮒﭨﭦﮒﺕ۵ﻟ۹ﮒ؟ﻛﺗﻛﺕﻠﻝ?Meter
    pub fn with_max(max_data_points: usize) -> Self {
        Meter {
            data_points: Vec::new(),
            max_data_points,
        }
    }

    /// ﻟ؟ﺍﮒﺛﮔﮔ ﮔﺍﮔ؟ﻝ?    pub fn record(&mut self, name: &str, value: f64, tags: Option<HashMap<String, String>>) {
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

    /// ﻟﺓﮒﮔﮒ؟ﮒﻝ۶ﺍﻝﮔﮔ ﮔﺍﮔ؟ﻝﺗ
    pub fn get(&self, name: &str) -> Vec<&MetricPoint> {
        self.data_points
            .iter()
            .filter(|p| p.name == name)
            .collect()
    }

    /// ﻟﺓﮒﮔﮔﮔﮔ ﮒﻝ۶?    pub fn get_all_names(&self) -> Vec<String> {
        let mut names: Vec<String> = self
            .data_points
            .iter()
            .map(|p| p.name.clone())
            .collect();
        names.sort();
        names.dedup();
        names
    }

    /// ﻟﺓﮒﮔﮔﮔﺍﮔ؟ﻝﺗ
    pub fn get_all(&self) -> &[MetricPoint] {
        &self.data_points
    }

    /// ﻠﻝﺛ؟ﮔﮔﮔﺍﮔ?    pub fn reset(&mut self) {
        self.data_points.clear();
    }
}

/// ﻟﺟﺛﻟﺕ۹ﮒ?#[derive(Debug)]
pub struct Tracer {
    spans: Vec<Span>,
    max_spans: usize,
}

impl Tracer {
    /// ﮒﮒﭨﭦﮔﺍﻝ Tracer
    pub fn new() -> Self {
        Tracer {
            spans: Vec::new(),
            max_spans: DEFAULT_MAX_SPANS,
        }
    }

    /// ﮒﮒﭨﭦﮒﺕ۵ﻟ۹ﮒ؟ﻛﺗﻛﺕﻠﻝ?Tracer
    pub fn with_max(max_spans: usize) -> Self {
        Tracer {
            spans: Vec::new(),
            max_spans,
        }
    }

    /// ﮒﺙﮒ۶ﮔﺍﻝ?Span
    pub fn start_span(&mut self, name: &str) -> Span {
        if self.spans.len() >= self.max_spans {
            self.spans.remove(0);
        }
        let span = Span::new(name);
        self.spans.push(span.clone());
        span
    }

    /// ﻝﭨﮔ Span
    pub fn finish_span(&mut self, span: &mut Span) {
        span.finish();
    }

    /// ﻟﺓﮒﮔﮔ?Span
    pub fn get_spans(&self) -> &[Span] {
        &self.spans
    }

    /// ﻠﻝﺛ؟ﮔﮔ?Span
    pub fn reset(&mut self) {
        self.spans.clear();
    }
}

/// ﻠ۴ﮔﭖﻟﮒﮒ?#[derive(Debug)]
pub struct Telemetry {
    service_name: String,
    meter: Arc<Mutex<Meter>>,
    tracer: Arc<Mutex<Tracer>>,
}

impl Telemetry {
    /// ﮒﮒﭨﭦﮔﺍﻝﻠ۴ﮔﭖﻟﮒﮒ?    pub fn new(service_name: &str) -> Self {
        Telemetry {
            service_name: service_name.to_string(),
            meter: Arc::new(Mutex::new(Meter::new())),
            tracer: Arc::new(Mutex::new(Tracer::new())),
        }
    }

    /// ﮒﮒﭨﭦﮒﺕ۵ﻠﭨﻟ؟۳ﮔﮒ۰ﮒﻝﻠ۴ﮔﭖﻟﮒﮒ۷
    pub fn default_telemetry() -> Self {
        Telemetry::new("agentos-sdk")
    }

    /// ﻟﺓﮒﮔﮒ۰ﮒ?    pub fn service_name(&self) -> &str {
        &self.service_name
    }

    /// ﻟﺓﮒ Meter ﻝﮒﻠﮒﺙﻝ?    pub fn meter(&self) -> Arc<Mutex<Meter>> {
        Arc::clone(&self.meter)
    }

    /// ﻟﺓﮒ Tracer ﻝﮒﻠﮒﺙﻝ?    pub fn tracer(&self) -> Arc<Mutex<Tracer>> {
        Arc::clone(&self.tracer)
    }

    /// ﻠﻝﺛ؟ﮔﮔﻠ۴ﮔﭖﮔﺍﮔ?    pub fn reset(&self) {
        if let Ok(mut meter) = self.meter.lock() {
            meter.reset();
        }
        if let Ok(mut tracer) = self.tracer.lock() {
            tracer.reset();
        }
    }
}
