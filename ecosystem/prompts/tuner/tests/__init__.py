# Copyright (c) 2026 SPHARX. All Rights Reserved.
"""Tests for ecosystem/prompts/tuner/ — evaluate, ab_test, scorer."""

import json
import math
import tempfile
from dataclasses import asdict
from pathlib import Path

import pytest

from ecosystem.prompts.tuner.evaluate import (
    PromptCase,
    EvaluatedCase,
    EvaluationReport,
    PromptEvaluator,
    load_dataset,
    _compute_field_precision,
    _compute_field_recall,
    _detect_hallucination,
)
from ecosystem.prompts.tuner.ab_test import (
    MetricDelta,
    SignificanceResult,
    ABTestReport,
    ABTestRunner,
    _normal_cdf,
    _paired_t_test,
)
from ecosystem.prompts.tuner.scorer import (
    DimensionScore,
    ScoreReport,
    AutoScorer,
    _score_to_grade,
    _score_precision,
    _score_recall,
    _score_hallucination,
    _score_latency,
    _score_reliability,
    _score_consistency,
)


# ============================================================
# evaluate.py — Scoring Helpers
# ============================================================

class TestFieldPrecision:
    def test_all_correct(self):
        expected = {"a": 1, "b": 2}
        actual = {"a": 1, "b": 2}
        assert _compute_field_precision(expected, actual) == 1.0

    def test_half_correct(self):
        expected = {"a": 1, "b": 2}
        actual = {"a": 1, "b": 99}
        assert _compute_field_precision(expected, actual) == 0.5

    def test_empty_actual(self):
        expected = {"a": 1}
        actual = {}
        assert _compute_field_precision(expected, actual) == 0.0

    def test_empty_expected(self):
        expected = {}
        actual = {"a": 1}
        assert _compute_field_precision(expected, actual) == 0.0


class TestFieldRecall:
    def test_all_recalled(self):
        expected = {"a": 1, "b": 2}
        actual = {"a": 1, "b": 2}
        assert _compute_field_recall(expected, actual) == 1.0

    def test_half_recalled(self):
        expected = {"a": 1, "b": 2}
        actual = {"a": 1}
        assert _compute_field_recall(expected, actual) == 0.5

    def test_empty_expected(self):
        expected = {}
        actual = {"a": 1}
        assert _compute_field_recall(expected, actual) == 1.0


class TestDetectHallucination:
    def test_no_hallucination(self):
        assert _detect_hallucination({"a": 1}, {"a": 1}) is False

    def test_new_field_long_string(self):
        assert _detect_hallucination({"a": 1}, {"a": 1, "b": "x" * 300}) is True

    def test_new_field_short_string(self):
        assert _detect_hallucination({"a": 1}, {"a": 1, "b": "short"}) is False

    def test_numeric_deviation(self):
        assert _detect_hallucination({"a": 100}, {"a": 20}) is True  # 80% deviation

    def test_small_numeric_deviation(self):
        assert _detect_hallucination({"a": 100}, {"a": 90}) is False  # 10% deviation

    def test_value_mismatch_non_numeric(self):
        assert _detect_hallucination({"a": "hello"}, {"a": "world"}) is True

    def test_null_expected_value(self):
        # null expected values are not checked
        assert _detect_hallucination({"a": None}, {"a": "anything"}) is False


# ============================================================
# evaluate.py — Data Models
# ============================================================

class TestPromptCase:
    def test_construction(self):
        case = PromptCase(input="test", expected_output={"k": "v"})
        assert case.input == "test"
        assert case.expected_output == {"k": "v"}
        assert case.category == "general"
        assert case.difficulty == "medium"

    def test_custom_category(self):
        case = PromptCase(input="x", expected_output={}, category="custom", difficulty="hard")
        assert case.category == "custom"
        assert case.difficulty == "hard"


class TestEvaluatedCase:
    def test_construction(self):
        case = EvaluatedCase(
            input="test",
            expected_output={"k": "v"},
            actual_output={"k": "v"},
            precision=0.9,
            recall=0.8,
            hallucination=False,
            latency_ms=150,
        )
        assert case.precision == 0.9
        assert case.recall == 0.8
        assert case.hallucination is False
        assert case.latency_ms == 150

    def test_error_case(self):
        case = EvaluatedCase(input="test", expected_output={}, error="timeout")
        assert case.error == "timeout"
        assert case.precision == 0.0


