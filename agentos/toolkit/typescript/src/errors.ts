// AgentOS TypeScript SDK - Errors Module
// Version: 3.0.0
// Last updated: 2026-04-05
//
// 定义 SDK 的完整错误类型层级、错误码枚举、哨兵错误和 HTTP 状态码映射。
// 与 Go SDK errors.go 保持一致。

// ============================================================
// 错误码常量
// 基于 ErrorCodeReference.md 规范，采用十六进制分类体系：
//   0x0xxx 通用错误 (General)
//   0x1xxx 核心循环错误 (CoreLoop)
//   0x2xxx 认知层错误 (Cognition)
//   0x3xxx 执行层错误 (Execution)
//   0x4xxx 记忆层错误 (Memory)
//   0x5xxx 系统调用错误 (Syscall)
//   0x6xxx 安全域错误 (Security)
//   0x7xxx 动态模块错误 (gateway)
// ============================================================

/**
 * 错误码常量，与 Go SDK ErrorCodeReference.md 对齐
 */
export const ErrorCode = {
  // 通用错误 (0x0xxx)
  SUCCESS: '0x0000',
  UNKNOWN: '0x0001',
  INVALID_PARAMETER: '0x0002',
  MISSING_PARAMETER: '0x0003',
  TIMEOUT: '0x0004',
  NOT_FOUND: '0x0005',
  ALREADY_EXISTS: '0x0006',
  CONFLICT: '0x0007',
  INVALID_CONFIG: '0x0008',
  INVALID_ENDPOINT: '0x0009',
  NETWORK_ERROR: '0x000A',
  CONNECTION_REFUSED: '0x000B',
  SERVER_ERROR: '0x000C',
  UNAUTHORIZED: '0x000D',
  FORBIDDEN: '0x000E',
  RATE_LIMITED: '0x000F',
  INVALID_RESPONSE: '0x0010',
  PARSE_ERROR: '0x0011',
  VALIDATION_ERROR: '0x0012',
  NOT_SUPPORTED: '0x0013',
  INTERNAL: '0x0014',
  BUSY: '0x0015',

  // 核心循环错误 (0x1xxx)
  LOOP_CREATE_FAILED: '0x1001',
  LOOP_START_FAILED: '0x1002',
  LOOP_STOP_FAILED: '0x1003',

  // 认知层错误 (0x2xxx)
  COGNITION_FAILED: '0x2001',
  DAG_BUILD_FAILED: '0x2002',
  AGENT_DISPATCH_FAILED: '0x2003',
  INTENT_PARSE_FAILED: '0x2004',

  // 执行层错误 (0x3xxx)
  TASK_FAILED: '0x3001',
  TASK_CANCELLED: '0x3002',
  TASK_TIMEOUT: '0x3003',

  // 记忆层错误 (0x4xxx)
  MEMORY_NOT_FOUND: '0x4001',
  MEMORY_EVOLVE_FAILED: '0x4002',
  MEMORY_SEARCH_FAILED: '0x4003',
  SESSION_NOT_FOUND: '0x4004',
  SESSION_EXPIRED: '0x4005',
  SKILL_NOT_FOUND: '0x4006',
  SKILL_EXECUTION_FAILED: '0x4007',

  // 系统调用错误 (0x5xxx)
  TELEMETRY_ERROR: '0x5001',
  SYSCALL_ERROR: '0x5002',

  // 安全域错误 (0x6xxx)
  PERMISSION_DENIED: '0x6001',
  CORRUPTED_DATA: '0x6002',
} as const;

/** 错误码类型 */
export type ErrorCodeType = (typeof ErrorCode)[keyof typeof ErrorCode];

// ============================================================
// 错误类定义
// ============================================================

/**
 * AgentOS 基础错误类
 */
export class AgentOSError extends Error {
  /** 错误码 */
  public readonly code: string;
  /** 原始错误 */
  public readonly cause?: Error;

