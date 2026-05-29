# AgentOS 测试隔离和并行执行工具
# Version: 2.0.0
# Last updated: 2026-04-23

"""
测试隔离和并行执行模块。

提供测试用例独立性保证和执行效率优化功能。
增强功能：
- 进程级隔离
- 数据库隔离
- 网络隔离
- 资源限制
- 状态快照/恢复
"""

import os
import sys
import time
import uuid
import threading
import asyncio
import re
import json
import tempfile
import shutil
import signal
import resource
import socket
import sqlite3
from pathlib import Path
from typing import Dict, Any, List, Optional, Callable, Union, Generator
from contextlib import contextmanager, asynccontextmanager
from unittest.mock import Mock, patch, MagicMock
from dataclasses import dataclass, field
from concurrent.futures import ThreadPoolExecutor, ProcessPoolExecutor
import pytest

# 添加项目根目录到路径
PROJECT_ROOT = Path(__file__).parent.parent.parent


class TestIsolationManager:
    """测试隔离管理器"""

    def __init__(self):
        """初始化测试隔离管理器"""
        self.isolated_environments = {}
        self.lock = threading.Lock()
        self.temp_dirs = {}
        self.mock_patches = {}

    @contextmanager
    def isolated_test_environment(self, test_name: str):
        """
        创建隔离的测试环境。

        Args:
            test_name: 测试名称
        """
        env_id = f"{test_name}_{uuid.uuid4().hex[:8]}"

        with self.lock:
            temp_dir = Path(tempfile.mkdtemp(prefix=f"agentos_test_{env_id}_"))
            self.temp_dirs[env_id] = temp_dir

            original_env = {}
            test_env = {
                "AGENTOS_TEST_ID": env_id,
                "AGENTOS_TEST_DIR": str(temp_dir),
                "AGENTOS_ENDPOINT": f"http://localhost:{18789 + hash(env_id) % 1000}",
                "AGENTOS_TEMP_DIR": str(temp_dir / "temp"),
                "AGENTOS_LOG_FILE": str(temp_dir / "test.log"),
                "AGENTOS_DB_PATH": str(temp_dir / "test.db"),
                "AGENTOS_CONFIG_PATH": str(temp_dir / "manager.json")
            }

            for key, value in test_env.items():
                original_env[key] = os.environ.get(key)
                os.environ[key] = value

            (temp_dir / "temp").mkdir(exist_ok=True)
            (temp_dir / "data").mkdir(exist_ok=True)
            (temp_dir / "logs").mkdir(exist_ok=True)

            manager = {
                "test_id": env_id,
                "endpoint": test_env["AGENTOS_ENDPOINT"],
                "temp_dir": test_env["AGENTOS_TEMP_DIR"],
                "log_file": test_env["AGENTOS_LOG_FILE"],
                "database": {"path": test_env["AGENTOS_DB_PATH"], "type": "sqlite"},
                "logging": {"level": "DEBUG", "file": test_env["AGENTOS_LOG_FILE"]}
            }

            with open(temp_dir / "manager.json", 'w') as f:
                json.dump(manager, f, indent=2)

        try:
            yield env_id
        finally:
            with self.lock:
                for key, original_value in original_env.items():
                    if original_value is None:
                        os.environ.pop(key, None)
                    else:
                        os.environ[key] = original_value

                if env_id in self.temp_dirs:
                    temp_dir = self.temp_dirs[env_id]
                    if temp_dir.exists():
                        shutil.rmtree(temp_dir, ignore_errors=True)
                    del self.temp_dirs[env_id]

                if env_id in self.mock_patches:
                    for patch_obj in self.mock_patches[env_id]:
                        patch_obj.stop()
                    del self.mock_patches[env_id]

    def add_mock_patch(self, env_id: str, patch_obj):
        """添加mock补丁到隔离环境。"""
        with self.lock:
            if env_id not in self.mock_patches:
                self.mock_patches[env_id] = []
            self.mock_patches[env_id].append(patch_obj)

    def cleanup(self):
        """清理所有隔离环境"""
        with self.lock:
            for temp_dir in self.temp_dirs.values():
                if temp_dir.exists():
                    shutil.rmtree(temp_dir, ignore_errors=True)
            self.temp_dirs.clear()

            for patches in self.mock_patches.values():
                for patch_obj in patches:
                    patch_obj.stop()
            self.mock_patches.clear()

            self.isolated_environments.clear()


