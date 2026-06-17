"""CodeReviewSkill — 代码审查技能

对代码进行多维度结构化审查，识别安全漏洞、性能问题和最佳实践偏差。
本示例展示 SkillPlugin 的完整开发流程。
"""

from __future__ import annotations

import re
import logging
from typing import Any, Dict, List, Optional

import sys as _sys
from pathlib import Path as _Path

# 确保 agentos 包可导入（开发模式）
_sdk_python = _Path(__file__).resolve().parents[3] / "sdk" / "python"
if str(_sdk_python) not in _sys.path:
    _sys.path.insert(0, str(_sdk_python))

from agentos.plugin_types import SkillPlugin, SkillDefinition

logger = logging.getLogger(__name__)


class CodeReviewSkill(SkillPlugin):
    """代码审查技能。

    审查维度：
    1. 安全性 (security) — SQL注入、XSS、硬编码密钥
    2. 性能 (performance) — N+1查询、内存泄漏
    3. 可维护性 (maintainability) — 代码复杂度、命名
    4. 正确性 (correctness) — 逻辑错误、边界条件
    5. 风格 (style) — 语言惯例、格式一致性
    """

    PLUGIN_TYPE = "skill"

    # ── 技能定义 ──────────────────────────────────────────

    def get_definition(self) -> SkillDefinition:
        return SkillDefinition(
            name="code_review",
            description="对代码进行多维度结构化审查，识别安全漏洞、性能问题和最佳实践偏差",
            version="1.0.0",
            category="development",
            tags=["code-review", "security", "quality"],
            input_schema={
                "type": "object",
                "properties": {
                    "code": {"type": "string", "description": "要审查的代码"},
                    "language": {"type": "string", "description": "编程语言"},
                    "focus": {
                        "type": "string",
                        "description": "审查重点 (security/performance/all)",
                        "default": "all",
                    },
                },
                "required": ["code", "language"],
            },
            output_schema={
                "type": "object",
                "properties": {
                    "summary": {"type": "string"},
                    "overall_score": {"type": "number"},
                    "findings": {"type": "array"},
                },
            },
            requires=["llm"],
        )

    def get_prompt_template(self) -> Optional[str]:
        return (
            "Perform a {focus} code review on the following {language} code.\n\n"
            "Code:\n```{language}\n{code}\n```\n\n"
            "Provide findings in JSON format: id, severity, category, title, description, suggestion."
        )

    def get_system_instructions(self) -> Optional[str]:
        return (
            "You are a senior code reviewer. "
            "Prioritize security and correctness over style. "
            "Rate severity conservatively."
        )

    # ── 输入校验 ──────────────────────────────────────────

    def validate_input(self, context: Dict[str, Any]) -> bool:
        code = context.get("code", "")
        language = context.get("language", "")
        if not code or not code.strip():
            logger.warning("CodeReviewSkill: 代码输入为空")
            return False
        supported = {"python", "rust", "javascript", "go", "c", "java", "typescript", "cpp"}
        if language.lower() not in supported:
            logger.warning("CodeReviewSkill: 不支持的语言 '%s'", language)
            return False
        return True

    # ── 执行流程 ──────────────────────────────────────────

    async def pre_execute(self, context: Dict[str, Any]) -> Dict[str, Any]:
        context.setdefault("focus", "all")
        context.setdefault("severity_threshold", "low")
        # 预扫描：快速检测明显安全模式
        context["quick_scan_hints"] = self._quick_scan(context.get("code", ""))
        return context

    async def execute(self, context: Dict[str, Any]) -> Any:
        code = context.get("code", "")
        language = context.get("language", "")

        # 本地静态分析
        local_findings = self._static_analysis(code, language)
        # 合并快速扫描结果
        all_findings = context.get("quick_scan_hints", []) + local_findings

        # 去重
        seen = set()
        unique = []
        for f in all_findings:
            key = (f.get("category", ""), f.get("title", ""))
            if key not in seen:
                seen.add(key)
                unique.append(f)

        # 按严重级别排序
        severity_order = {"critical": 0, "high": 1, "medium": 2, "low": 3, "info": 4}
        unique.sort(key=lambda f: severity_order.get(f.get("severity", "info"), 5))

        # 计算评分
        score = self._compute_score(unique, len(code.splitlines()))

        return {
            "summary": self._generate_summary(unique, score, language),
            "overall_score": score,
            "findings": unique,
            "language": language,
            "lines_reviewed": len(code.splitlines()),
        }

    async def post_execute(self, context: Dict[str, Any], result: Any) -> Any:
        # 过滤低于阈值的发现
        threshold = context.get("severity_threshold", "low")
        severity_order = {"info": 0, "low": 1, "medium": 2, "high": 3, "critical": 4}
        min_level = severity_order.get(threshold, 1)

        if isinstance(result, dict) and "findings" in result:
            result["findings"] = [
                f for f in result["findings"]
                if severity_order.get(f.get("severity", "info"), 0) >= min_level
            ]
        return result

    # ── 快速扫描（正则匹配） ──────────────────────────────

    def _quick_scan(self, code: str) -> List[Dict[str, str]]:
        """快速正则扫描，检测明显安全模式。"""
        findings = []
        patterns = [
            (r'(?:password|secret|api_key)\s*=\s*["\'][^"\']+["\']',
             "critical", "security", "硬编码密钥或密码"),
            (r'eval\s*\(',
             "high", "security", "使用 eval() — 潜在代码注入风险"),
            (r'subprocess\.call\s*\([^)]*shell\s*=\s*True',
             "high", "security", "Shell 注入风险 (subprocess shell=True)"),
            (r'SELECT\s+.*\+.*FROM',
             "high", "security", "SQL 注入风险（字符串拼接）"),
        ]
        for pattern, severity, category, title in patterns:
            if re.search(pattern, code, re.IGNORECASE):
                findings.append({
                    "id": f"qs-{len(findings)+1}",
                    "severity": severity,
                    "category": category,
                    "title": title,
                    "suggestion": "请审查并修复",
                })
        return findings

    # ── 静态分析 ──────────────────────────────────────────

    def _static_analysis(self, code: str, language: str) -> List[Dict[str, str]]:
        """基于语言的静态分析。"""
        findings = []
        lines = code.splitlines()

        for i, line in enumerate(lines, 1):
            stripped = line.strip()

            # 过长的行
            if len(stripped) > 120:
                findings.append({
                    "id": f"sa-{len(findings)+1}",
                    "severity": "info",
                    "category": "style",
                    "title": "行过长",
                    "description": f"第 {i} 行超过 120 字符",
                    "suggestion": "拆分为多行",
                })

            # Python 特定：裸 except
            if language.lower() == "python" and "except:" in stripped and "except Exception" not in stripped:
                findings.append({
                    "id": f"sa-{len(findings)+1}",
                    "severity": "medium",
                    "category": "correctness",
                    "title": "裸 except 子句",
                    "description": f"第 {i} 行：裸 except 会捕获所有异常",
                    "suggestion": "使用 'except Exception:' 或指定异常类型",
                })

        return findings

    # ── 评分 ──────────────────────────────────────────────

    def _compute_score(self, findings: List[Dict[str, str]], lines: int) -> float:
        """计算代码质量评分 (0-100)。"""
        base = 100.0
        penalties = {"critical": 25, "high": 15, "medium": 8, "low": 3, "info": 1}
        for f in findings:
            base -= penalties.get(f.get("severity", "info"), 1)
        return max(0.0, min(100.0, base))

    def _generate_summary(self, findings: List[Dict[str, str]], score: float, language: str) -> str:
        """生成审查摘要。"""
        counts: Dict[str, int] = {}
        for f in findings:
            sev = f.get("severity", "info")
            counts[sev] = counts.get(sev, 0) + 1

        parts = [f"代码审查 ({language}): 评分 {score:.0f}/100"]
        for sev in ("critical", "high", "medium", "low", "info"):
            if sev in counts:
                parts.append(f"{counts[sev]} {sev}")

        return ", ".join(parts)


if __name__ == "__main__":
    import asyncio

    async def _test():
        skill = CodeReviewSkill()
        print("技能定义:", skill.get_definition().to_dict())

        # 测试有安全问题的代码
        test_code = 'password = "hardcoded_secret"\neval(user_input)\nexcept:\n    pass'
        result = await skill.execute({"code": test_code, "language": "python"})
        print("审查结果:", result)

    asyncio.run(_test())
