// AgentOS TypeScript SDK - Base Manager Module
// Version: 3.0.0
// Last updated: 2026-04-05
//
// 提供抽象 BaseManager 类，减少各 Manager 模块的重复代码。
// 使用 TypeScript 泛型和抽象类实现。

import { APIClient } from '../client';
import { APIResponse, ListOptions } from '../types';
import { AgentOSError, ErrorCode } from '../errors';
import {
  extractDataMap,
  validateAndExtractData,
  getLogger,
} from '../utils';

/**
 * 资源转换器接口
 * 用于将 API 响应数据转换为具体资源类型
 */
export interface ResourceConverter<T> {
  /**
   * 从原始数据转换资源
   * @param data - 原始数据
   * @param context - 转换上下文（用于错误信息）
   */
  convert(data: Record<string, unknown>, context?: string): T;
}

/**
 * 泛型基础管理器抽象类
 * 提供通用的 CRUD 操作和日志功能
 */
export abstract class BaseManager<T> {
  protected readonly api: APIClient;
  protected readonly resourceType: string;
  protected readonly converter: ResourceConverter<T>;

  /**
   * 创建基础管理器实例
   * @param api - API 客户端
   * @param resourceType - 资源类型标识（用于日志）
   * @param converter - 资源转换器
   */
  constructor(
    api: APIClient,
    resourceType: string,
    converter: ResourceConverter<T>,
  ) {
    this.api = api;
    this.resourceType = resourceType;
    this.converter = converter;
  }

  /**
   * 获取 API 客户端
   */
  getAPI(): APIClient {
    return this.api;
  }

  /**
   * 获取资源类型
   */
  getResourceType(): string {
    return this.resourceType;
  }

  /**
   * 执行通用 GET 请求
   * @param path - 请求路径
   * @param errorMsg - 错误信息
   */
  protected async executeGet(
    path: string,
    errorMsg: string,
  ): Promise<T> {
    const resp = await this.api.get<APIResponse<Record<string, unknown>>>(path);
    const data = validateAndExtractData(resp, errorMsg);
    return this.converter.convert(data, errorMsg);
  }

  /**
   * 执行通用 POST 请求
   * @param path - 请求路径
   * @param body - 请求体
   * @param errorMsg - 错误信息
   */
  protected async executePost(
    path: string,
    body: Record<string, unknown>,
    errorMsg: string,
  ): Promise<T> {
    const resp = await this.api.post<APIResponse<Record<string, unknown>>>(
      path,
      body,
    );
    const data = validateAndExtractData(resp, errorMsg);
    return this.converter.convert(data, errorMsg);
  }

  /**
   * 执行通用 PUT 请求
   * @param path - 请求路径
   * @param body - 请求体
   * @param errorMsg - 错误信息
   */
  protected async executePut(
    path: string,
    body: Record<string, unknown>,
    errorMsg: string,
  ): Promise<T> {
    const resp = await this.api.put<APIResponse<Record<string, unknown>>>(
      path,
      body,
    );
    const data = validateAndExtractData(resp, errorMsg);
    return this.converter.convert(data, errorMsg);
  }

  /**
   * 执行通用 DELETE 请求
   * @param path - 请求路径
   * @param errorMsg - 错误信息
   */
  protected async executeDelete(
    path: string,
    errorMsg: string,
  ): Promise<void> {
    const resp = await this.api.delete<APIResponse<Record<string, unknown>>>(
      path,
    );
    const data = extractDataMap(resp);
    if (data && data.success === false) {
      throw new AgentOSError(ErrorCode.Internal, errorMsg);
    }
  }

  /**
   * 验证并提取数据
   * @param resp - API 响应
   * @param errorMsg - 错误信息
   */
  protected validateAndExtract(
    resp: APIResponse<unknown>,
    errorMsg: string,
  ): Record<string, unknown> {
    return validateAndExtractData(resp, errorMsg);
  }

