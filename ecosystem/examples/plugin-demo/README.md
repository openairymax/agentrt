# Plugin Demo - Plugin SDK 开发教程

> 难度：初级 | 主题：AgentRT 插件开发

## 项目说明

本项目演示 AgentRT 的四种插件类型及其开发方式。通过四个独立的插件文件，展示如何扩展 AgentRT 的核心能力。

### 四种插件类型

| 插件类型 | 基类 | 用途 | 示例场景 |
|---------|------|------|---------|
| **AgentPlugin** | `AgentPlugin` | 自定义 Agent 行为和推理策略 | RAG Agent、反思 Agent |
| **ToolPlugin** | `ToolPlugin` | 为 Agent 提供可调用的工具 | 搜索工具、计算器工具 |
| **HookPlugin** | `HookPlugin` | 在 Agent 生命周期中注入钩子逻辑 | 日志记录、权限检查 |
| **SkillPlugin** | `SkillPlugin` | 封装可复用的技能组合 | 代码审查技能、文档生成技能 |

## 目录结构

```
plugin-demo/
├── README.md                    # 本文件
├── config.yaml                  # AgentRT 配置，注册所有插件
└── plugins/
    ├── my_agent_plugin.py       # AgentPlugin 示例
    ├── my_tool_plugin.py        # ToolPlugin 示例
    ├── my_hook_plugin.py        # HookPlugin 示例
    └── my_skill_plugin.py       # SkillPlugin 示例
```

## 运行方式

```bash
# 1. 确保 AgentRT 已安装
pip install agentrt

# 2. 运行 AgentRT，加载配置
agentrt run --config config.yaml

# 3. 或单独测试某个插件
python -m plugins.my_tool_plugin
```

## 关键概念

### 1. AgentPlugin — 自定义 Agent 行为

AgentPlugin 允许你定义 Agent 的推理策略和行为模式。通过重写 `run` 方法，可以实现自定义的思考-行动循环。

```python
from agentos.plugin_types import AgentPlugin

class MyAgent(AgentPlugin):
    async def run(self, task: str) -> str:
        # 自定义推理逻辑
        ...
```

**适用场景**：需要特殊推理策略的 Agent，如 RAG Agent、反思 Agent、多步规划 Agent。

### 2. ToolPlugin — 可调用的工具

ToolPlugin 为 Agent 提供可调用的外部工具。每个工具需要声明参数 Schema，Agent 会在需要时自动调用。

```python
from agentos.plugin_types import ToolPlugin

class MyTool(ToolPlugin):
    name = "my_tool"
    description = "工具描述"
    parameters = {...}  # JSON Schema

    async def execute(self, **kwargs) -> str:
        # 工具执行逻辑
        ...
```

**适用场景**：搜索、计算、文件操作、API 调用等需要与外部系统交互的场景。

### 3. HookPlugin — 生命周期钩子

HookPlugin 在 Agent 的关键生命周期节点注入自定义逻辑，如请求前后的日志记录、权限校验等。

```python
from agentos.plugin_types import HookPlugin

class MyHook(HookPlugin):
    async def on_before_invoke(self, context: dict) -> dict:
        # 请求前处理
        ...

    async def on_after_invoke(self, context: dict) -> dict:
        # 请求后处理
        ...
```

**适用场景**：日志审计、权限控制、请求/响应改写、性能监控。

### 4. SkillPlugin — 可复用技能

SkillPlugin 封装一组工具和提示词为可复用的技能组合，Agent 可以按需激活。

```python
from agentos.plugin_types import SkillPlugin

class MySkill(SkillPlugin):
    name = "my_skill"
    description = "技能描述"

    async def execute(self, task: str, context: dict) -> str:
        # 技能执行逻辑，可组合多个工具
        ...
```

**适用场景**：代码审查、文档生成、数据分析等需要多步骤、多工具协作的复合任务。

### 插件注册

在 `config.yaml` 中通过 `plugins` 字段注册插件：

```yaml
plugins:
  agents:
    - plugins.my_agent_plugin:MyCustomAgent
  tools:
    - plugins.my_tool_plugin:WeatherTool
  hooks:
    - plugins.my_hook_plugin:LoggingHook
  skills:
    - plugins.my_skill_plugin:CodeReviewSkill
```

## 扩展建议

- **组合使用**：在实际项目中，通常需要组合多种插件类型。例如，一个 RAG Agent（AgentPlugin）可能依赖搜索工具（ToolPlugin）和日志钩子（HookPlugin）。
- **参数化插件**：通过 `__init__` 接受配置参数，使插件更灵活可复用。
- **错误处理**：在 `execute` 方法中做好异常捕获，返回友好的错误信息给 Agent。
- **单元测试**：为每个插件编写独立的单元测试，确保插件行为正确。
- **插件市场**：开发通用插件后，可以发布到 AgentRT 插件市场供其他用户使用。
