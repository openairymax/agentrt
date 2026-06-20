"""
AutoScorer — Prompt 自动评分器

对 Prompt 模板在指定指标维度上自动评分，输出 ScoreReport。
支持自定义评分函数和权重配置。
"""

from __future__ import annotations

import json
import logging
import time
from dataclasses import dataclass, field, asdict
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional

from .evaluate import PromptEvaluator, EvaluationReport

logger = logging.getLogger(__name__)

# ─── Data Models ──────────────────────────────────────────


@dataclass
class DimensionScore:
    """单个评分维度的得分。"""

    dimension: str
    raw_value: float
    normalized_score: float  # 0-100
    weight: float
    weighted_score: float  # normalized_score * weight
    grade: str  # A/B/C/D/F
    details: Dict[str, Any] = field(default_factory=dict)


@dataclass
class ScoreReport:
    """完整评分报告。"""

    prompt_name: str
    version: str
    dataset_path: str
    overall_score: float = 0.0  # 0-100
    overall_grade: str = "F"
    dimensions: List[Dict[str, Any]] = field(default_factory=list)
    strengths: List[str] = field(default_factory=list)
    weaknesses: List[str] = field(default_factory=list)
    recommendations: List[str] = field(default_factory=list)
    timestamp: str = ""


# ─── Grading Helper ───────────────────────────────────────


def _score_to_grade(score: float) -> str:
    """将 0-100 分数映射到等级。"""
    if score >= 90:
        return "A"
    elif score >= 80:
        return "B"
    elif score >= 70:
        return "C"
    elif score >= 60:
        return "D"
    else:
        return "F"


# ─── Built-in Scoring Functions ───────────────────────────


def _score_precision(report: EvaluationReport) -> float:
    """精确率评分 (0-100)。"""
    return report.avg_precision * 100


def _score_recall(report: EvaluationReport) -> float:
    """召回率评分 (0-100)。"""
    return report.avg_recall * 100


def _score_hallucination(report: EvaluationReport) -> float:
    """幻觉率评分 (0-100, 越低越好, 反转为分数)。"""
    return (1 - report.hallucination_rate) * 100


def _score_latency(report: EvaluationReport) -> float:
    """延迟评分 (0-100)。

    基准: < 500ms = 100, > 5000ms = 0, 线性插值。
    """
    latency = report.avg_latency_ms
    if latency <= 500:
        return 100.0
    elif latency >= 5000:
        return 0.0
    else:
        return 100.0 * (1 - (latency - 500) / 4500)


def _score_reliability(report: EvaluationReport) -> float:
    """可靠性评分 (0-100)。

    基于 error_rate 和 pass_rate。
    """
    if report.total_cases == 0:
        return 0.0
    error_rate = report.error_cases / report.total_cases
    pass_rate = report.passed_cases / report.total_cases
    return (pass_rate * 0.7 + (1 - error_rate) * 0.3) * 100


def _score_consistency(report: EvaluationReport) -> float:
    """一致性评分 (0-100)。

    基于不同类别和难度下表现的方差。
    """
    category_scores = []
    for cat_stats in report.by_category.values():
        avg = (cat_stats.get("avg_precision", 0) + cat_stats.get("avg_recall", 0)) / 2
        category_scores.append(avg)

    difficulty_scores = []
    for diff_stats in report.by_difficulty.values():
        avg = (diff_stats.get("avg_precision", 0) + diff_stats.get("avg_recall", 0)) / 2
        difficulty_scores.append(avg)

    all_scores = category_scores + difficulty_scores
    if len(all_scores) < 2:
        return 50.0

    mean = sum(all_scores) / len(all_scores)
    variance = sum((s - mean) ** 2 for s in all_scores) / len(all_scores)
    std_dev = variance**0.5

    # 标准差越小越一致，映射到 0-100
    if std_dev <= 0.05:
        return 100.0
    elif std_dev >= 0.3:
        return 0.0
    else:
        return 100.0 * (1 - (std_dev - 0.05) / 0.25)


