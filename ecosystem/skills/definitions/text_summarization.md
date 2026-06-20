# Text Summarization Skill

> **版本**: 1.0.0 | **类别**: text-processing | **标签**: summarization, nlp, text-processing, compression

## 概述

对长文本进行智能压缩和摘要生成，支持多种摘要模式。

## 摘要模式

| 模式 | 说明 |
|------|------|
| 提取式 (extractive) | 从原文中提取关键句子，保留原文表述 |
| 抽象式 (abstractive) | 生成全新的概括性摘要，用自己的话重新组织 |
| 要点式 (bullet) | 提取关键要点，以列表形式呈现 |
| 精简式 (concise) | 极简摘要，一句话概括核心内容 |

## 输入参数

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|:---:|------|------|
| text | string | ✅ | — | 要摘要的原始文本 |
| mode | string | ❌ | abstractive | 摘要模式 |
| max_length | integer | ❌ | 500 | 摘要最大长度（字符数） |
| language | string | ❌ | auto | 文本语言 (zh/en/auto) |
| focus | string | ❌ | general | 摘要关注点 (general/technical/business/news) |

## 输出格式

```json
{
  "summary": "This is the generated summary...",
  "mode": "abstractive",
  "original_length": 5000,
  "summary_length": 200,
  "compression_ratio": 96.0,
  "key_points": ["Key point 1", "Key point 2"],
  "language": "zh",
  "focus": "general"
}
```

## 使用示例

```python
from ecosystem.skills import TextSummarizationSkill
import asyncio

skill = TextSummarizationSkill()
result = asyncio.run(skill.execute({
    "text": "长文本内容...",
    "mode": "bullet",
    "max_length": 300
}))
print(result["summary"])  # • 要点1\n• 要点2\n• 要点3
print(result["compression_ratio"])  # 94.5
```

## 实现文件

- [text_summarization.py](text_summarization.py) — SkillPlugin 实现