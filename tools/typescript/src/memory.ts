// AgentOS TypeScript SDK Memory
// Version: 2.0.0
// Last updated: 2026-03-23

import { MemoryError, AgentOSError } from './errors';
import { Memory, MemoryLayer } from './types';
import { AgentOS } from './agent';

/** AgentOS 记忆管理�?*/
export class MemoryManager {
  private client: AgentOS;

  /** 创建新的 MemoryManager */
  constructor(client: AgentOS) {
    this.client = client;
  }

  /** 写入记忆 */
  async write(
    content: string,
    metadata?: Record<string, any>,
    layer?: MemoryLayer,
  ): Promise<string> {
    const response = await this.client.request<{ memory_id: string }>(
      'POST',
      '/api/v1/memories',
      { content, metadata: metadata || {}, layer },
    );
    if (!response.memory_id) {
      throw new AgentOSError('响应格式异常: 缺少 memory_id');
    }
    return response.memory_id;
  }

  /** 获取记忆 */
  async get(memoryId: string): Promise<Memory> {
    const response = await this.client.request<any>(
      'GET',
      `/api/v1/memories/${memoryId}`,
    );
    return {
      memoryId: response.memory_id,
      content: response.content,
      createdAt: response.created_at,
      layer: response.layer,
      metadata: response.metadata,
    };
  }

  /** 搜索记忆 */
  async search(query: string, topK: number = 5): Promise<Memory[]> {
    const encodedQuery = encodeURIComponent(query);
    const response = await this.client.request<{ memories: any[] }>(
      'GET',
      `/api/v1/memories/search?query=${encodedQuery}&top_k=${topK}`,
    );
    return (response.memories || []).map((mem) => ({
      memoryId: mem.memory_id,
      content: mem.content,
      createdAt: mem.created_at,
      layer: mem.layer,
      metadata: mem.metadata,
    }));
  }

  /** 更新记忆 */
  async update(memoryId: string, content: string): Promise<boolean> {
    const response = await this.client.request<{ success: boolean }>(
      'PUT',
      `/api/v1/memories/${memoryId}`,
      { content },
    );
    return response.success;
  }

  /** 删除记忆 */
  async delete(memoryId: string): Promise<boolean> {
    const response = await this.client.request<{ success: boolean }>(
      'DELETE',
      `/api/v1/memories/${memoryId}`,
    );
    return response.success;
  }

  /** 按层级搜索记�?*/
  async searchByLayer(
    layer: MemoryLayer,
    topK: number = 10,
  ): Promise<Memory[]> {
    const response = await this.client.request<{ memories: any[] }>(
      'GET',
      `/api/v1/memories?layer=${layer}&top_k=${topK}`,
    );
    return (response.memories || []).map((mem) => ({
      memoryId: mem.memory_id,
      content: mem.content,
      createdAt: mem.created_at,
      layer: mem.layer,
      metadata: mem.metadata,
    }));
  }

  /** 记忆演化（升级层级） */
  async evolve(memoryId: string, targetLayer: MemoryLayer): Promise<boolean> {
    const response = await this.client.request<{ success: boolean }>(
      'POST',
      `/api/v1/memories/${memoryId}/evolve`,
      { target_layer: targetLayer },
    );
    return response.success;
  }

  /** 获取记忆统计 */
  async getStats(): Promise<{
    total: number;
    byLayer: Record<string, number>;
  }> {
    return this.client.request('/api/v1/memories/stats', 'GET');
  }
}
