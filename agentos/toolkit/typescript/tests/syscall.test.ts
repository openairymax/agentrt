import {
  SyscallNamespace,
  SyscallBinding,
  TaskSyscall,
  MemorySyscall,
  SessionSyscall,
} from '../src/syscall';
import type { SyscallRequest, SyscallResponse } from '../src/syscall';

class MockSyscallBinding extends SyscallBinding {
  private responses: Map<string, SyscallResponse> = new Map();
  private requests: SyscallRequest[] = [];

  setResponse(namespace: SyscallNamespace, operation: string, response: SyscallResponse): void {
    this.responses.set(`${namespace}:${operation}`, response);
  }

  getRequests(): SyscallRequest[] {
    return [...this.requests];
  }

  async invoke(request: SyscallRequest): Promise<SyscallResponse> {
    this.requests.push(request);
    const key = `${request.namespace}:${request.operation}`;
    const resp = this.responses.get(key);
    if (resp) {
      return resp;
    }
    return { success: true, data: {} };
  }
}

describe('SyscallNamespace', () => {
  test('should have correct values', () => {
    expect(SyscallNamespace.TASK).toBe('task');
    expect(SyscallNamespace.MEMORY).toBe('memory');
    expect(SyscallNamespace.SESSION).toBe('session');
    expect(SyscallNamespace.TELEMETRY).toBe('telemetry');
  });
});

describe('SyscallBinding', () => {
  test('should be abstract class', () => {
    expect(SyscallBinding).toBeDefined();
  });
});

describe('TaskSyscall', () => {
  let binding: MockSyscallBinding;
  let syscall: TaskSyscall;

  beforeEach(() => {
    binding = new MockSyscallBinding();
    syscall = new TaskSyscall(binding);
  });

  test('should submit task', async () => {
    binding.setResponse(SyscallNamespace.TASK, 'submit', {
      success: true,
      data: { task_id: 'task-001' },
    });
    const resp = await syscall.submit('test task');
    expect(resp.success).toBe(true);
    expect(resp.data.task_id).toBe('task-001');

    const reqs = binding.getRequests();
    expect(reqs).toHaveLength(1);
    expect(reqs[0].namespace).toBe(SyscallNamespace.TASK);
    expect(reqs[0].operation).toBe('submit');
    expect(reqs[0].params).toEqual({ description: 'test task' });
  });

  test('should query task', async () => {
    binding.setResponse(SyscallNamespace.TASK, 'query', {
      success: true,
      data: { task_id: 'task-001', status: 'running' },
    });
    const resp = await syscall.query('task-001');
    expect(resp.success).toBe(true);

    const reqs = binding.getRequests();
    expect(reqs[0].params).toEqual({ task_id: 'task-001' });
  });

  test('should cancel task', async () => {
    binding.setResponse(SyscallNamespace.TASK, 'cancel', {
      success: true,
      data: {},
    });
    const resp = await syscall.cancel('task-001');
    expect(resp.success).toBe(true);

    const reqs = binding.getRequests();
    expect(reqs[0].operation).toBe('cancel');
    expect(reqs[0].params).toEqual({ task_id: 'task-001' });
  });
});

describe('MemorySyscall', () => {
  let binding: MockSyscallBinding;
  let syscall: MemorySyscall;

  beforeEach(() => {
    binding = new MockSyscallBinding();
    syscall = new MemorySyscall(binding);
  });

  test('should write memory', async () => {
    binding.setResponse(SyscallNamespace.MEMORY, 'write', {
      success: true,
      data: { memory_id: 'mem-001' },
    });
    const resp = await syscall.write('test content');
    expect(resp.success).toBe(true);

    const reqs = binding.getRequests();
    expect(reqs[0].operation).toBe('write');
    expect(reqs[0].params).toEqual({ content: 'test content', metadata: undefined });
  });

  test('should write memory with metadata', async () => {
    binding.setResponse(SyscallNamespace.MEMORY, 'write', {
      success: true,
      data: { memory_id: 'mem-001' },
    });
    const resp = await syscall.write('test content', { source: 'test' });
    expect(resp.success).toBe(true);

    const reqs = binding.getRequests();
    expect(reqs[0].params).toEqual({ content: 'test content', metadata: { source: 'test' } });
  });

  test('should search memory', async () => {
    binding.setResponse(SyscallNamespace.MEMORY, 'search', {
      success: true,
      data: { results: [] },
    });
    const resp = await syscall.search('query', 10);
    expect(resp.success).toBe(true);

    const reqs = binding.getRequests();
    expect(reqs[0].params).toEqual({ query: 'query', top_k: 10 });
  });

  test('should search memory with default topK', async () => {
    binding.setResponse(SyscallNamespace.MEMORY, 'search', {
      success: true,
      data: { results: [] },
    });
    await syscall.search('query');

    const reqs = binding.getRequests();
    expect(reqs[0].params).toEqual({ query: 'query', top_k: 5 });
  });

  test('should delete memory', async () => {
    binding.setResponse(SyscallNamespace.MEMORY, 'delete', {
      success: true,
      data: {},
    });
    const resp = await syscall.delete('mem-001');
    expect(resp.success).toBe(true);

    const reqs = binding.getRequests();
    expect(reqs[0].params).toEqual({ memory_id: 'mem-001' });
  });
});

describe('SessionSyscall', () => {
  let binding: MockSyscallBinding;
  let syscall: SessionSyscall;

  beforeEach(() => {
    binding = new MockSyscallBinding();
    syscall = new SessionSyscall(binding);
  });

  test('should create session', async () => {
    binding.setResponse(SyscallNamespace.SESSION, 'create', {
      success: true,
      data: { session_id: 'sess-001' },
    });
    const resp = await syscall.create();
    expect(resp.success).toBe(true);

    const reqs = binding.getRequests();
    expect(reqs[0].namespace).toBe(SyscallNamespace.SESSION);
    expect(reqs[0].operation).toBe('create');
    expect(reqs[0].params).toBeUndefined();
  });

  test('should set context', async () => {
    binding.setResponse(SyscallNamespace.SESSION, 'set_context', {
      success: true,
      data: {},
    });
    const resp = await syscall.setContext('sess-001', 'key', 'value');
    expect(resp.success).toBe(true);

    const reqs = binding.getRequests();
    expect(reqs[0].params).toEqual({ session_id: 'sess-001', key: 'key', value: 'value' });
  });

  test('should get context', async () => {
    binding.setResponse(SyscallNamespace.SESSION, 'get_context', {
      success: true,
      data: { value: 'stored-value' },
    });
    const resp = await syscall.getContext('sess-001', 'key');
    expect(resp.success).toBe(true);

    const reqs = binding.getRequests();
    expect(reqs[0].params).toEqual({ session_id: 'sess-001', key: 'key' });
  });
});
