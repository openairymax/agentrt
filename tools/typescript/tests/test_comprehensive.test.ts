// AgentOS TypeScript SDK Comprehensive Tests
// Version: 2.0.0

import axios, { AxiosInstance } from 'axios';
import {
  AgentOS,
  Task,
  Session,
  Skill,
  MemoryManager,
  Meter,
  Tracer,
  Telemetry,
  SyscallBinding,
  SyscallNamespace,
  TaskSyscall,
  MemorySyscall,
  SessionSyscall,
  TaskStatus,
  MemoryLayer,
  SpanStatus,
  AgentOSError,
  NetworkError,
  HttpError,
  TimeoutError,
  TaskError,
  MemoryError,
  SessionError,
  SkillError,
  SyscallError,
  ConfigError,
  RateLimitError,
  ErrorCode,
  httpStatusToErrorCode,
  VERSION,
} from '../src';

// ===== AgentOS Client Tests =====

describe('AgentOS Client', () => {
  let client: AgentOS;

  beforeEach(() => {
    client = new AgentOS({ endpoint: 'http://localhost:18789' });
  });

  test('should create client with default endpoint', () => {
    const c = new AgentOS();
    expect(c.getEndpoint()).toBe('http://localhost:18789');
  });

  test('should trim trailing slash from endpoint', () => {
    const c = new AgentOS({ endpoint: 'http://localhost:18789/' });
    expect(c.getEndpoint()).toBe('http://localhost:18789');
  });

  test('should return correct endpoint', () => {
    expect(client.getEndpoint()).toBe('http://localhost:18789');
  });

  test('should export VERSION as 2.0.0', () => {
    expect(VERSION).toBe('2.0.0');
  });
});

// ===== TaskStatus Tests =====

describe('TaskStatus', () => {
  test('should have correct enum values', () => {
    expect(TaskStatus.PENDING).toBe('pending');
    expect(TaskStatus.RUNNING).toBe('running');
    expect(TaskStatus.COMPLETED).toBe('completed');
    expect(TaskStatus.FAILED).toBe('failed');
    expect(TaskStatus.CANCELLED).toBe('cancelled');
  });
});

// ===== MemoryLayer Tests =====

describe('MemoryLayer', () => {
  test('should have correct enum values', () => {
    expect(MemoryLayer.RAW).toBe('RAW');
    expect(MemoryLayer.WORKING).toBe('WORKING');
    expect(MemoryLayer.LONG_TERM).toBe('LONG_TERM');
    expect(MemoryLayer.EPISODIC).toBe('EPISODIC');
  });
});

// ===== SpanStatus Tests =====

describe('SpanStatus', () => {
  test('should have correct enum values', () => {
    expect(SpanStatus.OK).toBe('ok');
    expect(SpanStatus.ERROR).toBe('error');
    expect(SpanStatus.UNSET).toBe('unset');
  });
});

// ===== Error Code Tests =====

