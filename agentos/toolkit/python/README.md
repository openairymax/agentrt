# Python SDK

`toolkit/python/` 是 AgentOS 的 Python 语言 SDK，提供完整的 AgentOS 客户端功能。

## 版本

当前版本: **v1.0.0.9**

## 安装

```bash
cd toolkit/python
pip install -e .
```

## 结构

```
python/
├── agentos/                  # SDK 主包
│   ├── client/              # HTTP/WebSocket 客户端
│   ├── framework/           # Agent 框架支持
│   ├── modules/             # 模块管理器
│   │   ├── task/           # 任务管理
│   │   ├── session/        # 会话管理
│   │   ├── memory/         # 内存管理
│   │   └── skill/          # 技能管理
│   ├── types/              # 类型定义
│   ├── utils/              # 工具函数
│   ├── agent.py            # Agent 核心
│   ├── syscall.py          # 系统调用接口
│   └── protocol.py         # 协议实现
├── examples/                # 示例代码
├── tests/                   # 测试用例
├── setup.py                 # 打包配置
└── README.md                # 本文件
```

## 使用示例

```python
from agentos import AgentOSClient

# 客户端初始化
client = AgentOSClient(base_url="http://localhost:8080")

# 获取 Agent
agent = client.get_agent("agent-001")

# 提交任务
task = client.create_task(
    agent_id="agent-001",
    name="example_task",
    input_data={"prompt": "Hello"}
)

# 查询任务状态
status = client.get_task_status(task.id)
print(f"Task status: {status}")

# 管理内存
memory = client.memory_manager
memory.store("key", {"data": "value"})
result = memory.retrieve("key")
```

## 运行测试

```bash
cd toolkit/python
pytest tests/ -v
```

---

*AgentOS Toolkit — Python SDK*
