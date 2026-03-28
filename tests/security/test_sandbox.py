﻿# AgentOS 沙箱安全测试
# Version: 1.0.0.6
# Last updated: 2026-03-23

"""
AgentOS 沙箱安全测试模块。

测试沙箱隔离机制、权限控制、资源限制等安全功能。
"""

import pytest
import os
import sys
import time
import threading
import tempfile
import shutil
from typing import Dict, Any, List, Optional, Set
from unittest.mock import Mock, MagicMock, patch
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from contextlib import contextmanager

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', 'tools', 'python')))


# ============================================================
# 测试标记
# ============================================================

pytestmark = pytest.mark.security


# ============================================================
# 枚举和数据类定义
# ============================================================

class Permission(Enum):
    """权限枚举"""
    READ_FILE = "read_file"
    WRITE_FILE = "write_file"
    EXECUTE_PROCESS = "execute_process"
    NETWORK_OUTBOUND = "network_outbound"
    NETWORK_INBOUND = "network_inbound"
    ACCESS_ENV = "access_env"
    ACCESS_REGISTRY = "access_registry"
    CREATE_PROCESS = "create_process"


class ResourceType(Enum):
    """资源类型枚举"""
    CPU = "cpu"
    MEMORY = "memory"
    DISK = "disk"
    NETWORK = "network"
    TIME = "time"


class SandboxState(Enum):
    """沙箱状态枚举"""
    CREATED = "created"
    RUNNING = "running"
    PAUSED = "paused"
    STOPPED = "stopped"
    ERROR = "error"


@dataclass
class ResourceLimits:
    """资源限制"""
    max_cpu_percent: float = 80.0
    max_memory_mb: int = 512
    max_disk_mb: int = 1024
    max_network_bandwidth_mbps: int = 10
    max_execution_time_seconds: int = 300
    max_file_size_mb: int = 100
    max_open_files: int = 100
    max_processes: int = 10


@dataclass
class SandboxConfig:
    """沙箱配置"""
    sandbox_id: str
    allowed_permissions: Set[Permission] = field(default_factory=set)
    resource_limits: ResourceLimits = field(default_factory=ResourceLimits)
    allowed_paths: List[str] = field(default_factory=list)
    denied_paths: List[str] = field(default_factory=list)
    allowed_network_hosts: List[str] = field(default_factory=list)
    environment_vars: Dict[str, str] = field(default_factory=dict)


@dataclass
class SandboxMetrics:
    """沙箱指标"""
    cpu_usage_percent: float = 0.0
    memory_usage_mb: float = 0.0
    disk_usage_mb: float = 0.0
    network_bytes_sent: int = 0
    network_bytes_received: int = 0
    execution_time_seconds: float = 0.0
    file_operations_count: int = 0
    process_count: int = 0


# ============================================================
# 沙箱管理器实现
# ============================================================

class SandboxManager:
    """
    沙箱管理器。
    
    提供沙箱创建、销毁、权限控制和资源限制功能。
    """
    
    def __init__(self):
        """初始化沙箱管理器"""
        self._sandboxes: Dict[str, 'Sandbox'] = {}
        self._lock = threading.RLock()
    
    def create_sandbox(self, manager: SandboxConfig) -> 'Sandbox':
        """
        创建沙箱。
        
        Args:
            manager: 沙箱配置
            
        Returns:
            Sandbox: 创建的沙箱实例
        """
        with self._lock:
            if manager.sandbox_id in self._sandboxes:
                raise ValueError(f"沙箱 {manager.sandbox_id} 已存在")
            
            sandbox = Sandbox(manager)
            self._sandboxes[manager.sandbox_id] = sandbox
            
            return sandbox
    
    def get_sandbox(self, sandbox_id: str) -> Optional['Sandbox']:
        """
        获取沙箱。
        
        Args:
            sandbox_id: 沙箱 ID
            
        Returns:
            Optional[Sandbox]: 沙箱实例
        """
        with self._lock:
            return self._sandboxes.get(sandbox_id)
    
    def destroy_sandbox(self, sandbox_id: str) -> bool:
        """
        销毁沙箱。
        
        Args:
            sandbox_id: 沙箱 ID
            
        Returns:
            bool: 是否成功销毁
        """
        with self._lock:
            if sandbox_id not in self._sandboxes:
                return False
            
            sandbox = self._sandboxes.pop(sandbox_id)
            sandbox.stop()
            
            return True
    
    def list_sandboxes(self) -> List[str]:
        """
        列出所有沙箱 ID。
        
        Returns:
            List[str]: 沙箱 ID 列表
        """
        with self._lock:
            return list(self._sandboxes.keys())


