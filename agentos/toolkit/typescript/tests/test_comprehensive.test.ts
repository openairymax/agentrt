// AgentOS TypeScript SDK Comprehensive Tests
// Version: 3.0.0
// Last updated: 2026-03-24

import {
  AgentOS,
  MockClient,
  TaskManager,
  MemoryManager,
  SessionManager,
  SkillManager,
  withEndpoint,
  withAPIKey,
  withTimeout,
  TaskStatus,
  MemoryLayer,
  TaskError,
  MemoryError,
  SessionError,
  SkillError,
  NetworkError,
  TimeoutError,
  ErrorCode,
} from '../src';

describe('AgentOS Client', () => {
  test('should create client with default manager', () => {
    const client = new AgentOS();
    const manager = client.getConfig();
    expect(manager.endpoint).toBe('http://localhost:18789');
    client.close();
  });

  test('should create client with custom endpoint', () => {
    const client = new AgentOS([withEndpoint('http://localhost:9999')]);
    const manager = client.getConfig();
    expect(manager.endpoint).toBe('http://localhost:9999');
    client.close();
  });

  test('should strip trailing slash from endpoint', () => {
    const client = new AgentOS([withEndpoint('http://localhost:9999/')]);
    const manager = client.getConfig();
    expect(manager.endpoint).toBe('http://localhost:9999');
    client.close();
  });

  test('should create client with API key', () => {
    const client = new AgentOS([withAPIKey('test-api-key')]);
    const manager = client.getConfig();
    expect(manager.apiKey).toBe('test-api-key');
    client.close();
  });

  test('should create client with timeout', () => {
    const client = new AgentOS([withTimeout(5000)]);
    const manager = client.getConfig();
    expect(manager.timeout).toBe(5000);
    client.close();
  });
});

describe('TaskManager', () => {
  test('should submit task', async () => {
    const mock = new MockClient();
    mock.setResponse('POST', '/api/v1/tasks', {
      success: true,
      data: { task_id: 'task-123' },
      message: 'OK',
    } as any);

    const taskManager = new TaskManager(mock);
    const task = await taskManager.submit('test task');

    expect(task.id).toBe('task-123');
  });

  test('should query task status', async () => {
    const mock = new MockClient();
    mock.setResponse('GET', '/api/v1/tasks/task-123', {
      success: true,
      data: { task_id: 'task-123', status: TaskStatus.RUNNING, description: 'test task' },
      message: 'OK',
    } as any);

    const taskManager = new TaskManager(mock);
    const status = await taskManager.query('task-123');

    expect(status).toBe(TaskStatus.RUNNING);
  });

  test('should get task details', async () => {
    const mock = new MockClient();
    mock.setResponse('GET', '/api/v1/tasks/task-123', {
      success: true,
      data: { task_id: 'task-123', status: TaskStatus.RUNNING, description: 'test task' },
      message: 'OK',
    } as any);

    const taskManager = new TaskManager(mock);
    const task = await taskManager.get('task-123');

    expect(task.id).toBe('task-123');
    expect(task.status).toBe(TaskStatus.RUNNING);
  });

  test('should cancel task', async () => {
    const mock = new MockClient();
    mock.setResponse('POST', '/api/v1/tasks/task-123/cancel', {
      success: true,
      data: {},
      message: 'OK',
    } as any);

    const taskManager = new TaskManager(mock);
    await taskManager.cancel('task-123');

    const log = mock.getRequestLog();
    expect(log.some((r) => r.method === 'POST' && r.path === '/api/v1/tasks/task-123/cancel')).toBe(true);
  });
});

describe('MemoryManager', () => {
  test('should write memory', async () => {
    const mock = new MockClient();
    mock.setResponse('POST', '/api/v1/memories', {
      success: true,
      data: { memory_id: 'mem-123' },
      message: 'OK',
    } as any);

    const memoryManager = new MemoryManager(mock);
    const memory = await memoryManager.write('test content', MemoryLayer.L1);

    expect(memory.id).toBe('mem-123');
  });

  test('should search memory', async () => {
    const mock = new MockClient();
    mock.setResponse('GET', '/api/v1/memories/search', {
      success: true,
      data: { memories: [{ memory_id: 'mem-1', content: 'test 1', layer: MemoryLayer.L1 }], total: 1 },
      message: 'OK',
    } as any);

    const memoryManager = new MemoryManager(mock);
    const result = await memoryManager.search('test query', 5);

    expect(result.memories).toHaveLength(1);
    expect(result.memories[0].id).toBe('mem-1');
    expect(result.total).toBe(1);
  });

  test('should get memory by ID', async () => {
    const mock = new MockClient();
    mock.setResponse('GET', '/api/v1/memories/mem-123', {
      success: true,
      data: { memory_id: 'mem-123', content: 'test content', layer: MemoryLayer.L1 },
      message: 'OK',
    } as any);

    const memoryManager = new MemoryManager(mock);
    const memory = await memoryManager.get('mem-123');

    expect(memory.id).toBe('mem-123');
    expect(memory.content).toBe('test content');
  });

  test('should delete memory', async () => {
    const mock = new MockClient();
    mock.setResponse('DELETE', '/api/v1/memories/mem-123', { data: {} });

    const memoryManager = new MemoryManager(mock);
    await memoryManager.delete('mem-123');

    const log = mock.getRequestLog();
    expect(log.some((r) => r.method === 'DELETE' && r.path === '/api/v1/memories/mem-123')).toBe(true);
  });
});