  /**
   * 创建 AgentOS 错误
   * @param message - 错误消息
   * @param code - 错误码
   * @param cause - 原始错误
   */
  constructor(message: string, code: string = ErrorCode.UNKNOWN, cause?: Error) {
    super(cause ? `[${code}] ${message}: ${cause.message}` : `[${code}] ${message}`);
    this.name = 'AgentOSError';
    this.code = code;
    this.cause = cause;
  }

  /**
   * 判断是否匹配指定错误码
   * @param code - 错误码
   */
  is(code: string): boolean {
    return this.code === code;
  }
}

/**
 * 创建指定错误码的新错误
 * @param code - 错误码
 * @param message - 错误消息
 * @param cause - 原始错误
 */
export function newError(code: string, message: string, cause?: Error): AgentOSError {
  return new AgentOSError(message, code, cause);
}

/**
 * 格式化创建指定错误码的新错误
 * @param code - 错误码
 * @param format - 格式字符串
 * @param args - 参数
 */
export function newErrorf(code: string, format: string, ...args: unknown[]): AgentOSError {
  const message = format.replace(/%[vsd]/g, (_, index) => String(args[index]));
  return new AgentOSError(message, code);
}

/**
 * 包装已有错误并附加 SDK 错误码
 * @param code - 错误码
 * @param message - 错误消息
 * @param cause - 原始错误
 */
export function wrapError(code: string, message: string, cause: Error): AgentOSError {
  return new AgentOSError(message, code, cause);
}

// ============================================================
// 错误判断函数
// ============================================================

/**
 * 判断错误是否匹配指定错误码
 * @param err - 错误对象
 * @param code - 错误码
 */
export function isErrorCode(err: unknown, code: string): boolean {
  if (err instanceof AgentOSError) {
    return err.code === code;
  }
  return false;
}

/**
 * 判断是否为网络相关错误
 * @param err - 错误对象
 */
export function isNetworkError(err: unknown): boolean {
  if (err instanceof AgentOSError) {
    return (
      err.code === ErrorCode.NETWORK_ERROR ||
      err.code === ErrorCode.TIMEOUT ||
      err.code === ErrorCode.CONNECTION_REFUSED
    );
  }
  return false;
}

/**
 * 判断是否为服务端错误
 * @param err - 错误对象
 */
export function isServerError(err: unknown): boolean {
  if (err instanceof AgentOSError) {
    return (
      err.code === ErrorCode.SERVER_ERROR ||
      err.code === ErrorCode.RATE_LIMITED ||
      err.code === ErrorCode.TASK_FAILED ||
      err.code === ErrorCode.SKILL_EXECUTION_FAILED
    );
  }
  return false;
}

// ============================================================
// HTTP 状态码映射
// ============================================================

/**
 * HTTP 状态码到错误码的映射，与 Go SDK HTTPStatusToError 一致
 * @param status - HTTP 状态码
 */
export function httpStatusToErrorCode(status: number): string {
  const mapping: Record<number, string> = {
    400: ErrorCode.INVALID_PARAMETER,
    401: ErrorCode.UNAUTHORIZED,
    403: ErrorCode.FORBIDDEN,
    404: ErrorCode.NOT_FOUND,
    408: ErrorCode.TIMEOUT,
    409: ErrorCode.CONFLICT,
    429: ErrorCode.RATE_LIMITED,
    422: ErrorCode.VALIDATION_ERROR,
    500: ErrorCode.SERVER_ERROR,
    502: ErrorCode.SERVER_ERROR,
    503: ErrorCode.SERVER_ERROR,
    504: ErrorCode.TIMEOUT,
  };
  return mapping[status] || ErrorCode.UNKNOWN;
}

/**
 * 将 HTTP 状态码映射为 SDK 错误
 * @param statusCode - HTTP 状态码
 * @param message - 错误消息
 */
