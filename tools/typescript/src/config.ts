// AgentOS TypeScript SDK - Configuration Module
// Version: 3.0.0
// Last updated: 2026-03-24
//
// 提供客户端配置的定义、创建、验证和合并功能。
// 支持函数式选项模式和环境变量注入，零值保护守卫。
// 与 Go SDK config.go 保持一致。

import { ConfigError, ErrorCode } from './errors';

/**
 * Config 定义 AgentOS 客户端的完整配置
 */
export interface Config {
  /** 服务端点地址 */
  endpoint: string;
  /** 请求超时时间（毫秒） */
  timeout: number;
  /** 最大重试次数 */
  maxRetries: number;
  /** 重试间隔（毫秒） */
  retryDelay: number;
  /** API 密钥 */
  apiKey?: string;
  /** User-Agent 头 */
  userAgent: string;
  /** 调试模式 */
  debug: boolean;
  /** 日志级别 */
  logLevel: string;
  /** 最大连接数 */
  maxConnections: number;
  /** 空闲连接超时（毫秒） */
  idleConnTimeout: number;
  /** 自定义请求头 */
  headers?: Record<string, string>;
}

/**
 * ClientConfig 是 Config 的别名，用于向后兼容
 * @deprecated 请使用 Config 代替
 */
export type ClientConfig = Partial<Config>;

/**
 * ConfigOption 定义配置选项的函数签名
 */
export type ConfigOption = (config: Config) => void;

/**
 * 默认配置
 */
export function defaultConfig(): Config {
  return {
    endpoint: 'http://localhost:18789',
    timeout: 30000,
    maxRetries: 3,
    retryDelay: 1000,
    userAgent: 'AgentOS-TypeScript-SDK/3.0.0',
    debug: false,
    logLevel: 'info',
    maxConnections: 10,
    idleConnTimeout: 90000,
  };
}

/**
 * 默认任务轮询间隔（毫秒）
 */
export const DEFAULT_POLL_INTERVAL_MS = 500;

/**
 * 设置服务端点地址
 * @param endpoint - 端点地址
 */
export function withEndpoint(endpoint: string): ConfigOption {
  return (config: Config) => {
    if (endpoint) {
      config.endpoint = endpoint.replace(/\/$/, '');
    }
  };
}

/**
 * 设置请求超时时间
 * @param timeout - 超时时间（毫秒）
 */
export function withTimeout(timeout: number): ConfigOption {
  return (config: Config) => {
    if (timeout > 0) {
      config.timeout = timeout;
    }
  };
}

/**
 * 设置最大重试次数
 * @param maxRetries - 最大重试次数
 */
export function withMaxRetries(maxRetries: number): ConfigOption {
  return (config: Config) => {
    if (maxRetries >= 0) {
      config.maxRetries = maxRetries;
    }
  };
}

/**
 * 设置重试间隔
 * @param delay - 重试间隔（毫秒）
 */
export function withRetryDelay(delay: number): ConfigOption {
  return (config: Config) => {
    if (delay > 0) {
      config.retryDelay = delay;
    }
  };
}

/**
 * 设置 API 密钥
 * @param apiKey - API 密钥
 */
export function withAPIKey(apiKey: string): ConfigOption {
  return (config: Config) => {
    config.apiKey = apiKey;
  };
}

/**
 * 设置 User-Agent 头
 * @param userAgent - User-Agent 字符串
 */
export function withUserAgent(userAgent: string): ConfigOption {
  return (config: Config) => {
    if (userAgent) {
      config.userAgent = userAgent;
    }
  };
}

/**
 * 设置调试模式
 * @param debug - 是否启用调试
 */
export function withDebug(debug: boolean): ConfigOption {
  return (config: Config) => {
    config.debug = debug;
  };
}

/**
 * 设置日志级别
 * @param level - 日志级别
 */
export function withLogLevel(level: string): ConfigOption {
  return (config: Config) => {
    if (level) {
      config.logLevel = level;
    }
  };
}

/**
 * 设置最大连接数
 * @param maxConn - 最大连接数
 */
export function withMaxConnections(maxConn: number): ConfigOption {
  return (config: Config) => {
    if (maxConn > 0) {
      config.maxConnections = maxConn;
    }
  };
}

/**
 * 设置自定义请求头
 * @param headers - 自定义请求头
 */
export function withHeaders(headers: Record<string, string>): ConfigOption {
  return (config: Config) => {
    config.headers = { ...config.headers, ...headers };
  };
}

/**
 * 使用函数式选项创建新配置
 * @param opts - 配置选项数组
 */
export function newConfig(...opts: ConfigOption[]): Config {
  const config = defaultConfig();
  for (const opt of opts) {
    opt(config);
  }
  return config;
}

