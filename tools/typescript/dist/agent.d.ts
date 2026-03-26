import { ClientConfig, Memory } from './types';
import { Task } from './task';
import { Session } from './session';
import { Skill } from './skill';
/** AgentOS 客户端类 */
export declare class AgentOS {
    private client;
    private endpoint;
    /** 创建新的 AgentOS 客户端 */
    constructor(config?: ClientConfig);
    /** 向 AgentOS 服务端发起 HTTP 请求 */
    request<T>(method: string, path: string, data?: any): Promise<T>;
    /** 提交任务到 AgentOS 系统 */
    submitTask(taskDescription: string): Promise<Task>;
    /** 写入记忆到 AgentOS 系统 */
    writeMemory(content: string, metadata?: Record<string, any>): Promise<string>;
    /** 搜索记忆 */
    searchMemory(query: string, topK?: number): Promise<Memory[]>;
    /** 根据 ID 获取记忆 */
    getMemory(memoryId: string): Promise<Memory>;
    /** 根据 ID 删除记忆 */
    deleteMemory(memoryId: string): Promise<boolean>;
    /** 创建新会话 */
    createSession(): Promise<Session>;
    /** 加载技能 */
    loadSkill(skillName: string): Promise<Skill>;
    /** 健康检查 */
    health(): Promise<boolean>;
    /** 获取客户端端点地址 */
    getEndpoint(): string;
    /** 关闭客户端（释放资源） */
    close(): void;
}
