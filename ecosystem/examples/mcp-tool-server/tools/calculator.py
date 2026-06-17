# 计算器工具 — MCP 工具示例
# 演示如何实现一个 MCP 工具：数学表达式计算器

import math
import re


class CalculatorTool:
    """数学表达式计算器，支持基本运算和常用数学函数。"""

    # 允许的安全函数白名单
    SAFE_FUNCTIONS = {
        "abs": abs,
        "round": round,
        "min": min,
        "max": max,
        "sqrt": math.sqrt,
        "pow": math.pow,
        "sin": math.sin,
        "cos": math.cos,
        "tan": math.tan,
        "log": math.log,
        "log10": math.log10,
        "pi": math.pi,
        "e": math.e,
    }

    async def execute(self, expression: str) -> str:
        """
        执行数学表达式计算。
        仅允许数字、运算符和白名单函数，防止代码注入。
        """
        # 安全检查：仅允许数字、运算符、括号和白名单函数名
        if not self._is_safe_expression(expression):
            return f"错误：表达式包含不允许的字符或函数 — {expression}"

        try:
            # 在受限环境中计算表达式
            result = eval(expression, {"__builtins__": {}}, self.SAFE_FUNCTIONS)
            return f"计算结果：{expression} = {result}"
        except ZeroDivisionError:
            return "错误：除零错误"
        except Exception as e:
            return f"计算错误：{e}"

    def _is_safe_expression(self, expression: str) -> bool:
        """检查表达式是否安全（仅包含允许的字符和函数）。"""
        # 移除所有空格
        expr = expression.replace(" ", "")

        # 允许的字符：数字、运算符、括号、小数点、逗号、函数名
        pattern = r'^[0-9+\-*/().,%^_a-zA-Z]+$'
        if not re.match(pattern, expr):
            return False

        # 检查所有标识符是否在白名单中
        identifiers = re.findall(r'[a-zA-Z_][a-zA-Z0-9_]*', expr)
        for ident in identifiers:
            if ident not in self.SAFE_FUNCTIONS:
                return False

        return True