describe('SessionManager', () => {
  test('should create session', async () => {
    const mock = new MockClient();
    mock.setResponse('POST', '/api/v1/sessions', {
      success: true,
      data: { session_id: 'sess-123' },
      message: 'OK',
    } as any);

    const sessionManager = new SessionManager(mock);
    const session = await sessionManager.create('user-123');

    expect(session.id).toBe('sess-123');
  });

  test('should set context', async () => {
    const mock = new MockClient();
    mock.setResponse('POST', '/api/v1/sessions/sess-123/context', {
      success: true,
      data: {},
      message: 'OK',
    } as any);

    const sessionManager = new SessionManager(mock);
    await sessionManager.setContext('sess-123', 'key', 'value');

    const log = mock.getRequestLog();
    expect(log.some((r) => r.method === 'POST' && r.path === '/api/v1/sessions/sess-123/context')).toBe(true);
  });

  test('should close session', async () => {
    const mock = new MockClient();
    mock.setResponse('DELETE', '/api/v1/sessions/sess-123', {
      success: true,
      data: {},
      message: 'OK',
    } as any);

    const sessionManager = new SessionManager(mock);
    await sessionManager.close('sess-123');

    const log = mock.getRequestLog();
    expect(log.some((r) => r.method === 'DELETE' && r.path === '/api/v1/sessions/sess-123')).toBe(true);
  });
});

describe('SkillManager', () => {
  test('should load skill', async () => {
    const mock = new MockClient();
    mock.setResponse('POST', '/api/v1/skills/my-skill/load', {
      success: true,
      data: { name: 'my-skill', version: '1.0.0', description: 'Test skill' },
      message: 'OK',
    } as any);

    const skillManager = new SkillManager(mock);
    const skill = await skillManager.load('my-skill');

    expect(skill.name).toBe('my-skill');
  });

  test('should execute skill', async () => {
    const mock = new MockClient();
    mock.setResponse('POST', '/api/v1/skills/my-skill/execute', {
      success: true,
      data: { output: { result: 'success' } },
      message: 'OK',
    } as any);

    const skillManager = new SkillManager(mock);
    const result = await skillManager.execute('my-skill', { param: 'value' });

    expect((result.output as any).result).toBe('success');
  });
});

describe('Error Handling', () => {
  test('should throw TaskError with correct code', () => {
    const error = new TaskError('Task failed');
    expect(error.code).toBe(ErrorCode.TASK_FAILED);
    expect(error.message).toContain('Task failed');
  });

  test('should throw MemoryError with correct code', () => {
    const error = new MemoryError('Memory not found');
    expect(error.code).toBe(ErrorCode.MEMORY_NOT_FOUND);
    expect(error.message).toContain('Memory not found');
  });

  test('should throw SessionError with correct code', () => {
    const error = new SessionError('Session not found');
    expect(error.code).toBe(ErrorCode.SESSION_NOT_FOUND);
    expect(error.message).toContain('Session not found');
  });

  test('should throw SkillError with correct code', () => {
    const error = new SkillError('Skill not found');
    expect(error.code).toBe(ErrorCode.SKILL_EXECUTION_FAILED);
    expect(error.message).toContain('Skill not found');
  });

  test('should throw NetworkError with correct code', () => {
    const error = new NetworkError('Connection refused');
    expect(error.code).toBe(ErrorCode.NETWORK_ERROR);
    expect(error.message).toContain('Connection refused');
  });

  test('should throw TimeoutError with correct code', () => {
    const error = new TimeoutError('Request timeout');
    expect(error.code).toBe(ErrorCode.TIMEOUT);
    expect(error.message).toContain('Request timeout');
  });
});

describe('Enums', () => {
  test('TaskStatus values', () => {
    expect(TaskStatus.PENDING).toBe('pending');
    expect(TaskStatus.RUNNING).toBe('running');
    expect(TaskStatus.COMPLETED).toBe('completed');
    expect(TaskStatus.FAILED).toBe('failed');
    expect(TaskStatus.CANCELLED).toBe('cancelled');
  });

  test('MemoryLayer values', () => {
    expect(MemoryLayer.L1).toBe('L1');
    expect(MemoryLayer.L2).toBe('L2');
    expect(MemoryLayer.L3).toBe('L3');
    expect(MemoryLayer.L4).toBe('L4');
  });
});

describe('MockClient', () => {
  test('should return mock response', async () => {
    const mock = new MockClient();
    mock.setResponse('GET', '/test', {
      success: true,
      data: { value: 'test' },
      message: 'OK',
    } as any);

    const response = await mock.get('/test');

    expect((response as any).data.value).toBe('test');
  });

  test('should record requests', async () => {
    const mock = new MockClient();
    await mock.get('/test1');
    await mock.post('/test2', { data: 'value' });

    const log = mock.getRequestLog();

    expect(log).toHaveLength(2);
  });

  test('should reset', async () => {
    const mock = new MockClient();
    await mock.get('/test');

    mock.reset();

    expect(mock.getRequestLog()).toHaveLength(0);
  });
});

describe('AgentOS Integration', () => {
  test('should access task manager', () => {
    const client = new AgentOS([withEndpoint('http://localhost:18789')]);
    expect(client.tasks).toBeInstanceOf(TaskManager);
    client.close();
  });

  test('should access memory manager', () => {
    const client = new AgentOS([withEndpoint('http://localhost:18789')]);
    expect(client.memories).toBeInstanceOf(MemoryManager);
    client.close();
  });

  test('should access session manager', () => {
    const client = new AgentOS([withEndpoint('http://localhost:18789')]);
    expect(client.sessions).toBeInstanceOf(SessionManager);
    client.close();
  });

  test('should access skill manager', () => {
    const client = new AgentOS([withEndpoint('http://localhost:18789')]);
    expect(client.skills).toBeInstanceOf(SkillManager);
    client.close();
  });

  test('should return API client', () => {
    const client = new AgentOS([withEndpoint('http://localhost:18789')]);
    expect(client.api).toBeDefined();
    client.close();
  });
});