class Sandbox:
    """
    沙箱实例。
    
    提供隔离的执行环境和权限控制。
    """
    
    def __init__(self, manager: SandboxConfig):
        """
        初始化沙箱。
        
        Args:
            manager: 沙箱配置
        """
        self.manager = manager
        self.state = SandboxState.CREATED
        self._metrics = SandboxMetrics()
        self._start_time: Optional[float] = None
        self._temp_dir: Optional[str] = None
    
    def start(self) -> bool:
        """
        启动沙箱。
        
        Returns:
            bool: 是否成功启动
        """
        if self.state == SandboxState.RUNNING:
            return True
        
        try:
            self._temp_dir = tempfile.mkdtemp(prefix=f"sandbox_{self.manager.sandbox_id}_")
            self._start_time = time.time()
            self.state = SandboxState.RUNNING
            
            return True
        except Exception:
            self.state = SandboxState.ERROR
            
            return False
    
    def stop(self) -> bool:
        """
        停止沙箱。
        
        Returns:
            bool: 是否成功停止
        """
        if self.state == SandboxState.STOPPED:
            return True
        
        try:
            if self._temp_dir and os.path.exists(self._temp_dir):
                shutil.rmtree(self._temp_dir)
            
            self.state = SandboxState.STOPPED
            
            return True
        except Exception:
            self.state = SandboxState.ERROR
            
            return False
    
    def pause(self) -> bool:
        """
        暂停沙箱。
        
        Returns:
            bool: 是否成功暂停
        """
        if self.state != SandboxState.RUNNING:
            return False
        
        self.state = SandboxState.PAUSED
        
        return True
    
    def resume(self) -> bool:
        """
        恢复沙箱。
        
        Returns:
            bool: 是否成功恢复
        """
        if self.state != SandboxState.PAUSED:
            return False
        
        self.state = SandboxState.RUNNING
        
        return True
    
    def check_permission(self, permission: Permission) -> bool:
        """
        检查权限。
        
        Args:
            permission: 要检查的权限
            
        Returns:
            bool: 是否有权限
        """
        return permission in self.manager.allowed_permissions
    
    def check_path_access(self, path: str, write: bool = False) -> bool:
        """
        检查路径访问权限。
        
        Args:
            path: 要访问的路径
            write: 是否为写操作
            
        Returns:
            bool: 是否允许访问
        """
        normalized_path = os.path.normpath(path)
        
        for denied in self.manager.denied_paths:
            if normalized_path.startswith(os.path.normpath(denied)):
                return False
        
        for allowed in self.manager.allowed_paths:
            if normalized_path.startswith(os.path.normpath(allowed)):
                if write and not self.check_permission(Permission.WRITE_FILE):
                    return False
                if not write and not self.check_permission(Permission.READ_FILE):
                    return False
                
                return True
        
        return False
    
    def check_network_access(self, host: str) -> bool:
        """
        检查网络访问权限。
        
        Args:
            host: 目标主机
            
        Returns:
            bool: 是否允许访问
        """
        if not self.check_permission(Permission.NETWORK_OUTBOUND):
            return False
        
        if not self.manager.allowed_network_hosts:
            return True
        
        return host in self.manager.allowed_network_hosts
    
    def get_metrics(self) -> SandboxMetrics:
        """
        获取沙箱指标。
        
        Returns:
            SandboxMetrics: 沙箱指标
        """
        if self._start_time:
            self._metrics.execution_time_seconds = time.time() - self._start_time
        
        return self._metrics
    
    def check_resource_limits(self) -> Dict[ResourceType, bool]:
        """
        检查资源限制。
        
        Returns:
            Dict[ResourceType, bool]: 各资源是否超限
        """
        limits = self.manager.resource_limits
        metrics = self.get_metrics()
        
        return {
            ResourceType.CPU: metrics.cpu_usage_percent <= limits.max_cpu_percent,
            ResourceType.MEMORY: metrics.memory_usage_mb <= limits.max_memory_mb,
            ResourceType.DISK: metrics.disk_usage_mb <= limits.max_disk_mb,
            ResourceType.TIME: metrics.execution_time_seconds <= limits.max_execution_time_seconds,
        }
    
    def get_environment_var(self, name: str) -> Optional[str]:
        """
        获取环境变量。
        
        Args:
            name: 环境变量名
            
        Returns:
            Optional[str]: 环境变量值
        """
        if not self.check_permission(Permission.ACCESS_ENV):
            return None
        
        return self.manager.environment_vars.get(name)
    
    def set_environment_var(self, name: str, value: str) -> bool:
        """
        设置环境变量。
        
        Args:
            name: 环境变量名
            value: 环境变量值
            
        Returns:
            bool: 是否成功设置
        """
        if not self.check_permission(Permission.ACCESS_ENV):
            return False
        
        self.manager.environment_vars[name] = value
        
        return True