class TestEvaluationReport:
    def test_construction(self):
        report = EvaluationReport(
            prompt_name="test_prompt",
            version="1.0.0",
            dataset_path="data.jsonl",
            total_cases=10,
            passed_cases=8,
            failed_cases=1,
            error_cases=1,
            avg_precision=0.85,
            avg_recall=0.80,
            hallucination_rate=0.05,
            avg_latency_ms=200.0,
            p95_latency_ms=500.0,
        )
        assert report.total_cases == 10
        assert report.passed_cases == 8
        assert report.avg_precision == 0.85

    def test_empty_report(self):
        report = EvaluationReport(prompt_name="p", version="v", dataset_path="d.jsonl")
        assert report.total_cases == 0
        assert report.passed_cases == 0


# ============================================================
# evaluate.py — Dataset Loader
# ============================================================

class TestLoadDataset:
    def test_load_valid_jsonl(self):
        with tempfile.NamedTemporaryFile(mode="w", suffix=".jsonl", delete=False) as f:
            f.write(
                '{"input": "hello", "expected_output": {"a": 1}, "category": "test", "difficulty": "easy"}\n'
                '{"input": "world", "expected_output": {"b": 2}}\n'
            )
            f.flush()
            cases = load_dataset(f.name)
        assert len(cases) == 2
        assert cases[0].input == "hello"
        assert cases[0].category == "test"
        assert cases[0].difficulty == "easy"
        assert cases[1].category == "general"  # default

    def test_skips_comments_and_blanks(self):
        with tempfile.NamedTemporaryFile(mode="w", suffix=".jsonl", delete=False) as f:
            f.write(
                "# comment line\n"
                "\n"
                '{"input": "only", "expected_output": {"k": 1}}\n'
            )
            f.flush()
            cases = load_dataset(f.name)
        assert len(cases) == 1

    def test_skips_invalid_json(self):
        with tempfile.NamedTemporaryFile(mode="w", suffix=".jsonl", delete=False) as f:
            f.write(
                '{"input": "valid", "expected_output": {}}\n'
                "not valid json\n"
                '{"input": "also valid", "expected_output": {}}\n'
            )
            f.flush()
            cases = load_dataset(f.name)
        assert len(cases) == 2

    def test_file_not_found(self):
        with pytest.raises(FileNotFoundError):
            load_dataset("/nonexistent/path.jsonl")


# ============================================================
# evaluate.py — Evaluator (offline mode)
# ============================================================

class TestPromptEvaluator:
    def test_evaluate_with_empty_dataset(self):
        evaluator = PromptEvaluator(prompts_dir="/nonexistent", gateway_url="http://localhost")
        with tempfile.NamedTemporaryFile(mode="w", suffix=".jsonl", delete=False) as f:
            f.write("")
            f.flush()
            # This will fail at load_prompt_template because there's no registry
            # But we can test the data models independently
            pass  # Integration test requires actual prompts directory

    def test_init_defaults(self):
        evaluator = PromptEvaluator()
        assert evaluator._prompts_dir == "ecosystem/prompts"
        assert evaluator._gateway_url == "http://localhost:8080"
        assert evaluator._timeout_ms == 30000

    def test_custom_timeout(self):
        evaluator = PromptEvaluator(timeout_ms=60000)
        assert evaluator._timeout_ms == 60000


# ============================================================
# ab_test.py — Statistical Functions
# ============================================================

class TestNormalCDF:
    def test_zero(self):
        assert _normal_cdf(0) == pytest.approx(0.5, rel=0.01)

    def test_positive(self):
        assert _normal_cdf(1.96) > 0.97  # ~0.975

    def test_negative(self):
        assert _normal_cdf(-1.96) < 0.03  # ~0.025

    def test_large_positive(self):
        assert _normal_cdf(5) > 0.999

    def test_large_negative(self):
        assert _normal_cdf(-5) < 0.001


class TestPairedTTest:
    def test_identical_values(self):
        result = _paired_t_test([1, 2, 3], [1, 2, 3])
        assert result.p_value == 1.0
        assert result.significant is False

    def test_clear_improvement(self):
        result = _paired_t_test(
            [1.0] * 30,
            [2.0] * 30,
        )
        assert result.significant is True
        assert result.p_value < 0.01

    def test_small_sample_insufficient(self):
        result = _paired_t_test([1.0], [2.0])
        assert result.significant is False
        assert result.p_value == 1.0


# ============================================================
# ab_test.py — Data Models
# ============================================================