/**
 * 从环境变量创建配置
 * 支持的环境变量：
 * - AGENTOS_ENDPOINT
 * - AGENTOS_TIMEOUT
 * - AGENTOS_MAX_RETRIES
 * - AGENTOS_RETRY_DELAY
 * - AGENTOS_API_KEY
 * - AGENTOS_DEBUG
 * - AGENTOS_LOG_LEVEL
 * - AGENTOS_MAX_CONNECTIONS
 * - AGENTOS_USER_AGENT
 */
export function newConfigFromEnv(): Config {
  const config = defaultConfig();

  // 仅在浏览器环境之外使用 process.env
  if (typeof process !== 'undefined' && process.env) {
    const env = process.env;

    if (env.AGENTOS_ENDPOINT) {
      config.endpoint = env.AGENTOS_ENDPOINT;
    }

    if (env.AGENTOS_TIMEOUT) {
      const timeout = parseInt(env.AGENTOS_TIMEOUT, 10);
      if (!isNaN(timeout) && timeout > 0) {
        config.timeout = timeout;
      }
    }

    if (env.AGENTOS_MAX_RETRIES) {
      const maxRetries = parseInt(env.AGENTOS_MAX_RETRIES, 10);
      if (!isNaN(maxRetries) && maxRetries >= 0) {
        config.maxRetries = maxRetries;
      }
    }

    if (env.AGENTOS_RETRY_DELAY) {
      const delay = parseInt(env.AGENTOS_RETRY_DELAY, 10);
      if (!isNaN(delay) && delay > 0) {
        config.retryDelay = delay;
      }
    }

    if (env.AGENTOS_API_KEY) {
      config.apiKey = env.AGENTOS_API_KEY;
    }

    if (env.AGENTOS_DEBUG) {
      config.debug = env.AGENTOS_DEBUG === 'true' || env.AGENTOS_DEBUG === '1';
    }

    if (env.AGENTOS_LOG_LEVEL) {
      config.logLevel = env.AGENTOS_LOG_LEVEL;
    }

    if (env.AGENTOS_MAX_CONNECTIONS) {
      const maxConn = parseInt(env.AGENTOS_MAX_CONNECTIONS, 10);
      if (!isNaN(maxConn) && maxConn > 0) {
        config.maxConnections = maxConn;
      }
    }

    if (env.AGENTOS_USER_AGENT) {
      config.userAgent = env.AGENTOS_USER_AGENT;
    }
  }

  return config;
}

/**
 * 验证配置的合法性
 * @param config - 配置对象
 * @throws ConfigError 如果配置无效
 */
export function validateConfig(config: Config): void {
  if (!config.endpoint) {
    throw new ConfigError('端点地址不能为空');
  }

  // 验证端点格式
  try {
    const url = new URL(config.endpoint);
    if (url.protocol !== 'http:' && url.protocol !== 'https:') {
      throw new ConfigError('端点地址必须以 http:// 或 https:// 开头');
    }
  } catch {
    throw new ConfigError('端点地址格式无效');
  }

  if (config.timeout <= 0) {
    throw new ConfigError('超时时间必须大于零');
  }

  if (config.maxConnections <= 0) {
    throw new ConfigError('最大连接数必须大于零');
  }
}

/**
 * 创建配置的深拷贝
 * @param config - 源配置
 */
export function cloneConfig(config: Config): Config {
  return {
    ...config,
    headers: config.headers ? { ...config.headers } : undefined,
  };
}

/**
 * 将 override 的非零值合并到当前配置
 * @param base - 基础配置
 * @param override - 覆盖配置
 */
export function mergeConfig(base: Config, override?: Partial<Config>): Config {
  const result = cloneConfig(base);

  if (!override) {
    return result;
  }

  if (override.endpoint) {
    result.endpoint = override.endpoint;
  }
  if (override.timeout && override.timeout > 0) {
    result.timeout = override.timeout;
  }
  if (override.maxRetries !== undefined && override.maxRetries >= 0) {
    result.maxRetries = override.maxRetries;
  }
  if (override.retryDelay && override.retryDelay > 0) {
    result.retryDelay = override.retryDelay;
  }
  if (override.apiKey) {
    result.apiKey = override.apiKey;
  }
  if (override.userAgent) {
    result.userAgent = override.userAgent;
  }
  // Debug 始终取 override 值
  if (override.debug !== undefined) {
    result.debug = override.debug;
  }
  if (override.logLevel) {
    result.logLevel = override.logLevel;
  }
  if (override.maxConnections && override.maxConnections > 0) {
    result.maxConnections = override.maxConnections;
  }
  if (override.idleConnTimeout && override.idleConnTimeout > 0) {
    result.idleConnTimeout = override.idleConnTimeout;
  }
  if (override.headers) {
    result.headers = { ...result.headers, ...override.headers };
  }

  return result;
}

/**
 * 返回配置的可读描述
 * @param config - 配置对象
 */
export function configToString(config: Config): string {
  return `Config[endpoint=${config.endpoint}, timeout=${config.timeout}ms, retries=${config.maxRetries}]`;
}
