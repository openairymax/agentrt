# 计算器工具 — MCP 工具示例
# 演示如何实现一个 MCP 工具：数学表达式计算器

import ast
import math
import operator
import re

# 安全运算符映射
_SAFE_OPS = {
    ast.Add: operator.add,
    ast.Sub: operator.sub,
    ast.Mult: operator.mul,
    ast.Div: operator.truediv,
    ast.FloorDiv: operator.floordiv,
    ast.Mod: operator.mod,
    ast.Pow: operator.pow,
    ast.USub: operator.neg,
    ast.UAdd: operator.pos,
}


def _safe_eval(expr: str, allowed_names: dict) -> float:
    """使用 AST 安全求值数学表达式，替代 eval()。

    仅允许数字、基本运算符和白名单中的函数名，
    防止任意代码注入。
    """
    tree = ast.parse(expr, mode="eval")

    def _eval_node(node):
        if isinstance(node, ast.Expression):
            return _eval_node(node.body)
        elif isinstance(node, ast.Constant):
            return node.value
        elif isinstance(node, ast.BinOp):
            op_type = type(node.op)
            if op_type not in _SAFE_OPS:
                raise ValueError(f"不允许的运算符: {op_type.__name__}")
            left = _eval_node(node.left)
            right = _eval_node(node.right)
            return _SAFE_OPS[op_type](left, right)
        elif isinstance(node, ast.UnaryOp):
            op_type = type(node.op)
            if op_type not in _SAFE_OPS:
                raise ValueError(f"不允许的一元运算符: {op_type.__name__}")
            operand = _eval_node(node.operand)
            return _SAFE_OPS[op_type](operand)
        elif isinstance(node, ast.Call):
            func_name = node.func.id
            if func_name not in allowed_names:
                raise ValueError(f"不允许的函数: {func_name}")
            args = [_eval_node(arg) for arg in node.args]
            return allowed_names[func_name](*args)
        elif isinstance(node, ast.Name):
            name = node.id
            if name not in allowed_names:
                raise ValueError(f"不允许的变量: {name}")
            return allowed_names[name]
        else:
            raise ValueError(f"不支持的表达式类型: {type(node).__name__}")

    return _eval_node(tree)


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
            # 使用安全 AST 求值代替 eval()（BAN 合规）
            result = _safe_eval(expression, self.SAFE_FUNCTIONS)
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
