# Python Agent 模板

**模块路径**: `agentos/openlab/markets/templates/python-agent/`
**版本**: v0.0.5

> **Status**: 本模块作为 AgentOS 的正式组成部分，API 持续演进中。本模块通过 JSON-RPC 2.0 协议与 AgentOS 核心运行时集成。

## 概述

Python Agent 模板是 AgentOS 生态市场中提供的标准化 Agent 开发模板，帮助开发者快速创建基于 Python 的智能 Agent 应用。该模板遵循 AgentOS 的组件规范和通信协议，开箱即用，支持自定义技能注册、记忆管理和协议处理。

## 目录结构

```
python-agent/
├── agent.py                    # Agent 主入口，继承 Agent 基类
├── requirements.txt            # Python 依赖
├── config.yaml                 # Agent 配置（运行时/技能/记忆/日志）
├── skills/                     # 技能目录
│   ├── __init__.py
│   └── custom_skill.py         # 自定义技能示例
├── memory/                     # 记忆模块
│   └── __init__.py
├── protocols/                  # 协议处理
│   └── __init__.py
└── README.md                   # 本文件
```

## 核心组件

### Agent 主类 (`agent.py`)

模板提供 `MyCustomAgent` 类，继承自 `agentos.Agent`：

| 方法 | 说明 |
|------|------|
| `__init__(config)` | 初始化 Agent，配置客户端和技能 |
| `on_start()` | Agent 启动时的初始化逻辑 |
| `on_message(message)` | 处理收到的消息，解析意图并执行 |
| `register_skills()` | 注册 Agent 技能到运行时 |
| `parse_intent(message)` | 解析用户意图 |
| `execute(intent)` | 执行具体任务 |

### 配置文件 (`config.yaml`)

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `agent.name` | Agent 名称 | `my-agent` |
| `agent.version` | Agent 版本 | `1.0.0` |
| `runtime.loop_interval` | 主循环间隔（秒） | 0.1 |
| `runtime.max_idle_time` | 最大空闲时间（秒） | 300 |
| `runtime.auto_reconnect` | 断线自动重连 | `true` |
| `skills.enabled` | 启用的技能列表 | `["custom_skill"]` |
| `skills.auto_load` | 自动加载技能 | `true` |
| `memory.type` | 记忆存储类型 | `local` |
| `memory.max_size` | 最大记忆条目 | 1000 |
| `memory.ttl` | 记忆过期时间（秒） | 3600 |
| `logging.level` | 日志级别 | `INFO` |
| `logging.format` | 日志格式 | `json` |

## 接口说明

### Agent 生命周期

```python
class MyCustomAgent(Agent):
    async def on_start(self)           # 启动初始化
    async def on_message(self, message) # 消息处理
    async def register_skills(self)     # 技能注册
    def parse_intent(self, message)     # 意图解析
    async def execute(self, intent)     # 任务执行
```

### AgentOSClient API

```python
client = AgentOSClient(endpoint="http://localhost:8080")
await client.register_skill(skill)      # 注册技能
await client.send_message(msg)          # 发送消息
await client.get_status()               # 获取状态
```

## 依赖关系

- **核心依赖**: agentos (AgentOS Python SDK), agentos.toolkit.python
- **Python**: >= 3.10

## 构建说明

```bash
# 创建新 Agent 项目
market install python-agent my-first-agent
cd my-first-agent

# 安装依赖
pip install -r requirements.txt

# 启动 Agent
python agent.py
```

## 使用示例

```python
from agentos import Agent
from agentos.toolkit.python.client import AgentOSClient

class MyCustomAgent(Agent):
    def __init__(self, config):
        super().__init__(config)
        self.client = AgentOSClient(
            endpoint=config.get("endpoint", "http://localhost:8080")
        )
        self.name = config.get("name", "MyAgent")
        self.skills = []

    async def on_start(self):
        print(f"Agent {self.name} 已启动")
        await self.register_skills()

    async def on_message(self, message):
        intent = self.parse_intent(message)
        result = await self.execute(intent)
        return result

if __name__ == "__main__":
    agent = MyCustomAgent({
        "name": "MyFirstAgent",
        "endpoint": "http://localhost:8080"
    })
    agent.run()
```

## 发布到市场

```bash
market package my-agent
market publish my-agent.tar.gz
market version my-agent --patch
```

## 最佳实践

1. **模块化设计** — 将功能拆分为独立的技能模块，便于复用和测试
2. **错误处理** — 实现完善的错误处理和重试机制
3. **日志记录** — 使用结构化日志记录 Agent 运行状态
4. **资源管理** — 合理配置内存和会话资源的限制
5. **安全性** — 敏感信息使用环境变量或密钥管理服务

---

© 2026 SPHARX Ltd. All Rights Reserved.