# ============================================================
# 测试用例
# ============================================================

class TestSandboxCreation:
    """沙箱创建测试"""
    
    @pytest.fixture
    def manager(self) -> SandboxManager:
        """
        提供沙箱管理器实例。
        
        Returns:
            SandboxManager: 管理器实例
        """
        return SandboxManager()
    
    @pytest.fixture
    def default_config(self) -> SandboxConfig:
        """
        提供默认沙箱配置。
        
        Returns:
            SandboxConfig: 默认配置
        """
        return SandboxConfig(
            sandbox_id="test_sandbox_001",
            allowed_permissions={Permission.READ_FILE, Permission.WRITE_FILE},
            resource_limits=ResourceLimits(),
            allowed_paths=["/tmp"],
            denied_paths=["/etc/passwd"]
        )
    
    def test_create_sandbox_success(self, manager, default_config):
        """
        测试成功创建沙箱。
        
        验证:
            - 沙箱被正确创建
            - 沙箱状态为 CREATED
        """
        sandbox = manager.create_sandbox(default_config)
        
        assert sandbox is not None
        assert sandbox.state == SandboxState.CREATED
        assert sandbox.manager.sandbox_id == "test_sandbox_001"
    
    def test_create_duplicate_sandbox_fails(self, manager, default_config):
        """
        测试创建重复沙箱失败。
        
        验证:
            - 重复创建相同 ID 的沙箱抛出异常
        """
        manager.create_sandbox(default_config)
        
        with pytest.raises(ValueError):
            manager.create_sandbox(default_config)
    
    def test_get_sandbox(self, manager, default_config):
        """
        测试获取沙箱。
        
        验证:
            - 可以通过 ID 获取已创建的沙箱
        """
        created = manager.create_sandbox(default_config)
        retrieved = manager.get_sandbox("test_sandbox_001")
        
        assert retrieved is created
    
    def test_get_nonexistent_sandbox(self, manager):
        """
        测试获取不存在的沙箱。
        
        验证:
            - 获取不存在的沙箱返回 None
        """
        result = manager.get_sandbox("nonexistent")
        
        assert result is None


class TestSandboxLifecycle:
    """沙箱生命周期测试"""
    
    @pytest.fixture
    def sandbox(self) -> Sandbox:
        """
        提供沙箱实例。
        
        Returns:
            Sandbox: 沙箱实例
        """
        manager = SandboxConfig(
            sandbox_id="lifecycle_test",
            allowed_permissions=set(),
            resource_limits=ResourceLimits()
        )
        
        return Sandbox(manager)
    
    def test_start_sandbox(self, sandbox):
        """
        测试启动沙箱。
        
        验证:
            - 沙箱成功启动
            - 状态变为 RUNNING
        """
        result = sandbox.start()
        
        assert result is True
        assert sandbox.state == SandboxState.RUNNING
    
    def test_stop_sandbox(self, sandbox):
        """
        测试停止沙箱。
        
        验证:
            - 沙箱成功停止
            - 状态变为 STOPPED
        """
        sandbox.start()
        result = sandbox.stop()
        
        assert result is True
        assert sandbox.state == SandboxState.STOPPED
    
    def test_pause_sandbox(self, sandbox):
        """
        测试暂停沙箱。
        
        验证:
            - 沙箱成功暂停
            - 状态变为 PAUSED
        """
        sandbox.start()
        result = sandbox.pause()
        
        assert result is True
        assert sandbox.state == SandboxState.PAUSED
    
    def test_resume_sandbox(self, sandbox):
        """
        测试恢复沙箱。
        
        验证:
            - 暂停的沙箱可以恢复
            - 状态变为 RUNNING
        """
        sandbox.start()
        sandbox.pause()
        result = sandbox.resume()
        
        assert result is True
        assert sandbox.state == SandboxState.RUNNING
    
    def test_pause_non_running_sandbox_fails(self, sandbox):
        """
        测试暂停非运行状态的沙箱失败。
        
        验证:
            - 暂停非运行状态的沙箱返回 False
        """
        result = sandbox.pause()
        
        assert result is False


