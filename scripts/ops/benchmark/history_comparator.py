#!/usr/bin/env python3
"""
AgentOS 性能基准测试历史比较器

提供历史测试结果的比较和分析功能：
1. 版本间性能对比
2. 回归检测和告警
3. 性能趋势分析
4. 统计显著性检验
5. 变化可视化

设计原则：
1. 准确性 - 使用科学的统计方法进行比较
2. 可操作性 - 提供具体的改进建议
3. 可视化 - 清晰的对比图表
4. 自动化 - 支持CI/CD集成
5. 可追溯性 - 完整的比较历史记录

@version v1.0.0
@date 2026-04-11
@copyright (c) 2026 SPHARX. All Rights Reserved.
"""

import json
import math
import statistics
from dataclasses import dataclass, field
from datetime import datetime, timedelta
from enum import Enum
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple, Union

import matplotlib.pyplot as plt
import numpy as np
from scipy import stats as scipy_stats


class ChangeSignificance(Enum):
    """变化显著性"""
    INSIGNIFICANT = "insignificant"      # 不显著变化
    SMALL = "small"                      # 小变化
    MEDIUM = "medium"                    # 中等变化
    LARGE = "large"                      # 大变化
    VERY_LARGE = "very_large"            # 极大变化


class ChangeDirection(Enum):
    """变化方向"""
    IMPROVEMENT = "improvement"          # 性能提升
    REGRESSION = "regression"            # 性能回退
    NO_CHANGE = "no_change"              # 无变化
    INCONCLUSIVE = "inconclusive"        # 不确定


class AlertLevel(Enum):
    """告警级别"""
    INFO = "info"                        # 信息
    WARNING = "warning"                  # 警告
    CRITICAL = "critical"                # 严重


@dataclass
class PerformanceChange:
    """性能变化"""
    metric_name: str                     # 指标名称
    baseline_value: float                # 基线值
    current_value: float                 # 当前值
    change_percentage: float             # 变化百分比
    change_absolute: float               # 绝对变化量
    change_direction: ChangeDirection    # 变化方向
    change_significance: ChangeSignificance  # 变化显著性
    confidence_interval: Tuple[float, float]  # 置信区间
    p_value: float                       # 统计检验p值
    effect_size: float                   # 效应大小
    is_regression: bool                  # 是否回归
    alert_level: AlertLevel              # 告警级别
    explanation: str                     # 解释说明
    recommendations: List[str]           # 改进建议
    metadata: Dict[str, Any] = field(default_factory=dict)  # 元数据
    
    def to_dict(self) -> Dict[str, Any]:
        """转换为字典格式"""
        return {
            "metric_name": self.metric_name,
            "baseline_value": self.baseline_value,
            "current_value": self.current_value,
            "change_percentage": self.change_percentage,
            "change_absolute": self.change_absolute,
            "change_direction": self.change_direction.value,
            "change_significance": self.change_significance.value,
            "confidence_interval": {
                "lower": self.confidence_interval[0],
                "upper": self.confidence_interval[1]
            },
            "p_value": self.p_value,
            "effect_size": self.effect_size,
            "is_regression": self.is_regression,
            "alert_level": self.alert_level.value,
            "explanation": self.explanation,
            "recommendations": self.recommendations,
            "metadata": self.metadata
        }


@dataclass
class ComparisonResult:
    """比较结果"""
    comparison_id: str                   # 比较ID
    baseline_id: str                     # 基线ID
    current_id: str                      # 当前ID
    baseline_version: str                # 基线版本
    current_version: str                 # 当前版本
    comparison_date: str                 # 比较日期
    changes: List[PerformanceChange]     # 性能变化列表
    summary: Dict[str, Any]              # 摘要信息
    alerts: List[Dict[str, Any]]         # 告警列表
    metadata: Dict[str, Any] = field(default_factory=dict)  # 元数据
    
    def to_dict(self) -> Dict[str, Any]:
        """转换为字典格式"""
        return {
            "comparison_id": self.comparison_id,
            "baseline_id": self.baseline_id,
            "current_id": self.current_id,
            "baseline_version": self.baseline_version,
            "current_version": self.current_version,
            "comparison_date": self.comparison_date,
            "changes": [change.to_dict() for change in self.changes],
            "summary": self.summary,
            "alerts": self.alerts,
            "metadata": self.metadata
        }
    
    def to_json(self, indent: int = 2) -> str:
        """转换为JSON格式"""
        return json.dumps(self.to_dict(), indent=indent, ensure_ascii=False)


