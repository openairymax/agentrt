#!/usr/bin/env python3
"""
AgentOS 性能基准测试框架核心模块

提供统一的基准测试框架API，支持测试定义、执行、监控和结果收集。
本模块是性能基准测试框架的基础，所有基准测试都应基于此框架构建。

设计原则：
1. 统一接口 - 所有基准测试使用相同的API定义和执行
2. 可扩展性 - 支持自定义测试场景和指标收集
3. 可重复性 - 确保测试结果的可重复性和可比性
4. 实时监控 - 实时收集性能指标和系统状态

@version v1.0.0
@date 2026-04-11
@copyright (c) 2026 SPHARX. All Rights Reserved.
"""

import abc
import asyncio
import concurrent.futures
import dataclasses
import datetime
import enum
import json
import logging
import os
import statistics
import sys
import time
import typing
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Tuple, Union

# 配置日志
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


class BenchmarkPhase(enum.Enum):
    """基准测试阶段"""
    INITIALIZATION = "initialization"      # 初始化阶段
    WARMUP = "warmup"                      # 预热阶段
    MEASUREMENT = "measurement"            # 测量阶段
    COOLDOWN = "cooldown"                  # 冷却阶段
    CLEANUP = "cleanup"                    # 清理阶段


class BenchmarkStatus(enum.Enum):
    """基准测试状态"""
    PENDING = "pending"                    # 待开始
    RUNNING = "running"                    # 运行中
    COMPLETED = "completed"                # 已完成
    FAILED = "failed"                      # 失败
    CANCELLED = "cancelled"                # 取消


class MetricType(enum.Enum):
    """指标类型"""
    THROUGHPUT = "throughput"              # 吞吐量 (ops/s)
    LATENCY = "latency"                    # 延迟 (ms)
    CPU_USAGE = "cpu_usage"                # CPU使用率 (%)
    MEMORY_USAGE = "memory_usage"          # 内存使用 (MB)
    IO_THROUGHPUT = "io_throughput"        # I/O吞吐量 (MB/s)
    NETWORK_THROUGHPUT = "network_throughput"  # 网络吞吐量 (MB/s)
    ERROR_RATE = "error_rate"              # 错误率 (%)
    CUSTOM = "custom"                      # 自定义指标


@dataclasses.dataclass
class BenchmarkMetric:
    """基准测试指标"""
    name: str                              # 指标名称
    value: float                           # 指标值
    unit: str                              # 单位
    metric_type: MetricType                # 指标类型
    timestamp: datetime.datetime           # 时间戳
    metadata: Dict[str, Any] = dataclasses.field(default_factory=dict)  # 元数据


@dataclasses.dataclass
class BenchmarkResult:
    """基准测试结果"""
    test_id: str                           # 测试ID
    test_name: str                         # 测试名称
    start_time: datetime.datetime          # 开始时间
    end_time: datetime.datetime            # 结束时间
    status: BenchmarkStatus                # 测试状态
    metrics: List[BenchmarkMetric]         # 指标列表
    metadata: Dict[str, Any] = dataclasses.field(default_factory=dict)  # 元数据
    error_message: Optional[str] = None    # 错误信息
    
    @property
    def duration(self) -> float:
        """测试持续时间（秒）"""
        return (self.end_time - self.start_time).total_seconds()
    
    def to_dict(self) -> Dict[str, Any]:
        """转换为字典格式"""
        return {
            "test_id": self.test_id,
            "test_name": self.test_name,
            "start_time": self.start_time.isoformat(),
            "end_time": self.end_time.isoformat(),
            "duration": self.duration,
            "status": self.status.value,
            "metrics": [
                {
                    "name": metric.name,
                    "value": metric.value,
                    "unit": metric.unit,
                    "type": metric.metric_type.value,
                    "timestamp": metric.timestamp.isoformat(),
                    "metadata": metric.metadata
                }
                for metric in self.metrics
            ],
            "metadata": self.metadata,
            "error_message": self.error_message
        }
    
    def to_json(self, indent: int = 2) -> str:
        """转换为JSON格式"""
        return json.dumps(self.to_dict(), indent=indent, ensure_ascii=False)