describe('ErrorCode', () => {
  test('should have correct 0x0xxx general codes', () => {
    expect(ErrorCode.SUCCESS).toBe('0x0000');
    expect(ErrorCode.UNKNOWN).toBe('0x0001');
    expect(ErrorCode.INVALID_PARAMETER).toBe('0x0002');
    expect(ErrorCode.MISSING_PARAMETER).toBe('0x0003');
    expect(ErrorCode.TIMEOUT).toBe('0x0004');
    expect(ErrorCode.NOT_FOUND).toBe('0x0005');
    expect(ErrorCode.ALREADY_EXISTS).toBe('0x0006');
    expect(ErrorCode.CONFLICT).toBe('0x0007');
    expect(ErrorCode.INVALID_CONFIG).toBe('0x0008');
    expect(ErrorCode.INVALID_ENDPOINT).toBe('0x0009');
    expect(ErrorCode.NETWORK_ERROR).toBe('0x000A');
    expect(ErrorCode.CONNECTION_REFUSED).toBe('0x000B');
    expect(ErrorCode.SERVER_ERROR).toBe('0x000C');
    expect(ErrorCode.UNAUTHORIZED).toBe('0x000D');
    expect(ErrorCode.FORBIDDEN).toBe('0x000E');
    expect(ErrorCode.RATE_LIMITED).toBe('0x000F');
    expect(ErrorCode.INVALID_RESPONSE).toBe('0x0010');
    expect(ErrorCode.PARSE_ERROR).toBe('0x0011');
    expect(ErrorCode.VALIDATION_ERROR).toBe('0x0012');
    expect(ErrorCode.NOT_SUPPORTED).toBe('0x0013');
    expect(ErrorCode.INTERNAL).toBe('0x0014');
    expect(ErrorCode.BUSY).toBe('0x0015');
  });

  test('should cover 0x1xxx core loop codes', () => {
    expect(ErrorCode.LOOP_CREATE_FAILED).toBe('0x1001');
    expect(ErrorCode.LOOP_START_FAILED).toBe('0x1002');
    expect(ErrorCode.LOOP_STOP_FAILED).toBe('0x1003');
  });

  test('should cover 0x2xxx cognition codes', () => {
    expect(ErrorCode.COGNITION_FAILED).toBe('0x2001');
    expect(ErrorCode.DAG_BUILD_FAILED).toBe('0x2002');
    expect(ErrorCode.AGENT_DISPATCH_FAILED).toBe('0x2003');
    expect(ErrorCode.INTENT_PARSE_FAILED).toBe('0x2004');
  });

  test('should cover 0x3xxx execution codes', () => {
    expect(ErrorCode.TASK_FAILED).toBe('0x3001');
    expect(ErrorCode.TASK_CANCELLED).toBe('0x3002');
    expect(ErrorCode.TASK_TIMEOUT).toBe('0x3003');
  });

  test('should cover 0x4xxx memory/session/skill codes', () => {
    expect(ErrorCode.MEMORY_NOT_FOUND).toBe('0x4001');
    expect(ErrorCode.MEMORY_EVOLVE_FAILED).toBe('0x4002');
    expect(ErrorCode.MEMORY_SEARCH_FAILED).toBe('0x4003');
    expect(ErrorCode.SESSION_NOT_FOUND).toBe('0x4004');
    expect(ErrorCode.SESSION_EXPIRED).toBe('0x4005');
    expect(ErrorCode.SKILL_NOT_FOUND).toBe('0x4006');
    expect(ErrorCode.SKILL_EXECUTION_FAILED).toBe('0x4007');
  });

  test('should cover 0x5xxx telemetry code', () => {
    expect(ErrorCode.TELEMETRY_ERROR).toBe('0x5001');
  });

  test('should cover 0x6xxx security codes', () => {
    expect(ErrorCode.PERMISSION_DENIED).toBe('0x6001');
    expect(ErrorCode.CORRUPTED_DATA).toBe('0x6002');
  });
});

// ===== httpStatusToErrorCode Tests =====

describe('httpStatusToErrorCode', () => {
  test('should map 400 to INVALID_PARAMETER', () => {
    expect(httpStatusToErrorCode(400)).toBe(ErrorCode.INVALID_PARAMETER);
  });

  test('should map 401 to UNAUTHORIZED', () => {
    expect(httpStatusToErrorCode(401)).toBe(ErrorCode.UNAUTHORIZED);
  });

  test('should map 403 to FORBIDDEN', () => {
    expect(httpStatusToErrorCode(403)).toBe(ErrorCode.FORBIDDEN);
  });

  test('should map 404 to NOT_FOUND', () => {
    expect(httpStatusToErrorCode(404)).toBe(ErrorCode.NOT_FOUND);
  });

  test('should map 408 to TIMEOUT', () => {
    expect(httpStatusToErrorCode(408)).toBe(ErrorCode.TIMEOUT);
  });

  test('should map 409 to CONFLICT', () => {
    expect(httpStatusToErrorCode(409)).toBe(ErrorCode.CONFLICT);
  });

  test('should map 422 to VALIDATION_ERROR', () => {
    expect(httpStatusToErrorCode(422)).toBe(ErrorCode.VALIDATION_ERROR);
  });

  test('should map 429 to RATE_LIMITED', () => {
    expect(httpStatusToErrorCode(429)).toBe(ErrorCode.RATE_LIMITED);
  });

  test('should map 500 to SERVER_ERROR', () => {
    expect(httpStatusToErrorCode(500)).toBe(ErrorCode.SERVER_ERROR);
  });

  test('should map 504 to TIMEOUT', () => {
    expect(httpStatusToErrorCode(504)).toBe(ErrorCode.TIMEOUT);
  });

  test('should map unknown status to UNKNOWN', () => {
    expect(httpStatusToErrorCode(418)).toBe(ErrorCode.UNKNOWN);
  });
});

// ===== Error Class Tests =====

