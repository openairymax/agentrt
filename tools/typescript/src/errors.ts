// AgentOS TypeScript SDK Errors
// Version: 2.0.0
// Last updated: 2026-03-23
// �?Go SDK errors.go 保持一致的十六进制错误码体�?
/** 错误码常量，�?Go SDK ErrorCodeReference.md 对齐 */
export const ErrorCode = {
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

  LOOP_CREATE_FAILED: '0x1001',
  LOOP_START_FAILED: '0x1002',
  LOOP_STOP_FAILED: '0x1003',

  COGNITION_FAILED: '0x2001',
  DAG_BUILD_FAILED: '0x2002',
  AGENT_DISPATCH_FAILED: '0x2003',
  INTENT_PARSE_FAILED: '0x2004',

  TASK_FAILED: '0x3001',
  TASK_CANCELLED: '0x3002',
  TASK_TIMEOUT: '0x3003',

  MEMORY_NOT_FOUND: '0x4001',
  MEMORY_EVOLVE_FAILED: '0x4002',
  MEMORY_SEARCH_FAILED: '0x4003',
  SESSION_NOT_FOUND: '0x4004',
  SESSION_EXPIRED: '0x4005',
  SKILL_NOT_FOUND: '0x4006',
  SKILL_EXECUTION_FAILED: '0x4007',

  TELEMETRY_ERROR: '0x5001',

  PERMISSION_DENIED: '0x6001',
  CORRUPTED_DATA: '0x6002',
} as const;

/** 错误码类�?*/
export type ErrorCodes = (typeof ErrorCode)[keyof typeof ErrorCode];

/** HTTP 状态码到错误码的映射，�?Go SDK HTTPStatusToError 一�?*/
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

/** AgentOS 基础错误�?*/
export class AgentOSError extends Error {
  public readonly code: string;

  /** 创建 AgentOS 错误 */
  constructor(message: string, code: string = ErrorCode.UNKNOWN) {
    super(`[${code}] ${message}`);
    this.name = 'AgentOSError';
    this.code = code;
  }
}

/** 网络错误�?*/
export class NetworkError extends AgentOSError {
  /** 创建网络错误 */
  constructor(message: string = '网络连接失败') {
    super(message, ErrorCode.NETWORK_ERROR);
    this.name = 'NetworkError';
  }
}

/** HTTP 错误�?*/
export class HttpError extends AgentOSError {
  /** 创建 HTTP 错误 */
  constructor(message: string, public readonly statusCode: number) {
    super(message, httpStatusToErrorCode(statusCode));
    this.name = 'HttpError';
  }
}

/** 超时错误�?*/
export class TimeoutError extends AgentOSError {
  /** 创建超时错误 */
  constructor(message: string = '操作超时') {
    super(message, ErrorCode.TIMEOUT);
    this.name = 'TimeoutError';
  }
}

/** 任务错误�?*/
export class TaskError extends AgentOSError {
  /** 创建任务错误 */
  constructor(message: string) {
    super(message, ErrorCode.TASK_FAILED);
    this.name = 'TaskError';
  }
}

/** 记忆错误�?*/
export class MemoryError extends AgentOSError {
  /** 创建记忆错误 */
  constructor(message: string) {
    super(message, ErrorCode.MEMORY_NOT_FOUND);
    this.name = 'MemoryError';
  }
}

/** 会话错误�?*/
export class SessionError extends AgentOSError {
  /** 创建会话错误 */
  constructor(message: string) {
    super(message, ErrorCode.SESSION_NOT_FOUND);
    this.name = 'SessionError';
  }
}

/** 技能错误类 */
export class SkillError extends AgentOSError {
  /** 创建技能错�?*/
  constructor(message: string) {
    super(message, ErrorCode.SKILL_EXECUTION_FAILED);
    this.name = 'SkillError';
  }
}

/** 系统调用错误�?*/
export class SyscallError extends AgentOSError {
  /** 创建系统调用错误 */
  constructor(message: string) {
    super(message, ErrorCode.TELEMETRY_ERROR);
    this.name = 'SyscallError';
  }
}

/** 配置错误�?*/
export class ConfigError extends AgentOSError {
  /** 创建配置错误 */
  constructor(message: string) {
    super(message, ErrorCode.INVALID_CONFIG);
    this.name = 'ConfigError';
  }
}

/** 限流错误�?*/
export class RateLimitError extends AgentOSError {
  /** 创建限流错误 */
  constructor(message: string = '请求频率超限') {
    super(message, ErrorCode.RATE_LIMITED);
    this.name = 'RateLimitError';
  }
}
