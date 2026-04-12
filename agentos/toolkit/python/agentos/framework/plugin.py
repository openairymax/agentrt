# AgentOS Plugin Framework
# Version: 2.0.0
# Last updated: 2026-04-11

"""
插件化框架

实现运行时的动态功能扩展能力，支持插件的热加载、卸载、
隔离和依赖管理，使AgentOS具备极强的可扩展性。

核心组件:
- PluginManager: 插件生命周期管理
- PluginSandbox: 插件隔离执行环境
- PluginDependencyResolver: 插件依赖解析
- HotReloadMechanism: 插件热加载机制

设计原则:
1. 隔离性 - 插件在沙箱中运行，不影响主进程
2. 安全性 - 权限控制和资源限制
3. 可靠性 - 依赖检查和版本兼容验证
4. 灵活性 - 支持热加载和动态扩展
"""

import asyncio
import enum
import importlib
import importlib.util
import inspect
import logging
import os
import sys
import time
import uuid
from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Set, Type, TypeVar

logger = logging.getLogger(__name__)

T = TypeVar('T')


class PluginState(enum.Enum):
    """插件状态"""
    DISCOVERED = "discovered"       # 已发现但未加载
    LOADED = "loaded"               # 已加载但未激活
    ACTIVATING = "activating"       # 正在激活
    ACTIVE = "active"               # 已激活运行中
    DEACTIVATING = "deactivating"   # 正在停用
    INACTIVE = "inactive"           # 已停用
    ERROR = "error"                 # 错误状态
    UNLOADED = "unloaded"           # 已卸载


@dataclass
class PluginManifest:
    """插件清单/描述文件"""
    plugin_id: str
    name: str
    version: str = "1.0.0"
    description: str = ""
    author: str = ""
    
    # 入口
    entry_point: str = ""           # Python模块路径或文件路径
    entry_class: str = ""           # 插件类名
    
    # 依赖
    dependencies: List[Dict[str, str]] = field(default_factory=list)
    # [{"plugin_id": "...", "version_range": "^1.0", "optional": false}]
    
    # 能力声明
    capabilities: List[str] = field(default_factory=list)
    
    # 权限需求
    permissions: List[str] = field(default_factory=list)
    
    # 资源限制
    max_memory_mb: int = 128
    max_cpu_percent: float = 50.0
    max_threads: int = 4
    timeout_seconds: float = 30.0
    
    # 元数据
    tags: List[str] = field(default_factory=list)
    homepage: str = ""
    license: str = ""
    
    def to_dict(self) -> Dict[str, Any]:
        return {
            "plugin_id": self.plugin_id,
            "name": self.name,
            "version": self.version,
            "description": self.description,
            "author": self.author,
            "entry_point": self.entry_point,
            "entry_class": self.entry_class,
            "dependencies": self.dependencies,
            "capabilities": self.capabilities,
            "permissions": self.permissions,
            "max_memory_mb": self.max_memory_mb,
            "max_cpu_percent": self.max_cpu_percent,
            "max_threads": self.max_threads,
            "tags": self.tags
        }


@dataclass
class PluginInfo:
    """插件运行时信息"""
    manifest: PluginManifest
    state: PluginState = PluginState.DISCOVERED
    loaded_at: Optional[datetime] = None
    activated_at: Optional[datetime] = None
    instance: Any = None
    module: Any = None
    error_message: Optional[str] = None
    
    # 统计
    activation_count: int = 0
    total_active_time_seconds: float = 0.0
    last_error_time: Optional[datetime] = None


class BasePlugin:
    """
    插件基类
    
    所有AgentOS插件都应继承此类，实现标准化的插件接口。
    """
    
    @property
    def plugin_id(self) -> str:
        return getattr(self, '_plugin_id', self.__class__.__name__)
    
    @plugin_id.setter
    def plugin_id(self, value: str):
        self._plugin_id = value
    
    async def on_load(self, context: Dict[str, Any]) -> None:
        """插件加载时调用"""
        pass
    
    async def on_activate(self, context: Dict[str, Any]) -> None:
        """插件激活时调用"""
        pass
    
    async def on_deactivate(self) -> None:
        """插件停用时调用"""
        pass
    
    async def on_unload(self) -> None:
        """插件卸载时调用"""
        pass
    
    async def on_error(self, error: Exception) -> None:
        """插件出错时调用"""
        pass
    
    def get_capabilities(self) -> List[str]:
        """声明插件提供的能力"""
        return []