@dataclasses.dataclass
class BenchmarkConfig:
    """基准测试配置"""
    test_name: str                         # 测试名称
    description: str = ""                  # 测试描述
    warmup_duration: float = 5.0           # 预热时长（秒）
    measurement_duration: float = 30.0     # 测量时长（秒）
    cooldown_duration: float = 5.0         # 冷却时长（秒）
    concurrency_level: int = 1             # 并发级别
    iteration_count: int = 1               # 迭代次数
    target_throughput: Optional[float] = None  # 目标吞吐量（ops/s）
    timeout_seconds: float = 300.0         # 超时时长（秒）
    enable_cpu_monitoring: bool = True     # 启用CPU监控
    enable_memory_monitoring: bool = True  # 启用内存监控
    enable_io_monitoring: bool = False     # 启用I/O监控
    enable_network_monitoring: bool = False  # 启用网络监控
    custom_metrics: List[str] = dataclasses.field(default_factory=list)  # 自定义指标
    metadata: Dict[str, Any] = dataclasses.field(default_factory=dict)  # 元数据


class BenchmarkContext:
    """基准测试上下文"""
    
    def __init__(self, config: BenchmarkConfig):
        self.config = config
        self.start_time: Optional[datetime.datetime] = None
        self.end_time: Optional[datetime.datetime] = None
        self.status: BenchmarkStatus = BenchmarkStatus.PENDING
        self.metrics: List[BenchmarkMetric] = []
        self.phase: BenchmarkPhase = BenchmarkPhase.INITIALIZATION
        self._phase_start_time: Dict[BenchmarkPhase, datetime.datetime] = {}
        self._metrics_lock = asyncio.Lock()
        
    def set_phase(self, phase: BenchmarkPhase):
        """设置当前阶段"""
        old_phase = self.phase
        self.phase = phase
        self._phase_start_time[phase] = datetime.datetime.now()
        logger.info(f"基准测试阶段转换: {old_phase.value} -> {phase.value}")
        
    def add_metric(self, metric: BenchmarkMetric):
        """添加指标"""
        self.metrics.append(metric)
        
    def get_metrics_by_type(self, metric_type: MetricType) -> List[BenchmarkMetric]:
        """按类型获取指标"""
        return [m for m in self.metrics if m.metric_type == metric_type]
    
    def get_latest_metric(self, metric_type: MetricType) -> Optional[BenchmarkMetric]:
        """获取指定类型的最新指标"""
        metrics = self.get_metrics_by_type(metric_type)
        return metrics[-1] if metrics else None
    
    def calculate_statistics(self, metric_type: MetricType) -> Dict[str, float]:
        """计算指定类型指标的统计信息"""
        metrics = self.get_metrics_by_type(metric_type)
        if not metrics:
            return {}
        
        values = [m.value for m in metrics]
        return {
            "count": len(values),
            "mean": statistics.mean(values) if len(values) > 0 else 0,
            "median": statistics.median(values) if len(values) > 0 else 0,
            "stddev": statistics.stdev(values) if len(values) > 1 else 0,
            "min": min(values) if values else 0,
            "max": max(values) if values else 0,
            "p50": statistics.quantiles(values, n=100)[49] if len(values) > 1 else values[0] if values else 0,
            "p95": statistics.quantiles(values, n=100)[94] if len(values) > 1 else values[0] if values else 0,
            "p99": statistics.quantiles(values, n=100)[98] if len(values) > 1 else values[0] if values else 0,
        }


