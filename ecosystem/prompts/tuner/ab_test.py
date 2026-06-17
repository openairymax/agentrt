"""
ABTestRunner — Prompt A/B 对比测试

对同一 Prompt 的两个版本（baseline vs candidate）在相同数据集上运行评测，
输出统计显著性检验结果和 ABTestReport。
"""

from __future__ import annotations

import json
import logging
import math
import time
from dataclasses import dataclass, field, asdict
from pathlib import Path
from typing import Any, Dict, List, Optional

from .evaluate import (
    PromptEvaluator,
    EvaluationReport,
    load_dataset,
    load_prompt_template,
)

logger = logging.getLogger(__name__)

# ─── Data Models ──────────────────────────────────────────


@dataclass
class MetricDelta:
    """两个版本之间某指标的差异。"""

    metric_name: str
    baseline_value: float
    candidate_value: float
    delta: float  # candidate - baseline
    delta_percent: float  # percentage change
    improved: bool  # delta > 0 for metrics where higher is better


@dataclass
class SignificanceResult:
    """统计显著性检验结果（配对 t 检验）。"""

    metric_name: str
    t_statistic: float
    p_value: float
    significant: bool  # p < 0.05
    confidence_level: float  # 1 - p_value


@dataclass
class ABTestReport:
    """A/B 测试完整报告。"""

    prompt_name: str
    baseline_version: str
    candidate_version: str
    dataset_path: str
    total_cases: int = 0
    baseline_report: Optional[Dict[str, Any]] = None
    candidate_report: Optional[Dict[str, Any]] = None
    metric_deltas: List[Dict[str, Any]] = field(default_factory=list)
    significance_tests: List[Dict[str, Any]] = field(default_factory=list)
    recommendation: str = ""  # adopt_baseline / adopt_candidate / inconclusive
    summary: str = ""
    timestamp: str = ""


# ─── Statistical Helpers ──────────────────────────────────


def _paired_t_test(
    baseline_scores: List[float],
    candidate_scores: List[float],
) -> SignificanceResult:
    """配对 t 检验。"""
    n = min(len(baseline_scores), len(candidate_scores))
    if n < 2:
        return SignificanceResult(
            metric_name="",
            t_statistic=0.0,
            p_value=1.0,
            significant=False,
            confidence_level=0.0,
        )

    diffs = [c - b for b, c in zip(baseline_scores[:n], candidate_scores[:n])]
    mean_diff = sum(diffs) / n
    var_diff = sum((d - mean_diff) ** 2 for d in diffs) / (n - 1)
    std_err = math.sqrt(var_diff / n) if var_diff > 0 else 0.0

    if std_err == 0:
        t_stat = 0.0
        p_value = 1.0
    else:
        t_stat = mean_diff / std_err
        # 近似 p 值（双尾检验，自由度 n-1）
        # 使用正态近似（n >= 30 时足够精确）
        if n >= 30:
            p_value = 2 * (1 - _normal_cdf(abs(t_stat)))
        else:
            # 小样本保守估计
            p_value = 2 * (1 - _normal_cdf(abs(t_stat))) if abs(t_stat) > 2 else 1.0

    return SignificanceResult(
        metric_name="",
        t_statistic=round(t_stat, 4),
        p_value=round(p_value, 6),
        significant=p_value < 0.05,
        confidence_level=round(1 - p_value, 4),
    )


def _normal_cdf(x: float) -> float:
    """标准正态分布 CDF 近似（Abramowitz & Stegun）。"""
    a1 = 0.254829592
    a2 = -0.284496736
    a3 = 1.421413741
    a4 = -1.453152027
    a5 = 1.061405429
    p = 0.3275911

    sign = 1 if x >= 0 else -1
    x = abs(x) / math.sqrt(2.0)
    t = 1.0 / (1.0 + p * x)
    y = 1.0 - (((((a5 * t + a4) * t) + a3) * t + a2) * t + a1) * t * math.exp(-x * x)
    return 0.5 * (1.0 + sign * y)


# ─── AB Test Runner ───────────────────────────────────────