  /**
   * 构建列表查询参数
   * @param opts - 列表选项
   */
  protected buildListOptions(opts?: ListOptions): string[] {
    if (!opts) {
      return [];
    }

    const params: string[] = [];

    if (opts.pagination) {
      if (opts.pagination.page !== undefined) {
        params.push(`page=${opts.pagination.page}`);
      }
      if (opts.pagination.pageSize !== undefined) {
        params.push(`page_size=${opts.pagination.pageSize}`);
      }
    }

    if (opts.sort) {
      if (opts.sort.field) {
        params.push(`sort_by=${opts.sort.field}`);
      }
      if (opts.sort.order) {
        params.push(`sort_order=${opts.sort.order}`);
      }
    }

    if (opts.filter) {
      Object.entries(opts.filter).forEach(([key, value]) => {
        if (value !== undefined && value !== null) {
          params.push(`${key}=${encodeURIComponent(String(value))}`);
        }
      });
    }

    return params;
  }

  /**
   * 记录操作日志
   * @param operation - 操作名称
   * @param resourceId - 资源 ID
   */
  protected logOperation(operation: string, resourceId: string): void {
    getLogger().debug(`[${this.resourceType}] ${operation}: ID=${resourceId}`);
  }

  /**
   * 记录错误日志
   * @param operation - 操作名称
   * @param error - 错误对象
   */
  protected logError(operation: string, error: Error): void {
    getLogger().error(
      `[${this.resourceType}] ${operation} failed: ${error.message}`,
    );
  }

  /**
   * 记录警告日志
   * @param message - 警告信息
   */
  protected logWarning(message: string): void {
    getLogger().warn(`[${this.resourceType}] ${message}`);
  }

  /**
   * 记录信息日志
   * @param message - 信息
   */
  protected logInfo(message: string): void {
    getLogger().info(`[${this.resourceType}] ${message}`);
  }
}

/**
 * 预定义的资源转换器
 */

/** 任务转换器 */
export class TaskConverter implements ResourceConverter<any> {
  convert(data: Record<string, unknown>, context?: string): any {
    const taskId = data.task_id as string;
    if (!taskId) {
      throw new AgentOSError(
        ErrorCode.InvalidResponse,
        context || '缺少 task_id',
      );
    }

    return {
      id: taskId,
      description: (data.description as string) || '',
      status: data.status as string,
      priority: (data.priority as number) || 0,
      createdAt: data.created_at ? new Date(data.created_at as string) : new Date(),
      updatedAt: data.updated_at ? new Date(data.updated_at as string) : new Date(),
    };
  }
}

/** 记忆转换器 */
export class MemoryConverter implements ResourceConverter<any> {
  convert(data: Record<string, unknown>, context?: string): any {
    const memoryId = data.memory_id as string;
    if (!memoryId) {
      throw new AgentOSError(
        ErrorCode.InvalidResponse,
        context || '缺少 memory_id',
      );
    }

    return {
      id: memoryId,
      content: (data.content as string) || '',
      layer: (data.layer as string) || 'L1',
      score: (data.score as number) || 1.0,
      metadata: (data.metadata as Record<string, unknown>) || {},
      createdAt: data.created_at ? new Date(data.created_at as string) : new Date(),
      updatedAt: data.updated_at ? new Date(data.updated_at as string) : new Date(),
    };
  }
}

/** 会话转换器 */
export class SessionConverter implements ResourceConverter<any> {
  convert(data: Record<string, unknown>, context?: string): any {
    const sessionId = data.session_id as string;
    if (!sessionId) {
      throw new AgentOSError(
        ErrorCode.InvalidResponse,
        context || '缺少 session_id',
      );
    }

    return {
      id: sessionId,
      status: (data.status as string) || 'active',
      context: (data.context as Record<string, unknown>) || {},
      createdAt: data.created_at ? new Date(data.created_at as string) : new Date(),
      updatedAt: data.updated_at ? new Date(data.updated_at as string) : new Date(),
      expiresAt: data.expires_at ? new Date(data.expires_at as string) : undefined,
    };
  }
}

/** 技能转换器 */
export class SkillConverter implements ResourceConverter<any> {
  convert(data: Record<string, unknown>, context?: string): any {
    const skillName = data.skill_name as string;
    if (!skillName) {
      throw new AgentOSError(
        ErrorCode.InvalidResponse,
        context || '缺少 skill_name',
      );
    }

    return {
      name: skillName,
      description: (data.description as string) || '',
      status: (data.status as string) || 'available',
      loaded: (data.loaded as boolean) || false,
      createdAt: data.created_at ? new Date(data.created_at as string) : new Date(),
      updatedAt: data.updated_at ? new Date(data.updated_at as string) : new Date(),
    };
  }
}