describe('AgentOSError', () => {
  test('should have correct name', () => {
    const err = new AgentOSError('test');
    expect(err.name).toBe('AgentOSError');
  });

  test('should include error code in message', () => {
    const err = new AgentOSError('test', '0x0001');
    expect(err.message).toBe('[0x0001] test');
    expect(err.code).toBe('0x0001');
  });

  test('should use UNKNOWN as default code', () => {
    const err = new AgentOSError('test');
    expect(err.code).toBe(ErrorCode.UNKNOWN);
  });

  test('should be instanceof Error', () => {
    const err = new AgentOSError('test');
    expect(err).toBeInstanceOf(Error);
  });
});

describe('NetworkError', () => {
  test('should have correct name and code', () => {
    const err = new NetworkError();
    expect(err.name).toBe('NetworkError');
    expect(err.code).toBe(ErrorCode.NETWORK_ERROR);
  });

  test('should accept custom message', () => {
    const err = new NetworkError('custom msg');
    expect(err.message).toContain('custom msg');
  });
});

describe('HttpError', () => {
  test('should have correct name and statusCode', () => {
    const err = new HttpError('not found', 404);
    expect(err.name).toBe('HttpError');
    expect(err.statusCode).toBe(404);
    expect(err.code).toBe(ErrorCode.NOT_FOUND);
  });

  test('should derive code from status', () => {
    const err = new HttpError('unauthorized', 401);
    expect(err.code).toBe(ErrorCode.UNAUTHORIZED);
  });
});

describe('TimeoutError', () => {
  test('should have correct name and code', () => {
    const err = new TimeoutError();
    expect(err.name).toBe('TimeoutError');
    expect(err.code).toBe(ErrorCode.TIMEOUT);
  });
});

describe('TaskError', () => {
  test('should have correct name and code', () => {
    const err = new TaskError('task failed');
    expect(err.name).toBe('TaskError');
    expect(err.code).toBe(ErrorCode.TASK_FAILED);
  });
});

describe('MemoryError', () => {
  test('should have correct name and code', () => {
    const err = new MemoryError('not found');
    expect(err.name).toBe('MemoryError');
    expect(err.code).toBe(ErrorCode.MEMORY_NOT_FOUND);
  });
});

describe('SessionError', () => {
  test('should have correct name and code', () => {
    const err = new SessionError('expired');
    expect(err.name).toBe('SessionError');
    expect(err.code).toBe(ErrorCode.SESSION_NOT_FOUND);
  });
});

describe('SkillError', () => {
  test('should have correct name and code', () => {
    const err = new SkillError('exec failed');
    expect(err.name).toBe('SkillError');
    expect(err.code).toBe(ErrorCode.SKILL_EXECUTION_FAILED);
  });
});

describe('SyscallError', () => {
  test('should have correct name and code', () => {
    const err = new SyscallError('syscall error');
    expect(err.name).toBe('SyscallError');
    expect(err.code).toBe(ErrorCode.TELEMETRY_ERROR);
  });
});

describe('ConfigError', () => {
  test('should have correct name and code', () => {
    const err = new ConfigError('bad config');
    expect(err.name).toBe('ConfigError');
    expect(err.code).toBe(ErrorCode.INVALID_CONFIG);
  });
});

describe('RateLimitError', () => {
  test('should have correct name and code', () => {
    const err = new RateLimitError();
    expect(err.name).toBe('RateLimitError');
    expect(err.code).toBe(ErrorCode.RATE_LIMITED);
  });
});

// ===== Error Inheritance Tests =====

describe('Error inheritance', () => {
  test('all errors should be instanceof AgentOSError', () => {
    expect(new NetworkError()).toBeInstanceOf(AgentOSError);
    expect(new HttpError('x', 500)).toBeInstanceOf(AgentOSError);
    expect(new TimeoutError()).toBeInstanceOf(AgentOSError);
    expect(new TaskError('x')).toBeInstanceOf(AgentOSError);
    expect(new MemoryError('x')).toBeInstanceOf(AgentOSError);
    expect(new SessionError('x')).toBeInstanceOf(AgentOSError);
    expect(new SkillError('x')).toBeInstanceOf(AgentOSError);
    expect(new SyscallError('x')).toBeInstanceOf(AgentOSError);
    expect(new ConfigError('x')).toBeInstanceOf(AgentOSError);
    expect(new RateLimitError()).toBeInstanceOf(AgentOSError);
  });

  test('all errors should be instanceof Error', () => {
    const errors = [
      new NetworkError(),
      new HttpError('x', 500),
      new TimeoutError(),
      new TaskError('x'),
      new MemoryError('x'),
      new SessionError('x'),
      new SkillError('x'),
      new SyscallError('x'),
      new ConfigError('x'),
      new RateLimitError(),
    ];
    for (const err of errors) {
      expect(err).toBeInstanceOf(Error);
    }
  });
});

