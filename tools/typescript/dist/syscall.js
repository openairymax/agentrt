"use strict";
// AgentOS TypeScript SDK Syscall
// Version: 2.0.0
// Last updated: 2026-03-23
Object.defineProperty(exports, "__esModule", { value: true });
exports.SessionSyscall = exports.MemorySyscall = exports.TaskSyscall = exports.SyscallBinding = exports.SyscallNamespace = void 0;
/** 系统调用命名空间 */
var SyscallNamespace;
(function (SyscallNamespace) {
    SyscallNamespace["TASK"] = "task";
    SyscallNamespace["MEMORY"] = "memory";
    SyscallNamespace["SESSION"] = "session";
    SyscallNamespace["TELEMETRY"] = "telemetry";
})(SyscallNamespace || (exports.SyscallNamespace = SyscallNamespace = {}));
/** 系统调用绑定（抽象基类，供特定运行时实现） */
class SyscallBinding {
}
exports.SyscallBinding = SyscallBinding;
/** 任务系统调用便捷方法 */
class TaskSyscall {
    constructor(binding) {
        this.binding = binding;
    }
    /** 提交任务 */
    async submit(description) {
        return this.binding.invoke({
            namespace: SyscallNamespace.TASK,
            operation: 'submit',
            params: { description },
        });
    }
    /** 查询任务状态 */
    async query(taskId) {
        return this.binding.invoke({
            namespace: SyscallNamespace.TASK,
            operation: 'query',
            params: { task_id: taskId },
        });
    }
    /** 取消任务 */
    async cancel(taskId) {
        return this.binding.invoke({
            namespace: SyscallNamespace.TASK,
            operation: 'cancel',
            params: { task_id: taskId },
        });
    }
}
exports.TaskSyscall = TaskSyscall;
/** 记忆系统调用便捷方法 */
class MemorySyscall {
    constructor(binding) {
        this.binding = binding;
    }
    /** 写入记忆 */
    async write(content, metadata) {
        return this.binding.invoke({
            namespace: SyscallNamespace.MEMORY,
            operation: 'write',
            params: { content, metadata },
        });
    }
    /** 搜索记忆 */
    async search(query, topK = 5) {
        return this.binding.invoke({
            namespace: SyscallNamespace.MEMORY,
            operation: 'search',
            params: { query, top_k: topK },
        });
    }
    /** 删除记忆 */
    async delete(memoryId) {
        return this.binding.invoke({
            namespace: SyscallNamespace.MEMORY,
            operation: 'delete',
            params: { memory_id: memoryId },
        });
    }
}
exports.MemorySyscall = MemorySyscall;
/** 会话系统调用便捷方法 */
class SessionSyscall {
    constructor(binding) {
        this.binding = binding;
    }
    /** 创建会话 */
    async create() {
        return this.binding.invoke({
            namespace: SyscallNamespace.SESSION,
            operation: 'create',
        });
    }
    /** 设置上下文 */
    async setContext(sessionId, key, value) {
        return this.binding.invoke({
            namespace: SyscallNamespace.SESSION,
            operation: 'set_context',
            params: { session_id: sessionId, key, value },
        });
    }
    /** 获取上下文 */
    async getContext(sessionId, key) {
        return this.binding.invoke({
            namespace: SyscallNamespace.SESSION,
            operation: 'get_context',
            params: { session_id: sessionId, key },
        });
    }
}
exports.SessionSyscall = SessionSyscall;
