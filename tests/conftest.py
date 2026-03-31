# AgentOS 测试夹具和共享配置
# Version: 1.0.0.6
# Last updated: 2026-03-22

"""
测试夹具和共享配置模块。

提供测试所需的共享资源、模拟对象和配置。
"""

import os
import sys
import json
import time
import tempfile
import shutil
from pathlib import Path
from typing import Dict, Any, List, Optional, Generator
from dataclasses import dataclass, field
from unittest.mock import Mock, MagicMock, patch

import pytest

# 添加项目根目录到路径
PROJECT_ROOT = Path(__file__).parent.parent.parent
sys.path.insert(0, str(PROJECT_ROOT / "tools" / "python"))


# ============================================================
# 测试配置常量
# ============================================================

class TestConfig:
    """测试配置常量"""

    # 默认测试端点
    DEFAULT_ENDPOINT = "http://localhost:18789"

    # 测试超时时间（秒）
    DEFAULT_TIMEOUT = 30

    # 测试数据目录
    TEST_DATA_DIR = PROJECT_ROOT / "tests" / "fixtures" / "data"

    # 临时文件目录
    TEMP_DIR = Path(tempfile.gettempdir()) / "agentos_tests"

    # 覆盖率目标
    COVERAGE_TARGET = 80

    # 性能基准
    BENCHMARK_ITERATIONS = 100
    BENCHMARK_WARMUP = 10

    # 测试标记
    MARKERS = {
        "unit": "单元测试",
        "integration": "集成测试",
        "e2e": "端到端测试",
        "security": "安全测试",
        "benchmark": "性能基准测试",
        "contract": "合约测试",
        "slow": "慢速测试",
        "smoke": "冒烟测试",
        "regression": "回归测试",
        "sdk": "SDK测试",
        "api": "API测试",
        "ffi": "FFI测试",
    }


# ============================================================
# 测试数据类
# ============================================================

@dataclass
class TestDataRecord:
    """测试数据记录"""
    id: str
    content: str
    metadata: Dict[str, Any] = field(default_factory=dict)
    created_at: float = field(default_factory=time.time)


@dataclass
class MockTaskResponse:
    """模拟任务响应"""
    task_id: str
    status: str = "pending"
    output: Optional[str] = None
    error: Optional[str] = None


@dataclass
class MockMemoryResponse:
    """模拟记忆响应"""
    memory_id: str
    content: str
    score: float = 1.0


# ============================================================
# 基础测试夹具
# ============================================================

@pytest.fixture(scope="session")
def test_config():
    """提供测试配置"""
    return TestConfig()


@pytest.fixture(scope="function")
def temp_dir():
    """
    提供临时目录。

    Yields:
        Path: 临时目录路径
    """
    temp_path = Path(tempfile.mkdtemp(prefix="agentos_test_"))

    yield temp_path

    # 清理临时目录
    shutil.rmtree(temp_path, ignore_errors=True)


@pytest.fixture(scope="function")
def sample_task_data():
    """
    提供示例任务数据。

    Returns:
        Dict: 示例任务数据
    """
    return {
        "task_id": "test_task_001",
        "description": "测试任务",
        "status": "pending",
        "priority": 1,
        "created_at": "2026-03-22T10:00:00Z",
        "metadata": {
            "type": "test",
            "source": "unit_test"
        }
    }


@pytest.fixture(scope="function")
def sample_memory_data():
    """
    提供示例记忆数据。

    Returns:
        Dict: 示例记忆数据
    """
    return {
        "memory_id": "test_mem_001",
        "content": "测试记忆内容",
        "layer": "L1",
        "created_at": "2026-03-22T10:00:00Z",
        "metadata": {
            "category": "test",
            "confidence": 0.95
        }
    }


# ============================================================
# Mock 对象夹具
# ============================================================

@pytest.fixture(scope="function")
def mock_http_response():
    """
    提供模拟HTTP响应。

    Returns:
        Mock: 模拟的HTTP响应对象
    """
    response = Mock()
    response.status_code = 200
    response.json.return_value = {}
    response.text = ""
    return response


