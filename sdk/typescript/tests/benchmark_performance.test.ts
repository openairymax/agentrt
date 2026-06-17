/**
 * AgentOS TypeScript SDK Performance Benchmarks
 *
 * This file contains performance benchmark tests to ensure
 * the SDK meets production performance standards.
 */

import { MockClient } from '../src/client/mock';
import { TaskManager } from '../src/modules/task';
import { MemoryManager } from '../src/modules/memory';
import { SessionManager } from '../src/modules/session';
import { SkillManager } from '../src/modules/skill';
import { MemoryLayer } from '../src/types';

const PERFORMANCE_THRESHOLDS = {
  maxTaskSubmitTime: 100,
  maxTaskGetTime: 50,
  maxTaskListTime: 50,
  maxMemoryWriteTime: 50,
  maxMemoryGetTime: 50,
  maxMemorySearchTime: 50,
  maxSessionCreateTime: 100,
  maxSkillGetTime: 50,
  maxBulkOperationTime: 200,
};

describe('AgentOS Performance Benchmarks', () => {
  describe('Task Operations', () => {
    it('should submit task within threshold', async () => {
      const mock = new MockClient();
      mock.setResponse('POST', '/api/v1/tasks', {
        success: true,
        data: { task_id: 'test-task-id' },
        message: 'OK',
      } as any);
      const taskManager = new TaskManager(mock);

      const startTime = performance.now();
      await taskManager.submit('Performance test task');
      const endTime = performance.now();
      const duration = endTime - startTime;

      expect(duration).toBeLessThan(PERFORMANCE_THRESHOLDS.maxTaskSubmitTime);
    });

    it('should get task within threshold', async () => {
      const mock = new MockClient();
      mock.setResponse('GET', '/api/v1/tasks/test-task-id', {
        success: true,
        data: { task_id: 'test-task-id', status: 'pending' },
        message: 'OK',
      } as any);
      const taskManager = new TaskManager(mock);

      const startTime = performance.now();
      await taskManager.get('test-task-id');
      const endTime = performance.now();
      const duration = endTime - startTime;

      expect(duration).toBeLessThan(PERFORMANCE_THRESHOLDS.maxTaskGetTime);
    });

    it('should list tasks within threshold', async () => {
      const mock = new MockClient();
      mock.setResponse('GET', '/api/v1/tasks', {
        success: true,
        data: { tasks: [] },
        message: 'OK',
      } as any);
      const taskManager = new TaskManager(mock);

      const startTime = performance.now();
      await taskManager.list();
      const endTime = performance.now();
      const duration = endTime - startTime;

      expect(duration).toBeLessThan(PERFORMANCE_THRESHOLDS.maxTaskListTime);
    });

    it('should handle concurrent task operations', async () => {
      const mock = new MockClient();
      mock.setResponse('POST', '/api/v1/tasks', {
        success: true,
        data: { task_id: 'test-task-id' },
        message: 'OK',
      } as any);
      const taskManager = new TaskManager(mock);

      const startTime = performance.now();
      await Promise.all([
        taskManager.submit('concurrent task 1'),
        taskManager.submit('concurrent task 2'),
        taskManager.submit('concurrent task 3'),
        taskManager.submit('concurrent task 4'),
        taskManager.submit('concurrent task 5'),
      ]);
      const endTime = performance.now();
      const duration = endTime - startTime;

      expect(duration).toBeLessThan(PERFORMANCE_THRESHOLDS.maxBulkOperationTime);
    });
  });

  describe('Memory Operations', () => {
    it('should write memory within threshold', async () => {
      const mock = new MockClient();
      mock.setResponse('POST', '/api/v1/memories', {
        success: true,
        data: { memory_id: 'test-memory-id' },
        message: 'OK',
      } as any);
      const memoryManager = new MemoryManager(mock);

      const startTime = performance.now();
      await memoryManager.write('Test memory content for performance', MemoryLayer.L2);
      const endTime = performance.now();
      const duration = endTime - startTime;

      expect(duration).toBeLessThan(PERFORMANCE_THRESHOLDS.maxMemoryWriteTime);
    });

    it('should get memory within threshold', async () => {
      const mock = new MockClient();
      mock.setResponse('GET', '/api/v1/memories/test-memory-id', {
        success: true,
        data: { memory_id: 'test-memory-id', content: 'test', layer: 'L2' },
        message: 'OK',
      } as any);
      const memoryManager = new MemoryManager(mock);

      const startTime = performance.now();
      await memoryManager.get('test-memory-id');
      const endTime = performance.now();
      const duration = endTime - startTime;

      expect(duration).toBeLessThan(PERFORMANCE_THRESHOLDS.maxMemoryGetTime);
    });

    it('should search memory within threshold', async () => {
      const mock = new MockClient();
      mock.setResponse('GET', '/api/v1/memories/search', {
        success: true,
        data: { memories: [], total: 0 },
        message: 'OK',
      } as any);
      const memoryManager = new MemoryManager(mock);

      const startTime = performance.now();
      await memoryManager.search('test query');
      const endTime = performance.now();
      const duration = endTime - startTime;

      expect(duration).toBeLessThan(PERFORMANCE_THRESHOLDS.maxMemorySearchTime);
    });
  });

  describe('Session Operations', () => {
    it('should create session within threshold', async () => {
      const mock = new MockClient();
      mock.setResponse('POST', '/api/v1/sessions', {
        success: true,
        data: { session_id: 'test-session-id' },
        message: 'OK',
      } as any);
      const sessionManager = new SessionManager(mock);

      const startTime = performance.now();
      await sessionManager.create('test-user');
      const endTime = performance.now();
      const duration = endTime - startTime;

      expect(duration).toBeLessThan(PERFORMANCE_THRESHOLDS.maxSessionCreateTime);
    });

    it('should get session within threshold', async () => {
      const mock = new MockClient();
      mock.setResponse('GET', '/api/v1/sessions/test-session-id', {
        success: true,
        data: { session_id: 'test-session-id', user_id: 'test-user' },
        message: 'OK',
      } as any);
      const sessionManager = new SessionManager(mock);

      const startTime = performance.now();
      await sessionManager.get('test-session-id');
      const endTime = performance.now();
      const duration = endTime - startTime;

      expect(duration).toBeLessThan(PERFORMANCE_THRESHOLDS.maxSessionCreateTime);
    });
  });

  describe('Skill Operations', () => {
    it('should get skill within threshold', async () => {
      const mock = new MockClient();
      mock.setResponse('GET', '/api/v1/skills/test-skill-id', {
        success: true,
        data: { skill_id: 'test-skill-id', name: 'test skill' },
        message: 'OK',
      } as any);
      const skillManager = new SkillManager(mock);

      const startTime = performance.now();
      await skillManager.get('test-skill-id');
      const endTime = performance.now();
      const duration = endTime - startTime;

      expect(duration).toBeLessThan(PERFORMANCE_THRESHOLDS.maxSkillGetTime);
    });

    it('should list skills within threshold', async () => {
      const mock = new MockClient();
      mock.setResponse('GET', '/api/v1/skills', {
        success: true,
        data: { skills: [] },
        message: 'OK',
      } as any);
      const skillManager = new SkillManager(mock);

      const startTime = performance.now();
      await skillManager.list();
      const endTime = performance.now();
      const duration = endTime - startTime;

      expect(duration).toBeLessThan(PERFORMANCE_THRESHOLDS.maxSkillGetTime);
    });
  });
});