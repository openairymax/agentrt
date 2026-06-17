import {
  Meter,
  Tracer,
  Telemetry,
  SpanStatus,
} from '../src/telemetry';

describe('Meter', () => {
  test('should create meter with default maxDataPoints', () => {
    const meter = new Meter();
    expect(meter.getAll()).toHaveLength(0);
  });

  test('should create meter with custom maxDataPoints', () => {
    const meter = new Meter(100);
    expect(meter.getAll()).toHaveLength(0);
  });

  test('should record a metric', () => {
    const meter = new Meter();
    meter.record('request_count', 1);
    const points = meter.get('request_count');
    expect(points).toHaveLength(1);
    expect(points[0].name).toBe('request_count');
    expect(points[0].value).toBe(1);
    expect(points[0].timestamp).toBeGreaterThan(0);
  });

  test('should record metric with tags', () => {
    const meter = new Meter();
    meter.record('latency', 150, { endpoint: '/api/v1/tasks' });
    const points = meter.get('latency');
    expect(points[0].tags).toEqual({ endpoint: '/api/v1/tasks' });
  });

  test('should record multiple data points for same metric', () => {
    const meter = new Meter();
    meter.record('cpu', 50);
    meter.record('cpu', 60);
    meter.record('cpu', 70);
    const points = meter.get('cpu');
    expect(points).toHaveLength(3);
    expect(points[0].value).toBe(50);
    expect(points[1].value).toBe(60);
    expect(points[2].value).toBe(70);
  });

  test('should return empty array for unknown metric', () => {
    const meter = new Meter();
    const points = meter.get('nonexistent');
    expect(points).toEqual([]);
  });

  test('should return all metric names', () => {
    const meter = new Meter();
    meter.record('cpu', 50);
    meter.record('memory', 80);
    const names = meter.getAll();
    expect(names).toContain('cpu');
    expect(names).toContain('memory');
    expect(names).toHaveLength(2);
  });

  test('should trim data points when exceeding maxDataPoints', () => {
    const meter = new Meter(3);
    meter.record('cpu', 10);
    meter.record('cpu', 20);
    meter.record('cpu', 30);
    meter.record('cpu', 40);
    meter.record('cpu', 50);
    const points = meter.get('cpu');
    expect(points).toHaveLength(3);
    expect(points[0].value).toBe(30);
    expect(points[2].value).toBe(50);
  });

  test('should reset all metrics', () => {
    const meter = new Meter();
    meter.record('cpu', 50);
    meter.record('memory', 80);
    meter.reset();
    expect(meter.getAll()).toHaveLength(0);
    expect(meter.get('cpu')).toEqual([]);
  });
});

describe('Tracer', () => {
  test('should create tracer with default maxSpans', () => {
    const tracer = new Tracer();
    expect(tracer.getSpans()).toHaveLength(0);
  });

  test('should create tracer with custom maxSpans', () => {
    const tracer = new Tracer(100);
    expect(tracer.getSpans()).toHaveLength(0);
  });

  test('should start a span', () => {
    const tracer = new Tracer();
    const span = tracer.startSpan('operation');
    expect(span.name).toBe('operation');
    expect(span.status).toBe(SpanStatus.OK);
    expect(span.startTime).toBeGreaterThan(0);
    expect(span.endTime).toBeUndefined();
    expect(span.duration).toBeUndefined();
  });

  test('should finish a span and record it', () => {
    const tracer = new Tracer();
    const span = tracer.startSpan('operation');
    tracer.finishSpan(span);
    expect(span.endTime).toBeGreaterThan(0);
    expect(span.duration).toBeGreaterThanOrEqual(0);

    const spans = tracer.getSpans();
    expect(spans).toHaveLength(1);
    expect(spans[0].name).toBe('operation');
    expect(spans[0].duration).toBeGreaterThanOrEqual(0);
  });

  test('should return copies of spans', () => {
    const tracer = new Tracer();
    const span = tracer.startSpan('op1');
    tracer.finishSpan(span);
    const spans1 = tracer.getSpans();
    const spans2 = tracer.getSpans();
    expect(spans1).not.toBe(spans2);
    expect(spans1).toEqual(spans2);
  });

  test('should trim spans when exceeding maxSpans', () => {
    const tracer = new Tracer(2);
    const span1 = tracer.startSpan('op1');
    tracer.finishSpan(span1);
    const span2 = tracer.startSpan('op2');
    tracer.finishSpan(span2);
    const span3 = tracer.startSpan('op3');
    tracer.finishSpan(span3);
    const spans = tracer.getSpans();
    expect(spans).toHaveLength(2);
    expect(spans[0].name).toBe('op2');
    expect(spans[1].name).toBe('op3');
  });

  test('should reset all spans', () => {
    const tracer = new Tracer();
    const span = tracer.startSpan('op');
    tracer.finishSpan(span);
    tracer.reset();
    expect(tracer.getSpans()).toHaveLength(0);
  });
});

describe('Telemetry', () => {
  test('should create telemetry with default service name', () => {
    const telemetry = new Telemetry();
    expect(telemetry.getServiceName()).toBe('agentos-sdk');
  });

  test('should create telemetry with custom service name', () => {
    const telemetry = new Telemetry('my-service');
    expect(telemetry.getServiceName()).toBe('my-service');
  });

  test('should provide meter', () => {
    const telemetry = new Telemetry();
    const meter = telemetry.getMeter();
    expect(meter).toBeInstanceOf(Meter);
    meter.record('test', 1);
    expect(meter.get('test')).toHaveLength(1);
  });

  test('should provide tracer', () => {
    const telemetry = new Telemetry();
    const tracer = telemetry.getTracer();
    expect(tracer).toBeInstanceOf(Tracer);
    const span = tracer.startSpan('test');
    tracer.finishSpan(span);
    expect(tracer.getSpans()).toHaveLength(1);
  });

  test('should reset meter and tracer', () => {
    const telemetry = new Telemetry();
    telemetry.getMeter().record('test', 1);
    const span = telemetry.getTracer().startSpan('op');
    telemetry.getTracer().finishSpan(span);
    telemetry.reset();
    expect(telemetry.getMeter().getAll()).toHaveLength(0);
    expect(telemetry.getTracer().getSpans()).toHaveLength(0);
  });

  test('should return same meter and tracer instances', () => {
    const telemetry = new Telemetry();
    expect(telemetry.getMeter()).toBe(telemetry.getMeter());
    expect(telemetry.getTracer()).toBe(telemetry.getTracer());
  });
});

describe('SpanStatus', () => {
  test('should have correct values', () => {
    expect(SpanStatus.OK).toBe('ok');
    expect(SpanStatus.ERROR).toBe('error');
    expect(SpanStatus.UNSET).toBe('unset');
  });
});
