// AgentOS TypeScript SDK Agent
// Version: 1.0.0.5
// Last updated: 2026-03-21

import axios, { AxiosInstance, AxiosRequestConfig, AxiosResponse } from 'axios';
import { ClientConfig, Memory, TaskResult, SkillInfo, SkillResult } from './types';
import { NetworkError, HttpError, TimeoutError } from './errors';
import { Task } from './task';
import { Session } from './session';
import { Skill } from './skill';

/**
 * AgentOS client class
 */
export class AgentOS {
  private client: AxiosInstance;
  private endpoint: string;

  /**
   * Create a new AgentOS client
   * @param config Client configuration
   */
  constructor(config: ClientConfig = {}) {
    this.endpoint = config.endpoint || 'http://localhost:18789';
    this.endpoint = this.endpoint.endsWith('/') ? this.endpoint.slice(0, -1) : this.endpoint;

    this.client = axios.create({
      baseURL: this.endpoint,
      timeout: config.timeout || 30000,
      headers: {
        'Content-Type': 'application/json',
        ...config.headers,
      },
    });

    // Error handling interceptor
    this.client.interceptors.response.use(
      (response) => response,
      (error) => {
        if (error.code === 'ECONNABORTED') {
          throw new TimeoutError('Request timed out');
        } else if (error.code === 'ERR_NETWORK') {
          throw new NetworkError('Network error');
        } else if (error.response) {
          throw new HttpError(
            `Server returned error: ${error.response.status}`,
            error.response.status
          );
        }
        throw error;
      }
    );
  }

  /**
   * Make an HTTP request to the AgentOS server
   * @param method HTTP method
   * @param path API path
   * @param data Request data
   * @returns Response data
   */
  private async request<T>(method: string, path: string, data?: any): Promise<T> {
    const config: AxiosRequestConfig = {
      method,
      url: path,
      data,
    };

    try {
      const response: AxiosResponse<T> = await this.client(config);
      return response.data;
    } catch (error) {
      throw error;
    }
  }

  /**
   * Submit a task to the AgentOS system
   * @param taskDescription Task description
   * @returns Task object
   */
  async submitTask(taskDescription: string): Promise<Task> {
    const response = await this.request<{ task_id: string }>(
      'POST',
      '/api/tasks',
      { description: taskDescription }
    );

    if (!response.task_id) {
      throw new Error('Invalid response: missing task_id');
    }

    return new Task(this, response.task_id);
  }

  /**
   * Write a memory to the AgentOS system
   * @param content Memory content
   * @param metadata Optional metadata
   * @returns Memory ID
   */
  async writeMemory(content: string, metadata?: Record<string, any>): Promise<string> {
    const response = await this.request<{ memory_id: string }>(
      'POST',
      '/api/memories',
      { content, metadata: metadata || {} }
    );

    if (!response.memory_id) {
      throw new Error('Invalid response: missing memory_id');
    }

    return response.memory_id;
  }

  /**
   * Search memories in the AgentOS system
   * @param query Search query
   * @param topK Maximum number of results
   * @returns List of memories
   */
  async searchMemory(query: string, topK: number = 5): Promise<Memory[]> {
    const response = await this.request<{ memories: any[] }>(
      'GET',
      `/api/memories/search?query=${encodeURIComponent(query)}&top_k=${topK}`
    );

    if (!response.memories) {
      throw new Error('Invalid response: missing memories');
    }

    return response.memories.map((mem) => ({
      memoryId: mem.memory_id,
      content: mem.content,
      createdAt: mem.created_at,
      metadata: mem.metadata,
    }));
  }

  /**
   * Get a memory by ID
   * @param memoryId Memory ID
   * @returns Memory object
   */
  async getMemory(memoryId: string): Promise<Memory> {
    const response = await this.request<any>(
      'GET',
      `/api/memories/${memoryId}`
    );

    return {
      memoryId: response.memory_id,
      content: response.content,
      createdAt: response.created_at,
      metadata: response.metadata,
    };
  }

  /**
   * Delete a memory by ID
   * @param memoryId Memory ID
   * @returns True if the memory was deleted successfully
   */
  async deleteMemory(memoryId: string): Promise<boolean> {
    const response = await this.request<{ success: boolean }>(
      'DELETE',
      `/api/memories/${memoryId}`
    );

    return response.success;
  }

  /**
   * Create a new session
   * @returns Session object
   */
  async createSession(): Promise<Session> {
    const response = await this.request<{ session_id: string }>(
      'POST',
      '/api/sessions'
    );

    if (!response.session_id) {
      throw new Error('Invalid response: missing session_id');
    }

    return new Session(this, response.session_id);
  }

  /**
   * Load a skill by name
   * @param skillName Skill name
   * @returns Skill object
   */
  async loadSkill(skillName: string): Promise<Skill> {
    return new Skill(this, skillName);
  }
}
