"""WeatherTool — 天气查询工具

模拟天气查询工具，演示 ToolPlugin 的开发方式。
实际项目中可替换为真实天气 API 调用。
"""

from __future__ import annotations

from typing import Any, Dict

import sys as _sys
from pathlib import Path as _Path

# 确保 agentos 包可导入（开发模式）
_sdk_python = _Path(__file__).resolve().parents[3] / "sdk" / "python"
if str(_sdk_python) not in _sys.path:
    _sys.path.insert(0, str(_sdk_python))

from agentos.plugin_types import ToolPlugin, ToolMetadata, ToolParameter


# 模拟天气数据库
_MOCK_WEATHER = {
    "北京": {"temperature": 28, "condition": "晴", "humidity": 45, "wind": "北风3级"},
    "上海": {"temperature": 32, "condition": "多云", "humidity": 72, "wind": "东南风2级"},
    "深圳": {"temperature": 35, "condition": "雷阵雨", "humidity": 85, "wind": "南风4级"},
    "成都": {"temperature": 26, "condition": "阴", "humidity": 60, "wind": "微风"},
    "杭州": {"temperature": 30, "condition": "晴转多云", "humidity": 55, "wind": "东风2级"},
}


class WeatherTool(ToolPlugin):
    """天气查询工具，根据城市名返回天气信息。"""

    PLUGIN_TYPE = "tool"

    def get_metadata(self) -> ToolMetadata:
        """声明工具元数据，供 Agent 发现和调用。"""
        return ToolMetadata(
            name="get_weather",
            description="查询指定城市的当前天气信息，包括温度、天气状况、湿度和风力",
            version="1.0.0",
            parameters=[
                ToolParameter(
                    name="city",
                    type="string",
                    description="城市名称，如：北京、上海、深圳",
                    required=True,
                ),
            ],
            category="weather",
            tags=["weather", "query", "mock"],
            is_async=True,
            timeout_seconds=10.0,
        )

    async def execute(self, params: Dict[str, Any]) -> Any:
        """执行天气查询。

        Args:
            params: 包含 city 参数的字典

        Returns:
            天气信息字典
        """
        city = params.get("city", "")

        # 查询模拟数据
        weather = _MOCK_WEATHER.get(city)

        if weather:
            return {
                "city": city,
                **weather,
                "source": "mock",
            }

        # 城市不在数据库中时返回默认值
        return {
            "city": city,
            "temperature": 25,
            "condition": "未知",
            "humidity": 50,
            "wind": "无数据",
            "source": "mock",
            "note": f"暂无 {city} 的天气数据，已返回默认值",
        }

    async def on_error(self, error: Exception, params: Dict[str, Any]) -> Dict[str, Any]:
        """错误处理：返回友好的错误信息。"""
        return {
            "error": f"天气查询失败: {str(error)}",
            "city": params.get("city", "unknown"),
            "suggestion": "请检查城市名称是否正确，或稍后重试",
        }


if __name__ == "__main__":
    # 本地测试
    import asyncio

    async def _test():
        tool = WeatherTool()
        print("工具元数据:", tool.get_metadata().to_dict())
        result = await tool.execute({"city": "北京"})
        print("查询结果:", result)
        result2 = await tool.execute({"city": "东京"})
        print("查询结果(未知城市):", result2)

    asyncio.run(_test())
