// AgentOS TypeScript SDK - Memory Manager Module
// Version: 3.0.0
// Last updated: 2026-03-24
//
// 提供记忆的写入、搜索、更新、删除及统计功能。
// 与 Go SDK modules/memory/manager.go 保持一致。

import { APIClient } from '../client';
import {
  Memory,
  MemorySearchResult,
  MemoryLayer,
  ListOptions,
  APIResponse,
} from '../types';
import { AgentOSError, ErrorCode } from '../errors';
import { extractDataMap, getString, getInt64, getFloat64, getMap, parseTime, getInterfaceSlice } from '../utils';

/**
 * MemoryWriteItem 批量写入时的单条记忆项
 */
export interface MemoryWriteItem {
  /** 内容 */
  content: string;
  /** 记忆层级 */
  layer: MemoryLayer;
  /** 元数据 */
  metadata?: Record<string, unknown>;
}

/**
 * MemoryManager 管理记忆完整生命周期
 */
export class MemoryManager {
  private api: APIClient;

  /**
   * 创建新的记忆管理器实例
   * @param api - API 客户端
   */
  constructor(api: APIClient) {
    this.api = api;
  }

  /**
   * 写入一条新记忆到指定层级
   * @param content - 记忆内容
   * @param layer - 记忆层级
   */
  async write(content: string, layer: MemoryLayer): Promise<Memory> {
    return this.writeWithOptions(content, layer);
  }

  /**
   * 使用元数据选项写入新记忆
   * @param content - 记忆内容
   * @param layer - 记忆层级
   * @param metadata - 元数据
   */
  async writeWithOptions(
    content: string,
    layer: MemoryLayer,
    metadata?: Record<string, unknown>,
  ): Promise<Memory> {
    if (!content) {
      throw new AgentOSError('记忆内容不能为空', ErrorCode.MISSING_PARAMETER);
    }

    const body: Record<string, unknown> = { content, layer };
    if (metadata) {
      body['metadata'] = metadata;
    }

    const resp = await this.api.post<APIResponse<{ memory_id: string }>>(
      '/api/v1/memories',
      body,
    );

    const data = extractDataMap(resp);
    if (!data) {
      throw new AgentOSError('记忆写入响应格式异常', ErrorCode.INVALID_RESPONSE);
    }

    return {
      id: getString(data, 'memory_id'),
      content,
      layer,
      score: 1.0,
      metadata,
      createdAt: new Date(),
      updatedAt: new Date(),
    };
  }

  /**
   * 获取指定记忆的详细信息
   * @param memoryId - 记忆 ID
   */
  async get(memoryId: string): Promise<Memory> {
    if (!memoryId) {
      throw new AgentOSError('记忆ID不能为空', ErrorCode.MISSING_PARAMETER);
    }

    const resp = await this.api.get<APIResponse>(`/api/v1/memories/${memoryId}`);
    const data = extractDataMap(resp);
    if (!data) {
      throw new AgentOSError('记忆详情响应格式异常', ErrorCode.INVALID_RESPONSE);
    }

    return this.parseMemoryFromMap(data);
  }

  /**
   * 搜索记忆，返回按相关度排序的结果
   * @param query - 搜索查询
   * @param topK - 返回数量
   */
  async search(query: string, topK: number = 10): Promise<MemorySearchResult> {
    if (!query) {
      throw new AgentOSError('搜索查询不能为空', ErrorCode.MISSING_PARAMETER);
    }
    if (topK <= 0) {
      topK = 10;
    }

    const path = `/api/v1/memories/search?query=${encodeURIComponent(query)}&top_k=${topK}`;
    const resp = await this.api.get<APIResponse>(path);

    return this.parseMemorySearchResult(resp, query, topK);
  }

  /**
   * 在指定层级内搜索记忆
   * @param query - 搜索查询
   * @param layer - 记忆层级
   * @param topK - 返回数量
   */
  async searchByLayer(
    query: string,
    layer: MemoryLayer,
    topK: number = 10,
  ): Promise<MemorySearchResult> {
    if (!query) {
      throw new AgentOSError('搜索查询不能为空', ErrorCode.MISSING_PARAMETER);
    }
    if (topK <= 0) {
      topK = 10;
    }

    const path = `/api/v1/memories/search?query=${encodeURIComponent(query)}&layer=${layer}&top_k=${topK}`;
    const resp = await this.api.get<APIResponse>(path);

    return this.parseMemorySearchResult(resp, query, topK);
  }

  /**
   * 更新指定记忆的内容
   * @param memoryId - 记忆 ID
   * @param content - 新内容
   */
  async update(memoryId: string, content: string): Promise<Memory> {
    if (!memoryId) {
      throw new AgentOSError('记忆ID不能为空', ErrorCode.MISSING_PARAMETER);
    }

    const resp = await this.api.put<APIResponse>(
      `/api/v1/memories/${memoryId}`,
      { content },
    );

    const data = extractDataMap(resp);
    if (!data) {
      throw new AgentOSError('记忆更新响应格式异常', ErrorCode.INVALID_RESPONSE);
    }

    return this.parseMemoryFromMap(data);
  }

