# weather-agent — 自定义工具 + MCP 协议

> 难度：初级 | 预计时间：15分钟 | 主题：ToolPlugin 开发与 MCP 协议

## 项目说明

本项目演示如何为 AgentRT 开发自定义工具（ToolPlugin），并通过 MCP（Model Context Protocol）协议注册给 Agent 使用。

`weather-agent` 配备了一个模拟天气查询工具，用户可以询问任意城市的天气情况，Agent 会自动调用 `weather_tool` 获取结果。

### 你将学到

- 如何继承 `ToolPlugin` 开发自定义工具
- 如何定义工具的参数 Schema（`ToolMetadata` / `ToolParameter`）
- 如何在 `agent.yaml` 中注册工具
- MCP 协议的基本概念和工具发现机制

## 目录结构

```
weather-agent/
├── README.md                     # 本文件
├── config.yaml                   # 运行时配置
├── agents/
│   └── weather.agent.yaml        # Agent 定义，注册 weather_tool
└── tools/
    └── weather_tool.py           # 天气查询工具实现（ToolPlugin 子类）
```

## 运行方式

```bash
# 1. 启动 Agent
agentrt run --config config.yaml

# 2. 与 Agent 对话
curl -X POST http://localhost:8080/api/v1/chat \
  -H "Content-Type: application/json" \
  -d '{"message": "北京今天天气怎么样？"}'

# 3. 直接调用工具（调试用）
python -m tools.weather_tool
```

## 关键概念

### 1. ToolPlugin — 自定义工具开发

所有 AgentRT 工具都继承自 `ToolPlugin`，需要实现两个核心方法：

```python
from agentos.plugin_types import ToolPlugin, ToolMetadata, ToolParameter

class WeatherTool(ToolPlugin):
    def get_metadata(self) -> ToolMetadata:
        # 声明工具的名称、描述、参数
        return ToolMetadata(
            name="get_weather",
            description="查询城市天气",
            parameters=[
                ToolParameter(name="city", type="string",
                              description="城市名称", required=True),
            ],
        )

    async def execute(self, params):
        # 工具的实际执行逻辑
        city = params["city"]
        return {"temperature": 22, "condition": "sunny"}
```

| 方法 | 说明 |
|------|------|
| `get_metadata()` | 返回工具元数据，供 Agent 发现和调用 |
| `execute(params)` | 工具执行入口，接收参数字典，返回结果 |
| `validate_params(params)` | 可选，自定义参数校验逻辑 |
| `on_error(error, params)` | 可选，错误处理回调 |

### 2. MCP 协议 — 工具发现与调用

MCP（Model Context Protocol）是 AgentRT 工具注册和调用的标准协议：

- **工具注册**：ToolPlugin 通过 `get_metadata()` 声明工具的能力和参数
- **工具发现**：Agent 启动时自动扫描 `tools/` 目录，发现并注册所有工具
- **工具调用**：Agent 在推理过程中决定调用哪个工具，传入参数，获取结果
- **结果回传**：工具结果通过 MCP 协议回传给 Agent，继续推理

```
用户提问 → Agent 推理 → 决定调用 get_weather(city="北京")
    → MCP 协议调用 → WeatherTool.execute() → 返回天气数据
    → Agent 继续推理 → 生成最终回答
```

### 3. agent.yaml 中的工具注册

```yaml
tools:
  - name: "get_weather"
    module: "tools.weather_tool"
    class: "WeatherTool"
```

Agent 启动时会根据此配置加载工具，使其在推理过程中可被调用。

## 扩展建议

- **接入真实 API**：将 `weather_tool.py` 中的模拟数据替换为真实天气 API（如 OpenWeatherMap）
- **添加更多参数**：支持 `unit`（摄氏/华氏）、`forecast_days`（预报天数）等参数
- **工具链组合**：结合 `http_skill` 实现网络请求，结合 `python_skill` 实现数据处理
- **错误重试**：重写 `on_error` 方法实现优雅降级（如 API 不可用时返回缓存数据）
- **缓存优化**：利用 `config.yaml` 中的 `cache` 配置，避免重复查询同一城市