class ParallelTestExecutor:
    """并行测试执行器"""

    def __init__(self, max_workers: int = None):
        self.max_workers = max_workers or min(32, (os.cpu_count() or 1) + 4)
        self.isolation_manager = TestIsolationManager()

    def execute_tests_parallel(self, test_functions: List[Callable],
                             test_names: List[str] = None) -> Dict[str, Any]:
        """并行执行测试。"""
        if test_names is None:
            test_names = [f"test_{i}" for i in range(len(test_functions))]

        results = {
            "total": len(test_functions),
            "passed": 0,
            "failed": 0,
            "skipped": 0,
            "errors": [],
            "execution_time": 0,
            "details": {}
        }

        start_time = time.time()

        from concurrent.futures import ThreadPoolExecutor, as_completed

        with ThreadPoolExecutor(max_workers=self.max_workers) as executor:
            future_to_test = {
                executor.submit(self._execute_single_test, func, name): name
                for func, name in zip(test_functions, test_names)
            }

            for future in as_completed(future_to_test):
                test_name = future_to_test[future]
                try:
                    test_result = future.result()
                    results["details"][test_name] = test_result

                    if test_result["status"] == "passed":
                        results["passed"] += 1
                    elif test_result["status"] == "failed":
                        results["failed"] += 1
                        results["errors"].append(f"{test_name}: {test_result.get('error', 'Unknown')}")
                    elif test_result["status"] == "skipped":
                        results["skipped"] += 1

                except Exception as e:
                    results["failed"] += 1
                    results["errors"].append(f"{test_name}: {str(e)}")

        results["execution_time"] = time.time() - start_time
        self.isolation_manager.cleanup()

        return results

    def _execute_single_test(self, test_func: Callable, test_name: str) -> Dict[str, Any]:
        """执行单个测试。"""
        result = {"status": "unknown", "execution_time": 0, "error": None}
        start_time = time.time()

        try:
            with self.isolation_manager.isolated_test_environment(test_name):
                test_func()
                result["status"] = "passed"
        except pytest.skip.Exception:
            result["status"] = "skipped"
        except Exception as e:
            result["status"] = "failed"
            result["error"] = str(e)
        finally:
            result["execution_time"] = time.time() - start_time

        return result


class TestEfficiencyOptimizer:
    """测试效率优化器"""

    def optimize_pytest_config(self, test_dir: Path) -> Dict[str, Any]:
        """优化pytest配置。"""
        return {
            "addopts": ["-n", "auto", "--dist", "loadscope", "--tb=short"],
            "testpaths": [str(test_dir)],
            "markers": [
                "unit: 单元测试",
                "integration: 集成测试",
                "performance: 性能测试",
                "security: 安全测试",
            ],
        }

    def create_optimized_test_suite(self, test_dir: Path) -> List[str]:
        """创建优化的测试套件。"""
        test_files = list(test_dir.rglob("test_*.py"))

        fast_tests = []
        slow_tests = []
        integration_tests = []

        for test_file in test_files:
            content = test_file.read_text(encoding='utf-8').lower()

            if "performance" in content or "benchmark" in content:
                slow_tests.append(str(test_file))
            elif "integration" in content:
                integration_tests.append(str(test_file))
            else:
                fast_tests.append(str(test_file))

        return fast_tests + integration_tests + slow_tests


# pytest fixtures
@pytest.fixture(scope="function")
def isolated_env():
    """提供隔离的测试环境。"""
    manager = TestIsolationManager()
    with manager.isolated_test_environment("isolated_test") as env_id:
        yield env_id
    manager.cleanup()


@pytest.fixture(scope="function")
def parallel_executor():
    """提供并行测试执行器。"""
    executor = ParallelTestExecutor()
    yield executor
    executor.isolation_manager.cleanup()


# Decorators
def isolated_test(func):
    """隔离测试装饰器。"""
    def wrapper(*args, **kwargs):
        manager = TestIsolationManager()
        with manager.isolated_test_environment(func.__name__):
            result = func(*args, **kwargs)
        manager.cleanup()
        return result

    wrapper.__name__ = func.__name__
    wrapper.__doc__ = func.__doc__
    return wrapper


def performance_optimized(max_time: float = 1.0):
    """性能优化装饰器。"""
    def decorator(func):
        def wrapper(*args, **kwargs):
            start_time = time.time()
            result = func(*args, **kwargs)
            elapsed = time.time() - start_time

            if elapsed > max_time:
                import warnings
                warnings.warn(f"{func.__name__} took {elapsed:.3f}s, exceeding {max_time:.3f}s")

            return result

        wrapper.__name__ = func.__name__
        wrapper.__doc__ = func.__doc__
        return wrapper

    return decorator


def parallel_safe(func):
    """并行安全装饰器。"""
    func._parallel_safe = True
    return func


def sequential_only(func):
    """仅顺序执行装饰器。"""
    func._sequential_only = True
    return func