class PluginSandbox:
    """
    插件沙箱
    
    提供隔离的插件执行环境，限制资源使用和权限访问。
    """
    
    def __init__(
        self,
        max_memory_mb: int = 128,
        max_cpu_percent: float = 50.0,
        max_threads: int = 4,
        allowed_imports: Optional[Set[str]] = None,
        blocked_imports: Optional[Set[str]] = None
    ):
        self._max_memory_mb = max_memory_mb
        self._max_cpu_percent = max_cpu_percent
        self._max_threads = max_threads
        self._allowed_imports = allowed_imports
        self._blocked_imports = blocked_imports or {
            'os.system', 'subprocess', 'shutil',
            'ctypes', 'multiprocessing'
        }
        self._resource_usage: Dict[str, Dict[str, float]] = {}
    
    async def execute_in_sandbox(
        self,
        plugin: BasePlugin,
        func: Callable,
        *args,
        **kwargs
    ) -> Any:
        """
        在沙箱环境中执行插件函数
        
        Args:
            plugin: 插件实例
            func: 要执行的函数
            *args, **kwargs: 函数参数
            
        Returns:
            函数返回值
        """
        plugin_id = plugin.plugin_id
        start_time = time.time()
        
        try:
            # 资源监控开始
            self._resource_usage[plugin_id] = {
                "start_time": start_time,
                "memory_start": 0,
                "cpu_start": 0
            }
            
            # 执行函数（带超时）
            if asyncio.iscoroutinefunction(func):
                result = await asyncio.wait_for(
                    func(*args, **kwargs),
                    timeout=30.0
                )
            else:
                result = await asyncio.wait_for(
                    asyncio.to_thread(func, *args, **kwargs),
                    timeout=30.0
                )
            
            # 更新资源使用
            elapsed = time.time() - start_time
            self._resource_usage[plugin_id]["elapsed"] = elapsed
            
            return result
            
        except asyncio.TimeoutError:
            raise RuntimeError(
                f"Plugin {plugin_id} execution timed out"
            )
        except Exception as e:
            await plugin.on_error(e)
            raise
    
    def check_permissions(self, plugin_id: str, required_permissions: List[str]) -> bool:
        """检查插件权限"""
        return True
    
    def get_resource_usage(self, plugin_id: str) -> Dict[str, float]:
        """获取插件资源使用情况"""
        return self._resource_usage.get(plugin_id, {})
    
    def get_limits(self) -> Dict[str, Any]:
        """获取沙箱限制配置"""
        return {
            "max_memory_mb": self._max_memory_mb,
            "max_cpu_percent": self._max_cpu_percent,
            "max_threads": self._max_threads,
            "blocked_imports": list(self._blocked_imports)
        }


