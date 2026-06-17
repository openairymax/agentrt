"""
PromptEvaluator — Prompt 评测引擎

对指定 Prompt 模板在给定数据集上运行评测，输出 EvaluationReport。
支持 JSONL 格式数据集，每个样本包含 input / expected_output / category / difficulty。
"""

from __future__ import annotations

import json
import logging
import os
import time
from dataclasses import dataclass, field, asdict
from pathlib import Path
from typing import Any, Dict, List, Optional

import yaml

logger = logging.getLogger(__name__)

# ─── Data Models ──────────────────────────────────────────


@dataclass
class PromptCase:
    """单个评测样本。"""

    input: str
    expected_output: Dict[str, Any]
    category: str = "general"
    difficulty: str = "medium"  # easy / medium / hard


@dataclass
class EvaluatedCase(PromptCase):
    """评测后的样本，包含实际输出和评测指标。"""

    actual_output: Dict[str, Any] = field(default_factory=dict)
    precision: float = 0.0
    recall: float = 0.0
    hallucination: bool = False
    latency_ms: int = 0
    error: Optional[str] = None


@dataclass
class EvaluationReport:
    """完整评测报告。"""

    prompt_name: str
    version: str
    dataset_path: str
    total_cases: int = 0
    passed_cases: int = 0
    failed_cases: int = 0
    error_cases: int = 0
    avg_precision: float = 0.0
    avg_recall: float = 0.0
    hallucination_rate: float = 0.0
    avg_latency_ms: float = 0.0
    p95_latency_ms: float = 0.0
    by_category: Dict[str, Dict[str, float]] = field(default_factory=dict)
    by_difficulty: Dict[str, Dict[str, float]] = field(default_factory=dict)
    cases: List[Dict[str, Any]] = field(default_factory=list)
    timestamp: str = ""


# ─── Dataset Loader ───────────────────────────────────────


def load_dataset(dataset_path: str) -> List[PromptCase]:
    """从 JSONL 文件加载评测数据集。

    每行格式: {"input": "...", "expected_output": {...}, "category": "...", "difficulty": "..."}
    """
    cases: List[PromptCase] = []
    path = Path(dataset_path)
    if not path.exists():
        raise FileNotFoundError(f"Dataset not found: {dataset_path}")

    with open(path, "r", encoding="utf-8") as f:
        for line_no, line in enumerate(f, 1):
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            try:
                obj = json.loads(line)
                cases.append(
                    PromptCase(
                        input=obj["input"],
                        expected_output=obj.get("expected_output", {}),
                        category=obj.get("category", "general"),
                        difficulty=obj.get("difficulty", "medium"),
                    )
                )
            except (json.JSONDecodeError, KeyError) as exc:
                logger.warning("Skipping invalid line %d: %s", line_no, exc)

    logger.info("Loaded %d cases from %s", len(cases), dataset_path)
    return cases


# ─── Prompt Template Loader ───────────────────────────────


def load_prompt_template(prompts_dir: str, prompt_name: str) -> Dict[str, Any]:
    """从 registry.yaml 查找并加载指定 Prompt 模板。"""
    registry_path = Path(prompts_dir) / "registry.yaml"
    if not registry_path.exists():
        raise FileNotFoundError(f"Registry not found: {registry_path}")

    with open(registry_path, "r", encoding="utf-8") as f:
        registry = yaml.safe_load(f)

    for entry in registry.get("prompts", []):
        if entry["name"] == prompt_name:
            template_path = Path(prompts_dir) / entry["path"]
            with open(template_path, "r", encoding="utf-8") as tf:
                return yaml.safe_load(tf)

    raise ValueError(f"Prompt '{prompt_name}' not found in registry")


# ─── Scoring Helpers ──────────────────────────────────────


def _compute_field_precision(
    expected: Dict[str, Any], actual: Dict[str, Any]
) -> float:
    """计算字段级精确率：actual 中正确字段数 / actual 中总字段数。"""
    if not actual:
        return 0.0
    correct = 0
    total = 0
    for key, value in actual.items():
        total += 1
        if key in expected and actual[key] == expected[key]:
            correct += 1
    return correct / total if total > 0 else 0.0


def _compute_field_recall(
    expected: Dict[str, Any], actual: Dict[str, Any]
) -> float:
    """计算字段级召回率：expected 中被正确覆盖的字段数 / expected 中总字段数。"""
    if not expected:
        return 1.0
    correct = 0
    for key, value in expected.items():
        if key in actual and actual[key] == value:
            correct += 1
    return correct / len(expected)


