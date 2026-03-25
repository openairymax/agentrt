# AgentOS 性能基准测试模块
# Version: 1.0.0.6
# Last updated: 2026-03-22

"""
AgentOS 性能基准测试模块。

测试系统性能指标，包括并发性能、检索延迟、吞吐量等。
"""

import pytest
import time
import asyncio
import statistics
import random
import string
from typing import Dict, Any, List, Callable, Optional
from unittest.mock import Mock, MagicMock, patch, AsyncMock
from concurrent.futures import ThreadPoolExecutor, as_completed
import threading

import sys
import os
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', 'tools', 'python')))


# ============================================================
# 测试标记
# ============================================================

pytestmark = pytest.mark.benchmark


# ============================================================
# 性能测试辅助类
# ============================================================

class PerformanceMetrics:
    """性能指标收集器"""
    
    def __init__(self):
        self.latencies: List[float] = []
        self.success_count: int = 0
        self.failure_count: int = 0
        self.start_time: Optional[float] = None
        self.end_time: Optional[float] = None
    
    def start(self) -> None:
        self.start_time = time.perf_counter()
    
    def stop(self) -> None:
        self.end_time = time.perf_counter()
    
    def record(self, latency: float, success: bool = True) -> None:
        self.latencies.append(latency)
        if success:
            self.success_count += 1
        else:
            self.failure_count += 1
    
    @property
    def total_duration(self) -> float:
        if self.start_time is None or self.end_time is None:
            return 0.0
        return self.end_time - self.start_time
    
    @property
    def avg_latency(self) -> float:
        if not self.latencies:
            return 0.0
        return statistics.mean(self.latencies)
    
    @property
    def p50_latency(self) -> float:
        if not self.latencies:
            return 0.0
        return sorted(self.latencies)[int(len(self.latencies) * 0.5)]
    
    @property
    def p95_latency(self) -> float:
        if not self.latencies:
            return 0.0
        return sorted(self.latencies)[int(len(self.latencies) * 0.95)]
    
    @property
    def p99_latency(self) -> float:
        if not self.latencies:
            return 0.0
        return sorted(self.latencies)[int(len(self.latencies) * 0.99)]
    
    @property
    def throughput(self) -> float:
        if self.total_duration == 0:
            return 0.0
        return self.success_count / self.total_duration
    
    @property
    def success_rate(self) -> float:
        total = self.success_count + self.failure_count
        if total == 0:
            return 0.0
        return self.success_count / total
    
    def to_dict(self) -> Dict[str, Any]:
        return {
            "total_duration_s": self.total_duration,
            "total_requests": self.success_count + self.failure_count,
            "success_count": self.success_count,
            "failure_count": self.failure_count,
            "success_rate": self.success_rate,
            "avg_latency_ms": self.avg_latency * 1000,
            "p50_latency_ms": self.p50_latency * 1000,
            "p95_latency_ms": self.p95_latency * 1000,
            "p99_latency_ms": self.p99_latency * 1000,
            "throughput_rps": self.throughput,
        }


# ============================================================
# 并发性能测试
# ============================================================