class BenchmarkBase(abc.ABC):
    """基准测试基类"""
    
    def __init__(self, config: BenchmarkConfig):
        self.config = config
        self.context: Optional[BenchmarkContext] = None
        self._stop_event = asyncio.Event()
        
    @abc.abstractmethod
    async def setup(self, context: BenchmarkContext) -> None:
        """测试准备阶段"""
        pass
    
    @abc.abstractmethod
    async def run(self, context: BenchmarkContext) -> None:
        """测试执行阶段"""
        pass
    
    @abc.abstractmethod
    async def cleanup(self, context: BenchmarkContext) -> None:
        """测试清理阶段"""
        pass
    
    async def monitor_resources(self, context: BenchmarkContext):
        """监控系统资源"""
        import psutil
        
        async def monitor_cpu():
            while not self._stop_event.is_set():
                cpu_percent = psutil.cpu_percent(interval=1)
                context.add_metric(BenchmarkMetric(
                    name="cpu_usage",
                    value=cpu_percent,
                    unit="%",
                    metric_type=MetricType.CPU_USAGE,
                    timestamp=datetime.datetime.now(),
                    metadata={"interval": 1}
                ))
                await asyncio.sleep(1)
        
        async def monitor_memory():
            while not self._stop_event.is_set():
                memory = psutil.virtual_memory()
                context.add_metric(BenchmarkMetric(
                    name="memory_usage",
                    value=memory.used / 1024 / 1024,  # MB
                    unit="MB",
                    metric_type=MetricType.MEMORY_USAGE,
                    timestamp=datetime.datetime.now(),
                    metadata={
                        "total_mb": memory.total / 1024 / 1024,
                        "percent": memory.percent
                    }
                ))
                await asyncio.sleep(1)
        
        # 启动监控任务
        monitor_tasks = []
        if self.config.enable_cpu_monitoring:
            monitor_tasks.append(asyncio.create_task(monitor_cpu()))
        if self.config.enable_memory_monitoring:
            monitor_tasks.append(asyncio.create_task(monitor_memory()))
        
        return monitor_tasks
    
    async def execute(self) -> BenchmarkResult:
        """执行基准测试"""
        self.context = BenchmarkContext(self.config)
        self.context.status = BenchmarkStatus.RUNNING
        self.context.start_time = datetime.datetime.now()
        
        monitor_tasks = []
        
        try:
            # 初始化阶段
            self.context.set_phase(BenchmarkPhase.INITIALIZATION)
            await self.setup(self.context)
            
            # 启动资源监控
            monitor_tasks = await self.monitor_resources(self.context)
            
            # 预热阶段
            if self.config.warmup_duration > 0:
                self.context.set_phase(BenchmarkPhase.WARMUP)
                logger.info(f"预热阶段开始，持续 {self.config.warmup_duration} 秒")
                await asyncio.sleep(self.config.warmup_duration)
            
            # 测量阶段
            self.context.set_phase(BenchmarkPhase.MEASUREMENT)
            measurement_start = datetime.datetime.now()
            
            # 运行基准测试
            await self.run(self.context)
            
            # 确保达到最小测量时长
            elapsed = (datetime.datetime.now() - measurement_start).total_seconds()
            if elapsed < self.config.measurement_duration:
                remaining = self.config.measurement_duration - elapsed
                logger.info(f"等待达到最小测量时长，剩余 {remaining:.1f} 秒")
                await asyncio.sleep(remaining)
            
            # 冷却阶段
            if self.config.cooldown_duration > 0:
                self.context.set_phase(BenchmarkPhase.COOLDOWN)
                logger.info(f"冷却阶段开始，持续 {self.config.cooldown_duration} 秒")
                await asyncio.sleep(self.config.cooldown_duration)
            
            # 停止监控
            self._stop_event.set()
            for task in monitor_tasks:
                task.cancel()
            
            # 清理阶段
            self.context.set_phase(BenchmarkPhase.CLEANUP)
            await self.cleanup(self.context)
            
            self.context.status = BenchmarkStatus.COMPLETED
            
        except asyncio.CancelledError:
            self.context.status = BenchmarkStatus.CANCELLED
            logger.warning("基准测试被取消")
        except Exception as e:
            self.context.status = BenchmarkStatus.FAILED
            self.context.error_message = str(e)
            logger.error(f"基准测试失败: {e}", exc_info=True)
        finally:
            # 确保监控任务停止
            self._stop_event.set()
            for task in monitor_tasks:
                if not task.done():
                    task.cancel()
            
            self.context.end_time = datetime.datetime.now()
            
            # 构建结果
            result = BenchmarkResult(
                test_id=f"{self.config.test_name}_{int(time.time())}",
                test_name=self.config.test_name,
                start_time=self.context.start_time,
                end_time=self.context.end_time,
                status=self.context.status,
                metrics=self.context.metrics,
                metadata=self.config.metadata,
                error_message=self.context.error_message
            )
            
            return result


