// AgentOS TypeScript SDK Syscall
// Version: 3.0.0
// Last updated: 2026-04-04

import { AgentOSError } from './errors';

/** 系统调用命名空间 */
export enum SyscallNamespace {
  TASK = 'task',
  MEMORY = 'memory',
  SESSION = 'session',
  TELEMETRY = 'telemetry',
}

/** 系统调用请求 */
export interface SyscallRequest {
  namespace: SyscallNamespace;
  operation: string;
  params?: Record<string, any>;
}

/** 系统调用响应 */
export interface SyscallResponse {
  success: boolean;
  data?: any;
  error?: string;
  error_code?: string;
}

/** 系统调用绑定（抽象基类，供特定运行时实现�?*/
export abstract class SyscallBinding {
  /** 执行系统调用 */
  abstract invoke(request: SyscallRequest): Promise<SyscallResponse>;
}

/** 任务系统调用便捷方法 */
export class TaskSyscall {
  constructor(private binding: SyscallBinding) {}

  /** 提交任务 */
  async submit(description: string): Promise<SyscallResponse> {
    return this.binding.invoke({
      namespace: SyscallNamespace.TASK,
      operation: 'submit',
      params: { description },
    });
  }

  /** 查询任务状�?*/
  async query(taskId: string): Promise<SyscallResponse> {
    return this.binding.invoke({
      namespace: SyscallNamespace.TASK,
      operation: 'query',
      params: { task_id: taskId },
    });
  }

  /** 取消任务 */
  async cancel(taskId: string): Promise<SyscallResponse> {
    return this.binding.invoke({
      namespace: SyscallNamespace.TASK,
      operation: 'cancel',
      params: { task_id: taskId },
    });
  }
}

/** 记忆系统调用便捷方法 */
export class MemorySyscall {
  constructor(private binding: SyscallBinding) {}

  /** 写入记忆 */
  async write(content: string, metadata?: Record<string, any>): Promise<SyscallResponse> {
    return this.binding.invoke({
      namespace: SyscallNamespace.MEMORY,
      operation: 'write',
      params: { content, metadata },
    });
  }

  /** 搜索记忆 */
  async search(query: string, topK: number = 5): Promise<SyscallResponse> {
    return this.binding.invoke({
      namespace: SyscallNamespace.MEMORY,
      operation: 'search',
      params: { query, top_k: topK },
    });
  }

  /** 删除记忆 */
  async delete(memoryId: string): Promise<SyscallResponse> {
    return this.binding.invoke({
      namespace: SyscallNamespace.MEMORY,
      operation: 'delete',
      params: { memory_id: memoryId },
    });
  }
}

/** 会话系统调用便捷方法 */
export class SessionSyscall {
  constructor(private binding: SyscallBinding) {}

  /** 创建会话 */
  async create(): Promise<SyscallResponse> {
    return this.binding.invoke({
      namespace: SyscallNamespace.SESSION,
      operation: 'create',
    });
  }

  /** 设置上下�?*/
  async setContext(sessionId: string, key: string, value: any): Promise<SyscallResponse> {
    return this.binding.invoke({
      namespace: SyscallNamespace.SESSION,
      operation: 'set_context',
      params: { session_id: sessionId, key, value },
    });
  }

  /** 获取上下�?*/
  async getContext(sessionId: string, key: string): Promise<SyscallResponse> {
    return this.binding.invoke({
      namespace: SyscallNamespace.SESSION,
      operation: 'get_context',
      params: { session_id: sessionId, key },
    });
  }
}
