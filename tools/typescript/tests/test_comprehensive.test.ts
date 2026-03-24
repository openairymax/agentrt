// AgentOS TypeScript SDK Comprehensive Tests
// Version: 3.0.0
// Last updated: 2026-03-24

import {
  AgentOS,
  Client,
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
  AgentOSError,
  NetworkError,
  TimeoutError,
  TaskError,
  MemoryError,
  SessionError,
  SkillError,
  ErrorCode,
} from '../src';

let mockClient: MockClient;
let client: AgentOS;

beforeEach(() => {
  mockClient = new MockClient();
  client = new AgentOS([withEndpoint('http://localhost:18789')]);
});

afterEach(() => {
  client.close();
});

// ============================================================
// Client Tests
// ============================================================

describe('AgentOS Client', () => {
  test('should create client with default config', () => {
    const c = new AgentOS();
    const config = c.getConfig();
    expect(config.endpoint).toBe('http://localhost:18789');
  });

  test('should create client with custom endpoint', () => {
    const c = new AgentOS([withEndpoint('http://localhost:9999')]);
    const config = c.getConfig();
    expect(config.endpoint).toBe('http://localhost:9999');
  });

  test('should strip trailing slash from endpoint', () => {
    const c = new AgentOS([withEndpoint('http://localhost:9999/')]);
    const config = c.getConfig();
    expect(config.endpoint).toBe('http://localhost:9999');
  });

  test('should create client with API key', () => {
    const c = new AgentOS([withAPIKey('test-api-key')]);
    const config = c.getConfig();
    expect(config.apiKey).toBe('test-api-key');
  });

  test('should create client with timeout', () => {
    const c = new AgentOS([withTimeout(5000)]);
    const config = c.getConfig();
    expect(config.timeout).toBe(5000);
  });
});

// ============================================================
// Task Manager Tests
// ============================================================

describe('TaskManager', () => {
  test('should submit task', async () => {
    const mock = new MockClient();
    mock.setResponse('POST', '/api/v1/tasks', { data: { task_id: 'task-123' } });
    
    const taskManager = new TaskManager(mock);
    const task = await taskManager.submit('test task');
    
    expect(task.id).toBe('task-123');
  });

  test('should query task status', async () => {
    const mock = new MockClient();
    mock.setResponse('GET', '/api/v1/tasks/task-123', { 
      data: { task_id: 'task-123', status: 'running', description: 'test task' } 
    });
    
    const taskManager = new TaskManager(mock);
    const status = await taskManager.query('task-123');
    
    expect(status).toBe(TaskStatus.RUNNING);
  });

  test('should get task details', async () => {
    const mock = new MockClient();
    mock.setResponse('GET', '/api/v1/tasks/task-123', { 
      data: { task_id: 'task-123', status: 'running', description: 'test task' } 
    });
    
    const taskManager = new TaskManager(mock);
    const task = await taskManager.get('task-123');
    
    expect(task.id).toBe('task-123');
    expect(task.status).toBe(TaskStatus.RUNNING);
  });

  test('should cancel task', async () => {
    const mock = new MockClient();
    mock.setResponse('POST', '/api/v1/tasks/task-123/cancel', { data: {} });
    
    const taskManager = new TaskManager(mock);
    await taskManager.cancel('task-123');
    
    const log = mock.getRequestLog();
    expect(log.some(r => r.method === 'POST' && r.path === '/api/v1/tasks/task-123/cancel')).toBe(true);
  });
});

// ============================================================
// Memory Manager Tests
// ============================================================

describe('MemoryManager', () => {
  test('should write memory', async () => {
    const mock = new MockClient();
    mock.setResponse('POST', '/api/v1/memories', { data: { memory_id: 'mem-123' } });
    
    const memoryManager = new MemoryManager(mock);
    const memory = await memoryManager.write('test content', MemoryLayer.L1);
    
    expect(memory.id).toBe('mem-123');
  });

  test('should search memory', async () => {
    const mock = new MockClient();
    mock.setResponse('GET', '/api/v1/memories/search', { 
      data: { 
        memories: [{ memory_id: 'mem-1', content: 'test 1', layer: 'L1' }],
        total: 1
      } 
    });
    
    const memoryManager = new MemoryManager(mock);
    const result = await memoryManager.search('test query', 5);
    
    expect(result.memories).toHaveLength(1);
    expect(result.memories[0].id).toBe('mem-1');
    expect(result.total).toBe(1);
  });

  test('should get memory by ID', async () => {
    const mock = new MockClient();
    mock.setResponse('GET', '/api/v1/memories/mem-123', { 
      data: { memory_id: 'mem-123', content: 'test content', layer: 'L1' } 
    });
    
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
    expect(log.some(r => r.method === 'DELETE' && r.path === '/api/v1/memories/mem-123')).toBe(true);
  });
});

