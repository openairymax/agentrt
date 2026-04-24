"""
AgentOS 测试实用工具函数库

提供测试中常用的辅助函数，包括：
- 测试数据生成
- Mock 对象创建
- 断言辅助
- 性能测量
- 测试隔离
- 环境验证
- 报告生成

Version: 2.0.0
Last updated: 2026-04-23
"""

import os
import sys
import json
import time
import tempfile
import shutil
import hashlib
import threading
import traceback
import platform
import subprocess
import tracemalloc
import asyncio
from pathlib import Path
from typing import Any, Dict, List, Optional, Callable, Generator, Union, Tuple
from unittest.mock import MagicMock, patch, AsyncMock
from contextlib import contextmanager, asynccontextmanager
from dataclasses import dataclass, field
from datetime import datetime, timedelta
from concurrent.futures import ThreadPoolExecutor, ProcessPoolExecutor
from functools import wraps, lru_cache


class TestDataGenerator:
    """测试数据生成器"""

    @staticmethod
    def create_temp_file(content: str, suffix: str = '.json') -> Path:
        """创建临时测试文件"""
        with tempfile.NamedTemporaryFile(mode='w', suffix=suffix, delete=False) as f:
            f.write(content)
            return Path(f.name)

    @staticmethod
    def create_temp_dir(prefix: str = 'test_') -> Path:
        """创建临时测试目录"""
        return Path(tempfile.mkdtemp(prefix=prefix))

    @staticmethod
    def generate_task_data(task_id: str = "test-task-001") -> Dict:
        """生成测试任务数据"""
        return {
            "id": task_id,
            "name": f"Test Task {task_id}",
            "description": "Automated test task",
            "priority": "medium",
            "status": "pending",
            "created_at": "2026-04-22T10:00:00Z",
            "updated_at": "2026-04-22T10:00:00Z",
            "metadata": {
                "source": "test",
                "version": "1.0"
            }
        }

    @staticmethod
    def generate_memory_data(memory_id: str = "test-mem-001") -> Dict:
        """生成测试记忆数据"""
        return {
            "id": memory_id,
            "type": "episodic",
            "content": {
                "text": "Test memory content",
                "embeddings": [0.1, 0.2, 0.3]
            },
            "metadata": {
                "source": "test",
                "timestamp": "2026-04-22T10:00:00Z"
            }
        }

    @staticmethod
    def generate_agent_data(agent_id: str = "test-agent-001") -> Dict:
        """生成测试 Agent 数据"""
        return {
            "id": agent_id,
            "name": f"Test Agent {agent_id}",
            "type": "general",
            "status": "active",
            "skills": ["skill1", "skill2"],
            "config": {
                "max_tokens": 4096,
                "temperature": 0.7
            }
        }


class MockFactory:
    """Mock 对象工厂"""

    @staticmethod
    def create_mock_response(status_code: int = 200, json_data: Dict = None) -> MagicMock:
        """创建模拟 HTTP 响应"""
        mock_resp = MagicMock()
        mock_resp.status_code = status_code
        mock_resp.json.return_value = json_data or {}
        mock_resp.text = json.dumps(json_data or {})
        return mock_resp

    @staticmethod
    def create_mock_config(**kwargs) -> MagicMock:
        """创建模拟配置对象"""
        mock_config = MagicMock()
        for key, value in kwargs.items():
            setattr(mock_config, key, value)
        return mock_config

    @staticmethod
    def create_mock_logger() -> MagicMock:
        """创建模拟日志器"""
        mock_logger = MagicMock()
        mock_logger.debug = MagicMock()
        mock_logger.info = MagicMock()
        mock_logger.warning = MagicMock()
        mock_logger.error = MagicMock()
        return mock_logger


class AssertHelpers:
    """断言辅助类"""

    @staticmethod
    def assert_json_equal(actual: Any, expected: Any, path: str = "") -> None:
        """递归比较 JSON 对象"""
        if isinstance(actual, dict) and isinstance(expected, dict):
            assert set(actual.keys()) == set(expected.keys()), f"Keys mismatch at {path}"
            for key in actual:
                AssertHelpers.assert_json_equal(actual[key], expected[key], f"{path}.{key}")
        elif isinstance(actual, list) and isinstance(expected, list):
            assert len(actual) == len(expected), f"List length mismatch at {path}"
            for i, (a, e) in enumerate(zip(actual, expected)):
                AssertHelpers.assert_json_equal(a, e, f"{path}[{i}]")
        else:
            assert actual == expected, f"Value mismatch at {path}: {actual} != {expected}"

    @staticmethod
    def assert_response_ok(response: Dict, expected_keys: List[str] = None) -> None:
        """断言响应成功"""
        assert response.get("status") == "ok", f"Expected status 'ok', got {response.get('status')}"
        if expected_keys:
            for key in expected_keys:
                assert key in response, f"Missing key: {key}"


