# Prompt 评估脚本
# 使用 PromptEvaluator 和 AutoScorer 对 extract_facts Prompt 进行评估

import argparse
import json
from pathlib import Path

from agentos.evaluation import PromptEvaluator, AutoScorer


# 待评估的 Prompt 模板
PROMPT_TEMPLATE = """
你是一个事实提取专家。请从以下文本中提取所有关键事实，并以结构化格式输出。

要求：
1. 提取所有关键事实要素（人物、时间、地点、事件、金额等）
2. 不遗漏关键信息，不编造不存在的信息
3. 以"类别：内容"的格式逐行输出

输入文本：
{input}
"""


def load_dataset(dataset_path: str) -> list[dict]:
    """加载 JSONL 格式的评估数据集。"""
    dataset = []
    with open(dataset_path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line:
                dataset.append(json.loads(line))
    return dataset


def run_evaluation(dataset_path: str, prompt: str | None = None) -> dict:
    """
    运行 Prompt 评估流程。
    1. 加载评估数据集
    2. 使用 PromptEvaluator 运行 Prompt
    3. 使用 AutoScorer 对输出评分
    4. 输出评估报告
    """
    # 加载数据集
    dataset = load_dataset(dataset_path)
    print(f"已加载 {len(dataset)} 条评估用例")

    # 确定使用的 Prompt
    eval_prompt = prompt or PROMPT_TEMPLATE
    print(f"Prompt 长度：{len(eval_prompt)} 字符")

    # 初始化评估器和评分器
    evaluator = PromptEvaluator(prompt_template=eval_prompt)
    scorer = AutoScorer(strategy="fuzzy_match")

    # 运行评估
    print("\n开始评估...")
    results = evaluator.evaluate(dataset)

    # 逐条评分
    scores = []
    for i, (case, output) in enumerate(zip(dataset, results.outputs)):
        score = scorer.score(
            output=output,
            expected=case["expected"],
            criteria=case["criteria"],
        )
        scores.append(score)
        print(f"\n--- 用例 {i + 1} ---")
        print(f"输入：{case['input'][:50]}...")
        print(f"得分：{score.score:.2f}")
        if score.reasoning:
            print(f"评分理由：{score.reasoning}")

    # 汇总统计
    avg_score = sum(s.score for s in scores) / len(scores)
    report = {
        "total_cases": len(dataset),
        "average_score": avg_score,
        "min_score": min(s.score for s in scores),
        "max_score": max(s.score for s in scores),
        "scores": [s.score for s in scores],
    }

    print(f"\n{'=' * 40}")
    print(f"评估报告")
    print(f"{'=' * 40}")
    print(f"总用例数：{report['total_cases']}")
    print(f"平均得分：{report['average_score']:.2f}")
    print(f"最低得分：{report['min_score']:.2f}")
    print(f"最高得分：{report['max_score']:.2f}")

    return report


def main():
    parser = argparse.ArgumentParser(description="Prompt 评估脚本")
    parser.add_argument(
        "--dataset",
        type=str,
        default="eval/sample_dataset.jsonl",
        help="评估数据集路径",
    )
    parser.add_argument(
        "--prompt",
        type=str,
        default=None,
        help="自定义 Prompt（不指定则使用默认模板）",
    )
    args = parser.parse_args()

    # 确保数据集路径存在
    dataset_path = Path(__file__).parent.parent / args.dataset
    if not dataset_path.exists():
        print(f"错误：数据集文件不存在 — {dataset_path}")
        return

    run_evaluation(str(dataset_path), args.prompt)


if __name__ == "__main__":
    main()