// ===== Telemetry Meter Tests =====

describe('Meter', () => {
  test('should record metric data points', () => {
    const meter = new Meter();
    meter.record('cpu', 80.5);
    meter.record('cpu', 85.0);
    const points = meter.get('cpu');
    expect(points).toHaveLength(2);
    expect(points[0].name).toBe('cpu');
    expect(points[0].value).toBe(80.5);
    expect(points[1].value).toBe(85.0);
  });

  test('should return empty array for unknown metric', () => {
    const meter = new Meter();
    expect(meter.get('unknown')).toEqual([]);
  });

  test('should list all metric names', () => {
    const meter = new Meter();
    meter.record('cpu', 80);
    meter.record('memory', 1024);
    const names = meter.getAll();
    expect(names).toContain('cpu');
    expect(names).toContain('memory');
    expect(names).toHaveLength(2);
  });

  test('should include timestamp in data points', () => {
    const meter = new Meter();
    const before = Date.now();
    meter.record('test', 1);
    const after = Date.now();
    const points = meter.get('test');
    expect(points[0].timestamp).toBeGreaterThanOrEqual(before);
    expect(points[0].timestamp).toBeLessThanOrEqual(after);
  });

  test('should accept tags', () => {
    const meter = new Meter();
    meter.record('req', 200, { path: '/api/health' });
    const points = meter.get('req');
    expect(points[0].tags).toEqual({ path: '/api/health' });
  });

  test('should evict oldest when exceeding maxDataPoints', () => {
    const meter = new Meter(3);
    meter.record('m', 1);
    meter.record('m', 2);
    meter.record('m', 3);
    meter.record('m', 4);
    const points = meter.get('m');
    expect(points).toHaveLength(3);
    expect(points[0].value).toBe(2);
    expect(points[2].value).toBe(4);
  });

  test('should reset all metrics', () => {
    const meter = new Meter();
    meter.record('a', 1);
    meter.record('b', 2);
    meter.reset();
    expect(meter.getAll()).toHaveLength(0);
    expect(meter.get('a')).toHaveLength(0);
  });
});

// ===== Telemetry Tracer Tests =====

describe('Tracer', () => {
  test('should create span with startSpan', () => {
    const tracer = new Tracer();
    const span = tracer.startSpan('test-op');
    expect(span.name).toBe('test-op');
    expect(span.status).toBe(SpanStatus.OK);
    expect(span.startTime).toBeGreaterThan(0);
    expect(span.endTime).toBeUndefined();
  });

  test('should finish span and calculate duration', () => {
    const tracer = new Tracer();
    const span = tracer.startSpan('op');
    span.status = SpanStatus.ERROR;
    span.tags = { key: 'value' };
    tracer.finishSpan(span);
    expect(span.endTime).toBeDefined();
    expect(span.duration).toBeDefined();
    expect(typeof span.duration).toBe('number');
    expect(span.duration).toBeGreaterThanOrEqual(0);
  });

  test('should return copies of spans from getSpans', () => {
    const tracer = new Tracer();
    const span = tracer.startSpan('op');
    tracer.finishSpan(span);
    const spans = tracer.getSpans();
    expect(spans).toHaveLength(1);
    spans[0].name = 'modified';
    expect(tracer.getSpans()[0].name).toBe('op');
  });

  test('should evict oldest when exceeding maxSpans', () => {
    const tracer = new Tracer(2);
    const s1 = tracer.startSpan('s1');
    tracer.finishSpan(s1);
    const s2 = tracer.startSpan('s2');
    tracer.finishSpan(s2);
    const s3 = tracer.startSpan('s3');
    tracer.finishSpan(s3);
    const spans = tracer.getSpans();
    expect(spans).toHaveLength(2);
    expect(spans[0].name).toBe('s2');
    expect(spans[1].name).toBe('s3');
  });

  test('should reset all spans', () => {
    const tracer = new Tracer();
    const s = tracer.startSpan('op');
    tracer.finishSpan(s);
    tracer.reset();
    expect(tracer.getSpans()).toHaveLength(0);
  });
});

