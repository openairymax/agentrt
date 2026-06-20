# AgentRT Official Skills

> 官方技能集合，基于 `SkillPlugin` 基类实现，提供可复用的智能体能力。

## 技能列表

| 技能 | 类别 | 版本 | 说明 |
|------|------|:---:|------|
| [code_review](code_review.md) | development | 1.0.0 | 多维度代码审查：安全漏洞、性能问题、最佳实践偏差 |
| [text_summarization](text_summarization.md) | text-processing | 1.0.0 | 长文本智能摘要：提取式、抽象式、要点式、精简式 |
| [security_audit](security_audit.md) | security | 1.0.0 | 系统安全审计：配置、依赖、权限、网络、合规 |

## 架构

所有技能继承 `SkillPlugin` 基类（定义于 `sdk/python/agentos/plugin_types.py`），实现 `get_definition()` 和 `execute()` 方法。

```
SkillPlugin
├── CodeReviewSkill        # 代码审查
├── TextSummarizationSkill  # 文本摘要
└── SecurityAuditSkill      # 安全审计
```

## 使用方式

```python
from ecosystem.skills import CodeReviewSkill
import asyncio

skill = CodeReviewSkill()
result = asyncio.run(skill.execute({
    "code": "your code here",
    "language": "python"
}))
```

## 开发新技能

1. 继承 `SkillPlugin` 基类
2. 实现 `get_definition()` → 返回 `SkillDefinition`
3. 实现 `execute(parameters)` → 返回技能执行结果
4. 编写配套 `.md` 文档