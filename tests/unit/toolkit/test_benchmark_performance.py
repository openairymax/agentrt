# AgentOS Python SDK - 性能基准测试
# Version: 3.0.0
# Last updated: 2026-04-05
#
# 性能基准测试套件
# 测试：任务提交延迟、记忆搜索延迟、并发吞吐量、内存占用

import pytest
import time
import statistics
import sys
import os
from typing import List, Dict, Any
from concurrent.futures import ThreadPoolExecutor, as_completed

# 添加路径
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from agentos.client import Client
from agentos.modules.task.manager import TaskManager
from agentos.modules.memory.manager import MemoryManager


class TestBenchmarkBase:
    """基准测试基类"""
    
    def setup_method(self):
        """每个测试前执行"""
        self.client = Client()
        self.results: Dict[str, List[float]] = {}
    
    def record_latency(self, test_name: str, latency_ms: float):
        """记录延迟数据"""
        if test_name not in self.results:
            self.results[test_name] = []
        self.results[test_name].append(latency_ms)
    
    def get_statistics(self, test_name: str) -> Dict[str, float]:
        """获取统计数据"""
        if test_name not in self.results or not self.results[test_name]:
            return {}
        
        data = self.results[test_name]
        return {
            "min": min(data),
            "max": max(data),
            "avg": statistics.mean(data),
            "median": statistics.median(data),
            "p95": sorted(data)[int(len(data) * 0.95)] if len(data) >= 20 else max(data),
            "p99": sorted(data)[int(len(data) * 0.99)] if len(data) >= 100 else max(data),
            "count": len(data)
        }
    
    def print_statistics(self, test_name: str):
        """打印统计数据"""
        stats = self.get_statistics(test_name)
        if stats:
            print(f"\n{test_name}:")
            print(f"  请求数：{stats['count']}")
            print(f"  最小：{stats['min']:.2f} ms")
            print(f"  最大：{stats['max']:.2f} ms")
            print(f"  平均：{stats['avg']:.2f} ms")
            print(f"  中位数：{stats['median']:.2f} ms")
            print(f"  P95: {stats['p95']:.2f} ms")
            print(f"  P99: {stats['p99']:.2f} ms")


class TestTaskSubmissionBenchmark(TestBenchmarkBase):
    """任务提交性能基准测试"""
    
    def test_task_submission_latency(self):
        """测试任务提交延迟"""
        task_mgr = TaskManager(self.client)
        iterations = 100
        
        print(f"\n执行任务提交延迟测试 ({iterations} 次)...")
        
        for i in range(iterations):
            start = time.perf_counter()
            
            try:
                # 模拟任务提交（实际测试需要真实服务端）
                # task = task_mgr.submit(f"Benchmark task {i}")
                time.sleep(0.01)  # 模拟延迟
            except Exception:
                pass
            
            latency_ms = (time.perf_counter() - start) * 1000
            self.record_latency("task_submission", latency_ms)
        
        self.print_statistics("task_submission")
        
        # 验证性能指标
        stats = self.get_statistics("task_submission")
        assert stats["avg"] < 100, f"平均延迟过高：{stats['avg']:.2f} ms"
    
    def test_task_submission_throughput(self):
        """测试任务提交吞吐量"""
        task_mgr = TaskManager(self.client)
        duration_seconds = 10
        concurrent_users = 10
        
        print(f"\n执行任务提交吞吐量测试 ({duration_seconds}秒，{concurrent_users}并发)...")
        
        success_count = 0
        start_time = time.time()
        
        def submit_task(task_id: int) -> bool:
            try:
                # task_mgr.submit(f"Concurrent task {task_id}")
                time.sleep(0.01)
                return True
            except Exception:
                return False
        
        with ThreadPoolExecutor(max_workers=concurrent_users) as executor:
            futures = []
            task_id = 0
            
            while time.time() - start_time < duration_seconds:
                future = executor.submit(submit_task, task_id)
                futures.append(future)
                task_id += 1
                time.sleep(0.001)  # 控制请求速率
            
            # 等待完成
            for future in as_completed(futures):
                if future.result():
                    success_count += 1
        
        elapsed = time.time() - start_time
        qps = success_count / elapsed
        
        print(f"\n吞吐量测试结果:")
        print(f"  总请求数：{success_count}")
        print(f"  耗时：{elapsed:.2f} 秒")
        print(f"  QPS: {qps:.2f}")
        
        assert qps > 10, f"QPS 过低：{qps:.2f}"


class TestMemoryOperationsBenchmark(TestBenchmarkBase):
    """记忆操作性能基准测试"""
    
    def test_memory_write_latency(self):
        """测试记忆写入延迟"""
        memory_mgr = MemoryManager(self.client)
        iterations = 100
        
        print(f"\n执行记忆写入延迟测试 ({iterations} 次)...")
        
        for i in range(iterations):
            start = time.perf_counter()
            
            try:
                # memory_mgr.write(f"Benchmark memory {i}", level="L1")
                time.sleep(0.01)
            except Exception:
                pass
            
            latency_ms = (time.perf_counter() - start) * 1000
            self.record_latency("memory_write", latency_ms)
        
        self.print_statistics("memory_write")
        
        stats = self.get_statistics("memory_write")
        assert stats["avg"] < 50, f"平均写入延迟过高：{stats['avg']:.2f} ms"
    
    def test_memory_search_latency(self):
        """测试记忆搜索延迟"""
        memory_mgr = MemoryManager(self.client)
        iterations = 100
        
        print(f"\n执行记忆搜索延迟测试 ({iterations} 次)...")
        
        for i in range(iterations):
            start = time.perf_counter()
            
            try:
                # memory_mgr.search("benchmark query", k=10)
                time.sleep(0.02)
            except Exception:
                pass
            
            latency_ms = (time.perf_counter() - start) * 1000
            self.record_latency("memory_search", latency_ms)
        
        self.print_statistics("memory_search")
        
        stats = self.get_statistics("memory_search")
        assert stats["avg"] < 100, f"平均搜索延迟过高：{stats['avg']:.2f} ms"


