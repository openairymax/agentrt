// AgentOS TypeScript SDK - Plugin Framework
// Version: 3.0.0
// Last updated: 2026-04-26
//
// 插件化框架，提供运行时的动态功能扩展能力。
// 与 Python SDK: framework/plugin.py 保持一致。

/**
 * PluginState - 插件状态枚举
 */
export enum PluginState {
  DISCOVERED = 'discovered',
  LOADED = 'loaded',
  ACTIVATING = 'activating',
  ACTIVE = 'active',
  DEACTIVATING = 'deactivating',
  INACTIVE = 'inactive',
  ERROR = 'error',
  UNLOADED = 'unloaded',
}

/**
 * PluginManifest - 插件清单
 */
export interface PluginManifest {
  pluginId: string;
  name: string;
  version: string;
  description?: string;
  author?: string;
  entryPoint?: string;
  entryClass?: string;
  dependencies?: PluginDependency[];
  capabilities?: string[];
  permissions?: string[];
  maxMemoryMb?: number;
  maxCpuPercent?: number;
  maxThreads?: number;
  timeoutSeconds?: number;
  tags?: string[];
}

/**
 * PluginDependency - 插件依赖
 */
export interface PluginDependency {
  pluginId: string;
  versionRange: string;
  optional?: boolean;
}

/**
 * PluginInfo - 插件运行时信息
 */
export interface PluginInfo {
  manifest: PluginManifest;
  state: PluginState;
  loadedAt?: Date;
  activatedAt?: Date;
  errorMessage?: string;
  activationCount: number;
  totalActiveTimeSeconds: number;
}

/**
 * BasePlugin - 插件基类
 *
 * 所有 AgentOS 插件都应继承此类，实现标准化的插件接口。
 */
export abstract class BasePlugin {
  protected _pluginId: string;

  constructor() {
    this._pluginId = this.constructor.name;
  }

  get pluginId(): string {
    return this._pluginId;
  }

  set pluginId(value: string) {
    this._pluginId = value;
  }

  async onLoad(context: Record<string, unknown>): Promise<void> {}
  async onActivate(context: Record<string, unknown>): Promise<void> {}
  async onDeactivate(): Promise<void> {}
  async onUnload(): Promise<void> {}
  async onError(error: Error): Promise<void> {}

  getCapabilities(): string[] {
    return [];
  }
}

/**
 * PluginRegistry - 插件注册表
 *
 * 提供简化的插件注册/发现/加载/卸载API。
 */
export class PluginRegistry {
  private _pluginClasses: Map<string, new () => BasePlugin> = new Map();
  private _instances: Map<string, BasePlugin> = new Map();
  private _manifests: Map<string, PluginManifest> = new Map();
  private _states: Map<string, PluginState> = new Map();

  /**
   * 注册插件类
   *
   * @param pluginClass - 插件类（必须继承BasePlugin）
   * @param manifest - 可选的插件清单
   * @returns 插件ID
   * @throws TypeError 如果pluginClass不继承BasePlugin
   * @throws Error 如果插件ID已存在
   */
  register(
    pluginClass: new () => BasePlugin,
    manifest?: Partial<PluginManifest>
  ): string {
    let temp: BasePlugin;
    try {
      temp = new pluginClass();
    } catch {
      throw new TypeError(`Plugin class must be a subclass of BasePlugin`);
    }

    const instancePluginId = temp.pluginId;
    const pluginId = manifest?.pluginId || instancePluginId;

    if (this._pluginClasses.has(pluginId)) {
      throw new Error(`Plugin '${pluginId}' already registered`);
    }

    const fullManifest: PluginManifest = {
      pluginId,
      name: manifest?.name ?? pluginClass.name,
      version: manifest?.version ?? '1.0.0',
      description: manifest?.description ?? '',
      author: manifest?.author ?? '',
      entryPoint: manifest?.entryPoint ?? '',
      entryClass: manifest?.entryClass ?? '',
      dependencies: manifest?.dependencies ?? [],
      capabilities: manifest?.capabilities ?? temp.getCapabilities(),
      permissions: manifest?.permissions ?? [],
      maxMemoryMb: manifest?.maxMemoryMb ?? 128,
      maxCpuPercent: manifest?.maxCpuPercent ?? 50.0,
      maxThreads: manifest?.maxThreads ?? 4,
      timeoutSeconds: manifest?.timeoutSeconds ?? 30.0,
      tags: manifest?.tags ?? [],
    };

    this._pluginClasses.set(pluginId, pluginClass);
    this._manifests.set(pluginId, fullManifest);
    this._states.set(pluginId, PluginState.DISCOVERED);

    return pluginId;
  }

