import {
  defaultConfig,
  newConfig,
  newConfigFromEnv,
  validateConfig,
  cloneConfig,
  mergeConfig,
  configToString,
  withEndpoint,
  withTimeout,
  withMaxRetries,
  withRetryDelay,
  withAPIKey,
  withUserAgent,
  withDebug,
  withLogLevel,
  withMaxConnections,
  withHeaders,
  DEFAULT_POLL_INTERVAL_MS,
} from '../src/config';
import { ConfigError } from '../src/errors';

describe('Config - defaultConfig', () => {
  test('should return default configuration', () => {
    const config = defaultConfig();
    expect(config.endpoint).toBe('http://localhost:18789');
    expect(config.timeout).toBe(30000);
    expect(config.maxRetries).toBe(3);
    expect(config.retryDelay).toBe(1000);
    expect(config.userAgent).toBe('AgentOS-TypeScript-tools/3.0.0');
    expect(config.debug).toBe(false);
    expect(config.logLevel).toBe('info');
    expect(config.maxConnections).toBe(10);
    expect(config.idleConnTimeout).toBe(90000);
  });

  test('should return a new object each time', () => {
    const config1 = defaultConfig();
    const config2 = defaultConfig();
    config1.endpoint = 'http://changed';
    expect(config2.endpoint).toBe('http://localhost:18789');
  });
});

describe('Config - newConfig', () => {
  test('should create config with no options', () => {
    const config = newConfig();
    expect(config.endpoint).toBe('http://localhost:18789');
  });

  test('should apply withEndpoint option', () => {
    const config = newConfig(withEndpoint('http://custom:9999'));
    expect(config.endpoint).toBe('http://custom:9999');
  });

  test('should strip trailing slash from endpoint', () => {
    const config = newConfig(withEndpoint('http://custom:9999/'));
    expect(config.endpoint).toBe('http://custom:9999');
  });

  test('should not modify endpoint for empty string', () => {
    const config = newConfig(withEndpoint(''));
    expect(config.endpoint).toBe('http://localhost:18789');
  });

  test('should apply withTimeout option', () => {
    const config = newConfig(withTimeout(5000));
    expect(config.timeout).toBe(5000);
  });

  test('should ignore zero timeout', () => {
    const config = newConfig(withTimeout(0));
    expect(config.timeout).toBe(30000);
  });

  test('should ignore negative timeout', () => {
    const config = newConfig(withTimeout(-1));
    expect(config.timeout).toBe(30000);
  });

  test('should apply withMaxRetries option', () => {
    const config = newConfig(withMaxRetries(5));
    expect(config.maxRetries).toBe(5);
  });

  test('should allow zero maxRetries', () => {
    const config = newConfig(withMaxRetries(0));
    expect(config.maxRetries).toBe(0);
  });

  test('should ignore negative maxRetries', () => {
    const config = newConfig(withMaxRetries(-1));
    expect(config.maxRetries).toBe(3);
  });

  test('should apply withRetryDelay option', () => {
    const config = newConfig(withRetryDelay(2000));
    expect(config.retryDelay).toBe(2000);
  });

  test('should ignore zero retryDelay', () => {
    const config = newConfig(withRetryDelay(0));
    expect(config.retryDelay).toBe(1000);
  });

  test('should apply withAPIKey option', () => {
    const config = newConfig(withAPIKey('my-secret-key'));
    expect(config.apiKey).toBe('my-secret-key');
  });

  test('should apply withUserAgent option', () => {
    const config = newConfig(withUserAgent('CustomAgent/1.0'));
    expect(config.userAgent).toBe('CustomAgent/1.0');
  });

  test('should ignore empty userAgent', () => {
    const config = newConfig(withUserAgent(''));
    expect(config.userAgent).toBe('AgentOS-TypeScript-tools/3.0.0');
  });

  test('should apply withDebug option', () => {
    const config = newConfig(withDebug(true));
    expect(config.debug).toBe(true);
  });

  test('should apply withLogLevel option', () => {
    const config = newConfig(withLogLevel('debug'));
    expect(config.logLevel).toBe('debug');
  });

  test('should ignore empty logLevel', () => {
    const config = newConfig(withLogLevel(''));
    expect(config.logLevel).toBe('info');
  });

  test('should apply withMaxConnections option', () => {
    const config = newConfig(withMaxConnections(50));
    expect(config.maxConnections).toBe(50);
  });

  test('should ignore zero maxConnections', () => {
    const config = newConfig(withMaxConnections(0));
    expect(config.maxConnections).toBe(10);
  });

  test('should apply withHeaders option', () => {
    const config = newConfig(withHeaders({ 'X-Custom': 'value' }));
    expect(config.headers).toEqual({ 'X-Custom': 'value' });
  });

  test('should merge multiple headers', () => {
    const config = newConfig(
      withHeaders({ 'X-First': '1' }),
      withHeaders({ 'X-Second': '2' }),
    );
    expect(config.headers).toEqual({ 'X-First': '1', 'X-Second': '2' });
  });

  test('should apply multiple options together', () => {
    const config = newConfig(
      withEndpoint('http://remote:8080'),
      withTimeout(10000),
      withAPIKey('key-123'),
      withDebug(true),
    );
    expect(config.endpoint).toBe('http://remote:8080');
    expect(config.timeout).toBe(10000);
    expect(config.apiKey).toBe('key-123');
    expect(config.debug).toBe(true);
  });
});