# ─── Default Dimension Config ─────────────────────────────

DEFAULT_DIMENSIONS: Dict[str, Dict[str, Any]] = {
    "precision": {
        "scorer": _score_precision,
        "weight": 0.25,
        "description": "字段级精确率",
    },
    "recall": {
        "scorer": _score_recall,
        "weight": 0.25,
        "description": "字段级召回率",
    },
    "hallucination": {
        "scorer": _score_hallucination,
        "weight": 0.20,
        "description": "幻觉率（越低越好）",
    },
    "latency": {
        "scorer": _score_latency,
        "weight": 0.10,
        "description": "响应延迟",
    },
    "reliability": {
        "scorer": _score_reliability,
        "weight": 0.10,
        "description": "可靠性和通过率",
    },
    "consistency": {
        "scorer": _score_consistency,
        "weight": 0.10,
        "description": "跨类别/难度的一致性",
    },
}


# ─── AutoScorer ───────────────────────────────────────────


class AutoScorer:
    """Prompt 自动评分器。

    用法::

        scorer = AutoScorer(
            prompts_dir="ecosystem/prompts",
            gateway_url="http://localhost:8080",
        )
        report = scorer.score(
            prompt_name="extract_facts",
            version="1.0.0",
            dataset_path="eval/extract_facts.jsonl",
            metrics=["precision", "recall", "hallucination"],
        )
    """

    def __init__(
        self,
        prompts_dir: str = "ecosystem/prompts",
        gateway_url: str = "http://localhost:8080",
        timeout_ms: int = 30000,
        dimensions: Optional[Dict[str, Dict[str, Any]]] = None,
    ):
        self._evaluator = PromptEvaluator(
            prompts_dir=prompts_dir,
            gateway_url=gateway_url,
            timeout_ms=timeout_ms,
        )
        self._dimensions = dimensions or DEFAULT_DIMENSIONS

    def score(
        self,
        prompt_name: str,
        version: str,
        dataset_path: str,
        *,
        metrics: Optional[List[str]] = None,
        max_cases: Optional[int] = None,
    ) -> ScoreReport:
        """对指定 Prompt 模板进行自动评分。"""
        logger.info("Scoring prompt %s@%s on %s", prompt_name, version, dataset_path)

        # 运行评测
        eval_report = self._evaluator.evaluate(
            prompt_name=prompt_name,
            version=version,
            dataset_path=dataset_path,
            max_cases=max_cases,
        )

        # 选择评分维度
        dim_configs = self._dimensions
        if metrics:
            dim_configs = {
                k: v for k, v in dim_configs.items() if k in metrics
            }

        # 计算各维度得分
        dimension_scores: List[DimensionScore] = []
        for dim_name, dim_config in dim_configs.items():
            scorer_fn: Callable = dim_config["scorer"]
            weight: float = dim_config.get("weight", 1.0 / len(dim_configs))

            raw_value = scorer_fn(eval_report)
            normalized = max(0.0, min(100.0, raw_value))
            weighted = normalized * weight
            grade = _score_to_grade(normalized)

            dimension_scores.append(
                DimensionScore(
                    dimension=dim_name,
                    raw_value=round(raw_value, 4),
                    normalized_score=round(normalized, 2),
                    weight=weight,
                    weighted_score=round(weighted, 2),
                    grade=grade,
                    details={"description": dim_config.get("description", "")},
                )
            )

        # 计算总分
        total_weight = sum(d.weight for d in dimension_scores)
        overall_score = (
            sum(d.weighted_score for d in dimension_scores) / total_weight
            if total_weight > 0
            else 0.0
        )
        overall_grade = _score_to_grade(overall_score)

        # 识别优势和劣势
        sorted_dims = sorted(dimension_scores, key=lambda d: d.normalized_score, reverse=True)
        strengths = [
            f"{d.dimension}: {d.normalized_score:.1f} ({d.grade})"
            for d in sorted_dims[:3]
            if d.normalized_score >= 70
        ]
        weaknesses = [
            f"{d.dimension}: {d.normalized_score:.1f} ({d.grade})"
            for d in sorted_dims[-3:]
            if d.normalized_score < 70
        ]

        # 生成改进建议
        recommendations = self._generate_recommendations(dimension_scores, eval_report)

        return ScoreReport(
            prompt_name=prompt_name,
            version=version,
            dataset_path=dataset_path,
            overall_score=round(overall_score, 2),
            overall_grade=overall_grade,
            dimensions=[asdict(d) for d in dimension_scores],
            strengths=strengths,
            weaknesses=weaknesses,
            recommendations=recommendations,
            timestamp=time.strftime("%Y-%m-%dT%H:%M:%S%z"),
        )

    def _generate_recommendations(
        self,
        dimension_scores: List[DimensionScore],
        eval_report: EvaluationReport,
    ) -> List[str]:
        """根据评分结果生成改进建议。"""
        recs: List[str] = []

        for d in dimension_scores:
            if d.dimension == "precision" and d.normalized_score < 80:
                recs.append(
                    "精确率偏低：考虑在 Prompt 中增加更严格的输出格式约束和示例"
                )
            elif d.dimension == "recall" and d.normalized_score < 80:
                recs.append(
                    "召回率偏低：考虑扩展 Prompt 中的类别定义和边界条件说明"
                )
            elif d.dimension == "hallucination" and d.normalized_score < 85:
                recs.append(
                    "幻觉率偏高：增加 '仅使用提供的信息' 约束，降低 temperature"
                )
            elif d.dimension == "latency" and d.normalized_score < 70:
                recs.append(
                    "延迟偏高：减少 max_tokens，简化 Prompt 结构，或考虑更快的模型"
                )
            elif d.dimension == "reliability" and d.normalized_score < 80:
                recs.append(
                    "可靠性不足：增加错误处理逻辑和 fallback 策略"
                )
            elif d.dimension == "consistency" and d.normalized_score < 75:
                recs.append(
                    "一致性不足：统一不同类别/难度的处理逻辑，增加 few-shot 示例"
                )

        if not recs:
            recs.append("各项指标表现良好，建议持续监控")

        return recs