  /**
   * 注销插件
   */
  async unregister(pluginId: string): Promise<boolean> {
    if (!this._pluginClasses.has(pluginId)) {
      return false;
    }

    if (this._instances.has(pluginId)) {
      await this.unload(pluginId);
    }

    this._pluginClasses.delete(pluginId);
    this._manifests.delete(pluginId);
    this._states.delete(pluginId);

    return true;
  }

  /**
   * 发现已注册的插件
   *
   * @param capability - 可选的能力过滤条件
   */
  discover(capability?: string): PluginManifest[] {
    const manifests = Array.from(this._manifests.values());
    if (capability) {
      return manifests.filter(
        (m) => m.capabilities && m.capabilities.includes(capability)
      );
    }
    return manifests;
  }

  /**
   * 加载插件实例
   */
  async load(pluginId: string, context?: Record<string, unknown>): Promise<BasePlugin | null> {
    const pluginClass = this._pluginClasses.get(pluginId);
    if (!pluginClass) {
      return null;
    }

    const existing = this._instances.get(pluginId);
    if (existing) {
      return existing;
    }

    const instance = new pluginClass();
    try {
      (instance as any).pluginId = pluginId;
    } catch {
      // Subclass may define pluginId as read-only getter
    }

    try {
      await instance.onLoad(context || {});
    } catch (e) {
      this._states.set(pluginId, PluginState.ERROR);
      try {
        await instance.onError(e instanceof Error ? e : new Error(String(e)));
      } catch (nestedError) {
        // onError itself failed, state already set to ERROR
      }
      return null;
    }

    this._instances.set(pluginId, instance);
    this._states.set(pluginId, PluginState.LOADED);

    return instance;
  }

  /**
   * 卸载插件实例
   */
  async unload(pluginId: string): Promise<boolean> {
    if (!this._instances.has(pluginId)) {
      return false;
    }

    const instance = this._instances.get(pluginId)!;
    try {
      await instance.onUnload();
    } catch (e) {
      try {
        await instance.onError(e instanceof Error ? e : new Error(String(e)));
      } catch (nestedError) {
        // onError itself failed
      }
    }

    this._instances.delete(pluginId);
    this._states.set(pluginId, PluginState.UNLOADED);

    return true;
  }

  async activate(pluginId: string, context?: Record<string, unknown>): Promise<boolean> {
    const instance = this._instances.get(pluginId);
    if (!instance) {
      return false;
    }
    const state = this._states.get(pluginId);
    if (state !== PluginState.LOADED && state !== PluginState.INACTIVE) {
      return false;
    }

    try {
      await instance.onActivate(context || {});
    } catch (e) {
      this._states.set(pluginId, PluginState.ERROR);
      try {
        await instance.onError(e instanceof Error ? e : new Error(String(e)));
      } catch (nestedError) {
        // onError itself failed
      }
      return false;
    }

    this._states.set(pluginId, PluginState.ACTIVE);
    return true;
  }

  async deactivate(pluginId: string): Promise<boolean> {
    const instance = this._instances.get(pluginId);
    if (!instance) {
      return false;
    }
    if (this._states.get(pluginId) !== PluginState.ACTIVE) {
      return false;
    }

    try {
      await instance.onDeactivate();
    } catch (e) {
      this._states.set(pluginId, PluginState.ERROR);
      try {
        await instance.onError(e instanceof Error ? e : new Error(String(e)));
      } catch (nestedError) {
        // onError itself failed
      }
      return false;
    }

    this._states.set(pluginId, PluginState.INACTIVE);
    return true;
  }

  /**
   * 获取已加载的插件实例
   */
  get(pluginId: string): BasePlugin | undefined {
    return this._instances.get(pluginId);
  }

  /**
   * 获取插件清单
   */
  getManifest(pluginId: string): PluginManifest | undefined {
    return this._manifests.get(pluginId);
  }