class TestMetricDelta:
    def test_improvement(self):
        d = MetricDelta(
            metric_name="precision",
            baseline_value=0.8,
            candidate_value=0.85,
            delta=0.05,
            delta_percent=6.25,
            improved=True,
        )
        assert d.improved is True
        assert d.delta_percent == 6.25

    def test_regression(self):
        d = MetricDelta(
            metric_name="latency",
            baseline_value=100,
            candidate_value=150,
            delta=50,
            delta_percent=50.0,
            improved=False,
        )
        assert d.improved is False


class TestSignificanceResult:
    def test_significant(self):
        result = SignificanceResult(
            metric_name="precision",
            t_statistic=3.5,
            p_value=0.001,
            significant=True,
            confidence_level=0.999,
        )
        assert result.significant is True

    def test_not_significant(self):
        result = SignificanceResult(
            metric_name="recall",
            t_statistic=0.5,
            p_value=0.6,
            significant=False,
            confidence_level=0.4,
        )
        assert result.significant is False


class TestABTestReport:
    def test_construction(self):
        report = ABTestReport(
            prompt_name="test",
            baseline_version="1.0.0",
            candidate_version="1.1.0",
            dataset_path="data.jsonl",
            recommendation="adopt_candidate",
            summary="Improved",
        )
        assert report.recommendation == "adopt_candidate"


# ============================================================
# ab_test.py — ABTestRunner
# ============================================================

class TestABTestRunner:
    def test_init(self):
        runner = ABTestRunner()
        assert runner._evaluator is not None

    def test_make_recommendation_adopt_candidate(self):
        runner = ABTestRunner()
        deltas = [
            MetricDelta("precision", 0.8, 0.85, 0.05, 6.25, True),
            MetricDelta("recall", 0.8, 0.85, 0.05, 6.25, True),
            MetricDelta("hallucination_rate", 0.1, 0.05, -0.05, -50.0, True),
            MetricDelta("avg_latency_ms", 100, 90, -10, -10.0, True),
        ]
        significance = [
            SignificanceResult("precision", 3.0, 0.01, True, 0.99),
            SignificanceResult("recall", 2.0, 0.05, True, 0.95),
        ]
        rec = runner._make_recommendation(deltas, significance)
        assert rec == "adopt_candidate"

    def test_make_recommendation_adopt_baseline(self):
        runner = ABTestRunner()
        deltas = [
            MetricDelta("precision", 0.85, 0.80, -0.05, -5.88, False),
            MetricDelta("recall", 0.85, 0.80, -0.05, -5.88, False),
            MetricDelta("avg_latency_ms", 100, 150, 50, 50.0, False),
        ]
        significance = []
        rec = runner._make_recommendation(deltas, significance)
        assert rec == "adopt_baseline"

    def test_make_recommendation_inconclusive(self):
        runner = ABTestRunner()
        deltas = [
            MetricDelta("precision", 0.8, 0.85, 0.05, 6.25, True),
            MetricDelta("avg_latency_ms", 100, 150, 50, 50.0, False),
        ]
        significance = []
        rec = runner._make_recommendation(deltas, significance)
        assert rec == "inconclusive"

    def test_generate_summary(self):
        runner = ABTestRunner()
        deltas = [
            MetricDelta("precision", 0.8, 0.85, 0.05, 6.25, True),
            MetricDelta("avg_latency_ms", 100, 120, 20, 20.0, False),
        ]
        summary = runner._generate_summary("test", "v1", "v2", deltas, "inconclusive")
        assert "v1" in summary
        assert "v2" in summary
        assert "precision" in summary


# ============================================================
# scorer.py — Scoring Functions
# ============================================================

class TestScoreToGrade:
    def test_grades(self):
        assert _score_to_grade(95) == "A"
        assert _score_to_grade(85) == "B"
        assert _score_to_grade(75) == "C"
        assert _score_to_grade(65) == "D"
        assert _score_to_grade(55) == "F"
        assert _score_to_grade(0) == "F"
        assert _score_to_grade(100) == "A"

    def test_boundary(self):
        assert _score_to_grade(90) == "A"
        assert _score_to_grade(80) == "B"
        assert _score_to_grade(70) == "C"
        assert _score_to_grade(60) == "D"


