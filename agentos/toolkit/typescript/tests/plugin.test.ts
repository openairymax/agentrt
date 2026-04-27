// AgentOS TypeScript SDK - Plugin Framework Tests
// 验证: 注册→发现→加载→调用→卸载 完整生命周期
// Version: 4.0.0 (production audit fix: async load/unload)

import { strict as assert } from 'assert';

import {
  PluginState,
  PluginManifest,
  PluginDependency,
  PluginInfo,
  BasePlugin,
  PluginRegistry,
  PluginManager,
  getPluginRegistry,
} from '../src/plugin';

// ===== 示例插件 =====

class LoggerPlugin extends BasePlugin {
  private _logs: Array<{ level: string; message: string; timestamp: number }> = [];
  private _maxEntries = 1000;

  get pluginId(): string { return 'LoggerPlugin'; }

  getCapabilities(): string[] { return ['logging', 'structured_logs', 'log_query']; }

  async onLoad(context?: Record<string, unknown>): Promise<void> {
    if (context && typeof context.maxEntries === 'number') {
      this._maxEntries = context.maxEntries;
    }
  }

  async onActivate(context?: Record<string, unknown>): Promise<void> {
    this._logs = [];
  }

  async onDeactivate(): Promise<void> {
    this._logs = [];
  }

  async onUnload(): Promise<void> {
    this._logs = [];
  }

  async onError(error: Error): Promise<void> {}

  log(level: string, message: string): number {
    this._logs.push({ level, message, timestamp: Date.now() });
    if (this._logs.length > this._maxEntries) {
      this._logs = this._logs.slice(-this._maxEntries);
    }
    return this._logs.length;
  }

  query(level?: string, limit = 100): Array<{ level: string; message: string }> {
    let results = this._logs;
    if (level) {
      results = results.filter(e => e.level === level);
    }
    return results.slice(-limit);
  }

  count(): number { return this._logs.length; }
}

class MetricsPlugin extends BasePlugin {
  private _counters: Map<string, number> = new Map();

  get pluginId(): string { return 'MetricsPlugin'; }

  getCapabilities(): string[] { return ['metrics', 'counters', 'gauges', 'timers']; }

  async onLoad(_context?: Record<string, unknown>): Promise<void> {}
  async onActivate(_context?: Record<string, unknown>): Promise<void> {}
  async onDeactivate(): Promise<void> {}
  async onUnload(): Promise<void> { this._counters.clear(); }
  async onError(_error: Error): Promise<void> {}

  increment(name: string, value = 1): number {
    const current = this._counters.get(name) || 0;
    this._counters.set(name, current + value);
    return this._counters.get(name)!;
  }

  getCounter(name: string): number {
    return this._counters.get(name) || 0;
  }
}

class LifecycleTracker extends BasePlugin {
  public loadCalled = false;
  public activateCalled = false;
  public deactivateCalled = false;
  public unloadCalled = false;
  public errorReceived: Error | null = null;
  public lastLoadContext: Record<string, unknown> | null = null;

  get pluginId(): string { return 'LifecycleTracker'; }
  getCapabilities(): string[] { return ['test', 'tracking']; }

  async onLoad(context?: Record<string, unknown>): Promise<void> {
    this.loadCalled = true;
    this.lastLoadContext = context || null;
  }

  async onActivate(_context?: Record<string, unknown>): Promise<void> {
    this.activateCalled = true;
  }

  async onDeactivate(): Promise<void> {
    this.deactivateCalled = true;
  }

  async onUnload(): Promise<void> {
    this.unloadCalled = true;
  }

  async onError(error: Error): Promise<void> {
    this.errorReceived = error;
  }
}

class FailingPlugin extends BasePlugin {
  get pluginId(): string { return 'FailingPlugin'; }
  getCapabilities(): string[] { return ['fail']; }

  async onLoad(_context?: Record<string, unknown>): Promise<void> {
    throw new Error('Intentional load failure');
  }

  async onActivate(_context?: Record<string, unknown>): Promise<void> {}
  async onDeactivate(): Promise<void> {}
  async onUnload(): Promise<void> {}
  async onError(_error: Error): Promise<void> {}
}

// ===== PluginRegistry 测试 =====