describe('Config - newConfigFromEnv', () => {
  const originalEnv = process.env;

  beforeEach(() => {
    process.env = { ...originalEnv };
  });

  afterAll(() => {
    process.env = originalEnv;
  });

  test('should return default config when no env vars set', () => {
    const config = newConfigFromEnv();
    expect(config.endpoint).toBe('http://localhost:18789');
  });

  test('should read AGENTOS_ENDPOINT', () => {
    process.env.AGENTOS_ENDPOINT = 'http://env-host:9999';
    const config = newConfigFromEnv();
    expect(config.endpoint).toBe('http://env-host:9999');
  });

  test('should read AGENTOS_TIMEOUT', () => {
    process.env.AGENTOS_TIMEOUT = '5000';
    const config = newConfigFromEnv();
    expect(config.timeout).toBe(5000);
  });

  test('should ignore invalid AGENTOS_TIMEOUT', () => {
    process.env.AGENTOS_TIMEOUT = 'not-a-number';
    const config = newConfigFromEnv();
    expect(config.timeout).toBe(30000);
  });

  test('should ignore zero AGENTOS_TIMEOUT', () => {
    process.env.AGENTOS_TIMEOUT = '0';
    const config = newConfigFromEnv();
    expect(config.timeout).toBe(30000);
  });

  test('should read AGENTOS_MAX_RETRIES', () => {
    process.env.AGENTOS_MAX_RETRIES = '5';
    const config = newConfigFromEnv();
    expect(config.maxRetries).toBe(5);
  });

  test('should read AGENTOS_RETRY_DELAY', () => {
    process.env.AGENTOS_RETRY_DELAY = '2000';
    const config = newConfigFromEnv();
    expect(config.retryDelay).toBe(2000);
  });

  test('should read AGENTOS_API_KEY', () => {
    process.env.AGENTOS_API_KEY = 'env-api-key';
    const config = newConfigFromEnv();
    expect(config.apiKey).toBe('env-api-key');
  });

  test('should read AGENTOS_DEBUG true', () => {
    process.env.AGENTOS_DEBUG = 'true';
    const config = newConfigFromEnv();
    expect(config.debug).toBe(true);
  });

  test('should read AGENTOS_DEBUG as 1', () => {
    process.env.AGENTOS_DEBUG = '1';
    const config = newConfigFromEnv();
    expect(config.debug).toBe(true);
  });

  test('should not enable debug for other values', () => {
    process.env.AGENTOS_DEBUG = 'false';
    const config = newConfigFromEnv();
    expect(config.debug).toBe(false);
  });

  test('should read AGENTOS_LOG_LEVEL', () => {
    process.env.AGENTOS_LOG_LEVEL = 'debug';
    const config = newConfigFromEnv();
    expect(config.logLevel).toBe('debug');
  });

  test('should read AGENTOS_MAX_CONNECTIONS', () => {
    process.env.AGENTOS_MAX_CONNECTIONS = '20';
    const config = newConfigFromEnv();
    expect(config.maxConnections).toBe(20);
  });

  test('should read AGENTOS_USER_AGENT', () => {
    process.env.AGENTOS_USER_AGENT = 'EnvAgent/2.0';
    const config = newConfigFromEnv();
    expect(config.userAgent).toBe('EnvAgent/2.0');
  });
});

