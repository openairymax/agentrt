// AgentOS TypeScript SDK - Enum Types
// Version: 3.0.0
// Last updated: 2026-03-24
//
// 定义 SDK 中使用的所有枚举类型。
// 与 Go SDK types/types.go 枚举部分保持一致。

/**
 * 任务状态枚举
 */
export enum TaskStatus {
  /** 待执行 */
  PENDING = 'pending',
  /** 执行中 */
  RUNNING = 'running',
  /** 已完成 */
  COMPLETED = 'completed',
  /** 已失败 */
  FAILED = 'failed',
  /** 已取消 */
  CANCELLED = 'cancelled',
}

/**
 * 判断任务是否处于终态
 * @param status - 任务状态
 */
export function isTerminalStatus(status: TaskStatus): boolean {
  return status === TaskStatus.COMPLETED ||
    status === TaskStatus.FAILED ||
    status === TaskStatus.CANCELLED;
}

/**
 * 记忆层级枚举（对应认知深度分层）
 */
export enum MemoryLayer {
  /** L1 层级 */
  L1 = 'L1',
  /** L2 层级 */
  L2 = 'L2',
  /** L3 层级 */
  L3 = 'L3',
  /** L4 层级 */
  L4 = 'L4',
}

/**
 * 判断记忆层级是否合法
 * @param layer - 记忆层级
 */
export function isValidMemoryLayer(layer: MemoryLayer): boolean {
  return Object.values(MemoryLayer).includes(layer);
}

/**
 * 会话状态枚举
 */
export enum SessionStatus {
  /** 活跃 */
  ACTIVE = 'active',
  /** 非活跃 */
  INACTIVE = 'inactive',
  /** 已过期 */
  EXPIRED = 'expired',
}

/**
 * 技能状态枚举
 */
export enum SkillStatus {
  /** 活跃 */
  ACTIVE = 'active',
  /** 非活跃 */
  INACTIVE = 'inactive',
  /** 已废弃 */
  DEPRECATED = 'deprecated',
}

/**
 * 遥测 Span 状态枚举
 */
export enum SpanStatus {
  /** 正常 */
  OK = 'ok',
  /** 错误 */
  ERROR = 'error',
  /** 未设置 */
  UNSET = 'unset',
}
