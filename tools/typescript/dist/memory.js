"use strict";
// AgentOS TypeScript SDK Memory
// Version: 2.0.0
// Last updated: 2026-03-23
Object.defineProperty(exports, "__esModule", { value: true });
exports.MemoryManager = void 0;
const errors_1 = require("./errors");
/** AgentOS 记忆管理类 */
class MemoryManager {
    /** 创建新的 MemoryManager */
    constructor(client) {
        this.client = client;
    }
    /** 写入记忆 */
    async write(content, metadata, layer) {
        const response = await this.client.request('POST', '/api/memories', { content, metadata: metadata || {}, layer });
        if (!response.memory_id) {
            throw new errors_1.AgentOSError('响应格式异常: 缺少 memory_id');
        }
        return response.memory_id;
    }
    /** 获取记忆 */
    async get(memoryId) {
        const response = await this.client.request('GET', `/api/memories/${memoryId}`);
        return {
            memoryId: response.memory_id,
            content: response.content,
            createdAt: response.created_at,
            layer: response.layer,
            metadata: response.metadata,
        };
    }
    /** 搜索记忆 */
    async search(query, topK = 5) {
        const encodedQuery = encodeURIComponent(query);
        const response = await this.client.request('GET', `/api/memories/search?query=${encodedQuery}&top_k=${topK}`);
        return (response.memories || []).map((mem) => ({
            memoryId: mem.memory_id,
            content: mem.content,
            createdAt: mem.created_at,
            layer: mem.layer,
            metadata: mem.metadata,
        }));
    }
    /** 更新记忆 */
    async update(memoryId, content) {
        const response = await this.client.request('PUT', `/api/memories/${memoryId}`, { content });
        return response.success;
    }
    /** 删除记忆 */
    async delete(memoryId) {
        const response = await this.client.request('DELETE', `/api/memories/${memoryId}`);
        return response.success;
    }
    /** 按层级搜索记忆 */
    async searchByLayer(layer, topK = 10) {
        const response = await this.client.request('GET', `/api/memories?layer=${layer}&top_k=${topK}`);
        return (response.memories || []).map((mem) => ({
            memoryId: mem.memory_id,
            content: mem.content,
            createdAt: mem.created_at,
            layer: mem.layer,
            metadata: mem.metadata,
        }));
    }
    /** 记忆演化（升级层级） */
    async evolve(memoryId, targetLayer) {
        const response = await this.client.request('POST', `/api/memories/${memoryId}/evolve`, { target_layer: targetLayer });
        return response.success;
    }
    /** 获取记忆统计 */
    async getStats() {
        return this.client.request('/api/memories/stats', 'GET');
    }
}
exports.MemoryManager = MemoryManager;
