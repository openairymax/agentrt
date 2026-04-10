import axios from 'axios';
import { Client } from '../src/client/client';
import { defaultConfig } from '../src/config';
import type { manager } from '../src/config';
import {
  NetworkError,
  TimeoutError,
  HttpError,
  AgentOSError,
  ErrorCode,
} from '../src/errors';

jest.mock('axios');
const mockedAxios = axios as jest.Mocked<typeof axios>;

describe('Client', () => {
  let client: Client;
  let config: manager;
  let mockAxiosInstance: any;

  beforeEach(() => {
    config = defaultConfig();
    mockAxiosInstance = {
      request: jest.fn(),
      interceptors: {
        request: { clear: jest.fn() },
        response: { clear: jest.fn() },
      },
    };
    mockedAxios.create.mockReturnValue(mockAxiosInstance);
    client = new Client(config);
  });

  afterEach(() => {
    jest.clearAllMocks();
  });

  describe('constructor', () => {
    test('should create client with default config', () => {
      expect(client.getConfig().endpoint).toBe('http://localhost:18789');
    });

    test('should set Authorization header when apiKey provided', () => {
      const configWithKey = { ...defaultConfig(), apiKey: 'test-key' };
      const c = new Client(configWithKey);
      expect(mockedAxios.create).toHaveBeenCalledWith(
        expect.objectContaining({
          headers: expect.objectContaining({
            Authorization: 'Bearer test-key',
          }),
        }),
      );
    });

    test('should merge custom headers', () => {
      const configWithHeaders = {
        ...defaultConfig(),
        headers: { 'X-Custom': 'value' },
      };
      const c = new Client(configWithHeaders);
      expect(mockedAxios.create).toHaveBeenCalledWith(
        expect.objectContaining({
          headers: expect.objectContaining({
            'X-Custom': 'value',
          }),
        }),
      );
    });
  });

  describe('getConfig', () => {
    test('should return config copy', () => {
      const cfg = client.getConfig();
      expect(cfg.endpoint).toBe('http://localhost:18789');
      cfg.endpoint = 'http://modified';
      expect(client.getConfig().endpoint).toBe('http://localhost:18789');
    });
  });

  describe('toString', () => {
    test('should return readable description', () => {
      const str = client.toString();
      expect(str).toContain('http://localhost:18789');
      expect(str).toContain('30000ms');
    });
  });

  describe('close', () => {
    test('should clear interceptors', () => {
      client.close();
      expect(mockAxiosInstance.interceptors.request.clear).toHaveBeenCalled();
      expect(mockAxiosInstance.interceptors.response.clear).toHaveBeenCalled();
    });
  });

  describe('HTTP methods', () => {
    test('should send GET request', async () => {
      mockAxiosInstance.request.mockResolvedValue({
        data: { success: true, data: { id: '1' } },
      });
      const result = await client.get('/api/v1/test');
      expect(mockAxiosInstance.request).toHaveBeenCalledWith(
        expect.objectContaining({ method: 'GET', url: '/api/v1/test' }),
      );
    });

    test('should send POST request with body', async () => {
      mockAxiosInstance.request.mockResolvedValue({
        data: { success: true, data: { id: '1' } },
      });
      const body = { name: 'test' };
      await client.post('/api/v1/test', body);
      expect(mockAxiosInstance.request).toHaveBeenCalledWith(
        expect.objectContaining({ method: 'POST', data: body }),
      );
    });

    test('should send PUT request with body', async () => {
      mockAxiosInstance.request.mockResolvedValue({
        data: { success: true, data: { id: '1' } },
      });
      const body = { name: 'updated' };
      await client.put('/api/v1/test', body);
      expect(mockAxiosInstance.request).toHaveBeenCalledWith(
        expect.objectContaining({ method: 'PUT', data: body }),
      );
    });

    test('should send DELETE request', async () => {
      mockAxiosInstance.request.mockResolvedValue({
        data: { success: true, data: {} },
      });
      await client.delete('/api/v1/test/123');
      expect(mockAxiosInstance.request).toHaveBeenCalledWith(
        expect.objectContaining({ method: 'DELETE' }),
      );
    });
  });

  describe('error handling', () => {
    test('should handle ECONNABORTED as TimeoutError', async () => {
      const axiosError = Object.create(new Error('timeout'));
      axiosError.code = 'ECONNABORTED';
      axiosError.isAxiosError = true;
      mockAxiosInstance.request.mockRejectedValue(axiosError);

      mockedAxios.isAxiosError.mockReturnValue(true);
      mockedAxios.isCancel.mockReturnValue(false);

      await expect(client.get('/test')).rejects.toThrow();
    });

    test('should handle ERR_NETWORK as NetworkError', async () => {
      const axiosError = Object.create(new Error('network error'));
      axiosError.code = 'ERR_NETWORK';
      axiosError.isAxiosError = true;
      mockAxiosInstance.request.mockRejectedValue(axiosError);

      mockedAxios.isAxiosError.mockReturnValue(true);
      mockedAxios.isCancel.mockReturnValue(false);

      await expect(client.get('/test')).rejects.toThrow();
    });

    test('should handle HTTP error response', async () => {
      const axiosError = Object.create(new Error('server error'));
      axiosError.isAxiosError = true;
      axiosError.response = { status: 500, data: 'Internal Server Error' };
      mockAxiosInstance.request.mockRejectedValue(axiosError);

      mockedAxios.isAxiosError.mockReturnValue(true);
      mockedAxios.isCancel.mockReturnValue(false);

      await expect(client.get('/test')).rejects.toThrow();
    });

    test('should handle cancel as AgentOSError', async () => {
      const cancelError = new Error('cancel');
      mockAxiosInstance.request.mockRejectedValue(cancelError);

      mockedAxios.isCancel.mockReturnValue(true);
      mockedAxios.isAxiosError.mockReturnValue(false);

      await expect(client.get('/test')).rejects.toThrow(AgentOSError);
    });
  });

  describe('retry mechanism', () => {
    test('should retry on network error', async () => {
      const networkError = Object.create(new Error('network'));
      networkError.code = 'ERR_NETWORK';
      networkError.isAxiosError = true;

      mockedAxios.isAxiosError.mockReturnValue(true);
      mockedAxios.isCancel.mockReturnValue(false);

      mockAxiosInstance.request
        .mockRejectedValueOnce(networkError)
        .mockResolvedValueOnce({ data: { success: true } });

      const result = await client.get('/test');
      expect(mockAxiosInstance.request).toHaveBeenCalledTimes(2);
    });

    test('should not retry on 4xx client error', async () => {
      const httpError = Object.create(new Error('bad request'));
      httpError.isAxiosError = true;
      httpError.response = { status: 400, data: 'Bad Request' };

      mockedAxios.isAxiosError.mockReturnValue(true);
      mockedAxios.isCancel.mockReturnValue(false);

      mockAxiosInstance.request.mockRejectedValue(httpError);

      await expect(client.get('/test')).rejects.toThrow();
      expect(mockAxiosInstance.request).toHaveBeenCalledTimes(1);
    });

    test('should retry on 5xx server error', async () => {
      const httpError = Object.create(new Error('server error'));
      httpError.isAxiosError = true;
      httpError.response = { status: 500, data: 'Internal Server Error' };

      mockedAxios.isAxiosError.mockReturnValue(true);
      mockedAxios.isCancel.mockReturnValue(false);

      mockAxiosInstance.request
        .mockRejectedValueOnce(httpError)
        .mockResolvedValueOnce({ data: { success: true } });

      const result = await client.get('/test');
      expect(mockAxiosInstance.request).toHaveBeenCalledTimes(2);
    });

    test('should retry on 429 rate limited', async () => {
      const httpError = Object.create(new Error('rate limited'));
      httpError.isAxiosError = true;
      httpError.response = { status: 429, data: 'Too Many Requests' };

      mockedAxios.isAxiosError.mockReturnValue(true);
      mockedAxios.isCancel.mockReturnValue(false);

      mockAxiosInstance.request
        .mockRejectedValueOnce(httpError)
        .mockResolvedValueOnce({ data: { success: true } });

      const result = await client.get('/test');
      expect(mockAxiosInstance.request).toHaveBeenCalledTimes(2);
    });
  });

  describe('health', () => {
    test('should return health status', async () => {
      mockAxiosInstance.request.mockResolvedValue({
        data: {
          status: 'healthy',
          version: '3.0.0',
          uptime: 3600,
          checks: { database: 'ok' },
        },
      });

      const health = await client.health();
      expect(health.status).toBe('healthy');
      expect(health.version).toBe('3.0.0');
      expect(health.uptime).toBe(3600);
      expect(health.timestamp).toBeInstanceOf(Date);
    });
  });

  describe('metrics', () => {
    test('should return system metrics', async () => {
      mockAxiosInstance.request.mockResolvedValue({
        data: {
          tasks_total: 100,
          tasks_completed: 80,
          tasks_failed: 5,
          memories_total: 500,
          sessions_active: 10,
          skills_loaded: 3,
          cpu_usage: 45.5,
          memory_usage: 60.2,
          request_count: 1000,
          average_latency_ms: 150,
        },
      });

      const metrics = await client.metrics();
      expect(metrics.tasksTotal).toBe(100);
      expect(metrics.tasksCompleted).toBe(80);
      expect(metrics.cpuUsage).toBe(45.5);
    });
  });
});
