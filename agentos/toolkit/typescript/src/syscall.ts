import { AgentOSError } from './errors';

export enum SyscallNamespace {
  TASK = 'task',
  MEMORY = 'memory',
  SESSION = 'session',
  SKILL = 'skill',
  AGENT = 'agent',
  TELEMETRY = 'telemetry',
}

export interface SyscallRequest {
  namespace: SyscallNamespace;
  operation: string;
  params?: Record<string, any>;
}

export interface SyscallResponse {
  success: boolean;
  data?: any;
  error?: string;
  error_code?: string;
}

export abstract class SyscallBinding {
  abstract invoke(request: SyscallRequest): Promise<SyscallResponse>;
}

export class HttpSyscallBinding extends SyscallBinding {
  private baseUrl: string;
  private defaultHeaders: Record<string, string>;

  constructor(baseUrl: string, options?: { apiKey?: string; timeout?: number }) {
    super();
    this.baseUrl = baseUrl.replace(/\/+$/, '');
    this.defaultHeaders = {
      'Content-Type': 'application/json',
    };
    if (options?.apiKey) {
      this.defaultHeaders['Authorization'] = `Bearer ${options.apiKey}`;
    }
  }

  async invoke(request: SyscallRequest): Promise<SyscallResponse> {
    const url = `${this.baseUrl}/api/v1/syscall/${request.namespace}`;
    try {
      const response = await fetch(url, {
        method: 'POST',
        headers: this.defaultHeaders,
        body: JSON.stringify(request),
      });

      if (!response.ok) {
        throw AgentOSError.http(
          `Syscall HTTP ${response.status}: ${request.operation}`
        );
      }

      const data = await response.json();
      return data as SyscallResponse;
    } catch (err) {
      if (err instanceof AgentOSError) {
        throw err;
      }
      throw AgentOSError.network(
        `Syscall network error: ${(err as Error).message}`
      );
    }
  }
}

export class TaskSyscall {
  constructor(private binding: SyscallBinding) {}

  async submit(description: string): Promise<SyscallResponse> {
    return this.binding.invoke({
      namespace: SyscallNamespace.TASK,
      operation: 'submit',
      params: { description },
    });
  }

  async query(taskId: string): Promise<SyscallResponse> {
    return this.binding.invoke({
      namespace: SyscallNamespace.TASK,
      operation: 'query',
      params: { task_id: taskId },
    });
  }

  async wait(taskId: string, timeoutMs: number = 0): Promise<SyscallResponse> {
    return this.binding.invoke({
      namespace: SyscallNamespace.TASK,
      operation: 'wait',
      params: { task_id: taskId, timeout_ms: timeoutMs },
    });
  }

  async cancel(taskId: string): Promise<SyscallResponse> {
    return this.binding.invoke({
      namespace: SyscallNamespace.TASK,
      operation: 'cancel',
      params: { task_id: taskId },
    });
  }
}

export class MemorySyscall {
  constructor(private binding: SyscallBinding) {}

  async write(content: string, metadata?: Record<string, any>): Promise<SyscallResponse> {
    return this.binding.invoke({
      namespace: SyscallNamespace.MEMORY,
      operation: 'write',
      params: { content, metadata },
    });
  }

  async read(memoryId: string): Promise<SyscallResponse> {
    return this.binding.invoke({
      namespace: SyscallNamespace.MEMORY,
      operation: 'read',
      params: { memory_id: memoryId },
    });
  }

  async get(memoryId: string): Promise<SyscallResponse> {
    return this.binding.invoke({
      namespace: SyscallNamespace.MEMORY,
      operation: 'get',
      params: { memory_id: memoryId },
    });
  }

  async search(query: string, topK: number = 5): Promise<SyscallResponse> {
    return this.binding.invoke({
      namespace: SyscallNamespace.MEMORY,
      operation: 'search',
      params: { query, top_k: topK },
    });
  }

  async delete(memoryId: string): Promise<SyscallResponse> {
    return this.binding.invoke({
      namespace: SyscallNamespace.MEMORY,
      operation: 'delete',
      params: { memory_id: memoryId },
    });
  }
}

export class SessionSyscall {
  constructor(private binding: SyscallBinding) {}

  async create(): Promise<SyscallResponse> {
    return this.binding.invoke({
      namespace: SyscallNamespace.SESSION,
      operation: 'create',
    });
  }

  async get(sessionId: string): Promise<SyscallResponse> {
    return this.binding.invoke({
      namespace: SyscallNamespace.SESSION,
      operation: 'get',
      params: { session_id: sessionId },
    });
  }

  async close(sessionId: string): Promise<SyscallResponse> {
    return this.binding.invoke({
      namespace: SyscallNamespace.SESSION,
      operation: 'close',
      params: { session_id: sessionId },
    });
  }

  async list(): Promise<SyscallResponse> {
    return this.binding.invoke({
      namespace: SyscallNamespace.SESSION,
      operation: 'list',
    });
  }

  async setContext(sessionId: string, key: string, value: any): Promise<SyscallResponse> {
    return this.binding.invoke({
      namespace: SyscallNamespace.SESSION,
      operation: 'set_context',
      params: { session_id: sessionId, key, value },
    });
  }

  async getContext(sessionId: string, key: string): Promise<SyscallResponse> {
    return this.binding.invoke({
      namespace: SyscallNamespace.SESSION,
      operation: 'get_context',
      params: { session_id: sessionId, key },
    });
  }
}

export class SkillSyscall {
  constructor(private binding: SyscallBinding) {}

  async load(skillName: string): Promise<SyscallResponse> {
    return this.binding.invoke({
      namespace: SyscallNamespace.SKILL,
      operation: 'load',
      params: { skill_name: skillName },
    });
  }

  async execute(skillId: string, params: Record<string, any>): Promise<SyscallResponse> {
    return this.binding.invoke({
      namespace: SyscallNamespace.SKILL,
      operation: 'execute',
      params: { skill_id: skillId, params },
    });
  }

  async unload(skillId: string): Promise<SyscallResponse> {
    return this.binding.invoke({
      namespace: SyscallNamespace.SKILL,
      operation: 'unload',
      params: { skill_id: skillId },
    });
  }

  async list(): Promise<SyscallResponse> {
    return this.binding.invoke({
      namespace: SyscallNamespace.SKILL,
      operation: 'list',
    });
  }
}

export class AgentSyscall {
  constructor(private binding: SyscallBinding) {}

  async spawn(spec: Record<string, any>): Promise<SyscallResponse> {
    return this.binding.invoke({
      namespace: SyscallNamespace.AGENT,
      operation: 'spawn',
      params: { spec },
    });
  }

  async terminate(agentId: string): Promise<SyscallResponse> {
    return this.binding.invoke({
      namespace: SyscallNamespace.AGENT,
      operation: 'terminate',
      params: { agent_id: agentId },
    });
  }

  async invoke(agentId: string, method: string, args: Record<string, any>): Promise<SyscallResponse> {
    return this.binding.invoke({
      namespace: SyscallNamespace.AGENT,
      operation: 'invoke',
      params: { agent_id: agentId, method, args },
    });
  }
}