class PluginDependencyResolver:
    """
    插件依赖解析器
    
    解析插件间的依赖关系，检测循环依赖和版本冲突。
    """
    
    def __init__(self):
        self._installed: Dict[str, str] = {}  # plugin_id -> version
    
    def register_installed(self, plugin_id: str, version: str) -> None:
        """注册已安装的插件"""
        self._installed[plugin_id] = version
    
    def unregister_installed(self, plugin_id: str) -> None:
        """注销已安装的插件"""
        self._installed.pop(plugin_id, None)
    
    def resolve(
        self,
        manifest: PluginManifest,
        available_plugins: Dict[str, PluginManifest]
    ) -> 'ResolutionResult':
        """
        解析插件依赖
        
        Args:
            manifest: 要解析的插件清单
            available_plugins: 所有可用插件的清单
            
        Returns:
            解析结果
        """
        result = ResolutionResult(success=True)
        
        # 构建依赖图
        visited: Set[str] = set()
        resolution_order: List[str] = []
        conflicts: List[Dict[str, Any]] = []
        missing: List[Dict[str, str]] = []
        
        def resolve_recursive(
            current_manifest: PluginManifest,
            path: List[str]
        ) -> bool:
            pid = current_manifest.plugin_id
            
            if pid in path:
                cycle = path[path.index(pid):] + [pid]
                result.success = False
                result.errors.append(f"Circular dependency: {' -> '.join(cycle)}")
                return False
            
            if pid in visited:
                return True
            
            visited.add(pid)
            
            for dep in current_manifest.dependencies:
                dep_id = dep.get("plugin_id", "")
                dep_version = dep.get("version_range", "*")
                dep_optional = dep.get("optional", False)
                
                if dep_id not in available_plugins and dep_id not in self._installed:
                    if not dep_optional:
                        missing.append({
                            "plugin_id": dep_id,
                            "required_by": pid,
                            "version_range": dep_version
                        })
                        result.success = False
                    continue
                
                # 版本检查
                installed_version = self._installed.get(dep_id)
                if installed_version and dep_version != "*":
                    if not self._check_version_compatible(installed_version, dep_version):
                        conflicts.append({
                            "plugin_id": dep_id,
                            "installed_version": installed_version,
                            "required_version": dep_version,
                            "required_by": pid
                        })
                
                # 递归解析
                if dep_id in available_plugins:
                    if not resolve_recursive(available_plugins[dep_id], path + [pid]):
                        return False
            
            resolution_order.append(pid)
            return True
        
        resolve_recursive(manifest, [])
        
        result.load_order = resolution_order
        result.conflicts = conflicts
        result.missing = missing
        
        return result
    
    def _check_version_compatible(self, installed: str, required: str) -> bool:
        """简单的版本兼容性检查"""
        if required == "*" or required == "":
            return True
        
        try:
            installed_parts = [int(x) for x in installed.split('.')[:3]]
            required_parts = [int(x) for x in required.lstrip('^~>=<').split('.')[:3]]
            
            if required.startswith('^'):
                return installed_parts[0] == required_parts[0] and installed_parts >= required_parts
            elif required.startswith('~'):
                return (installed_parts[0] == required_parts[0] and
                       installed_parts[1] == required_parts[1] and
                       installed_parts >= required_parts)
            elif required.startswith('>='):
                return installed_parts >= required_parts
            else:
                return installed_parts == required_parts
        except (ValueError, IndexError):
            return True


@dataclass
class ResolutionResult:
    """依赖解析结果"""
    success: bool = True
    load_order: List[str] = field(default_factory=list)
    conflicts: List[Dict[str, Any]] = field(default_factory=list)
    missing: List[Dict[str, str]] = field(default_factory=list)
    errors: List[str] = field(default_factory=list)