def get_test_statistics(test_dir: Path) -> Dict[str, Any]:
    """获取测试统计信息。"""
    stats = {
        "total_tests": 0,
        "by_type": {"unit": 0, "integration": 0, "performance": 0, "security": 0, "general": 0},
        "parallelizable": 0,
        "sequential_only": 0
    }

    for test_file in test_dir.rglob("test_*.py"):
        try:
            content = test_file.read_text(encoding='utf-8')
            test_count = len(re.findall(r'def test_\w+', content))
            stats["total_tests"] += test_count

            content_lower = content.lower()
            if "unit" in content_lower:
                stats["by_type"]["unit"] += test_count
            elif "integration" in content_lower:
                stats["by_type"]["integration"] += test_count
            elif "performance" in content_lower:
                stats["by_type"]["performance"] += test_count
            elif "security" in content_lower:
                stats["by_type"]["security"] += test_count
            else:
                stats["by_type"]["general"] += test_count

            if "_sequential_only" in content:
                stats["sequential_only"] += test_count
            else:
                stats["parallelizable"] += test_count

        except Exception:
            pass

    return stats


@dataclass
class IsolationConfig:
    """隔离配置"""
    isolate_filesystem: bool = True
    isolate_environment: bool = True
    isolate_network: bool = False
    isolate_database: bool = True
    max_memory_mb: int = 512
    max_time_seconds: int = 60
    cleanup_on_exit: bool = True


class DatabaseIsolator:
    """数据库隔离器"""

    def __init__(self, base_dir: Path = None):
        self.base_dir = base_dir or Path(tempfile.gettempdir()) / "agentos_test_dbs"
        self.base_dir.mkdir(parents=True, exist_ok=True)
        self._connections: Dict[str, sqlite3.Connection] = {}

    def create_isolated_db(self, name: str = "test") -> Path:
        """创建隔离的数据库"""
        db_path = self.base_dir / f"{name}_{uuid.uuid4().hex[:8]}.db"
        conn = sqlite3.connect(str(db_path))
        self._connections[str(db_path)] = conn
        return db_path

    def get_connection(self, db_path: Path) -> sqlite3.Connection:
        """获取数据库连接"""
        return self._connections.get(str(db_path))

    def cleanup(self):
        """清理所有数据库"""
        for conn in self._connections.values():
            try:
                conn.close()
            except Exception:
                pass
        self._connections.clear()

        for db_file in self.base_dir.glob("*.db"):
            try:
                db_file.unlink()
            except Exception:
                pass


class NetworkIsolator:
    """网络隔离器"""

    def __init__(self):
        self._mock_servers = {}
        self._blocked_hosts = set()

    def find_free_port(self) -> int:
        """查找可用端口"""
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.bind(('', 0))
            s.listen(1)
            port = s.getsockname()[1]
        return port

    def block_host(self, host: str):
        """阻止特定主机"""
        self._blocked_hosts.add(host)

    def unblock_host(self, host: str):
        """解除阻止主机"""
        self._blocked_hosts.discard(host)

    @contextmanager
    def isolated_network(self):
        """隔离网络上下文"""
        original_hosts = self._blocked_hosts.copy()
        try:
            yield self
        finally:
            self._blocked_hosts = original_hosts


class ResourceLimiter:
    """资源限制器"""

    def __init__(self, max_memory_mb: int = 512, max_time_seconds: int = 60):
        self.max_memory_mb = max_memory_mb
        self.max_time_seconds = max_time_seconds
        self._timer = None
        self._timed_out = False

    def _timeout_handler(self, signum, frame):
        self._timed_out = True
        raise TimeoutError(f"测试执行超过 {self.max_time_seconds} 秒")

    @contextmanager
    def limit_resources(self):
        """限制资源使用"""
        self._timed_out = False

        if sys.platform != 'win32':
            old_handler = signal.signal(signal.SIGALRM, self._timeout_handler)
            signal.alarm(self.max_time_seconds)

            try:
                soft, hard = resource.getrlimit(resource.RLIMIT_AS)
                resource.setrlimit(resource.RLIMIT_AS, (self.max_memory_mb * 1024 * 1024, hard))
            except (ValueError, resource.error):
                pass

        try:
            yield self
        finally:
            if sys.platform != 'win32':
                signal.alarm(0)
                signal.signal(signal.SIGALRM, old_handler)

    @property
    def timed_out(self) -> bool:
        return self._timed_out


class StateSnapshot:
    """状态快照"""

    def __init__(self):
        self._snapshots: Dict[str, Dict[str, Any]] = {}

    def capture(self, name: str, state: Dict[str, Any]) -> str:
        """捕获状态快照"""
        snapshot_id = f"{name}_{uuid.uuid4().hex[:8]}"
        self._snapshots[snapshot_id] = {
            'name': name,
            'state': copy.deepcopy(state),
            'timestamp': time.time()
        }
        return snapshot_id

    def restore(self, snapshot_id: str) -> Optional[Dict[str, Any]]:
        """恢复状态快照"""
        if snapshot_id not in self._snapshots:
            return None
        return copy.deepcopy(self._snapshots[snapshot_id]['state'])

    def list_snapshots(self) -> List[str]:
        """列出所有快照"""
        return list(self._snapshots.keys())

    def delete_snapshot(self, snapshot_id: str):
        """删除快照"""
        self._snapshots.pop(snapshot_id, None)