class ThroughputBenchmark(BenchmarkBase):
    """吞吐量基准测试"""
    
    def __init__(self, config: BenchmarkConfig, workload_func: Callable):
        super().__init__(config)
        self.workload_func = workload_func
        self._completed_operations = 0
        self._error_count = 0
        
    async def setup(self, context: BenchmarkContext) -> None:
        """准备阶段"""
        logger.info(f"准备吞吐量基准测试: {self.config.test_name}")
        self._completed_operations = 0
        self._error_count = 0
        
    async def run(self, context: BenchmarkContext) -> None:
        """执行阶段"""
        logger.info(f"开始吞吐量基准测试，并发级别: {self.config.concurrency_level}")
        
        measurement_start = datetime.datetime.now()
        
        # 创建并发任务
        tasks = []
        for i in range(self.config.concurrency_level):
            task = asyncio.create_task(self._worker(i, context))
            tasks.append(task)
        
        # 等待测量阶段结束
        try:
            await asyncio.sleep(self.config.measurement_duration)
        finally:
            # 取消所有工作线程
            for task in tasks:
                if not task.done():
                    task.cancel()
            
            # 等待任务结束
            await asyncio.gather(*tasks, return_exceptions=True)
        
        measurement_end = datetime.datetime.now()
        duration = (measurement_end - measurement_start).total_seconds()
        
        # 计算吞吐量
        throughput = self._completed_operations / duration if duration > 0 else 0
        error_rate = (self._error_count / max(self._completed_operations, 1)) * 100
        
        # 记录吞吐量指标
        context.add_metric(BenchmarkMetric(
            name="throughput",
            value=throughput,
            unit="ops/s",
            metric_type=MetricType.THROUGHPUT,
            timestamp=datetime.datetime.now(),
            metadata={
                "operations": self._completed_operations,
                "duration": duration,
                "concurrency": self.config.concurrency_level
            }
        ))
        
        # 记录错误率指标
        context.add_metric(BenchmarkMetric(
            name="error_rate",
            value=error_rate,
            unit="%",
            metric_type=MetricType.ERROR_RATE,
            timestamp=datetime.datetime.now(),
            metadata={
                "error_count": self._error_count,
                "total_operations": self._completed_operations
            }
        ))
        
        logger.info(f"吞吐量基准测试完成: {throughput:.2f} ops/s, 错误率: {error_rate:.2f}%")
        
    async def _worker(self, worker_id: int, context: BenchmarkContext):
        """工作线程"""
        logger.debug(f"工作线程 {worker_id} 启动")
        
        while not self._stop_event.is_set():
            try:
                # 执行工作负载
                start_time = time.perf_counter()
                await self.workload_func(worker_id, context)
                end_time = time.perf_counter()
                
                # 记录延迟
                latency_ms = (end_time - start_time) * 1000
                context.add_metric(BenchmarkMetric(
                    name=f"latency_worker_{worker_id}",
                    value=latency_ms,
                    unit="ms",
                    metric_type=MetricType.LATENCY,
                    timestamp=datetime.datetime.now(),
                    metadata={"worker_id": worker_id}
                ))
                
                self._completed_operations += 1
                
                # 如果设置了目标吞吐量，控制请求速率
                if self.config.target_throughput:
                    target_interval = 1.0 / (self.config.target_throughput / self.config.concurrency_level)
                    actual_interval = end_time - start_time
                    if actual_interval < target_interval:
                        await asyncio.sleep(target_interval - actual_interval)
                        
            except Exception as e:
                self._error_count += 1
                logger.error(f"工作线程 {worker_id} 执行失败: {e}")
                
    async def cleanup(self, context: BenchmarkContext) -> None:
        """清理阶段"""
        logger.info("清理吞吐量基准测试资源")