class PerformanceTester:
    """性能测试辅助"""

    def __init__(self, name: str = "test"):
        self.name = name
        self.start_time = None
        self.end_time = None

    def __enter__(self):
        self.start_time = time.perf_counter()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.end_time = time.perf_counter()

    @property
    def elapsed_ms(self) -> float:
        """获取执行时间（毫秒）"""
        if self.start_time and self.end_time:
            return (self.end_time - self.start_time) * 1000
        return 0.0

    @property
    def elapsed_s(self) -> float:
        """获取执行时间（秒）"""
        return self.elapsed_ms / 1000.0


def load_fixture(filename: str, base_dir: str = None) -> Dict:
    """加载测试夹具数据"""
    if base_dir is None:
        base_dir = Path(__file__).parent.parent / "fixtures" / "data"

    filepath = Path(base_dir) / filename
    with open(filepath, 'r', encoding='utf-8') as f:
        return json.load(f)


class TestDataBuilder:
    """测试数据构建器 - 流式API"""

    def __init__(self):
        self._data = {}

    def with_id(self, id: str) -> 'TestDataBuilder':
        self._data['id'] = id
        return self

    def with_name(self, name: str) -> 'TestDataBuilder':
        self._data['name'] = name
        return self

    def with_status(self, status: str) -> 'TestDataBuilder':
        self._data['status'] = status
        return self

    def with_metadata(self, **kwargs) -> 'TestDataBuilder':
        if 'metadata' not in self._data:
            self._data['metadata'] = {}
        self._data['metadata'].update(kwargs)
        return self

    def with_timestamp(self, key: str = 'created_at') -> 'TestDataBuilder':
        self._data[key] = datetime.now().isoformat()
        return self

    def build(self) -> Dict:
        return self._data.copy()


class MemoryProfiler:
    """内存分析器"""

    def __init__(self, threshold_mb: float = 100.0):
        self.threshold_mb = threshold_mb
        self._snapshots = []
        self._peak = 0

    def __enter__(self):
        tracemalloc.start()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        tracemalloc.stop()

    def snapshot(self, label: str = "") -> Dict:
        """获取内存快照"""
        snapshot = tracemalloc.take_snapshot()
        stats = snapshot.statistics('lineno')
        total_mb = sum(stat.size for stat in stats) / 1024 / 1024

        if total_mb > self._peak:
            self._peak = total_mb

        result = {
            'label': label,
            'total_mb': total_mb,
            'peak_mb': self._peak,
            'top_allocations': [
                {'file': str(stat.traceback[0].filename), 'line': stat.traceback[0].lineno, 'size_mb': stat.size / 1024 / 1024}
                for stat in stats[:5]
            ]
        }
        self._snapshots.append(result)
        return result

    def assert_below_threshold(self) -> None:
        """断言内存使用低于阈值"""
        assert self._peak < self.threshold_mb, f"内存使用 {self._peak:.2f}MB 超过阈值 {self.threshold_mb}MB"

    @property
    def peak_mb(self) -> float:
        return self._peak


class TestIsolation:
    """测试隔离工具"""

    _lock = threading.Lock()
    _isolated_dirs = {}

    @classmethod
    @contextmanager
    def isolated_directory(cls, prefix: str = "isolated_test_") -> Generator[Path, None, None]:
        """创建隔离的临时目录"""
        temp_dir = Path(tempfile.mkdtemp(prefix=prefix))
        thread_id = threading.get_ident()

        with cls._lock:
            cls._isolated_dirs[thread_id] = temp_dir

        try:
            yield temp_dir
        finally:
            with cls._lock:
                if thread_id in cls._isolated_dirs:
                    del cls._isolated_dirs[thread_id]
            shutil.rmtree(temp_dir, ignore_errors=True)

    @classmethod
    @contextmanager
    def isolated_environment(cls, env_vars: Dict[str, str] = None) -> Generator[None, None, None]:
        """创建隔离的环境变量"""
        original_env = os.environ.copy()

        if env_vars:
            os.environ.update(env_vars)

        try:
            yield
        finally:
            os.environ.clear()
            os.environ.update(original_env)

    @classmethod
    @contextmanager
    def isolated_path(cls, extra_paths: List[str] = None) -> Generator[None, None, None]:
        """创建隔离的Python路径"""
        original_path = sys.path.copy()

        if extra_paths:
            sys.path = extra_paths + sys.path

        try:
            yield
        finally:
            sys.path = original_path