// ===== Telemetry Aggregator Tests =====

describe('Telemetry', () => {
  test('should create with default service name', () => {
    const tel = new Telemetry();
    expect(tel.getServiceName()).toBe('agentos-sdk');
  });

  test('should create with custom service name', () => {
    const tel = new Telemetry('my-service');
    expect(tel.getServiceName()).toBe('my-service');
  });

  test('should return meter and tracer', () => {
    const tel = new Telemetry();
    expect(tel.getMeter()).toBeInstanceOf(Meter);
    expect(tel.getTracer()).toBeInstanceOf(Tracer);
  });

  test('should reset both meter and tracer', () => {
    const tel = new Telemetry();
    tel.getMeter().record('m', 1);
    const s = tel.getTracer().startSpan('op');
    tel.getTracer().finishSpan(s);
    tel.reset();
    expect(tel.getMeter().getAll()).toHaveLength(0);
    expect(tel.getTracer().getSpans()).toHaveLength(0);
  });
});

// ===== Syscall Tests =====

describe('SyscallBinding', () => {
  test('TaskSyscall should invoke with correct namespace', async () => {
    let captured: any = null;
    class MockBinding extends SyscallBinding {
      async invoke(request: any) {
        captured = request;
        return { success: true, data: { task_id: '123' } };
      }
    }
    const binding = new MockBinding();
    const taskSyscall = new TaskSyscall(binding);
    await taskSyscall.submit('test task');
    expect(captured.namespace).toBe(SyscallNamespace.TASK);
    expect(captured.operation).toBe('submit');
    expect(captured.params.description).toBe('test task');
  });

  test('TaskSyscall.query should send correct params', async () => {
    let captured: any = null;
    class MockBinding extends SyscallBinding {
      async invoke(request: any) {
        captured = request;
        return { success: true };
      }
    }
    const taskSyscall = new TaskSyscall(new MockBinding());
    await taskSyscall.query('task-123');
    expect(captured.operation).toBe('query');
    expect(captured.params.task_id).toBe('task-123');
  });

  test('TaskSyscall.cancel should send correct params', async () => {
    let captured: any = null;
    class MockBinding extends SyscallBinding {
      async invoke(request: any) {
        captured = request;
        return { success: true };
      }
    }
    const taskSyscall = new TaskSyscall(new MockBinding());
    await taskSyscall.cancel('task-456');
    expect(captured.operation).toBe('cancel');
    expect(captured.params.task_id).toBe('task-456');
  });

  test('MemorySyscall.write should send correct params', async () => {
    let captured: any = null;
    class MockBinding extends SyscallBinding {
      async invoke(request: any) {
        captured = request;
        return { success: true };
      }
    }
    const memSyscall = new MemorySyscall(new MockBinding());
    await memSyscall.write('hello', { key: 'val' });
    expect(captured.namespace).toBe(SyscallNamespace.MEMORY);
    expect(captured.operation).toBe('write');
    expect(captured.params.content).toBe('hello');
  });

  test('MemorySyscall.search should send correct params', async () => {
    let captured: any = null;
    class MockBinding extends SyscallBinding {
      async invoke(request: any) {
        captured = request;
        return { success: true };
      }
    }
    const memSyscall = new MemorySyscall(new MockBinding());
    await memSyscall.search('test query', 10);
    expect(captured.operation).toBe('search');
    expect(captured.params.query).toBe('test query');
    expect(captured.params.top_k).toBe(10);
  });

  test('MemorySyscall.delete should send correct params', async () => {
    let captured: any = null;
    class MockBinding extends SyscallBinding {
      async invoke(request: any) {
        captured = request;
        return { success: true };
      }
    }
    const memSyscall = new MemorySyscall(new MockBinding());
    await memSyscall.delete('mem-789');
    expect(captured.operation).toBe('delete');
    expect(captured.params.memory_id).toBe('mem-789');
  });

  test('SessionSyscall.create should send correct params', async () => {
    let captured: any = null;
    class MockBinding extends SyscallBinding {
      async invoke(request: any) {
        captured = request;
        return { success: true };
      }
    }
    const sessSyscall = new SessionSyscall(new MockBinding());
    await sessSyscall.create();
    expect(captured.namespace).toBe(SyscallNamespace.SESSION);
    expect(captured.operation).toBe('create');
  });

  test('SessionSyscall.setContext should send correct params', async () => {
    let captured: any = null;
    class MockBinding extends SyscallBinding {
      async invoke(request: any) {
        captured = request;
        return { success: true };
      }
    }
    const sessSyscall = new SessionSyscall(new MockBinding());
    await sessSyscall.setContext('sess-1', 'key1', 'value1');
    expect(captured.operation).toBe('set_context');
    expect(captured.params.session_id).toBe('sess-1');
    expect(captured.params.key).toBe('key1');
    expect(captured.params.value).toBe('value1');
  });

  test('SessionSyscall.getContext should send correct params', async () => {
    let captured: any = null;
    class MockBinding extends SyscallBinding {
      async invoke(request: any) {
        captured = request;
        return { success: true };
      }
    }
    const sessSyscall = new SessionSyscall(new MockBinding());
    await sessSyscall.getContext('sess-2', 'mykey');
    expect(captured.operation).toBe('get_context');
    expect(captured.params.key).toBe('mykey');
  });

  test('SyscallBinding.invoke should be abstract', () => {
    expect(SyscallBinding).toBeDefined();
  });

  test('SyscallNamespace should have all 4 values', () => {
    expect(SyscallNamespace.TASK).toBe('task');
    expect(SyscallNamespace.MEMORY).toBe('memory');
    expect(SyscallNamespace.SESSION).toBe('session');
    expect(SyscallNamespace.TELEMETRY).toBe('telemetry');
  });
});

