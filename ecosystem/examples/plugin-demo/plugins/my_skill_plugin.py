# SkillPlugin 示例 — 代码审查技能
# 演示如何通过 SkillPlugin 封装可复用的多步骤技能组合

from agentos.plugin_types import SkillPlugin


class CodeReviewSkill(SkillPlugin):
    """代码审查技能，组合多个步骤完成代码审查任务。"""

    name = "code_review"
    description = "对代码进行自动审查，包括语法检查、风格检查和安全扫描"

    # 技能所需的工具列表（AgentRT 会自动注入）
    required_tools = ["file_reader", "static_analyzer"]

    async def execute(self, task: str, context: dict | None = None) -> str:
        """
        执行代码审查技能。
        组合多个工具和步骤，完成完整的代码审查流程。
        """
        context = context or {}
        file_path = context.get("file_path", task)

        # 步骤1：读取代码文件
        code = await self._read_code(file_path)

        # 步骤2：语法检查
        syntax_result = await self._check_syntax(code)

        # 步骤3：风格检查
        style_result = await self._check_style(code)

        # 步骤4：安全扫描
        security_result = await self._scan_security(code)

        # 步骤5：汇总审查报告
        report = self._generate_report(
            file_path, syntax_result, style_result, security_result
        )

        return report

    async def _read_code(self, file_path: str) -> str:
        """读取代码文件内容。"""
        # 实际项目中调用 file_reader 工具
        return f"# 代码内容来自 {file_path}\ndef hello(): pass"

    async def _check_syntax(self, code: str) -> dict:
        """语法检查。"""
        # 实际项目中调用 static_analyzer 工具
        return {"passed": True, "errors": []}

    async def _check_style(self, code: str) -> dict:
        """代码风格检查。"""
        return {"score": 85, "issues": ["建议添加类型注解"]}

    async def _scan_security(self, code: str) -> dict:
        """安全漏洞扫描。"""
        return {"vulnerabilities": [], "warnings": ["检测到硬编码路径"]}

    def _generate_report(self, file_path: str, syntax: dict,
                         style: dict, security: dict) -> str:
        """生成审查报告。"""
        lines = [
            f"📋 代码审查报告 — {file_path}",
            "=" * 40,
            f"✅ 语法检查：{'通过' if syntax['passed'] else '未通过'}",
            f"📊 风格评分：{style['score']}/100",
        ]
        if style["issues"]:
            lines.append("  风格问题：" + "；".join(style["issues"]))
        if security["vulnerabilities"]:
            lines.append(f"🔴 安全漏洞：{len(security['vulnerabilities'])} 个")
        if security["warnings"]:
            lines.append("⚠️ 安全警告：" + "；".join(security["warnings"]))
        if not security["vulnerabilities"] and not security["warnings"]:
            lines.append("🟢 安全扫描：未发现问题")
        return "\n".join(lines)