class TestPermissionControl:
    """权限控制测试"""
    
    @pytest.fixture
    def sandbox_with_permissions(self) -> Sandbox:
        """
        提供带权限的沙箱实例。
        
        Returns:
            Sandbox: 沙箱实例
        """
        manager = SandboxConfig(
            sandbox_id="permission_test",
            allowed_permissions={
                Permission.READ_FILE,
                Permission.WRITE_FILE,
                Permission.NETWORK_OUTBOUND
            },
            resource_limits=ResourceLimits()
        )
        
        return Sandbox(manager)
    
    def test_check_allowed_permission(self, sandbox_with_permissions):
        """
        测试检查允许的权限。
        
        验证:
            - 允许的权限检查返回 True
        """
        assert sandbox_with_permissions.check_permission(Permission.READ_FILE) is True
        assert sandbox_with_permissions.check_permission(Permission.WRITE_FILE) is True
        assert sandbox_with_permissions.check_permission(Permission.NETWORK_OUTBOUND) is True
    
    def test_check_denied_permission(self, sandbox_with_permissions):
        """
        测试检查拒绝的权限。
        
        验证:
            - 未授权的权限检查返回 False
        """
        assert sandbox_with_permissions.check_permission(Permission.EXECUTE_PROCESS) is False
        assert sandbox_with_permissions.check_permission(Permission.NETWORK_INBOUND) is False
        assert sandbox_with_permissions.check_permission(Permission.ACCESS_ENV) is False
    
    def test_empty_permissions(self):
        """
        测试空权限集。
        
        验证:
            - 无任何权限的沙箱拒绝所有操作
        """
        manager = SandboxConfig(
            sandbox_id="no_permissions",
            allowed_permissions=set(),
            resource_limits=ResourceLimits()
        )
        sandbox = Sandbox(manager)
        
        for permission in Permission:
            assert sandbox.check_permission(permission) is False


class TestPathAccessControl:
    """路径访问控制测试"""
    
    @pytest.fixture
    def sandbox_with_path_rules(self) -> Sandbox:
        """
        提供带路径规则的沙箱实例。
        
        Returns:
            Sandbox: 沙箱实例
        """
        manager = SandboxConfig(
            sandbox_id="path_test",
            allowed_permissions={
                Permission.READ_FILE,
                Permission.WRITE_FILE
            },
            resource_limits=ResourceLimits(),
            allowed_paths=["/workspace", "/tmp"],
            denied_paths=["/etc/passwd", "/etc/shadow"]
        )
        
        return Sandbox(manager)
    
    def test_allowed_path_read(self, sandbox_with_path_rules):
        """
        测试允许的路径读取。
        
        验证:
            - 允许列表中的路径可以读取
        """
        assert sandbox_with_path_rules.check_path_access("/workspace/file.txt", write=False) is True
        assert sandbox_with_path_rules.check_path_access("/tmp/test.txt", write=False) is True
    
    def test_allowed_path_write(self, sandbox_with_path_rules):
        """
        测试允许的路径写入。
        
        验证:
            - 允许列表中的路径可以写入
        """
        assert sandbox_with_path_rules.check_path_access("/workspace/file.txt", write=True) is True
        assert sandbox_with_path_rules.check_path_access("/tmp/test.txt", write=True) is True
    
    def test_denied_path(self, sandbox_with_path_rules):
        """
        测试拒绝的路径。
        
        验证:
            - 拒绝列表中的路径无法访问
        """
        assert sandbox_with_path_rules.check_path_access("/etc/passwd", write=False) is False
        assert sandbox_with_path_rules.check_path_access("/etc/shadow", write=False) is False
    
    def test_path_not_in_allowed_list(self, sandbox_with_path_rules):
        """
        测试不在允许列表中的路径。
        
        验证:
            - 不在允许列表中的路径无法访问
        """
        assert sandbox_with_path_rules.check_path_access("/home/user/file.txt", write=False) is False
        assert sandbox_with_path_rules.check_path_access("/var/log/app.log", write=False) is False
    
    def test_path_traversal_attempt(self, sandbox_with_path_rules):
        """
        测试路径遍历攻击。
        
        验证:
            - 路径遍历尝试被正确处理
        """
        assert sandbox_with_path_rules.check_path_access("/workspace/../../../etc/passwd", write=False) is False