class ABTestRunner:
    """Prompt A/B 对比测试运行器。

    用法::

        runner = ABTestRunner(
            prompts_dir="ecosystem/prompts",
            gateway_url="http://localhost:8080",
        )
        report = runner.ab_test(
            prompt_name="extract_facts",
            baseline_version="1.0.0",
            candidate_version="1.1.0",
            dataset_path="eval/extract_facts.jsonl",
        )
    """

    # 需要对比的核心指标（值越大越好）
    METRICS_HIGHER_IS_BETTER = [
        "avg_precision",
        "avg_recall",
        "passed_cases",
    ]

    # 需要对比的核心指标（值越小越好）
    METRICS_LOWER_IS_BETTER = [
        "hallucination_rate",
        "avg_latency_ms",
        "error_cases",
    ]

    def __init__(
        self,
        prompts_dir: str = "ecosystem/prompts",
        gateway_url: str = "http://localhost:8080",
        timeout_ms: int = 30000,
    ):
        self._evaluator = PromptEvaluator(
            prompts_dir=prompts_dir,
            gateway_url=gateway_url,
            timeout_ms=timeout_ms,
        )

    def ab_test(
        self,
        prompt_name: str,
        baseline_version: str,
        candidate_version: str,
        dataset_path: str,
        *,
        max_cases: Optional[int] = None,
    ) -> ABTestReport:
        """运行 A/B 对比测试。"""
        logger.info(
            "Starting A/B test: %s baseline=%s vs candidate=%s",
            prompt_name,
            baseline_version,
            candidate_version,
        )

        # 评测 baseline
        baseline_report = self._evaluator.evaluate(
            prompt_name=prompt_name,
            version=baseline_version,
            dataset_path=dataset_path,
            max_cases=max_cases,
        )

        # 评测 candidate
        candidate_report = self._evaluator.evaluate(
            prompt_name=prompt_name,
            version=candidate_version,
            dataset_path=dataset_path,
            max_cases=max_cases,
        )

        # 计算指标差异
        deltas = self._compute_deltas(baseline_report, candidate_report)

        # 统计显著性检验
        significance = self._compute_significance(baseline_report, candidate_report)

        # 生成推荐
        recommendation = self._make_recommendation(deltas, significance)
        summary = self._generate_summary(
            prompt_name, baseline_version, candidate_version, deltas, recommendation
        )

        return ABTestReport(
            prompt_name=prompt_name,
            baseline_version=baseline_version,
            candidate_version=candidate_version,
            dataset_path=dataset_path,
            total_cases=baseline_report.total_cases,
            baseline_report=asdict(baseline_report),
            candidate_report=asdict(candidate_report),
            metric_deltas=[asdict(d) for d in deltas],
            significance_tests=[asdict(s) for s in significance],
            recommendation=recommendation,
            summary=summary,
            timestamp=time.strftime("%Y-%m-%dT%H:%M:%S%z"),
        )

    def _compute_deltas(
        self,
        baseline: EvaluationReport,
        candidate: EvaluationReport,
    ) -> List[MetricDelta]:
        """计算各指标的差异。"""
        deltas: List[MetricDelta] = []

        for metric in self.METRICS_HIGHER_IS_BETTER:
            b_val = getattr(baseline, metric, 0)
            c_val = getattr(candidate, metric, 0)
            if isinstance(b_val, (int, float)) and isinstance(c_val, (int, float)):
                delta = c_val - b_val
                delta_pct = (delta / b_val * 100) if b_val != 0 else 0.0
                deltas.append(
                    MetricDelta(
                        metric_name=metric,
                        baseline_value=b_val,
                        candidate_value=c_val,
                        delta=round(delta, 4),
                        delta_percent=round(delta_pct, 2),
                        improved=delta > 0,
                    )
                )

        for metric in self.METRICS_LOWER_IS_BETTER:
            b_val = getattr(baseline, metric, 0)
            c_val = getattr(candidate, metric, 0)
            if isinstance(b_val, (int, float)) and isinstance(c_val, (int, float)):
                delta = c_val - b_val
                delta_pct = (delta / b_val * 100) if b_val != 0 else 0.0
                deltas.append(
                    MetricDelta(
                        metric_name=metric,
                        baseline_value=b_val,
                        candidate_value=c_val,
                        delta=round(delta, 4),
                        delta_percent=round(delta_pct, 2),
                        improved=delta < 0,
                    )
                )

        return deltas

    def _compute_significance(
        self,
        baseline: EvaluationReport,
        candidate: EvaluationReport,
    ) -> List[SignificanceResult]:
        """对逐样本精度和召回进行配对 t 检验。"""
        results: List[SignificanceResult] = []

        b_cases = baseline.cases
        c_cases = candidate.cases
        n = min(len(b_cases), len(c_cases))

        for metric_key in ["precision", "recall"]:
            b_scores = [
                b_cases[i].get(metric_key, 0) for i in range(n)
            ]
            c_scores = [
                c_cases[i].get(metric_key, 0) for i in range(n)
            ]
            sig = _paired_t_test(b_scores, c_scores)
            sig.metric_name = metric_key
            results.append(sig)

        return results

    def _make_recommendation(
        self,
        deltas: List[MetricDelta],
        significance: List[SignificanceResult],
    ) -> str:
        """根据指标差异和显著性生成推荐。"""
        improved_count = sum(1 for d in deltas if d.improved)
        total_count = len(deltas)
        significant_count = sum(1 for s in significance if s.significant)

        if total_count == 0:
            return "inconclusive"

        improvement_ratio = improved_count / total_count

        if improvement_ratio >= 0.7 and significant_count >= 1:
            return "adopt_candidate"
        elif improvement_ratio <= 0.3:
            return "adopt_baseline"
        else:
            return "inconclusive"

    def _generate_summary(
        self,
        prompt_name: str,
        baseline_version: str,
        candidate_version: str,
        deltas: List[MetricDelta],
        recommendation: str,
    ) -> str:
        """生成人类可读的摘要。"""
        improved = [d for d in deltas if d.improved]
        regressed = [d for d in deltas if not d.improved]

        lines = [
            f"A/B Test: {prompt_name} ({baseline_version} vs {candidate_version})",
            f"Recommendation: {recommendation}",
            f"Improved metrics: {len(improved)}/{len(deltas)}",
            f"Regressed metrics: {len(regressed)}/{len(deltas)}",
        ]

        for d in deltas:
            direction = "+" if d.delta >= 0 else ""
            lines.append(
                f"  {d.metric_name}: {d.baseline_value} -> {d.candidate_value} "
                f"({direction}{d.delta_percent}%)"
            )

        return "\n".join(lines)


