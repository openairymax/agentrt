// AgentRT TypeScript SDK Memory
// Version: 0.1.0
// Last updated: 2026-03-23

import { MemoryError, AgentOSError } from './errors';
import { Memory, MemoryLayer, MemoryRaw } from './types';
import { AgentRT } from './agent';

/** AgentRT 记忆管理�?*/
export class MemoryManager {
  private client: AgentRT;

  /** 创建新的 MemoryManager */
  constructor(client: AgentRT) {
    this.client = client;
  }

  /** 写入记忆 */
  async write(
    content: string,
    metadata?: Record<string, unknown>,
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
    const response = await this.client.request<MemoryRaw>(
      'GET',
      `/api/v1/memories/${memoryId}`,
    );
    return {
      id: response.memory_id || response.id || '',
      content: response.content,
      layer: response.layer as MemoryLayer,
      score: response.score || 0,
      createdAt: new Date(response.created_at || response.createdAt || Date.now()),
      updatedAt: new Date(response.updated_at || response.updatedAt || Date.now()),
      metadata: response.metadata,
    };
  }

  /** 搜索记忆 */
  async search(query: string, topK: number = 5): Promise<Memory[]> {
    const encodedQuery = encodeURIComponent(query);
    const response = await this.client.request<{ memories: MemoryRaw[] }>(
      'GET',
      `/api/v1/memories/search?query=${encodedQuery}&top_k=${topK}`,
    );
    return (response.memories || []).map((mem: MemoryRaw) => ({
      id: mem.memory_id || mem.id || '',
      content: mem.content,
      layer: mem.layer as MemoryLayer,
      score: mem.score || 0,
      createdAt: new Date(mem.created_at || mem.createdAt || Date.now()),
      updatedAt: new Date(mem.updated_at || mem.updatedAt || Date.now()),
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

  /** 按层级搜索记忆 */
  async searchByLayer(
    layer: MemoryLayer,
    topK: number = 10,
  ): Promise<Memory[]> {
    const response = await this.client.request<{ memories: MemoryRaw[] }>(
      'GET',
      `/api/v1/memories?layer=${layer}&top_k=${topK}`,
    );
    return (response.memories || []).map((mem: MemoryRaw) => ({
      id: mem.memory_id || mem.id || '',
      content: mem.content,
      layer: mem.layer as MemoryLayer,
      score: mem.score || 0,
      createdAt: new Date(mem.created_at || mem.createdAt || Date.now()),
      updatedAt: new Date(mem.updated_at || mem.updatedAt || Date.now()),
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
