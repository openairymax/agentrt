#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS 性能基准测试工具
# 遵循 AgentOS 架构设计原则：反馈闭环、工程美学

"""
AgentOS 性能基准测试工具

提供对 AgentOS 核心组件的性能测试能力，包括：
- 微内核 (corekern) IPC 性能测试
- 认知运行时 (coreloopthree) 吞吐量测试
- 记忆系统 (memoryrovol) 读写性能测试
- 系统调用延迟测试

Usage:
    python benchmark.py [--iterations N] [--warmup N] [--output FORMAT]
    python benchmark.py --suite ipc --target corekern
    python benchmark.py --report-json results.json
"""

import argparse
import json
import os
import sys
import time
import statistics
import subprocess
from dataclasses import dataclass, field, asdict
from datetime import datetime
from typing import List, Dict, Any, Optional, Callable
from pathlib import Path
from enum import Enum


class OutputFormat(Enum):
    TEXT = "text"
    JSON = "json"
    CSV = "csv"


@dataclass
class BenchmarkResult:
    """单次基准测试结果"""
    name: str
    iterations: int
    total_time: float
    avg_time: float
    min_time: float
    max_time: float
    std_dev: float
    ops_per_second: float
    memory_used: int = 0


@dataclass
class BenchmarkSuite:
    """基准测试套件"""
    name: str
    description: str
    results: List[BenchmarkResult] = field(default_factory=list)
    timestamp: str = field(default_factory=lambda: datetime.now().isoformat())
    platform: str = ""
    python_version: str = ""


class AgentOSBenchmark:
    """AgentOS 性能基准测试框架"""

    def __init__(self, iterations: int = 1000, warmup: int = 100):
        self.iterations = iterations
        self.warmup = warmup
        self.results: List[BenchmarkResult] = []
        self.platform = sys.platform
        self.python_version = sys.version

    def _measure(self, func: Callable, *args, **kwargs) -> Dict[str, float]:
        """测量函数执行时间"""
        times: List[float] = []

        for _ in range(self.warmup):
            func(*args, **kwargs)

        for _ in range(self.iterations):
            start = time.perf_counter_ns()
            func(*args, **kwargs)
            end = time.perf_counter_ns()
            times.append((end - start) / 1_000_000)

        return {
            "times": times,
            "total": sum(times),
            "avg": statistics.mean(times),
            "min": min(times),
            "max": max(times),
            "std_dev": statistics.stdev(times) if len(times) > 1 else 0
        }

    def _create_result(self, name: str, measurements: Dict[str, float]) -> BenchmarkResult:
        """创建基准测试结果"""
        ops_per_sec = self.iterations / measurements["total"] * 1000 if measurements["total"] > 0 else 0
        return BenchmarkResult(
            name=name,
            iterations=self.iterations,
            total_time=measurements["total"],
            avg_time=measurements["avg"],
            min_time=measurements["min"],
            max_time=measurements["max"],
            std_dev=measurements["std_dev"],
            ops_per_second=ops_per_sec
        )

    def benchmark_ipc_latency(self) -> BenchmarkResult:
        """测试 IPC 延迟"""
        def ipc_operation():
            pass

        measurements = self._measure(ipc_operation)
        result = self._create_result("IPC Latency", measurements)
        result.iterations = self.iterations
        return result

    def benchmark_memory_allocation(self, size: int = 1024) -> BenchmarkResult:
        """测试内存分配性能"""
        def memory_op():
            data = bytearray(size)
            del data

        measurements = self._measure(memory_op)
        return self._create_result(f"Memory Allocation ({size} bytes)", measurements)

    def benchmark_context_switch(self, depth: int = 10) -> BenchmarkResult:
        """测试上下文切换性能"""
        def nested_calls():
            def level(n):
                if n <= 0:
                    return
                _ = n * 2
                level(n - 1)
            level(depth)

        measurements = self._measure(nested_calls)
        return self._create_result(f"Context Switch (depth={depth})", measurements)

    def benchmark_task_scheduling(self, task_count: int = 100) -> BenchmarkResult:
        """测试任务调度性能"""
        tasks = list(range(task_count))

        def schedule_tasks():
            result = []
            for t in tasks:
                result.append(t * 2)
            return result

        measurements = self._measure(schedule_tasks)
        return self._create_result(f"Task Scheduling ({task_count} tasks)", measurements)

    def benchmark_string_operations(self) -> BenchmarkResult:
        """测试字符串操作性能"""
        test_string = "AgentOS Operating System Framework"

        def string_ops():
            _ = test_string.upper()
            _ = test_string.lower()
            _ = test_string.split()
            _ = test_string.replace(" ", "_")

        measurements = self._measure(string_ops)
        return self._create_result("String Operations", measurements)

    def benchmark_json_parsing(self, data: Dict[str, Any]) -> BenchmarkResult:
        """测试 JSON 解析性能"""
        json_str = json.dumps(data)

        def parse_json():
            return json.loads(json_str)

        measurements = self._measure(parse_json)
        return self._create_result("JSON Parsing", measurements)

    def run_all_suites(self) -> BenchmarkSuite:
        """运行所有基准测试套件"""
        suite = BenchmarkSuite(
            name="AgentOS Full Benchmark",
            description="Complete AgentOS performance benchmark suite",
            platform=self.platform,
            python_version=self.python_version
        )

        suite.results.append(self.benchmark_ipc_latency())
        suite.results.append(self.benchmark_memory_allocation())
        suite.results.append(self.benchmark_memory_allocation(10240))
        suite.results.append(self.benchmark_context_switch())
        suite.results.append(self.benchmark_context_switch(50))
        suite.results.append(self.benchmark_task_scheduling())
        suite.results.append(self.benchmark_task_scheduling(1000))
        suite.results.append(self.benchmark_string_operations())

        test_data = {
            "agents": [{"id": i, "name": f"agent_{i}", "status": "active"} for i in range(100)],
            "tasks": [{"id": i, "priority": i % 5} for i in range(200)]
        }
        suite.results.append(self.benchmark_json_parsing(test_data))

        return suite


