// AgentOS TypeScript SDK - Mock Client
// Version: 3.0.0
// Last updated: 2026-03-24
//
// 提供 Mock 客户端用于测试和开发环境。
// 与 Go SDK client/mock.go 保持一致。

import { APIClient } from './client';
import { APIResponse, HealthStatus, Metrics, RequestOption } from '../types';
import { AgentOSError, ErrorCode } from '../errors';

/**
 * MockResponse 定义 Mock 响应配置
 */
interface MockResponse<T = unknown> {
  /** 响应数据 */
  data: T;
  /** 是否抛出错误 */
  error?: AgentOSError;
  /** 延迟毫秒数 */
  delay?: number;
}

/**
 * MockClient 用于测试和开发的 Mock 客户端
 * 实现 APIClient 接口，支持预设响应和错误模拟
 */
export class MockClient implements APIClient {
  private responses: Map<string, MockResponse> = new Map();
  private requestLog: Array<{ method: string; path: string; body?: unknown; timestamp: Date }> = [];

  /**
   * 设置 Mock 响应
   * @param method - HTTP 方法
   * @param path - 请求路径
   * @param response - Mock 响应
   */
  setResponse<T>(method: string, path: string, response: MockResponse<T>): void {
    const key = `${method.toUpperCase()}:${path}`;
    this.responses.set(key, response as MockResponse);
  }

  /**
   * 设置默认成功响应
   * @param path - 请求路径
   * @param data - 响应数据
   */
  setSuccessResponse<T>(path: string, data: T): void {
    this.setResponse('GET', path, { data });
    this.setResponse('POST', path, { data });
    this.setResponse('PUT', path, { data });
    this.setResponse('DELETE', path, { data });
  }

  /**
   * 设置错误响应
   * @param method - HTTP 方法
   * @param path - 请求路径
   * @param error - 错误对象
   */
  setError(method: string, path: string, error: AgentOSError): void {
    this.setResponse(method, path, { data: null, error });
  }

  /**
   * 获取请求日志
   */
  getRequestLog(): Array<{ method: string; path: string; body?: unknown; timestamp: Date }> {
    return [...this.requestLog];
  }

  /**
   * 清空请求日志和预设响应
   */
  reset(): void {
    this.responses.clear();
    this.requestLog = [];
  }

  /**
   * 检查健康状态（Mock 实现）
   */
  async health(): Promise<HealthStatus> {
    return {
      status: 'healthy',
      version: '3.0.0-mock',
      uptime: 0,
      checks: { database: 'ok', memory: 'ok' },
      timestamp: new Date(),
    };
  }

  /**
   * 获取系统指标（Mock 实现）
   */
  async metrics(): Promise<Metrics> {
    return {
      tasksTotal: 0,
      tasksCompleted: 0,
      tasksFailed: 0,
      memoriesTotal: 0,
      sessionsActive: 0,
      skillsLoaded: 0,
      cpuUsage: 0,
      memoryUsage: 0,
      requestCount: 0,
      averageLatencyMs: 0,
    };
  }

  /**
   * 执行 Mock 请求
   * @param method - HTTP 方法
   * @param path - 请求路径（可能包含查询参数）
   * @param body - 请求体
   */
  private async mockRequest<T>(method: string, path: string, body?: unknown): Promise<T> {
    this.requestLog.push({
      method: method.toUpperCase(),
      path,
      body,
      timestamp: new Date(),
    });

    const upperMethod = method.toUpperCase();

    // 优先精确匹配
    const exactKey = `${upperMethod}:${path}`;
    if (this.responses.has(exactKey)) {
      const mockResponse = this.responses.get(exactKey)!;
      return this.buildMockResponse<T>(mockResponse);
    }

    // 支持路径前缀匹配（忽略查询参数）
    // 提取不含查询参数的路径
    const urlPath = path.split('?')[0];
    for (const [key, mockResponse] of this.responses.entries()) {
      const [keyMethod, keyPath] = key.split(':');
      const keyPathWithoutQuery = keyPath.split('?')[0];
      if (keyMethod === upperMethod && urlPath.startsWith(keyPathWithoutQuery) && keyPathWithoutQuery !== '') {
        return this.buildMockResponse<T>(mockResponse);
      }
    }

    // 如果没有预设响应，返回默认响应
    return this.defaultResponse<T>();
  }

  /**
   * 构建 Mock 响应
   */
  private buildMockResponse<T>(mockResponse: MockResponse): T {
    if (mockResponse.delay && mockResponse.delay > 0) {
      setTimeout(() => Promise.resolve(), mockResponse.delay);
    }
    if (mockResponse.error) {
      throw mockResponse.error;
    }
    return {
      success: true,
      data: mockResponse.data,
      message: 'OK',
    } as unknown as T;
  }

  /**
   * 返回默认响应
   */
  private defaultResponse<T>(): T {
    return {
      success: true,
      data: null,
      message: 'Mock response',
    } as unknown as T;
  }

  /**
   * 延迟函数
   */
  private sleep(ms: number): Promise<void> {
    return new Promise((resolve) => setTimeout(resolve, ms));
  }

  /**
   * 执行 HTTP GET 请求
   */
  async get<T = APIResponse>(path: string, _opts?: RequestOption[]): Promise<T> {
    return this.mockRequest<T>('GET', path);
  }

  /**
   * 执行 HTTP POST 请求
   */
  async post<T = APIResponse>(path: string, body?: unknown, _opts?: RequestOption[]): Promise<T> {
    return this.mockRequest<T>('POST', path, body);
  }

  /**
   * 执行 HTTP PUT 请求
   */
  async put<T = APIResponse>(path: string, body?: unknown, _opts?: RequestOption[]): Promise<T> {
    return this.mockRequest<T>('PUT', path, body);
  }

  /**
   * 执行 HTTP DELETE 请求
   */
  async delete<T = APIResponse>(path: string, _opts?: RequestOption[]): Promise<T> {
    return this.mockRequest<T>('DELETE', path);
  }
}