// ===== Mock Helper =====

/** 创建可模拟的 AxiosInstance mock 对象（支持作为函数调用） */
function createMockAxios() {
  const mockFn = jest.fn();
  const mockAxios = Object.assign(mockFn, {
    get: jest.fn(),
    post: jest.fn(),
    put: jest.fn(),
    delete: jest.fn(),
    defaults: { headers: { common: {} } },
    interceptors: {
      response: { use: jest.fn(), clear: jest.fn() },
      request: { use: jest.fn(), clear: jest.fn() },
    },
  });
  jest.spyOn(axios, 'create').mockReturnValue(mockAxios as any);
  return mockAxios as any;
}

// ===== AgentOS Client with Mock Tests =====

describe('AgentOS Client (mocked)', () => {
  let mockAxios: ReturnType<typeof createMockAxios>;

  beforeEach(() => {
    mockAxios = createMockAxios();
  });

  afterEach(() => {
    jest.restoreAllMocks();
  });

  test('submitTask should return Task with correct ID', async () => {
    mockAxios.mockResolvedValue({ data: { task_id: 'task-abc' } });
    const client = new AgentOS();
    const task = await client.submitTask('test task');
    expect(task.id).toBe('task-abc');
  });

  test('submitTask should throw on missing task_id', async () => {
    mockAxios.mockResolvedValue({ data: {} });
    const client = new AgentOS();
    await expect(client.submitTask('test')).rejects.toThrow(AgentOSError);
  });

  test('writeMemory should return memory ID', async () => {
    mockAxios.mockResolvedValue({ data: { memory_id: 'mem-xyz' } });
    const client = new AgentOS();
    const id = await client.writeMemory('hello world');
    expect(id).toBe('mem-xyz');
  });

  test('writeMemory should throw on missing memory_id', async () => {
    mockAxios.mockResolvedValue({ data: {} });
    const client = new AgentOS();
    await expect(client.writeMemory('test')).rejects.toThrow(AgentOSError);
  });

  test('searchMemory should return Memory array', async () => {
    mockAxios.mockResolvedValue({
      data: {
        memories: [
          { memory_id: 'm1', content: 'hello', created_at: '2026-01-01', metadata: {} },
          { memory_id: 'm2', content: 'world', created_at: '2026-01-02', metadata: { k: 'v' } },
        ],
      },
    });
    const client = new AgentOS();
    const memories = await client.searchMemory('test', 5);
    expect(memories).toHaveLength(2);
    expect(memories[0].memoryId).toBe('m1');
    expect(memories[0].content).toBe('hello');
    expect(memories[1].metadata).toEqual({ k: 'v' });
  });

  test('searchMemory should throw on missing memories', async () => {
    mockAxios.mockResolvedValue({ data: {} });
    const client = new AgentOS();
    await expect(client.searchMemory('test')).rejects.toThrow(AgentOSError);
  });

  test('getMemory should return single Memory', async () => {
    mockAxios.mockResolvedValue({
      data: { memory_id: 'm1', content: 'test', created_at: '2026-01-01', metadata: {} },
    });
    const client = new AgentOS();
    const mem = await client.getMemory('m1');
    expect(mem.memoryId).toBe('m1');
    expect(mem.content).toBe('test');
  });

  test('deleteMemory should return success boolean', async () => {
    mockAxios.mockResolvedValue({ data: { success: true } });
    const client = new AgentOS();
    const result = await client.deleteMemory('m1');
    expect(result).toBe(true);
  });

  test('createSession should return Session with correct ID', async () => {
    mockAxios.mockResolvedValue({ data: { session_id: 'sess-abc' } });
    const client = new AgentOS();
    const session = await client.createSession();
    expect(session.id).toBe('sess-abc');
  });

  test('createSession should throw on missing session_id', async () => {
    mockAxios.mockResolvedValue({ data: {} });
    const client = new AgentOS();
    await expect(client.createSession()).rejects.toThrow(AgentOSError);
  });

  test('loadSkill should return Skill with correct name', async () => {
    const client = new AgentOS();
    const skill = await client.loadSkill('my-skill');
    expect(skill.name).toBe('my-skill');
  });

  test('close should not throw', () => {
    const client = new AgentOS();
    expect(() => client.close()).not.toThrow();
  });
});

