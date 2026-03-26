// AgentOS TypeScript SDK - Logger Module
// Version: 3.0.0
// Last updated: 2026-03-25
//
// 提供可配置的日志功能，遵循架构设计原则 E-2 可观测性原则。

/**
 * 日志级别枚举
 */
export enum LogLevel {
  DEBUG = 0,
  INFO = 1,
  WARN = 2,
  ERROR = 3,
  NONE = 4,
}

/**
 * 日志配置接口
 */
export interface LoggerConfig {
  /** 是否启用调试模式 */
  debug?: boolean;
  /** 日志级别 */
  level?: string;
  /** 自定义日志输出函数 */
  output?: (level: LogLevel, message: string, ...args: unknown[]) => void;
}

/**
 * 日志记录器类
 * 支持可配置的日志级别和输出方式
 */
export class Logger {
  private level: LogLevel;
  private output: (level: LogLevel, message: string, ...args: unknown[]) => void;

  /**
   * 创建日志记录器
   * @param config - 日志配置
   */
  constructor(config: LoggerConfig = {}) {
    this.level = this.parseLevel(config.level, config.debug);
    this.output = config.output || this.defaultOutput;
  }

  /**
   * 解析日志级别
   * @param level - 日志级别字符串
   * @param debug - 是否调试模式
   */
  private parseLevel(level?: string, debug?: boolean): LogLevel {
    if (debug) {
      return LogLevel.DEBUG;
    }
    switch (level?.toLowerCase()) {
      case 'debug':
        return LogLevel.DEBUG;
      case 'info':
        return LogLevel.INFO;
      case 'warn':
        return LogLevel.WARN;
      case 'error':
        return LogLevel.ERROR;
      case 'none':
        return LogLevel.NONE;
      default:
        return LogLevel.INFO;
    }
  }

  /**
   * 默认日志输出函数
   * @param level - 日志级别
   * @param message - 日志消息
   * @param args - 额外参数
   */
  private defaultOutput(level: LogLevel, message: string, ...args: unknown[]): void {
    const timestamp = new Date().toISOString();
    const prefix = `[AgentOS][${timestamp}]`;
    switch (level) {
      case LogLevel.DEBUG:
        console.debug(`${prefix} [DEBUG] ${message}`, ...args);
        break;
      case LogLevel.INFO:
        console.info(`${prefix} [INFO] ${message}`, ...args);
        break;
      case LogLevel.WARN:
        console.warn(`${prefix} [WARN] ${message}`, ...args);
        break;
      case LogLevel.ERROR:
        console.error(`${prefix} [ERROR] ${message}`, ...args);
        break;
    }
  }

  /**
   * 输出调试日志
   * @param message - 日志消息
   * @param args - 额外参数
   */
  debug(message: string, ...args: unknown[]): void {
    if (this.level <= LogLevel.DEBUG) {
      this.output(LogLevel.DEBUG, message, ...args);
    }
  }

  /**
   * 输出信息日志
   * @param message - 日志消息
   * @param args - 额外参数
   */
  info(message: string, ...args: unknown[]): void {
    if (this.level <= LogLevel.INFO) {
      this.output(LogLevel.INFO, message, ...args);
    }
  }

  /**
   * 输出警告日志
   * @param message - 日志消息
   * @param args - 额外参数
   */
  warn(message: string, ...args: unknown[]): void {
    if (this.level <= LogLevel.WARN) {
      this.output(LogLevel.WARN, message, ...args);
    }
  }

  /**
   * 输出错误日志
   * @param message - 日志消息
   * @param args - 额外参数
   */
  error(message: string, ...args: unknown[]): void {
    if (this.level <= LogLevel.ERROR) {
      this.output(LogLevel.ERROR, message, ...args);
    }
  }
}

/** 默认日志记录器实例 */
let defaultLogger: Logger = new Logger();

/**
 * 获取默认日志记录器
 */
export function getLogger(): Logger {
  return defaultLogger;
}

/**
 * 设置默认日志记录器
 * @param config - 日志配置
 */
export function setLogger(config: LoggerConfig): void {
  defaultLogger = new Logger(config);
}

/**
 * 创建新的日志记录器
 * @param config - 日志配置
 */
export function newLogger(config: LoggerConfig = {}): Logger {
  return new Logger(config);
}
