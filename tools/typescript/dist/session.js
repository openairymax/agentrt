"use strict";
// AgentOS TypeScript SDK Session
// Version: 2.0.0
// Last updated: 2026-03-23
Object.defineProperty(exports, "__esModule", { value: true });
exports.Session = void 0;
/** AgentOS 会话管理类 */
class Session {
    /** 创建新的 Session 对象 */
    constructor(client, sessionId) {
        this.client = client;
        this.sessionId = sessionId;
    }
    /** 获取会话 ID */
    get id() {
        return this.sessionId;
    }
    /** 设置会话上下文值 */
    async setContext(key, value) {
        const response = await this.client.request('POST', `/api/sessions/${this.sessionId}/context`, { key, value });
        return response.success;
    }
    /** 获取会话上下文值 */
    async getContext(key) {
        const response = await this.client.request('GET', `/api/sessions/${this.sessionId}/context/${key}`);
        return response.value;
    }
    /** 删除会话上下文值 */
    async deleteContext(key) {
        const response = await this.client.request('DELETE', `/api/sessions/${this.sessionId}/context/${key}`);
        return response.success;
    }
    /** 获取所有上下文 */
    async getAllContext() {
        const response = await this.client.request('GET', `/api/sessions/${this.sessionId}/context`);
        return response.context;
    }
    /** 关闭会话 */
    async close() {
        const response = await this.client.request('DELETE', `/api/sessions/${this.sessionId}`);
        return response.success;
    }
}
exports.Session = Session;