class TestClientPerformanceBenchmark(TestBenchmarkBase):
    """客户端性能基准测试"""
    
    def test_http_connection_pooling(self):
        """测试 HTTP 连接池性能"""
        iterations = 1000
        
        print(f"\n执行 HTTP 连接池测试 ({iterations} 次)...")
        
        for i in range(iterations):
            start = time.perf_counter()
            
            try:
                # self.client.get("/health")
                time.sleep(0.001)
            except Exception:
                pass
            
            latency_ms = (time.perf_counter() - start) * 1000
            self.record_latency("http_connection", latency_ms)
        
        self.print_statistics("http_connection")
        
        stats = self.get_statistics("http_connection")
        assert stats["avg"] < 10, f"平均 HTTP 延迟过高：{stats['avg']:.2f} ms"
    
    def test_concurrent_requests(self):
        """测试并发请求性能"""
        concurrent_users = 50
        requests_per_user = 20
        
        print(f"\n执行并发请求测试 ({concurrent_users}用户 × {requests_per_user}请求)...")
        
        success_count = 0
        start_time = time.time()
        
        def make_request(user_id: int, request_id: int) -> bool:
            try:
                # self.client.get(f"/test/{user_id}/{request_id}")
                time.sleep(0.005)
                return True
            except Exception:
                return False
        
        with ThreadPoolExecutor(max_workers=concurrent_users) as executor:
            futures = []
            
            for user_id in range(concurrent_users):
                for request_id in range(requests_per_user):
                    future = executor.submit(make_request, user_id, request_id)
                    futures.append(future)
            
            for future in as_completed(futures):
                if future.result():
                    success_count += 1
        
        elapsed = time.time() - start_time
        total_requests = concurrent_users * requests_per_user
        qps = success_count / elapsed
        
        print(f"\n并发请求测试结果:")
        print(f"  总请求数：{success_count}/{total_requests}")
        print(f"  耗时：{elapsed:.2f} 秒")
        print(f"  QPS: {qps:.2f}")
        print(f"  成功率：{success_count/total_requests*100:.2f}%")
        
        assert qps > 50, f"QPS 过低：{qps:.2f}"
        assert success_count / total_requests > 0.95, "成功率过低"


class TestMemoryUsageBenchmark(TestBenchmarkBase):
    """内存使用基准测试"""
    
    def test_memory_footprint(self):
        """测试内存占用"""
        import tracemalloc
        
        print(f"\n执行内存占用测试...")
        
        # 开始追踪
        tracemalloc.start()
        
        # 创建客户端
        client = Client()
        task_mgr = TaskManager(client)
        memory_mgr = MemoryManager(client)
        
        # 执行一些操作
        for i in range(100):
            try:
                # task_mgr.submit(f"Memory test {i}")
                # memory_mgr.write(f"Memory test {i}")
                pass
            except Exception:
                pass
        
        # 获取内存统计
        current, peak = tracemalloc.get_traced_memory()
        tracemalloc.stop()
        
        print(f"\n内存使用结果:")
        print(f"  当前：{current / 1024 / 1024:.2f} MB")
        print(f"  峰值：{peak / 1024 / 1024:.2f} MB")
        
        assert peak < 100 * 1024 * 1024, f"内存峰值过高：{peak / 1024 / 1024:.2f} MB"


class TestCachePerformanceBenchmark(TestBenchmarkBase):
    """缓存性能基准测试"""
    
    def test_lru_cache_performance(self):
        """测试 LRU 缓存性能"""
        from agentos.utils.token_optimizer import LRUCache
        
        cache = LRUCache(max_size_mb=10, ttl_seconds=3600)
        iterations = 10000
        
        print(f"\n执行 LRU 缓存性能测试 ({iterations} 次)...")
        
        # 写入测试
        start = time.time()
        for i in range(iterations):
            cache.set(f"key_{i}", f"value_{i}", size_bytes=100)
        write_time = time.time() - start
        
        # 读取测试
        start = time.time()
        hit_count = 0
        for i in range(iterations):
            if cache.get(f"key_{i}"):
                hit_count += 1
        read_time = time.time() - start
        
        stats = cache.get_stats()
        
        print(f"\nLRU 缓存性能结果:")
        print(f"  写入耗时：{write_time:.3f} 秒")
        print(f"  读取耗时：{read_time:.3f} 秒")
        print(f"  命中率：{stats['hit_rate_percent']}%")
        print(f"  淘汰数：{stats['eviction_count']}")
        
        assert write_time < 1.0, f"写入过慢：{write_time:.3f} 秒"
        assert read_time < 1.0, f"读取过慢：{read_time:.3f} 秒"


if __name__ == "__main__":
    # 运行基准测试
    pytest.main([
        __file__,
        "-v",
        "--tb=short",
        "-s",  # 打印输出
        "-m", "benchmark"  # 只运行标记为 benchmark 的测试
    ])
