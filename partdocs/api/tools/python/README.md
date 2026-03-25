Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# AgentOS Python SDK

**版本**: Doc V1.5  
**最后更新**: 2026-03-23  
**状态**: 🟢 生产就绪  
**Python 版本**: 3.9+

---

## 🎯 概述

AgentOS Python SDK 提供对 AgentOS 系统调用 API 的高级 Python 封装。SDK 遵循 Pythonic 风格，支持异步操作、类型提示和上下文管理器，同时保持与底层 C API 的完整功能对应。

---

## 📦 安装

```bash
# 从 PyPI 安装
pip install agentos

# 从源码安装
git clone https://gitee.com/spharx/agentos.git
cd agentos/bindings/python
pip install -e .
```

---

## 🚀 快速开始

### 创建 Agent 并执行任务

```python
import asyncio
from agentos import Agent, AgentConfig, TaskPriority

async def main():
    # 创建 Agent 配置
    config = AgentConfig(
        name="my_agent",
        description="My first AgentOS agent",
        agent_type="chat",
        max_concurrent_tasks=8,
        task_queue_depth=64
    )
    
    # 使用上下文管理器创建 Agent
    async with Agent(config) as agent:
        # 提交任务
        task_id = await agent.submit_task(
            description="分析用户反馈数据",
            priority=TaskPriority.NORMAL,
            input_params={"data_source": "feedback.csv"},
            timeout_ms=30000
        )
        
        print(f"任务已提交: {task_id}")
        
        # 等待任务完成
        result = await agent.wait_task(task_id, timeout_ms=35000)
        
        print(f"任务结果: {result}")

if __name__ == "__main__":
    asyncio.run(main())
```

### 记忆系统操作

```python
from agentos import MemoryClient, MemoryQuery

async def memory_example():
    client = MemoryClient()
    
    # 写入记忆
    record_id = await client.write(
        content="用户对新产品功能表示满意，特别是实时协作功能",
        metadata={"type": "user_feedback", "rating": 5},
        importance=0.8,
        tags=["feedback", "positive", "collaboration"]
    )
    
    print(f"记忆写入成功: {record_id}")
    
    # 语义检索
    query = MemoryQuery(
        text="用户对协作功能的反馈",
        limit=10,
        similarity_threshold=0.6
    )
    
    results = await client.query(query)
    
    for result in results:
        print(f"[相似度: {result.similarity:.2f}] {result.content}")

asyncio.run(memory_example())
```

---

## 📖 API 参考

### Agent 模块

#### `AgentConfig`

```python
@dataclass
class AgentConfig:
    """Agent 配置类"""
    
    name: str                          # Agent 名称（唯一）
    description: str = ""              # Agent 描述
    agent_type: str = "generic"        # Agent 类型
    max_concurrent_tasks: int = 8      # 最大并发任务数
    task_queue_depth: int = 64         # 任务队列深度
    default_task_timeout_ms: int = 30000  # 默认任务超时
    system1_max_desc_len: int = 128    # System 1 描述长度阈值
    system1_max_priority: int = 50     # System 1 优先级阈值
    cognition_config: dict | None = None  # 认知引擎配置
    memory_config: dict | None = None     # 记忆配置
    security_config: dict | None = None   # 安全策略配置
    resource_limits: ResourceLimits | None = None  # 资源限制
    retry_config: RetryConfig | None = None        # 重试配置
```

#### `Agent`

```python
class Agent:
    """Agent 实例管理类"""
    
    def __init__(self, config: AgentConfig):
        """初始化 Agent"""
        ...
    
    async def __aenter__(self) -> "Agent":
        """异步上下文管理器入口"""
        ...
    
    async def __aexit__(self, exc_type, exc_val, exc_tb):
        """异步上下文管理器出口"""
        ...
    
    @property
    def id(self) -> str:
        """Agent ID"""
        ...
    
    @property
    def state(self) -> AgentState:
        """Agent 状态"""
        ...
    
    async def start(self) -> None:
        """启动 Agent"""
        ...
    
    async def pause(self) -> None:
        """暂停 Agent"""
        ...
    
    async def stop(self, force: bool = False) -> None:
        """停止 Agent"""
        ...
    
    async def bind_skill(self, skill_name: str, config: dict | None = None) -> None:
        """绑定 Skill"""
        ...
    
    async def unbind_skill(self, skill_name: str) -> None:
        """解绑 Skill"""
        ...
    
    async def submit_task(
        self,
        description: str,
        priority: int = TaskPriority.NORMAL,
        input_params: dict | None = None,
        timeout_ms: int = 0
    ) -> str:
        """
        提交任务
        
        Args:
            description: 任务描述
            priority: 任务优先级（0-255，0为最高）
            input_params: 输入参数
            timeout_ms: 超时时间（毫秒）
        
        Returns:
            任务 ID
        """
        ...
    
    async def query_task(self, task_id: str) -> TaskDescriptor:
        """查询任务状态"""
        ...
    
    async def cancel_task(self, task_id: str, force: bool = False) -> None:
        """取消任务"""
        ...
    
    async def wait_task(
        self,
        task_id: str,
        timeout_ms: int = 0
    ) -> dict:
        """等待任务完成"""
        ...
    
    async def list_tasks(
        self,
        filter: dict | None = None,
        offset: int = 0,
        limit: int = 100
    ) -> list[TaskDescriptor]:
        """列出任务"""
        ...
```

### Memory 模块

#### `MemoryClient`