@pytest.fixture(scope="function")
def mock_agentos_client():
    """
    提供模拟的AgentOS客户端。

    Returns:
        Mock: 模拟的AgentOS客户端
    """
    client = Mock()
    client.endpoint = TestConfig.DEFAULT_ENDPOINT
    client.timeout = TestConfig.DEFAULT_TIMEOUT
    return client


# ============================================================
# 测试数据加载器
# ============================================================

@pytest.fixture(scope="session")
def load_test_data():
    """
    提供测试数据加载函数。

    Returns:
        Callable: 数据加载函数
    """
    def _load_data(data_type: str, filename: str = None) -> Dict[str, Any]:
        """
        加载测试数据。

        Args:
            data_type: 数据类型 (tasks, memories, sessions, skills)
            filename: 文件名，如果为None则使用默认文件

        Returns:
            Dict: 加载的数据
        """
        if filename is None:
            filename = f"sample_{data_type}.json"

        data_file = TestConfig.TEST_DATA_DIR / data_type / filename

        if not data_file.exists():
            return {}

        try:
            with open(data_file, 'r', encoding='utf-8') as f:
                return json.load(f)
        except (json.JSONDecodeError, IOError):
            return {}

    return _load_data


# ============================================================
# 性能测试辅助
# ============================================================

class PerformanceTimer:
    """性能计时器"""

    def __init__(self, name: str = "operation"):
        """
        初始化计时器。

        Args:
            name: 操作名称
        """
        self.name = name
        self.start_time: Optional[float] = None
        self.end_time: Optional[float] = None
        self.elapsed: Optional[float] = None

    def __enter__(self) -> "PerformanceTimer":
        """进入上下文"""
        self.start_time = time.perf_counter()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        """退出上下文"""
        self.end_time = time.perf_counter()
        self.elapsed = self.end_time - self.start_time

    def assert_faster_than(self, max_seconds: float) -> None:
        """
        断言执行时间小于指定值。

        Args:
            max_seconds: 最大允许时间（秒）

        Raises:
            AssertionError: 如果执行时间超过限制
        """
        if self.elapsed is None:
            raise RuntimeError("Timer has not been stopped")

        if self.elapsed > max_seconds:
            raise AssertionError(
                f"{self.name} took {self.elapsed:.3f}s, "
                f"expected < {max_seconds:.3f}s"
            )


@pytest.fixture(scope="function")
def performance_timer() -> PerformanceTimer:
    """
    提供性能计时器。

    Returns:
        PerformanceTimer: 计时器实例
    """
    return PerformanceTimer()


# ============================================================
# 测试环境检查
# ============================================================

def check_test_environment() -> Dict[str, bool]:
    """
    检查测试环境是否满足要求。

    Returns:
        Dict[str, bool]: 环境检查结果
    """
    results = {}

    # 检查Python版本
    results["python_version"] = sys.version_info >= (3, 8)

    # 检查必要的模块
    required_modules = [
        "pytest",
        "requests",
        "aiohttp",
    ]

    for module in required_modules:
        try:
            __import__(module)
            results[f"module_{module}"] = True
        except ImportError:
            results[f"module_{module}"] = False

    # 检查测试数据目录
    results["test_data_dir"] = TestConfig.TEST_DATA_DIR.exists()

    # 检查临时目录权限
    try:
        TestConfig.TEMP_DIR.mkdir(parents=True, exist_ok=True)
        test_file = TestConfig.TEMP_DIR / "permission_test"
        test_file.write_text("test")
        test_file.unlink()
        results["temp_dir_writable"] = True
    except Exception:
        results["temp_dir_writable"] = False

    return results


@pytest.fixture(scope="session", autouse=True)
def verify_test_environment():
    """
    自动验证测试环境。

    Yields:
        None
    """
    results = check_test_environment()

    failed_checks = [k for k, v in results.items() if not v]

    if failed_checks:
        pytest.fail(
            f"测试环境检查失败: {', '.join(failed_checks)}"
        )

    yield


# ============================================================
# 异步测试支持
# ============================================================

try:
    import pytest_asyncio
    pytest_asyncio.auto_mode = True
except ImportError:
    # 如果没有安装pytest-asyncio，提供一个警告
    import warnings
    warnings.warn(
        "pytest-asyncio not installed. Async tests will be skipped. "
        "Install with: pip install pytest-asyncio"
    )
