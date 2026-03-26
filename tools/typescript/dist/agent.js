"use strict";
// AgentOS TypeScript SDK Agent
// Version: 2.0.0
// Last updated: 2026-03-23
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.AgentOS = void 0;
const axios_1 = __importDefault(require("axios"));
const errors_1 = require("./errors");
const task_1 = require("./task");
const session_1 = require("./session");
const skill_1 = require("./skill");
/** AgentOS 客户端类 */
class AgentOS {
    /** 创建新的 AgentOS 客户端 */
    constructor(config = {}) {
        this.endpoint = config.endpoint || 'http://localhost:18789';
        this.endpoint = this.endpoint.endsWith('/')
            ? this.endpoint.slice(0, -1)
            : this.endpoint;
        this.client = axios_1.default.create({
            baseURL: this.endpoint,
            timeout: config.timeout || 30000,
            headers: {
                'Content-Type': 'application/json',
                ...config.headers,
            },
        });
        this.client.interceptors.response.use((response) => response, (error) => {
            if (error.code === 'ECONNABORTED') {
                throw new errors_1.TimeoutError('请求超时');
            }
            else if (error.code === 'ERR_NETWORK') {
                throw new errors_1.NetworkError('网络错误');
            }
            else if (error.response) {
                throw new errors_1.HttpError(`服务端返回错误: ${error.response.status}`, error.response.status);
            }
            throw new errors_1.AgentOSError(error.message || '未知错误');
        });
    }
    /** 向 AgentOS 服务端发起 HTTP 请求 */
    async request(method, path, data) {
        const config = { method, url: path, data };
        const response = await this.client(config);
        return response.data;
    }
    /** 提交任务到 AgentOS 系统 */
    async submitTask(taskDescription) {
        const response = await this.request('POST', '/api/tasks', { description: taskDescription });
        if (!response.task_id) {
            throw new errors_1.AgentOSError('响应格式异常: 缺少 task_id');
        }
        return new task_1.Task(this, response.task_id);
    }
    /** 写入记忆到 AgentOS 系统 */
    async writeMemory(content, metadata) {
        const response = await this.request('POST', '/api/memories', { content, metadata: metadata || {} });
        if (!response.memory_id) {
            throw new errors_1.AgentOSError('响应格式异常: 缺少 memory_id');
        }
        return response.memory_id;
    }
    /** 搜索记忆 */
    async searchMemory(query, topK = 5) {
        const encodedQuery = encodeURIComponent(query);
        const response = await this.request('GET', `/api/memories/search?query=${encodedQuery}&top_k=${topK}`);
        if (!response.memories) {
            throw new errors_1.AgentOSError('响应格式异常: 缺少 memories');
        }
        return response.memories.map((mem) => ({
            memoryId: mem.memory_id,
            content: mem.content,
            createdAt: mem.created_at,
            metadata: mem.metadata,
        }));
    }
    /** 根据 ID 获取记忆 */
    async getMemory(memoryId) {
        const response = await this.request('GET', `/api/memories/${memoryId}`);
        return {
            memoryId: response.memory_id,
            content: response.content,
            createdAt: response.created_at,
            metadata: response.metadata,
        };
    }
    /** 根据 ID 删除记忆 */
    async deleteMemory(memoryId) {
        const response = await this.request('DELETE', `/api/memories/${memoryId}`);
        return response.success;
    }
    /** 创建新会话 */
    async createSession() {
        const response = await this.request('POST', '/api/sessions');
        if (!response.session_id) {
            throw new errors_1.AgentOSError('响应格式异常: 缺少 session_id');
        }
        return new session_1.Session(this, response.session_id);
    }
    /** 加载技能 */
    async loadSkill(skillName) {
        return new skill_1.Skill(this, skillName);
    }
    /** 健康检查 */
    async health() {
        try {
            await this.request('GET', '/api/v1/health');
            return true;
        }
        catch (_a) {
            return false;
        }
    }
    /** 获取客户端端点地址 */
    getEndpoint() {
        return this.endpoint;
    }
    /** 关闭客户端（释放资源） */
    close() {
        this.client.interceptors.response.clear();
    }
}
exports.AgentOS = AgentOS;
