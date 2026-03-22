// AgentOS TypeScript SDK Types
// Version: 1.0.0.5
// Last updated: 2026-03-21

/**
 * Task status enumeration
 */
export enum TaskStatus {
  PENDING = 'pending',
  RUNNING = 'running',
  COMPLETED = 'completed',
  FAILED = 'failed',
  CANCELLED = 'cancelled'
}

/**
 * Task result interface
 */
export interface TaskResult {
  taskId: string;
  status: TaskStatus;
  output?: string;
  error?: string;
}

/**
 * Memory interface
 */
export interface Memory {
  memoryId: string;
  content: string;
  createdAt: string;
  metadata?: Record<string, any>;
}

/**
 * Skill info interface
 */
export interface SkillInfo {
  skillName: string;
  description: string;
  version: string;
  parameters?: Record<string, any>;
}

/**
 * Skill result interface
 */
export interface SkillResult {
  success: boolean;
  output?: any;
  error?: string;
}

/**
 * Client configuration interface
 */
export interface ClientConfig {
  endpoint?: string;
  timeout?: number;
  headers?: Record<string, string>;
}
