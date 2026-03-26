"use strict";
// AgentOS TypeScript SDK Telemetry
// Version: 2.0.0
// Last updated: 2026-03-23
Object.defineProperty(exports, "__esModule", { value: true });
exports.Telemetry = exports.Tracer = exports.Meter = exports.SpanStatus = void 0;
/** 追踪区间状态 */
var SpanStatus;
(function (SpanStatus) {
    SpanStatus["OK"] = "ok";
    SpanStatus["ERROR"] = "error";
    SpanStatus["UNSET"] = "unset";
})(SpanStatus || (exports.SpanStatus = SpanStatus = {}));
/** 指标收集器（线程安全） */
class Meter {
    /** 创建新的指标收集器 */
    constructor(maxDataPoints = 1000) {
        this.metrics = new Map();
        this.maxDataPoints = maxDataPoints;
    }
    /** 记录一个指标数据点 */
    record(name, value, tags) {
        const point = {
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
    /** 获取指定指标的所有数据点 */
    get(name) {
        return this.metrics.get(name) || [];
    }
    /** 获取所有指标名称 */
    getAll() {
        return Array.from(this.metrics.keys());
    }
    /** 清空所有指标 */
    reset() {
        this.metrics.clear();
    }
}
exports.Meter = Meter;
/** 追踪器（线程安全） */
class Tracer {
    /** 创建新的追踪器 */
    constructor(maxSpans = 500) {
        this.spans = [];
        this.maxSpans = maxSpans;
    }
    /** 开始一个新的追踪区间 */
    startSpan(name) {
        return {
            name,
            status: SpanStatus.OK,
            startTime: Date.now(),
            tags: {},
        };
    }
    /** 完成一个追踪区间并记录 */
    finishSpan(span) {
        span.endTime = Date.now();
        span.duration = span.endTime - span.startTime;
        this.spans.push({ ...span });
        if (this.spans.length > this.maxSpans) {
            this.spans.splice(0, this.spans.length - this.maxSpans);
        }
    }
    /** 获取所有已完成的追踪区间（返回副本） */
    getSpans() {
        return this.spans.map((s) => ({ ...s }));
    }
    /** 清空所有追踪区间 */
    reset() {
        this.spans = [];
    }
}
exports.Tracer = Tracer;
/** 遥测聚合器 */
class Telemetry {
    /** 创建新的遥测聚合器 */
    constructor(serviceName = 'agentos-sdk') {
        this.serviceName = serviceName;
        this.meter = new Meter();
        this.tracer = new Tracer();
    }
    /** 获取指标收集器 */
    getMeter() {
        return this.meter;
    }
    /** 获取追踪器 */
    getTracer() {
        return this.tracer;
    }
    /** 获取服务名称 */
    getServiceName() {
        return this.serviceName;
    }
    /** 重置所有遥测数据 */
    reset() {
        this.meter.reset();
        this.tracer.reset();
    }
}
exports.Telemetry = Telemetry;
