// AgentOS TypeScript SDK - Skill Manager Module
// Version: 3.0.0
// Last updated: 2026-03-24
//
// 提供技能的注册、加载、执行、卸载及验证功能。
// 与 Go SDK modules/skill/manager.go 保持一致。

import { APIClient } from '../client';
import {
  Skill,
  SkillResult,
  SkillInfo,
  SkillStatus,
  ListOptions,
  APIResponse,
} from '../types';
import { AgentOSError, ErrorCode } from '../errors';
import {
  extractDataMap,
  getString,
  getInt64,
  getMap,
  getBool,
  parseTime,
  getInterfaceSlice,
  buildListPath,
  parseList,
  validateAndExtractData,
  validateRequiredString,
} from '../utils';

/**
 * SkillExecuteRequest 批量执行时的单条请求
 */
export interface SkillExecuteRequest {
  /** 技能 ID */
  skillId: string;
  /** 参数 */
  parameters?: Record<string, unknown>;
}

/**
 * SkillManager 管理技能完整生命周期
 */
export class SkillManager {
  private api: APIClient;

  /**
   * 创建新的技能管理器实例
   * @param api - API 客户端
   */
  constructor(api: APIClient) {
    this.api = api;
  }

  /**
   * 加载指定技能到运行时
   * @param skillId - 技能 ID
   */
  async load(skillId: string): Promise<Skill> {
    validateRequiredString(skillId, '技能ID');

    const resp = await this.api.post<APIResponse>(
      `/api/v1/skills/${skillId}/load`,
    );

    const data = validateAndExtractData(resp, '技能加载响应格式异常');

    return {
      id: skillId,
      name: getString(data, 'name'),
      version: getString(data, 'version') || '1.0.0',
      description: getString(data, 'description') || '',
      status: SkillStatus.ACTIVE,
      parameters: getMap(data, 'parameters'),
      metadata: getMap(data, 'metadata'),
      createdAt: new Date(),
    };
  }

  /**
   * 获取指定技能的详细信息
   * @param skillId - 技能 ID
   */
  async get(skillId: string): Promise<Skill> {
    validateRequiredString(skillId, '技能ID');

    const resp = await this.api.get<APIResponse>(`/api/v1/skills/${skillId}`);
    const data = validateAndExtractData(resp, '技能详情响应格式异常');

    return this.parseSkillFromMap(data, skillId);
  }

  /**
   * 执行指定技能
   * @param skillId - 技能 ID
   * @param parameters - 参数
   */
  async execute(
    skillId: string,
    parameters?: Record<string, unknown>,
  ): Promise<SkillResult> {
    validateRequiredString(skillId, '技能ID');

    const resp = await this.api.post<APIResponse>(
      `/api/v1/skills/${skillId}/execute`,
      { parameters },
    );

    return this.parseSkillResult(resp);
  }

  /**
   * 在指定会话上下文中执行技能
   * @param skillId - 技能 ID
   * @param parameters - 参数
   * @param sessionId - 会话 ID
   */
  async executeWithContext(
    skillId: string,
    parameters: Record<string, unknown> | undefined,
    sessionId: string,
  ): Promise<SkillResult> {
    validateRequiredString(skillId, '技能ID');

    const resp = await this.api.post<APIResponse>(
      `/api/v1/skills/${skillId}/execute`,
      { parameters, session_id: sessionId },
    );

    return this.parseSkillResult(resp);
  }

  /**
   * 卸载指定技能，释放运行时资源
   * @param skillId - 技能 ID
   */
  async unload(skillId: string): Promise<void> {
    validateRequiredString(skillId, '技能ID');
    await this.api.post(`/api/v1/skills/${skillId}/unload`);
  }

  /**
   * 列出技能，支持分页和过滤
   * @param opts - 列表选项
   */
  async list(opts?: ListOptions): Promise<Skill[]> {
    const path = buildListPath('/api/v1/skills', opts);
    const resp = await this.api.get<APIResponse>(path);
    return parseList(resp, 'skills', (item) => this.parseSkillFromMap(item, ''), '技能列表响应格式异常');
  }

  /**
   * 列出当前已加载的技能
   */
  async listLoaded(): Promise<Skill[]> {
    const resp = await this.api.get<APIResponse>('/api/v1/skills?status=loaded');
    return parseList(resp, 'skills', (item) => this.parseSkillFromMap(item, ''), '技能列表响应格式异常');
  }

  /**
   * 注册新的技能
   * @param name - 技能名称
   * @param description - 描述
   * @param parameters - 参数定义
   */
  async register(
    name: string,
    description: string,
    parameters?: Record<string, unknown>,
  ): Promise<Skill> {
    validateRequiredString(name, '技能名称');

    const resp = await this.api.post<APIResponse<{ skill_id: string }>>(
      '/api/v1/skills',
      { name, description, parameters },
    );

    const data = validateAndExtractData(resp, '技能注册响应格式异常');

    return {
      id: getString(data, 'skill_id'),
      name,
      version: getString(data, 'version') || '1.0.0',
      description,
      parameters,
      status: SkillStatus.ACTIVE,
      metadata: getMap(data, 'metadata'),
      createdAt: new Date(),
    };
  }

