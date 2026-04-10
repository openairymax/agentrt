import { MockClient } from '../src/client/mock';
import {
  BaseManager,
  ResourceConverter,
  TaskConverter,
  MemoryConverter,
  SessionConverter,
  SkillConverter,
} from '../src/modules/base_manager';
import { APIClient } from '../src/client/client';
import { AgentOSError, ErrorCode } from '../src/errors';

class TestResource {
  constructor(public id: string, public name: string) {}
}

class TestConverter implements ResourceConverter<TestResource> {
  convert(data: Record<string, unknown>, context?: string): TestResource {
    const id = data.id as string;
    if (!id) {
      throw new AgentOSError(ErrorCode.InvalidResponse, context || 'missing id');
    }
    return new TestResource(id, (data.name as string) || '');
  }
}

class TestManager extends BaseManager<TestResource> {
  constructor(api: APIClient) {
    super(api, 'test', new TestConverter());
  }

  async getItem(id: string): Promise<TestResource> {
    return this.executeGet(`/api/v1/tests/${id}`, '获取测试资源失败');
  }

  async createItem(data: Record<string, unknown>): Promise<TestResource> {
    return this.executePost('/api/v1/tests', data, '创建测试资源失败');
  }

  async updateItem(id: string, data: Record<string, unknown>): Promise<TestResource> {
    return this.executePut(`/api/v1/tests/${id}`, data, '更新测试资源失败');
  }

  async deleteItem(id: string): Promise<void> {
    return this.executeDelete(`/api/v1/tests/${id}`, '删除测试资源失败');
  }
}

describe('BaseManager', () => {
  let mock: MockClient;
  let manager: TestManager;

  beforeEach(() => {
    mock = new MockClient();
    manager = new TestManager(mock);
  });

  test('should return API client', () => {
    expect(manager.getAPI()).toBe(mock);
  });

  test('should return resource type', () => {
    expect(manager.getResourceType()).toBe('test');
  });

  test('should execute GET request', async () => {
    mock.setResponse('GET', '/api/v1/tests/123', {
      success: true,
      data: { id: '123', name: 'test-item' },
    } as any);

    const result = await manager.getItem('123');
    expect(result.id).toBe('123');
    expect(result.name).toBe('test-item');
  });

  test('should execute POST request', async () => {
    mock.setResponse('POST', '/api/v1/tests', {
      success: true,
      data: { id: '456', name: 'new-item' },
    } as any);

    const result = await manager.createItem({ name: 'new-item' });
    expect(result.id).toBe('456');
    expect(result.name).toBe('new-item');
  });

  test('should execute PUT request', async () => {
    mock.setResponse('PUT', '/api/v1/tests/123', {
      success: true,
      data: { id: '123', name: 'updated' },
    } as any);

    const result = await manager.updateItem('123', { name: 'updated' });
    expect(result.name).toBe('updated');
  });

  test('should execute DELETE request', async () => {
    mock.setResponse('DELETE', '/api/v1/tests/123', {
      success: true,
      data: {},
    } as any);

    await expect(manager.deleteItem('123')).resolves.toBeUndefined();
  });

  test('should throw on GET with invalid response', async () => {
    mock.setResponse('GET', '/api/v1/tests/999', {
      success: false,
      data: null,
    } as any);

    await expect(manager.getItem('999')).rejects.toThrow();
  });

  test('should build list options with pagination', () => {
    const params = manager.buildListOptions({
      pagination: { page: 2, pageSize: 10 },
    });
    expect(params).toContain('page=2');
    expect(params).toContain('page_size=10');
  });

  test('should build list options with sort', () => {
    const params = manager.buildListOptions({
      sort: { field: 'name', order: 'asc' },
    });
    expect(params).toContain('sort_by=name');
    expect(params).toContain('sort_order=asc');
  });

  test('should build list options with filter', () => {
    const params = manager.buildListOptions({
      filter: { status: 'active', priority: 1 },
    });
    expect(params).toContain('status=active');
  });

  test('should return empty array for no options', () => {
    const params = manager.buildListOptions();
    expect(params).toEqual([]);
  });

  test('should log operation', () => {
    const logger = jest.spyOn(require('../src/utils/logger').getLogger(), 'debug');
    manager.logOperation('test-op', 'res-123');
    expect(logger).toHaveBeenCalledWith('[test] test-op: ID=res-123');
    logger.mockRestore();
  });

  test('should log error', () => {
    const logger = jest.spyOn(require('../src/utils/logger').getLogger(), 'error');
    manager.logError('test-op', new Error('test error'));
    expect(logger).toHaveBeenCalledWith('[test] test-op failed: test error');
    logger.mockRestore();
  });

  test('should log warning', () => {
    const logger = jest.spyOn(require('../src/utils/logger').getLogger(), 'warn');
    manager.logWarning('test warning');
    expect(logger).toHaveBeenCalledWith('[test] test warning');
    logger.mockRestore();
  });

  test('should log info', () => {
    const logger = jest.spyOn(require('../src/utils/logger').getLogger(), 'info');
    manager.logInfo('test info');
    expect(logger).toHaveBeenCalledWith('[test] test info');
    logger.mockRestore();
  });
});

