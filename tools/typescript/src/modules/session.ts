// AgentOS TypeScript SDK - Session Manager Module
// Version: 3.0.0
// Last updated: 2026-03-24
//
// 提供会话的创建、查询、上下文管理、清理等生命周期管理功能。
// 与 Go SDK modules/session/manager.go 保持一致。

import { APIClient } from '../client';
import {
  Session,
  SessionStatus,
  ListOptions,
  APIResponse,
} from '../types';
import { AgentOSError, ErrorCode } from '../errors';
import { extractDataMap, getString, getInt64, getMap, parseTime, getInterfaceSlice } from '../utils';

/**
 * SessionManager 管理会话完整生命周期
 */
export class SessionManager {
  private api: APIClient;

  /**
   * 创建新的会话管理器实例
   * @param api - API 客户端
   */
  constructor(api: APIClient) {
    this.api = api;
  }

  /**
   * 创建新的用户会话
   * @param userId - 用户 ID
   */
  async create(userId: string): Promise<Session> {
    return this.createWithOptions(userId);
  }

  /**
   * 使用元数据选项创建新会话
   * @param userId - 用户 ID
   * @param metadata - 元数据
   */
  async createWithOptions(
    userId: string,
    metadata?: Record<string, unknown>,
  ): Promise<Session> {
    const body: Record<string, unknown> = { user_id: userId };
    if (metadata) {
      body['metadata'] = metadata;
    }

    const resp = await this.api.post<APIResponse<{ session_id: string }>>(
      '/api/v1/sessions',
      body,
    );

    const data = extractDataMap(resp);
    if (!data) {
      throw new AgentOSError('会话创建响应格式异常', ErrorCode.INVALID_RESPONSE);
    }

    return {
      id: getString(data, 'session_id'),
      userId,
      status: SessionStatus.ACTIVE,
      context: {},
      metadata,
      createdAt: new Date(),
      lastActivity: new Date(),
    };
  }

  /**
   * 获取指定会话的详细信息
   * @param sessionId - 会话 ID
   */
  async get(sessionId: string): Promise<Session> {
    if (!sessionId) {
      throw new AgentOSError('会话ID不能为空', ErrorCode.MISSING_PARAMETER);
    }

    const resp = await this.api.get<APIResponse>(`/api/v1/sessions/${sessionId}`);
    const data = extractDataMap(resp);
    if (!data) {
      throw new AgentOSError('会话详情响应格式异常', ErrorCode.INVALID_RESPONSE);
    }

    return this.parseSessionFromMap(data);
  }

  /**
   * 设置会话上下文中的指定键值对
   * @param sessionId - 会话 ID
   * @param key - 键
   * @param value - 值
   */
  async setContext(
    sessionId: string,
    key: string,
    value: unknown,
  ): Promise<void> {
    if (!sessionId) {
      throw new AgentOSError('会话ID不能为空', ErrorCode.MISSING_PARAMETER);
    }
    if (!key) {
      throw new AgentOSError('上下文键不能为空', ErrorCode.MISSING_PARAMETER);
    }

    await this.api.post(`/api/v1/sessions/${sessionId}/context`, { key, value });
  }

  /**
   * 获取会话上下文中指定键的值
   * @param sessionId - 会话 ID
   * @param key - 键
   */
  async getContext(sessionId: string, key: string): Promise<unknown> {
    if (!sessionId) {
      throw new AgentOSError('会话ID不能为空', ErrorCode.MISSING_PARAMETER);
    }

    const resp = await this.api.get<APIResponse<{ value: unknown }>>(
      `/api/v1/sessions/${sessionId}/context/${key}`,
    );

    const data = extractDataMap(resp);
    if (!data) {
      return undefined;
    }
    return data['value'];
  }

  /**
   * 获取会话的全部上下文数据
   * @param sessionId - 会话 ID
   */
  async getAllContext(sessionId: string): Promise<Record<string, unknown>> {
    if (!sessionId) {
      throw new AgentOSError('会话ID不能为空', ErrorCode.MISSING_PARAMETER);
    }

    const resp = await this.api.get<APIResponse<{ context: Record<string, unknown> }>>(
      `/api/v1/sessions/${sessionId}/context`,
    );

    const data = extractDataMap(resp);
    if (!data) {
      return {};
    }
    return getMap(data, 'context') ?? {};
  }

  /**
   * 删除会话上下文中的指定键
   * @param sessionId - 会话 ID
   * @param key - 键
   */
  async deleteContext(sessionId: string, key: string): Promise<void> {
    if (!sessionId) {
      throw new AgentOSError('会话ID不能为空', ErrorCode.MISSING_PARAMETER);
    }
    await this.api.delete(`/api/v1/sessions/${sessionId}/context/${key}`);
  }