@dataclass
class BenchmarkResult:
    """基准测试结果（简化）"""
    result_id: str                       # 结果ID
    test_name: str                       # 测试名称
    version: str                         # 版本
    timestamp: str                       # 时间戳
    metrics: Dict[str, List[float]]      # 指标数据（多采样）
    metadata: Dict[str, Any] = field(default_factory=dict)  # 元数据


class HistoryComparator:
    """历史比较器"""
    
    def __init__(self, results_dir: Optional[Path] = None):
        self.results_dir = results_dir or Path.cwd() / "benchmark_results"
        self.results_dir.mkdir(parents=True, exist_ok=True)
        
        self.results: Dict[str, BenchmarkResult] = {}
        self.comparisons: Dict[str, ComparisonResult] = {}
        
        # 加载现有结果
        self._load_results()
    
    def _load_results(self):
        """加载历史结果"""
        result_files = list(self.results_dir.glob("*.json"))
        
        for file_path in result_files:
            try:
                with open(file_path, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                
                # 转换为BenchmarkResult
                if 'test_id' in data:
                    result_id = data['test_id']
                    
                    # 提取指标数据
                    metrics = {}
                    for metric in data.get('metrics', []):
                        metric_name = metric.get('name', 'unknown')
                        if metric_name not in metrics:
                            metrics[metric_name] = []
                        metrics[metric_name].append(metric.get('value', 0))
                    
                    result = BenchmarkResult(
                        result_id=result_id,
                        test_name=data.get('test_name', 'unknown'),
                        version=data.get('metadata', {}).get('version', 'unknown'),
                        timestamp=data.get('start_time', ''),
                        metrics=metrics,
                        metadata=data.get('metadata', {})
                    )
                    
                    self.results[result_id] = result
                    
            except Exception as e:
                print(f"加载结果文件失败 {file_path}: {e}")
    
    def add_result(self, result_data: Dict[str, Any]) -> str:
        """添加测试结果"""
        # 生成结果ID
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        result_id = f"result_{timestamp}"
        
        # 创建BenchmarkResult
        metrics = {}
        for metric in result_data.get('metrics', []):
            metric_name = metric.get('name', 'unknown')
            if metric_name not in metrics:
                metrics[metric_name] = []
            metrics[metric_name].append(metric.get('value', 0))
        
        result = BenchmarkResult(
            result_id=result_id,
            test_name=result_data.get('test_name', 'unknown'),
            version=result_data.get('metadata', {}).get('version', 'unknown'),
            timestamp=result_data.get('start_time', ''),
            metrics=metrics,
            metadata=result_data.get('metadata', {})
        )
        
        # 保存结果
        self.results[result_id] = result
        self._save_result(result)
        
        return result_id
    
    def _save_result(self, result: BenchmarkResult):
        """保存结果到文件"""
        file_path = self.results_dir / f"{result.result_id}.json"
        
        # 构建保存数据
        save_data = {
            "result_id": result.result_id,
            "test_name": result.test_name,
            "version": result.version,
            "timestamp": result.timestamp,
            "metrics": [
                {
                    "name": metric_name,
                    "values": values,
                    "mean": statistics.mean(values) if values else 0,
                    "std": statistics.stdev(values) if len(values) > 1 else 0
                }
                for metric_name, values in result.metrics.items()
            ],
            "metadata": result.metadata
        }
        
        with open(file_path, 'w', encoding='utf-8') as f:
            json.dump(save_data, f, indent=2, ensure_ascii=False)
    
    def compare_results(self, 
                       baseline_id: str, 
                       current_id: str,
                       test_name: Optional[str] = None,
                       significance_level: float = 0.05) -> ComparisonResult:
        """比较两个测试结果"""
        if baseline_id not in self.results:
            raise ValueError(f"基线结果不存在: {baseline_id}")
        if current_id not in self.results:
            raise ValueError(f"当前结果不存在: {current_id}")
        
        baseline = self.results[baseline_id]
        current = self.results[current_id]
        
        # 如果指定了测试名称，只比较该测试
        if test_name and (baseline.test_name != test_name or current.test_name != test_name):
            raise ValueError("测试名称不匹配")
        
        # 比较所有公共指标
        changes = []
        
        # 获取公共指标
        common_metrics = set(baseline.metrics.keys()) & set(current.metrics.keys())
        
        for metric_name in common_metrics:
            baseline_values = baseline.metrics[metric_name]
            current_values = current.metrics[metric_name]
            
            if not baseline_values or not current_values:
                continue
            
            # 计算性能变化
            change = self._calculate_performance_change(
                metric_name=metric_name,
                baseline_values=baseline_values,
                current_values=current_values,
                significance_level=significance_level
            )
            
            changes.append(change)
        
        # 生成摘要
        summary = self._generate_summary(changes)
        
        # 生成告警
        alerts = self._generate_alerts(changes)
        
        # 创建比较结果
        comparison_id = f"comparison_{baseline_id}_{current_id}"
        
        comparison = ComparisonResult(
            comparison_id=comparison_id,
            baseline_id=baseline_id,
            current_id=current_id,
            baseline_version=baseline.version,
            current_version=current.version,
            comparison_date=datetime.now().isoformat(),
            changes=changes,
            summary=summary,
            alerts=alerts,
            metadata={
                "test_name": baseline.test_name,
                "significance_level": significance_level,
                "baseline_timestamp": baseline.timestamp,
                "current_timestamp": current.timestamp
            }
        )
        
        # 保存比较结果
        self.comparisons[comparison_id] = comparison
        self._save_comparison(comparison)
        
        return comparison
    
    def _calculate_performance_change(self,
                                    metric_name: str,
                                    baseline_values: List[float],
                                    current_values: List[float],
                                    significance_level: float = 0.05) -> PerformanceChange:
        """计算性能变化"""
        # 计算基本统计量
        baseline_mean = statistics.mean(baseline_values)
        current_mean = statistics.mean(current_values)
        
        baseline_std = statistics.stdev(baseline_values) if len(baseline_values) > 1 else 0
        current_std = statistics.stdev(current_values) if len(current_values) > 1 else 0
        
        # 计算变化
        if baseline_mean != 0:
            change_percentage = ((current_mean - baseline_mean) / abs(baseline_mean)) * 100
        else:
            change_percentage = 0 if current_mean == 0 else (current_mean > 0) * 100
        
        change_absolute = current_mean - baseline_mean
        
        # 确定变化方向
        if abs(change_percentage) < 0.1:  # 小于0.1%视为无变化
            change_direction = ChangeDirection.NO_CHANGE
        elif change_percentage > 0:
            # 对于延迟等指标，增加是回退；对于吞吐量等指标，增加是提升
            # 这里需要根据指标类型判断，暂时假设增加是提升
            change_direction = ChangeDirection.IMPROVEMENT
        else:
            change_direction = ChangeDirection.REGRESSION
        
        # 统计检验（独立样本t检验）
        t_stat, p_value = scipy_stats.ttest_ind(baseline_values, current_values, equal_var=False)
        
        # 计算效应大小（Cohen's d）
        pooled_std = math.sqrt(
            ((len(baseline_values) - 1) * baseline_std ** 2 + 
             (len(current_values) - 1) * current_std ** 2) /
            (len(baseline_values) + len(current_values) - 2)
        ) if (len(baseline_values) + len(current_values) > 2) else 0
        
        effect_size = abs(current_mean - baseline_mean) / pooled_std if pooled_std > 0 else 0
        
        # 确定变化显著性
        if effect_size < 0.2:
            change_significance = ChangeSignificance.INSIGNIFICANT
        elif effect_size < 0.5:
            change_significance = ChangeSignificance.SMALL
        elif effect_size < 0.8:
            change_significance = ChangeSignificance.MEDIUM
        elif effect_size < 1.2:
            change_significance = ChangeSignificance.LARGE
        else:
            change_significance = ChangeSignificance.VERY_LARGE
        
        # 计算置信区间
        if len(baseline_values) > 1 and len(current_values) > 1:
            se_diff = math.sqrt(
                (baseline_std ** 2 / len(baseline_values)) + 
                (current_std ** 2 / len(current_values))
            )
            
            t_critical = scipy_stats.t.ppf(1 - significance_level/2, 
                                         len(baseline_values) + len(current_values) - 2)
            
            margin = t_critical * se_diff
            ci_lower = change_absolute - margin
            ci_upper = change_absolute + margin
        else:
            ci_lower = ci_upper = change_absolute
        
        # 判断是否回归（性能下降）
        # 这里需要根据指标类型判断，暂时假设负变化是回归
        is_regression = change_percentage < -5  # 超过5%的负变化
        
        # 确定告警级别
        if is_regression and abs(change_percentage) > 20:
            alert_level = AlertLevel.CRITICAL
        elif is_regression and abs(change_percentage) > 10:
            alert_level = AlertLevel.WARNING
        elif is_regression:
            alert_level = AlertLevel.INFO
        elif abs(change_percentage) > 20:
            alert_level = AlertLevel.WARNING
        elif abs(change_percentage) > 10:
            alert_level = AlertLevel.INFO
        else:
            alert_level = AlertLevel.INFO
        
        # 生成解释和建议
        explanation, recommendations = self._generate_explanation_and_recommendations(
            metric_name=metric_name,
            change_percentage=change_percentage,
            change_direction=change_direction,
            change_significance=change_significance,
            p_value=p_value,
            is_regression=is_regression
        )
        
        return PerformanceChange(
            metric_name=metric_name,
            baseline_value=baseline_mean,
            current_value=current_mean,
            change_percentage=change_percentage,
            change_absolute=change_absolute,
            change_direction=change_direction,
            change_significance=change_significance,
            confidence_interval=(ci_lower, ci_upper),
            p_value=p_value,
            effect_size=effect_size,
            is_regression=is_regression,
            alert_level=alert_level,
            explanation=explanation,
            recommendations=recommendations,
            metadata={
                "baseline_samples": len(baseline_values),
                "current_samples": len(current_values),
                "baseline_std": baseline_std,
                "current_std": current_std
            }
        )
    
    def _generate_explanation_and_recommendations(self,
                                                metric_name: str,
                                                change_percentage: float,
                                                change_direction: ChangeDirection,
                                                change_significance: ChangeSignificance,
                                                p_value: float,
                                                is_regression: bool) -> Tuple[str, List[str]]:
        """生成解释和改进建议"""
        explanations = []
        recommendations = []
        
        # 指标特定解释
        metric_descriptions = {
            "throughput": "吞吐量表示系统每秒钟处理的操作数量",
            "latency": "延迟表示系统处理单个操作所需的时间",
            "cpu_usage": "CPU使用率表示处理器资源的利用程度",
            "memory_usage": "内存使用表示系统内存的占用情况",
            "error_rate": "错误率表示操作失败的比例"
        }
        
        metric_desc = metric_descriptions.get(metric_name, "性能指标")
        
        # 变化解释
        abs_change = abs(change_percentage)
        
        if change_direction == ChangeDirection.NO_CHANGE:
            explanations.append(f"{metric_desc} 没有显著变化（变化 {abs_change:.1f}%）。")
        elif change_direction == ChangeDirection.IMPROVEMENT:
            explanations.append(f"{metric_desc} 提升了 {abs_change:.1f}%，性能有所改善。")
            
            if change_significance == ChangeSignificance.LARGE:
                explanations.append("这是一个显著的性能提升。")
                recommendations.append(f"分析 {metric_name} 提升的原因，考虑是否可以应用到其他组件。")
            elif change_significance == ChangeSignificance.MEDIUM:
                explanations.append("这是一个中等程度的性能提升。")
        else:  # REGRESSION
            explanations.append(f"{metric_desc} 下降了 {abs_change:.1f}%，性能出现回退。")
            
            if is_regression:
                explanations.append("这被标记为性能回归，需要重点关注。")
                
                if change_significance == ChangeSignificance.LARGE:
                    recommendations.append(f"立即调查 {metric_name} 下降的根本原因。")
                    recommendations.append("检查最近的代码变更、配置更新或依赖库升级。")
                    recommendations.append("考虑回退到之前的版本以确认问题。")
                elif change_significance == ChangeSignificance.MEDIUM:
                    recommendations.append(f"调查 {metric_name} 下降的原因。")
                    recommendations.append("进行更详细的性能剖析以定位问题。")
        
        # 统计显著性
        if p_value < 0.05:
            explanations.append("变化在统计上是显著的（p值 < 0.05）。")
        else:
            explanations.append("变化在统计上不显著（p值 ≥ 0.05），可能是随机波动。")
            recommendations.append("建议增加测试样本量以获得更可靠的结果。")
        
        # 根据变化显著性添加建议
        if change_significance in [ChangeSignificance.LARGE, ChangeSignificance.VERY_LARGE]:
            if change_direction == ChangeDirection.IMPROVEMENT:
                recommendations.append("考虑将相关优化方案标准化。")
            else:
                recommendations.append("建议进行根本原因分析并制定修复计划。")
                recommendations.append("更新性能基准目标以反映当前性能水平。")
        
        explanation = " ".join(explanations)
        
        return explanation, recommendations
    
    def _generate_summary(self, changes: List[PerformanceChange]) -> Dict[str, Any]:
        """生成比较摘要"""
        if not changes:
            return {
                "total_metrics": 0,
                "improvements": 0,
                "regressions": 0,
                "no_changes": 0,
                "significant_changes": 0,
                "critical_alerts": 0,
                "warning_alerts": 0
            }
        
        improvements = sum(1 for c in changes if c.change_direction == ChangeDirection.IMPROVEMENT)
        regressions = sum(1 for c in changes if c.change_direction == ChangeDirection.REGRESSION)
        no_changes = sum(1 for c in changes if c.change_direction == ChangeDirection.NO_CHANGE)
        
        significant_changes = sum(1 for c in changes if 
                                c.change_significance in [ChangeSignificance.MEDIUM, 
                                                         ChangeSignificance.LARGE,
                                                         ChangeSignificance.VERY_LARGE])
        
        critical_alerts = sum(1 for c in changes if c.alert_level == AlertLevel.CRITICAL)
        warning_alerts = sum(1 for c in changes if c.alert_level == AlertLevel.WARNING)
        
        # 计算平均变化
        if changes:
            avg_change = statistics.mean([c.change_percentage for c in changes])
            max_improvement = max([c.change_percentage for c in changes if c.change_percentage > 0], default=0)
            max_regression = min([c.change_percentage for c in changes if c.change_percentage < 0], default=0)
        else:
            avg_change = max_improvement = max_regression = 0
        
        # 总体评价
        if critical_alerts > 0:
            overall_assessment = "存在严重性能回归，需要立即处理"
        elif warning_alerts > 0:
            overall_assessment = "存在性能警告，建议调查"
        elif improvements > regressions:
            overall_assessment = "性能总体有所改善"
        elif regressions > improvements:
            overall_assessment = "性能总体有所下降"
        else:
            overall_assessment = "性能基本稳定"
        
        return {
            "total_metrics": len(changes),
            "improvements": improvements,
            "regressions": regressions,
            "no_changes": no_changes,
            "significant_changes": significant_changes,
            "critical_alerts": critical_alerts,
            "warning_alerts": warning_alerts,
            "average_change_percentage": avg_change,
            "maximum_improvement": max_improvement,
            "maximum_regression": max_regression,
            "overall_assessment": overall_assessment
        }
    
    def _generate_alerts(self, changes: List[PerformanceChange]) -> List[Dict[str, Any]]:
        """生成告警列表"""
        alerts = []
        
        for change in changes:
            if change.alert_level in [AlertLevel.WARNING, AlertLevel.CRITICAL]:
                alert = {
                    "metric": change.metric_name,
                    "level": change.alert_level.value,
                    "change_percentage": change.change_percentage,
                    "description": change.explanation,
                    "recommendations": change.recommendations,
                    "is_regression": change.is_regression
                }
                alerts.append(alert)
        
        return alerts
    
    def _save_comparison(self, comparison: ComparisonResult):
        """保存比较结果"""
        comparisons_dir = self.results_dir / "comparisons"
        comparisons_dir.mkdir(exist_ok=True)
        
        file_path = comparisons_dir / f"{comparison.comparison_id}.json"
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(comparison.to_json())
    
    def find_baseline(self, 
                     current_version: str,
                     days_back: int = 30,
                     same_test: bool = True) -> Optional[str]:
        """查找合适的基线版本"""
        # 收集符合条件的候选结果
        candidates = []
        
        for result_id, result in self.results.items():
            # 跳过当前版本
            if result.version == current_version:
                continue
            
            # 检查时间范围
            try:
                result_date = datetime.fromisoformat(result.timestamp.replace('Z', '+00:00'))
                days_diff = (datetime.now() - result_date).days
                
                if days_diff > days_back:
                    continue
                
                candidates.append((result_id, result, days_diff))
            except:
                continue
        
        if not candidates:
            return None
        
        # 优先选择最近的结果
        candidates.sort(key=lambda x: x[2])  # 按时间差排序
        
        return candidates[0][0]  # 返回最近的结果ID
    
    def compare_with_latest(self, 
                          current_result_id: str,
                          days_back: int = 30) -> Optional[ComparisonResult]:
        """与最近的基线版本比较"""
        current_result = self.results.get(current_result_id)
        if not current_result:
            raise ValueError(f"当前结果不存在: {current_result_id}")
        
        # 查找基线
        baseline_id = self.find_baseline(
            current_version=current_result.version,
            days_back=days_back,
            same_test=True
        )
        
        if not baseline_id:
            print("未找到合适的基线版本")
            return None
        
        # 进行比较
        return self.compare_results(baseline_id, current_result_id)
    
    def detect_regression_trend(self, 
                              test_name: str,
                              metric_name: str,
                              window_size: int = 5) -> Dict[str, Any]:
        """检测回归趋势"""
        # 收集相关结果
        relevant_results = []
        
        for result_id, result in self.results.items():
            if result.test_name == test_name and metric_name in result.metrics:
                try:
                    timestamp = datetime.fromisoformat(result.timestamp.replace('Z', '+00:00'))
                    relevant_results.append((timestamp, result_id, result))
                except:
                    continue
        
        if len(relevant_results) < 2:
            return {"has_trend": False, "message": "数据不足"}
        
        # 按时间排序
        relevant_results.sort(key=lambda x: x[0])
        
        # 提取指标值
        timestamps = [r[0] for r in relevant_results]
        versions = [r[2].version for r in relevant_results]
        values = [statistics.mean(r[2].metrics[metric_name]) for r in relevant_results]
        
        # 计算趋势
        x = np.arange(len(values))
        slope, intercept, r_value, p_value, std_err = scipy_stats.linregress(x, values)
        
        # 判断趋势
        has_trend = p_value < 0.05
        trend_direction = "increasing" if slope > 0 else "decreasing"
        
        # 计算移动平均
        if len(values) >= window_size:
            moving_avg = np.convolve(values, np.ones(window_size)/window_size, mode='valid')
            recent_trend = "increasing" if moving_avg[-1] > moving_avg[0] else "decreasing" if moving_avg[-1] < moving_avg[0] else "stable"
        else:
            moving_avg = []
            recent_trend = "insufficient_data"
        
        # 检查回归（对于延迟等指标，增加是回归；对于吞吐量，减少是回归）
        # 这里需要根据指标类型判断，暂时假设增加是回归
        is_regressing = slope > 0
        
        return {
            "has_trend": has_trend,
            "trend_direction": trend_direction,
            "trend_slope": slope,
            "trend_p_value": p_value,
            "r_squared": r_value ** 2,
            "is_regressing": is_regressing,
            "recent_trend": recent_trend,
            "data_points": len(values),
            "timestamps": [ts.isoformat() for ts in timestamps],
            "versions": versions,
            "values": values,
            "moving_average": moving_avg.tolist() if len(moving_avg) > 0 else []
        }
    
    def generate_comparison_report(self, 
                                 comparison: ComparisonResult,
                                 output_dir: Optional[Path] = None) -> Path:
        """生成比较报告"""
        if output_dir is None:
            output_dir = self.results_dir / "reports"
        output_dir.mkdir(parents=True, exist_ok=True)
        
        file_path = output_dir / f"{comparison.comparison_id}_report.json"
        
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(comparison.to_json())
        
        # 生成可视化图表
        self._generate_comparison_charts(comparison, output_dir)
        
        return file_path
    
    def _generate_comparison_charts(self, comparison: ComparisonResult, output_dir: Path):
        """生成比较图表"""
        if not comparison.changes:
            return
        
        # 1. 变化百分比柱状图
        plt.figure(figsize=(12, 6))
        
        metric_names = [c.metric_name for c in comparison.changes]
        change_percentages = [c.change_percentage for c in comparison.changes]
        
        # 按变化大小排序
        sorted_data = sorted(zip(metric_names, change_percentages), key=lambda x: x[1])
        metric_names, change_percentages = zip(*sorted_data) if sorted_data else ([], [])
        
        # 颜色：正变化为绿色，负变化为红色
        colors = ['green' if p >= 0 else 'red' for p in change_percentages]
        
        plt.barh(metric_names, change_percentages, color=colors)
        plt.xlabel('变化百分比 (%)')
        plt.title(f'性能变化对比: {comparison.baseline_version} → {comparison.current_version}')
        plt.axvline(x=0, color='black', linestyle='-', linewidth=0.5)
        
        # 添加数值标签
        for i, (name, pct) in enumerate(zip(metric_names, change_percentages)):
            plt.text(pct, i, f'{pct:.1f}%', 
                    va='center', 
                    ha='left' if pct >= 0 else 'right',
                    fontsize=9)
        
        plt.tight_layout()
        chart_path = output_dir / f"{comparison.comparison_id}_changes.png"
        plt.savefig(chart_path, dpi=150, bbox_inches='tight')
        plt.close()
        
        # 2. 告警级别分布图
        if comparison.alerts:
            plt.figure(figsize=(8, 6))
            
            alert_levels = [alert['level'] for alert in comparison.alerts]
            level_counts = {
                'critical': alert_levels.count('critical'),
                'warning': alert_levels.count('warning'),
                'info': alert_levels.count('info')
            }
            
            labels = ['严重', '警告', '信息']
            sizes = [level_counts['critical'], level_counts['warning'], level_counts['info']]
            colors = ['red', 'orange', 'blue']
            
            plt.pie(sizes, labels=labels, colors=colors, autopct='%1.1f%%', startangle=90)
            plt.axis('equal')
            plt.title('告警级别分布')
            
            chart_path = output_dir / f"{comparison.comparison_id}_alerts.png"
            plt.savefig(chart_path, dpi=150, bbox_inches='tight')
            plt.close()
    
    def list_results(self, 
                    test_name: Optional[str] = None,
                    version: Optional[str] = None,
                    days_back: Optional[int] = None) -> List[Dict[str, Any]]:
        """列出测试结果"""
        results_list = []
        
        for result_id, result in self.results.items():
            # 过滤条件
            if test_name and result.test_name != test_name:
                continue
            
            if version and result.version != version:
                continue
            
            if days_back:
                try:
                    result_date = datetime.fromisoformat(result.timestamp.replace('Z', '+00:00'))
                    days_diff = (datetime.now() - result_date).days
                    if days_diff > days_back:
                        continue
                except:
                    continue
            
            # 计算关键指标
            key_metrics = {}
            for metric_name, values in result.metrics.items():
                if values:
                    key_metrics[metric_name] = {
                        'mean': statistics.mean(values),
                        'std': statistics.stdev(values) if len(values) > 1 else 0,
                        'min': min(values),
                        'max': max(values)
                    }
            
            results_list.append({
                'id': result_id,
                'test_name': result.test_name,
                'version': result.version,
                'timestamp': result.timestamp,
                'metrics': key_metrics,
                'metadata': result.metadata
            })
        
        # 按时间倒序排序
        results_list.sort(key=lambda x: x['timestamp'], reverse=True)
        
        return results_list


# 命令行接口
def main():
    """命令行主函数"""
    import argparse
    
    parser = argparse.ArgumentParser(
        description="AgentOS 性能基准测试历史比较器",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
使用示例:
  # 比较两个测试结果
  python history_comparator.py compare --baseline result_20260410_103000 --current result_20260411_143000
  
  # 与最近的基线比较
  python history_comparator.py compare-latest --current result_20260411_143000
  
  # 检测回归趋势
  python history_comparator.py detect-trend --test gateway_throughput --metric throughput
  
  # 列出测试结果
  python history_comparator.py list-results --test gateway_throughput --days-back 7
        """
    )
    
    subparsers = parser.add_subparsers(dest="command", help="命令")
    
    # compare 命令
    compare_parser = subparsers.add_parser("compare", help="比较两个测试结果")
    compare_parser.add_argument("--baseline", "-b", required=True, help="基线结果ID")
    compare_parser.add_argument("--current", "-c", required=True, help="当前结果ID")
    compare_parser.add_argument("--output", "-o", type=Path, help="输出报告目录")
    compare_parser.add_argument("--significance", "-s", type=float, default=0.05, help="显著性水平")
    
    # compare-latest 命令
    compare_latest_parser = subparsers.add_parser("compare-latest", help="与最近的基线比较")
    compare_latest_parser.add_argument("--current", "-c", required=True, help="当前结果ID")
    compare_latest_parser.add_argument("--days-back", "-d", type=int, default=30, help="查找最近多少天的基线")
    compare_latest_parser.add_argument("--output", "-o", type=Path, help="输出报告目录")
    
    # detect-trend 命令
    trend_parser = subparsers.add_parser("detect-trend", help="检测回归趋势")
    trend_parser.add_argument("--test", "-t", required=True, help="测试名称")
    trend_parser.add_argument("--metric", "-m", required=True, help="指标名称")
    trend_parser.add_argument("--window", "-w", type=int, default=5, help="移动平均窗口大小")
    
    # list-results 命令
    list_parser = subparsers.add_parser("list-results", help="列出测试结果")
    list_parser.add_argument("--test", "-t", help="测试名称过滤器")
    list_parser.add_argument("--version", "-v", help="版本过滤器")
    list_parser.add_argument("--days-back", "-d", type=int, help="最近多少天的结果")
    list_parser.add_argument("--format", "-f", choices=["table", "json"], default="table", help="输出格式")
    
    # results-dir 参数（所有命令共用）
    parser.add_argument("--results-dir", "-r", type=Path, help="结果目录")
    
    args = parser.parse_args()
    
    # 创建比较器
    comparator = HistoryComparator(args.results_dir)
    
    if args.command == "compare":
        # 比较两个结果
        try:
            comparison = comparator.compare_results(
                baseline_id=args.baseline,
                current_id=args.current,
                significance_level=args.significance
            )
            
            print(f"比较完成: {comparison.comparison_id}")
            print(f"基线版本: {comparison.baseline_version}")
            print(f"当前版本: {comparison.current_version}")
            print(f"比较日期: {comparison.comparison_date}")
            
            summary = comparison.summary
            print(f"\n摘要:")
            print(f"  总指标数: {summary['total_metrics']}")
            print(f"  性能提升: {summary['improvements']}")
            print(f"  性能回退: {summary['regressions']}")
            print(f"  无变化: {summary['no_changes']}")
            print(f"  显著变化: {summary['significant_changes']}")
            print(f"  严重告警: {summary['critical_alerts']}")
            print(f"  警告告警: {summary['warning_alerts']}")
            print(f"  总体评价: {summary['overall_assessment']}")
            
            # 生成报告
            if args.output:
                report_path = comparator.generate_comparison_report(comparison, args.output)
                print(f"\n报告已生成: {report_path}")
            
            # 显示告警
            if comparison.alerts:
                print(f"\n告警 ({len(comparison.alerts)} 个):")
                for alert in comparison.alerts:
                    print(f"  [{alert['level'].upper()}] {alert['metric']}: {alert['change_percentage']:.1f}%")
                    print(f"     描述: {alert['description']}")
            
        except ValueError as e:
            print(f"错误: {e}")
            return 1
    
    elif args.command == "compare-latest":
        # 与最近的基线比较
        try:
            comparison = comparator.compare_with_latest(
                current_result_id=args.current,
                days_back=args.days_back
            )
            
            if not comparison:
                print("未找到合适的基线进行比较")
                return 0
            
            print(f"比较完成: {comparison.comparison_id}")
            print(f"基线版本: {comparison.baseline_version} (ID: {comparison.baseline_id})")
            print(f"当前版本: {comparison.current_version} (ID: {comparison.current_id})")
            
            summary = comparison.summary
            print(f"\n摘要:")
            print(f"  总指标数: {summary['total_metrics']}")
            print(f"  性能提升: {summary['improvements']}")
            print(f"  性能回退: {summary['regressions']}")
            print(f"  无变化: {summary['no_changes']}")
            print(f"  显著变化: {summary['significant_changes']}")
            
            # 生成报告
            if args.output:
                report_path = comparator.generate_comparison_report(comparison, args.output)
                print(f"\n报告已生成: {report_path}")
        
        except ValueError as e:
            print(f"错误: {e}")
            return 1
    
    elif args.command == "detect-trend":
        # 检测回归趋势
        trend_result = comparator.detect_regression_trend(
            test_name=args.test,
            metric_name=args.metric,
            window_size=args.window
        )
        
        print(f"回归趋势分析: {args.test} - {args.metric}")
        print(f"数据点数: {trend_result['data_points']}")
        print(f"是否有趋势: {trend_result['has_trend']}")
        
        if trend_result['has_trend']:
            print(f"趋势方向: {trend_result['trend_direction']}")
            print(f"趋势斜率: {trend_result['trend_slope']:.4f}")
            print(f"趋势显著性 (p值): {trend_result['trend_p_value']:.4f}")
            print(f"R²: {trend_result['r_squared']:.4f}")
            print(f"是否回归: {trend_result['is_regressing']}")
            print(f"近期趋势: {trend_result['recent_trend']}")
    
    elif args.command == "list-results":
        # 列出测试结果
        results = comparator.list_results(
            test_name=args.test,
            version=args.version,
            days_back=args.days_back
        )
        
        if args.format == "table":
            print(f"找到 {len(results)} 个结果:")
            print("-" * 100)
            print(f"{'ID':<25} {'测试名称':<25} {'版本':<15} {'时间':<25} {'关键指标'}")
            print("-" * 100)
            
            for result in results:
                # 提取1-2个关键指标
                metrics_summary = []
                for metric_name, stats in result['metrics'].items():
                    if len(metrics_summary) < 2:
                        metrics_summary.append(f"{metric_name}: {stats['mean']:.1f}")
                
                metrics_str = ", ".join(metrics_summary)
                
                print(f"{result['id']:<25} {result['test_name']:<25} {result['version']:<15} "
                      f"{result['timestamp']:<25} {metrics_str}")
        
        elif args.format == "json":
            print(json.dumps(results, indent=2, ensure_ascii=False))
    
    else:
        parser.print_help()
    
    return 0


if __name__ == "__main__":
    main()