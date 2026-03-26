"""
AgentOS 性能回归检测框架
Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
Version: 1.0.0

基于历史基准的性能回归检测系统
用于检测性能退化并生成趋势报告
"""

import json
import os
import time
import statistics
from datetime import datetime
from typing import Any, Dict, List, Optional
from dataclasses import dataclass, field
from pathlib import Path


@dataclass
class PerformanceMetric:
    """性能指标数据类"""
    name: str
    value: float
    unit: str
    timestamp: str = field(default_factory=lambda: datetime.now().isoformat())
    metadata: Dict[str, Any] = field(default_factory=dict)


@dataclass
class PerformanceThreshold:
    """性能阈值配置"""
    metric_name: str
    baseline_value: float
    max_regression_percent: float = 10.0
    min_improvement_percent: float = -5.0
    enabled: bool = True


class PerformanceBaseline:
    """
    性能基线管理器
    
    管理历史性能基线数据，支持基线更新和比较。
    """
    
    def __init__(self, baseline_dir: Optional[str] = None):
        """
        初始化性能基线管理器
        
        Args:
            baseline_dir: 基线数据存储目录
        """
        self.baseline_dir = Path(baseline_dir or "tests/reports/baselines")
        self.baseline_dir.mkdir(parents=True, exist_ok=True)
        self.baseline_file = self.baseline_dir / "performance_baseline.json"
        self.baselines: Dict[str, List[PerformanceMetric]] = {}
        self.thresholds: Dict[str, PerformanceThreshold] = {}
        
        self._load_baselines()
        self._init_default_thresholds()
    
    def _load_baselines(self):
        """加载历史基线数据"""
        if self.baseline_file.exists():
            try:
                with open(self.baseline_file, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                    for name, metrics in data.get('baselines', {}).items():
                        self.baselines[name] = [
                            PerformanceMetric(**m) for m in metrics
                        ]
            except (json.JSONDecodeError, KeyError):
                self.baselines = {}
    
    def _save_baselines(self):
        """保存基线数据"""
        data = {
            'version': '1.0.0',
            'updated': datetime.now().isoformat(),
            'baselines': {
                name: [
                    {
                        'name': m.name,
                        'value': m.value,
                        'unit': m.unit,
                        'timestamp': m.timestamp,
                        'metadata': m.metadata
                    }
                    for m in metrics
                ]
                for name, metrics in self.baselines.items()
            }
        }
        
        with open(self.baseline_file, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=2, ensure_ascii=False)
    
    def _init_default_thresholds(self):
        """初始化默认阈值"""
        default_thresholds = [
            PerformanceThreshold("latency_p50", 100.0, 15.0),
            PerformanceThreshold("latency_p95", 500.0, 20.0),
            PerformanceThreshold("latency_p99", 1000.0, 25.0),
            PerformanceThreshold("throughput", 1000.0, -10.0),
            PerformanceThreshold("memory_peak_mb", 512.0, 20.0),
            PerformanceThreshold("cpu_percent", 80.0, 15.0),
        ]
        
        for threshold in default_thresholds:
            self.thresholds[threshold.metric_name] = threshold
    
    def update_baseline(self, metric: PerformanceMetric):
        """
        更新性能基线
        
        Args:
            metric: 新的性能指标
        """
        if metric.name not in self.baselines:
            self.baselines[metric.name] = []
        
        self.baselines[metric.name].append(metric)
        
        if len(self.baselines[metric.name]) > 100:
            self.baselines[metric.name] = self.baselines[metric.name][-100:]
        
        self._save_baselines()
    
    def get_baseline_value(self, metric_name: str) -> Optional[float]:
        """
        获取基线值
        
        Args:
            metric_name: 指标名称
            
        Returns:
            基线值（最近10次的中位数）
        """
        if metric_name not in self.baselines:
            return None
        
        recent = self.baselines[metric_name][-10:]
        if not recent:
            return None
        
        return statistics.median([m.value for m in recent])


class PerformanceRegressionDetector:
    """
    性能回归检测器
    
    检测性能回归并生成报告。
    """
    
    def __init__(self, baseline_manager: PerformanceBaseline):
        """
        初始化回归检测器
        
        Args:
            baseline_manager: 基线管理器
        """
        self.baseline = baseline_manager
        self.results: List[Dict[str, Any]] = []
    
    def check_regression(
        self,
        metric_name: str,
        current_value: float
    ) -> Dict[str, Any]:
        """
        检查性能回归
        
        Args:
            metric_name: 指标名称
            current_value: 当前值
            
        Returns:
            检测结果
        """
        baseline_value = self.baseline.get_baseline_value(metric_name)
        
        if baseline_value is None:
            return {
                "status": "no_baseline",
                "metric": metric_name,
                "current_value": current_value,
                "message": "无历史基线数据"
            }
        
        threshold = self.baseline.thresholds.get(metric_name)
        
        if threshold is None or not threshold.enabled:
            return {
                "status": "skipped",
                "metric": metric_name,
                "current_value": current_value,
                "baseline_value": baseline_value,
                "message": "阈值检查已禁用"
            }
        
        if baseline_value == 0:
            change_percent = 0
        else:
            change_percent = ((current_value - baseline_value) / baseline_value) * 100
        
        is_regression = change_percent > threshold.max_regression_percent
        is_improvement = change_percent < threshold.min_improvement_percent
        
        if is_regression:
            status = "regression"
            severity = "high" if change_percent > threshold.max_regression_percent * 2 else "medium"
        elif is_improvement:
            status = "improvement"
            severity = "info"
        else:
            status = "stable"
            severity = "info"
        
        result = {
            "status": status,
            "severity": severity,
            "metric": metric_name,
            "current_value": current_value,
            "baseline_value": baseline_value,
            "change_percent": round(change_percent, 2),
            "threshold_percent": threshold.max_regression_percent,
            "message": self._generate_message(status, change_percent, threshold)
        }
        
        self.results.append(result)
        return result
    
    def _generate_message(
        self,
        status: str,
        change_percent: float,
        threshold: PerformanceThreshold
    ) -> str:
        """生成状态消息"""
        if status == "regression":
            return f"⚠️ 性能回归: +{change_percent:.1f}% (阈值: {threshold.max_regression_percent}%)"
        elif status == "improvement":
            return f"✅ 性能提升: {change_percent:.1f}%"
        else:
            return f"✓ 性能稳定: {change_percent:+.1f}%"
    
    def generate_report(self) -> str:
        """
        生成回归检测报告
        
        Returns:
            Markdown格式的报告
        """
        report = [
            "# 📈 性能回归检测报告",
            "",
            f"**检测时间**: {datetime.now().isoformat()}",
            "",
            "## 检测结果摘要",
            "",
            "| 指标 | 状态 | 当前值 | 基线值 | 变化 |",
            "|------|------|--------|--------|------|",
        ]
        
        for result in self.results:
            status_emoji = {
                "regression": "❌",
                "improvement": "✅",
                "stable": "✓",
                "no_baseline": "❓",
                "skipped": "⏭️"
            }.get(result["status"], "?")
            
            baseline = result.get("baseline_value", "N/A")
            change = result.get("change_percent", "N/A")
            
            if isinstance(change, float):
                change_str = f"{change:+.1f}%"
            else:
                change_str = str(change)
            
            report.append(
                f"| {result['metric']} | {status_emoji} {result['status']} | "
                f"{result['current_value']:.2f} | {baseline} | {change_str} |"
            )
        
        regressions = [r for r in self.results if r["status"] == "regression"]
        
        report.extend([
            "",
            "## 总结",
            "",
            f"- 总检测指标: {len(self.results)}",
            f"- 回归: {len(regressions)}",
            f"- 稳定: {len([r for r in self.results if r['status'] == 'stable'])}",
            f"- 改进: {len([r for r in self.results if r['status'] == 'improvement'])}",
        ])
        
        if regressions:
            report.extend([
                "",
                "## ⚠️ 需要关注的回归",
                ""
            ])
            for r in regressions:
                report.append(f"- **{r['metric']}**: {r['message']}")
        
        return "\n".join(report)


class BenchmarkRunner:
    """
    基准测试运行器
    
    运行性能基准测试并收集指标。
    """
    
    def __init__(self, baseline_manager: PerformanceBaseline):
        """
        初始化基准测试运行器
        
        Args:
            baseline_manager: 基线管理器
        """
        self.baseline = baseline_manager
        self.detector = PerformanceRegressionDetector(baseline_manager)
    
    def run_latency_benchmark(self, iterations: int = 100) -> Dict[str, float]:
        """
        运行延迟基准测试
        
        Args:
            iterations: 迭代次数
            
        Returns:
            延迟指标
        """
        latencies = []
        
        for _ in range(iterations):
            start = time.perf_counter()
            time.sleep(0.001)
            end = time.perf_counter()
            latencies.append((end - start) * 1000)
        
        latencies.sort()
        
        metrics = {
            "latency_p50": latencies[int(len(latencies) * 0.5)],
            "latency_p95": latencies[int(len(latencies) * 0.95)],
            "latency_p99": latencies[int(len(latencies) * 0.99)],
            "latency_avg": statistics.mean(latencies),
        }
        
        return metrics
    
    def run_throughput_benchmark(self, duration_seconds: float = 5.0) -> float:
        """
        运行吞吐量基准测试
        
        Args:
            duration_seconds: 测试持续时间
            
        Returns:
            每秒操作数
        """
        start_time = time.time()
        operations = 0
        
        while time.time() - start_time < duration_seconds:
            _ = [i * i for i in range(100)]
            operations += 1
        
        return operations / duration_seconds
    
    def run_full_benchmark(self) -> Dict[str, Any]:
        """
        运行完整基准测试
        
        Returns:
            测试结果
        """
        results = {
            "timestamp": datetime.now().isoformat(),
            "metrics": {},
            "regressions": []
        }
        
        latency_metrics = self.run_latency_benchmark()
        for name, value in latency_metrics.items():
            metric = PerformanceMetric(name=name, value=value, unit="ms")
            self.baseline.update_baseline(metric)
            regression = self.detector.check_regression(name, value)
            results["metrics"][name] = value
            if regression["status"] == "regression":
                results["regressions"].append(regression)
        
        throughput = self.run_throughput_benchmark()
        metric = PerformanceMetric(name="throughput", value=throughput, unit="ops/s")
        self.baseline.update_baseline(metric)
        regression = self.detector.check_regression("throughput", throughput)
        results["metrics"]["throughput"] = throughput
        if regression["status"] == "regression":
            results["regressions"].append(regression)
        
        return results


def run_performance_regression_check():
    """
    运行性能回归检测
    
    Returns:
        检测结果
    """
    baseline_manager = PerformanceBaseline()
    runner = BenchmarkRunner(baseline_manager)
    
    results = runner.run_full_benchmark()
    report = runner.detector.generate_report()
    
    return {
        "results": results,
        "report": report,
        "has_regressions": len(results["regressions"]) > 0
    }


if __name__ == "__main__":
    print("=" * 60)
    print("AgentOS 性能回归检测框架")
    print("Copyright (c) 2026 SPHARX Ltd.")
    print("=" * 60)
    
    check_result = run_performance_regression_check()
    
    print("\n" + check_result["report"])
    
    if check_result["has_regressions"]:
        print("\n⚠️ 检测到性能回归，请检查！")
        exit(1)
    else:
        print("\n✅ 性能检测通过！")
