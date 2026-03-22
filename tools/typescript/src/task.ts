// AgentOS TypeScript SDK Task
// Version: 1.0.0.5
// Last updated: 2026-03-21

import { TaskStatus, TaskResult } from './types';
import { TaskError, TimeoutError } from './errors';
import { AgentOS } from './agent';

/**
 * Task class for managing AgentOS tasks
 */
export class Task {
  private client: AgentOS;
  private taskId: string;

  /**
   * Create a new Task object
   * @param client AgentOS client
   * @param taskId Task ID
   */
  constructor(client: AgentOS, taskId: string) {
    this.client = client;
    this.taskId = taskId;
  }

  /**
   * Get the task ID
   * @returns Task ID
   */
  get id(): string {
    return this.taskId;
  }

  /**
   * Query the task status
   * @returns Task status
   */
  async query(): Promise<TaskStatus> {
    try {
      const response = await this.client['request']<{ status: string }>(
        'GET',
        `/api/tasks/${this.taskId}`
      );

      if (!response.status) {
        throw new TaskError('Invalid response: missing status');
      }

      return response.status as TaskStatus;
    } catch (error) {
      throw new TaskError(`Error querying task status: ${error.message}`);
    }
  }

  /**
   * Wait for the task to complete
   * @param options Options
   * @returns Task result
   */
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
        const response = await this.client['request']<{ output?: string; error?: string }>(
          'GET',
          `/api/tasks/${this.taskId}`
        );

        return {
          taskId: this.taskId,
          status,
          output: response.output,
          error: response.error,
        };
      }

      if (timeout > 0 && Date.now() - startTime > timeout) {
        throw new TimeoutError(`Task did not complete within ${timeout}ms`);
      }

      // Wait for 500ms before querying again
      await new Promise((resolve) => setTimeout(resolve, 500));
    }
  }

  /**
   * Cancel the task
   * @returns True if the task was cancelled successfully
   */
  async cancel(): Promise<boolean> {
    try {
      const response = await this.client['request']<{ success: boolean }>(
        'POST',
        `/api/tasks/${this.taskId}/cancel`
      );

      return response.success;
    } catch (error) {
      throw new TaskError(`Error cancelling task: ${error.message}`);
    }
  }
}
