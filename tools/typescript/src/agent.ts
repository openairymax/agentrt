// AgentOS TypeScript SDK Agent
// Version: 2.0.0
// Last updated: 2026-03-23

import axios, { AxiosInstance, AxiosRequestConfig, AxiosResponse } from 'axios';
import { ClientConfig, Memory, TaskResult, SkillInfo, SkillResult } from './types';
import { NetworkError, HttpError, TimeoutError, AgentOSError } from './errors';
import { Task } from './task';
import { Session } from './session';
import { Skill } from './skill';

/** AgentOS 客户端类 */
export class AgentOS {
  private client: AxiosInstance;
  private endpoint: string;

  /** 创建新的 AgentOS 客户�?*/
  constructor(config: ClientConfig = {}) {
    this.endpoint = config.endpoint || 'http://localhost:18789';
    this.endpoint = this.endpoint.endsWith('/')
      ? this.endpoint.slice(0, -1)
      : this.endpoint;

    this.client = axios.create({
      baseURL: this.endpoint,
      timeout: config.timeout || 30000,
      headers: {
        'Content-Type': 'application/json',
        ...config.headers,
      },
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
            `服务端返回错�? ${error.response.status}`,
            error.response.status,
          );
        }
        throw new AgentOSError(error.message || '未知错误');
      },
    );
  }

  /** �?AgentOS 服务端发�?HTTP 请求 */
  async request<T>(method: string, path: string, data?: any): Promise<T> {
    const config: AxiosRequestConfig = { method, url: path, data };
    const response: AxiosResponse<T> = await this.client(config);
    return response.data;
  }

  /** 提交任务�?AgentOS 系统 */
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

  /** 写入记忆�?AgentOS 系统 */
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
    return response.memories.map((mem) => ({
      memoryId: mem.memory_id,
      content: mem.content,
      createdAt: mem.created_at,
      metadata: mem.metadata,
    }));
  }

  /** 根据 ID 获取记忆 */
  async getMemory(memoryId: string): Promise<Memory> {
    const response = await this.request<any>('GET', `/api/v1/memories/${memoryId}`);
    return {
      memoryId: response.memory_id,
      content: response.content,
      createdAt: response.created_at,
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

  /** 创建新会�?*/
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

  /** 加载技�?*/
  async loadSkill(skillName: string): Promise<Skill> {
    return new Skill(this, skillName);
  }

  /** 健康检�?*/
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

  /** 关闭客户端（释放资源�?*/
  close(): void {
    this.client.interceptors.response.clear();
  }
}