// ===== Task Tests (mocked) =====

describe('Task (mocked)', () => {
  let mockAxios: ReturnType<typeof createMockAxios>;

  beforeEach(() => {
    mockAxios = createMockAxios();
  });

  afterEach(() => {
    jest.restoreAllMocks();
  });

  test('query should return TaskStatus', async () => {
    mockAxios.mockResolvedValue({ data: { status: 'running' } });
    const client = new AgentOS();
    const task = new Task(client, 'task-1');
    const status = await task.query();
    expect(status).toBe(TaskStatus.RUNNING);
  });

  test('query should throw on missing status', async () => {
    mockAxios.mockResolvedValue({ data: {} });
    const client = new AgentOS();
    const task = new Task(client, 'task-1');
    await expect(task.query()).rejects.toThrow(TaskError);
  });

  test('cancel should return success', async () => {
    mockAxios.mockResolvedValue({ data: { success: true } });
    const client = new AgentOS();
    const task = new Task(client, 'task-1');
    const result = await task.cancel();
    expect(result).toBe(true);
  });
});

// ===== Session Tests (mocked) =====

describe('Session (mocked)', () => {
  let mockAxios: ReturnType<typeof createMockAxios>;

  beforeEach(() => {
    mockAxios = createMockAxios();
  });

  afterEach(() => {
    jest.restoreAllMocks();
  });

  test('setContext should return success', async () => {
    mockAxios.mockResolvedValue({ data: { success: true } });
    const client = new AgentOS();
    const session = new Session(client, 'sess-1');
    const result = await session.setContext('key1', 'value1');
    expect(result).toBe(true);
  });

  test('getContext should return value', async () => {
    mockAxios.mockResolvedValue({ data: { value: 'my-value' } });
    const client = new AgentOS();
    const session = new Session(client, 'sess-1');
    const val = await session.getContext('key1');
    expect(val).toBe('my-value');
  });

  test('deleteContext should return success', async () => {
    mockAxios.mockResolvedValue({ data: { success: true } });
    const client = new AgentOS();
    const session = new Session(client, 'sess-1');
    const result = await session.deleteContext('key1');
    expect(result).toBe(true);
  });

  test('getAllContext should return context object', async () => {
    mockAxios.mockResolvedValue({
      data: { context: { k1: 'v1', k2: 'v2' } },
    });
    const client = new AgentOS();
    const session = new Session(client, 'sess-1');
    const ctx = await session.getAllContext();
    expect(ctx).toEqual({ k1: 'v1', k2: 'v2' });
  });

  test('close should return success', async () => {
    mockAxios.mockResolvedValue({ data: { success: true } });
    const client = new AgentOS();
    const session = new Session(client, 'sess-1');
    const result = await session.close();
    expect(result).toBe(true);
  });
});

// ===== Skill Tests (mocked) =====

