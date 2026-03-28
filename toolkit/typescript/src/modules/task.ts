// AgentOS TypeScript SDK - Task Manager Module
// Version: 3.0.0
// Last updated: 2026-03-24
//
// 提供任务的提交、查询、等待、取消、列表等生命周期管理功能。
// 与 Go SDK modules/task/manager.go 保持一致。

import { APIClient } from '../client';
import {
  Task,
  TaskResult,
  TaskStatus,
  ListOptions,
  APIResponse,
} from '../types';
import { AgentOSError, ErrorCode } from '../errors';
import {
  extractDataMap,
  getString,
  getInt64,
  getMap,
  parseTime,
  buildListPath,
  parseList,
} from '../utils';
import { DEFAULT_POLL_INTERVAL_MS } from '../manager';

/**
 * TaskManager 管理任务完整生命周期
 */
export class TaskManager {
  private api: APIClient;

  /**
   * 创建新的任务管理器实例
   * @param api - API 客户端
   */
  constructor(api: APIClient) {
    this.api = api;
  }

  /**
   * 提交新的执行任务
   * @param description - 任务描述
   */
  async submit(description: string): Promise<Task> {
    if (!description) {
      throw new AgentOSError('任务描述不能为空', ErrorCode.MISSING_PARAMETER);
    }

    const resp = await this.api.post<APIResponse<{ task_id: string }>>(
      '/api/v1/tasks',
      { description },
    );

    const data = extractDataMap(resp);
    if (!data) {
      throw new AgentOSError('任务创建响应格式异常', ErrorCode.INVALID_RESPONSE);
    }

    return {
      id: getString(data, 'task_id'),
      description,
      status: TaskStatus.PENDING,
      priority: 0,
      createdAt: new Date(),
      updatedAt: new Date(),
    };
  }

  /**
   * 使用扩展选项提交任务
   * @param description - 任务描述
   * @param priority - 优先级
   * @param metadata - 元数据
   */
  async submitWithOptions(
    description: string,
    priority: number,
    metadata?: Record<string, unknown>,
  ): Promise<Task> {
    if (!description) {
      throw new AgentOSError('任务描述不能为空', ErrorCode.MISSING_PARAMETER);
    }

    const body: Record<string, unknown> = { description, priority };
    if (metadata) {
      body['metadata'] = metadata;
    }

    const resp = await this.api.post<APIResponse<{ task_id: string }>>(
      '/api/v1/tasks',
      body,
    );

    const data = extractDataMap(resp);
    if (!data) {
      throw new AgentOSError('任务创建响应格式异常', ErrorCode.INVALID_RESPONSE);
    }

    return {
      id: getString(data, 'task_id'),
      description,
      priority,
      status: TaskStatus.PENDING,
      metadata,
      createdAt: new Date(),
      updatedAt: new Date(),
    };
  }

  /**
   * 获取指定任务的详细信息
   * @param taskId - 任务 ID
   */
  async get(taskId: string): Promise<Task> {
    if (!taskId) {
      throw new AgentOSError('任务ID不能为空', ErrorCode.MISSING_PARAMETER);
    }

    const resp = await this.api.get<APIResponse>(`/api/v1/tasks/${taskId}`);
    const data = extractDataMap(resp);
    if (!data) {
      throw new AgentOSError('任务详情响应格式异常', ErrorCode.INVALID_RESPONSE);
    }

    return this.parseTaskFromMap(data);
  }

  /**
   * 查询任务的当前状态
   * @param taskId - 任务 ID
   */
  async query(taskId: string): Promise<TaskStatus> {
    const task = await this.get(taskId);
    return task.status;
  }

