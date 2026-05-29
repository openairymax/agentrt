#!/usr/bin/env python3
"""
AgentOS 性能基准测试统计计算引擎

提供高级统计计算和分析功能，包括：
1. 描述性统计分析
2. 概率分布拟合
3. 显著性检验
4. 相关性分析
5. 回归分析
6. 时间序列分析

设计原则：
1. 精确性 - 使用高精度数值计算
2. 鲁棒性 - 处理异常值和边界情况
3. 可解释性 - 提供清晰的分析结果
4. 性能 - 优化大规模数据处理

@version v1.0.0
@date 2026-04-11
@copyright (c) 2026 SPHARX. All Rights Reserved.
"""

import json
import math
import statistics
import typing
from dataclasses import dataclass, field
from datetime import datetime
from enum import Enum
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple, Union

import numpy as np
from scipy import stats as scipy_stats


class DistributionType(Enum):
    """概率分布类型"""
    NORMAL = "normal"          # 正态分布
    LOG_NORMAL = "log_normal"  # 对数正态分布
    EXPONENTIAL = "exponential"  # 指数分布
    WEIBULL = "weibull"        # 威布尔分布
    GAMMA = "gamma"            # Gamma分布
    UNKNOWN = "unknown"        # 未知分布


class StatisticalTestResult(Enum):
    """统计检验结果"""
    SIGNIFICANT = "significant"      # 显著差异
    NOT_SIGNIFICANT = "not_significant"  # 无显著差异
    INCONCLUSIVE = "inconclusive"    # 不确定


@dataclass
class DistributionFit:
    """分布拟合结果"""
    distribution_type: DistributionType
    parameters: Dict[str, float]
    goodness_of_fit: float  # 拟合优度（如R²）
    log_likelihood: float   # 对数似然值
    aic: float             # 赤池信息准则
    bic: float             # 贝叶斯信息准则
    
    def to_dict(self) -> Dict[str, Any]:
        """转换为字典格式"""
        return {
            "distribution_type": self.distribution_type.value,
            "parameters": self.parameters,
            "goodness_of_fit": self.goodness_of_fit,
            "log_likelihood": self.log_likelihood,
            "aic": self.aic,
            "bic": self.bic
        }


@dataclass
class DescriptiveStatistics:
    """描述性统计"""
    count: int                      # 样本数
    mean: float                     # 均值
    median: float                   # 中位数
    mode: float                     # 众数
    std_dev: float                  # 标准差
    variance: float                 # 方差
    min: float                      # 最小值
    max: float                      # 最大值
    range: float                    # 极差
    q1: float                       # 第一四分位数
    q3: float                       # 第三四分位数
    iqr: float                      # 四分位距
    skewness: float                 # 偏度
    kurtosis: float                 # 峰度
    cv: float                       # 变异系数
    confidence_interval_95: Tuple[float, float]  # 95%置信区间
    percentiles: Dict[str, float] = field(default_factory=dict)  # 百分位数
    
    def to_dict(self) -> Dict[str, Any]:
        """转换为字典格式"""
        return {
            "count": self.count,
            "mean": self.mean,
            "median": self.median,
            "mode": self.mode,
            "std_dev": self.std_dev,
            "variance": self.variance,
            "min": self.min,
            "max": self.max,
            "range": self.range,
            "q1": self.q1,
            "q3": self.q3,
            "iqr": self.iqr,
            "skewness": self.skewness,
            "kurtosis": self.kurtosis,
            "cv": self.cv,
            "confidence_interval_95": {
                "lower": self.confidence_interval_95[0],
                "upper": self.confidence_interval_95[1]
            },
            "percentiles": self.percentiles
        }


@dataclass
class CorrelationResult:
    """相关性分析结果"""
    pearson_r: float                # Pearson相关系数
    pearson_p: float                # Pearson p-value
    spearman_rho: float             # Spearman相关系数
    spearman_p: float               # Spearman p-value
    kendall_tau: float              # Kendall相关系数
    kendall_p: float                # Kendall p-value
    interpretation: str             # 相关性解释
    
    def to_dict(self) -> Dict[str, Any]:
        """转换为字典格式"""
        return {
            "pearson": {
                "r": self.pearson_r,
                "p": self.pearson_p
            },
            "spearman": {
                "rho": self.spearman_rho,
                "p": self.spearman_p
            },
            "kendall": {
                "tau": self.kendall_tau,
                "p": self.kendall_p
            },
            "interpretation": self.interpretation
        }


