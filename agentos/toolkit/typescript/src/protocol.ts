// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: MIT
/**
 * @file protocol.ts
 * @brief AgentOS TypeScript SDK — Protocol Client Module
 *
 * Provides multi-protocol client support for communicating via:
 * - JSON-RPC 2.0 (default)
 * - MCP (Model Context Protocol) v1.0
 * - A2A (Agent-to-Agent) v0.3
 * - OpenAI API compatible
 *
 * @since 3.0.0
 */

import axios, { AxiosInstance, AxiosRequestConfig, AxiosResponse } from 'axios';
import { newError, NetworkError, TimeoutError, HttpError, ValidationError } from './errors';
import { newConfig, ConfigOption, ClientConfig, cloneConfig } from './config';

// ============================================================================
// Types
// ============================================================================

/** Supported protocol types */
export enum ProtocolType {
  JSONRPC = 'jsonrpc',
  MCP = 'mcp',
  A2A = 'a2a',
  OPENAI = 'openai',
  AUTO_DETECT = 'auto',
}

export interface ProtocolConfigOptions {
  protocolType?: ProtocolType;
  endpoint?: string;
  apiKey?: string;
  timeout?: number;
  retryCount?: number;
  retryDelay?: number;
  enableStreaming?: boolean;
  headers?: Record<string, string>;
}

/** Result of automatic protocol detection */
export interface ProtocolDetectionResult {
  detectedType: ProtocolType;
  typeName: string;
  confidence: number;
  isStreaming: boolean;
  hasBinaryPayload: boolean;
}

/** Result of a connection test to a protocol endpoint */
export interface ConnectionTestResult {
  protocol: string;
  status: string;
  statusCode?: number;
  latencyMs?: number;
  error?: string;
}

/** Protocol capability information */
export interface ProtocolCapabilities {
  name: string;
  version: string;
  supportedMethods: string[];
  features: string[];
  streamingSupported: boolean;
  authenticationRequired: boolean;
}

// ============================================================================
// Protocol Client Class
// ============================================================================

/**
 * Unified multi-protocol client for AgentOS.
 *
 * Provides a single interface for communicating via JSON-RPC, MCP, A2A,
 * or OpenAI-compatible endpoints. Automatically handles message format
 * transformation between protocols.
 *
 * @example
 * ```typescript
 * const client = new ProtocolClient({ protocolType: ProtocolType.AUTO_DETECT });
 * const result = await client.sendRequest('skill.execute', { name: 'echo' });
 * ```
 */
export class ProtocolClient {
  private config: Required<ClientConfig> & { protocolType: ProtocolType; retryCount: number; retryDelay: number; enableStreaming: boolean };
  private httpClient: AxiosInstance;
  private stats: { requestsSent: number; transformations: number };

  constructor(options: ProtocolConfigOptions = {}) {
    const baseConfig = options.endpoint ? newConfig([withEndpoint(options.endpoint)]) : newConfig([]);

    this.config = {
      ...baseConfig,
      protocolType: options.protocolType ?? ProtocolType.JSONRPC,
      retryCount: options.retryCount ?? 3,
      retryDelay: options.retryDelay ?? 1000,
      enableStreaming: options.enableStreaming ?? false,
    };

    this.httpClient = axios.create({
      baseURL: this.config.endpoint,
      timeout: this.config.timeout ?? 30000,
      headers: {
        'Content-Type': 'application/json',
        'Accept': 'application/json',
        ...(options.headers ?? {}),
      },
    });

    // Add auth interceptor
    if (options.apiKey) {
      this.httpClient.interceptors.request.use((config) => {
        config.headers.Authorization = `Bearer ${options.apiKey}`;
        return config;
      });
    }

    // Add response interceptor for error handling
    this.httpClient.interceptors.response.use(
      (response) => response,
      (error) => {
        if (error.response?.status && error.response.status >= 400) {
          throw new HttpError(
            error.response.status,
            typeof error.response.data === 'string'
              ? error.response.data
              : JSON.stringify(error.response.data)?.slice(0, 200) ?? 'HTTP Error',
          );
        }
        if (error.code === 'ECONNABORTED' || error.code === 'ETIMEDOUT') {
          throw new TimeoutError(error.message);
        }
        throw new NetworkError(error.message);
      },
    );

    this.stats = { requestsSent: 0, transformations: 0 };
  }

