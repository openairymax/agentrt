// AgentOS TypeScript SDK
// Version: 3.0.0
// Last updated: 2026-03-24
//
// 提供 TypeScript 接口与 AgentOS 系统交互。
// 包含任务管理、记忆操作、会话管理、技能加载功能。
// 与 Go SDK 结构保持一致。

/**
 * AgentOS TypeScript SDK
 *
 * 模块化结构：
 * - agentos.ts: 主入口，提供 AgentOS 类
 * - manager.ts: 配置管理
 * - errors.ts: 错误定义
 * - client/: HTTP 客户端层
 * - modules/: 业务模块层 (task, memory, session, skill)
 * - types/: 类型定义
 * - utils/: 工具函数
 */

// 主入口
export { AgentOS, createAgentOS } from './agentos';

// 客户端
export { Client, APIClient, MockClient } from './client';

// 配置
export {
  manager,
  ConfigOption,
  newConfig,
  newConfigFromEnv,
  defaultConfig,
  validateConfig,
  cloneConfig,
  mergeConfig,
  withEndpoint,
  withTimeout,
  withMaxRetries,
  withRetryDelay,
  withAPIKey,
  withUserAgent,
  withDebug,
  withLogLevel,
  withMaxConnections,
  withHeaders,
} from './manager';

// 模块管理器
export { TaskManager } from './modules/task';
export { MemoryManager, MemoryWriteItem } from './modules/memory';
export { SessionManager } from './modules/session';
export { SkillManager, SkillExecuteRequest } from './modules/skill';

// 类型
export * from './types';

// 错误
export * from './errors';

// 工具函数
export * from './utils';

/** SDK 版本号 */
export const VERSION = '3.0.0';