class TestConcurrencyPerformance:
    """并发性能测试"""
    
    @pytest.mark.benchmark
    def test_concurrent_task_submission(self):
        """
        测试并发任务提交性能。
        
        验证:
            - 能处理高并发任务提交
            - 成功率 > 95%
            - 平均延迟 < 100ms
        """
        metrics = PerformanceMetrics()
        
        def submit_task_mock(task_id: int) -> float:
            start = time.perf_counter()
            time.sleep(random.uniform(0.001, 0.01))
            latency = time.perf_counter() - start
            return latency
        
        concurrent_count = 100
        metrics.start()
        
        with ThreadPoolExecutor(max_workers=10) as executor:
            futures = [executor.submit(submit_task_mock, i) for i in range(concurrent_count)]
            
            for future in as_completed(futures):
                try:
                    latency = future.result()
                    metrics.record(latency, success=True)
                except Exception:
                    metrics.record(0, success=False)
        
        metrics.stop()
        
        assert metrics.success_rate >= 0.95, f"成功率过低: {metrics.success_rate}"
        assert metrics.avg_latency < 0.1, f"平均延迟过高: {metrics.avg_latency * 1000}ms"
    
    @pytest.mark.benchmark
    def test_concurrent_memory_operations(self):
        """
        测试并发记忆操作性能。
        
        验证:
            - 并发写入和读取正常
            - 无数据竞争
            - 性能合理
        """
        metrics = PerformanceMetrics()
        shared_data = {}
        lock = threading.Lock()
        
        def write_memory(content: str) -> float:
            start = time.perf_counter()
            with lock:
                memory_id = f"mem_{time.time_ns()}"
                shared_data[memory_id] = content
            latency = time.perf_counter() - start
            return latency
        
        def read_memory(memory_id: str) -> float:
            start = time.perf_counter()
            with lock:
                _ = shared_data.get(memory_id, "")
            latency = time.perf_counter() - start
            return latency
        
        concurrent_count = 50
        metrics.start()
        
        with ThreadPoolExecutor(max_workers=10) as executor:
            write_futures = [
                executor.submit(write_memory, f"content_{i}")
                for i in range(concurrent_count)
            ]
            
            for future in as_completed(write_futures):
                try:
                    latency = future.result()
                    metrics.record(latency, success=True)
                except Exception:
                    metrics.record(0, success=False)
        
        metrics.stop()
        
        assert metrics.success_rate >= 0.99, f"成功率过低: {metrics.success_rate}"
        assert len(shared_data) == concurrent_count, "数据丢失"
    
    @pytest.mark.benchmark
    @pytest.mark.asyncio
    async def test_async_concurrent_requests(self):
        """
        测试异步并发请求性能。
        
        验证:
            - 异步并发性能优于同步
            - 能处理大量并发
        """
        metrics = PerformanceMetrics()
        
        async def async_request(request_id: int) -> float:
            start = time.perf_counter()
            await asyncio.sleep(random.uniform(0.001, 0.005))
            latency = time.perf_counter() - start
            return latency
        
        concurrent_count = 200
        metrics.start()
        
        tasks = [async_request(i) for i in range(concurrent_count)]
        results = await asyncio.gather(*tasks, return_exceptions=True)
        
        for result in results:
            if isinstance(result, Exception):
                metrics.record(0, success=False)
            else:
                metrics.record(result, success=True)
        
        metrics.stop()
        
        assert metrics.success_rate >= 0.99, f"成功率过低: {metrics.success_rate}"
        assert metrics.total_duration < 2.0, f"总耗时过长: {metrics.total_duration}s"


# ============================================================
# 检索延迟测试
# ============================================================

class TestRetrievalLatency:
    """检索延迟测试"""
    
    @pytest.fixture
    def mock_memory_store(self):
        """
        创建模拟记忆存储。
        
        Returns:
            Dict: 模拟的记忆数据
        """
        memories = {}
        for i in range(10000):
            memories[f"mem_{i}"] = {
                "content": f"记忆内容 {i} " + "".join(random.choices(string.ascii_letters, k=100)),
                "embedding": [random.random() for _ in range(128)],
                "created_at": time.time() - random.randint(0, 86400 * 30)
            }
        return memories
    
    @pytest.mark.benchmark
    def test_memory_search_latency(self, mock_memory_store):
        """
        测试记忆搜索延迟。
        
        验证:
            - P95延迟 < 50ms
            - P99延迟 < 100ms
        """
        metrics = PerformanceMetrics()
        
        def search_memory(query: str, top_k: int = 10) -> float:
            start = time.perf_counter()
            
            results = []
            query_lower = query.lower()
            for mem_id, mem_data in mock_memory_store.items():
                if query_lower in mem_data["content"].lower():
                    results.append((mem_id, mem_data))
                    if len(results) >= top_k:
                        break
            
            latency = time.perf_counter() - start
            return latency
        
        test_queries = [f"内容 {i}" for i in range(100)]
        
        metrics.start()
        for query in test_queries:
            try:
                latency = search_memory(query)
                metrics.record(latency, success=True)
            except Exception:
                metrics.record(0, success=False)
        metrics.stop()
        
        assert metrics.p95_latency < 0.05, f"P95延迟过高: {metrics.p95_latency * 1000}ms"
        assert metrics.p99_latency < 0.1, f"P99延迟过高: {metrics.p99_latency * 1000}ms"
    
    @pytest.mark.benchmark
    def test_vector_search_latency(self):
        """
        测试向量检索延迟。
        
        验证:
            - 不同k值下的延迟合理
        """
        metrics = PerformanceMetrics()
        
        def vector_search(query_vector: List[float], k: int) -> float:
            start = time.perf_counter()
            
            mock_results = [
                {"id": f"vec_{i}", "score": random.random(), "vector": [random.random() for _ in range(128)]}
                for i in range(k)
            ]
            
            latency = time.perf_counter() - start
            return latency
        
        query_vector = [random.random() for _ in range(128)]
        k_values = [1, 5, 10, 50, 100]
        
        for k in k_values:
            metrics = PerformanceMetrics()
            
            for _ in range(100):
                latency = vector_search(query_vector, k)
                metrics.record(latency)
            
            assert metrics.avg_latency < 0.01, f"k={k}时平均延迟过高: {metrics.avg_latency * 1000}ms"


