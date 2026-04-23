# Frameworks — 框架集成层

**路径**: `agentos/atoms/frameworks/`

Frameworks 层是 AgentOS 连接外部 AI 框架的桥梁，采用适配器模式将各种外部 AI 框架（LangChain、MCP、A2A、OpenAI 等）无缝集成到 AgentOS 的运行时体系中。

---

## 设计理念

- **适配器模式**: 每个外部框架被封装为独立的适配器，通过统一接口接入
- **能力模型**: 定义标准的"能力"抽象，框架适配器只需声明其支持的能力集合
- **热插拔**: 框架适配器可在运行时动态加载和卸载，无需重启系统
- **解耦**: 上层应用不直接依赖任何特定框架，通过能力模型进行调用

---

## 架构总览

```
┌──────────────────────────────────────────────────────┐
│                    Application                         │
│  (通过能力模型调用，不感知具体框架)                     │
└────────────────────┬─────────────────────────────────┘
                     │
┌────────────────────▼─────────────────────────────────┐
│              Framework Manager                         │
│  ┌────────┐  ┌────────┐  ┌────────┐  ┌────────┐     │
│  │ Route  │  │ Config │  │ Health │  │ Metrics│     │
│  │ Router │  │ Manager│  │ Check  │  │ Collector│   │
│  └────────┘  └────────┘  └────────┘  └────────┘     │
└────────────────────┬─────────────────────────────────┘
                     │
    ┌────────────────┼────────────────┬──────────────┐
    │                │                │              │
    ▼                ▼                ▼              ▼
┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐
│ LangChain│  │   MCP    │  │   A2A    │  │  OpenAI  │
│ Adapter  │  │ Adapter  │  │ Adapter  │  │ Adapter  │
└──────────┘  └──────────┘  └──────────┘  └──────────┘
```

---

## 能力模型

Frameworks 层定义了一套标准的能力模型，每个框架适配器声明其支持的能力子集：

| 能力 | 说明 | LangChain | MCP | A2A | OpenAI |
|------|------|-----------|-----|-----|--------|
| text_generation | 文本生成 | ✅ | ✅ | ✅ | ✅ |
| chat_completion | 对话补全 | ✅ | ✅ | ✅ | ✅ |
| embedding | 文本嵌入 | ✅ | ✅ | ✅ | ✅ |
| tool_use | 工具调用 | ✅ | ✅ | ✅ | ✅ |
| agent_loop | 自主循环 | ✅ | ✅ | ✅ | ✅ |
| code_interpreter | 代码执行 | ✅ | ✅ | ✅ | ✅ |
| file_io | 文件读写 | ✅ | ✅ | ✅ | ✅ |
| web_search | 网络搜索 | ✅ | ✅ | ✅ | ✅ |
| image_generation | 图像生成 | ✅ | ✅ | ✅ | ✅ |
| audio_transcribe | 音频转录 | ✅ | ✅ | ✅ | ✅ |
| memory | 记忆管理 | ✅ | ✅ | ✅ | ✅ |
| retrieval | 知识检索 | ✅ | ✅ | ✅ | ✅ |
| multi_modal | 多模态 | ✅ | ✅ | ✅ | ✅ |

---

## 适配器接口

每个框架适配器需要实现以下标准接口：

```python
class FrameworkAdapter:
    """框架适配器基类"""

    @property
    def name(self) -> str: ...

    @property
    def capabilities(self) -> list[Capability]: ...

    async def initialize(self, config: dict) -> None: ...

    async def execute(self, capability: Capability,
                      params: dict) -> ExecutionResult: ...

    async def shutdown(self) -> None: ...

    async def health_check(self) -> HealthStatus: ...
```

---

## 框架管理

Framework Manager 负责适配器的生命周期管理和调用路由：

1. **注册**: 框架适配器启动时向 Manager 注册其能力
2. **路由**: 根据请求的能力动态选择合适的框架
3. **负载均衡**: 在支持同一能力的多个框架间分发请求
4. **故障转移**: 某框架不可用时自动切换到备用框架
5. **熔断保护**: 对异常框架进行自动熔断和恢复

---

## 配置示例

```json
{
  "frameworks": {
    "langchain": {
      "enabled": true,
      "provider": "openai",
      "model": "gpt-4",
      "api_key": "${OPENAI_API_KEY}"
    },
    "mcp": {
      "enabled": true,
      "servers": ["local", "remote"]
    }
  },
  "routing": {
    "strategy": "capability_first",
    "fallback": true
  }
}
```

---

## 与上层模块的关系

- **CoreLoopThree**: 在执行循环中通过 Frameworks 层调用外部 AI 能力
- **Daemon 服务**: LLM 守护进程(LLM_d)使用 Frameworks 层管理与 LLM 的通信
- **Toolkit SDK**: 客户端 SDK 通过 Frameworks 层与外部框架交互

---

© 2026 SPHARX Ltd. All Rights Reserved.