  /**
   * Auto-detect the appropriate protocol by querying the gateway.
   */
  async detectProtocol(): Promise<ProtocolDetectionResult> {
    try {
      const response = await this.httpClient.get('/api/v1/protocols', {
        timeout: 10000,
      });

      const contentType = response.headers['content-type'] || '';
      const body = typeof response.data === 'string' ? response.data : JSON.stringify(response.data ?? {});

      let confidence = 50.0;
      let detected: ProtocolType = ProtocolType.JSONRPC;
      const isStreaming = contentType.includes('event-stream');

      if (contentType.includes('json')) {
        confidence += 20;
        if (body.includes('"tools/call"') || body.includes('"method":"tools/list"')) {
          detected = ProtocolType.MCP;
          confidence += 30;
        } else if (body.includes('"task/delegate"') || body.includes('"agent/discover"')) {
          detected = ProtocolType.A2A;
          confidence += 30;
        } else if (body.includes('"model"') && body.includes('"choices"')) {
          detected = ProtocolType.OPENAI;
          confidence += 25;
        } else if (body.includes('"jsonrpc"')) {
          detected = ProtocolType.JSONRPC;
          confidence += 40;
        }
      }

      return {
        detectedType: detected,
        typeName: detected,
        confidence: Math.min(confidence, 100),
        isStreaming,
        hasBinaryPayload: false,
      };
    } catch (e) {
      return {
        detectedType: ProtocolType.JSONRPC,
        typeName: 'jsonrpc',
        confidence: 50,
        isStreaming: false,
        hasBinaryPayload: false,
      };
    }
  }

  /**
   * Send a unified request through the configured protocol.
   */
  async sendRequest(method: string, params: Record<string, any> = {}): Promise<any> {
    const payload = this.buildRequestPayload(method, params);
    const urlPath = this.getUrlPath();

    this.stats.requestsSent++;
    let lastError: Error | null = null;
    let delay = this.config.retryDelay;

    for (let attempt = 0; attempt <= this.config.retryCount; attempt++) {
      try {
        const response = await this.httpClient.post(urlPath, payload);

        if (this.config.protocolType !== ProtocolType.JSONRPC) {
          this.stats.transformations++;
        }

        return response.data;
      } catch (e: any) {
        lastError = e;
        if (!this.isRetryableError(e)) break;
        if (attempt < this.config.retryCount) {
          await this.sleep(delay);
          delay *= 2;
        }
      }
    }

    throw lastError ?? new Error('All retries exhausted');
  }

  /**
   * Send a streaming request and deliver chunks via callback.
   */
  async streamRequest(
    method: string,
    params: Record<string, any> = {},
    onChunk: (chunk: Buffer) => void | Promise<void>,
  ): Promise<void> {
    if (!this.config.enableStreaming) {
      const result = await this.sendRequest(method, params);
      await onChunk(Buffer.from(JSON.stringify(result)));
      return;
    }

    const payload = this.buildRequestPayload(method, params);
    const response = await this.httpClient.post('/rpc', payload, {
      responseType: 'stream',
      headers: { Accept: 'text/event-stream' },
    });

    const stream = response.data;
    stream.on('data', (chunk: Buffer) => {
      onChunk(chunk);
    });

    return new Promise((resolve, reject) => {
      stream.on('end', resolve);
      stream.on('error', reject);
    });
  }

