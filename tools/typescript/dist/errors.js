"use strict";
// AgentOS TypeScript SDK Errors
// Version: 2.0.0
// Last updated: 2026-03-23
Object.defineProperty(exports, "__esModule", { value: true });
exports.RateLimitError = exports.ConfigError = exports.SyscallError = exports.SkillError = exports.SessionError = exports.MemoryError = exports.TaskError = exports.TimeoutError = exports.HttpError = exports.NetworkError = exports.AgentOSError = exports.ErrorCode = void 0;
exports.httpStatusToErrorCode = httpStatusToErrorCode;
/** 错误码常量，与 ErrorCodeReference.md 对齐 */
exports.ErrorCode = {
    SUCCESS: '0x0000',
    UNKNOWN: '0x0001',
    INVALID_PARAMETER: '0x0002',
    TIMEOUT: '0x0003',
    NETWORK_ERROR: '0x0004',
    NOT_FOUND: '0x0005',
    CONFLICT: '0x0006',
    VALIDATION_ERROR: '0x0007',
    INVALID_CONFIG: '0x0008',
    RATE_LIMIT: '0x0009',
    SERVER_ERROR: '0x000A',
    CORE_LOOP_ERROR: '0x1001',
    CORE_LOOP_TIMEOUT: '0x1002',
    TASK_FAILED: '0x3001',
    TASK_TIMEOUT: '0x3002',
    TASK_CANCELLED: '0x3003',
    MEMORY_NOT_FOUND: '0x4001',
    MEMORY_WRITE_FAILED: '0x4002',
    MEMORY_SEARCH_FAILED: '0x4003',
    SESSION_NOT_FOUND: '0x4004',
    SESSION_EXPIRED: '0x4005',
    SKILL_NOT_FOUND: '0x4006',
    SKILL_EXECUTION_FAILED: '0x4007',
    SYSCALL_ERROR: '0x5001',
    SYSCALL_TIMEOUT: '0x5002',
    SYSCALL_INVALID_NAMESPACE: '0x5003',
    SECURITY_ERROR: '0x6001',
    AUTH_FAILED: '0x6002',
    PERMISSION_DENIED: '0x6003',
    DYNAMIC_LOAD_ERROR: '0x7001',
    DYNAMIC_UNLOAD_ERROR: '0x7002',
};
/** HTTP 状态码到错误码的映射 */
function httpStatusToErrorCode(status) {
    const mapping = {
        400: exports.ErrorCode.INVALID_PARAMETER,
        401: exports.ErrorCode.AUTH_FAILED,
        403: exports.ErrorCode.PERMISSION_DENIED,
        404: exports.ErrorCode.NOT_FOUND,
        408: exports.ErrorCode.TIMEOUT,
        409: exports.ErrorCode.CONFLICT,
        422: exports.ErrorCode.VALIDATION_ERROR,
        429: exports.ErrorCode.RATE_LIMIT,
        500: exports.ErrorCode.SERVER_ERROR,
        502: exports.ErrorCode.SERVER_ERROR,
        503: exports.ErrorCode.SERVER_ERROR,
        504: exports.ErrorCode.TIMEOUT,
    };
    return mapping[status] || exports.ErrorCode.UNKNOWN;
}
/** AgentOS 基础错误类 */
class AgentOSError extends Error {
    /** 创建 AgentOS 错误 */
    constructor(message, code = exports.ErrorCode.UNKNOWN) {
        super(`[${code}] ${message}`);
        this.name = 'AgentOSError';
        this.code = code;
    }
}
exports.AgentOSError = AgentOSError;
/** 网络错误类 */
class NetworkError extends AgentOSError {
    /** 创建网络错误 */
    constructor(message = '网络连接失败') {
        super(message, exports.ErrorCode.NETWORK_ERROR);
        this.name = 'NetworkError';
    }
}
exports.NetworkError = NetworkError;
/** HTTP 错误类 */
class HttpError extends AgentOSError {
    /** 创建 HTTP 错误 */
    constructor(message, statusCode) {
        super(message, httpStatusToErrorCode(statusCode));
        this.statusCode = statusCode;
        this.name = 'HttpError';
    }
}
exports.HttpError = HttpError;
/** 超时错误类 */
class TimeoutError extends AgentOSError {
    /** 创建超时错误 */
    constructor(message = '操作超时') {
        super(message, exports.ErrorCode.TIMEOUT);
        this.name = 'TimeoutError';
    }
}
exports.TimeoutError = TimeoutError;
/** 任务错误类 */
class TaskError extends AgentOSError {
    /** 创建任务错误 */
    constructor(message) {
        super(message, exports.ErrorCode.TASK_FAILED);
        this.name = 'TaskError';
    }
}
exports.TaskError = TaskError;
/** 记忆错误类 */
class MemoryError extends AgentOSError {
    /** 创建记忆错误 */
    constructor(message) {
        super(message, exports.ErrorCode.MEMORY_NOT_FOUND);
        this.name = 'MemoryError';
    }
}
exports.MemoryError = MemoryError;
/** 会话错误类 */
class SessionError extends AgentOSError {
    /** 创建会话错误 */
    constructor(message) {
        super(message, exports.ErrorCode.SESSION_NOT_FOUND);
        this.name = 'SessionError';
    }
}
exports.SessionError = SessionError;
/** 技能错误类 */
class SkillError extends AgentOSError {
    /** 创建技能错误 */
    constructor(message) {
        super(message, exports.ErrorCode.SKILL_EXECUTION_FAILED);
        this.name = 'SkillError';
    }
}
exports.SkillError = SkillError;
/** 系统调用错误类 */
class SyscallError extends AgentOSError {
    /** 创建系统调用错误 */
    constructor(message) {
        super(message, exports.ErrorCode.SYSCALL_ERROR);
        this.name = 'SyscallError';
    }
}
exports.SyscallError = SyscallError;
/** 配置错误类 */
class ConfigError extends AgentOSError {
    /** 创建配置错误 */
    constructor(message) {
        super(message, exports.ErrorCode.INVALID_CONFIG);
        this.name = 'ConfigError';
    }
}
exports.ConfigError = ConfigError;
/** 限流错误类 */
class RateLimitError extends AgentOSError {
    /** 创建限流错误 */
    constructor(message = '请求频率超限') {
        super(message, exports.ErrorCode.RATE_LIMIT);
        this.name = 'RateLimitError';
    }
}
exports.RateLimitError = RateLimitError;
