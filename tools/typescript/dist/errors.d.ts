/** 错误码常量，与 ErrorCodeReference.md 对齐 */
export declare const ErrorCode: {
    readonly SUCCESS: "0x0000";
    readonly UNKNOWN: "0x0001";
    readonly INVALID_PARAMETER: "0x0002";
    readonly TIMEOUT: "0x0003";
    readonly NETWORK_ERROR: "0x0004";
    readonly NOT_FOUND: "0x0005";
    readonly CONFLICT: "0x0006";
    readonly VALIDATION_ERROR: "0x0007";
    readonly INVALID_CONFIG: "0x0008";
    readonly RATE_LIMIT: "0x0009";
    readonly SERVER_ERROR: "0x000A";
    readonly CORE_LOOP_ERROR: "0x1001";
    readonly CORE_LOOP_TIMEOUT: "0x1002";
    readonly TASK_FAILED: "0x3001";
    readonly TASK_TIMEOUT: "0x3002";
    readonly TASK_CANCELLED: "0x3003";
    readonly MEMORY_NOT_FOUND: "0x4001";
    readonly MEMORY_WRITE_FAILED: "0x4002";
    readonly MEMORY_SEARCH_FAILED: "0x4003";
    readonly SESSION_NOT_FOUND: "0x4004";
    readonly SESSION_EXPIRED: "0x4005";
    readonly SKILL_NOT_FOUND: "0x4006";
    readonly SKILL_EXECUTION_FAILED: "0x4007";
    readonly SYSCALL_ERROR: "0x5001";
    readonly SYSCALL_TIMEOUT: "0x5002";
    readonly SYSCALL_INVALID_NAMESPACE: "0x5003";
    readonly SECURITY_ERROR: "0x6001";
    readonly AUTH_FAILED: "0x6002";
    readonly PERMISSION_DENIED: "0x6003";
    readonly DYNAMIC_LOAD_ERROR: "0x7001";
    readonly DYNAMIC_UNLOAD_ERROR: "0x7002";
};
/** 错误码类型 */
export type ErrorCodes = (typeof ErrorCode)[keyof typeof ErrorCode];
/** HTTP 状态码到错误码的映射 */
export declare function httpStatusToErrorCode(status: number): string;
/** AgentOS 基础错误类 */
export declare class AgentOSError extends Error {
    readonly code: string;
    /** 创建 AgentOS 错误 */
    constructor(message: string, code?: string);
}
/** 网络错误类 */
export declare class NetworkError extends AgentOSError {
    /** 创建网络错误 */
    constructor(message?: string);
}
/** HTTP 错误类 */
export declare class HttpError extends AgentOSError {
    readonly statusCode: number;
    /** 创建 HTTP 错误 */
    constructor(message: string, statusCode: number);
}
/** 超时错误类 */
export declare class TimeoutError extends AgentOSError {
    /** 创建超时错误 */
    constructor(message?: string);
}
/** 任务错误类 */
export declare class TaskError extends AgentOSError {
    /** 创建任务错误 */
    constructor(message: string);
}
/** 记忆错误类 */
export declare class MemoryError extends AgentOSError {
    /** 创建记忆错误 */
    constructor(message: string);
}
/** 会话错误类 */
export declare class SessionError extends AgentOSError {
    /** 创建会话错误 */
    constructor(message: string);
}
/** 技能错误类 */
export declare class SkillError extends AgentOSError {
    /** 创建技能错误 */
    constructor(message: string);
}
/** 系统调用错误类 */
export declare class SyscallError extends AgentOSError {
    /** 创建系统调用错误 */
    constructor(message: string);
}
/** 配置错误类 */
export declare class ConfigError extends AgentOSError {
    /** 创建配置错误 */
    constructor(message: string);
}
/** 限流错误类 */
export declare class RateLimitError extends AgentOSError {
    /** 创建限流错误 */
    constructor(message?: string);
}