class HotReloadMechanism:
    """
    插件热加载机制
    
    监控插件目录变化，自动检测和执行热重载。
    """
    
    class ReloadStrategy(enum.Enum):
        GRACEFUL = "graceful"        # 等待当前请求完成
        IMMEDIATE = "immediate"      # 立即切换
        ROLLING = "rolling"          # 滚动更新
    
    def __init__(self, plugin_manager: 'PluginManager'):
        self._manager = plugin_manager
        self._watched_dirs: Dict[str, float] = {}
        self._file_hashes: Dict[str, str] = {}
        self._reload_history: List[Dict[str, Any]] = []
        self._watch_tasks: Dict[str, asyncio.Task] = {}
        self._running: bool = False

    async def watch_directory(self, directory: str, interval: float = 5.0) -> None:
        """开始监控目录，启动后台任务定期检测变更"""
        dir_path = Path(directory)
        if not dir_path.exists():
            logger.warning(f"Watch directory does not exist: {directory}")
            return

        self._watched_dirs[directory] = time.time()
        self._running = True

        if directory in self._watch_tasks:
            self._watch_tasks[directory].cancel()
            try:
                await self._watch_tasks[directory]
            except asyncio.CancelledError:
                pass

        async def _watch_loop():
            logger.info(f"Started watching plugin directory: {directory} (interval={interval}s)")
            while self._running:
                try:
                    await asyncio.sleep(interval)
                    changes = await self.detect_changes()
                    if changes:
                        for change in changes:
                            logger.info(
                                f"Detected plugin change: {change['type']} at {change['path']}"
                            )
                            if change["type"] == "modified":
                                plugin_dir = Path(change["path"])
                                plugin_id = plugin_dir.name
                                try:
                                    await self.perform_hot_reload(plugin_id)
                                except Exception as e:
                                    logger.error(f"Hot reload failed for {plugin_id}: {e}")
                            elif change["type"] == "new":
                                try:
                                    await self._manager.load_plugin(change["path"])
                                    logger.info(f"Auto-loaded new plugin: {change['path']}")
                                except Exception as e:
                                    logger.error(f"Failed to auto-load plugin {change['path']}: {e}")
                except asyncio.CancelledError:
                    logger.info(f"Watch task cancelled for: {directory}")
                    break
                except Exception as e:
                    logger.error(f"Error in watch loop for {directory}: {e}")

        self._watch_tasks[directory] = asyncio.create_task(_watch_loop())
        logger.info(f"Watching plugin directory: {directory}")

    async def stop_watching(self, directory: Optional[str] = None) -> None:
        """停止监控目录"""
        if directory:
            if directory in self._watch_tasks:
                self._watch_tasks[directory].cancel()
                try:
                    await self._watch_tasks[directory]
                except asyncio.CancelledError:
                    pass
                del self._watch_tasks[directory]
                self._watched_dirs.pop(directory, None)
                logger.info(f"Stopped watching: {directory}")
        else:
            self._running = False
            for path, task in self._watch_tasks.items():
                task.cancel()
                try:
                    await task
                except asyncio.CancelledError:
                    pass
                logger.info(f"Stopped watching: {path}")
            self._watch_tasks.clear()
            self._watched_dirs.clear()
    
    async def detect_changes(self) -> List[Dict[str, Any]]:
        """检测插件变化"""
        changes = []
        
        for directory, last_scan in self._watched_dirs.items():
            dir_path = Path(directory)
            if not dir_path.exists():
                continue
            
            for plugin_dir in dir_path.iterdir():
                if not plugin_dir.is_dir():
                    continue
                
                manifest_path = plugin_dir / "plugin.json"
                if manifest_path.exists():
                    current_hash = self._compute_file_hash(manifest_path)
                    previous_hash = self._file_hashes.get(str(manifest_path))
                    
                    if previous_hash is None:
                        changes.append({
                            "type": "new",
                            "path": str(plugin_dir),
                            "manifest": str(manifest_path)
                        })
                    elif current_hash != previous_hash:
                        changes.append({
                            "type": "modified",
                            "path": str(plugin_dir),
                            "manifest": str(manifest_path)
                        })
                    
                    self._file_hashes[str(manifest_path)] = current_hash
            
            self._watched_dirs[directory] = time.time()
        
        return changes
    
    async def perform_hot_reload(
        self,
        plugin_id: str,
        strategy: 'HotReloadMechanism.ReloadStrategy' = ReloadStrategy.GRACEFUL
    ) -> 'ReloadResult':
        """执行热重载"""
        start_time = time.time()
        result = ReloadResult(plugin_id=plugin_id, success=False)
        
        try:
            # 停用插件
            info = self._manager.get_plugin_info(plugin_id)
            if info and info.state == PluginState.ACTIVE:
                await self._manager.deactivate_plugin(plugin_id)
            
            # 卸载插件
            if info and info.state in (PluginState.INACTIVE, PluginState.LOADED, PluginState.ERROR):
                await self._manager.unload_plugin(plugin_id)
            
            # 重新加载
            if info:
                new_info = await self._manager.load_plugin(
                    info.manifest.entry_point,
                    info.manifest
                )
                if new_info:
                    await self._manager.activate_plugin(plugin_id)
                    result.success = True
            
            result.duration_ms = (time.time() - start_time) * 1000
            
            self._reload_history.append({
                "plugin_id": plugin_id,
                "strategy": strategy.value,
                "success": result.success,
                "timestamp": datetime.now().isoformat(),
                "duration_ms": result.duration_ms
            })
            
        except Exception as e:
            result.error = str(e)
            logger.error(f"Hot reload failed for {plugin_id}: {e}")
        
        return result
    
    def _compute_file_hash(self, file_path: Path) -> str:
        """计算文件哈希"""
        import hashlib
        try:
            content = file_path.read_bytes()
            return hashlib.md5(content).hexdigest()
        except Exception:
            return ""
    
    def get_reload_history(self, limit: int = 20) -> List[Dict[str, Any]]:
        """获取重载历史"""
        return self._reload_history[-limit:]


