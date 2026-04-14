Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
"From data intelligence emerges."

# AgentOS TypeScript SDK

**版本**: Doc V1.8  
**最后更新**: 2026-04-12  
**状态**: 🟢 生产就绪

---

## 🎯 概述

AgentOS TypeScript SDK 提供 TypeScript/JavaScript 客户端，用于与 AgentOS 微内核系统交互。SDK 遵循 TypeScript 惯用设计，提供完整的类型定义、Promise/async-await 异步支持和自动资源管理。

### 🧩 五维正交原则体现

| 维度 | 原则体现 | 具体实现 |
|------|----------|---------|
| **系统观** | SDK 作为系统边界抽象 | 统一的 AgentOSClient 入口，封装所有子系统 |
| **内核观** | 接口契约化 | 完整的 TypeScript 接口定义，编译时类型检查 |
| **认知观** | 双系统认知模型映射 | System1/System2 路径的类型安全表达 |
| **工程观** | 可观测性集成 | 内置追踪、指标、日志的 SDK 集成 |
| **设计美学** | TypeScript 惯用设计 | async/await、泛型、枚举、命名空间 |

---

## 📦 安装

```bash
# npm
npm install @agentos/sdk

# yarn
yarn add @agentos/sdk

# pnpm
pnpm add @agentos/sdk
```

---

## 🚀 快速开始

### 初始化客户端

```typescript
import { AgentOSClient } from '@agentos/sdk';

const client = new AgentOSClient({
  endpoint: 'http://localhost:8080',
  timeout: 30000,
  apiKey: process.env.AGENTOS_API_KEY,
});

// 健康检查
const health = await client.healthCheck();
console.log(`AgentOS status: ${health.status}`);
```

### 创建 Agent

```typescript
const agent = await client.agents.create({
  name: 'my-agent',
  type: AgentType.TASK,
  maxConcurrentTasks: 4,
  cognitionConfig: {
    model: 'gpt-4',
    temperature: 0.7,
  },
});

console.log(`Agent created: ${agent.id}, state: ${agent.state}`);
```

### 记忆系统操作

```typescript
// 写入记忆
const record = await client.memory.write({
  content: 'AgentOS uses microkernel architecture',
  importance: 0.8,
  tags: ['architecture', 'kernel'],
  layer: MemoryLayer.L1_RAW,
});

// 查询记忆
const results = await client.memory.query({
  text: 'microkernel design patterns',
  limit: 5,
  similarityThreshold: 0.7,
});

for (const result of results.records) {
  console.log(`[${result.score.toFixed(2)}] ${result.content}`);
}
```

---

## 📖 API 参考

### Agent 管理

```typescript
interface AgentAPI {
  create(config: AgentConfig): Promise<AgentDescriptor>;
  get(agentId: string): Promise<AgentDescriptor>;
  update(agentId: string, config: Partial<AgentConfig>): Promise<void>;
  destroy(agentId: string): Promise<void>;
  list(filter?: AgentFilter): Promise<AgentDescriptor[]>;
  start(agentId: string): Promise<void>;
  pause(agentId: string): Promise<void>;
  stop(agentId: string): Promise<void>;
  state(agentId: string): Promise<AgentState>;
  stats(): Promise<AgentStatistics>;
  skillBind(agentId: string, skillName: string, config?: string): Promise<void>;
  skillUnbind(agentId: string, skillName: string): Promise<void>;
  history(agentId: string, offset?: number, limit?: number): Promise<StateHistory[]>;
}
```

**类型定义**：

```typescript
enum AgentType {
  GENERIC = 0,
  CHAT = 1,
  TASK = 2,
  ANALYSIS = 3,
  MONITOR = 4,
}

enum AgentState {
  CREATED = 0,
  INITING = 1,
  READY = 2,
  RUNNING = 3,
  PAUSED = 4,
  STOPPING = 5,
  STOPPED = 6,
  ERROR = 7,
  DESTROYED = 8,
}

interface AgentConfig {
  name: string;
  type: AgentType;
  maxConcurrentTasks?: number;
  defaultTaskTimeoutMs?: number;
  cognitionConfig?: Record<string, unknown>;
  memoryConfig?: Record<string, unknown>;
  securityConfig?: Record<string, unknown>;
  resourceLimits?: ResourceLimits;
  retryConfig?: RetryConfig;
}

interface AgentDescriptor {
  id: string;
  name: string;
  state: AgentState;
  type: AgentType;
  boundSkills: string[];
  system1ExecCount: number;
  system2ExecCount: number;
  activeTaskCount: number;
  memoryUsage: number;
  cpuUsage: number;
  createdAt: Date;
  updatedAt: Date;
}
```