  /**
   * Query available protocols from the gateway.
   */
  async listProtocols(): Promise<string[]> {
    const response = await this.httpClient.get('/api/v1/protocols');
    return response.data?.protocols ?? [];
  }

  /**
   * Test connectivity to a specific protocol endpoint.
   */
  async testConnection(protocolName: string): Promise<ConnectionTestResult> {
    const startTime = Date.now();

    try {
      const response = await this.httpClient.get(`/api/v1/protocols/${protocolName}/test`, {
        timeout: 10000,
      });

      return {
        protocol: protocolName,
        status: response.status < 400 ? 'ok' : 'error',
        statusCode: response.status,
        latencyMs: Math.round(Date.now() - startTime),
      };
    } catch (e: any) {
      return {
        protocol: protocolName,
        status: 'error',
        latencyMs: Math.round(Date.now() - startTime),
        error: e.message,
      };
    }
  }

  /**
   * Get capabilities of a specific protocol adapter.
   */
  async getCapabilities(protocolName: string): Promise<ProtocolCapabilities> {
    const response = await this.httpClient.get(`/api/v1/protocols/${protocolName}/capabilities`);
    return response.data;
  }

  /**
   * Get internal statistics about this client instance.
   */
  getStats(): { requestsSent: number; transformations: number } {
    return { ...this.stats };
  }

  // ==========================================================================
  // Private helpers
  // ==========================================================================

  private getUrlPath(): string {
    switch (this.config.protocolType) {
      case ProtocolType.OPENAI:
        return '/v1/chat/completions';
      case ProtocolType.MCP:
      case ProtocolType.A2A:
        return '/api/v1/invoke';
      default:
        return '/rpc';
    }
  }

  private buildRequestPayload(method: string, params: Record<string, any>): any {
    switch (this.config.protocolType) {
      case ProtocolType.OPENAI: {
        const { messages = [], model = 'gpt-4o', temperature = 0.7, maxTokens = 2048, ...rest } = params;
        return { model, messages, temperature, max_tokens: maxTokens, stream: this.config.enableStreaming };
      }
      case ProtocolType.MCP: {
        const mcpMethod = method.includes('.') ? method.split('.').pop()! : method;
        return { protocol: 'mcp', version: '1.0', method: mcpMethod, params: rest };
      }
      case ProtocolType.A2A: {
        const mapping: Record<string, string> = { 'agent.list': 'agent/discover', 'task.create': 'task/delegate' };
        const a2aMethod = mapping[method] ?? method;
        return { protocol: 'a2a', version: '0.3', method: a2aMethod, params };
      }
      default:
        return { jsonrpc: '2.0', id: `req_${Date.now()}`, method, params };
    }
  }

  private isRetryableError(e: any): boolean {
    if (e instanceof TimeoutError || e instanceof NetworkError) return true;
    if (e instanceof HttpError && e.statusCode >= 500) return true;
    if (e instanceof HttpError && e.statusCode === 429) return true;
    return false;
  }

  private sleep(ms: number): Promise<void> {
    return new Promise(resolve => setTimeout(resolve, ms));
  }
}

// ============================================================================
// Convenience factory functions
// ============================================================================

/**
 * Create a ProtocolClient with auto-detection enabled.
 */
export function createProtocolClient(options?: ProtocolConfigOptions): ProtocolClient {
  return new ProtocolClient({ ...options, protocolType: ProtocolType.AUTO_DETECT });
}

/**
 * Create a ProtocolClient optimized for MCP protocol.
 */
export function createMCPClient(endpoint: string): ProtocolClient {
  return new ProtocolClient({ endpoint, protocolType: ProtocolType.MCP });
}

/**
 * Create a ProtocolClient optimized for OpenAI API.
 */
export function createOpenAIClient(endpoint: string, apiKey: string): ProtocolClient {
  return new ProtocolClient({ endpoint, apiKey, protocolType: ProtocolType.OPENAI });
}