# ─── CLI Entry Point ──────────────────────────────────────


def main() -> None:
    """CLI 入口: agentrt prompt ab-test <name> --baseline=v1 --candidate=v2"""
    import argparse

    parser = argparse.ArgumentParser(description="AgentRT Prompt A/B Test Runner")
    parser.add_argument("prompt_name", help="Prompt template name")
    parser.add_argument(
        "--baseline", required=True, help="Baseline version"
    )
    parser.add_argument(
        "--candidate", required=True, help="Candidate version"
    )
    parser.add_argument("--dataset", required=True, help="Path to JSONL dataset")
    parser.add_argument(
        "--prompts-dir",
        default="ecosystem/prompts",
        help="Prompts directory",
    )
    parser.add_argument("--gateway-url", default="http://localhost:8080")
    parser.add_argument("--output", default=None, help="Output report path (JSON)")
    args = parser.parse_args()

    logging.basicConfig(level=logging.INFO)

    runner = ABTestRunner(
        prompts_dir=args.prompts_dir,
        gateway_url=args.gateway_url,
    )
    report = runner.ab_test(
        prompt_name=args.prompt_name,
        baseline_version=args.baseline,
        candidate_version=args.candidate,
        dataset_path=args.dataset,
    )

    report_dict = asdict(report)
    output = json.dumps(report_dict, indent=2, ensure_ascii=False)

    if args.output:
        Path(args.output).parent.mkdir(parents=True, exist_ok=True)
        Path(args.output).write_text(output, encoding="utf-8")
        logger.info("Report saved to %s", args.output)
    else:
        print(output)


if __name__ == "__main__":
    main()