class BenchmarkRegistry:
    """基准测试注册表"""
    
    _instance = None
    _benchmarks: Dict[str, type] = {}
    
    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
        return cls._instance
    
    @classmethod
    def register(cls, name: str, benchmark_class: type):
        """注册基准测试类"""
        if name in cls._benchmarks:
            raise ValueError(f"基准测试 '{name}' 已注册")
        
        if not issubclass(benchmark_class, BenchmarkBase):
            raise TypeError(f"基准测试类必须继承自 BenchmarkBase")
        
        cls._benchmarks[name] = benchmark_class
        logger.info(f"注册基准测试: {name}")
    
    @classmethod
    def get_benchmark(cls, name: str) -> Optional[type]:
        """获取基准测试类"""
        return cls._benchmarks.get(name)
    
    @classmethod
    def list_benchmarks(cls) -> List[str]:
        """列出所有注册的基准测试"""
        return list(cls._benchmarks.keys())
    
    @classmethod
    def create_benchmark(cls, name: str, config: BenchmarkConfig, **kwargs) -> BenchmarkBase:
        """创建基准测试实例"""
        benchmark_class = cls.get_benchmark(name)
        if not benchmark_class:
            raise ValueError(f"基准测试 '{name}' 未注册")
        
        return benchmark_class(config, **kwargs)


# 注册内置基准测试
BenchmarkRegistry.register("throughput", ThroughputBenchmark)


class BenchmarkRunner:
    """基准测试运行器"""
    
    def __init__(self, output_dir: Optional[Path] = None):
        self.output_dir = output_dir or Path.cwd() / "benchmark_results"
        self.output_dir.mkdir(parents=True, exist_ok=True)
        self.results: List[BenchmarkResult] = []
        
    async def run_benchmark(self, benchmark: BenchmarkBase) -> BenchmarkResult:
        """运行单个基准测试"""
        logger.info(f"开始运行基准测试: {benchmark.config.test_name}")
        
        result = await benchmark.execute()
        self.results.append(result)
        
        # 保存结果
        self.save_result(result)
        
        # 打印摘要
        self.print_summary(result)
        
        return result
    
    async def run_benchmark_suite(self, benchmarks: List[BenchmarkBase]) -> List[BenchmarkResult]:
        """运行基准测试套件"""
        results = []
        
        for benchmark in benchmarks:
            result = await self.run_benchmark(benchmark)
            results.append(result)
            
            # 测试之间暂停一下
            await asyncio.sleep(2)
        
        return results
    
    def save_result(self, result: BenchmarkResult, filename: Optional[str] = None):
        """保存测试结果到文件"""
        if filename is None:
            timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = f"benchmark_{result.test_name}_{timestamp}.json"
        
        filepath = self.output_dir / filename
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(result.to_json())
        
        logger.info(f"测试结果已保存到: {filepath}")
        return filepath
    
    def load_result(self, filepath: Path) -> BenchmarkResult:
        """从文件加载测试结果"""
        with open(filepath, 'r', encoding='utf-8') as f:
            data = json.load(f)
        
        # 重建结果对象
        metrics = []
        for metric_data in data["metrics"]:
            metrics.append(BenchmarkMetric(
                name=metric_data["name"],
                value=metric_data["value"],
                unit=metric_data["unit"],
                metric_type=MetricType(metric_data["type"]),
                timestamp=datetime.datetime.fromisoformat(metric_data["timestamp"]),
                metadata=metric_data.get("metadata", {})
            ))
        
        result = BenchmarkResult(
            test_id=data["test_id"],
            test_name=data["test_name"],
            start_time=datetime.datetime.fromisoformat(data["start_time"]),
            end_time=datetime.datetime.fromisoformat(data["end_time"]),
            status=BenchmarkStatus(data["status"]),
            metrics=metrics,
            metadata=data.get("metadata", {}),
            error_message=data.get("error_message")
        )
        
        return result
    
    def print_summary(self, result: BenchmarkResult):
        """打印测试摘要"""
        print("\n" + "="*70)
        print(f"基准测试摘要: {result.test_name}")
        print("="*70)
        
        print(f"测试ID: {result.test_id}")
        print(f"状态: {result.status.value}")
        print(f"开始时间: {result.start_time}")
        print(f"结束时间: {result.end_time}")
        print(f"持续时间: {result.duration:.2f} 秒")
        
        if result.error_message:
            print(f"错误信息: {result.error_message}")
        
        print("\n指标汇总:")
        print("-"*40)
        
        # 按类型分组指标
        metrics_by_type: Dict[MetricType, List[BenchmarkMetric]] = {}
        for metric in result.metrics:
            metrics_by_type.setdefault(metric.metric_type, []).append(metric)
        
        for metric_type, metrics in metrics_by_type.items():
            values = [m.value for m in metrics]
            if not values:
                continue
                
            print(f"{metric_type.value}:")
            print(f"  样本数: {len(values)}")
            print(f"  平均值: {statistics.mean(values):.2f}")
            print(f"  中位数: {statistics.median(values):.2f}")
            print(f"  最小值: {min(values):.2f}")
            print(f"  最大值: {max(values):.2f}")
            
            if len(values) > 1:
                print(f"  标准差: {statistics.stdev(values):.2f}")
                
                # 计算百分位数
                quantiles = statistics.quantiles(values, n=100)
                if len(quantiles) >= 95:
                    print(f"  P95: {quantiles[94]:.2f}")
                if len(quantiles) >= 99:
                    print(f"  P99: {quantiles[98]:.2f}")
            
            print()
        
        print("="*70)