  /**
   * 更新指定技能的描述和参数
   * @param skillId - 技能 ID
   * @param description - 描述
   * @param parameters - 参数定义
   */
  async update(
    skillId: string,
    description: string,
    parameters?: Record<string, unknown>,
  ): Promise<Skill> {
    validateRequiredString(skillId, '技能ID');

    const resp = await this.api.put<APIResponse>(
      `/api/v1/skills/${skillId}`,
      { description, parameters },
    );

    const data = validateAndExtractData(resp, '技能更新响应格式异常');

    return this.parseSkillFromMap(data, skillId);
  }

  /**
   * 删除指定技能
   * @param skillId - 技能 ID
   */
  async delete(skillId: string): Promise<void> {
    validateRequiredString(skillId, '技能ID');
    await this.api.delete(`/api/v1/skills/${skillId}`);
  }

  /**
   * 获取指定技能的只读元信息
   * @param skillId - 技能 ID
   */
  async getInfo(skillId: string): Promise<SkillInfo> {
    validateRequiredString(skillId, '技能ID');

    const resp = await this.api.get<APIResponse>(
      `/api/v1/skills/${skillId}/info`,
    );

    const data = validateAndExtractData(resp, '技能信息响应格式异常');

    return {
      name: getString(data, 'skill_name'),
      description: getString(data, 'description'),
      version: getString(data, 'version'),
      parameters: getMap(data, 'parameters'),
    };
  }

  /**
   * 验证技能参数是否合法
   * @param skillId - 技能 ID
   * @param parameters - 参数
   */
  async validate(
    skillId: string,
    parameters: Record<string, unknown>,
  ): Promise<{ valid: boolean; errors: string[] }> {
    validateRequiredString(skillId, '技能ID');

    const resp = await this.api.post<APIResponse<{ valid: boolean; errors?: string[] }>>(
      `/api/v1/skills/${skillId}/validate`,
      { parameters },
    );

    const data = validateAndExtractData(resp, '技能验证响应格式异常');

    const valid = getBool(data, 'valid');
    const errors: string[] = [];
    const errs = getInterfaceSlice(data, 'errors');
    for (const e of errs) {
      if (typeof e === 'string') {
        errors.push(e);
      }
    }

    return { valid, errors };
  }

  /**
   * 获取技能总数
   */
  async count(): Promise<number> {
    const resp = await this.api.get<APIResponse<{ count: number }>>('/api/v1/skills/count');
    const data = extractDataMap(resp);
    if (!data) {
      return 0;
    }
    return getInt64(data, 'count');
  }

  /**
   * 获取已加载技能数
   */
  async countLoaded(): Promise<number> {
    const resp = await this.api.get<APIResponse<{ count: number }>>(
      '/api/v1/skills/count?status=loaded',
    );
    const data = extractDataMap(resp);
    if (!data) {
      return 0;
    }
    return getInt64(data, 'count');
  }

  /**
   * 搜索技能
   * @param query - 搜索查询
   * @param topK - 返回数量
   */
  async search(query: string, topK: number = 10): Promise<Skill[]> {
    validateRequiredString(query, '搜索查询');
    if (topK <= 0) {
      topK = 10;
    }

    const path = `/api/v1/skills/search?query=${encodeURIComponent(query)}&top_k=${topK}`;
    const resp = await this.api.get<APIResponse>(path);

    return parseList(resp, 'skills', (item) => this.parseSkillFromMap(item, ''), '技能搜索响应格式异常');
  }

  /**
   * 批量执行多个技能
   * @param requests - 执行请求列表
   */
  async batchExecute(requests: SkillExecuteRequest[]): Promise<SkillResult[]> {
    const results: SkillResult[] = [];
    for (const req of requests) {
      const result = await this.execute(req.skillId, req.parameters);
      results.push(result);
    }
    return results;
  }

  /**
   * 获取指定技能的执行统计数据
   * @param skillId - 技能 ID
   */
  async getStats(skillId: string): Promise<Record<string, number>> {
    validateRequiredString(skillId, '技能ID');

    const resp = await this.api.get<APIResponse>(
      `/api/v1/skills/${skillId}/stats`,
    );

    const data = extractDataMap(resp);
    if (!data) {
      return {};
    }
    return this.extractInt64Stats(data);
  }

  /**
   * 从 map 解析 Skill 结构
   */
  private parseSkillFromMap(data: Record<string, unknown>, fallbackId: string): Skill {
    const id = getString(data, 'skill_id') || fallbackId;
    return {
      id,
      name: getString(data, 'name'),
      version: getString(data, 'version'),
      description: getString(data, 'description'),
      status: getString(data, 'status') as SkillStatus,
      parameters: getMap(data, 'parameters'),
      metadata: getMap(data, 'metadata'),
      createdAt: parseTime(getString(data, 'created_at')),
    };
  }

  /**
   * 从 APIResponse 解析 SkillResult
   */
  private parseSkillResult(resp: APIResponse): SkillResult {
    const data = extractDataMap(resp);
    if (!data) {
      throw new AgentOSError('技能执行响应格式异常', ErrorCode.INVALID_RESPONSE);
    }

    return {
      success: getBool(data, 'success'),
      output: data['output'],
      error: getString(data, 'error'),
    };
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
