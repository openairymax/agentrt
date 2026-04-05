// AgentOS TypeScript SDK - Domain Models
// Version: 3.0.0
// Last updated: 2026-03-24
//
// 定义 SDK 中使用的所有领域模型。
// 与 Go SDK types/types.go 领域模型部分保持一致。

import { TaskStatus, MemoryLayer, SessionStatus, SkillStatus } from './enums';

/**
 * 任务表示 AgentOS 系统中的一个执行任务
 */
export interface Task {
  /** 任务 ID */
  id: string;
  /** 任务描述 */
  description: string;
  /** 任务状态 */
  status: TaskStatus;
  /** 优先级 */
  priority: number;
  /** 输出结果 */
  output?: string;
  /** 错误信息 */
  error?: string;
  /** 元数据 */
  metadata?: Record<string, unknown>;
  /** 创建时间 */
  createdAt: Date;
  /** 更新时间 */
  updatedAt: Date;
}

/**
 * 任务结果表示已完成任务的结果快照
 */
export interface TaskResult {
  /** 任务 ID */
  id: string;
  /** 任务状态 */
  status: TaskStatus;
  /** 输出结果 */
  output?: string;
  /** 错误信息 */
  error?: string;
  /** 开始时间 */
  startTime: Date;
  /** 结束时间 */
  endTime: Date;
  /** 执行时长（毫秒） */
  duration: number;
}

/**
 * 记忆表示 AgentOS 系统中的一条记忆记录
 */
export interface Memory {
  /** 记忆 ID */
  id: string;
  /** 内容 */
  content: string;
  /** 记忆层级 */
  layer: MemoryLayer;
  /** 相关性分数 */
  score: number;
  /** 元数据 */
  metadata?: Record<string, unknown>;
  /** 创建时间 */
  createdAt: Date;
  /** 更新时间 */
  updatedAt: Date;
}

/**
 * 记忆搜索结果表示记忆搜索的聚合结果
 */
export interface MemorySearchResult {
  /** 记忆列表 */
  memories: Memory[];
  /** 总数 */
  total: number;
  /** 查询字符串 */
  query: string;
  /** TopK 参数 */
  topK: number;
}

/**
 * 会话表示用户与 Agent 交互的有状态通道
 */
export interface Session {
  /** 会话 ID */
  id: string;
  /** 用户 ID */
  userId: string;
  /** 会话状态 */
  status: SessionStatus;
  /** 上下文数据 */
  context?: Record<string, unknown>;
  /** 元数据 */
  metadata?: Record<string, unknown>;
  /** 创建时间 */
  createdAt: Date;
  /** 最后活动时间 */
  lastActivity: Date;
}

/**
 * 技能表示 AgentOS 系统中的可插拔能力单元
 */
export interface Skill {
  /** 技能 ID */
  id: string;
  /** 技能名称 */
  name: string;
  /** 版本 */
  version: string;
  /** 描述 */
  description: string;
  /** 技能状态 */
  status: SkillStatus;
  /** 参数定义 */
  parameters?: Record<string, unknown>;
  /** 元数据 */
  metadata?: Record<string, unknown>;
  /** 创建时间 */
  createdAt: Date;
}

/**
 * 技能执行结果
 */
export interface SkillResult<T = unknown> {
  /** 是否成功 */
  success: boolean;
  /** 输出结果 */
  output?: T;
  /** 错误信息 */
  error?: string;
}

/**
 * 技能元信息
 */
export interface SkillInfo {
  /** 技能名称 */
  name: string;
  /** 描述 */
  description: string;
  /** 版本 */
  version: string;
  /** 参数定义 */
  parameters?: Record<string, unknown>;
}

/**
 * 健康检查返回状态
 */
export interface HealthStatus {
  /** 状态 */
  status: string;
  /** 版本 */
  version: string;
  /** 运行时间（秒） */
  uptime: number;
  /** 健康检查项 */
  checks: Record<string, string>;
  /** 时间戳 */
  timestamp: Date;
}

/**
 * 系统运行指标快照
 */
export interface Metrics {
  /** 任务总数 */
  tasksTotal: number;
  /** 已完成任务数 */
  tasksCompleted: number;
  /** 失败任务数 */
  tasksFailed: number;
  /** 记忆总数 */
  memoriesTotal: number;
  /** 活跃会话数 */
  sessionsActive: number;
  /** 已加载技能数 */
  skillsLoaded: number;
  /** CPU 使用率 */
  cpuUsage: number;
  /** 内存使用率 */
  memoryUsage: number;
  /** 请求计数 */
  requestCount: number;
  /** 平均延迟（毫秒） */
  averageLatencyMs: number;
}