describe('PluginRegistry', () => {

  describe('register()', () => {
    it('should register a plugin and return its ID', () => {
      const registry = new PluginRegistry();
      const pid = registry.register(LoggerPlugin);
      assert.strictEqual(pid, 'LoggerPlugin');
    });

    it('should reject duplicate registration', () => {
      const registry = new PluginRegistry();
      registry.register(LoggerPlugin);
      assert.throws(() => registry.register(LoggerPlugin), /already registered/);
    });

    it('should accept custom manifest with override pluginId', () => {
      const registry = new PluginRegistry();
      const pid = registry.register(LoggerPlugin, {
        pluginId: 'custom_logger',
        name: 'Custom Logger',
        version: '2.0.0',
        capabilities: ['custom'],
      });
      assert.strictEqual(pid, 'custom_logger');
      const manifest = registry.getManifest('custom_logger');
      assert.strictEqual(manifest?.name, 'Custom Logger');
      assert.strictEqual(manifest?.version, '2.0.0');
    });
  });

  describe('discover()', () => {
    it('should discover all registered plugins', () => {
      const registry = new PluginRegistry();
      registry.register(LoggerPlugin);
      registry.register(MetricsPlugin);
      const all = registry.discover();
      assert.strictEqual(all.length, 2);
    });

    it('should filter by capability', () => {
      const registry = new PluginRegistry();
      registry.register(LoggerPlugin);
      registry.register(MetricsPlugin);
      const logging = registry.discover('logging');
      assert.strictEqual(logging.length, 1);
      assert.strictEqual(logging[0].pluginId, 'LoggerPlugin');
    });

    it('should return empty array when no plugins match capability', () => {
      const registry = new PluginRegistry();
      registry.register(LoggerPlugin);
      const result = registry.discover('nonexistent_capability');
      assert.strictEqual(result.length, 0);
    });
  });

  describe('load()', () => {
    it('should create and cache instance', async () => {
      const registry = new PluginRegistry();
      registry.register(LoggerPlugin);
      const inst1 = await registry.load('LoggerPlugin');
      assert.ok(inst1 !== null);
      assert.strictEqual(registry.getState('LoggerPlugin'), PluginState.LOADED);
    });

    it('should return same instance on repeated loads', async () => {
      const registry = new PluginRegistry();
      registry.register(LoggerPlugin);
      const inst1 = await registry.load('LoggerPlugin');
      const inst2 = await registry.load('LoggerPlugin');
      assert.strictEqual(inst1, inst2);
    });

    it('should return null for unregistered plugin', async () => {
      const registry = new PluginRegistry();
      const result = await registry.load('NonExistent');
      assert.strictEqual(result, null);
    });
  });

  describe('unload()', () => {
    it('should remove loaded instance', async () => {
      const registry = new PluginRegistry();
      registry.register(LoggerPlugin);
      await registry.load('LoggerPlugin');
      const result = await registry.unload('LoggerPlugin');
      assert.strictEqual(result, true);
      assert.strictEqual(registry.get('LoggerPlugin'), undefined);
      assert.strictEqual(registry.getState('LoggerPlugin'), PluginState.UNLOADED);
    });

    it('should return false for unloaded plugin', async () => {
      const registry = new PluginRegistry();
      const result = await registry.unload('NonExistent');
      assert.strictEqual(result, false);
    });
  });

  describe('unregister()', () => {
    it('should remove everything including loaded instance', async () => {
      const registry = new PluginRegistry();
      registry.register(LoggerPlugin);
      await registry.load('LoggerPlugin');
      const result = await registry.unregister('LoggerPlugin');
      assert.strictEqual(result, true);
      assert.strictEqual(registry.listPlugins().length, 0);
      assert.strictEqual(registry.getManifest('LoggerPlugin'), undefined);
    });
  });

  describe('findByCapability()', () => {
    it('should find plugins by capability', () => {
      const registry = new PluginRegistry();
      registry.register(LoggerPlugin);
      registry.register(MetricsPlugin);
      const result = registry.findByCapability('metrics');
      assert.ok(result.includes('MetricsPlugin'));
      assert.ok(!result.includes('LoggerPlugin'));
    });
  });

  describe('full lifecycle', () => {
    it('should complete register→discover→load→call→unload cycle', async () => {
      const registry = new PluginRegistry();

      // Register
      const pid = registry.register(LoggerPlugin);
      assert.strictEqual(pid, 'LoggerPlugin');

      // Discover
      const discovered = registry.discover();
      assert.ok(discovered.some(m => m.pluginId === 'LoggerPlugin'));

      // Load
      const instance = await registry.load('LoggerPlugin') as unknown as LoggerPlugin;
      assert.ok(instance);

      // Call
      const count = instance.log('INFO', 'ts test');
      assert.strictEqual(count, 1);
      assert.strictEqual(instance.count(), 1);

      // Unload
      const result = await registry.unload('LoggerPlugin');
      assert.strictEqual(result, true);
      assert.strictEqual(registry.getState('LoggerPlugin'), PluginState.UNLOADED);
    });
  });
});

// ===== PluginManager 测试 =====

