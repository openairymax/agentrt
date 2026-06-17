#!/usr/bin/env python3
"""
AgentOS Manager 模块性能基准测试

本脚本用于测试 Manager 模块的性能指标，包括：
- 任务管理器吞吐量
- 会话管理器响应时间
- 记忆管理器读写速度
- 技能管理器加载时间

使用方法:
    python benchmark_manager.py --verbose
    python benchmark_manager.py --iterations 1000
    python benchmark_manager.py --output report.json
"""

import argparse
import json
import sys
import time
from dataclasses import dataclass
from datetime import datetime
from typing import Any, Callable, Dict, List, Optional

# 尝试导入必要的模块，如果不存在则使用 mock
try:
    from agentos.modules.task.manager import TaskManager
    from agentos.modules.session.manager import SessionManager
    from agentos.modules.memory.manager import MemoryManager
    from agentos.modules.skill.manager import SkillManager
    MODULES_AVAILABLE = True
except ImportError:
    MODULES_AVAILABLE = False


@dataclass
class BenchmarkResult:
    """基准测试结果"""
    name: str
    iterations: int
    total_time_ms: float
    avg_time_ms: float
    min_time_ms: float
    max_time_ms: float
    ops_per_second: float
    std_dev_ms: Optional[float] = None

    def to_dict(self) -> Dict[str, Any]:
        return {
            "name": self.name,
            "iterations": self.iterations,
            "total_time_ms": self.total_time_ms,
            "avg_time_ms": self.avg_time_ms,
            "min_time_ms": self.min_time_ms,
            "max_time_ms": self.max_time_ms,
            "ops_per_second": self.ops_per_second,
            "std_dev_ms": self.std_dev_ms
        }


class PerformanceBenchmark:
    """性能基准测试类"""

    def __init__(self, verbose: bool = False):
        self.verbose = verbose
        self.results: List[BenchmarkResult] = []
        self.start_time = time.time()

    def log(self, message: str) -> None:
        """打印日志消息"""
        if self.verbose:
            print(f"  [Benchmark] {message}")

    def measure(
        self,
        name: str,
        func: Callable[[], Any],
        iterations: int = 100,
        warmup: int = 10
    ) -> BenchmarkResult:
        """测量函数执行性能

        Args:
            name: 测试名称
            func: 要测量的函数
            iterations: 迭代次数
            warmup: 预热次数

        Returns:
            BenchmarkResult: 测试结果
        """
        self.log(f"开始测量: {name}")

        # 预热
        self.log(f"预热 {warmup} 次...")
        for _ in range(warmup):
            func()

        # 实际测量
        self.log(f"执行 {iterations} 次测量...")
        times_ms: List[float] = []
        for _ in range(iterations):
            start = time.perf_counter()
            func()
            end = time.perf_counter()
            times_ms.append((end - start) * 1000)

        # 计算统计
        total_time = sum(times_ms)
        avg_time = total_time / iterations
        min_time = min(times_ms)
        max_time = max(times_ms)
        ops_per_second = 1000 / avg_time if avg_time > 0 else 0

        # 计算标准差
        variance = sum((t - avg_time) ** 2 for t in times_ms) / iterations
        std_dev = variance ** 0.5

        result = BenchmarkResult(
            name=name,
            iterations=iterations,
            total_time_ms=total_time,
            avg_time_ms=avg_time,
            min_time_ms=min_time,
            max_time_ms=max_time,
            ops_per_second=ops_per_second,
            std_dev_ms=std_dev
        )

        self.results.append(result)
        self.log(f"完成: 平均 {avg_time:.4f}ms, {ops_per_second:.2f} ops/s")

        return result

    def print_summary(self) -> None:
        """打印测试摘要"""
        elapsed = time.time() - self.start_time
        print("\n" + "=" * 70)
        print("AgentOS Manager 模块性能基准测试报告")
        print("=" * 70)
        print(f"测试时间: {datetime.now().isoformat()}")
        print(f"总耗时: {elapsed:.2f}秒")
        print(f"测试项目: {len(self.results)}")
        print("-" * 70)
        print(f"{'测试名称':<30} {'平均耗时':<12} {'吞吐量':<15} {'标准差':<10}")
        print("-" * 70)

        for r in self.results:
            print(
                f"{r.name:<30} "
                f"{r.avg_time_ms:>8.4f}ms  "
                f"{r.ops_per_second:>10.2f} ops/s  "
                f"{r.std_dev_ms:>8.4f}ms"
            )

        print("-" * 70)
        print("\n性能评级:")

        for r in self.results:
            if r.avg_time_ms < 1.0:
                rating = "🟢 优秀"
            elif r.avg_time_ms < 10.0:
                rating = "🟡 良好"
            elif r.avg_time_ms < 100.0:
                rating = "🟠 一般"
            else:
                rating = "🔴 需优化"

            print(f"  {r.name}: {rating} (平均 {r.avg_time_ms:.4f}ms)")

        print("=" * 70)

    def save_results(self, filepath: str) -> None:
        """保存结果到JSON文件"""
        output = {
            "timestamp": datetime.now().isoformat(),
            "total_elapsed_seconds": time.time() - self.start_time,
            "results": [r.to_dict() for r in self.results]
        }

        with open(filepath, 'w', encoding='utf-8') as f:
            json.dump(output, f, indent=2, ensure_ascii=False)

        print(f"\n结果已保存到: {filepath}")


