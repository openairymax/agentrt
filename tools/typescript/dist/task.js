"use strict";
// AgentOS TypeScript SDK Task
// Version: 2.0.0
// Last updated: 2026-03-23
Object.defineProperty(exports, "__esModule", { value: true });
exports.Task = void 0;
const types_1 = require("./types");
const errors_1 = require("./errors");
/** AgentOS 任务管理类 */
class Task {
    /** 创建新的 Task 对象 */
    constructor(client, taskId) {
        this.client = client;
        this.taskId = taskId;
    }
    /** 获取任务 ID */
    get id() {
        return this.taskId;
    }
    /** 查询任务状态 */
    async query() {
        const response = await this.client.request('GET', `/api/tasks/${this.taskId}`);
        if (!response.status) {
            throw new errors_1.TaskError('响应格式异常: 缺少 status');
        }
        return response.status;
    }
    /** 等待任务完成 */
    async wait(options) {
        const startTime = Date.now();
        const timeout = (options === null || options === void 0 ? void 0 : options.timeout) || 0;
        while (true) {
            const status = await this.query();
            if (status === types_1.TaskStatus.COMPLETED ||
                status === types_1.TaskStatus.FAILED ||
                status === types_1.TaskStatus.CANCELLED) {
                const response = await this.client.request('GET', `/api/tasks/${this.taskId}`);
                return {
                    taskId: this.taskId,
                    status,
                    output: response.output,
                    error: response.error,
                };
            }
            if (timeout > 0 && Date.now() - startTime > timeout) {
                throw new errors_1.TimeoutError(`任务在 ${timeout}ms 内未完成`);
            }
            await new Promise((resolve) => setTimeout(resolve, 500));
        }
    }
    /** 取消任务 */
    async cancel() {
        const response = await this.client.request('POST', `/api/tasks/${this.taskId}/cancel`);
        return response.success;
    }
}
exports.Task = Task;
