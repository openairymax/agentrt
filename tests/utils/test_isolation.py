# AgentOS 测试隔离和并行执行工具
# Version: 1.0.0.6
# Last updated: 2026-03-22

"""
测试隔离和并行执行模块。

提供测试用例独立性保证和执行效率优化功能。
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
from pathlib import Path
from typing import Dict, Any, List, Optional, Callable, Union
from contextlib import contextmanager
from unittest.mock import Mock, patch
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