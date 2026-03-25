/** 系统调用命名空间 */
export declare enum SyscallNamespace {
    TASK = "task",
    MEMORY = "memory",
    SESSION = "session",
    TELEMETRY = "telemetry"
}
/** 系统调用请求 */
export interface SyscallRequest {
    namespace: SyscallNamespace;
    operation: string;
    params?: Record<string, any>;
}
/** 系统调用响应 */
export interface SyscallResponse {
    success: boolean;
    data?: any;
    error?: string;
    error_code?: string;
}
/** 系统调用绑定（抽象基类，供特定运行时实现） */
export declare abstract class SyscallBinding {
    /** 执行系统调用 */
    abstract invoke(request: SyscallRequest): Promise<SyscallResponse>;
}
/** 任务系统调用便捷方法 */
export declare class TaskSyscall {
    private binding;
    constructor(binding: SyscallBinding);
    /** 提交任务 */
    submit(description: string): Promise<SyscallResponse>;
    /** 查询任务状态 */
    query(taskId: string): Promise<SyscallResponse>;
    /** 取消任务 */
    cancel(taskId: string): Promise<SyscallResponse>;
}
/** 记忆系统调用便捷方法 */
export declare class MemorySyscall {
    private binding;
    constructor(binding: SyscallBinding);
    /** 写入记忆 */
    write(content: string, metadata?: Record<string, any>): Promise<SyscallResponse>;
    /** 搜索记忆 */
    search(query: string, topK?: number): Promise<SyscallResponse>;
    /** 删除记忆 */
    delete(memoryId: string): Promise<SyscallResponse>;
}
/** 会话系统调用便捷方法 */
export declare class SessionSyscall {
    private binding;
    constructor(binding: SyscallBinding);
    /** 创建会话 */
    create(): Promise<SyscallResponse>;
    /** 设置上下文 */
    setContext(sessionId: string, key: string, value: any): Promise<SyscallResponse>;
    /** 获取上下文 */
    getContext(sessionId: string, key: string): Promise<SyscallResponse>;
}
