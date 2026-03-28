// AgentOS TypeScript SDK - HTTP Client
// Version: 3.0.0
// Last updated: 2026-03-24
//
// 提供 HTTP 通信层、APIClient 接口定义和重试机制。
// 与 Go SDK client/client.go 保持一致。

import axios, { AxiosInstance, AxiosRequestConfig, AxiosResponse, AxiosError } from 'axios';
import { manager } from '../manager';
import {
  APIResponse,
  HealthStatus,
  Metrics,
  RequestOptions,
  RequestOption,
} from '../types';
import {
  AgentOSError,
  ErrorCode,
  NetworkError,
  TimeoutError,
  HttpError,
  httpStatusToErrorCode,
} from '../errors';
import { getLogger } from '../utils/logger';

/** 响应体最大允许大小（10MB） */
const MAX_RESPONSE_BODY_SIZE = 10 * 1024 * 1024;

/**
 * APIClient 定义所有 Manager 共同依赖的 HTTP 通信接口
 * 与 Go SDK APIClient 接口保持一致
 */
export interface APIClient {
  /**
   * 执行 HTTP GET 请求
   * @param path - 请求路径
   * @param opts - 请求选项
   */
  get<T = APIResponse>(path: string, opts?: RequestOption[]): Promise<T>;

  /**
   * 执行 HTTP POST 请求
   * @param path - 请求路径
   * @param body - 请求体
   * @param opts - 请求选项
   */
  post<T = APIResponse>(path: string, body?: unknown, opts?: RequestOption[]): Promise<T>;

  /**
   * 执行 HTTP PUT 请求
   * @param path - 请求路径
   * @param body - 请求体
   * @param opts - 请求选项
   */
  put<T = APIResponse>(path: string, body?: unknown, opts?: RequestOption[]): Promise<T>;

  /**
   * 执行 HTTP DELETE 请求
   * @param path - 请求路径
   * @param opts - 请求选项
   */
  delete<T = APIResponse>(path: string, opts?: RequestOption[]): Promise<T>;
}

/**
 * Client 是 AgentOS TypeScript SDK 的核心 HTTP 客户端
 * 与 Go SDK Client 结构体保持一致
 */
export class Client implements APIClient {
  private manager: manager;
  private httpClient: AxiosInstance;

  /**
   * 使用配置对象创建新的 HTTP 客户端
   * @param manager - 客户端配置
   */
  constructor(manager: manager) {
    this.manager = manager;

    const headers: Record<string, string> = {
      'Content-Type': 'application/json',
      'User-Agent': manager.userAgent,
    };

    // 添加 API Key 认证
    if (manager.apiKey) {
      headers['Authorization'] = `Bearer ${manager.apiKey}`;
    }

    // 合并自定义 headers
    if (manager.headers) {
      Object.assign(headers, manager.headers);
    }

    this.httpClient = axios.create({
      baseURL: manager.endpoint,
      timeout: manager.timeout,
      headers,
      maxContentLength: MAX_RESPONSE_BODY_SIZE,
      maxBodyLength: MAX_RESPONSE_BODY_SIZE,
    });
  }

  /**
   * 获取当前客户端的配置副本
   */
  getConfig(): manager {
    return { ...this.manager };
  }

  /**
   * 检查 AgentOS 服务的健康状态
   */
  async health(): Promise<HealthStatus> {
    const resp = await this.get<{ status: string; version: string; uptime: number; checks: Record<string, string> }>('/health');

    return {
      status: resp.status,
      version: resp.version,
      uptime: resp.uptime,
      checks: resp.checks || {},
      timestamp: new Date(),
    };
  }

  /**
   * 获取 AgentOS 系统运行指标
   */
  async metrics(): Promise<Metrics> {
    const resp = await this.get<{
      tasks_total: number;
      tasks_completed: number;
      tasks_failed: number;
      memories_total: number;
      sessions_active: number;
      skills_loaded: number;
      cpu_usage: number;
      memory_usage: number;
      request_count: number;
      average_latency_ms: number;
    }>('/metrics');

    return {
      tasksTotal: resp.tasks_total,
      tasksCompleted: resp.tasks_completed,
      tasksFailed: resp.tasks_failed,
      memoriesTotal: resp.memories_total,
      sessionsActive: resp.sessions_active,
      skillsLoaded: resp.skills_loaded,
      cpuUsage: resp.cpu_usage,
      memoryUsage: resp.memory_usage,
      requestCount: resp.request_count,
      averageLatencyMs: resp.average_latency_ms,
    };
  }

  /**
   * 关闭客户端，释放 HTTP 连接池资源
   */
  close(): void {
    // Axios 没有显式的关闭方法，但可以清理拦截器
    this.httpClient.interceptors.request.clear();
    this.httpClient.interceptors.response.clear();
  }

  /**
   * 返回客户端的可读描述
   */
  toString(): string {
    return `AgentOS Client[endpoint=${this.manager.endpoint}, timeout=${this.manager.timeout}ms]`;
  }

  // ============================================================
  // HTTP 通信实现 (APIClient 接口)
  // ============================================================