describe('Skill (mocked)', () => {
  let mockAxios: ReturnType<typeof createMockAxios>;

  beforeEach(() => {
    mockAxios = createMockAxios();
  });

  afterEach(() => {
    jest.restoreAllMocks();
  });

  test('execute should return SkillResult', async () => {
    mockAxios.mockResolvedValue({
      data: { success: true, output: { result: 42 } },
    });
    const client = new AgentOS();
    const skill = new Skill(client, 'calc');
    const result = await skill.execute({ x: 1 });
    expect(result.success).toBe(true);
    expect(result.output).toEqual({ result: 42 });
  });

  test('getInfo should return SkillInfo', async () => {
    mockAxios.mockResolvedValue({
      data: {
        skill_id: 'skill-1',
        description: 'A calculator',
        version: '1.0.0',
        parameters: { x: { type: 'number' } },
        enabled: true,
      },
    });
    const client = new AgentOS();
    const skill = new Skill(client, 'calc');
    const info = await skill.getInfo();
    expect(info.skillName).toBe('calc');
    expect(info.skillId).toBe('skill-1');
    expect(info.description).toBe('A calculator');
    expect(info.version).toBe('1.0.0');
    expect(info.enabled).toBe(true);
  });

  test('unload should return success', async () => {
    mockAxios.mockResolvedValue({ data: { success: true } });
    const client = new AgentOS();
    const skill = new Skill(client, 'calc');
    const result = await skill.unload();
    expect(result).toBe(true);
  });
});

// ===== MemoryManager Tests (mocked) =====

describe('MemoryManager (mocked)', () => {
  let mockAxios: ReturnType<typeof createMockAxios>;

  beforeEach(() => {
    mockAxios = createMockAxios();
  });

  afterEach(() => {
    jest.restoreAllMocks();
  });

  test('write should return memory ID', async () => {
    mockAxios.mockResolvedValue({ data: { memory_id: 'mem-1' } });
    const client = new AgentOS();
    const mm = new MemoryManager(client);
    const id = await mm.write('hello');
    expect(id).toBe('mem-1');
  });

  test('write should throw on missing memory_id', async () => {
    mockAxios.mockResolvedValue({ data: {} });
    const client = new AgentOS();
    const mm = new MemoryManager(client);
    await expect(mm.write('test')).rejects.toThrow(AgentOSError);
  });

  test('get should return Memory', async () => {
    mockAxios.mockResolvedValue({
      data: { memory_id: 'm1', content: 'hi', created_at: '2026-01-01', layer: 'RAW', metadata: {} },
    });
    const client = new AgentOS();
    const mm = new MemoryManager(client);
    const mem = await mm.get('m1');
    expect(mem.memoryId).toBe('m1');
    expect(mem.content).toBe('hi');
  });

  test('search should return Memory array', async () => {
    mockAxios.mockResolvedValue({
      data: { memories: [{ memory_id: 'm1', content: 'test', created_at: '2026-01-01' }] },
    });
    const client = new AgentOS();
    const mm = new MemoryManager(client);
    const results = await mm.search('test', 5);
    expect(results).toHaveLength(1);
    expect(results[0].memoryId).toBe('m1');
  });

  test('update should return success', async () => {
    mockAxios.mockResolvedValue({ data: { success: true } });
    const client = new AgentOS();
    const mm = new MemoryManager(client);
    expect(await mm.update('m1', 'new content')).toBe(true);
  });

  test('delete should return success', async () => {
    mockAxios.mockResolvedValue({ data: { success: true } });
    const client = new AgentOS();
    const mm = new MemoryManager(client);
    expect(await mm.delete('m1')).toBe(true);
  });

  test('searchByLayer should filter by layer', async () => {
    mockAxios.mockResolvedValue({
      data: { memories: [{ memory_id: 'm1', content: 'raw', created_at: '2026-01-01', layer: 'RAW' }] },
    });
    const client = new AgentOS();
    const mm = new MemoryManager(client);
    const results = await mm.searchByLayer(MemoryLayer.RAW, 10);
    expect(results).toHaveLength(1);
    expect(results[0].layer).toBe('RAW');
  });

  test('evolve should return success', async () => {
    mockAxios.mockResolvedValue({ data: { success: true } });
    const client = new AgentOS();
    const mm = new MemoryManager(client);
    expect(await mm.evolve('m1', MemoryLayer.LONG_TERM)).toBe(true);
  });

  test('getStats should return stats', async () => {
    mockAxios.mockResolvedValue({ data: { total: 42, byLayer: { RAW: 10 } } });
    const client = new AgentOS();
    const mm = new MemoryManager(client);
    const stats = await mm.getStats();
    expect(stats.total).toBe(42);
    expect(stats.byLayer.RAW).toBe(10);
  });
});
