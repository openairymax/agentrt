// AgentOS TypeScript SDK Telemetry
// Version: 2.0.0
// Last updated: 2026-03-23

/** ﻟﺟﺛﻟﺕ۹ﮒﭦﻠﺑﻝﭘﮔ?*/
export enum SpanStatus {
  OK = 'ok',
  ERROR = 'error',
  UNSET = 'unset',
}

/** ﮔﮔ ﮔﺍﮔ؟ﻝ?*/
export interface MetricPoint {
  name: string;
  value: number;
  timestamp: number;
  tags?: Record<string, string>;
}

/** ﻟﺟﺛﻟﺕ۹ﮒﭦﻠﺑ */
export interface Span {
  name: string;
  status: SpanStatus;
  startTime: number;
  endTime?: number;
  duration?: number;
  tags?: Record<string, string>;
}

/** ﮔﮔ ﮔﭘﻠﮒ۷ﺅﺙﻝﭦﺟﻝ۷ﮒ؟ﮒ۷ﺅﺙ?*/
export class Meter {
  private metrics: Map<string, MetricPoint[]> = new Map();
  private maxDataPoints: number;

  /** ﮒﮒﭨﭦﮔﺍﻝﮔﮔ ﮔﭘﻠﮒ?*/
  constructor(maxDataPoints: number = 1000) {
    this.maxDataPoints = maxDataPoints;
  }

  /** ﻟ؟ﺍﮒﺛﻛﺕﻛﺕ۹ﮔﮔ ﮔﺍﮔ؟ﻝﺗ */
  record(name: string, value: number, tags?: Record<string, string>): void {
    const point: MetricPoint = {
      name,
      value,
      timestamp: Date.now(),
      tags,
    };
    const points = this.metrics.get(name) || [];
    points.push(point);

    if (points.length > this.maxDataPoints) {
      points.splice(0, points.length - this.maxDataPoints);
    }
    this.metrics.set(name, points);
  }

  /** ﻟﺓﮒﮔﮒ؟ﮔﮔ ﻝﮔﮔﮔﺍﮔ؟ﻝﺗ */
  get(name: string): MetricPoint[] {
    return this.metrics.get(name) || [];
  }

  /** ﻟﺓﮒﮔﮔﮔﮔ ﮒﻝ۶?*/
  getAll(): string[] {
    return Array.from(this.metrics.keys());
  }

  /** ﮔﺕﻝ۸ﭦﮔﮔﮔﮔ ?*/
  reset(): void {
    this.metrics.clear();
  }
}

/** ﻟﺟﺛﻟﺕ۹ﮒ۷ﺅﺙﻝﭦﺟﻝ۷ﮒ؟ﮒ۷ﺅﺙ?*/
export class Tracer {
  private spans: Span[] = [];
  private maxSpans: number;

  /** ﮒﮒﭨﭦﮔﺍﻝﻟﺟﺛﻟﺕ۹ﮒ?*/
  constructor(maxSpans: number = 500) {
    this.maxSpans = maxSpans;
  }

  /** ﮒﺙﮒ۶ﻛﺕﻛﺕ۹ﮔﺍﻝﻟﺟﺛﻟﺕ۹ﮒﭦﻠ?*/
  startSpan(name: string): Span {
    return {
      name,
      status: SpanStatus.OK,
      startTime: Date.now(),
      tags: {},
    };
  }

  /** ﮒ؟ﮔﻛﺕﻛﺕ۹ﻟﺟﺛﻟﺕ۹ﮒﭦﻠﺑﮒﺗﭘﻟ؟ﺍﮒﺛ */
  finishSpan(span: Span): void {
    span.endTime = Date.now();
    span.duration = span.endTime - span.startTime;

    this.spans.push({ ...span });
    if (this.spans.length > this.maxSpans) {
      this.spans.splice(0, this.spans.length - this.maxSpans);
    }
  }

  /** ﻟﺓﮒﮔﮔﮒﺓﺎﮒ؟ﮔﻝﻟﺟﺛﻟﺕ۹ﮒﭦﻠﺑﺅﺙﻟﺟﮒﮒﺁﮔ؛ﺅﺙ?*/
  getSpans(): Span[] {
    return this.spans.map((s) => ({ ...s }));
  }

  /** ﮔﺕﻝ۸ﭦﮔﮔﻟﺟﺛﻟﺕ۹ﮒﭦﻠ?*/
  reset(): void {
    this.spans = [];
  }
}

/** ﻠ۴ﮔﭖﻟﮒﮒ?*/
export class Telemetry {
  private meter: Meter;
  private tracer: Tracer;
  private serviceName: string;

  /** ﮒﮒﭨﭦﮔﺍﻝﻠ۴ﮔﭖﻟﮒﮒ?*/
  constructor(serviceName: string = 'agentos-sdk') {
    this.serviceName = serviceName;
    this.meter = new Meter();
    this.tracer = new Tracer();
  }

  /** ﻟﺓﮒﮔﮔ ﮔﭘﻠﮒ?*/
  getMeter(): Meter {
    return this.meter;
  }

  /** ﻟﺓﮒﻟﺟﺛﻟﺕ۹ﮒ?*/
  getTracer(): Tracer {
    return this.tracer;
  }

  /** ﻟﺓﮒﮔﮒ۰ﮒﻝ۶ﺍ */
  getServiceName(): string {
    return this.serviceName;
  }

  /** ﻠﻝﺛ؟ﮔﮔﻠ۴ﮔﭖﮔﺍﮔ?*/
  reset(): void {
    this.meter.reset();
    this.tracer.reset();
  }
}