def _detect_hallucination(
    expected: Dict[str, Any], actual: Dict[str, Any]
) -> bool:
    """检测幻觉：actual 中存在 expected 中没有的字段且值不匹配。"""
    for key, value in actual.items():
        if key not in expected:
            # 新增字段不一定是幻觉，但如果值明显不合理则标记
            if isinstance(value, str) and len(value) > 200:
                return True
        elif actual[key] != expected[key] and expected[key] is not None:
            # 字段存在但值不匹配
            if isinstance(value, (int, float)) and isinstance(
                expected[key], (int, float)
            ):
                # 数值偏差超过 50% 视为幻觉
                if expected[key] != 0 and abs(value - expected[key]) / abs(expected[key]) > 0.5:
                    return True
    return False


# ─── Evaluator ────────────────────────────────────────────


class PromptEvaluator:
    """Prompt 评测引擎。

    用法::

        evaluator = PromptEvaluator(
            prompts_dir="ecosystem/prompts",
            gateway_url="http://localhost:8080",
        )
        report = evaluator.evaluate(
            prompt_name="extract_facts",
            version="1.0.0",
            dataset_path="eval/extract_facts.jsonl",
        )
    """

    def __init__(
        self,
        prompts_dir: str = "ecosystem/prompts",
        gateway_url: str = "http://localhost:8080",
        timeout_ms: int = 30000,
    ):
        self._prompts_dir = prompts_dir
        self._gateway_url = gateway_url
        self._timeout_ms = timeout_ms

    # ── Main API ──────────────────────────────────────────

    def evaluate(
        self,
        prompt_name: str,
        version: str,
        dataset_path: str,
        *,
        max_cases: Optional[int] = None,
        categories: Optional[List[str]] = None,
    ) -> EvaluationReport:
        """对指定 Prompt 模板在给定数据集上运行评测。"""
        cases = load_dataset(dataset_path)
        if categories:
            cases = [c for c in cases if c.category in categories]
        if max_cases:
            cases = cases[:max_cases]

        template = load_prompt_template(self._prompts_dir, prompt_name)
        target_metrics = template.get("metrics", {})

        evaluated: List[EvaluatedCase] = []
        for idx, case in enumerate(cases):
            logger.info(
                "Evaluating case %d/%d [%s/%s]",
                idx + 1,
                len(cases),
                case.category,
                case.difficulty,
            )
            evaluated.append(self._evaluate_single(case, template))

        return self._build_report(
            prompt_name=prompt_name,
            version=version,
            dataset_path=dataset_path,
            evaluated=evaluated,
            target_metrics=target_metrics,
        )

    # ── Internal ──────────────────────────────────────────

    def _evaluate_single(
        self, case: PromptCase, template: Dict[str, Any]
    ) -> EvaluatedCase:
        """评测单个样本。"""
        start = time.monotonic()
        try:
            actual_output = self._call_gateway(case, template)
            latency_ms = int((time.monotonic() - start) * 1000)

            precision = _compute_field_precision(case.expected_output, actual_output)
            recall = _compute_field_recall(case.expected_output, actual_output)
            hallucination = _detect_hallucination(case.expected_output, actual_output)

            return EvaluatedCase(
                input=case.input,
                expected_output=case.expected_output,
                category=case.category,
                difficulty=case.difficulty,
                actual_output=actual_output,
                precision=precision,
                recall=recall,
                hallucination=hallucination,
                latency_ms=latency_ms,
            )
        except Exception as exc:
            latency_ms = int((time.monotonic() - start) * 1000)
            logger.error("Case evaluation failed: %s", exc)
            return EvaluatedCase(
                input=case.input,
                expected_output=case.expected_output,
                category=case.category,
                difficulty=case.difficulty,
                latency_ms=latency_ms,
                error=str(exc),
            )

    def _call_gateway(
        self, case: PromptCase, template: Dict[str, Any]
    ) -> Dict[str, Any]:
        """调用 Gateway API 执行 Prompt 并返回结果。

        在无 Gateway 环境下返回空结果（用于离线评测模式）。
        """
        try:
            import requests

            payload = {
                "prompt_name": template.get("name", ""),
                "version": template.get("version", ""),
                "input": case.input,
                "temperature": template.get("temperature", 0.3),
                "max_tokens": template.get("max_tokens", 1024),
            }
            resp = requests.post(
                f"{self._gateway_url}/v1/prompt/execute",
                json=payload,
                timeout=self._timeout_ms / 1000,
            )
            resp.raise_for_status()
            return resp.json().get("output", {})
        except ImportError:
            logger.warning("requests library not available, returning empty output")
            return {}
        except Exception as exc:
            logger.warning("Gateway call failed: %s, returning empty output", exc)
            return {}

    def _build_report(
        self,
        prompt_name: str,
        version: str,
        dataset_path: str,
        evaluated: List[EvaluatedCase],
        target_metrics: Dict[str, Any],
    ) -> EvaluationReport:
        """汇总评测结果生成报告。"""
        total = len(evaluated)
        error_cases = sum(1 for e in evaluated if e.error is not None)
        valid = [e for e in evaluated if e.error is None]

        avg_precision = (
            sum(e.precision for e in valid) / len(valid) if valid else 0.0
        )
        avg_recall = sum(e.recall for e in valid) / len(valid) if valid else 0.0
        hallucination_rate = (
            sum(1 for e in valid if e.hallucination) / len(valid) if valid else 0.0
        )
        latencies = sorted([e.latency_ms for e in valid])
        avg_latency = sum(latencies) / len(latencies) if latencies else 0.0
        p95_latency = (
            latencies[int(len(latencies) * 0.95)] if latencies else 0.0
        )

        # 按类别分组统计
        by_category: Dict[str, Dict[str, float]] = {}
        for e in valid:
            cat = e.category
            if cat not in by_category:
                by_category[cat] = {
                    "count": 0,
                    "precision_sum": 0.0,
                    "recall_sum": 0.0,
                    "hallucination_count": 0,
                }
            by_category[cat]["count"] += 1
            by_category[cat]["precision_sum"] += e.precision
            by_category[cat]["recall_sum"] += e.recall
            by_category[cat]["hallucination_count"] += int(e.hallucination)

        for cat, stats in by_category.items():
            n = stats.pop("count")
            stats["avg_precision"] = stats.pop("precision_sum") / n if n else 0.0
            stats["avg_recall"] = stats.pop("recall_sum") / n if n else 0.0
            stats["hallucination_rate"] = (
                stats.pop("hallucination_count") / n if n else 0.0
            )

        # 按难度分组统计
        by_difficulty: Dict[str, Dict[str, float]] = {}
        for e in valid:
            diff = e.difficulty
            if diff not in by_difficulty:
                by_difficulty[diff] = {
                    "count": 0,
                    "precision_sum": 0.0,
                    "recall_sum": 0.0,
                }
            by_difficulty[diff]["count"] += 1
            by_difficulty[diff]["precision_sum"] += e.precision
            by_difficulty[diff]["recall_sum"] += e.recall

        for diff, stats in by_difficulty.items():
            n = stats.pop("count")
            stats["avg_precision"] = stats.pop("precision_sum") / n if n else 0.0
            stats["avg_recall"] = stats.pop("recall_sum") / n if n else 0.0

        # 判断通过/失败
        passed = sum(
            1
            for e in valid
            if e.precision >= target_metrics.get("target_precision", 0.8)
            and e.recall >= target_metrics.get("target_recall", 0.75)
            and not e.hallucination
        )

        return EvaluationReport(
            prompt_name=prompt_name,
            version=version,
            dataset_path=dataset_path,
            total_cases=total,
            passed_cases=passed,
            failed_cases=len(valid) - passed,
            error_cases=error_cases,
            avg_precision=round(avg_precision, 4),
            avg_recall=round(avg_recall, 4),
            hallucination_rate=round(hallucination_rate, 4),
            avg_latency_ms=round(avg_latency, 1),
            p95_latency_ms=round(p95_latency, 1),
            by_category=by_category,
            by_difficulty=by_difficulty,
            cases=[asdict(e) for e in evaluated],
            timestamp=time.strftime("%Y-%m-%dT%H:%M:%S%z"),
        )


# ─── CLI Entry Point ──────────────────────────────────────


def main() -> None:
    """CLI 入口: agentrt prompt tune <name> --dataset=<path>"""
    import argparse

    parser = argparse.ArgumentParser(description="AgentRT Prompt Evaluator")
    parser.add_argument("prompt_name", help="Prompt template name")
    parser.add_argument("--version", default="1.0.0", help="Template version")
    parser.add_argument("--dataset", required=True, help="Path to JSONL dataset")
    parser.add_argument(
        "--prompts-dir",
        default="ecosystem/prompts",
        help="Prompts directory",
    )
    parser.add_argument("--gateway-url", default="http://localhost:8080")
    parser.add_argument("--max-cases", type=int, default=None)
    parser.add_argument("--output", default=None, help="Output report path (JSON)")
    args = parser.parse_args()

    logging.basicConfig(level=logging.INFO)

    evaluator = PromptEvaluator(
        prompts_dir=args.prompts_dir,
        gateway_url=args.gateway_url,
    )
    report = evaluator.evaluate(
        prompt_name=args.prompt_name,
        version=args.version,
        dataset_path=args.dataset,
        max_cases=args.max_cases,
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
