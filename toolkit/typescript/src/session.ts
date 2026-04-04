// AgentOS TypeScript SDK Session
// Version: 3.0.0
// Last updated: 2026-04-04

import { SessionError } from './errors';
import { AgentOS } from './agent';

/** AgentOS 会话管理�?*/
export class Session {
  private client: AgentOS;
  private sessionId: string;

  /** 创建新的 Session 对象 */
  constructor(client: AgentOS, sessionId: string) {
    this.client = client;
    this.sessionId = sessionId;
  }

  /** 获取会话 ID */
  get id(): string {
    return this.sessionId;
  }

  /** 设置会话上下文�?*/
  async setContext(key: string, value: any): Promise<boolean> {
    const response = await this.client.request<{ success: boolean }>(
      'POST',
      `/api/v1/sessions/${this.sessionId}/context`,
      { key, value },
    );
    return response.success;
  }

  /** 获取会话上下文�?*/
  async getContext(key: string): Promise<any> {
    const response = await this.client.request<{ value: any }>(
      'GET',
      `/api/v1/sessions/${this.sessionId}/context/${key}`,
    );
    return response.value;
  }

  /** 删除会话上下文�?*/
  async deleteContext(key: string): Promise<boolean> {
    const response = await this.client.request<{ success: boolean }>(
      'DELETE',
      `/api/v1/sessions/${this.sessionId}/context/${key}`,
    );
    return response.success;
  }

  /** 获取所有上下文 */
  async getAllContext(): Promise<Record<string, any>> {
    const response = await this.client.request<{ context: Record<string, any> }>(
      'GET',
      `/api/v1/sessions/${this.sessionId}/context`,
    );
    return response.context;
  }

  /** 关闭会话 */
  async close(): Promise<boolean> {
    const response = await this.client.request<{ success: boolean }>(
      'DELETE',
      `/api/v1/sessions/${this.sessionId}`,
    );
    return response.success;
  }
}