describe('Config - validateConfig', () => {
  test('should pass for valid config', () => {
    const config = defaultConfig();
    expect(() => validateConfig(config)).not.toThrow();
  });

  test('should throw for empty endpoint', () => {
    const config = defaultConfig();
    config.endpoint = '';
    expect(() => validateConfig(config)).toThrow(ConfigError);
  });

  test('should throw for invalid endpoint format', () => {
    const config = defaultConfig();
    config.endpoint = 'not-a-url';
    expect(() => validateConfig(config)).toThrow(ConfigError);
  });

  test('should throw for non-http endpoint', () => {
    const config = defaultConfig();
    config.endpoint = 'ftp://example.com';
    expect(() => validateConfig(config)).toThrow(ConfigError);
  });

  test('should accept https endpoint', () => {
    const config = defaultConfig();
    config.endpoint = 'https://secure.example.com';
    expect(() => validateConfig(config)).not.toThrow();
  });

  test('should throw for zero timeout', () => {
    const config = defaultConfig();
    config.timeout = 0;
    expect(() => validateConfig(config)).toThrow(ConfigError);
  });

  test('should throw for negative timeout', () => {
    const config = defaultConfig();
    config.timeout = -1;
    expect(() => validateConfig(config)).toThrow(ConfigError);
  });

  test('should throw for zero maxConnections', () => {
    const config = defaultConfig();
    config.maxConnections = 0;
    expect(() => validateConfig(config)).toThrow(ConfigError);
  });
});

describe('Config - cloneConfig', () => {
  test('should create independent copy', () => {
    const config = defaultConfig();
    const clone = cloneConfig(config);
    clone.endpoint = 'http://modified';
    expect(config.endpoint).toBe('http://localhost:18789');
  });

  test('should deep clone headers', () => {
    const config = defaultConfig();
    config.headers = { 'X-Key': 'value' };
    const clone = cloneConfig(config);
    clone.headers!['X-Key'] = 'modified';
    expect(config.headers!['X-Key']).toBe('value');
  });

  test('should handle undefined headers', () => {
    const config = defaultConfig();
    const clone = cloneConfig(config);
    expect(clone.headers).toBeUndefined();
  });
});

describe('Config - mergeConfig', () => {
  test('should return clone when no override', () => {
    const base = defaultConfig();
    const result = mergeConfig(base);
    expect(result.endpoint).toBe(base.endpoint);
    result.endpoint = 'http://modified';
    expect(base.endpoint).toBe('http://localhost:18789');
  });

  test('should merge endpoint', () => {
    const base = defaultConfig();
    const result = mergeConfig(base, { endpoint: 'http://override:8080' });
    expect(result.endpoint).toBe('http://override:8080');
  });

  test('should merge timeout', () => {
    const base = defaultConfig();
    const result = mergeConfig(base, { timeout: 5000 });
    expect(result.timeout).toBe(5000);
  });

  test('should ignore zero timeout override', () => {
    const base = defaultConfig();
    const result = mergeConfig(base, { timeout: 0 });
    expect(result.timeout).toBe(30000);
  });

  test('should merge maxRetries including zero', () => {
    const base = defaultConfig();
    const result = mergeConfig(base, { maxRetries: 0 });
    expect(result.maxRetries).toBe(0);
  });

  test('should merge apiKey', () => {
    const base = defaultConfig();
    const result = mergeConfig(base, { apiKey: 'new-key' });
    expect(result.apiKey).toBe('new-key');
  });

  test('should merge debug explicitly', () => {
    const base = defaultConfig();
    const result = mergeConfig(base, { debug: true });
    expect(result.debug).toBe(true);
  });

  test('should merge headers', () => {
    const base = defaultConfig();
    base.headers = { 'X-Base': 'base' };
    const result = mergeConfig(base, { headers: { 'X-New': 'new' } });
    expect(result.headers).toEqual({ 'X-Base': 'base', 'X-New': 'new' });
  });
});

describe('Config - configToString', () => {
  test('should format config as readable string', () => {
    const config = defaultConfig();
    const str = configToString(config);
    expect(str).toContain('endpoint=http://localhost:18789');
    expect(str).toContain('timeout=30000ms');
    expect(str).toContain('retries=3');
  });
});

describe('Config - DEFAULT_POLL_INTERVAL_MS', () => {
  test('should be 500ms', () => {
    expect(DEFAULT_POLL_INTERVAL_MS).toBe(500);
  });
});
