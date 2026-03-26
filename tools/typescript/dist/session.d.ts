import { AgentOS } from './agent';
/** AgentOS 会话管理类 */
export declare class Session {
    private client;
    private sessionId;
    /** 创建新的 Session 对象 */
    constructor(client: AgentOS, sessionId: string);
    /** 获取会话 ID */
    get id(): string;
    /** 设置会话上下文值 */
    setContext(key: string, value: any): Promise<boolean>;
    /** 获取会话上下文值 */
    getContext(key: string): Promise<any>;
    /** 删除会话上下文值 */
    deleteContext(key: string): Promise<boolean>;
    /** 获取所有上下文 */
    getAllContext(): Promise<Record<string, any>>;
    /** 关闭会话 */
    close(): Promise<boolean>;
}
