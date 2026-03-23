// AgentOS TypeScript SDK Types
// Version: 2.0.0
// Last updated: 2026-03-23

/** 任务状态枚举 */
export enum TaskStatus {
  PENDING = 'pending',
  RUNNING = 'running',
  COMPLETED = 'completed',
  FAILED = 'failed',
  CANCELLED = 'cancelled',
}

/** 记忆层级枚举 */
export enum MemoryLayer {
  RAW = 'RAW',
  WORKING = 'WORKING',
  LONG_TERM = 'LONG_TERM',
  EPISODIC = 'EPISODIC',
}

/** 任务结果接口 */
export interface TaskResult {
  taskId: string;
  status: TaskStatus;
  output?: string;
  error?: string;
}

/** 记忆接口 */
export interface Memory {
  memoryId: string;
  content: string;
  createdAt: string;
  layer?: MemoryLayer;
  metadata?: Record<string, unknown>;
}

/** 技能信息接口 */
export interface SkillInfo {
  skillName: string;
  skillId?: string;
  description: string;
  version: string;
  parameters?: Record<string, unknown>;
  enabled?: boolean;
}

/** 技能执行结果接口（泛型版本） */
export interface SkillResult<T = unknown> {
  success: boolean;
  output?: T;
  error?: string;
}

/** 客户端配置接口 */
export interface ClientConfig {
  endpoint?: string;
  timeout?: number;
  headers?: Record<string, string>;
  retryDelay?: number;
  maxRetries?: number;
}