# ─── CLI Entry Point ──────────────────────────────────────


def main() -> None:
    """CLI 入口: agentrt prompt score <name> --metric=precision,recall"""
    import argparse

    parser = argparse.ArgumentParser(description="AgentRT Prompt Auto Scorer")
    parser.add_argument("prompt_name", help="Prompt template name")
    parser.add_argument("--version", default="1.0.0", help="Template version")
    parser.add_argument("--dataset", required=True, help="Path to JSONL dataset")
    parser.add_argument(
        "--metric",
        default=None,
        help="Comma-separated metrics to score (default: all)",
    )
    parser.add_argument(
        "--prompts-dir",
        default="ecosystem/prompts",
        help="Prompts directory",
    )
    parser.add_argument("--gateway-url", default="http://localhost:8080")
    parser.add_argument("--output", default=None, help="Output report path (JSON)")
    args = parser.parse_args()

    logging.basicConfig(level=logging.INFO)

    metrics = args.metric.split(",") if args.metric else None

    scorer = AutoScorer(
        prompts_dir=args.prompts_dir,
        gateway_url=args.gateway_url,
    )
    report = scorer.score(
        prompt_name=args.prompt_name,
        version=args.version,
        dataset_path=args.dataset,
        metrics=metrics,
    )

    report_dict = asdict(report)
    output = json.dumps(report_dict, indent=2, ensure_ascii=False)

    if args.output:
        Path(args.output).parent.mkdir(parents=True, exist_ok=True)
        Path(args.output).write_text(output, encoding="utf-8")
        logger.info("Report saved to %s", args.output)
    else:
        print(output)  # noqa: T201


if __name__ == "__main__":
    main()