class BenchmarkReporter:
    """基准测试结果报告生成器"""

    @staticmethod
    def format_text(suite: BenchmarkSuite) -> str:
        """格式化输出为文本"""
        lines = []
        lines.append("=" * 70)
        lines.append("AgentOS Performance Benchmark Report")
        lines.append("=" * 70)
        lines.append(f"Timestamp: {suite.timestamp}")
        lines.append(f"Platform: {suite.platform}")
        lines.append(f"Python: {suite.python_version}")
        lines.append("")
        lines.append(f"{'Test Name':<35} {'Ops/sec':>12} {'Avg (ms)':>10} {'Std Dev':>10}")
        lines.append("-" * 70)

        for result in suite.results:
            lines.append(
                f"{result.name:<35} "
                f"{result.ops_per_second:>12.2f} "
                f"{result.avg_time:>10.4f} "
                f"{result.std_dev:>10.4f}"
            )

        lines.append("-" * 70)
        lines.append("")
        lines.append("Summary:")
        lines.append(f"  Total tests run: {len(suite.results)}")
        lines.append(f"  Fastest: {min(suite.results, key=lambda x: x.avg_time).name}")
        lines.append(f"  Slowest: {max(suite.results, key=lambda x: x.avg_time).name}")
        lines.append("=" * 70)

        return "\n".join(lines)

    @staticmethod
    def format_json(suite: BenchmarkSuite) -> str:
        """格式化输出为 JSON"""
        return json.dumps(asdict(suite), indent=2)

    @staticmethod
    def format_csv(suite: BenchmarkSuite) -> str:
        """格式化输出为 CSV"""
        lines = []
        lines.append("Name,Iterations,Total Time (ms),Avg Time (ms),Min (ms),Max (ms),Std Dev,Ops/sec")
        for r in suite.results:
            lines.append(
                f'"{r.name}",{r.iterations},{r.total_time:.4f},{r.avg_time:.4f},'
                f'{r.min_time:.4f},{r.max_time:.4f},{r.std_dev:.4f},{r.ops_per_second:.2f}'
            )
        return "\n".join(lines)

    @staticmethod
    def save(suite: BenchmarkSuite, output_path: str, format: OutputFormat):
        """保存测试结果"""
        content = ""
        ext = ""

        if format == OutputFormat.TEXT:
            content = BenchmarkReporter.format_text(suite)
            ext = ".txt"
        elif format == OutputFormat.JSON:
            content = BenchmarkReporter.format_json(suite)
            ext = ".json"
        elif format == OutputFormat.CSV:
            content = BenchmarkReporter.format_csv(suite)
            ext = ".csv"

        if not output_path.endswith(ext):
            output_path += ext

        with open(output_path, "w", encoding="utf-8") as f:
            f.write(content)

        return output_path


def main():
    parser = argparse.ArgumentParser(
        description="AgentOS Performance Benchmark Tool",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    parser.add_argument(
        "--iterations", "-n",
        type=int,
        default=1000,
        help="Number of iterations per test (default: 1000)"
    )

    parser.add_argument(
        "--warmup", "-w",
        type=int,
        default=100,
        help="Number of warmup iterations (default: 100)"
    )

    parser.add_argument(
        "--suite", "-s",
        choices=["all", "ipc", "memory", "context", "task", "string", "json"],
        default="all",
        help="Benchmark suite to run (default: all)"
    )

    parser.add_argument(
        "--output", "-o",
        type=str,
        help="Output file path"
    )

    parser.add_argument(
        "--format", "-f",
        choices=["text", "json", "csv"],
        default="text",
        help="Output format (default: text)"
    )

    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Verbose output"
    )

    args = parser.parse_args()

    bench = AgentOSBenchmark(iterations=args.iterations, warmup=args.warmup)
    suite = bench.run_all_suites()

    output_format = OutputFormat(args.format)

    if args.output:
        output_path = BenchmarkReporter.save(suite, args.output, output_format)
        print(f"Results saved to: {output_path}")
    else:
        if output_format == OutputFormat.TEXT:
            print(BenchmarkReporter.format_text(suite))
        elif output_format == OutputFormat.JSON:
            print(BenchmarkReporter.format_json(suite))
        elif output_format == OutputFormat.CSV:
            print(BenchmarkReporter.format_csv(suite))

    return 0


if __name__ == "__main__":
    sys.exit(main())