describe('TaskConverter', () => {
  test('should convert task data', () => {
    const converter = new TaskConverter();
    const result = converter.convert({
      task_id: 'task-001',
      description: 'test task',
      status: 'running',
      priority: 5,
    });
    expect(result.id).toBe('task-001');
    expect(result.description).toBe('test task');
    expect(result.status).toBe('running');
    expect(result.priority).toBe(5);
  });

  test('should throw for missing task_id', () => {
    const converter = new TaskConverter();
    expect(() => converter.convert({ description: 'no id' })).toThrow(AgentOSError);
  });

  test('should use defaults for missing fields', () => {
    const converter = new TaskConverter();
    const result = converter.convert({ task_id: 'task-001' });
    expect(result.description).toBe('');
    expect(result.priority).toBe(0);
  });
});

describe('MemoryConverter', () => {
  test('should convert memory data', () => {
    const converter = new MemoryConverter();
    const result = converter.convert({
      memory_id: 'mem-001',
      content: 'test content',
      layer: 'L2',
      score: 0.95,
    });
    expect(result.id).toBe('mem-001');
    expect(result.content).toBe('test content');
    expect(result.layer).toBe('L2');
    expect(result.score).toBe(0.95);
  });

  test('should throw for missing memory_id', () => {
    const converter = new MemoryConverter();
    expect(() => converter.convert({ content: 'no id' })).toThrow(AgentOSError);
  });

  test('should use defaults for missing fields', () => {
    const converter = new MemoryConverter();
    const result = converter.convert({ memory_id: 'mem-001' });
    expect(result.content).toBe('');
    expect(result.layer).toBe('L1');
    expect(result.score).toBe(1.0);
  });
});

describe('SessionConverter', () => {
  test('should convert session data', () => {
    const converter = new SessionConverter();
    const result = converter.convert({
      session_id: 'sess-001',
      status: 'active',
      context: { key: 'value' },
    });
    expect(result.id).toBe('sess-001');
    expect(result.status).toBe('active');
    expect(result.context).toEqual({ key: 'value' });
  });

  test('should throw for missing session_id', () => {
    const converter = new SessionConverter();
    expect(() => converter.convert({ status: 'active' })).toThrow(AgentOSError);
  });

  test('should use defaults for missing fields', () => {
    const converter = new SessionConverter();
    const result = converter.convert({ session_id: 'sess-001' });
    expect(result.status).toBe('active');
    expect(result.context).toEqual({});
  });
});

describe('SkillConverter', () => {
  test('should convert skill data', () => {
    const converter = new SkillConverter();
    const result = converter.convert({
      skill_name: 'my-skill',
      description: 'test skill',
      status: 'available',
      loaded: true,
    });
    expect(result.name).toBe('my-skill');
    expect(result.description).toBe('test skill');
    expect(result.status).toBe('available');
    expect(result.loaded).toBe(true);
  });

  test('should throw for missing skill_name', () => {
    const converter = new SkillConverter();
    expect(() => converter.convert({ description: 'no name' })).toThrow(AgentOSError);
  });

  test('should use defaults for missing fields', () => {
    const converter = new SkillConverter();
    const result = converter.convert({ skill_name: 'my-skill' });
    expect(result.description).toBe('');
    expect(result.status).toBe('available');
    expect(result.loaded).toBe(false);
  });
});