@dataclass
class StatisticalTest:
    """统计检验结果"""
    test_name: str                  # 检验名称
    test_statistic: float           # 检验统计量
    p_value: float                  # p值
    degrees_of_freedom: Optional[int] = None  # 自由度
    result: StatisticalTestResult = StatisticalTestResult.INCONCLUSIVE  # 检验结果
    effect_size: Optional[float] = None  # 效应大小
    power: Optional[float] = None   # 检验功效
    
    def to_dict(self) -> Dict[str, Any]:
        """转换为字典格式"""
        return {
            "test_name": self.test_name,
            "test_statistic": self.test_statistic,
            "p_value": self.p_value,
            "degrees_of_freedom": self.degrees_of_freedom,
            "result": self.result.value,
            "effect_size": self.effect_size,
            "power": self.power
        }


class StatisticsEngine:
    """统计计算引擎"""
    
    def __init__(self, data: List[float]):
        """初始化统计引擎"""
        if not data:
            raise ValueError("数据不能为空")
        
        self.data = np.array(data, dtype=np.float64)
        self.sorted_data = np.sort(self.data)
        self.n = len(data)
        
    def compute_descriptive_statistics(self) -> DescriptiveStatistics:
        """计算描述性统计"""
        # 基本统计量
        mean = float(np.mean(self.data))
        median = float(np.median(self.data))
        
        # 众数（使用最频繁的值）
        from scipy import stats
        mode_result = stats.mode(self.data)
        mode = float(mode_result.mode) if mode_result.count > 1 else mean
        
        std_dev = float(np.std(self.data, ddof=1))  # 样本标准差
        variance = float(np.var(self.data, ddof=1))  # 样本方差
        data_min = float(np.min(self.data))
        data_max = float(np.max(self.data))
        data_range = data_max - data_min
        
        # 四分位数
        q1 = float(np.percentile(self.data, 25))
        q3 = float(np.percentile(self.data, 75))
        iqr = q3 - q1
        
        # 偏度和峰度
        skewness = float(scipy_stats.skew(self.data))
        kurtosis = float(scipy_stats.kurtosis(self.data))
        
        # 变异系数
        cv = (std_dev / mean) * 100 if mean != 0 else 0
        
        # 置信区间
        if self.n > 1:
            se = std_dev / math.sqrt(self.n)
            z_score = scipy_stats.norm.ppf(0.975)  # 95%置信度的Z值
            margin = z_score * se
            ci_lower = mean - margin
            ci_upper = mean + margin
        else:
            ci_lower = ci_upper = mean
        
        # 百分位数
        percentiles = {}
        for p in [1, 5, 10, 25, 50, 75, 90, 95, 99]:
            percentiles[f"p{p}"] = float(np.percentile(self.data, p))
        
        return DescriptiveStatistics(
            count=self.n,
            mean=mean,
            median=median,
            mode=mode,
            std_dev=std_dev,
            variance=variance,
            min=data_min,
            max=data_max,
            range=data_range,
            q1=q1,
            q3=q3,
            iqr=iqr,
            skewness=skewness,
            kurtosis=kurtosis,
            cv=cv,
            confidence_interval_95=(ci_lower, ci_upper),
            percentiles=percentiles
        )
    
    def fit_distribution(self) -> DistributionFit:
        """拟合概率分布"""
        # 尝试拟合多种分布
        distributions = [
            (DistributionType.NORMAL, self._fit_normal),
            (DistributionType.LOG_NORMAL, self._fit_lognormal),
            (DistributionType.EXPONENTIAL, self._fit_exponential),
            (DistributionType.WEIBULL, self._fit_weibull),
            (DistributionType.GAMMA, self._fit_gamma),
        ]
        
        best_fit = None
        best_aic = float('inf')
        
        for dist_type, fit_func in distributions:
            try:
                fit_result = fit_func()
                if fit_result.aic < best_aic:
                    best_aic = fit_result.aic
                    best_fit = fit_result
            except Exception as e:
                print(f"分布 {dist_type.value} 拟合失败: {e}")
                continue
        
        if best_fit is None:
            # 返回未知分布
            best_fit = DistributionFit(
                distribution_type=DistributionType.UNKNOWN,
                parameters={},
                goodness_of_fit=0,
                log_likelihood=0,
                aic=float('inf'),
                bic=float('inf')
            )
        
        return best_fit
    
    def _fit_normal(self) -> DistributionFit:
        """拟合正态分布"""
        mu, sigma = scipy_stats.norm.fit(self.data)
        
        # 计算对数似然
        log_likelihood = np.sum(scipy_stats.norm.logpdf(self.data, mu, sigma))
        
        # 计算拟合优度（R²）
        sorted_data = np.sort(self.data)
        cdf = scipy_stats.norm.cdf(sorted_data, mu, sigma)
        empirical_cdf = np.arange(1, self.n + 1) / self.n
        r_squared = 1 - np.sum((empirical_cdf - cdf) ** 2) / np.sum((empirical_cdf - np.mean(empirical_cdf)) ** 2)
        
        # 计算AIC和BIC
        k = 2  # 正态分布参数个数
        aic = 2 * k - 2 * log_likelihood
        bic = k * np.log(self.n) - 2 * log_likelihood
        
        return DistributionFit(
            distribution_type=DistributionType.NORMAL,
            parameters={"mu": mu, "sigma": sigma},
            goodness_of_fit=r_squared,
            log_likelihood=log_likelihood,
            aic=aic,
            bic=bic
        )
    
    def _fit_lognormal(self) -> DistributionFit:
        """拟合对数正态分布"""
        # 确保所有数据为正
        if np.any(self.data <= 0):
            raise ValueError("数据必须为正才能拟合对数正态分布")
        
        # 对数据取对数
        log_data = np.log(self.data)
        mu, sigma = scipy_stats.norm.fit(log_data)
        
        # 计算对数似然
        log_likelihood = np.sum(scipy_stats.lognorm.logpdf(self.data, sigma, scale=np.exp(mu)))
        
        # 计算拟合优度
        sorted_data = np.sort(self.data)
        cdf = scipy_stats.lognorm.cdf(sorted_data, sigma, scale=np.exp(mu))
        empirical_cdf = np.arange(1, self.n + 1) / self.n
        r_squared = 1 - np.sum((empirical_cdf - cdf) ** 2) / np.sum((empirical_cdf - np.mean(empirical_cdf)) ** 2)
        
        # 计算AIC和BIC
        k = 2
        aic = 2 * k - 2 * log_likelihood
        bic = k * np.log(self.n) - 2 * log_likelihood
        
        return DistributionFit(
            distribution_type=DistributionType.LOG_NORMAL,
            parameters={"mu": mu, "sigma": sigma},
            goodness_of_fit=r_squared,
            log_likelihood=log_likelihood,
            aic=aic,
            bic=bic
        )
    
    def _fit_exponential(self) -> DistributionFit:
        """拟合指数分布"""
        # 确保所有数据非负
        if np.any(self.data < 0):
            raise ValueError("数据必须非负才能拟合指数分布")
        
        loc, scale = scipy_stats.expon.fit(self.data)
        
        # 计算对数似然
        log_likelihood = np.sum(scipy_stats.expon.logpdf(self.data, loc, scale))
        
        # 计算拟合优度
        sorted_data = np.sort(self.data)
        cdf = scipy_stats.expon.cdf(sorted_data, loc, scale)
        empirical_cdf = np.arange(1, self.n + 1) / self.n
        r_squared = 1 - np.sum((empirical_cdf - cdf) ** 2) / np.sum((empirical_cdf - np.mean(empirical_cdf)) ** 2)
        
        # 计算AIC和BIC
        k = 2
        aic = 2 * k - 2 * log_likelihood
        bic = k * np.log(self.n) - 2 * log_likelihood
        
        return DistributionFit(
            distribution_type=DistributionType.EXPONENTIAL,
            parameters={"loc": loc, "scale": scale},
            goodness_of_fit=r_squared,
            log_likelihood=log_likelihood,
            aic=aic,
            bic=bic
        )
    
    def _fit_weibull(self) -> DistributionFit:
        """拟合威布尔分布"""
        # 确保所有数据为正
        if np.any(self.data <= 0):
            raise ValueError("数据必须为正才能拟合威布尔分布")
        
        c, loc, scale = scipy_stats.weibull_min.fit(self.data)
        
        # 计算对数似然
        log_likelihood = np.sum(scipy_stats.weibull_min.logpdf(self.data, c, loc, scale))
        
        # 计算拟合优度
        sorted_data = np.sort(self.data)
        cdf = scipy_stats.weibull_min.cdf(sorted_data, c, loc, scale)
        empirical_cdf = np.arange(1, self.n + 1) / self.n
        r_squared = 1 - np.sum((empirical_cdf - cdf) ** 2) / np.sum((empirical_cdf - np.mean(empirical_cdf)) ** 2)
        
        # 计算AIC和BIC
        k = 3
        aic = 2 * k - 2 * log_likelihood
        bic = k * np.log(self.n) - 2 * log_likelihood
        
        return DistributionFit(
            distribution_type=DistributionType.WEIBULL,
            parameters={"c": c, "loc": loc, "scale": scale},
            goodness_of_fit=r_squared,
            log_likelihood=log_likelihood,
            aic=aic,
            bic=bic
        )
    
    def _fit_gamma(self) -> DistributionFit:
        """拟合Gamma分布"""
        # 确保所有数据为正
        if np.any(self.data <= 0):
            raise ValueError("数据必须为正才能拟合Gamma分布")
        
        a, loc, scale = scipy_stats.gamma.fit(self.data)
        
        # 计算对数似然
        log_likelihood = np.sum(scipy_stats.gamma.logpdf(self.data, a, loc, scale))
        
        # 计算拟合优度
        sorted_data = np.sort(self.data)
        cdf = scipy_stats.gamma.cdf(sorted_data, a, loc, scale)
        empirical_cdf = np.arange(1, self.n + 1) / self.n
        r_squared = 1 - np.sum((empirical_cdf - cdf) ** 2) / np.sum((empirical_cdf - np.mean(empirical_cdf)) ** 2)
        
        # 计算AIC和BIC
        k = 3
        aic = 2 * k - 2 * log_likelihood
        bic = k * np.log(self.n) - 2 * log_likelihood
        
        return DistributionFit(
            distribution_type=DistributionType.GAMMA,
            parameters={"a": a, "loc": loc, "scale": scale},
            goodness_of_fit=r_squared,
            log_likelihood=log_likelihood,
            aic=aic,
            bic=bic
        )
    
    def compare_with_other(self, other_data: List[float], test_type: str = "t_test") -> StatisticalTest:
        """与另一组数据比较"""
        if not other_data:
            raise ValueError("对比数据不能为空")
        
        other_array = np.array(other_data, dtype=np.float64)
        
        if test_type == "t_test":
            # 独立样本t检验
            t_stat, p_value = scipy_stats.ttest_ind(self.data, other_array, equal_var=False)
            
            # 计算效应大小（Cohen's d）
            pooled_std = math.sqrt(
                ((self.n - 1) * np.var(self.data) + (len(other_data) - 1) * np.var(other_array)) /
                (self.n + len(other_data) - 2)
            )
            effect_size = abs(np.mean(self.data) - np.mean(other_array)) / pooled_std
            
            # 计算检验功效
            power = self._calculate_power(t_stat, self.n + len(other_data) - 2, p_value)
            
            # 判断结果
            result = StatisticalTestResult.SIGNIFICANT if p_value < 0.05 else StatisticalTestResult.NOT_SIGNIFICANT
            
            return StatisticalTest(
                test_name="独立样本t检验",
                test_statistic=t_stat,
                p_value=p_value,
                degrees_of_freedom=self.n + len(other_data) - 2,
                result=result,
                effect_size=effect_size,
                power=power
            )
        
        elif test_type == "mannwhitney":
            # Mann-Whitney U检验（非参数）
            u_stat, p_value = scipy_stats.mannwhitneyu(self.data, other_array, alternative='two-sided')
            
            # 计算效应大小（r）
            n1, n2 = self.n, len(other_data)
            effect_size = 1 - (2 * u_stat) / (n1 * n2)
            
            result = StatisticalTestResult.SIGNIFICANT if p_value < 0.05 else StatisticalTestResult.NOT_SIGNIFICANT
            
            return StatisticalTest(
                test_name="Mann-Whitney U检验",
                test_statistic=u_stat,
                p_value=p_value,
                result=result,
                effect_size=effect_size
            )
        
        elif test_type == "ks_test":
            # Kolmogorov-Smirnov检验（分布比较）
            ks_stat, p_value = scipy_stats.ks_2samp(self.data, other_array)
            
            result = StatisticalTestResult.SIGNIFICANT if p_value < 0.05 else StatisticalTestResult.NOT_SIGNIFICANT
            
            return StatisticalTest(
                test_name="Kolmogorov-Smirnov检验",
                test_statistic=ks_stat,
                p_value=p_value,
                result=result
            )
        
        else:
            raise ValueError(f"不支持的检验类型: {test_type}")
    
    def _calculate_power(self, t_stat: float, df: int, alpha: float = 0.05) -> float:
        """计算检验功效"""
        try:
            # 使用非中心t分布计算功效
            from scipy.stats import nct
            
            # 计算非中心参数
            ncp = t_stat
            
            # 计算临界t值
            t_critical = scipy_stats.t.ppf(1 - alpha/2, df)
            
            # 计算功效
            power = 1 - nct.cdf(t_critical, df, ncp) + nct.cdf(-t_critical, df, ncp)
            
            return min(max(power, 0), 1)  # 确保在[0,1]范围内
        except:
            return 0.5  # 默认值
    
    def detect_outliers(self, method: str = "iqr", threshold: float = 1.5) -> Tuple[List[float], List[int]]:
        """检测异常值"""
        outliers = []
        outlier_indices = []
        
        if method == "iqr":
            # IQR方法
            q1 = np.percentile(self.data, 25)
            q3 = np.percentile(self.data, 75)
            iqr = q3 - q1
            
            lower_bound = q1 - threshold * iqr
            upper_bound = q3 + threshold * iqr
            
            for i, value in enumerate(self.data):
                if value < lower_bound or value > upper_bound:
                    outliers.append(float(value))
                    outlier_indices.append(i)
        
        elif method == "zscore":
            # Z分数方法
            mean = np.mean(self.data)
            std = np.std(self.data)
            
            for i, value in enumerate(self.data):
                z_score = abs(value - mean) / std if std > 0 else 0
                if z_score > threshold:
                    outliers.append(float(value))
                    outlier_indices.append(i)
        
        elif method == "modified_zscore":
            # 修正Z分数方法（对异常值更鲁棒）
            median = np.median(self.data)
            mad = np.median(np.abs(self.data - median))
            modified_z_scores = 0.6745 * (self.data - median) / mad if mad > 0 else 0
            
            for i, z_score in enumerate(modified_z_scores):
                if abs(z_score) > threshold:
                    outliers.append(float(self.data[i]))
                    outlier_indices.append(i)
        
        else:
            raise ValueError(f"不支持的异常值检测方法: {method}")
        
        return outliers, outlier_indices
    
    def time_series_analysis(self, timestamps: List[datetime]) -> Dict[str, Any]:
        """时间序列分析"""
        if len(timestamps) != self.n:
            raise ValueError("时间戳数量必须与数据数量相同")
        
        # 计算时间间隔
        time_diffs = []
        for i in range(1, len(timestamps)):
            diff = (timestamps[i] - timestamps[i-1]).total_seconds()
            time_diffs.append(diff)
        
        # 基本时间序列统计
        analysis = {
            "timestamps": [ts.isoformat() for ts in timestamps],
            "time_interval_stats": {
                "mean": float(np.mean(time_diffs)) if time_diffs else 0,
                "std": float(np.std(time_diffs)) if len(time_diffs) > 1 else 0,
                "min": float(np.min(time_diffs)) if time_diffs else 0,
                "max": float(np.max(time_diffs)) if time_diffs else 0
            },
            "trend": self._detect_trend(),
            "seasonality": self._detect_seasonality(timestamps),
            "autocorrelation": self._calculate_autocorrelation()
        }
        
        return analysis
    
    def _detect_trend(self) -> Dict[str, Any]:
        """检测趋势"""
        # 线性趋势
        x = np.arange(len(self.data))
        slope, intercept, r_value, p_value, std_err = scipy_stats.linregress(x, self.data)
        
        return {
            "linear_trend": {
                "slope": slope,
                "intercept": intercept,
                "r_squared": r_value ** 2,
                "p_value": p_value
            },
            "direction": "increasing" if slope > 0 else "decreasing" if slope < 0 else "stable",
            "strength": "strong" if abs(r_value) > 0.7 else "moderate" if abs(r_value) > 0.3 else "weak"
        }
    
    def _detect_seasonality(self, timestamps: List[datetime]) -> Dict[str, Any]:
        """检测季节性"""
        # 简化版本：按小时、星期等分组
        seasonal_analysis = {
            "detected": False,
            "periods": []
        }
        
        if len(timestamps) < 24:  # 数据太少
            return seasonal_analysis
        
        # 这里可以添加更复杂的季节性检测逻辑
        # 例如使用傅里叶变换或STL分解
        
        return seasonal_analysis
    
    def _calculate_autocorrelation(self, max_lag: int = 20) -> List[float]:
        """计算自相关函数"""
        if self.n < 2:
            return []
        
        # 限制滞后值
        max_lag = min(max_lag, self.n - 1)
        
        autocorrs = []
        for lag in range(1, max_lag + 1):
            if self.n - lag < 2:
                break
            
            # 计算滞后自相关
            corr = np.corrcoef(self.data[:-lag], self.data[lag:])[0, 1]
            autocorrs.append(float(corr))
        
        return autocorrs
    
    def correlation_analysis(self, other_data: List[float]) -> CorrelationResult:
        """相关性分析"""
        if len(other_data) != self.n:
            raise ValueError("数据长度必须相同")
        
        other_array = np.array(other_data, dtype=np.float64)
        
        # Pearson相关系数
        pearson_r, pearson_p = scipy_stats.pearsonr(self.data, other_array)
        
        # Spearman相关系数
        spearman_rho, spearman_p = scipy_stats.spearmanr(self.data, other_array)
        
        # Kendall相关系数
        kendall_tau, kendall_p = scipy_stats.kendalltau(self.data, other_array)
        
        # 解释相关性强度
        def interpret_correlation(r: float) -> str:
            abs_r = abs(r)
            if abs_r >= 0.9:
                return "极强相关"
            elif abs_r >= 0.7:
                return "强相关"
            elif abs_r >= 0.4:
                return "中等相关"
            elif abs_r >= 0.2:
                return "弱相关"
            else:
                return "极弱相关或无相关"
        
        interpretation = interpret_correlation(pearson_r)
        
        return CorrelationResult(
            pearson_r=pearson_r,
            pearson_p=pearson_p,
            spearman_rho=spearman_rho,
            spearman_p=spearman_p,
            kendall_tau=kendall_tau,
            kendall_p=kendall_p,
            interpretation=interpretation
        )
    
    def generate_report(self, output_path: Optional[Path] = None) -> Dict[str, Any]:
        """生成完整统计报告"""
        report = {
            "metadata": {
                "timestamp": datetime.now().isoformat(),
                "data_points": self.n,
                "data_range": [float(np.min(self.data)), float(np.max(self.data))]
            },
            "descriptive_statistics": self.compute_descriptive_statistics().to_dict(),
            "distribution_fit": self.fit_distribution().to_dict(),
            "outliers": {},
            "time_series_analysis": {},
            "notes": []
        }
        
        # 检测异常值
        outliers, indices = self.detect_outliers()
        if outliers:
            report["outliers"] = {
                "count": len(outliers),
                "values": outliers,
                "indices": indices,
                "percentage": (len(outliers) / self.n) * 100
            }
            report["notes"].append(f"检测到 {len(outliers)} 个异常值，占总数据的 {len(outliers)/self.n*100:.1f}%")
        
        # 如果数据有趋势，添加时间序列分析
        if self.n > 10:
            # 创建虚拟时间戳用于演示
            virtual_timestamps = [datetime.now() for _ in range(self.n)]
            report["time_series_analysis"] = self.time_series_analysis(virtual_timestamps)
        
        # 保存报告
        if output_path:
            output_path.parent.mkdir(parents=True, exist_ok=True)
            with open(output_path, 'w', encoding='utf-8') as f:
                json.dump(report, f, indent=2, ensure_ascii=False)
        
        return report