class TestNetworkAccessControl:
    """网络访问控制测试"""
    
    @pytest.fixture
    def sandbox_with_network_rules(self) -> Sandbox:
        """
        提供带网络规则的沙箱实例。
        
        Returns:
            Sandbox: 沙箱实例
        """
        manager = SandboxConfig(
            sandbox_id="network_test",
            allowed_permissions={Permission.NETWORK_OUTBOUND},
            resource_limits=ResourceLimits(),
            allowed_network_hosts=["api.example.com", "cdn.example.com"]
        )
        
        return Sandbox(manager)
    
    def test_allowed_network_host(self, sandbox_with_network_rules):
        """
        测试允许的网络主机。
        
        验证:
            - 允许列表中的主机可以访问
        """
        assert sandbox_with_network_rules.check_network_access("api.example.com") is True
        assert sandbox_with_network_rules.check_network_access("cdn.example.com") is True
    
    def test_denied_network_host(self, sandbox_with_network_rules):
        """
        测试拒绝的网络主机。
        
        验证:
            - 不在允许列表中的主机无法访问
        """
        assert sandbox_with_network_rules.check_network_access("malicious.com") is False
        assert sandbox_with_network_rules.check_network_access("internal.local") is False
    
    def test_network_without_permission(self):
        """
        测试无网络权限。
        
        验证:
            - 无网络权限时所有网络访问被拒绝
        """
        manager = SandboxConfig(
            sandbox_id="no_network",
            allowed_permissions=set(),
            resource_limits=ResourceLimits(),
            allowed_network_hosts=["api.example.com"]
        )
        sandbox = Sandbox(manager)
        
        assert sandbox.check_network_access("api.example.com") is False
    
    def test_wildcard_network_permission(self):
        """
        测试通配符网络权限。
        
        验证:
            - 空允许列表表示允许所有主机
        """
        manager = SandboxConfig(
            sandbox_id="wildcard_network",
            allowed_permissions={Permission.NETWORK_OUTBOUND},
            resource_limits=ResourceLimits(),
            allowed_network_hosts=[]
        )
        sandbox = Sandbox(manager)
        
        assert sandbox.check_network_access("any.host.com") is True


class TestResourceLimits:
    """资源限制测试"""
    
    @pytest.fixture
    def sandbox_with_limits(self) -> Sandbox:
        """
        提供带资源限制的沙箱实例。
        
        Returns:
            Sandbox: 沙箱实例
        """
        limits = ResourceLimits(
            max_cpu_percent=50.0,
            max_memory_mb=256,
            max_disk_mb=512,
            max_execution_time_seconds=60
        )
        manager = SandboxConfig(
            sandbox_id="resource_test",
            allowed_permissions=set(),
            resource_limits=limits
        )
        
        return Sandbox(manager)
    
    def test_resource_limits_configuration(self, sandbox_with_limits):
        """
        测试资源限制配置。
        
        验证:
            - 资源限制被正确配置
        """
        limits = sandbox_with_limits.manager.resource_limits
        
        assert limits.max_cpu_percent == 50.0
        assert limits.max_memory_mb == 256
        assert limits.max_disk_mb == 512
        assert limits.max_execution_time_seconds == 60
    
    def test_check_resource_limits(self, sandbox_with_limits):
        """
        测试检查资源限制。
        
        验证:
            - 资源限制检查返回正确结果
        """
        result = sandbox_with_limits.check_resource_limits()
        
        assert ResourceType.CPU in result
        assert ResourceType.MEMORY in result
        assert ResourceType.DISK in result
        assert ResourceType.TIME in result
    
    def test_execution_time_limit(self, sandbox_with_limits):
        """
        测试执行时间限制。
        
        验证:
            - 执行时间超过限制时被检测
        """
        sandbox_with_limits._start_time = time.time() - 100
        result = sandbox_with_limits.check_resource_limits()
        
        assert result[ResourceType.TIME] is False