### Memory 管理

```typescript
interface MemoryAPI {
  write(record: MemoryWriteRequest): Promise<MemoryRecord>;
  query(query: MemoryQuery): Promise<MemoryQueryResult>;
  evolve(options?: EvolveOptions): Promise<EvolveResult>;
  forget(strategy: ForgetStrategy): Promise<ForgetResult>;
  stats(): Promise<MemoryStatistics>;
  layerStats(layer?: number): Promise<LayerStats[]>;
  export(filter: ExportFilter): Promise<string>;
  import(data: string, options?: ImportOptions): Promise<ImportReport>;
}
```

**类型定义**：

```typescript
enum MemoryLayer {
  L1_RAW = 1,
  L2_FEATURE = 2,
  L3_STRUCTURE = 3,
  L4_PATTERN = 4,
}

interface MemoryWriteRequest {
  content: string;
  importance?: number;
  tags?: string[];
  layer?: MemoryLayer;
  vector?: number[];
  metadata?: Record<string, unknown>;
}

interface MemoryQuery {
  text: string;
  vector?: number[];
  tags?: string[];
  timeRange?: { start: Date; end: Date };
  importanceThreshold?: number;
  similarityThreshold?: number;
  limit?: number;
  sortBy?: 'relevance' | 'time' | 'importance';
}

interface MemoryRecord {
  id: string;
  content: string;
  importance: number;
  layer: MemoryLayer;
  tags: string[];
  relatedIds: string[];
  createdAt: Date;
  updatedAt: Date;
}
```

### Session 管理

```typescript
interface SessionAPI {
  create(descriptor: SessionCreateRequest): Promise<SessionDescriptor>;
  get(sessionId: string): Promise<SessionDescriptor>;
  update(sessionId: string, updates: Partial<SessionUpdateRequest>): Promise<void>;
  close(sessionId: string, archive?: boolean, summary?: string): Promise<void>;
  list(filter?: SessionFilter): Promise<SessionDescriptor[]>;
  messageAdd(sessionId: string, message: SessionMessage): Promise<void>;
  messageList(sessionId: string, offset?: number, limit?: number): Promise<SessionMessage[]>;
  contextGet(sessionId: string): Promise<SessionContext>;
  contextSet(sessionId: string, context: Partial<SessionContext>): Promise<void>;
  stats(): Promise<SessionStatistics>;
}
```

**类型定义**：

```typescript
enum SessionState {
  ACTIVE = 0,
  PAUSED = 1,
  CLOSED = 2,
  ARCHIVED = 3,
}

enum MessageRole {
  USER = 0,
  ASSISTANT = 1,
  SYSTEM = 2,
  TOOL = 3,
}

interface SessionMessage {
  role: MessageRole;
  content: string;
  processingPath?: 'system1' | 'system2';
  processingTimeMs?: number;
  metadata?: Record<string, unknown>;
}

interface SessionContext {
  conversationSummary: string;
  currentTopic: string;
  userIntent: string;
  relatedMemoryIds: string[];
  activeTaskIds: string[];
}
```

### Telemetry 管理

```typescript
interface TelemetryAPI {
  logWrite(record: LogRecord): Promise<void>;
  logQuery(filter: LogQueryFilter): Promise<LogRecord[]>;
  metricRecord(datapoint: MetricDatapoint): Promise<void>;
  metricQuery(name: string, labels?: Record<string, string>, timeRange?: TimeRange): Promise<MetricDatapoint[]>;
  traceStart(operationName: string, options?: TraceStartOptions): Promise<TraceSpan>;
  traceEnd(spanId: string, status?: SpanStatus): Promise<void>;
  traceContextGet(): Promise<TraceContext>;
  traceInject(context: TraceContext): Promise<void>;
  export(options: ExportOptions): Promise<string>;
  stats(): Promise<TelemetryStatistics>;
}
```

**类型定义**：

```typescript
enum LogLevel {
  TRACE = 0,
  DEBUG = 1,
  INFO = 2,
  WARN = 3,
  ERROR = 4,
  FATAL = 5,
}

enum MetricType {
  COUNTER = 0,
  GAUGE = 1,
  HISTOGRAM = 2,
  SUMMARY = 3,
}

enum SpanKind {
  INTERNAL = 0,
  SERVER = 1,
  CLIENT = 2,
  PRODUCER = 3,
  CONSUMER = 4,
}

interface LogRecord {
  level: LogLevel;
  module: string;
  message: string;
  fields?: Record<string, unknown>;
  traceId?: string;
  spanId?: string;
}

interface MetricDatapoint {
  name: string;
  type: MetricType;
  value: number;
  labels?: Record<string, string>;
  timestamp?: Date;
}

interface TraceSpan {
  spanId: string;
  traceId: string;
  parentSpanId?: string;
  operationName: string;
  startTime: Date;
  endTime?: Date;
  durationUs?: number;
  status: SpanStatus;
  events?: SpanEvent[];
  attributes?: Record<string, unknown>;
  kind: SpanKind;
}
```

