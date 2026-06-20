"""DataAnalysisSkill — 数据分析技能

对结构化/半结构化数据执行统计分析，生成洞察报告。
"""

from __future__ import annotations

import json
import logging
import math
import re
from collections import Counter
from typing import Any, Dict, List, Optional, Tuple, Union

import sys as _sys
from pathlib import Path as _Path

# 确保 agentos 包可导入（开发模式）
_sdk_python = _Path(__file__).resolve().parents[3] / "sdk" / "python"
if str(_sdk_python) not in _sys.path:
    _sys.path.insert(0, str(_sdk_python))

from agentos.plugin_types import (
    SkillPlugin,
    SkillDefinition,
)

logger = logging.getLogger(__name__)


class DataAnalysisSkill(SkillPlugin):
    """数据分析技能。

    对输入数据执行统计分析，包括描述性统计、分布分析、
    异常检测和趋势识别，输出结构化洞察报告。

    支持的数据格式：
    - JSON 数组（对象列表）
    - CSV 格式文本
    - 键值对字典

    分析维度：
    1. 描述性统计 — 均值、中位数、标准差、分位数
    2. 分布分析 — 偏度、峰度、正态性
    3. 异常检测 — 基于IQR和Z-score的离群值检测
    4. 趋势识别 — 简单线性趋势和周期性检测
    """

    PLUGIN_TYPE = "skill"

    # ── Skill Definition ──────────────────────────────────

    def get_definition(self) -> SkillDefinition:
        return SkillDefinition(
            name="data_analysis",
            description="对结构化/半结构化数据执行统计分析，生成洞察报告",
            version="1.0.0",
            category="analytics",
            tags=["data-analysis", "statistics", "analytics", "insights"],
            input_schema={
                "type": "object",
                "properties": {
                    "data": {
                        "type": "string",
                        "description": "待分析数据 (JSON数组/CSV文本/JSON对象)",
                    },
                    "format": {
                        "type": "string",
                        "description": "数据格式 (json/csv/auto)",
                        "default": "auto",
                    },
                    "analysis_type": {
                        "type": "string",
                        "description": "分析类型 (descriptive/distribution/outliers/trend/all)",
                        "default": "all",
                    },
                    "target_fields": {
                        "type": "array",
                        "items": {"type": "string"},
                        "description": "重点分析的字段列表 (空=全部数值字段)",
                    },
                },
                "required": ["data"],
            },
            output_schema={
                "type": "object",
                "properties": {
                    "summary": {"type": "string"},
                    "record_count": {"type": "integer"},
                    "fields_analyzed": {"type": "array"},
                    "descriptive_stats": {"type": "object"},
                    "outliers": {"type": "object"},
                    "insights": {"type": "array"},
                },
            },
            examples=[
                {
                    "input": "分析一组销售数据的统计特征和异常值",
                    "output": "发现3个异常高值，销售额呈上升趋势，月均增长5.2%",
                },
                {
                    "input": "分析用户行为日志的分布",
                    "output": "请求量呈右偏分布，P95延迟为320ms",
                },
            ],
            requires=[],
        )

    def get_prompt_template(self) -> Optional[str]:
        return (
            "Analyze the following data with focus on {analysis_type}:\n\n"
            "{data}\n\n"
            "Provide statistical insights and actionable recommendations."
        )

    def get_system_instructions(self) -> Optional[str]:
        return (
            "You are a data analyst. Provide clear, actionable insights. "
            "Always include confidence levels. Distinguish correlation from causation. "
            "Note limitations of the analysis (sample size, missing data, etc.)."
        )

    # ── Validation ────────────────────────────────────────

    def validate_input(self, context: Dict[str, Any]) -> bool:
        data = context.get("data", "")
        if not data or (isinstance(data, str) and not data.strip()):
            logger.warning("DataAnalysisSkill: empty data input")
            return False
        return True

    # ── Execution ─────────────────────────────────────────

    async def pre_execute(self, context: Dict[str, Any]) -> Dict[str, Any]:
        context.setdefault("format", "auto")
        context.setdefault("analysis_type", "all")
        context.setdefault("target_fields", [])
        return context

    async def execute(self, context: Dict[str, Any]) -> Any:
        raw_data = context.get("data", "")
        fmt = context.get("format", "auto")
        analysis_type = context.get("analysis_type", "all")
        target_fields = context.get("target_fields", [])

        # 解析数据
        records = self._parse_data(raw_data, fmt)
        if not records:
            return {
                "summary": "No valid data found for analysis",
                "record_count": 0,
                "fields_analyzed": [],
                "descriptive_stats": {},
                "outliers": {},
                "insights": ["Input data could not be parsed"],
            }

        # 识别数值字段
        numeric_fields = self._detect_numeric_fields(records, target_fields)

        # 描述性统计
        desc_stats = {}
        if analysis_type in ("descriptive", "all"):
            desc_stats = self._descriptive_statistics(records, numeric_fields)

        # 异常检测
        outliers = {}
        if analysis_type in ("outliers", "all"):
            outliers = self._detect_outliers(records, numeric_fields)

        # 趋势识别
        trends = {}
        if analysis_type in ("trend", "all"):
            trends = self._detect_trends(records, numeric_fields)

        # 生成洞察
        insights = self._generate_insights(desc_stats, outliers, trends, len(records))

        # 生成摘要
        summary = self._generate_summary(len(records), numeric_fields, insights)

        return {
            "summary": summary,
            "record_count": len(records),
            "fields_analyzed": numeric_fields,
            "descriptive_stats": desc_stats,
            "outliers": outliers,
            "trends": trends,
            "insights": insights,
        }

    # ── Data Parsing ──────────────────────────────────────

    def _parse_data(self, raw: str, fmt: str) -> List[Dict[str, Any]]:
        """解析输入数据为记录列表。"""
        raw = raw.strip()
        if not raw:
            return []

        # 自动检测格式
        if fmt == "auto":
            if raw.startswith("[") or raw.startswith("{"):
                fmt = "json"
            elif "," in raw and "\n" in raw:
                fmt = "csv"
            else:
                fmt = "json"

        if fmt == "json":
            return self._parse_json(raw)
        elif fmt == "csv":
            return self._parse_csv(raw)
        return []

    @staticmethod
    def _parse_json(raw: str) -> List[Dict[str, Any]]:
        """解析 JSON 数据。"""
        try:
            data = json.loads(raw)
            if isinstance(data, list):
                return data
            elif isinstance(data, dict):
                return [data]
        except json.JSONDecodeError:
            pass
        return []

    @staticmethod
    def _parse_csv(raw: str) -> List[Dict[str, Any]]:
        """解析 CSV 数据。"""
        lines = [l.strip() for l in raw.strip().splitlines() if l.strip()]
        if len(lines) < 2:
            return []

        headers = [h.strip() for h in lines[0].split(",")]
        records = []
        for line in lines[1:]:
            values = [v.strip() for v in line.split(",")]
            if len(values) != len(headers):
                continue
            record = {}
            for h, v in zip(headers, values):
                try:
                    record[h] = float(v) if "." in v else int(v)
                except ValueError:
                    record[h] = v
            records.append(record)
        return records

    # ── Field Detection ───────────────────────────────────

    @staticmethod
    def _detect_numeric_fields(
        records: List[Dict[str, Any]], target: List[str]
    ) -> List[str]:
        """检测数值字段。"""
        if not records:
            return []

        all_fields = list(records[0].keys())
        numeric = []
        for field in all_fields:
            values = [r.get(field) for r in records if field in r]
            num_count = sum(1 for v in values if isinstance(v, (int, float)))
            if num_count > len(values) * 0.5:
                numeric.append(field)

        if target:
            numeric = [f for f in numeric if f in target]

        return numeric

    # ── Descriptive Statistics ────────────────────────────

    def _descriptive_statistics(
        self, records: List[Dict[str, Any]], fields: List[str]
    ) -> Dict[str, Dict[str, Any]]:
        """计算描述性统计。"""
        stats = {}
        for field in fields:
            values = [
                r[field] for r in records
                if field in r and isinstance(r[field], (int, float))
            ]
            if not values:
                continue

            values.sort()
            n = len(values)
            mean = sum(values) / n
            variance = sum((v - mean) ** 2 for v in values) / n if n > 1 else 0
            std_dev = math.sqrt(variance)

            stats[field] = {
                "count": n,
                "mean": round(mean, 4),
                "median": self._percentile(values, 50),
                "std_dev": round(std_dev, 4),
                "min": values[0],
                "max": values[-1],
                "p25": self._percentile(values, 25),
                "p75": self._percentile(values, 75),
                "p95": self._percentile(values, 95),
                "p99": self._percentile(values, 99),
                "iqr": round(self._percentile(values, 75) - self._percentile(values, 25), 4),
                "skewness": round(self._skewness(values, mean, std_dev), 4) if std_dev > 0 else 0,
            }
        return stats

    @staticmethod
    def _percentile(sorted_values: List[float], pct: float) -> float:
        """计算百分位数。"""
        n = len(sorted_values)
        if n == 0:
            return 0.0
        k = (n - 1) * pct / 100
        f = int(k)
        c = f + 1
        if c >= n:
            return round(sorted_values[-1], 4)
        return round(sorted_values[f] + (k - f) * (sorted_values[c] - sorted_values[f]), 4)

    @staticmethod
    def _skewness(values: List[float], mean: float, std_dev: float) -> float:
        """计算偏度。"""
        n = len(values)
        if n < 3 or std_dev == 0:
            return 0.0
        m3 = sum((v - mean) ** 3 for v in values) / n
        return m3 / (std_dev ** 3)

    # ── Outlier Detection ─────────────────────────────────

    def _detect_outliers(
        self, records: List[Dict[str, Any]], fields: List[str]
    ) -> Dict[str, Dict[str, Any]]:
        """基于 IQR 和 Z-score 检测离群值。"""
        outliers = {}
        for field in fields:
            values = [
                r[field] for r in records
                if field in r and isinstance(r[field], (int, float))
            ]
            if len(values) < 4:
                continue

            values.sort()
            q1 = self._percentile(values, 25)
            q3 = self._percentile(values, 75)
            iqr = q3 - q1

            # IQR 方法
            lower = q1 - 1.5 * iqr
            upper = q3 + 1.5 * iqr
            iqr_outliers = [v for v in values if v < lower or v > upper]

            # Z-score 方法
            mean = sum(values) / len(values)
            std_dev = math.sqrt(sum((v - mean) ** 2 for v in values) / len(values))
            zscore_outliers = []
            if std_dev > 0:
                zscore_outliers = [v for v in values if abs((v - mean) / std_dev) > 3]

            outliers[field] = {
                "iqr_method": {
                    "lower_bound": round(lower, 4),
                    "upper_bound": round(upper, 4),
                    "count": len(iqr_outliers),
                    "values": iqr_outliers[:10],
                },
                "zscore_method": {
                    "threshold": 3.0,
                    "count": len(zscore_outliers),
                    "values": zscore_outliers[:10],
                },
            }
        return outliers

    # ── Trend Detection ───────────────────────────────────

    def _detect_trends(
        self, records: List[Dict[str, Any]], fields: List[str]
    ) -> Dict[str, Dict[str, Any]]:
        """简单线性趋势检测。"""
        trends = {}
        n = len(records)
        if n < 3:
            return trends

        for field in fields:
            values = [
                r.get(field) for r in records
                if field in r and isinstance(r.get(field), (int, float))
            ]
            if len(values) < 3:
                continue

            # 简单线性回归
            x = list(range(len(values)))
            slope, intercept, r_squared = self._linear_regression(x, values)

            # 趋势方向
            if abs(slope) < 1e-10:
                direction = "flat"
            elif slope > 0:
                direction = "upward"
            else:
                direction = "downward"

            # 变化率
            mean_val = sum(values) / len(values) if values else 1
            change_rate = (slope / mean_val * 100) if mean_val != 0 else 0

            trends[field] = {
                "direction": direction,
                "slope": round(slope, 6),
                "r_squared": round(r_squared, 4),
                "change_rate_percent": round(change_rate, 2),
                "start_value": values[0],
                "end_value": values[-1],
            }
        return trends

    @staticmethod
    def _linear_regression(
        x: List[int], y: List[float]
    ) -> Tuple[float, float, float]:
        """简单线性回归，返回 (slope, intercept, r_squared)。"""
        n = len(x)
        if n < 2:
            return 0.0, 0.0, 0.0

        sum_x = sum(x)
        sum_y = sum(y)
        sum_xy = sum(xi * yi for xi, yi in zip(x, y))
        sum_x2 = sum(xi ** 2 for xi in x)

        denom = n * sum_x2 - sum_x ** 2
        if denom == 0:
            return 0.0, sum_y / n, 0.0

        slope = (n * sum_xy - sum_x * sum_y) / denom
        intercept = (sum_y - slope * sum_x) / n

        # R²
        mean_y = sum_y / n
        ss_tot = sum((yi - mean_y) ** 2 for yi in y)
        ss_res = sum((yi - (slope * xi + intercept)) ** 2 for xi, yi in zip(x, y))
        r_squared = 1 - (ss_res / ss_tot) if ss_tot > 0 else 0.0

        return slope, intercept, r_squared

    # ── Insights Generation ───────────────────────────────

    def _generate_insights(
        self,
        desc_stats: Dict[str, Dict[str, Any]],
        outliers: Dict[str, Dict[str, Any]],
        trends: Dict[str, Dict[str, Any]],
        record_count: int,
    ) -> List[str]:
        """生成数据洞察。"""
        insights: List[str] = []

        # 样本量评估
        if record_count < 30:
            insights.append(
                f"[Low confidence] Sample size ({record_count}) is small; "
                "statistical conclusions may be unreliable"
            )

        # 异常值洞察
        total_outliers = sum(
            o.get("iqr_method", {}).get("count", 0) for o in outliers.values()
        )
        if total_outliers > 0:
            outlier_fields = [
                f for f, o in outliers.items()
                if o.get("iqr_method", {}).get("count", 0) > 0
            ]
            insights.append(
                f"Found {total_outliers} outlier(s) in field(s): "
                f"{', '.join(outlier_fields)}"
            )

        # 趋势洞察
        for field, trend in trends.items():
            direction = trend.get("direction", "flat")
            change = trend.get("change_rate_percent", 0)
            r_sq = trend.get("r_squared", 0)
            if direction != "flat" and r_sq > 0.5:
                insights.append(
                    f"{field} shows a {direction} trend "
                    f"({change:+.1f}% per period, R²={r_sq:.2f})"
                )

        # 分布洞察
        for field, stats in desc_stats.items():
            skew = stats.get("skewness", 0)
            if abs(skew) > 1:
                direction = "right" if skew > 0 else "left"
                insights.append(
                    f"{field} has a {direction}-skewed distribution (skewness={skew:.2f})"
                )

        if not insights:
            insights.append("Data appears well-distributed with no significant anomalies")

        return insights

    # ── Summary ───────────────────────────────────────────

    def _generate_summary(
        self,
        record_count: int,
        fields: List[str],
        insights: List[str],
    ) -> str:
        """生成分析摘要。"""
        parts = [
            f"Analyzed {record_count} records across {len(fields)} numeric field(s)",
        ]
        if insights:
            parts.append(f"Key insight: {insights[0]}")
        return ". ".join(parts)
