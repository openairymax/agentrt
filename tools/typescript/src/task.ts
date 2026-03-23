// AgentOS TypeScript SDK Task
// Version: 2.0.0
// Last updated: 2026-03-23

import { TaskStatus, TaskResult } from './types';
import { TaskError, TimeoutError } from './errors';
import { AgentOS } from './agent';

/** AgentOS 任务管理�?*/
export class Task {
  private client: AgentOS;
  private taskId: string;

  /** 创建新的 Task 对象 */
  constructor(client: AgentOS, taskId: string) {
    this.client = client;
    this.taskId = taskId;
  }

  /** 获取任务 ID */
  get id(): string {
    return this.taskId;
  }

  /** 查询任务状�?*/
  async query(): Promise<TaskStatus> {
    const response = await this.client.request<{ status: string }>(
      'GET',
      `/api/v1/tasks/${this.taskId}`,
    );
    if (!response.status) {
      throw new TaskError('响应格式异常: 缺少 status');
    }
    return response.status as TaskStatus;
  }

  /** 等待任务完成 */
  async wait(options?: { timeout?: number }): Promise<TaskResult> {
    const startTime = Date.now();
    const timeout = options?.timeout || 0;

    while (true) {
      const status = await this.query();

      if (
        status === TaskStatus.COMPLETED ||
        status === TaskStatus.FAILED ||
        status === TaskStatus.CANCELLED
      ) {
        const response = await this.client.request<{
          output?: string;
          error?: string;
        }>('GET', `/api/v1/tasks/${this.taskId}`);

        return {
          taskId: this.taskId,
          status,
          output: response.output,
          error: response.error,
        };
      }

      if (timeout > 0 && Date.now() - startTime > timeout) {
        throw new TimeoutError(
          `任务�?${timeout}ms 内未完成`,
        );
      }

      await new Promise((resolve) => setTimeout(resolve, 500));
    }
  }

  /** 取消任务 */
  async cancel(): Promise<boolean> {
    const response = await this.client.request<{ success: boolean }>(
      'POST',
      `/api/v1/tasks/${this.taskId}/cancel`,
    );
    return response.success;
  }
}
