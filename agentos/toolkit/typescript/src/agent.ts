// AgentOS TypeScript SDK Agent
// Version: 2.0.0
// Last updated: 2026-03-23

import axios, { AxiosInstance, AxiosRequestConfig, AxiosResponse } from 'axios';
import { ClientConfig } from './manager';
import { Memory, TaskResult, SkillInfo, SkillResult } from './types';
import { NetworkError, HttpError, TimeoutError, AgentOSError } from './errors';
import { Task } from './task';
import { Session } from './session';
import { Skill } from './skill';
import { getLogger } from './utils/logger';

/** AgentOS 客户端类 */
export class AgentOS {
  private client: AxiosInstance;
  private endpoint: string;
  private apiKey?: string;
  private maxRetries: number;
  private retryDelay: number;

  /** 创建新的 AgentOS 客户端 */
  constructor(manager: ClientConfig = {}) {
    this.endpoint = manager.endpoint || 'http://localhost:18789';
    this.endpoint = this.endpoint.endsWith('/')
      ? this.endpoint.slice(0, -1)
      : this.endpoint;
    this.apiKey = manager.apiKey;
    this.maxRetries = manager.maxRetries ?? 3;
    this.retryDelay = manager.retryDelay ?? 1000;

    const headers: Record<string, string> = {
      'Content-Type': 'application/json',
      ...manager.headers,
    };
    if (manager.apiKey) {
      headers['Authorization'] = `Bearer ${manager.apiKey}`;
    }

    this.client = axios.create({
      baseURL: this.endpoint,
      timeout: manager.timeout || 30000,
      headers,
    });

    this.client.interceptors.response.use(
      (response) => response,
      (error) => {
        if (error.code === 'ECONNABORTED') {
          throw new TimeoutError('请求超时');
        } else if (error.code === 'ERR_NETWORK') {
          throw new NetworkError('网络错误');
        } else if (error.response) {
          throw new HttpError(
            `服务端返回错误: ${error.response.status}`,
            error.response.status,
          );
        }
        throw new AgentOSError(error.message || '未知错误');
      },
    );
  }

  /** 延迟函数 */
  private sleep(ms: number): Promise<void> {
    return new Promise((resolve) => setTimeout(resolve, ms));
  }

  /** 判断是否可重试的错误 */
  private isRetryableError(error: any): boolean {
    if (error instanceof NetworkError || error instanceof TimeoutError) {
      return true;
    }
    if (error instanceof HttpError) {
      const status = error.statusCode;
      return status >= 500 || status === 429;
    }
    return false;
  }

  /** 向 AgentOS 服务端发送 HTTP 请求（带重试） */
  async request<T>(method: string, path: string, data?: any): Promise<T> {
    let lastError: any;

    for (let attempt = 0; attempt <= this.maxRetries; attempt++) {
      try {
        const manager: AxiosRequestConfig = { method, url: path, data };
        const response: AxiosResponse<T> = await this.client(manager);
        return response.data;
      } catch (error) {
        lastError = error;

        if (attempt === this.maxRetries) {
          break;
        }

        if (!this.isRetryableError(error)) {
          throw error;
        }

        const delay = this.retryDelay * Math.pow(2, attempt);
        getLogger().warn(`请求失败，${delay}ms 后重试 (尝试 ${attempt + 1}/${this.maxRetries}): ${error}`);
        await this.sleep(delay);
      }
    }

    throw lastError;
  }

  /** 提交任务到 AgentOS 系统 */
  async submitTask(taskDescription: string): Promise<Task> {
    const response = await this.request<{ task_id: string }>(
      'POST',
      '/api/v1/tasks',
      { description: taskDescription },
    );
    if (!response.task_id) {
      throw new AgentOSError('响应格式异常: 缺少 task_id');
    }
    return new Task(this, response.task_id);
  }

  /** 写入记忆到 AgentOS 系统 */
  async writeMemory(content: string, metadata?: Record<string, any>): Promise<string> {
    const response = await this.request<{ memory_id: string }>(
      'POST',
      '/api/v1/memories',
      { content, metadata: metadata || {} },
    );
    if (!response.memory_id) {
      throw new AgentOSError('响应格式异常: 缺少 memory_id');
    }
    return response.memory_id;
  }

  /** 搜索记忆 */
  async searchMemory(query: string, topK: number = 5): Promise<Memory[]> {
    const encodedQuery = encodeURIComponent(query);
    const response = await this.request<{ memories: any[] }>(
      'GET',
      `/api/v1/memories/search?query=${encodedQuery}&top_k=${topK}`,
    );
    if (!response.memories) {
      throw new AgentOSError('响应格式异常: 缺少 memories');
    }
    return response.memories.map((mem: any) => ({
      id: mem.memory_id || mem.id,
      content: mem.content,
      layer: mem.layer,
      score: mem.score || 0,
      createdAt: new Date(mem.created_at || mem.createdAt),
      updatedAt: new Date(mem.updated_at || mem.updatedAt || Date.now()),
      metadata: mem.metadata,
    }));
  }

  /** 根据 ID 获取记忆 */
  async getMemory(memoryId: string): Promise<Memory> {
    const response = await this.request<any>('GET', `/api/v1/memories/${memoryId}`);
    return {
      id: response.memory_id || response.id,
      content: response.content,
      layer: response.layer,
      score: response.score || 0,
      createdAt: new Date(response.created_at || response.createdAt),
      updatedAt: new Date(response.updated_at || response.updatedAt || Date.now()),
      metadata: response.metadata,
    };
  }

  /** 根据 ID 删除记忆 */
  async deleteMemory(memoryId: string): Promise<boolean> {
    const response = await this.request<{ success: boolean }>(
      'DELETE',
      `/api/v1/memories/${memoryId}`,
    );
    return response.success;
  }

  /** 创建新会话 */
  async createSession(): Promise<Session> {
    const response = await this.request<{ session_id: string }>(
      'POST',
      '/api/v1/sessions',
    );
    if (!response.session_id) {
      throw new AgentOSError('响应格式异常: 缺少 session_id');
    }
    return new Session(this, response.session_id);
  }

  /** 加载技能 */
  async loadSkill(skillName: string): Promise<Skill> {
    return new Skill(this, skillName);
  }

  /** 健康检查 */
  async health(): Promise<boolean> {
    try {
      await this.request<any>('GET', '/api/v1/health');
      return true;
    } catch {
      return false;
    }
  }

  /** 获取客户端端点地址 */
  getEndpoint(): string {
    return this.endpoint;
  }

  /** 关闭客户端（释放资源） */
  close(): void {
    this.client.interceptors.response.clear();
  }
}