// ============================================================
// Session Manager Tests
// ============================================================

describe('SessionManager', () => {
  test('should create session', async () => {
    const mock = new MockClient();
    mock.setResponse('POST', '/api/v1/sessions', { data: { session_id: 'sess-123' } });
    
    const sessionManager = new SessionManager(mock);
    const session = await sessionManager.create('user-123');
    
    expect(session.id).toBe('sess-123');
  });

  test('should set context', async () => {
    const mock = new MockClient();
    mock.setResponse('PUT', '/api/v1/sessions/sess-123/context', { data: {} });
    
    const sessionManager = new SessionManager(mock);
    await sessionManager.setContext('sess-123', 'key', 'value');
    
    const log = mock.getRequestLog();
    expect(log.some(r => r.method === 'PUT' && r.path === '/api/v1/sessions/sess-123/context')).toBe(true);
  });

  test('should close session', async () => {
    const mock = new MockClient();
    mock.setResponse('DELETE', '/api/v1/sessions/sess-123', { data: {} });
    
    const sessionManager = new SessionManager(mock);
    await sessionManager.close('sess-123');
    
    const log = mock.getRequestLog();
    expect(log.some(r => r.method === 'DELETE' && r.path === '/api/v1/sessions/sess-123')).toBe(true);
  });
});

// ============================================================
// Skill Manager Tests
// ============================================================

describe('SkillManager', () => {
  test('should load skill', async () => {
    const mock = new MockClient();
    mock.setResponse('GET', '/api/v1/skills/my-skill', { 
      data: { name: 'my-skill', version: '1.0.0', description: 'Test skill' } 
    });
    
    const skillManager = new SkillManager(mock);
    const skill = await skillManager.load('my-skill');
    
    expect(skill.name).toBe('my-skill');
  });

  test('should execute skill', async () => {
    const mock = new MockClient();
    mock.setResponse('POST', '/api/v1/skills/my-skill/execute', { 
      data: { output: { result: 'success' } } 
    });
    
    const skillManager = new SkillManager(mock);
    const result = await skillManager.execute('my-skill', { param: 'value' });
    
    expect(result.output.result).toBe('success');
  });
});

// ============================================================
// Error Handling Tests
// ============================================================

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

// ============================================================
// Enum Tests
// ============================================================

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

// ============================================================
// Mock Client Tests
// ============================================================

describe('MockClient', () => {
  test('should return mock response', async () => {
    const mock = new MockClient();
    mock.setResponse('GET', '/test', { data: { value: 'test' } });
    
    const response = await mock.get('/test');
    
    expect(response.data.value).toBe('test');
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

// ============================================================
// Integration Tests
// ============================================================

describe('AgentOS Integration', () => {
  test('should access task manager', () => {
    const c = new AgentOS([withEndpoint('http://localhost:18789')]);
    expect(c.tasks).toBeInstanceOf(TaskManager);
  });

  test('should access memory manager', () => {
    const c = new AgentOS([withEndpoint('http://localhost:18789')]);
    expect(c.memories).toBeInstanceOf(MemoryManager);
  });

  test('should access session manager', () => {
    const c = new AgentOS([withEndpoint('http://localhost:18789')]);
    expect(c.sessions).toBeInstanceOf(SessionManager);
  });

  test('should access skill manager', () => {
    const c = new AgentOS([withEndpoint('http://localhost:18789')]);
    expect(c.skills).toBeInstanceOf(SkillManager);
  });

  test('should return API client', () => {
    const c = new AgentOS([withEndpoint('http://localhost:18789')]);
    expect(c.api).toBeDefined();
  });
});