  /**
   * 获取插件状态
   */
  getState(pluginId: string): PluginState | undefined {
    return this._states.get(pluginId);
  }

  /**
   * 列出所有已注册的插件ID
   */
  listPlugins(): string[] {
    return Array.from(this._pluginClasses.keys());
  }

  /**
   * 按能力查找插件
   */
  findByCapability(capability: string): string[] {
    const result: string[] = [];
    for (const [pid, manifest] of this._manifests) {
      if (manifest.capabilities && manifest.capabilities.includes(capability)) {
        result.push(pid);
      }
    }
    return result;
  }
}

/**
 * PluginManager - 插件管理器
 *
 * 负责插件的完整生命周期管理，包括发现、加载、激活、停用、卸载。
 */
export class PluginManager {
  private _plugins: Map<string, PluginInfo> = new Map();
  private _sandboxEnabled: boolean;
  private _autoDiscover: boolean;
  private _pluginDirectories: string[];

  constructor(options?: {
    sandboxEnabled?: boolean;
    autoDiscover?: boolean;
    pluginDirectories?: string[];
  }) {
    this._sandboxEnabled = options?.sandboxEnabled ?? true;
    this._autoDiscover = options?.autoDiscover ?? true;
    this._pluginDirectories = options?.pluginDirectories ?? [];
  }

  async loadPlugin(
    pluginId: string,
    manifest: PluginManifest
  ): Promise<PluginInfo | null> {
    if (this._plugins.has(pluginId)) {
      return this._plugins.get(pluginId)!;
    }

    const info: PluginInfo = {
      manifest,
      state: PluginState.LOADED,
      loadedAt: new Date(),
      activationCount: 0,
      totalActiveTimeSeconds: 0,
    };

    this._plugins.set(pluginId, info);
    return info;
  }

  async unloadPlugin(pluginId: string): Promise<boolean> {
    const info = this._plugins.get(pluginId);
    if (!info) {
      return false;
    }

    if (info.state === PluginState.ACTIVE) {
      await this.deactivatePlugin(pluginId);
    }

    info.state = PluginState.UNLOADED;
    this._plugins.delete(pluginId);
    return true;
  }

  async activatePlugin(pluginId: string): Promise<boolean> {
    const info = this._plugins.get(pluginId);
    if (!info || (info.state !== PluginState.LOADED && info.state !== PluginState.INACTIVE)) {
      return false;
    }

    info.state = PluginState.ACTIVE;
    info.activatedAt = new Date();
    info.activationCount++;
    return true;
  }

  async deactivatePlugin(pluginId: string): Promise<boolean> {
    const info = this._plugins.get(pluginId);
    if (!info || info.state !== PluginState.ACTIVE) {
      return false;
    }

    if (info.activatedAt) {
      const activeTime =
        (new Date().getTime() - info.activatedAt.getTime()) / 1000;
      info.totalActiveTimeSeconds += activeTime;
    }

    info.state = PluginState.INACTIVE;
    return true;
  }

  getPluginInfo(pluginId: string): PluginInfo | undefined {
    return this._plugins.get(pluginId);
  }

  listPlugins(stateFilter?: PluginState): PluginInfo[] {
    const plugins = Array.from(this._plugins.values());
    if (stateFilter) {
      return plugins.filter((p) => p.state === stateFilter);
    }
    return plugins;
  }

  getActivePlugins(): PluginInfo[] {
    return this.listPlugins(PluginState.ACTIVE);
  }

  getStats(): Record<string, unknown> {
    const stateCounts: Record<string, number> = {};
    for (const state of Object.values(PluginState)) {
      stateCounts[state] = 0;
    }
    for (const info of this._plugins.values()) {
      stateCounts[info.state]++;
    }

    return {
      totalPlugins: this._plugins.size,
      stateDistribution: stateCounts,
      activePlugins: stateCounts[PluginState.ACTIVE] ?? 0,
      sandboxEnabled: this._sandboxEnabled,
      pluginDirectories: this._pluginDirectories,
    };
  }
}

let _globalRegistry: PluginRegistry | null = null;

export function getPluginRegistry(): PluginRegistry {
  if (!_globalRegistry) {
    _globalRegistry = new PluginRegistry();
  }
  return _globalRegistry;
}
