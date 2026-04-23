# Python Agent 模板

`agentos/openlab/markets/templates/python-agent/`

## 概述

Python Agent 模板是 AgentOS 生态市场中提供的标准化 Agent 开发模板，帮助开发者快速创建基于 Python 的智能 Agent 应用。该模板遵循 AgentOS 的组件规范和通信协议，开箱即用。

## 模板结构

```
python-agent/
├── agent.py                 # Agent 主入口
├── requirements.txt         # Python 依赖
├── config.yaml              # Agent 配置
├── skills/                  # 技能目录
│   ├── __init__.py
│   └── custom_skill.py      # 自定义技能示例
├── memory/                  # 记忆模块
│   └── __init__.py
├── protocols/               # 协议处理
│   └── __init__.py
└── README.md
```

## 快速开始

```bash
# 创建新 Agent 项目
market install python-agent my-first-agent
cd my-first-agent

# 安装依赖
pip install -r requirements.txt

# 启动 Agent
python agent.py
```

## Agent 开发示例

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
        """Agent 启动时的初始化逻辑"""
        print(f"Agent {self.name} 已启动")
        await self.register_skills()

    async def on_message(self, message):
        """处理收到的消息"""
        intent = self.parse_intent(message)
        result = await self.execute(intent)
        return result

    async def register_skills(self):
        """注册 Agent 技能"""
        for skill in self.skills:
            await self.client.register_skill(skill)

    def parse_intent(self, message):
        """解析用户意图"""
        pass

    async def execute(self, intent):
        """执行任务"""
        pass

# 启动入口
if __name__ == "__main__":
    agent = MyCustomAgent({
        "name": "MyFirstAgent",
        "endpoint": "http://localhost:8080"
    })
    agent.run()
```

## 配置说明

```yaml
# config.yaml
agent:
  name: "my-agent"
  version: "1.0.0"
  description: "My custom Agent"

runtime:
  loop_interval: 0.1       # 主循环间隔(秒)
  max_idle_time: 300        # 最大空闲时间(秒)
  auto_reconnect: true      # 断线自动重连

skills:
  enabled: ["custom_skill"]
  auto_load: true

memory:
  type: "local"             # 记忆存储类型
  max_size: 1000            # 最大记忆条目
  ttl: 3600                 # 记忆过期时间(秒)

logging:
  level: "INFO"
  format: "json"
```

## 发布到市场

```bash
# 打包 Agent
market package my-agent

# 发布到市场
market publish my-agent.tar.gz

# 版本管理
market version my-agent --patch  # 更新补丁版本
market version my-agent --minor  # 更新次要版本
market version my-agent --major  # 更新主要版本
```

## 最佳实践

1. **模块化设计** - 将功能拆分为独立的技能模块，便于复用和测试
2. **错误处理** - 实现完善的错误处理和重试机制
3. **日志记录** - 使用结构化日志记录 Agent 运行状态
4. **资源管理** - 合理配置内存和会话资源的限制
5. **安全性** - 敏感信息使用环境变量或密钥管理服务

---

© 2026 SPHARX Ltd. All Rights Reserved.