class RetryHelper:
    """重试辅助工具"""

    def __init__(self, max_retries: int = 3, delay: float = 1.0, backoff: float = 2.0):
        self.max_retries = max_retries
        self.delay = delay
        self.backoff = backoff

    def execute(self, func: Callable, *args, **kwargs) -> Any:
        """执行带重试的函数"""
        last_exception = None
        current_delay = self.delay

        for attempt in range(self.max_retries):
            try:
                return func(*args, **kwargs)
            except Exception as e:
                last_exception = e
                if attempt < self.max_retries - 1:
                    time.sleep(current_delay)
                    current_delay *= self.backoff

        raise last_exception


class AsyncTestHelper:
    """异步测试辅助工具"""

    @staticmethod
    @asynccontextmanager
    async def async_timeout(seconds: float):
        """异步超时上下文"""
        try:
            async with asyncio.timeout(seconds):
                yield
        except asyncio.TimeoutError:
            raise TimeoutError(f"操作超时 ({seconds}秒)")

    @staticmethod
    async def wait_for_condition(
        condition: Callable[[], bool],
        timeout: float = 10.0,
        interval: float = 0.1
    ) -> bool:
        """等待条件满足"""
        start_time = time.time()
        while time.time() - start_time < timeout:
            if condition():
                return True
            await asyncio.sleep(interval)
        return False

    @staticmethod
    def run_async(coro):
        """运行异步协程"""
        try:
            loop = asyncio.get_running_loop()
        except RuntimeError:
            loop = None

        if loop and loop.is_running():
            import concurrent.futures
            with concurrent.futures.ThreadPoolExecutor() as pool:
                future = pool.submit(asyncio.run, coro)
                return future.result()
        else:
            return asyncio.run(coro)


class TestReporter:
    """测试报告生成器"""

    def __init__(self, name: str = "Test Report"):
        self.name = name
        self._results = []
        self._start_time = None

    def start(self):
        """开始记录"""
        self._start_time = time.time()
        self._results = []

    def record(self, test_name: str, passed: bool, duration: float = 0, message: str = ""):
        """记录测试结果"""
        self._results.append({
            'name': test_name,
            'passed': passed,
            'duration': duration,
            'message': message,
            'timestamp': datetime.now().isoformat()
        })

    def summary(self) -> Dict:
        """生成摘要"""
        total = len(self._results)
        passed = sum(1 for r in self._results if r['passed'])
        failed = total - passed
        total_duration = sum(r['duration'] for r in self._results)

        return {
            'name': self.name,
            'total': total,
            'passed': passed,
            'failed': failed,
            'pass_rate': (passed / total * 100) if total > 0 else 0,
            'total_duration': total_duration,
            'elapsed_time': time.time() - self._start_time if self._start_time else 0,
            'results': self._results
        }

    def to_json(self, filepath: str = None) -> str:
        """导出为JSON"""
        data = self.summary()
        json_str = json.dumps(data, indent=2, ensure_ascii=False)

        if filepath:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(json_str)

        return json_str


class EnvironmentValidator:
    """环境验证器"""

    @staticmethod
    def check_python_version(min_version: Tuple[int, int] = (3, 8)) -> bool:
        """检查Python版本"""
        return sys.version_info >= min_version

    @staticmethod
    def check_module(module_name: str) -> bool:
        """检查模块是否可用"""
        try:
            __import__(module_name)
            return True
        except ImportError:
            return False

    @staticmethod
    def check_command(command: str) -> bool:
        """检查命令是否可用"""
        try:
            result = subprocess.run(
                ['where' if platform.system() == 'Windows' else 'which', command],
                capture_output=True,
                timeout=5
            )
            return result.returncode == 0
        except Exception:
            return False

    @classmethod
    def validate_all(cls, requirements: Dict[str, Any] = None) -> Dict[str, bool]:
        """验证所有要求"""
        results = {}

        results['python_version'] = cls.check_python_version()

        default_modules = ['pytest', 'json', 'pathlib', 'unittest.mock']
        modules = requirements.get('modules', default_modules) if requirements else default_modules

        for module in modules:
            results[f'module_{module}'] = cls.check_module(module)

        default_commands = ['python', 'pip']
        commands = requirements.get('commands', default_commands) if requirements else default_commands

        for cmd in commands:
            results[f'command_{cmd}'] = cls.check_command(cmd)

        return results