export function httpStatusToError(statusCode: number, message: string): AgentOSError {
  const code = httpStatusToErrorCode(statusCode);
  return newError(code, message);
}

// ============================================================
// 哨兵错误 (Err 前缀，支持错误匹配)
// ============================================================

/**
 * 哨兵错误实例，用于错误匹配
 */
export const Err = {
  Success: new AgentOSError('操作成功', ErrorCode.SUCCESS),
  Unknown: new AgentOSError('未知错误', ErrorCode.UNKNOWN),
  InvalidParameter: new AgentOSError('参数无效', ErrorCode.INVALID_PARAMETER),
  MissingParameter: new AgentOSError('缺少必要参数', ErrorCode.MISSING_PARAMETER),
  Timeout: new AgentOSError('操作超时', ErrorCode.TIMEOUT),
  NotFound: new AgentOSError('资源未找到', ErrorCode.NOT_FOUND),
  AlreadyExists: new AgentOSError('资源已存在', ErrorCode.ALREADY_EXISTS),
  Conflict: new AgentOSError('资源冲突', ErrorCode.CONFLICT),
  InvalidConfig: new AgentOSError('配置无效', ErrorCode.INVALID_CONFIG),
  InvalidEndpoint: new AgentOSError('端点地址无效', ErrorCode.INVALID_ENDPOINT),
  NetworkError: new AgentOSError('网络错误', ErrorCode.NETWORK_ERROR),
  ConnectionRefused: new AgentOSError('连接被拒绝', ErrorCode.CONNECTION_REFUSED),
  ServerError: new AgentOSError('服务端错误', ErrorCode.SERVER_ERROR),
  Unauthorized: new AgentOSError('未授权', ErrorCode.UNAUTHORIZED),
  Forbidden: new AgentOSError('访问被禁止', ErrorCode.FORBIDDEN),
  RateLimited: new AgentOSError('请求频率超限', ErrorCode.RATE_LIMITED),
  InvalidResponse: new AgentOSError('响应格式异常', ErrorCode.INVALID_RESPONSE),
  ParseError: new AgentOSError('数据解析失败', ErrorCode.PARSE_ERROR),
  ValidationError: new AgentOSError('数据验证失败', ErrorCode.VALIDATION_ERROR),
  NotSupported: new AgentOSError('操作不支持', ErrorCode.NOT_SUPPORTED),
  Internal: new AgentOSError('内部错误', ErrorCode.INTERNAL),
  Busy: new AgentOSError('系统繁忙', ErrorCode.BUSY),

  LoopCreateFailed: new AgentOSError('核心循环创建失败', ErrorCode.LOOP_CREATE_FAILED),
  LoopStartFailed: new AgentOSError('核心循环启动失败', ErrorCode.LOOP_START_FAILED),
  LoopStopFailed: new AgentOSError('核心循环停止失败', ErrorCode.LOOP_STOP_FAILED),

  CognitionFailed: new AgentOSError('认知处理失败', ErrorCode.COGNITION_FAILED),
  DAGBuildFailed: new AgentOSError('DAG 构建失败', ErrorCode.DAG_BUILD_FAILED),
  AgentDispatchFailed: new AgentOSError('Agent 调度失败', ErrorCode.AGENT_DISPATCH_FAILED),
  IntentParseFailed: new AgentOSError('意图解析失败', ErrorCode.INTENT_PARSE_FAILED),

  TaskFailed: new AgentOSError('任务执行失败', ErrorCode.TASK_FAILED),
  TaskCancelled: new AgentOSError('任务已取消', ErrorCode.TASK_CANCELLED),
  TaskTimeout: new AgentOSError('任务超时', ErrorCode.TASK_TIMEOUT),

  MemoryNotFound: new AgentOSError('记忆未找到', ErrorCode.MEMORY_NOT_FOUND),
  MemoryEvolveFailed: new AgentOSError('记忆演化失败', ErrorCode.MEMORY_EVOLVE_FAILED),
  MemorySearchFailed: new AgentOSError('记忆搜索失败', ErrorCode.MEMORY_SEARCH_FAILED),

  SessionNotFound: new AgentOSError('会话未找到', ErrorCode.SESSION_NOT_FOUND),
  SessionExpired: new AgentOSError('会话已过期', ErrorCode.SESSION_EXPIRED),

  SkillNotFound: new AgentOSError('技能未找到', ErrorCode.SKILL_NOT_FOUND),
  SkillExecution: new AgentOSError('技能执行失败', ErrorCode.SKILL_EXECUTION_FAILED),

  TelemetryError: new AgentOSError('遥测错误', ErrorCode.TELEMETRY_ERROR),
  PermissionDenied: new AgentOSError('权限不足', ErrorCode.PERMISSION_DENIED),
  CorruptedData: new AgentOSError('数据损坏', ErrorCode.CORRUPTED_DATA),
};

