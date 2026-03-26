import { TaskStatus, TaskResult } from './types';
import { AgentOS } from './agent';
/** AgentOS 任务管理类 */
export declare class Task {
    private client;
    private taskId;
    /** 创建新的 Task 对象 */
    constructor(client: AgentOS, taskId: string);
    /** 获取任务 ID */
    get id(): string;
    /** 查询任务状态 */
    query(): Promise<TaskStatus>;
    /** 等待任务完成 */
    wait(options?: {
        timeout?: number;
    }): Promise<TaskResult>;
    /** 取消任务 */
    cancel(): Promise<boolean>;
}