class DataComparator:
    """数据比较器"""

    @staticmethod
    def compare_dicts(actual: Dict, expected: Dict, ignore_keys: List[str] = None) -> Tuple[bool, List[str]]:
        """比较两个字典"""
        ignore_keys = ignore_keys or []
        differences = []

        def _compare(a, e, path=""):
            if isinstance(a, dict) and isinstance(e, dict):
                all_keys = set(a.keys()) | set(e.keys())
                for key in all_keys:
                    if key in ignore_keys:
                        continue
                    new_path = f"{path}.{key}" if path else key
                    if key not in a:
                        differences.append(f"Missing key: {new_path}")
                    elif key not in e:
                        differences.append(f"Extra key: {new_path}")
                    else:
                        _compare(a[key], e[key], new_path)
            elif isinstance(a, list) and isinstance(e, list):
                if len(a) != len(e):
                    differences.append(f"List length mismatch at {path}: {len(a)} vs {len(e)}")
                else:
                    for i, (ai, ei) in enumerate(zip(a, e)):
                        _compare(ai, ei, f"{path}[{i}]")
            elif a != e:
                differences.append(f"Value mismatch at {path}: {a} vs {e}")

        _compare(actual, expected)
        return len(differences) == 0, differences

    @staticmethod
    def compare_json_files(file1: Path, file2: Path) -> Tuple[bool, List[str]]:
        """比较两个JSON文件"""
        with open(file1, 'r', encoding='utf-8') as f:
            data1 = json.load(f)
        with open(file2, 'r', encoding='utf-8') as f:
            data2 = json.load(f)

        return DataComparator.compare_dicts(data1, data2)


class TestCleanup:
    """测试清理工具"""

    def __init__(self):
        self._cleanup_tasks = []

    def register(self, func: Callable, *args, **kwargs):
        """注册清理任务"""
        self._cleanup_tasks.append((func, args, kwargs))

    def cleanup(self):
        """执行所有清理任务"""
        errors = []
        for func, args, kwargs in reversed(self._cleanup_tasks):
            try:
                func(*args, **kwargs)
            except Exception as e:
                errors.append(f"{func.__name__}: {e}")

        self._cleanup_tasks.clear()
        return errors

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        errors = self.cleanup()
        if errors:
            print(f"清理警告: {errors}")


def skip_if(condition: bool, reason: str):
    """条件跳过装饰器"""
    def decorator(func):
        @wraps(func)
        def wrapper(*args, **kwargs):
            if condition:
                import pytest
                pytest.skip(reason)
            return func(*args, **kwargs)
        return wrapper
    return decorator


def timeout(seconds: float):
    """超时装饰器"""
    def decorator(func):
        @wraps(func)
        def wrapper(*args, **kwargs):
            import signal

            def handler(signum, frame):
                raise TimeoutError(f"函数 {func.__name__} 执行超时 ({seconds}秒)")

            if platform.system() != 'Windows':
                old_handler = signal.signal(signal.SIGALRM, handler)
                signal.alarm(int(seconds))
                try:
                    result = func(*args, **kwargs)
                finally:
                    signal.alarm(0)
                    signal.signal(signal.SIGALRM, old_handler)
                return result
            else:
                import threading
                result = [None]
                exception = [None]

                def target():
                    try:
                        result[0] = func(*args, **kwargs)
                    except Exception as e:
                        exception[0] = e

                thread = threading.Thread(target=target)
                thread.start()
                thread.join(timeout=seconds)

                if thread.is_alive():
                    raise TimeoutError(f"函数 {func.__name__} 执行超时 ({seconds}秒)")

                if exception[0]:
                    raise exception[0]

                return result[0]
        return wrapper
    return decorator


def benchmark(iterations: int = 100, warmup: int = 10):
    """基准测试装饰器"""
    def decorator(func):
        @wraps(func)
        def wrapper(*args, **kwargs):
            for _ in range(warmup):
                func(*args, **kwargs)

            times = []
            for _ in range(iterations):
                start = time.perf_counter()
                func(*args, **kwargs)
                times.append(time.perf_counter() - start)

            return {
                'function': func.__name__,
                'iterations': iterations,
                'min_ms': min(times) * 1000,
                'max_ms': max(times) * 1000,
                'avg_ms': sum(times) / len(times) * 1000,
                'median_ms': sorted(times)[len(times) // 2] * 1000
            }
        return wrapper
    return decorator


PROJECT_ROOT = Path(__file__).parent.parent.parent


def get_project_root() -> Path:
    """获取项目根目录"""
    return PROJECT_ROOT


def get_test_data_dir() -> Path:
    """获取测试数据目录"""
    return PROJECT_ROOT / "tests" / "fixtures" / "data"


def ensure_dir(path: Path) -> Path:
    """确保目录存在"""
    path.mkdir(parents=True, exist_ok=True)
    return path
