# Prompt Tuner Demo - Prompt 调优框架

> 难度：中级 | 主题：Prompt 评估与自动调优

## 项目说明

本项目演示 AgentRT 的 Prompt 调优框架。通过 `PromptEvaluator` 和 `AutoScorer`，你可以系统化地评估和优化 Prompt 的效果，而非依赖人工试错。

### 核心组件

- **PromptEvaluator**：在评估数据集上运行 Prompt，收集输出结果
- **AutoScorer**：自动对 Prompt 输出进行评分，支持多种评分策略
- **评估数据集**：JSONL 格式的标准评估用例，包含输入、期望输出和评分标准

## 目录结构

```
prompt-tuner-demo/
├── README.md                    # 本文件
├── config.yaml                  # AgentRT 配置
├── eval/
│   └── sample_dataset.jsonl     # 评估数据集（5条样例）
└── scripts/
    └── run_eval.py              # 评估脚本
```

## 运行方式

```bash
# 1. 确保 AgentRT 已安装
pip install agentrt

# 2. 运行评估脚本
python scripts/run_eval.py

# 3. 指定自定义数据集和 Prompt
python scripts/run_eval.py --dataset eval/sample_dataset.jsonl --prompt "你的自定义Prompt"

# 4. 使用 AgentRT CLI 运行
agentrt eval --config config.yaml --dataset eval/sample_dataset.jsonl
```

## 关键概念

### 1. 评估数据集格式

每条评估用例包含三个字段：

```jsonl
{"input": "用户输入", "expected": "期望输出", "criteria": "评分标准"}
```

- `input`：传给 Prompt 的输入内容
- `expected`：期望的输出（用于对比评分）
- `criteria`：评分标准描述（AutoScorer 据此判断输出质量）

### 2. PromptEvaluator 工作流程

```
评估数据集 → PromptEvaluator → 逐条运行Prompt → 收集原始输出
                                                    ↓
原始输出 + 期望输出 → AutoScorer → 计算每条得分 → 汇总评估报告
```

### 3. AutoScorer 评分策略

| 策略 | 说明 | 适用场景 |
|------|------|---------|
| `exact_match` | 精确匹配期望输出 | 分类、抽取等确定性任务 |
| `fuzzy_match` | 模糊匹配，计算相似度 | 生成类任务的近似评估 |
| `llm_judge` | 使用 LLM 作为裁判评分 | 开放式生成任务 |
| `custom` | 自定义评分函数 | 特定业务规则 |

### 4. Prompt 迭代优化

通过对比不同 Prompt 版本在相同数据集上的评分，可以量化 Prompt 改进效果：

```python
# 对比两个 Prompt 版本
result_v1 = evaluator.evaluate(prompt_v1, dataset)
result_v2 = evaluator.evaluate(prompt_v2, dataset)
print(f"V1: {result_v1.avg_score:.2f} → V2: {result_v2.avg_score:.2f}")
```

## 扩展建议

- **扩大数据集**：5条样例仅用于演示，实际评估建议至少50-100条覆盖各种边界情况
- **多维度评分**：在 `criteria` 中定义多个评分维度（准确性、完整性、简洁性等）
- **自动化调优**：结合 LLM 自动生成 Prompt 变体，通过评估选择最优版本
- **回归测试**：将评估数据集纳入 CI/CD，确保 Prompt 修改不会导致性能退化
- **A/B 测试**：在生产环境中对不同 Prompt 版本进行 A/B 测试，收集真实用户反馈