  /**
   * 阻塞等待任务到达终态，支持超时控制
   * @param taskId - 任务 ID
   * @param timeout - 超时时间（毫秒）
   */
  async wait(taskId: string, timeout?: number): Promise<TaskResult> {
    const start = Date.now();

    while (true) {
      const status = await this.query(taskId);

      if (
        status === TaskStatus.COMPLETED ||
        status === TaskStatus.FAILED ||
        status === TaskStatus.CANCELLED
      ) {
        const task = await this.get(taskId);
        return {
          id: task.id,
          status: task.status,
          output: task.output,
          error: task.error,
          startTime: new Date(start),
          endTime: new Date(),
          duration: (Date.now() - start) / 1000,
        };
      }

      if (timeout && timeout > 0 && Date.now() - start > timeout) {
        throw new AgentOSError(`任务 ${taskId} 超时`, ErrorCode.TASK_TIMEOUT);
      }

      await this.sleep(DEFAULT_POLL_INTERVAL_MS);
    }
  }

  /**
   * 取消正在执行的任务
   * @param taskId - 任务 ID
   */
  async cancel(taskId: string): Promise<void> {
    if (!taskId) {
      throw new AgentOSError('任务ID不能为空', ErrorCode.MISSING_PARAMETER);
    }
    await this.api.post(`/api/v1/tasks/${taskId}/cancel`);
  }

  /**
   * 列出任务，支持分页和过滤
   * @param opts - 列表选项
   */
  async list(opts?: ListOptions): Promise<Task[]> {
    const path = buildListPath('/api/v1/tasks', opts);
    const resp = await this.api.get<APIResponse>(path);
    return parseList(resp, 'tasks', (item) => this.parseTaskFromMap(item), '任务列表响应格式异常');
  }

  /**
   * 删除指定任务
   * @param taskId - 任务 ID
   */
  async delete(taskId: string): Promise<void> {
    if (!taskId) {
      throw new AgentOSError('任务ID不能为空', ErrorCode.MISSING_PARAMETER);
    }
    await this.api.delete(`/api/v1/tasks/${taskId}`);
  }

  /**
   * 获取已完成任务的结果
   * @param taskId - 任务 ID
   */
  async getResult(taskId: string): Promise<TaskResult> {
    const task = await this.get(taskId);
    if (
      task.status !== TaskStatus.COMPLETED &&
      task.status !== TaskStatus.FAILED &&
      task.status !== TaskStatus.CANCELLED
    ) {
      throw new AgentOSError('任务尚未完成', ErrorCode.INVALID_PARAMETER);
    }
    return {
      id: task.id,
      status: task.status,
      output: task.output,
      error: task.error,
      startTime: task.createdAt,
      endTime: task.updatedAt,
      duration: 0,
    };
  }

  /**
   * 批量提交多个任务
   * @param descriptions - 任务描述列表
   */
  async batchSubmit(descriptions: string[]): Promise<Task[]> {
    const tasks: Task[] = [];
    for (const desc of descriptions) {
      const task = await this.submit(desc);
      tasks.push(task);
    }
    return tasks;
  }

  /**
   * 获取任务总数
   */
  async count(): Promise<number> {
    const resp = await this.api.get<APIResponse<{ count: number }>>('/api/v1/tasks/count');
    const data = extractDataMap(resp);
    if (!data) {
      return 0;
    }
    return getInt64(data, 'count');
  }

  /**
   * 从 map 解析 Task 结构
   */
  private parseTaskFromMap(data: Record<string, unknown>): Task {
    return {
      id: getString(data, 'task_id'),
      description: getString(data, 'description'),
      status: getString(data, 'status') as TaskStatus,
      priority: getInt64(data, 'priority'),
      output: getString(data, 'output'),
      error: getString(data, 'error'),
      metadata: getMap(data, 'metadata'),
      createdAt: parseTime(getString(data, 'created_at')),
      updatedAt: parseTime(getString(data, 'updated_at')),
    };
  }

  /**
   * 延迟函数
   */
  private sleep(ms: number): Promise<void> {
    return new Promise((resolve) => setTimeout(resolve, ms));
  }
}