# 示例工作负载函数
async def example_workload(worker_id: int, context: BenchmarkContext):
    """示例工作负载函数"""
    # 模拟一些处理工作
    await asyncio.sleep(0.01)
    # 可以在这里添加实际的测试逻辑


# 命令行接口
def main():
    """命令行主函数"""
    import argparse
    
    parser = argparse.ArgumentParser(
        description="AgentOS 性能基准测试框架",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
使用示例:
  # 运行吞吐量基准测试
  python benchmark_core.py --test throughput --name example --duration 30
  
  # 自定义工作负载
  python benchmark_core.py --test throughput --name custom --duration 60 --concurrency 4
  
  # 保存结果到指定目录
  python benchmark_core.py --test throughput --name example --output ./results
        """
    )
    
    parser.add_argument(
        "--test", "-t",
        choices=BenchmarkRegistry.list_benchmarks(),
        default="throughput",
        help="基准测试类型"
    )
    
    parser.add_argument(
        "--name", "-n",
        default="benchmark_test",
        help="测试名称"
    )
    
    parser.add_argument(
        "--description", "-d",
        default="",
        help="测试描述"
    )
    
    parser.add_argument(
        "--duration", "-D",
        type=float,
        default=30.0,
        help="测量时长（秒）"
    )
    
    parser.add_argument(
        "--concurrency", "-c",
        type=int,
        default=1,
        help="并发级别"
    )
    
    parser.add_argument(
        "--warmup", "-w",
        type=float,
        default=5.0,
        help="预热时长（秒）"
    )
    
    parser.add_argument(
        "--cooldown", "-C",
        type=float,
        default=5.0,
        help="冷却时长（秒）"
    )
    
    parser.add_argument(
        "--output", "-o",
        type=Path,
        default=None,
        help="输出目录"
    )
    
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="详细输出"
    )
    
    args = parser.parse_args()
    
    # 配置日志级别
    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)
    
    # 创建测试配置
    config = BenchmarkConfig(
        test_name=args.name,
        description=args.description,
        warmup_duration=args.warmup,
        measurement_duration=args.duration,
        cooldown_duration=args.cooldown,
        concurrency_level=args.concurrency
    )
    
    # 创建基准测试
    benchmark = BenchmarkRegistry.create_benchmark(
        args.test,
        config,
        workload_func=example_workload
    )
    
    # 创建运行器并执行测试
    runner = BenchmarkRunner(args.output)
    
    # 运行测试
    asyncio.run(runner.run_benchmark(benchmark))
    
    print("\n基准测试完成！")


if __name__ == "__main__":
    main()