```python
class MemoryClient:
    """记忆系统客户端"""
    
    async def write(
        self,
        content: str,
        metadata: dict | None = None,
        importance: float = 0.5,
        tags: list[str] | None = None
    ) -> str:
        """
        写入记忆记录
        
        Args:
            content: 记忆内容
            metadata: 元数据
            importance: 重要性评分（0.0-1.0）
            tags: 标签列表
        
        Returns:
            记录 ID
        """
        ...
    
    async def query(
        self,
        query: MemoryQuery
    ) -> list[MemoryResult]:
        """语义检索记忆"""
        ...
    
    async def get(self, record_id: str) -> MemoryRecord:
        """获取指定记录"""
        ...
    
    async def forget(
        self,
        policy: ForgetPolicy | None = None
    ) -> int:
        """
        主动遗忘
        
        Returns:
            遗忘的记录数
        """
        ...
    
    async def evolve(self, config: dict | None = None) -> EvolutionStats:
        """触发记忆进化"""
        ...
    
    async def stats(self) -> MemoryStatistics:
        """获取统计信息"""
        ...
```

#### `MemoryQuery`

```python
@dataclass
class MemoryQuery:
    """记忆查询条件"""
    
    text: str | None = None              # 查询文本
    vector: list[float] | None = None    # 查询向量
    tags: list[str] | None = None        # 标签过滤
    time_start: int | None = None        # 时间范围开始
    time_end: int | None = None          # 时间范围结束
    importance_threshold: float = 0.0    # 重要性阈值
    source_agent: str | None = None      # 来源 Agent
    layer: int = 0                       # 层级（0表示所有）
    limit: int = 10                      # 结果数量限制
    similarity_threshold: float = 0.0    # 相似度阈值
    sort_by: str = "similarity"          # 排序方式
```

### Session 模块

#### `SessionClient`

```python
class SessionClient:
    """会话管理客户端"""
    
    async def create(
        self,
        agent_id: str,
        session_type: str = "chat",
        title: str | None = None,
        metadata: dict | None = None
    ) -> str:
        """创建会话"""
        ...
    
    async def get(self, session_id: str) -> SessionDescriptor:
        """获取会话信息"""
        ...
    
    async def close(
        self,
        session_id: str,
        archive: bool = True,
        summary: str | None = None
    ) -> None:
        """关闭会话"""
        ...
    
    async def add_message(
        self,
        session_id: str,
        role: str,
        content: str,
        metadata: dict | None = None
    ) -> str:
        """添加消息"""
        ...
    
    async def list_messages(
        self,
        session_id: str,
        offset: int = 0,
        limit: int = 100
    ) -> list[SessionMessage]:
        """列出消息"""
        ...
    
    async def get_context(self, session_id: str) -> SessionContext:
        """获取会话上下文"""
        ...
    
    async def set_context(
        self,
        session_id: str,
        context: SessionContext
    ) -> None:
        """设置会话上下文"""
        ...
```

### Telemetry 模块

#### `TelemetryClient`

```python
class TelemetryClient:
    """可观测性客户端"""
    
    def log(
        self,
        level: str,
        module: str,
        message: str,
        fields: dict | None = None
    ) -> None:
        """写入日志"""
        ...
    
    def metric(
        self,
        name: str,
        value: float,
        metric_type: str = "gauge",
        labels: dict | None = None
    ) -> None:
        """记录指标"""
        ...
    
    @asynccontextmanager
    async def trace(
        self,
        operation_name: str,
        kind: str = "internal",
        attributes: dict | None = None
    ):
        """追踪上下文管理器"""
        ...
    
    async def stats(self) -> TelemetryStatistics:
        """获取统计信息"""
        ...
```

---

## 🔄 异步与同步 API

SDK 同时提供异步和同步两种 API：

```python
# 异步 API（推荐）
from agentos import Agent, MemoryClient

async def async_example():
    async with Agent(config) as agent:
        task_id = await agent.submit_task("...")

# 同步 API（阻塞调用）
from agentos.sync import Agent, MemoryClient

def sync_example():
    with Agent(config) as agent:
        task_id = agent.submit_task("...")
```

---

## 📝 类型提示

SDK 完全支持类型提示，与 mypy 和 pyright 兼容：

```python
from agentos import (
    Agent,
    AgentConfig,
    AgentState,
    TaskDescriptor,
    TaskPriority,
    MemoryClient,
    MemoryQuery,
    MemoryResult,
    SessionClient,
    TelemetryClient,
)

def process_task(agent: Agent, description: str) -> str:
    task_id: str = agent.submit_task(description)
    return task_id
```

---

## ⚠️ 异常处理

```python
from agentos import AgentOSError, ErrorCode

try:
    async with Agent(config) as agent:
        await agent.submit_task("...")
except AgentOSError as e:
    print(f"错误码: 0x{e.code:04X}")
    print(f"错误信息: {e.message}")
    print(f"trace_id: {e.trace_id}")
    
    if e.code == ErrorCode.INVALID_ARGUMENT:
        print("参数无效")
    elif e.code == ErrorCode.RESOURCE_LIMIT:
        print("资源限制超出")
```

---

## 🧪 测试

```python
import pytest
from agentos import Agent, AgentConfig
from agentos.testing import MockAgent, MockMemoryClient

@pytest.mark.asyncio
async def test_agent_submit_task():
    # 使用 Mock 进行单元测试
    mock_agent = MockAgent()
    
    task_id = await mock_agent.submit_task("test task")
    
    assert task_id is not None
    assert mock_agent.submit_task_called
```

---

## 📚 相关文档

- [系统调用 API](../../syscalls/) - 底层 C API 参考
- [Rust SDK](../rust/README.md) - Rust SDK 文档
- [Go SDK](../go/README.md) - Go SDK 文档
- [开发指南：创建 Agent](../../../guides/folder/create_agent.md) - Agent 开发教程

---

© 2026 SPHARX Ltd. All Rights Reserved.
*"From data intelligence emerges."*