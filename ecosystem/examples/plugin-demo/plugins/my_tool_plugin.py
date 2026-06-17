# ToolPlugin 示例 — 天气查询工具
# 演示如何通过 ToolPlugin 为 Agent 提供可调用的外部工具

from agentos.plugin_types import ToolPlugin


class WeatherTool(ToolPlugin):
    """天气查询工具，Agent 可通过该工具获取指定城市的天气信息。"""

    name = "weather_query"
    description = "查询指定城市的当前天气信息，包括温度、湿度、天气状况"

    # 工具参数 Schema（JSON Schema 格式）
    parameters = {
        "type": "object",
        "properties": {
            "city": {
                "type": "string",
                "description": "要查询天气的城市名称，如：北京、上海",
            },
            "unit": {
                "type": "string",
                "enum": ["celsius", "fahrenheit"],
                "description": "温度单位，默认为摄氏度",
            },
        },
        "required": ["city"],
    }

    async def execute(self, **kwargs) -> str:
        """
        执行天气查询。
        Agent 会在需要时自动调用此方法，传入参数符合 parameters Schema。
        """
        city = kwargs["city"]
        unit = kwargs.get("unit", "celsius")

        # 实际项目中调用天气 API，此处为模拟数据
        weather_data = self._mock_weather(city, unit)
        return weather_data

    def _mock_weather(self, city: str, unit: str) -> str:
        """模拟天气数据返回。"""
        temp = 25 if unit == "celsius" else 77
        return (
            f"城市：{city}\n"
            f"温度：{temp}°{'C' if unit == 'celsius' else 'F'}\n"
            f"湿度：65%\n"
            f"天气：晴转多云"
        )