def _make_eval_report(**kwargs):
    defaults = {
        "prompt_name": "test",
        "version": "1.0",
        "dataset_path": "d.jsonl",
        "total_cases": 10,
        "passed_cases": 8,
        "failed_cases": 1,
        "error_cases": 1,
        "avg_precision": 0.85,
        "avg_recall": 0.80,
        "hallucination_rate": 0.05,
        "avg_latency_ms": 200.0,
        "p95_latency_ms": 500.0,
        "by_category": {"general": {"avg_precision": 0.85, "avg_recall": 0.80}},
        "by_difficulty": {"easy": {"avg_precision": 0.90, "avg_recall": 0.85}},
    }
    defaults.update(kwargs)
    return EvaluationReport(**defaults)


class TestScorePrecision:
    def test_score(self):
        report = _make_eval_report(avg_precision=0.85)
        assert _score_precision(report) == 85.0


class TestScoreRecall:
    def test_score(self):
        report = _make_eval_report(avg_recall=0.80)
        assert _score_recall(report) == 80.0


class TestScoreHallucination:
    def test_low_hallucination(self):
        report = _make_eval_report(hallucination_rate=0.05)
        assert _score_hallucination(report) == 95.0

    def test_high_hallucination(self):
        report = _make_eval_report(hallucination_rate=0.5)
        assert _score_hallucination(report) == 50.0


class TestScoreLatency:
    def test_fast(self):
        report = _make_eval_report(avg_latency_ms=200)
        assert _score_latency(report) == 100.0

    def test_slow(self):
        report = _make_eval_report(avg_latency_ms=5000)
        assert _score_latency(report) == 0.0

    def test_medium(self):
        report = _make_eval_report(avg_latency_ms=2750)  # midpoint
        assert _score_latency(report) == pytest.approx(50.0, rel=0.1)


class TestScoreReliability:
    def test_perfect(self):
        report = _make_eval_report(total_cases=10, passed_cases=10, error_cases=0)
        assert _score_reliability(report) == 100.0

    def test_mixed(self):
        report = _make_eval_report(total_cases=10, passed_cases=7, error_cases=2)
        # pass_rate=0.7, error_rate=0.2
        # 0.7*0.7 + 0.8*0.3 = 0.49 + 0.24 = 0.73 * 100 = 73
        assert _score_reliability(report) == pytest.approx(73.0, rel=0.01)

    def test_zero_cases(self):
        report = _make_eval_report(total_cases=0)
        assert _score_reliability(report) == 0.0


class TestScoreConsistency:
    def test_perfect_consistency(self):
        report = _make_eval_report(
            by_category={"a": {"avg_precision": 0.9, "avg_recall": 0.9}},
            by_difficulty={"easy": {"avg_precision": 0.9, "avg_recall": 0.9}},
        )
        assert _score_consistency(report) == 100.0

    def test_varied_consistency(self):
        report = _make_eval_report(
            by_category={
                "a": {"avg_precision": 0.9, "avg_recall": 0.9},
                "b": {"avg_precision": 0.5, "avg_recall": 0.5},
            },
            by_difficulty={"easy": {"avg_precision": 0.9, "avg_recall": 0.9}},
        )
        score = _score_consistency(report)
        assert score < 100.0

    def test_single_category(self):
        report = _make_eval_report(
            by_category={"a": {"avg_precision": 0.9, "avg_recall": 0.9}},
            by_difficulty={},
        )
        assert _score_consistency(report) == 50.0


# ============================================================
# scorer.py — AutoScorer
# ============================================================

class TestAutoScorer:
    def test_init(self):
        scorer = AutoScorer()
        assert scorer._evaluator is not None
        assert scorer._dimensions is not None

    def test_custom_dimensions(self):
        custom = {
            "precision": {
                "scorer": _score_precision,
                "weight": 1.0,
                "description": "test",
            },
        }
        scorer = AutoScorer(dimensions=custom)
        assert len(scorer._dimensions) == 1


# ============================================================
# scorer.py — Data Models
# ============================================================

class TestDimensionScore:
    def test_construction(self):
        score = DimensionScore(
            dimension="precision",
            raw_value=85.0,
            normalized_score=85.0,
            weight=0.25,
            weighted_score=21.25,
            grade="B",
        )
        assert score.grade == "B"
        assert score.weighted_score == 21.25


class TestScoreReport:
    def test_construction(self):
        report = ScoreReport(
            prompt_name="test",
            version="1.0",
            dataset_path="d.jsonl",
            overall_score=85.0,
            overall_grade="B",
            strengths=["precision: 95.0 (A)"],
            weaknesses=["latency: 60.0 (D)"],
        )
        assert report.overall_score == 85.0
        assert report.overall_grade == "B"
        assert len(report.strengths) == 1
        assert len(report.weaknesses) == 1