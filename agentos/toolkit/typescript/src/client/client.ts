// AgentOS TypeScript SDK - HTTP Client
// Version: 3.0.0
// Last updated: 2026-04-05
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
   * 构建请求配置
   * @param method - HTTP 方法
   * @param path - 请求路径
   * @param body - 请求体
   * @param options - 请求选项
   */
  private buildRequestConfig(
    method: string,
    path: string,
    body?: unknown,
    options?: RequestOptions,
  ): AxiosRequestConfig {
    let fullURL = path;
    if (options?.queryParams && Object.keys(options.queryParams).length > 0) {
      const params = new URLSearchParams(options.queryParams);
      fullURL = `${path}?${params.toString()}`;
    }

    const config: AxiosRequestConfig = {
      method,
      url: fullURL,
      data: body,
      headers: options?.headers,
      signal: options?.signal,
    };

    if (options?.timeout && options.timeout > 0) {
      config.timeout = options.timeout;
    }

    return config;
  }

  /**
   * 执行单次 HTTP 请求
   * @param config - Axios 请求配置
   */
  private async executeRequest<T>(config: AxiosRequestConfig): Promise<T> {
    try {
      const response: AxiosResponse<T> = await this.httpClient.request(config);
      return response.data;
    } catch (error) {
      if (axios.isCancel(error)) {
        throw new AgentOSError('请求已被取消', ErrorCode.TIMEOUT);
      }
      throw this.handleError(error as Error);
    }
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
      // 408 Request Timeout, 429 Too Many Requests, 5xx Server Errors
      return status === 408 || status === 429 || status >= 500;
    }
    
    return false;
  }

  /**
   * 执行带重试的 HTTP 请求
   * @param config - Axios 请求配置
   * @param options - 请求选项
   */
  private async executeWithRetry<T>(
    config: AxiosRequestConfig,
    options?: RequestOptions,
  ): Promise<T> {
    let lastError: Error | null = null;

    for (let attempt = 0; attempt <= this.manager.maxRetries; attempt++) {
      if (options?.signal?.aborted) {
        throw new AgentOSError('请求已被取消', ErrorCode.TIMEOUT);
      }

      if (attempt > 0) {
        const delay = this.calculateBackoff(this.manager.retryDelay, attempt);
        getLogger().warn(`请求失败，${delay}ms 后重试 (尝试 ${attempt}/${this.manager.maxRetries})`);
        await this.sleep(delay);
      }

      try {
        return await this.executeRequest<T>(config);
      } catch (error) {
        lastError = error as Error;
        if (!this.shouldRetry(error as Error)) {
          throw lastError;
        }
      }
    }

    throw lastError || new AgentOSError('请求失败', ErrorCode.UNKNOWN);
  }

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
    const options: RequestOptions = {};
    if (opts) {
      for (const opt of opts) {
        opt(options);
      }
    }

    if (options.signal?.aborted) {
      throw new AgentOSError('请求已被取消', ErrorCode.TIMEOUT);
    }

    const config = this.buildRequestConfig(method, path, body, options);
    return this.executeWithRetry<T>(config, options);
  }

  /**
   * 处理请求错误，转换为 SDK 错误类型
   * 使用策略映射表降低圈复杂度
   * @param error - 原始错误
   */
  private handleError(error: Error): AgentOSError {
    if (error instanceof AgentOSError) {
      return error;
    }

    if (axios.isAxiosError(error)) {
      const axiosError = error as AxiosError;
      return this.handleAxiosError(axiosError);
    }

    return new AgentOSError(error.message || '未知错误', ErrorCode.UNKNOWN);
  }

  /**
   * 处理 Axios 特定错误
   * @param axiosError - Axios 错误对象
   */
  private handleAxiosError(axiosError: AxiosError): AgentOSError {
    // 网络层错误映射表
    const networkErrors: Record<string, () => AgentOSError> = {
      ECONNABORTED: () => new TimeoutError('请求超时'),
      ETIMEDOUT: () => new TimeoutError('连接超时'),
      ERR_NETWORK: () => new NetworkError('网络错误'),
      ECONNREFUSED: () => new NetworkError('连接被拒绝'),
    };

    // 检查网络层错误
    const networkHandler = networkErrors[axiosError.code || ''];
    if (networkHandler) {
      return networkHandler();
    }

    // HTTP 响应错误
    if (axiosError.response) {
      const status = axiosError.response.status;
      const message = this.extractErrorMessage(axiosError);
      return new HttpError(message, status);
    }

    return new NetworkError('网络请求失败');
  }

  /**
   * 从 Axios 响应中提取错误消息
   * @param axiosError - Axios 错误对象
   */
  private extractErrorMessage(axiosError: AxiosError): string {
    if (!axiosError.response?.data) {
      return '服务端返回错误';
    }

    const data = axiosError.response.data;
    if (typeof data === 'string') {
      return data;
    }
    
    if (typeof data === 'object' && data !== null) {
      const obj = data as Record<string, unknown>;
      return String(obj.message || obj.error || JSON.stringify(data).substring(0, 200));
    }
    
    return '服务端返回错误';
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
