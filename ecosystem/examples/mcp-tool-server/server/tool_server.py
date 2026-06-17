# MCP 工具服务器实现
# 基于 MCP 协议提供计算器和文件读取两个工具

import asyncio
import sys
from pathlib import Path

# 将工具目录加入路径，以便导入工具模块
sys.path.insert(0, str(Path(__file__).parent.parent))

from mcp.server import MCPServer, Tool
from tools.calculator import CalculatorTool
from tools.file_reader import FileReaderTool


class MyToolServer(MCPServer):
    """自定义 MCP 工具服务器，提供计算器和文件读取工具。"""

    name = "my-tool-server"
    version = "1.0.0"

    def __init__(self):
        super().__init__()
        # 初始化工具实例
        self._tools = {
            "calculator": CalculatorTool(),
            "file_reader": FileReaderTool(),
        }

    async def list_tools(self) -> list[Tool]:
        """返回服务器提供的所有工具列表。"""
        return [
            Tool(
                name="calculator",
                description="数学表达式计算器，支持加减乘除和常用数学函数",
                parameters={
                    "type": "object",
                    "properties": {
                        "expression": {
                            "type": "string",
                            "description": "数学表达式，如 '2 + 3 * 4'、'sqrt(16)'、'sin(3.14)'",
                        }
                    },
                    "required": ["expression"],
                },
            ),
            Tool(
                name="file_reader",
                description="读取指定文件的内容，支持文本文件",
                parameters={
                    "type": "object",
                    "properties": {
                        "path": {
                            "type": "string",
                            "description": "要读取的文件路径",
                        },
                        "encoding": {
                            "type": "string",
                            "description": "文件编码，默认为 utf-8",
                        },
                        "max_lines": {
                            "type": "integer",
                            "description": "最大读取行数，默认读取全部",
                        },
                    },
                    "required": ["path"],
                },
            ),
        ]

    async def call_tool(self, name: str, arguments: dict) -> str:
        """调用指定工具并返回结果。"""
        if name not in self._tools:
            return f"错误：未知工具 '{name}'，可用工具：{list(self._tools.keys())}"

        tool = self._tools[name]
        try:
            result = await tool.execute(**arguments)
            return result
        except Exception as e:
            return f"工具执行错误：{e}"


def main():
    """启动 MCP 工具服务器。"""
    server = MyToolServer()
    # 使用 stdio 传输方式启动
    asyncio.run(server.run(transport="stdio"))


if __name__ == "__main__":
    main()