  /**
   * 删除指定记忆
   * @param memoryId - 记忆 ID
   */
  async delete(memoryId: string): Promise<void> {
    if (!memoryId) {
      throw new AgentOSError('记忆ID不能为空', ErrorCode.MISSING_PARAMETER);
    }
    await this.api.delete(`/api/v1/memories/${memoryId}`);
  }

  /**
   * 列出记忆，支持分页和过滤
   * @param opts - 列表选项
   */
  async list(opts?: ListOptions): Promise<Memory[]> {
    let path = '/api/v1/memories';
    if (opts) {
      const params = this.buildQueryParams(opts);
      if (Object.keys(params).length > 0) {
        path += '?' + new URLSearchParams(params).toString();
      }
    }

    const resp = await this.api.get<APIResponse>(path);
    return this.parseMemoryList(resp);
  }

  /**
   * 按层级列出记忆
   * @param layer - 记忆层级
   * @param opts - 列表选项
   */
  async listByLayer(layer: MemoryLayer, opts?: ListOptions): Promise<Memory[]> {
    const params: Record<string, string> = { layer };
    if (opts) {
      Object.assign(params, this.buildQueryParams(opts));
    }

    const path = '/api/v1/memories?' + new URLSearchParams(params).toString();
    const resp = await this.api.get<APIResponse>(path);

    return this.parseMemoryList(resp);
  }

  /**
   * 获取记忆总数
   */
  async count(): Promise<number> {
    const resp = await this.api.get<APIResponse<{ count: number }>>('/api/v1/memories/count');
    const data = extractDataMap(resp);
    if (!data) {
      return 0;
    }
    return getInt64(data, 'count');
  }

  /**
   * 清空所有记忆数据
   */
  async clear(): Promise<void> {
    await this.api.delete('/api/v1/memories');
  }

  /**
   * 批量写入多条记忆
   * @param memories - 记忆项列表
   */
  async batchWrite(memories: MemoryWriteItem[]): Promise<Memory[]> {
    const results: Memory[] = [];
    for (const m of memories) {
      const mem = await this.writeWithOptions(m.content, m.layer, m.metadata);
      results.push(mem);
    }
    return results;
  }

  /**
   * 触发记忆演化过程（L1→L2→L3→L4 层级升华）
   */
  async evolve(): Promise<void> {
    await this.api.post('/api/v1/memories/evolve');
  }

  /**
   * 获取各层级的记忆统计数据
   */
  async getStats(): Promise<Record<string, number>> {
    const resp = await this.api.get<APIResponse>('/api/v1/memories/stats');
    const data = extractDataMap(resp);
    if (!data) {
      return {};
    }
    return this.extractInt64Stats(data);
  }

  /**
   * 从 map 解析 Memory 结构
   */
  private parseMemoryFromMap(data: Record<string, unknown>): Memory {
    return {
      id: getString(data, 'memory_id'),
      content: getString(data, 'content'),
      layer: getString(data, 'layer') as MemoryLayer,
      score: getFloat64(data, 'score'),
      metadata: getMap(data, 'metadata'),
      createdAt: parseTime(getString(data, 'created_at')),
      updatedAt: parseTime(getString(data, 'updated_at')),
    };
  }

  /**
   * 从 APIResponse 解析 Memory 列表
   */
  private parseMemoryList(resp: APIResponse): Memory[] {
    const data = extractDataMap(resp);
    if (!data) {
      throw new AgentOSError('记忆列表响应格式异常', ErrorCode.INVALID_RESPONSE);
    }

    const items = getInterfaceSlice(data, 'memories');
    return items
      .filter((item): item is Record<string, unknown> => typeof item === 'object' && item !== null)
      .map((item) => this.parseMemoryFromMap(item));
  }

  /**
   * 从 APIResponse 解析记忆搜索结果
   */
  private parseMemorySearchResult(
    resp: APIResponse,
    query: string,
    topK: number,
  ): MemorySearchResult {
    const data = extractDataMap(resp);
    if (!data) {
      throw new AgentOSError('记忆搜索响应格式异常', ErrorCode.INVALID_RESPONSE);
    }

    const memories = this.parseMemoryList(resp);

    return {
      memories,
      total: getInt64(data, 'total'),
      query,
      topK,
    };
  }

  /**
   * 构建查询参数
   */
  private buildQueryParams(opts: ListOptions): Record<string, string> {
    const params: Record<string, string> = {};
    if (opts.pagination) {
      if (opts.pagination.page > 0) {
        params['page'] = String(opts.pagination.page);
      }
      if (opts.pagination.pageSize > 0) {
        params['page_size'] = String(opts.pagination.pageSize);
      }
    }
    if (opts.sort?.field) {
      params['sort_by'] = opts.sort.field;
    }
    if (opts.sort?.order) {
      params['sort_order'] = opts.sort.order;
    }
    if (opts.filter?.key) {
      params['filter_key'] = opts.filter.key;
      params['filter_value'] = String(opts.filter.value);
    }
    return params;
  }

  /**
   * 提取 int64 统计数据
   */
  private extractInt64Stats(data: Record<string, unknown>): Record<string, number> {
    const result: Record<string, number> = {};
    for (const [key, value] of Object.entries(data)) {
      if (typeof value === 'number') {
        result[key] = value;
      } else if (typeof value === 'string') {
        const num = parseInt(value, 10);
        if (!isNaN(num)) {
          result[key] = num;
        }
      }
    }
    return result;
  }
}