def create_mock_api():
    """创建Mock API客户端"""
    class MockAPI:
        def __init__(self):
            self.call_count = 0

        def post(self, endpoint: str, data: Any = None) -> Dict[str, Any]:
            self.call_count += 1
            return {"success": True, "data": {"id": f"mock_{self.call_count}"}}

        def get(self, endpoint: str) -> Dict[str, Any]:
            self.call_count += 1
            return {"success": True, "data": {"id": f"mock_{self.call_count}"}}

        def delete(self, endpoint: str) -> Dict[str, Any]:
            self.call_count += 1
            return {"success": True}

        def put(self, endpoint: str, data: Any = None) -> Dict[str, Any]:
            self.call_count += 1
            return {"success": True, "data": {"id": f"mock_{self.call_count}"}}

    return MockAPI()


def benchmark_task_manager(benchmark: PerformanceBenchmark, iterations: int) -> None:
    """测试任务管理器性能"""
    from agentos.modules.task.manager import TaskManager

    api = create_mock_api()
    manager = TaskManager(api)

    # 测试任务提交
    benchmark.measure(
        "TaskManager.submit",
        lambda: manager.submit(f"task_{time.time()}"),
        iterations=iterations
    )

    # 测试任务查询
    benchmark.measure(
        "TaskManager.query",
        lambda: manager.query("task_1"),
        iterations=iterations
    )

    # 测试任务列表
    benchmark.measure(
        "TaskManager.list",
        lambda: manager.list(),
        iterations=iterations
    )


def benchmark_session_manager(benchmark: PerformanceBenchmark, iterations: int) -> None:
    """测试会话管理器性能"""
    from agentos.modules.session.manager import SessionManager

    api = create_mock_api()
    manager = SessionManager(api)

    # 测试会话创建
    benchmark.measure(
        "SessionManager.create",
        lambda: manager.create("user_test"),
        iterations=iterations
    )

    # 测试会话获取
    benchmark.measure(
        "SessionManager.get",
        lambda: manager.get("sess_1"),
        iterations=iterations
    )

    # 测试会话关闭
    benchmark.measure(
        "SessionManager.close",
        lambda: manager.close("sess_1"),
        iterations=iterations
    )


def benchmark_memory_manager(benchmark: PerformanceBenchmark, iterations: int) -> None:
    """测试记忆管理器性能"""
    from agentos.modules.memory.manager import MemoryManager
    from agentos.types.common import MemoryLayer

    api = create_mock_api()
    manager = MemoryManager(api)

    # 测试记忆写入
    benchmark.measure(
        "MemoryManager.write",
        lambda: manager.write(f"memory_content_{time.time()}", MemoryLayer.L1),
        iterations=iterations
    )

    # 测试记忆搜索
    benchmark.measure(
        "MemoryManager.search",
        lambda: manager.search("test query"),
        iterations=iterations
    )

    # 测试记忆列表
    benchmark.measure(
        "MemoryManager.list",
        lambda: manager.list(),
        iterations=iterations
    )


def benchmark_skill_manager(benchmark: PerformanceBenchmark, iterations: int) -> None:
    """测试技能管理器性能"""
    from agentos.modules.skill.manager import SkillManager

    api = create_mock_api()
    manager = SkillManager(api)

    # 测试技能加载
    benchmark.measure(
        "SkillManager.load",
        lambda: manager.load("skill_1"),
        iterations=iterations
    )

    # 测试技能列表
    benchmark.measure(
        "SkillManager.list",
        lambda: manager.list(),
        iterations=iterations
    )

    # 测试技能获取
    benchmark.measure(
        "SkillManager.get",
        lambda: manager.get("skill_1"),
        iterations=iterations
    )


def main():
    parser = argparse.ArgumentParser(
        description="AgentOS Manager 模块性能基准测试"
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="显示详细日志"
    )
    parser.add_argument(
        "-i", "--iterations",
        type=int,
        default=100,
        help="每个测试的迭代次数 (默认: 100)"
    )
    parser.add_argument(
        "-o", "--output",
        type=str,
        default=None,
        help="输出JSON结果文件路径"
    )
    parser.add_argument(
        "--skip-modules",
        action="store_true",
        help="跳过实际模块测试 (仅测试框架)"
    )

    args = parser.parse_args()

    print("=" * 70)
    print("AgentOS Manager 模块性能基准测试")
    print("=" * 70)
    print(f"迭代次数: {args.iterations}")
    print(f"详细模式: {'是' if args.verbose else '否'}")
    print("-" * 70)

    benchmark = PerformanceBenchmark(verbose=args.verbose)

    if not args.skip_modules and MODULES_AVAILABLE:
        print("\n开始性能测试...")
        print("-" * 70)

        try:
            benchmark_task_manager(benchmark, args.iterations)
        except Exception as e:
            print(f"警告: TaskManager 测试失败: {e}")

        try:
            benchmark_session_manager(benchmark, args.iterations)
        except Exception as e:
            print(f"警告: SessionManager 测试失败: {e}")

        try:
            benchmark_memory_manager(benchmark, args.iterations)
        except Exception as e:
            print(f"警告: MemoryManager 测试失败: {e}")

        try:
            benchmark_skill_manager(benchmark, args.iterations)
        except Exception as e:
            print(f"警告: SkillManager 测试失败: {e}")

    else:
        # 仅测试框架
        print("\n执行框架测试...")
        print("-" * 70)

        def dummy_operation():
            """模拟耗时操作"""
            _ = sum(range(100))

        benchmark.measure(
            "Framework.dummy_operation",
            dummy_operation,
            iterations=args.iterations
        )

    # 打印摘要
    benchmark.print_summary()

    # 保存结果
    if args.output:
        benchmark.save_results(args.output)

    return 0


if __name__ == "__main__":
    sys.exit(main())