  /**
   * 关闭指定会话
   * @param sessionId - 会话 ID
   */
  async close(sessionId: string): Promise<void> {
    if (!sessionId) {
      throw new AgentOSError('会话ID不能为空', ErrorCode.MISSING_PARAMETER);
    }
    await this.api.delete(`/api/v1/sessions/${sessionId}`);
  }

  /**
   * 列出会话，支持分页和过滤
   * @param opts - 列表选项
   */
  async list(opts?: ListOptions): Promise<Session[]> {
    let path = '/api/v1/sessions';
    if (opts) {
      const params = this.buildQueryParams(opts);
      if (Object.keys(params).length > 0) {
        path += '?' + new URLSearchParams(params).toString();
      }
    }

    const resp = await this.api.get<APIResponse>(path);
    return this.parseSessionList(resp);
  }

  /**
   * 列出指定用户的所有会话
   * @param userId - 用户 ID
   * @param opts - 列表选项
   */
  async listByUser(userId: string, opts?: ListOptions): Promise<Session[]> {
    const params: Record<string, string> = { user_id: userId };
    if (opts) {
      Object.assign(params, this.buildQueryParams(opts));
    }

    const path = '/api/v1/sessions?' + new URLSearchParams(params).toString();
    const resp = await this.api.get<APIResponse>(path);

    return this.parseSessionList(resp);
  }

  /**
   * 列出当前所有活跃会话
   */
  async listActive(): Promise<Session[]> {
    const resp = await this.api.get<APIResponse>('/api/v1/sessions?status=active');
    return this.parseSessionList(resp);
  }

  /**
   * 更新会话的元数据
   * @param sessionId - 会话 ID
   * @param metadata - 元数据
   */
  async update(
    sessionId: string,
    metadata: Record<string, unknown>,
  ): Promise<Session> {
    if (!sessionId) {
      throw new AgentOSError('会话ID不能为空', ErrorCode.MISSING_PARAMETER);
    }

    const resp = await this.api.put<APIResponse>(
      `/api/v1/sessions/${sessionId}`,
      { metadata },
    );

    const data = extractDataMap(resp);
    if (!data) {
      throw new AgentOSError('会话更新响应格式异常', ErrorCode.INVALID_RESPONSE);
    }

    return this.parseSessionFromMap(data);
  }

  /**
   * 刷新会话的活跃时间，防止过期
   * @param sessionId - 会话 ID
   */
  async refresh(sessionId: string): Promise<void> {
    if (!sessionId) {
      throw new AgentOSError('会话ID不能为空', ErrorCode.MISSING_PARAMETER);
    }
    await this.api.post(`/api/v1/sessions/${sessionId}/refresh`);
  }

  /**
   * 检查指定会话是否已过期
   * @param sessionId - 会话 ID
   */
  async isExpired(sessionId: string): Promise<boolean> {
    const session = await this.get(sessionId);
    return session.status === SessionStatus.EXPIRED;
  }

  /**
   * 获取会话总数
   */
  async count(): Promise<number> {
    const resp = await this.api.get<APIResponse<{ count: number }>>('/api/v1/sessions/count');
    const data = extractDataMap(resp);
    if (!data) {
      return 0;
    }
    return getInt64(data, 'count');
  }

  /**
   * 获取活跃会话数
   */
  async countActive(): Promise<number> {
    const resp = await this.api.get<APIResponse<{ count: number }>>(
      '/api/v1/sessions/count?status=active',
    );
    const data = extractDataMap(resp);
    if (!data) {
      return 0;
    }
    return getInt64(data, 'count');
  }

  /**
   * 清理所有已过期的会话，返回清理数量
   */
  async cleanExpired(): Promise<number> {
    const resp = await this.api.post<APIResponse<{ cleaned: number }>>(
      '/api/v1/sessions/clean-expired',
    );
    const data = extractDataMap(resp);
    if (!data) {
      return 0;
    }
    return getInt64(data, 'cleaned');
  }

  /**
   * 从 map 解析 Session 结构
   */
  private parseSessionFromMap(data: Record<string, unknown>): Session {
    return {
      id: getString(data, 'session_id'),
      userId: getString(data, 'user_id'),
      status: getString(data, 'status') as SessionStatus,
      context: getMap(data, 'context'),
      metadata: getMap(data, 'metadata'),
      createdAt: parseTime(getString(data, 'created_at')),
      lastActivity: parseTime(getString(data, 'last_activity')),
    };
  }

  /**
   * 从 APIResponse 解析 Session 列表
   */
  private parseSessionList(resp: APIResponse): Session[] {
    const data = extractDataMap(resp);
    if (!data) {
      throw new AgentOSError('会话列表响应格式异常', ErrorCode.INVALID_RESPONSE);
    }

    const items = getInterfaceSlice(data, 'sessions');
    return items
      .filter((item): item is Record<string, unknown> => typeof item === 'object' && item !== null)
      .map((item) => this.parseSessionFromMap(item));
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
}