---

## 🔧 高级功能

### 追踪上下文管理

```typescript
// 自动追踪上下文传播
const span = await client.telemetry.traceStart('process_request', {
  parentSpanId: request.headers['x-span-id'],
  attributes: { 'http.method': 'POST' },
});

try {
  const result = await client.agents.invoke(agentId, input);
  await client.telemetry.traceEnd(span.spanId, SpanStatus.OK);
  return result;
} catch (error) {
  await client.telemetry.traceEnd(span.spanId, SpanStatus.ERROR);
  throw error;
}
```

### 错误处理

```typescript
import {
  AgentOSError,
  AgentNotFoundError,
  SessionNotFoundError,
  PermissionDeniedError,
  TimeoutError,
  ValidationError,
} from '@agentos/sdk';

try {
  const agent = await client.agents.get('agent_nonexistent');
} catch (error) {
  if (error instanceof AgentNotFoundError) {
    console.error(`Agent not found: ${error.agentId}`);
  } else if (error instanceof PermissionDeniedError) {
    console.error(`Permission denied: ${error.code}`);
  } else if (error instanceof TimeoutError) {
    console.error(`Operation timed out after ${error.timeoutMs}ms`);
  } else if (error instanceof AgentOSError) {
    console.error(`AgentOS error: ${error.code} - ${error.message}`);
  }
}
```

### 配置选项

```typescript
const client = new AgentOSClient({
  endpoint: 'http://localhost:8080',
  timeout: 30000,
  maxRetries: 3,
  retryDelay: 1000,
  apiKey: process.env.AGENTOS_API_KEY,
  telemetry: {
    enabled: true,
    samplingRate: 0.1,
    logLevel: LogLevel.INFO,
  },
  connection: {
    keepAlive: true,
    maxConnections: 10,
    compression: true,
  },
});
```

### 环境变量

| 变量名 | 描述 | 默认值 |
|--------|------|--------|
| `AGENTOS_ENDPOINT` | 服务端地址 | `http://localhost:8080` |
| `AGENTOS_API_KEY` | API 密钥 | - |
| `AGENTOS_TIMEOUT` | 请求超时(ms) | `30000` |
| `AGENTOS_LOG_LEVEL` | 日志级别 | `INFO` |
| `AGENTOS_TELEMETRY_ENABLED` | 启用遥测 | `true` |
| `AGENTOS_SAMPLING_RATE` | 追踪采样率 | `0.1` |

---

## 🧪 测试

```typescript
import { AgentOSClient } from '@agentos/sdk';
import { MockAgentOSClient } from '@agentos/sdk/testing';

// 使用 Mock 客户端进行单元测试
const mockClient = new MockAgentOSClient();
mockClient.agents.create.mockResolvedValue({
  id: 'agent_test_0',
  name: 'test-agent',
  state: AgentState.READY,
  type: AgentType.TASK,
  boundSkills: [],
  system1ExecCount: 0,
  system2ExecCount: 0,
  activeTaskCount: 0,
  memoryUsage: 0,
  cpuUsage: 0,
  createdAt: new Date(),
  updatedAt: new Date(),
});

const agent = await mockClient.agents.create({
  name: 'test-agent',
  type: AgentType.TASK,
});

expect(agent.id).toBe('agent_test_0');
expect(mockClient.agents.create).toHaveBeenCalledTimes(1);
```

---

## 📚 相关文档

| 文档 | 描述 |
|------|------|
| [Agent 管理 API](../../syscalls/agent.md) | Agent 生命周期管理 |
| [Memory 管理 API](../../syscalls/memory.md) | 记忆系统操作 |
| [Session 管理 API](../../syscalls/session.md) | 会话管理 |
| [Telemetry API](../../syscalls/telemetry.md) | 可观测性 |
| [Skill 管理 API](../../syscalls/skill.md) | 技能管理 |
| [Go SDK](../go/README.md) | Go 语言 SDK |
| [Python SDK](../python/README.md) | Python SDK |
| [Rust SDK](../rust/README.md) | Rust SDK |

---

**最后更新**: 2026-04-12  
**维护者**: AgentOS SDK 委员会 (作者: LirenWang)

---

© 2026 SPHARX Ltd. All Rights Reserved.  
*"From data intelligence emerges."*
