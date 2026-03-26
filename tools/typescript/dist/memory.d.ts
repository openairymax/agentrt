import { Memory, MemoryLayer } from './types';
import { AgentOS } from './agent';
/** AgentOS 记忆管理类 */
export declare class MemoryManager {
    private client;
    /** 创建新的 MemoryManager */
    constructor(client: AgentOS);
    /** 写入记忆 */
    write(content: string, metadata?: Record<string, any>, layer?: MemoryLayer): Promise<string>;
    /** 获取记忆 */
    get(memoryId: string): Promise<Memory>;
    /** 搜索记忆 */
    search(query: string, topK?: number): Promise<Memory[]>;
    /** 更新记忆 */
    update(memoryId: string, content: string): Promise<boolean>;
    /** 删除记忆 */
    delete(memoryId: string): Promise<boolean>;
    /** 按层级搜索记忆 */
    searchByLayer(layer: MemoryLayer, topK?: number): Promise<Memory[]>;
    /** 记忆演化（升级层级） */
    evolve(memoryId: string, targetLayer: MemoryLayer): Promise<boolean>;
    /** 获取记忆统计 */
    getStats(): Promise<{
        total: number;
        byLayer: Record<string, number>;
    }>;
}