# 命令行接口
def main():
    """命令行主函数"""
    import argparse
    
    parser = argparse.ArgumentParser(
        description="AgentOS 统计计算引擎",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
使用示例:
  # 分析数据文件
  python statistics_engine.py --input data.json
  
  # 计算描述性统计
  python statistics_engine.py --input data.json --descriptive
  
  # 拟合分布
  python statistics_engine.py --input data.json --fit-distribution
  
  # 生成完整报告
  python statistics_engine.py --input data.json --output report.json
        """
    )
    
    parser.add_argument(
        "--input", "-i",
        type=Path,
        required=True,
        help="输入数据文件（JSON或CSV）"
    )
    
    parser.add_argument(
        "--descriptive", "-d",
        action="store_true",
        help="计算描述性统计"
    )
    
    parser.add_argument(
        "--fit-distribution", "-f",
        action="store_true",
        help="拟合概率分布"
    )
    
    parser.add_argument(
        "--detect-outliers", "-o",
        action="store_true",
        help="检测异常值"
    )
    
    parser.add_argument(
        "--output", "-O",
        type=Path,
        help="输出报告路径"
    )
    
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="详细输出"
    )
    
    args = parser.parse_args()
    
    # 加载数据
    if not args.input.exists():
        print(f"错误: 文件不存在: {args.input}")
        return 1
    
    data = []
    if args.input.suffix.lower() == '.json':
        with open(args.input, 'r', encoding='utf-8') as f:
            content = json.load(f)
            # 尝试提取数据
            if isinstance(content, list):
                data = [float(x) for x in content]
            elif isinstance(content, dict) and 'data' in content:
                data = [float(x) for x in content['data']]
            else:
                print("错误: JSON格式不支持")
                return 1
    else:
        # 假设是CSV或其他格式（简化处理）
        with open(args.input, 'r', encoding='utf-8') as f:
            for line in f:
                try:
                    value = float(line.strip())
                    data.append(value)
                except:
                    pass
    
    if not data:
        print("错误: 没有有效数据")
        return 1
    
    print(f"加载了 {len(data)} 个数据点")
    
    # 创建统计引擎
    engine = StatisticsEngine(data)
    
    # 执行请求的分析
    if args.descriptive or (not args.descriptive and not args.fit_distribution and not args.detect_outliers):
        print("\n" + "="*70)
        print("描述性统计分析")
        print("="*70)
        
        stats = engine.compute_descriptive_statistics()
        stats_dict = stats.to_dict()
        
        for key, value in stats_dict.items():
            if isinstance(value, dict):
                print(f"{key}:")
                for subkey, subvalue in value.items():
                    print(f"  {subkey}: {subvalue}")
            else:
                print(f"{key}: {value}")
    
    if args.fit_distribution:
        print("\n" + "="*70)
        print("概率分布拟合")
        print("="*70)
        
        fit = engine.fit_distribution()
        fit_dict = fit.to_dict()
        
        print(f"最佳拟合分布: {fit_dict['distribution_type']}")
        print(f"拟合优度 (R²): {fit_dict['goodness_of_fit']:.4f}")
        print(f"参数: {fit_dict['parameters']}")
        print(f"对数似然值: {fit_dict['log_likelihood']:.2f}")
        print(f"AIC: {fit_dict['aic']:.2f}")
        print(f"BIC: {fit_dict['bic']:.2f}")
    
    if args.detect_outliers:
        print("\n" + "="*70)
        print("异常值检测")
        print("="*70)
        
        outliers, indices = engine.detect_outliers()
        
        if outliers:
            print(f"检测到 {len(outliers)} 个异常值:")
            for i, (value, idx) in enumerate(zip(outliers, indices)):
                print(f"  {i+1}. 索引 {idx}: {value}")
            print(f"异常值比例: {len(outliers)/len(data)*100:.2f}%")
        else:
            print("未检测到异常值")
    
    # 生成完整报告
    if args.output:
        print("\n" + "="*70)
        print("生成完整统计报告")
        print("="*70)
        
        report = engine.generate_report(args.output)
        print(f"报告已保存到: {args.output}")
    
    return 0


if __name__ == "__main__":
    main()