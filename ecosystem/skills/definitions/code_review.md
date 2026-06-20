# Code Review Skill

> **版本**: 1.0.0 | **类别**: development | **标签**: code-review, security, quality, best-practices

## 概述

对代码进行多维度结构化审查，识别安全漏洞、性能问题和最佳实践偏差。

## 审查维度

| 维度 | 说明 |
|------|------|
| 安全性 (security) | SQL注入、XSS、硬编码密钥、eval/exec 误用 |
| 性能 (performance) | N+1查询、内存泄漏、不必要的拷贝 |
| 可维护性 (maintainability) | 代码复杂度、命名规范、TODO/FIXME 标记 |
| 正确性 (correctness) | 逻辑错误、边界条件、类型安全、裸 except |
| 风格 (style) | 行长度、通配符导入、语言惯例 |

## 输入参数

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|:---:|------|------|
| code | string | ✅ | — | 要审查的代码片段 |
| language | string | ✅ | — | 编程语言 (python/rust/javascript/go/c/java) |
| focus | string | ❌ | all | 审查重点 (security/performance/maintainability/all) |
| severity_threshold | string | ❌ | low | 最低报告严重级别 (info/low/medium/high/critical) |

## 输出格式

```json
{
  "summary": "Code review (python): score 75/100, 1 critical, 2 medium",
  "overall_score": 75.0,
  "findings": [
    {
      "id": "qs-1",
      "severity": "critical",
      "category": "security",
      "title": "Hardcoded secret or API key",
      "description": "Pattern detected: Hardcoded secret or API key",
      "location": "quick-scan",
      "suggestion": "Review and remediate"
    }
  ],
  "language": "python",
  "focus": "all",
  "lines_reviewed": 10
}
```

## 评分机制

- 基础分 100，按发现的问题严重级别扣分：
  - critical: -25 | high: -15 | medium: -8 | low: -3 | info: -1
- 最低 0 分，最高 100 分

## 使用示例

```python
from ecosystem.skills import CodeReviewSkill
import asyncio

skill = CodeReviewSkill()
result = asyncio.run(skill.execute({
    "code": 'password = "secret123"',
    "language": "python",
    "focus": "security"
}))
print(result["overall_score"])  # 75.0
print(len(result["findings"]))  # 1 (critical: hardcoded secret)
```

## 实现文件

- [code_review.py](code_review.py) — SkillPlugin 实现