  /**
   * 执行底层 HTTP 请求，包含序列化、重试和响应解析逻辑
   * @param method - HTTP 方法
   * @param path - 请求路径
   * @param body - 请求体
   * @param opts - 请求选项
   */
  private async request<T>(
    method: string,
    path: string,
    body?: unknown,
    opts?: RequestOption[],
  ): Promise<T> {
    // 应用请求选项
    const options: RequestOptions = {};
    if (opts) {
      for (const opt of opts) {
        opt(options);
      }
    }

    // 检查是否已取消
    if (options.signal?.aborted) {
      throw new AgentOSError('请求已被取消', ErrorCode.TIMEOUT);
    }

    // 构建完整 URL
    let fullURL = path;
    if (options.queryParams && Object.keys(options.queryParams).length > 0) {
      const params = new URLSearchParams(options.queryParams);
      fullURL = `${path}?${params.toString()}`;
    }

    // 准备请求配置
    const manager: AxiosRequestConfig = {
      method,
      url: fullURL,
      data: body,
      headers: options.headers,
      signal: options.signal, // 添加取消信号支持
    };

    // 覆盖单次请求超时
    if (options.timeout && options.timeout > 0) {
      manager.timeout = options.timeout;
    }

    let lastError: Error | null = null;

    // 重试循环
    for (let attempt = 0; attempt <= this.manager.maxRetries; attempt++) {
      // 每次重试前检查是否已取消
      if (options.signal?.aborted) {
        throw new AgentOSError('请求已被取消', ErrorCode.TIMEOUT);
      }

      if (attempt > 0) {
        const delay = this.calculateBackoff(this.manager.retryDelay, attempt);
        getLogger().warn(`请求失败，${delay}ms 后重试 (尝试 ${attempt}/${this.manager.maxRetries})`);
        await this.sleep(delay);
      }

      try {
        const response: AxiosResponse<T> = await this.httpClient.request(manager);
        return response.data;
      } catch (error) {
        // 检查是否是取消错误
        if (axios.isCancel(error)) {
          throw new AgentOSError('请求已被取消', ErrorCode.TIMEOUT);
        }

        lastError = this.handleError(error as Error);

        // 判断是否应该重试
        if (!this.shouldRetry(error as Error)) {
          throw lastError;
        }
      }
    }

    throw lastError || new AgentOSError('请求失败', ErrorCode.UNKNOWN);
  }

  /**
   * 处理请求错误，转换为 SDK 错误类型
   * @param error - 原始错误
   */
  private handleError(error: Error): AgentOSError {
    if (axios.isAxiosError(error)) {
      const axiosError = error as AxiosError;

      if (axiosError.code === 'ECONNABORTED' || axiosError.code === 'ETIMEDOUT') {
        return new TimeoutError('请求超时');
      }

      if (axiosError.code === 'ERR_NETWORK' || axiosError.code === 'ECONNREFUSED') {
        return new NetworkError('网络连接失败');
      }

      if (axiosError.response) {
        const status = axiosError.response.status;
        const message = typeof axiosError.response.data === 'string'
          ? axiosError.response.data
          : JSON.stringify(axiosError.response.data);
        return new HttpError(`服务端返回错误: ${status}`, status);
      }
    }

    if (error instanceof AgentOSError) {
      return error;
    }

    return new AgentOSError(error.message || '未知错误', ErrorCode.UNKNOWN);
  }

  /**
   * 判断是否应该重试请求
   * @param error - 错误对象
   */
  private shouldRetry(error: Error): boolean {
    if (error instanceof NetworkError || error instanceof TimeoutError) {
      return true;
    }

    if (error instanceof HttpError) {
      const status = error.statusCode;
      return status >= 500 || status === 429;
    }

    return false;
  }

  /**
   * 生成唯一的请求 ID
   */
  private generateRequestID(): string {
    const timestamp = Date.now() * 1000;
    const random = Math.floor(Math.random() * 1000000);
    return `req-${timestamp}-${random.toString().padStart(6, '0')}`;
  }

  /**
   * 计算指数退避延迟（含抖动）
   * @param baseDelay - 基础延迟（毫秒）
   * @param attempt - 当前尝试次数
   */
  private calculateBackoff(baseDelay: number, attempt: number): number {
    const backoff = baseDelay * Math.pow(2, attempt - 1);
    const jitter = Math.random() * backoff;
    return Math.floor(backoff + jitter);
  }

  /**
   * 延迟函数
   * @param ms - 延迟毫秒数
   */
  private sleep(ms: number): Promise<void> {
    return new Promise((resolve) => setTimeout(resolve, ms));
  }

  /**
   * 执行 HTTP GET 请求
   */
  async get<T = APIResponse>(path: string, opts?: RequestOption[]): Promise<T> {
    return this.request<T>('GET', path, undefined, opts);
  }

  /**
   * 执行 HTTP POST 请求
   */
  async post<T = APIResponse>(path: string, body?: unknown, opts?: RequestOption[]): Promise<T> {
    return this.request<T>('POST', path, body, opts);
  }

  /**
   * 执行 HTTP PUT 请求
   */
  async put<T = APIResponse>(path: string, body?: unknown, opts?: RequestOption[]): Promise<T> {
    return this.request<T>('PUT', path, body, opts);
  }

  /**
   * 执行 HTTP DELETE 请求
   */
  async delete<T = APIResponse>(path: string, opts?: RequestOption[]): Promise<T> {
    return this.request<T>('DELETE', path, undefined, opts);
  }
}