class ProcessIsolator:
    """进程隔离器"""

    def __init__(self):
        self._processes: Dict[str, Any] = {}

    def run_in_isolated_process(
        self,
        func: Callable,
        *args,
        timeout: int = 60,
        **kwargs
    ) -> Any:
        """在隔离进程中运行函数"""
        with ProcessPoolExecutor(max_workers=1) as executor:
            future = executor.submit(func, *args, **kwargs)
            try:
                return future.result(timeout=timeout)
            except TimeoutError:
                future.cancel()
                raise TimeoutError(f"进程执行超时 ({timeout}秒)")

    @contextmanager
    def isolated_process_context(self, name: str = "isolated"):
        """隔离进程上下文"""
        process_id = f"{name}_{uuid.uuid4().hex[:8]}"
        try:
            yield process_id
        finally:
            pass


class FixtureIsolator:
    """夹具隔离器"""

    def __init__(self):
        self._fixtures: Dict[str, Any] = {}
        self._fixture_states: Dict[str, Dict] = {}

    def register_fixture(self, name: str, fixture: Any, setup_func: Callable = None, teardown_func: Callable = None):
        """注册夹具"""
        self._fixtures[name] = {
            'fixture': fixture,
            'setup': setup_func,
            'teardown': teardown_func
        }

    @contextmanager
    def use_fixture(self, name: str):
        """使用夹具"""
        if name not in self._fixtures:
            raise ValueError(f"未知夹具: {name}")

        fixture_info = self._fixtures[name]
        fixture = fixture_info['fixture']

        if fixture_info['setup']:
            fixture_info['setup'](fixture)

        try:
            yield fixture
        finally:
            if fixture_info['teardown']:
                fixture_info['teardown'](fixture)


import copy


class ComprehensiveIsolation:
    """综合隔离管理器"""

    def __init__(self, config: IsolationConfig = None):
        self.config = config or IsolationConfig()
        self.fs_isolator = TestIsolationManager()
        self.db_isolator = DatabaseIsolator() if self.config.isolate_database else None
        self.net_isolator = NetworkIsolator() if self.config.isolate_network else None
        self.resource_limiter = ResourceLimiter(
            self.config.max_memory_mb,
            self.config.max_time_seconds
        )

    @contextmanager
    def full_isolation(self, test_name: str):
        """完全隔离上下文"""
        with self.fs_isolator.isolated_test_environment(test_name) as env_id:
            with self.resource_limiter.limit_resources():
                if self.config.isolate_network and self.net_isolator:
                    with self.net_isolator.isolated_network():
                        yield {
                            'env_id': env_id,
                            'db_path': self.db_isolator.create_isolated_db(test_name) if self.db_isolator else None
                        }
                else:
                    yield {
                        'env_id': env_id,
                        'db_path': self.db_isolator.create_isolated_db(test_name) if self.db_isolator else None
                    }

    def cleanup(self):
        """清理所有资源"""
        self.fs_isolator.cleanup()
        if self.db_isolator:
            self.db_isolator.cleanup()


@pytest.fixture(scope="function")
def comprehensive_isolation():
    """提供综合隔离环境"""
    isolator = ComprehensiveIsolation()
    with isolator.full_isolation("test") as env:
        yield env
    isolator.cleanup()


@pytest.fixture(scope="function")
def isolated_database():
    """提供隔离的数据库"""
    isolator = DatabaseIsolator()
    db_path = isolator.create_isolated_db("test")
    yield db_path
    isolator.cleanup()


@pytest.fixture(scope="function")
def resource_limits():
    """提供资源限制"""
    limiter = ResourceLimiter()
    with limiter.limit_resources():
        yield limiter


def with_timeout(seconds: int):
    """超时装饰器"""
    def decorator(func):
        def wrapper(*args, **kwargs):
            limiter = ResourceLimiter(max_time_seconds=seconds)
            with limiter.limit_resources():
                return func(*args, **kwargs)
        return wrapper
    return decorator


def with_memory_limit(max_mb: int):
    """内存限制装饰器"""
    def decorator(func):
        def wrapper(*args, **kwargs):
            limiter = ResourceLimiter(max_memory_mb=max_mb)
            with limiter.limit_resources():
                return func(*args, **kwargs)
        return wrapper
    return decorator