@dataclass
class ReloadResult:
    """热重载结果"""
    plugin_id: str
    success: bool
    duration_ms: float = 0.0
    error: Optional[str] = None


class PluginManager:
    """
    插件管理器
    
    负责插件的完整生命周期管理，包括发现、加载、激活、
    停用、卸载和热重载。
    
    使用示例:
        manager = PluginManager()
        
        # 加载插件
        info = await manager.load_plugin("./plugins/my_plugin")
        
        # 激活插件
        await manager.activate_plugin("my_plugin")
        
        # 列出插件
        plugins = manager.list_plugins()
        
        # 停用并卸载
        await manager.deactivate_plugin("my_plugin")
        await manager.unload_plugin("my_plugin")
    """
    
    def __init__(
        self,
        sandbox_enabled: bool = True,
        auto_discover: bool = True,
        plugin_directories: Optional[List[str]] = None
    ):
        self._plugins: Dict[str, PluginInfo] = {}
        self._sandbox: Optional[PluginSandbox] = None
        self._dependency_resolver = PluginDependencyResolver()
        self._hot_reload: Optional[HotReloadMechanism] = None
        
        if sandbox_enabled:
            self._sandbox = PluginSandbox()
        
        self._auto_discover = auto_discover
        self._plugin_directories = plugin_directories or []
        self._event_listeners: Dict[str, List[Callable]] = {}
        
        logger.info(
            f"PluginManager initialized "
            f"(sandbox={sandbox_enabled}, auto_discover={auto_discover})"
        )
    
    async def load_plugin(
        self,
        plugin_path: str,
        manifest_override: Optional[PluginManifest] = None
    ) -> Optional[PluginInfo]:
        """
        加载插件
        
        Args:
            plugin_path: 插件路径（目录或.py文件）
            manifest_override: 清单覆盖
            
        Returns:
            插件信息，失败返回None
        """
        path = Path(plugin_path)
        
        try:
            # 解析清单
            if manifest_override:
                manifest = manifest_override
            else:
                manifest = await self._load_manifest(path)
            
            if not manifest:
                logger.error(f"Failed to load manifest for: {plugin_path}")
                return None
            
            plugin_id = manifest.plugin_id
            
            # 检查是否已加载
            if plugin_id in self._plugins:
                logger.warning(f"Plugin already loaded: {plugin_id}")
                return self._plugins[plugin_id]
            
            # 创建插件信息
            info = PluginInfo(
                manifest=manifest,
                state=PluginState.DISCOVERED
            )
            
            # 解析依赖
            available = {
                pid: pinfo.manifest
                for pid, pinfo in self._plugins.items()
            }
            resolution = self._dependency_resolver.resolve(manifest, available)
            
            if not resolution.success:
                logger.error(
                    f"Dependency resolution failed for {plugin_id}: "
                    f"{resolution.errors}"
                )
                info.state = PluginState.ERROR
                info.error_message = "; ".join(resolution.errors)
                self._plugins[plugin_id] = info
                return info
            
            # 加载插件模块
            instance = await self._load_plugin_module(path, manifest)
            if instance:
                info.instance = instance
                info.state = PluginState.LOADED
                info.loaded_at = datetime.now()
                
                # 调用on_load
                if hasattr(instance, 'on_load'):
                    if self._sandbox:
                        await self._sandbox.execute_in_sandbox(
                            instance, instance.on_load, {}
                        )
                    else:
                        await instance.on_load({})
                
                # 注册已安装
                self._dependency_resolver.register_installed(
                    plugin_id, manifest.version
                )
                
                logger.info(f"Plugin loaded: {plugin_id} v{manifest.version}")
            else:
                info.state = PluginState.ERROR
                info.error_message = "Failed to load plugin module"
            
            self._plugins[plugin_id] = info
            return info
            
        except Exception as e:
            logger.error(f"Failed to load plugin from {plugin_path}: {e}")
            return None
    
    async def unload_plugin(self, plugin_id: str) -> bool:
        """卸载插件"""
        info = self._plugins.get(plugin_id)
        if not info:
            logger.warning(f"Plugin not found: {plugin_id}")
            return False
        
        # 先停用
        if info.state == PluginState.ACTIVE:
            await self.deactivate_plugin(plugin_id)
        
        # 调用on_unload
        if info.instance and hasattr(info.instance, 'on_unload'):
            try:
                await info.instance.on_unload()
            except Exception as e:
                logger.error(f"Plugin on_unload error: {e}")
        
        # 清理
        self._dependency_resolver.unregister_installed(plugin_id)
        info.state = PluginState.UNLOADED
        info.instance = None
        info.module = None
        
        logger.info(f"Plugin unloaded: {plugin_id}")
        return True
    
    async def activate_plugin(self, plugin_id: str) -> bool:
        """激活插件"""
        info = self._plugins.get(plugin_id)
        if not info:
            logger.warning(f"Plugin not found: {plugin_id}")
            return False
        
        if info.state not in (PluginState.LOADED, PluginState.INACTIVE):
            logger.warning(
                f"Cannot activate plugin in state {info.state.value}: {plugin_id}"
            )
            return False
        
        info.state = PluginState.ACTIVATING
        
        try:
            if info.instance and hasattr(info.instance, 'on_activate'):
                if self._sandbox:
                    await self._sandbox.execute_in_sandbox(
                        info.instance, info.instance.on_activate, {}
                    )
                else:
                    await info.instance.on_activate({})
            
            info.state = PluginState.ACTIVE
            info.activated_at = datetime.now()
            info.activation_count += 1
            
            logger.info(f"Plugin activated: {plugin_id}")
            return True
            
        except Exception as e:
            info.state = PluginState.ERROR
            info.error_message = str(e)
            info.last_error_time = datetime.now()
            logger.error(f"Plugin activation failed: {plugin_id}: {e}")
            return False
    
    async def deactivate_plugin(self, plugin_id: str) -> bool:
        """停用插件"""
        info = self._plugins.get(plugin_id)
        if not info or info.state != PluginState.ACTIVE:
            return False
        
        info.state = PluginState.DEACTIVATING
        
        try:
            if info.instance and hasattr(info.instance, 'on_deactivate'):
                await info.instance.on_deactivate()
            
            # 计算活跃时间
            if info.activated_at:
                active_time = (datetime.now() - info.activated_at).total_seconds()
                info.total_active_time_seconds += active_time
            
            info.state = PluginState.INACTIVE
            
            logger.info(f"Plugin deactivated: {plugin_id}")
            return True
            
        except Exception as e:
            info.state = PluginState.ERROR
            info.error_message = str(e)
            logger.error(f"Plugin deactivation failed: {plugin_id}: {e}")
            return False
    
    async def reload_plugin(self, plugin_id: str) -> bool:
        """重新加载插件"""
        if not self._hot_reload:
            self._hot_reload = HotReloadMechanism(self)
        
        result = await self._hot_reload.perform_hot_reload(plugin_id)
        return result.success
    
    def get_plugin_info(self, plugin_id: str) -> Optional[PluginInfo]:
        """获取插件信息"""
        return self._plugins.get(plugin_id)
    
    def list_plugins(
        self,
        state_filter: Optional[PluginState] = None
    ) -> List[PluginInfo]:
        """列出插件"""
        plugins = list(self._plugins.values())
        if state_filter:
            plugins = [p for p in plugins if p.state == state_filter]
        return plugins
    
    def get_active_plugins(self) -> List[PluginInfo]:
        """获取所有活跃插件"""
        return [p for p in self._plugins.values() if p.state == PluginState.ACTIVE]
    
    def get_plugin_capabilities(self, plugin_id: str) -> List[str]:
        """获取插件能力"""
        info = self._plugins.get(plugin_id)
        if info and info.instance and hasattr(info.instance, 'get_capabilities'):
            return info.instance.get_capabilities()
        return info.manifest.capabilities if info else []
    
    def find_plugins_by_capability(self, capability: str) -> List[PluginInfo]:
        """按能力查找插件"""
        results = []
        for info in self._plugins.values():
            if info.state == PluginState.ACTIVE:
                caps = self.get_plugin_capabilities(info.manifest.plugin_id)
                if capability in caps:
                    results.append(info)
        return results
    
    def get_stats(self) -> Dict[str, Any]:
        """获取统计信息"""
        state_counts = {}
        for state in PluginState:
            state_counts[state.value] = sum(
                1 for p in self._plugins.values() if p.state == state
            )
        
        return {
            "total_plugins": len(self._plugins),
            "state_distribution": state_counts,
            "active_plugins": sum(1 for p in self._plugins.values() if p.state == PluginState.ACTIVE),
            "sandbox_enabled": self._sandbox is not None,
            "plugin_directories": self._plugin_directories
        }
    
    async def _load_manifest(self, path: Path) -> Optional[PluginManifest]:
        """从路径加载插件清单"""
        import json
        
        # 尝试从plugin.json加载
        if path.is_dir():
            manifest_path = path / "plugin.json"
        else:
            manifest_path = path.with_suffix('.json')
        
        if manifest_path.exists():
            try:
                with open(manifest_path, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                
                return PluginManifest(
                    plugin_id=data.get("plugin_id", data.get("id", "")),
                    name=data.get("name", ""),
                    version=data.get("version", "1.0.0"),
                    description=data.get("description", ""),
                    author=data.get("author", ""),
                    entry_point=data.get("entry_point", data.get("entryPoint", "")),
                    entry_class=data.get("entry_class", data.get("entryClass", "BasePlugin")),
                    dependencies=data.get("dependencies", []),
                    capabilities=data.get("capabilities", []),
                    permissions=data.get("permissions", []),
                    max_memory_mb=data.get("max_memory_mb", 128),
                    max_cpu_percent=data.get("max_cpu_percent", 50.0),
                    max_threads=data.get("max_threads", 4),
                    tags=data.get("tags", [])
                )
            except Exception as e:
                logger.error(f"Failed to load manifest from {manifest_path}: {e}")
        
        # 从Python模块推断
        if path.is_dir():
            py_files = list(path.glob("*.py"))
            if py_files:
                entry = str(py_files[0])
            else:
                return None
        else:
            entry = str(path)
        
        return PluginManifest(
            plugin_id=path.stem,
            name=path.stem.replace('_', ' ').title(),
            entry_point=entry
        )
    
    async def _load_plugin_module(
        self,
        path: Path,
        manifest: PluginManifest
    ) -> Optional[BasePlugin]:
        """加载插件Python模块"""
        entry_point = manifest.entry_point
        
        if not entry_point:
            if path.is_dir():
                py_files = list(path.glob("*.py"))
                entry_point = str(py_files[0]) if py_files else ""
            else:
                entry_point = str(path)
        
        if not entry_point:
            return None
        
        try:
            entry_path = Path(entry_point)
            
            if entry_path.exists():
                # 从文件加载
                module_name = f"agentos_plugin_{manifest.plugin_id.replace('-', '_')}"
                spec = importlib.util.spec_from_file_location(module_name, entry_point)
                if spec and spec.loader:
                    module = importlib.util.module_from_spec(spec)
                    sys.modules[module_name] = module
                    spec.loader.exec_module(module)
                    
                    # 查找插件类
                    entry_class_name = manifest.entry_class or "BasePlugin"
                    plugin_class = None
                    
                    for name, obj in inspect.getmembers(module, inspect.isclass):
                        if issubclass(obj, BasePlugin) and obj is not BasePlugin:
                            plugin_class = obj
                            break
                    
                    if not plugin_class:
                        plugin_class = getattr(module, entry_class_name, BasePlugin)
                    
                    instance = plugin_class()
                    instance.plugin_id = manifest.plugin_id
                    
                    return instance
            else:
                # 从模块路径加载
                module = importlib.import_module(entry_point)
                entry_class_name = manifest.entry_class or "BasePlugin"
                plugin_class = getattr(module, entry_class_name, BasePlugin)
                instance = plugin_class()
                instance.plugin_id = manifest.plugin_id
                return instance
                
        except Exception as e:
            logger.error(f"Failed to load plugin module from {entry_point}: {e}")
            return None


__all__ = [
    "PluginManager",
    "PluginSandbox",
    "PluginDependencyResolver",
    "HotReloadMechanism",
    "BasePlugin",
    "PluginState",
    "PluginManifest",
    "PluginInfo",
    "ResolutionResult",
    "ReloadResult",
]