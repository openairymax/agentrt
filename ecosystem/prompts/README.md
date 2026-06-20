# Prompts — AgentRT 提示词管理模块

**模块路径**: `ecosystem/prompts/`
**版本**: v1.0.0

## 概述

Prompts 是 AgentRT 的提示词管理模块，提供系统化的 Prompt 模板管理、版本控制、调优和评估能力。模板按认知（Cognition）、记忆（Memory）、安全（Security）、系统（System）四大类别组织，支持 A/B 测试和数据集驱动的自动调优，帮助开发者持续优化智能体行为。

## 目录结构

```
prompts/
├── registry.yaml             # 提示词注册表（版本/分类/状态）
├── templates/
│   ├── cognition/            # 认知类提示词
│   │   ├── intent_classify.yaml   # 意图分类
│   │   ├── entity_extract.yaml    # 实体提取
│   │   ├── plan_generate.yaml     # 计划生成
│   │   └── reflection.yaml        # 反思
│   ├── memory/               # 记忆类提示词
│   │   ├── extract_facts.yaml     # 事实提取
│   │   ├── dedup_decision.yaml    # 去重决策
│   │   ├── summarize.yaml         # 摘要压缩
│   │   └── rule_generate.yaml     # 规则归纳
│   ├── security/             # 安全类提示词
│   │   ├── code_review.yaml       # 代码安全审查
│   │   ├── security_scan.yaml     # 安全扫描
│   │   └── input_validate.yaml    # 输入校验
│   └── system/               # 系统类提示词
│       ├── default_agent.yaml     # 默认智能体
│       ├── coding_agent.yaml      # 编码智能体
│       └── research_agent.yaml    # 研究智能体
├── datasets/                 # 评估数据集
│   ├── cognition/
│   │   ├── dataset_v1.jsonl
│   │   └── dataset_v2.jsonl
│   ├── memory/
│   │   └── dataset_v1.jsonl
│   ├── security/
│   │   └── dataset_v1.jsonl
│   └── system/
│       └── dataset_v1.jsonl
├── tuner/                    # 提示词调优工具
│   ├── __init__.py
│   ├── scorer.py             # 评分器
│   ├── evaluate.py           # 评估器
│   └── ab_test.py            # A/B 测试
└── README.md                 # 本文件
```

## 提示词分类

| 分类 | 数量 | 说明 |
|------|:----:|------|
| **Cognition** | 4 | 意图分类、实体提取、计划生成、反思 |
| **Memory** | 4 | 事实提取、去重决策、摘要压缩、规则归纳 |
| **Security** | 3 | 代码审查、安全扫描、输入校验 |
| **System** | 3 | 默认智能体、编码智能体、研究智能体 |

## 注册表管理

`registry.yaml` 集中管理所有提示词的元信息：

```yaml
prompts:
  - name: "intent_classify"
    version: "1.0.0"
    category: "cognition"
    path: "templates/cognition/intent_classify.yaml"
    status: "stable"  # stable | testing | deprecated
```

### 状态说明

| 状态 | 说明 |
|------|------|
| `stable` | 生产可用，经过充分测试 |
| `testing` | 测试中，可能存在变更 |
| `deprecated` | 已弃用，计划移除 |

## 提示词模板结构

每个模板为 YAML 格式，包含以下字段：

```yaml
name: intent_classify
version: 1.0.0
description: "将用户输入分类为预定义的意图类别"
model_family: any
parameters:
  temperature: 0.3
  max_tokens: 256
messages:
  - role: system
    content: |
      你是一个意图分类器...
  - role: user
    content: |
      用户输入: {{input}}
      可选的类别: {{categories}}
```

## 调优工具 (Tuner)

提供 Prompt 调优和评估能力，支持数据驱动的迭代优化。

### 评分器 (`scorer.py`)

```python
from tuner.scorer import Scorer

scorer = Scorer()
score = scorer.score(
    prompt_name="intent_classify",
    dataset_path="datasets/cognition/dataset_v1.jsonl"
)
```

### 评估器 (`evaluate.py`)

```python
from tuner.evaluate import Evaluator

evaluator = Evaluator()
report = evaluator.evaluate(
    prompt_name="intent_classify",
    version="1.0.0"
)
```

### A/B 测试 (`ab_test.py`)

```python
from tuner.src.ab_test import ABTest

ab = ABTest()
result = ab.compare(
    prompt_name="intent_classify",
    baseline="1.0.0",
    candidate="1.1.0",
    dataset="datasets/cognition/dataset_v1.jsonl"
)
```

## 使用示例 (CLI)

```bash
# 列出所有提示词
agentrt prompt list

# 查看提示词详情
agentrt prompt show intent_classify

# 调优提示词
agentrt prompt tune intent_classify --dataset ./datasets/cognition/dataset_v1.jsonl

# A/B 测试
agentrt prompt ab-test intent_classify --baseline v1 --candidate v2
```

## 扩展指南

### 添加新提示词

1. 在对应分类目录下创建 YAML 模板文件
2. 在 `registry.yaml` 中注册新提示词
3. （可选）添加评估数据集到对应分类目录
4. 使用 CLI 验证：`agentrt prompt show <name>`

### 添加新分类

1. 在 `templates/` 下创建新分类目录
2. 添加提示词模板文件
3. 更新 `registry.yaml` 注册新分类的提示词

---

© 2026 SPHARX Ltd. All Rights Reserved.