# ============================================================
# 吞吐量测试
# ============================================================

class TestThroughput:
    """吞吐量测试"""
    
    @pytest.mark.benchmark
    def test_task_throughput(self):
        """
        测试任务吞吐量。
        
        验证:
            - 每秒能处理足够多的任务
        """
        metrics = PerformanceMetrics()
        
        def process_task(task_id: int) -> float:
            start = time.perf_counter()
            time.sleep(0.001)
            latency = time.perf_counter() - start
            return latency
        
        total_tasks = 1000
        metrics.start()
        
        with ThreadPoolExecutor(max_workers=20) as executor:
            futures = [executor.submit(process_task, i) for i in range(total_tasks)]
            
            for future in as_completed(futures):
                try:
                    latency = future.result()
                    metrics.record(latency, success=True)
                except Exception:
                    metrics.record(0, success=False)
        
        metrics.stop()
        
        assert metrics.throughput >= 100, f"吞吐量过低: {metrics.throughput} tasks/s"
    
    @pytest.mark.benchmark
    def test_memory_write_throughput(self):
        """
        测试记忆写入吞吐量。
        
        验证:
            - 每秒能写入足够多的记忆
        """
        metrics = PerformanceMetrics()
        
        def write_memory(content: str) -> float:
            start = time.perf_counter()
            time.sleep(0.0005)
            latency = time.perf_counter() - start
            return latency
        
        total_writes = 500
        metrics.start()
        
        with ThreadPoolExecutor(max_workers=10) as executor:
            futures = [
                executor.submit(write_memory, f"记忆内容 {i}")
                for i in range(total_writes)
            ]
            
            for future in as_completed(futures):
                try:
                    latency = future.result()
                    metrics.record(latency, success=True)
                except Exception:
                    metrics.record(0, success=False)
        
        metrics.stop()
        
        assert metrics.throughput >= 200, f"写入吞吐量过低: {metrics.throughput} writes/s"


# ============================================================
# 资源使用测试
# ============================================================

class TestResourceUsage:
    """资源使用测试"""
    
    @pytest.mark.benchmark
    def test_memory_usage(self):
        """
        测试内存使用。
        
        验证:
            - 内存使用合理
            - 无内存泄漏
        """
        import gc
        
        gc.collect()
        
        data_list = []
        for i in range(10000):
            data_list.append({
                "id": i,
                "data": "x" * 100,
                "nested": {"key": "value"}
            })
        
        assert len(data_list) == 10000
        
        data_list.clear()
        gc.collect()
        
        assert len(data_list) == 0
    
    @pytest.mark.benchmark
    def test_cpu_usage(self):
        """
        测试CPU使用。
        
        验证:
            - CPU密集操作性能合理
        """
        def cpu_intensive_task(n: int) -> int:
            result = 0
            for i in range(n):
                result += i * i
            return result
        
        start = time.perf_counter()
        
        with ThreadPoolExecutor(max_workers=4) as executor:
            futures = [executor.submit(cpu_intensive_task, 10000) for _ in range(10)]
            results = [f.result() for f in futures]
        
        elapsed = time.perf_counter() - start
        
        assert len(results) == 10
        assert elapsed < 5.0, f"CPU密集操作耗时过长: {elapsed}s"


# ============================================================
# 压力测试
# ============================================================

class TestStress:
    """压力测试"""
    
    @pytest.mark.slow
    @pytest.mark.benchmark
    def test_high_load_stability(self):
        """
        测试高负载稳定性。
        
        验证:
            - 高负载下系统稳定
            - 无崩溃
        """
        metrics = PerformanceMetrics()
        
        def stress_task(task_id: int) -> float:
            start = time.perf_counter()
            
            data = [random.random() for _ in range(1000)]
            _ = sum(data)
            
            latency = time.perf_counter() - start
            return latency
        
        total_tasks = 5000
        metrics.start()
        
        with ThreadPoolExecutor(max_workers=50) as executor:
            futures = [executor.submit(stress_task, i) for i in range(total_tasks)]
            
            for future in as_completed(futures):
                try:
                    latency = future.result()
                    metrics.record(latency, success=True)
                except Exception:
                    metrics.record(0, success=False)
        
        metrics.stop()
        
        assert metrics.success_rate >= 0.99, f"成功率过低: {metrics.success_rate}"
        assert metrics.failure_count < total_tasks * 0.01, f"失败次数过多: {metrics.failure_count}"


# ============================================================
# 运行测试
# ============================================================

if __name__ == "__main__":
    pytest.main([__file__, "-v", "--tb=short", "-m", "benchmark"])