class TestEnvironmentVariables:
    """环境变量测试"""
    
    @pytest.fixture
    def sandbox_with_env(self) -> Sandbox:
        """
        提供带环境变量的沙箱实例。
        
        Returns:
            Sandbox: 沙箱实例
        """
        manager = SandboxConfig(
            sandbox_id="env_test",
            allowed_permissions={Permission.ACCESS_ENV},
            resource_limits=ResourceLimits(),
            environment_vars={"API_KEY": "test_key", "DEBUG": "true"}
        )
        
        return Sandbox(manager)
    
    def test_get_environment_var(self, sandbox_with_env):
        """
        测试获取环境变量。
        
        验证:
            - 可以获取已设置的环境变量
        """
        assert sandbox_with_env.get_environment_var("API_KEY") == "test_key"
        assert sandbox_with_env.get_environment_var("DEBUG") == "true"
    
    def test_get_nonexistent_env_var(self, sandbox_with_env):
        """
        测试获取不存在的环境变量。
        
        验证:
            - 获取不存在的环境变量返回 None
        """
        assert sandbox_with_env.get_environment_var("NONEXISTENT") is None
    
    def test_set_environment_var(self, sandbox_with_env):
        """
        测试设置环境变量。
        
        验证:
            - 可以设置新的环境变量
        """
        result = sandbox_with_env.set_environment_var("NEW_VAR", "new_value")
        
        assert result is True
        assert sandbox_with_env.get_environment_var("NEW_VAR") == "new_value"
    
    def test_env_without_permission(self):
        """
        测试无环境变量权限。
        
        验证:
            - 无权限时无法访问环境变量
        """
        manager = SandboxConfig(
            sandbox_id="no_env",
            allowed_permissions=set(),
            resource_limits=ResourceLimits(),
            environment_vars={"SECRET": "value"}
        )
        sandbox = Sandbox(manager)
        
        assert sandbox.get_environment_var("SECRET") is None
        assert sandbox.set_environment_var("NEW", "value") is False


class TestSandboxMetrics:
    """沙箱指标测试"""
    
    @pytest.fixture
    def sandbox(self) -> Sandbox:
        """
        提供沙箱实例。
        
        Returns:
            Sandbox: 沙箱实例
        """
        manager = SandboxConfig(
            sandbox_id="metrics_test",
            allowed_permissions=set(),
            resource_limits=ResourceLimits()
        )
        
        return Sandbox(manager)
    
    def test_get_initial_metrics(self, sandbox):
        """
        测试获取初始指标。
        
        验证:
            - 初始指标值为零
        """
        metrics = sandbox.get_metrics()
        
        assert metrics.cpu_usage_percent == 0.0
        assert metrics.memory_usage_mb == 0.0
        assert metrics.disk_usage_mb == 0.0
        assert metrics.execution_time_seconds == 0.0
    
    def test_execution_time_tracking(self, sandbox):
        """
        测试执行时间跟踪。
        
        验证:
            - 执行时间被正确跟踪
        """
        sandbox.start()
        time.sleep(0.1)
        
        metrics = sandbox.get_metrics()
        
        assert metrics.execution_time_seconds >= 0.1


class TestSandboxManagerOperations:
    """沙箱管理器操作测试"""
    
    @pytest.fixture
    def manager(self) -> SandboxManager:
        """
        提供沙箱管理器实例。
        
        Returns:
            SandboxManager: 管理器实例
        """
        return SandboxManager()
    
    def test_list_sandboxes(self, manager):
        """
        测试列出沙箱。
        
        验证:
            - 可以列出所有沙箱 ID
        """
        config1 = SandboxConfig(sandbox_id="sandbox_1", allowed_permissions=set())
        config2 = SandboxConfig(sandbox_id="sandbox_2", allowed_permissions=set())
        
        manager.create_sandbox(config1)
        manager.create_sandbox(config2)
        
        sandbox_ids = manager.list_sandboxes()
        
        assert len(sandbox_ids) == 2
        assert "sandbox_1" in sandbox_ids
        assert "sandbox_2" in sandbox_ids
    
    def test_destroy_sandbox(self, manager):
        """
        测试销毁沙箱。
        
        验证:
            - 沙箱被正确销毁
        """
        manager = SandboxConfig(sandbox_id="to_destroy", allowed_permissions=set())
        manager.create_sandbox(manager)
        
        result = manager.destroy_sandbox("to_destroy")
        
        assert result is True
        assert manager.get_sandbox("to_destroy") is None
    
    def test_destroy_nonexistent_sandbox(self, manager):
        """
        测试销毁不存在的沙箱。
        
        验证:
            - 销毁不存在的沙箱返回 False
        """
        result = manager.destroy_sandbox("nonexistent")
        
        assert result is False


# ============================================================
# 运行测试
# ============================================================

if __name__ == "__main__":
    pytest.main([__file__, "-v", "--tb=short", "-m", "security"])