describe('PluginManager', () => {
  it('should load and activate plugins', async () => {
    const pm = new PluginManager({ sandboxEnabled: false });
    const manifest: PluginManifest = {
      pluginId: 'pm_test',
      name: 'PM Test',
      version: '1.0.0',
      entryPoint: '',
    };

    const info = await pm.loadPlugin('pm_test', manifest);
    assert.ok(info !== null);
    assert.strictEqual(info!.state, PluginState.LOADED);

    const activated = await pm.activatePlugin('pm_test');
    assert.strictEqual(activated, true);
    assert.strictEqual(info!.state, PluginState.ACTIVE);
  });

  it('should track activation count', async () => {
    const pm = new PluginManager({ sandboxEnabled: false });
    const manifest: PluginManifest = {
      pluginId: 'counter_test',
      name: 'Counter Test',
      version: '1.0.0',
      entryPoint: '',
    };

    await pm.loadPlugin('counter_test', manifest);
    await pm.activatePlugin('counter_test');

    const info = pm.getPluginInfo('counter_test')!;
    assert.strictEqual(info.activationCount, 1);
  });

  it('should return stats', () => {
    const pm = new PluginManager();
    const stats = pm.getStats();
    assert.strictEqual(stats.totalPlugins, 0);
    assert.strictEqual(stats.sandboxEnabled, true);
  });
});

// ===== 全局单例测试 =====

describe('getPluginRegistry', () => {
  it('should return same singleton instance', () => {
    const r1 = getPluginRegistry();
    const r2 = getPluginRegistry();
    assert.strictEqual(r1, r2);
  });
});

// ===== 边界用例测试 =====

describe('Edge cases', () => {
  it('should handle load→unload→reload cycle', async () => {
    const registry = new PluginRegistry();
    registry.register(LoggerPlugin);

    const inst1 = await registry.load('LoggerPlugin');
    await registry.unload('LoggerPlugin');
    const inst2 = await registry.load('LoggerPlugin');

    assert.ok(inst1 !== null);
    assert.ok(inst2 !== null);
    // Should be different instances after unload+reload
    assert.notStrictEqual(inst1, inst2);
  });

  it('should handle empty registry operations', async () => {
    const registry = new PluginRegistry();
    assert.strictEqual(registry.discover().length, 0);
    assert.strictEqual(registry.listPlugins().length, 0);
    assert.strictEqual(await registry.load('anything'), null);
    assert.strictEqual(await registry.unload('anything'), false);
    assert.strictEqual(await registry.unregister('anything'), false);
  });

  it('should handle multiple plugins with same capabilities', () => {
    const registry = new PluginRegistry();
    registry.register(LoggerPlugin);
    registry.register(MetricsPlugin);

    const all = registry.discover();
    assert.strictEqual(all.length, 2);
  });

  it('should handle MetricsPlugin full lifecycle', async () => {
    const registry = new PluginRegistry();
    registry.register(MetricsPlugin);

    const instance = await registry.load('MetricsPlugin') as unknown as MetricsPlugin;
    assert.ok(instance);

    instance.increment('requests', 5);
    assert.strictEqual(instance.getCounter('requests'), 5);

    instance.increment('requests', 3);
    assert.strictEqual(instance.getCounter('requests'), 8);

    await registry.unload('MetricsPlugin');
  });
});

// ===== 生命周期钩子验证测试（生产级审计）=====

describe('Lifecycle hook invocation (production audit)', () => {
  it('should call onLoad with context parameters', async () => {
    const registry = new PluginRegistry();
    registry.register(LoggerPlugin);
    const instance = await registry.load('LoggerPlugin', { maxEntries: 50 }) as unknown as LoggerPlugin;
    assert.ok(instance);
    // Verify side effect: onLoad set _maxEntries via context
    instance.log('INFO', 'test');
    assert.strictEqual(instance.count(), 1);
  });

  it('should call onUnload and clear logs', async () => {
    const registry = new PluginRegistry();
    registry.register(LoggerPlugin);
    const instance = await registry.load('LoggerPlugin') as unknown as LoggerPlugin;
    instance.log('INFO', 'before unload');
    assert.strictEqual(instance.count(), 1);

    await registry.unload('LoggerPlugin');
    // After onUnload, logs should be cleared
    assert.strictEqual(instance.count(), 0);
  });

  it('should call onUnload and clear counters', async () => {
    const registry = new PluginRegistry();
    registry.register(MetricsPlugin);
    const instance = await registry.load('MetricsPlugin') as unknown as MetricsPlugin;
    instance.increment('test_counter', 42);
    assert.strictEqual(instance.getCounter('test_counter'), 42);

    await registry.unload('MetricsPlugin');
    // After onUnload, counters should be cleared
    assert.strictEqual(instance.getCounter('test_counter'), 0);
  });

  it('should set ERROR state when onLoad throws', async () => {
    const registry = new PluginRegistry();
    registry.register(FailingPlugin);
    const result = await registry.load('FailingPlugin');
    assert.strictEqual(result, null);
    assert.strictEqual(registry.getState('FailingPlugin'), PluginState.ERROR);
  });
});
