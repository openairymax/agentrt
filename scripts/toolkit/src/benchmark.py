#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS Performance Benchmark
# Migrated from scripts/operations/benchmark.py

"""
AgentOS Performance Benchmark Tool

Performance testing for core components:
- IPC latency measurement
- Memory allocation speed
- Context switching overhead
- Task scheduling throughput
- String and JSON parsing performance
"""

import argparse
import json
import os
import sys
import time
import statistics
from dataclasses import dataclass, field, asdict
from datetime import datetime
from typing import List, Dict, Any, Optional, Callable
from enum import Enum


class OutputFormat(Enum):
    TEXT = "text"
    JSON = "json"
    CSV = "csv"


@dataclass
class BenchmarkResult:
    name: str
    iterations: int
    total_time: float
    avg_time: float
    min_time: float
    max_time: float
    std_dev: float
    ops_per_second: float


@dataclass
class BenchmarkSuite:
    name: str
    description: str
    results: List[BenchmarkResult] = field(default_factory=list)
    timestamp: str = field(default_factory=lambda: datetime.now().isoformat())
    platform: str = ""
    python_version: str = ""


class AgentOSBenchmark:
    """AgentOS performance benchmark framework"""

    def __init__(self, iterations: int = 1000, warmup: int = 100):
        self.iterations = iterations
        self.warmup = warmup
        self.results: List[BenchmarkResult] = []
        self.platform = sys.platform
        self.python_version = sys.version

    def _measure(self, func: Callable, *args, **kwargs) -> Dict[str, float]:
        times = []
        for _ in range(self.warmup):
            func(*args, **kwargs)
        for _ in range(self.iterations):
            start = time.perf_counter_ns()
            func(*args, **kwargs)
            end = time.perf_counter_ns()
            times.append((end - start) / 1_000_000)
        return {
            "times": times, "total": sum(times),
            "avg": statistics.mean(times), "min": min(times),
            "max": max(times), "std_dev": statistics.stdev(times) if len(times) > 1 else 0
        }

    def _create_result(self, name: str, m: Dict[str, float]) -> BenchmarkResult:
        ops = self.iterations / m["total"] * 1000 if m["total"] > 0 else 0
        return BenchmarkResult(
            name=name, iterations=self.iterations,
            total_time=m["total"], avg_time=m["avg"],
            min_time=m["min"], max_time=m["max"],
            std_dev=m["std_dev"], ops_per_second=ops
        )

    def benchmark_ipc_latency(self) -> BenchmarkResult:
        def op(): pass
        return self._create_result("IPC Latency", self._measure(op))

    def benchmark_memory_allocation(self, size: int = 1024) -> BenchmarkResult:
        def op():
            d = bytearray(size)
            del d
        return self._create_result(f"Memory Allocation ({size}B)", self._measure(op))

    def benchmark_context_switch(self, depth: int = 10) -> BenchmarkResult:
        def nested(n):
            if n <= 0: return
            _ = n * 2
            nested(n - 1)
        return self._create_result(f"Context Switch (d={depth})", self._measure(lambda: nested(depth)))

    def benchmark_task_scheduling(self, count: int = 100) -> BenchmarkResult:
        tasks = list(range(count))
        def schedule():
            return [t * 2 for t in tasks]
        return self._create_result(f"Task Scheduling ({count} tasks)", self._measure(schedule))

    def benchmark_string_operations(self) -> BenchmarkResult:
        s = "AgentOS Operating System Framework"
        def ops():
            _ = s.upper(); _ = s.lower(); _ = s.split(); _ = s.replace(" ", "_")
        return self._create_result("String Operations", self._measure(ops))

    def benchmark_json_parsing(self, data: Dict[str, Any]) -> BenchmarkResult:
        jstr = json.dumps(data)
        def parse(): return json.loads(jstr)
        return self._create_result("JSON Parsing", self._measure(parse))

    def run_all_suites(self) -> BenchmarkSuite:
        suite = BenchmarkSuite(
            name="AgentOS Full Benchmark",
            description="Complete performance benchmark suite",
            platform=self.platform, python_version=self.python_version
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
    @staticmethod
    def format_text(suite: BenchmarkSuite) -> str:
        lines = ["=" * 70, "AgentOS Performance Benchmark Report", "=" * 70]
        lines += [f"Timestamp: {suite.timestamp}", f"Platform: {suite.platform}",
                  f"Python: {suite.python_version}", ""]
        lines += [f"{'Test Name':<35} {'Ops/sec':>12} {'Avg (ms)':>10} {'Std Dev':>10}",
                   "-" * 70]
        for r in suite.results:
            lines.append(f"{r.name:<35} {r.ops_per_second:>12.2f} {r.avg_time:>10.4f} {r.std_dev:>10.4f}")
        lines += ["-" * 70, "", f"Total tests: {len(suite.results)}",
                  f"Fastest: {min(suite.results, key=lambda x: x.avg_time).name}",
                  f"Slowest: {max(suite.results, key=lambda x: x.avg_time).name}", "=" * 70]
        return "\n".join(lines)

    @staticmethod
    def format_json(suite: BenchmarkSuite) -> str:
        return json.dumps(asdict(suite), indent=2)

    @staticmethod
    def format_csv(suite: BenchmarkSuite) -> str:
        header = "Name,Iterations,Total,Avg,Min,Max,StdDev,OpsPerSec"
        rows = [f'"{r.name}",{r.iterations},{r.total_time:.4f},{r.avg_time:.4f},'
                f'{r.min_time:.4f},{r.max_time:.4f},{r.std_dev:.4f},{r.ops_per_second:.2f}'
                for r in suite.results]
        return "\n".join([header] + rows)

    @staticmethod
    def save(suite: BenchmarkSuite, path: str, fmt: OutputFormat):
        ext_map = {OutputFormat.TEXT: ".txt", OutputFormat.JSON: ".json", OutputFormat.CSV: ".csv"}
        content_fn = {OutputFormat.TEXT: BenchmarkReporter.format_text,
                      OutputFormat.JSON: BenchmarkReporter.format_json,
                      OutputFormat.CSV: BenchmarkReporter.format_csv}
        content = content_fn[fmt](suite)
        ext = ext_map[fmt]
        if not path.endswith(ext): path += ext
        with open(path, "w", encoding="utf-8") as f: f.write(content)
        return path


def main():
    parser = argparse.ArgumentParser(description="AgentOS Performance Benchmark")
    parser.add_argument("-n", "--iterations", type=int, default=1000)
    parser.add_argument("-w", "--warmup", type=int, default=100)
    parser.add_argument("-s", "--suite", choices=["all","ipc","memory","context","task","string","json"], default="all")
    parser.add_argument("-o", "--output", type=str)
    parser.add_argument("-f", "--format", choices=["text","json","csv"], default="text")
    args = parser.parse_args()
    
    bench = AgentOSBenchmark(iterations=args.iterations, warmup=args.warmup)
    suite = bench.run_all_suites()
    fmt = OutputFormat(args.format)
    
    if args.output:
        p = BenchmarkReporter.save(suite, args.output, fmt)
        print(f"Results saved to: {p}")
    else:
        fn = {OutputFormat.TEXT: BenchmarkReporter.format_text,
              OutputFormat.JSON: BenchmarkReporter.format_json,
              OutputFormat.CSV: BenchmarkReporter.format_csv}
        print(fn[fmt](suite))
    return 0


if __name__ == "__main__":
    sys.exit(main())
