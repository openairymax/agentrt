"""CodeReviewSkill — 代码审查技能

对代码进行结构化审查，识别安全漏洞、性能问题和最佳实践偏差。
"""

from __future__ import annotations

import json
import logging
import re
from typing import Any, Dict, List, Optional

import sys as _sys
from pathlib import Path as _Path

# 确保 agentos 包可导入（开发模式）
_sdk_python = _Path(__file__).resolve().parents[3] / "sdk" / "python"
if str(_sdk_python) not in _sys.path:
    _sys.path.insert(0, str(_sdk_python))

from agentos.plugin_types import (
    SkillPlugin,
    SkillDefinition,
)

logger = logging.getLogger(__name__)


class CodeReviewSkill(SkillPlugin):
    """代码审查技能。

    对输入的代码片段进行多维度审查，输出结构化的审查报告。

    审查维度：
    1. 安全性 (security) — SQL注入、XSS、硬编码密钥等
    2. 性能 (performance) — N+1查询、内存泄漏、不必要的拷贝
    3. 可维护性 (maintainability) — 代码复杂度、命名、注释
    4. 正确性 (correctness) — 逻辑错误、边界条件、类型安全
    5. 风格 (style) — 语言惯例、格式一致性
    """

    PLUGIN_TYPE = "skill"

    # ── Skill Definition ──────────────────────────────────

    def get_definition(self) -> SkillDefinition:
        return SkillDefinition(
            name="code_review",
            description="对代码进行多维度结构化审查，识别安全漏洞、性能问题和最佳实践偏差",
            version="1.0.0",
            category="development",
            tags=["code-review", "security", "quality", "best-practices"],
            input_schema={
                "type": "object",
                "properties": {
                    "code": {
                        "type": "string",
                        "description": "要审查的代码片段",
                    },
                    "language": {
                        "type": "string",
                        "description": "编程语言 (python/rust/javascript/go/c/java)",
                    },
                    "focus": {
                        "type": "string",
                        "description": "审查重点 (security/performance/maintainability/all)",
                        "default": "all",
                    },
                    "severity_threshold": {
                        "type": "string",
                        "description": "最低报告严重级别 (info/low/medium/high/critical)",
                        "default": "low",
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
            examples=[
                {
                    "input": "审查一段 Python Flask 路由代码的安全性",
                    "output": "发现 2 个高危 SQL 注入风险，1 个中危硬编码密钥",
                },
                {
                    "input": "审查 Rust 异步代码的性能",
                    "output": "发现 1 个不必要的 clone()，建议使用引用",
                },
            ],
            requires=["llm"],
        )

    def get_prompt_template(self) -> Optional[str]:
        return (
            "Perform a {focus} code review on the following {language} code.\n\n"
            "Code:\n```{language}\n{code}\n```\n\n"
            "Provide findings in JSON format with fields: "
            "id, severity (critical/high/medium/low/info), category, "
            "title, description, location, suggestion."
        )

    def get_system_instructions(self) -> Optional[str]:
        return (
            "You are a senior code reviewer. Analyze code objectively. "
            "Prioritize security and correctness over style. "
            "Provide actionable suggestions with code examples when possible. "
            "Rate severity conservatively — only mark as critical if it poses "
            "a real security risk or data loss risk."
        )

    # ── Validation ────────────────────────────────────────

    def validate_input(self, context: Dict[str, Any]) -> bool:
        code = context.get("code", "")
        language = context.get("language", "")
        if not code or not code.strip():
            logger.warning("CodeReviewSkill: empty code input")
            return False
        supported = {"python", "rust", "javascript", "go", "c", "java", "typescript", "cpp"}
        if language.lower() not in supported:
            logger.warning("CodeReviewSkill: unsupported language '%s'", language)
            return False
        return True

    # ── Execution ─────────────────────────────────────────

    async def pre_execute(self, context: Dict[str, Any]) -> Dict[str, Any]:
        context.setdefault("focus", "all")
        context.setdefault("severity_threshold", "low")
        # 预扫描：快速检测明显的安全问题模式
        code = context.get("code", "")
        context["quick_scan_hints"] = self._quick_scan(code)
        return context

    async def execute(self, context: Dict[str, Any]) -> Any:
        code = context.get("code", "")
        language = context.get("language", "")
        focus = context.get("focus", "all")
        hints = context.get("quick_scan_hints", [])

        # 本地静态分析
        local_findings = self._static_analysis(code, language)

        # 合并快速扫描提示
        all_findings = hints + local_findings

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

        # 计算总分
        score = self._compute_score(unique, len(code.splitlines()))

        return {
            "summary": self._generate_summary(unique, score, language),
            "overall_score": score,
            "findings": unique,
            "language": language,
            "focus": focus,
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

    # ── Quick Scan (regex-based) ──────────────────────────

    def _quick_scan(self, code: str) -> List[Dict[str, str]]:
        """快速正则扫描，检测明显安全模式。"""
        findings = []
        patterns = [
            (r'(?:password|passwd|secret|api_key|apikey)\s*=\s*["\'][^"\']+["\']',
             "critical", "security", "Hardcoded secret or API key"),
            (r'eval\s*\(',
             "high", "security", "Use of eval() — potential code injection"),
            (r'exec\s*\(',
             "high", "security", "Use of exec() — potential code injection"),
            (r'subprocess\.call\s*\([^)]*shell\s*=\s*True',
             "high", "security", "Shell injection risk with subprocess(shell=True)"),
            (r'SELECT\s+.*\+.*FROM',
             "high", "security", "Potential SQL injection via string concatenation"),
            (r'innerHTML\s*=',
             "medium", "security", "XSS risk with innerHTML assignment"),
        ]
        for pattern, severity, category, title in patterns:
            if re.search(pattern, code, re.IGNORECASE):
                findings.append({
                    "id": f"qs-{len(findings)+1}",
                    "severity": severity,
                    "category": category,
                    "title": title,
                    "description": f"Pattern detected: {title}",
                    "location": "quick-scan",
                    "suggestion": "Review and remediate",
                })
        return findings

    # ── Static Analysis ───────────────────────────────────

    def _static_analysis(self, code: str, language: str) -> List[Dict[str, str]]:
        """基于语言的静态分析。"""
        findings = []

        # 通用检查
        lines = code.splitlines()
        for i, line in enumerate(lines, 1):
            stripped = line.strip()

            # 过长的行
            if len(stripped) > 120:
                findings.append({
                    "id": f"sa-{len(findings)+1}",
                    "severity": "info",
                    "category": "style",
                    "title": "Line too long",
                    "description": f"Line {i} exceeds 120 characters ({len(stripped)} chars)",
                    "location": f"line {i}",
                    "suggestion": "Break into multiple lines",
                })

            # TODO/FIXME/HACK 注释
            for marker in ("TODO", "FIXME", "HACK", "XXX"):
                if marker in stripped.upper() and marker.lower() in stripped.lower():
                    findings.append({
                        "id": f"sa-{len(findings)+1}",
                        "severity": "info",
                        "category": "maintainability",
                        "title": f"{marker} comment found",
                        "description": f"Line {i} contains {marker}",
                        "location": f"line {i}",
                        "suggestion": "Resolve or create tracking issue",
                    })

        # Python 特定检查
        if language.lower() in ("python",):
            for i, line in enumerate(lines, 1):
                stripped = line.strip()
                if "except:" in stripped and "except Exception" not in stripped:
                    findings.append({
                        "id": f"sa-{len(findings)+1}",
                        "severity": "medium",
                        "category": "correctness",
                        "title": "Bare except clause",
                        "description": f"Line {i}: bare 'except:' catches all exceptions including KeyboardInterrupt",
                        "location": f"line {i}",
                        "suggestion": "Use 'except Exception:' or specify exception types",
                    })
                if "import *" in stripped:
                    findings.append({
                        "id": f"sa-{len(findings)+1}",
                        "severity": "low",
                        "category": "style",
                        "title": "Wildcard import",
                        "description": f"Line {i}: 'import *' pollutes namespace",
                        "location": f"line {i}",
                        "suggestion": "Import specific names",
                    })

        return findings

    # ── Scoring ───────────────────────────────────────────

    def _compute_score(self, findings: List[Dict[str, str]], lines: int) -> float:
        """计算代码质量评分 (0-100)。"""
        base = 100.0
        penalties = {"critical": 25, "high": 15, "medium": 8, "low": 3, "info": 1}
        for f in findings:
            base -= penalties.get(f.get("severity", "info"), 1)
        return max(0.0, min(100.0, base))

    def _generate_summary(
        self, findings: List[Dict[str, str]], score: float, language: str
    ) -> str:
        """生成审查摘要。"""
        severity_counts: Dict[str, int] = {}
        for f in findings:
            sev = f.get("severity", "info")
            severity_counts[sev] = severity_counts.get(sev, 0) + 1

        parts = [f"Code review ({language}): score {score:.0f}/100"]
        for sev in ("critical", "high", "medium", "low", "info"):
            if sev in severity_counts:
                parts.append(f"{severity_counts[sev]} {sev}")

        return ", ".join(parts)
