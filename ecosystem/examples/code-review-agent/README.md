# code-review-agent — 代码审查 Skill + 安全扫描

> 难度：中级 | 预计时间：25分钟 | 主题：SkillPlugin 与安全审查

## 项目说明

本项目演示 AgentRT 的 Skill（技能）系统。`code-review-agent` 配备了代码审查技能，能对代码进行多维度审查，包括安全漏洞检测、性能分析和最佳实践检查。

同时展示了如何利用 AgentRT 生态系统中已有的 `CodeReviewSkill`，以及如何在此基础上扩展自定义审查逻辑。

### 你将学到

- SkillPlugin 与 ToolPlugin 的区别和适用场景
- 如何开发自定义 Skill 并在 Agent 中激活
- 代码安全扫描的基本流程
- 如何利用 `ecosystem.skills` 中已有的技能

## 目录结构

```
code-review-agent/
├── README.md                        # 本文件
├── config.yaml                      # 运行时配置（含安全策略）
├── agents/
│   └── code_review.agent.yaml       # Agent 定义，注册代码审查技能
└── skills/
    └── code_review_skill.py         # 自定义代码审查技能实现
```

## 运行方式

```bash
# 1. 启动 Agent
agentrt run --config config.yaml

# 2. 提交代码审查任务
curl -X POST http://localhost:8080/api/v1/chat \
  -H "Content-Type: application/json" \
  -d '{
    "message": "请审查以下代码的安全性",
    "context": {
      "code": "password = \"hardcoded_secret\"\neval(user_input)",
      "language": "python"
    }
  }'

# 3. 直接测试技能
python -m skills.code_review_skill
```

## 关键概念

### 1. Skill vs Tool — 何时用哪个？

| 特性 | Tool（工具） | Skill（技能） |
|------|-------------|--------------|
| 粒度 | 单一操作 | 多步骤组合 |
| 复杂度 | 简单输入→输出 | 包含提示词模板+执行逻辑+后处理 |
| 典型场景 | 查询天气、搜索文档 | 代码审查、文档生成 |
| 交互模式 | Agent 按需调用 | Agent 激活后持续作用 |
| 提示词 | 无 | 可附带系统指令和提示词模板 |

### 2. SkillPlugin 核心方法

```python
from agentos.plugin_types import SkillPlugin, SkillDefinition

class CodeReviewSkill(SkillPlugin):
    def get_definition(self) -> SkillDefinition:
        # 声明技能的名称、描述、输入输出 Schema
        return SkillDefinition(name="code_review", ...)

    def get_prompt_template(self) -> str:
        # 可选：返回提示词模板，指导 LLM 执行此技能
        return "Perform a {focus} code review on {language} code..."

    def get_system_instructions(self) -> str:
        # 可选：返回额外的系统指令
        return "You are a senior code reviewer..."

    async def pre_execute(self, context):
        # 执行前预处理（如快速扫描）
        return context

    async def execute(self, context):
        # 核心执行逻辑
        return {"findings": [...], "score": 85}

    async def post_execute(self, context, result):
        # 执行后处理（如过滤低严重级别发现）
        return result
```

### 3. 安全扫描流程

本示例的代码审查技能实现了两级安全扫描：

```
输入代码
  ├── 快速扫描（_quick_scan）：正则匹配硬编码密钥、eval()、SQL 注入等
  ├── 静态分析（_static_analysis）：行级检查，如裸 except、通配符导入
  └── 结果合并 → 去重 → 按严重级别排序 → 评分
```

严重级别：`critical` > `high` > `medium` > `low` > `info`

### 4. 利用 ecosystem.skills

AgentRT 生态系统中已内置 `CodeReviewSkill`（位于 `ecosystem/skills/code_review.py`）。本示例展示了两种用法：

- **直接引用**：在 `agent.yaml` 中引用 `ecosystem.skills.code_review:CodeReviewSkill`
- **自定义扩展**：继承并扩展，添加项目特定的审查规则

## 扩展建议

- **集成 LLM 深度审查**：在 `execute` 中调用 LLM 对代码进行语义级分析
- **添加自定义规则**：在 `_quick_scan` 中添加项目特定的安全模式检测
- **集成 SAST 工具**：调用 Semgrep、Bandit 等静态分析工具增强检测能力
- **审查报告持久化**：将审查结果写入记忆系统，支持历史追踪
- **CI/CD 集成**：通过 Hook 在代码提交时自动触发审查
