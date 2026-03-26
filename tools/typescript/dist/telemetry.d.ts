/** 追踪区间状态 */
export declare enum SpanStatus {
    OK = "ok",
    ERROR = "error",
    UNSET = "unset"
}
/** 指标数据点 */
export interface MetricPoint {
    name: string;
    value: number;
    timestamp: number;
    tags?: Record<string, string>;
}
/** 追踪区间 */
export interface Span {
    name: string;
    status: SpanStatus;
    startTime: number;
    endTime?: number;
    duration?: number;
    tags?: Record<string, string>;
}
/** 指标收集器（线程安全） */
export declare class Meter {
    private metrics;
    private maxDataPoints;
    /** 创建新的指标收集器 */
    constructor(maxDataPoints?: number);
    /** 记录一个指标数据点 */
    record(name: string, value: number, tags?: Record<string, string>): void;
    /** 获取指定指标的所有数据点 */
    get(name: string): MetricPoint[];
    /** 获取所有指标名称 */
    getAll(): string[];
    /** 清空所有指标 */
    reset(): void;
}
/** 追踪器（线程安全） */
export declare class Tracer {
    private spans;
    private maxSpans;
    /** 创建新的追踪器 */
    constructor(maxSpans?: number);
    /** 开始一个新的追踪区间 */
    startSpan(name: string): Span;
    /** 完成一个追踪区间并记录 */
    finishSpan(span: Span): void;
    /** 获取所有已完成的追踪区间（返回副本） */
    getSpans(): Span[];
    /** 清空所有追踪区间 */
    reset(): void;
}
/** 遥测聚合器 */
export declare class Telemetry {
    private meter;
    private tracer;
    private serviceName;
    /** 创建新的遥测聚合器 */
    constructor(serviceName?: string);
    /** 获取指标收集器 */
    getMeter(): Meter;
    /** 获取追踪器 */
    getTracer(): Tracer;
    /** 获取服务名称 */
    getServiceName(): string;
    /** 重置所有遥测数据 */
    reset(): void;
}
