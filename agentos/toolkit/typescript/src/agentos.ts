// AgentOS TypeScript SDK - Main Entry
// Version: 3.0.0
// Last updated: 2026-03-24
//
// 提供 TypeScript 接口与 AgentOS 系统交互。
// 包含任务管理、记忆操作、会话管理、技能加载、遥测和系统调用功能。
// 与 Go SDK agentos.go 保持一致。

import { Client, APIClient } from './client';
import { MockClient } from './client/mock';
import {
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
import { TaskManager } from './modules/task';
import { MemoryManager } from './modules/memory';
import { SessionManager } from './modules/session';
import { SkillManager } from './modules/skill';
import {
  HealthStatus,
  Metrics,
} from './types';

/**
 * AgentOS 是 TypeScript SDK 的主入口类
 * 提供对各个业务模块的统一访问入口
 */
export class AgentOS {
  private client: Client;
  private _tasks?: TaskManager;
  private _memories?: MemoryManager;
  private _sessions?: SessionManager;
  private _skills?: SkillManager;

  /**
   * 创建新的 AgentOS 客户端实例
   * @param manager - 配置对象或配置选项
   */
  constructor(manager?: manager | ConfigOption[]) {
    let finalConfig: manager;

    if (Array.isArray(manager)) {
      finalConfig = newConfig(...manager);
    } else if (manager) {
      finalConfig = manager;
    } else {
      finalConfig = defaultConfig();
    }

    validateConfig(finalConfig);
    this.client = new Client(finalConfig);
  }

  /**
   * 从环境变量创建 AgentOS 客户端
   */
  static fromEnv(): AgentOS {
    const manager = newConfigFromEnv();
    return new AgentOS(manager);
  }

  /**
   * 获取任务管理器
   */
  get tasks(): TaskManager {
    if (!this._tasks) {
      this._tasks = new TaskManager(this.client);
    }
    return this._tasks;
  }

  /**
   * 获取记忆管理器
   */
  get memories(): MemoryManager {
    if (!this._memories) {
      this._memories = new MemoryManager(this.client);
    }
    return this._memories;
  }

  /**
   * 获取会话管理器
   */
  get sessions(): SessionManager {
    if (!this._sessions) {
      this._sessions = new SessionManager(this.client);
    }
    return this._sessions;
  }

  /**
   * 获取技能管理器
   */
  get skills(): SkillManager {
    if (!this._skills) {
      this._skills = new SkillManager(this.client);
    }
    return this._skills;
  }

  /**
   * 获取底层 API 客户端
   */
  get api(): APIClient {
    return this.client;
  }

  /**
   * 获取当前配置
   */
  getConfig(): manager {
    return this.client.getConfig();
  }

  /**
   * 检查服务健康状态
   */
  async health(): Promise<HealthStatus> {
    return this.client.health();
  }

  /**
   * 获取系统运行指标
   */
  async metrics(): Promise<Metrics> {
    return this.client.metrics();
  }

  /**
   * 关闭客户端，释放资源
   */
  close(): void {
    this.client.close();
  }

  /**
   * 返回客户端的可读描述
   */
  toString(): string {
    return this.client.toString();
  }
}

/**
 * 创建新的 AgentOS 客户端实例（快捷方式）
 * @param opts - 配置选项
 */
export function createAgentOS(...opts: ConfigOption[]): AgentOS {
  return new AgentOS(opts);
}

// ============================================================
// 便捷导出
// ============================================================

// 客户端
export { Client, APIClient, MockClient };

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
};

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