// ============================================================
// 专用错误类
// 使用工厂函数创建，避免重复代码
// ============================================================

/**
 * 创建专用错误类的工厂函数
 * @param className - 错误类名称
 * @param defaultMsg - 默认错误消息
 * @param errorCode - 错误码
 */
function createErrorClass(className: string, defaultMsg: string, errorCode: string) {
  return class extends AgentOSError {
    constructor(message: string = defaultMsg) {
      super(message, errorCode);
      this.name = className;
    }
  };
}

/** 网络错误类 */
export const NetworkError = createErrorClass('NetworkError', '网络连接失败', ErrorCode.NETWORK_ERROR);

/** HTTP 错误类 */
export class HttpError extends AgentOSError {
  /** HTTP 状态码 */
  public readonly statusCode: number;

  /**
   * 创建 HTTP 错误
   * @param message - 错误消息
   * @param statusCode - HTTP 状态码
   */
  constructor(message: string, statusCode: number) {
    super(message, httpStatusToErrorCode(statusCode));
    this.name = 'HttpError';
    this.statusCode = statusCode;
  }
}

/** 超时错误类 */
export const TimeoutError = createErrorClass('TimeoutError', '操作超时', ErrorCode.TIMEOUT);

/** 任务错误类 */
export const TaskError = createErrorClass('TaskError', '', ErrorCode.TASK_FAILED);

/** 记忆错误类 */
export const MemoryError = createErrorClass('MemoryError', '', ErrorCode.MEMORY_NOT_FOUND);

/** 会话错误类 */
export const SessionError = createErrorClass('SessionError', '', ErrorCode.SESSION_NOT_FOUND);

/** 技能错误类 */
export const SkillError = createErrorClass('SkillError', '', ErrorCode.SKILL_EXECUTION_FAILED);

/** 系统调用错误类 */
export const SyscallError = createErrorClass('SyscallError', '', ErrorCode.SYSCALL_ERROR);

/** 配置错误类 */
export const ConfigError = createErrorClass('ConfigError', '', ErrorCode.INVALID_CONFIG);

/** 限流错误类 */
export const RateLimitError = createErrorClass('RateLimitError', '请求频率超限', ErrorCode.RATE_LIMITED);

/** 验证错误类 */
export const ValidationError = createErrorClass('ValidationError', '数据验证失败', ErrorCode.VALIDATION_ERROR);

export interface AgentOSErrorStatic {
  new (message: string, code?: string, cause?: Error): AgentOSError;
  http(message: string): AgentOSError;
  network(message: string): AgentOSError;
}

function _httpError(message: string): AgentOSError {
  return new AgentOSError(message, ErrorCode.SERVER_ERROR);
}

function _networkError(message: string): AgentOSError {
  return new AgentOSError(message, ErrorCode.NETWORK_ERROR);
}

(AgentOSError as unknown as AgentOSErrorStatic).http = _httpError;
(AgentOSError as unknown as AgentOSErrorStatic).network = _networkError;
