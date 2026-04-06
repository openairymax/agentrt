Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# AgentOS Python SDK

**版本**: 3.0.0
**最后更新**: 2026-04-06
**状态**: 生产就绪
**Python 版本**: 3.10+
**核心文档**: [Python SDK 规范](../../agentos/manuals/api/toolkit/python/README.md)
**源码位置**: [agentos/toolkit/python](../../agentos/toolkit/python/)

---

## 📦 安装

### 从 PyPI 安装

```bash
pip install agentos-sdk
```

### 从源码安装

```bash
cd AgentOS/agentos/toolkit/python
pip install -e .
```

### 验证安装

```python
from agentos.client import Client
print("AgentOS Python SDK 3.0.0 installed successfully")
```

---

## 🚀 快速开始

### 1. 初始化客户端（基于实际源码 agentos/client/client.py）

```python
from agentos.client import Client, manager

# 使用配置对象初始化（推荐）
config = manager(
    endpoint="http://localhost:18789",  # 默认端口（与源码一致）
    timeout=30.0,
    max_retries=3,
    retry_delay=1.0,
    max_connections=100,
    api_key="your-api-key"
)

client = Client(manager=config)

# 或使用上下文管理器（自动关闭连接）
with Client(endpoint="http://localhost:18789", api_key="your-api-key") as client:
    health = client.health()
    print(f"System status: {health.status}, Version: {health.version}")
```

### 2. 创建第一个 Agent

```python
from agentos import Agent, AgentConfig

# 配置 Agent
config = AgentConfig(
    name="my-assistant",
    description="A helpful AI assistant",
    model="gpt-4-turbo",
    max_tokens=4096,
    temperature=0.7,
    memory_enabled=True,
    tools=["web-search", "code-executor"]
)

# 创建并启动 Agent
agent = client.create_agent(config)
agent.start()
print(f"Agent started with ID: {agent.id}")
```

### 3. 与 Agent 对话

```python
# 同步对话
response = agent.chat("Hello, how are you?")
print(response.content)

# 流式对话（推荐用于长回复）
for chunk in agent.chat_stream("Tell me a story"):
    print(chunk.content, end="", flush=True)
```

---

## 📚 核心 API 参考

### AgentOSClient - 客户端类

```python
class AgentOSClient:
    """AgentOS 主客户端"""

    def __init__(
        self,
        base_url: str = "http://localhost:8080",
        api_key: Optional[str] = None,
        timeout: float = 30.0,
        max_retries: int = 3,
        session: Optional[requests.Session] = None
    ):
        """
        初始化客户端

        Args:
            base_url: 内核服务基础 URL
            api_key: API 密钥
            timeout: 请求超时时间（秒）
            max_retries: 最大重试次数
            session: 自定义 requests Session
        """

    def create_agent(self, config: AgentConfig) -> Agent:
        """创建新 Agent"""

    def get_agent(self, agent_id: str) -> Agent:
        """获取已存在的 Agent"""

    def list_agents(self, limit: int = 20, offset: int = 0) -> List[Agent]:
        """列出所有 Agent"""

    def health_check(self) -> HealthStatus:
        """健康检查"""

    def close(self):
        """关闭连接"""
```

### Agent - Agent 类

```python
class Agent:
    """Agent 实例"""

    @property
    def id(self) -> str:
        """Agent ID"""

    @property
    def status(self) -> AgentStatus:
        """Agent 当前状态"""

    def start(self) -> None:
        """启动 Agent"""

    def stop(self) -> None:
        """停止 Agent"""

    def chat(
        self,
        message: str,
        system_prompt: Optional[str] = None,
        context: Optional[Dict] = None,
        **kwargs
    ) -> ChatResponse:
        """
        同步对话

        Args:
            message: 用户消息
            system_prompt: 系统提示词
            context: 上下文信息
            **kwargs: 其他参数（temperature, max_tokens 等）

        Returns:
            ChatResponse: 响应对象
        """

    async def chat_async(
        self,
        message: str,
        **kwargs
    ) -> ChatResponse:
        """异步对话（需要 asyncio）"""

    def chat_stream(
        self,
        message: str,
        **kwargs
    ) -> Iterator[ChatChunk]:
        """流式对话（生成器）"""

    def execute_task(
        self,
        task_name: str,
        params: Dict[str, Any],
        timeout: Optional[int] = None
    ) -> TaskResult:
        """执行任务"""

    def memory_search(
        self,
        query: str,
        top_k: int = 10,
        filters: Optional[Dict] = None
    ) -> List[MemoryRecord]:
        """搜索记忆"""

    def get_session(self) -> Session:
        """获取当前会话"""
```

### MemoryClient - 记忆操作

```python
class MemoryClient:
    """记忆系统客户端"""

    def store(
        self,
        data: Any,
        metadata: Optional[Dict] = None,
        tags: Optional[List[str]] = None
    ) -> str:
        """
        存储数据到记忆系统

        Args:
            data: 要存储的数据（支持文本、字典、列表等）
            metadata: 元数据
            tags: 标签列表

        Returns:
            str: 记录 ID
        """

    def search(
        self,
        query: str,
        top_k: int = 10,
        filters: Optional[Dict] = None,
        similarity_threshold: float = 0.7
    ) -> List[MemoryRecord]:
        """
        语义搜索记忆

        Args:
            query: 搜索查询
            top_k: 返回数量
            filters: 过滤条件
            similarity_threshold: 相似度阈值

        Returns:
            List[MemoryRecord]: 匹配的记忆记录
        """

    def get_by_id(self, record_id: str) -> Optional[MemoryRecord]:
        """根据 ID 获取记录"""

    def delete(self, record_id: str) -> bool:
        """删除记录（标记为删除）"""

    def count(self, filters: Optional[Dict] = None) -> int:
        """统计记录数"""
```

### TaskClient - 任务管理

```python
class TaskClient:
    """任务管理客户端"""

    def create(
        self,
        name: str,
        payload: Dict[str, Any],
        priority: int = 50,
        delay_ms: int = 0,
        **kwargs
    ) -> Task:
        """创建任务"""

    def get_status(self, task_id: str) -> TaskStatus:
        """查询任务状态"""

    def cancel(self, task_id: str) -> bool:
        """取消任务"""

    def list_tasks(
        self,
        status: Optional[str] = None,
        limit: int = 20
    ) -> List[Task]:
        """列出任务"""
```

---

## 🔧 高级用法

### 异步编程

```python
import asyncio
from agentos import AsyncAgentOSClient

async def main():
    client = AsyncAgentOSClient(base_url="http://localhost:8080")
    agent = await client.create_agent(config)

    # 并发执行多个请求
    tasks = [
        agent.chat_async(f"Question {i}")
        for i in range(5)
    ]
    responses = await asyncio.gather(*tasks)

    for i, resp in enumerate(responses):
        print(f"Q{i}: {resp.content}")

asyncio.run(main())
```

### 批量操作

```python
# 批量存储记忆
records = [
    {"text": f"Document {i}", "metadata": {"source": f"doc_{i}"}, "tags": ["batch"]}
    for i in range(100)
]

ids = client.memory.batch_store(records)
print(f"Stored {len(ids)} records")

# 批量搜索
queries = ["query 1", "query 2", "query 3"]
results = client.memory.batch_search(queries, top_k=5)
for query, result_list in zip(queries, results):
    print(f"{query}: Found {len(result_list)} results")
```

### 自定义工具

```python
from agentos import Tool, ToolRegistry

@Tool(name="calculator", description="Perform math calculations")
def calculator(expression: str) -> str:
    """安全计算数学表达式"""
    # 使用受限的 eval 环境
    allowed_chars = set('0123456789+-*/().% ')
    if all(c in allowed_chars for c in expression):
        return str(eval(expression))
    raise ValueError("Invalid expression")

# 注册工具
client.tools.register(calculator)

# 在 Agent 中使用自定义工具
agent = client.create_agent(AgentConfig(tools=["calculator"]))
response = agent.chat("What is 123 * 456?")
print(response.content)  # 56088
```

### 错误处理

```python
from agentos import (
    AgentOSError,
    AuthenticationError,
    RateLimitError,
    NotFoundError,
    TimeoutError
)

try:
    response = agent.chat("Hello!")
except AuthenticationError as e:
    print(f"认证失败: {e}")
    # 重新获取 token
except RateLimitError as e:
    print(f"请求过于频繁: {e}")
    # 等待后重试
    time.sleep(e.retry_after)
except NotFoundError as e:
    print(f"资源不存在: {e}")
except TimeoutError as e:
    print(f"请求超时: {e}")
except AgentOSError as e:
    print(f"AgentOS 错误 [{e.code}]: {e.message}")
```

---

## ⚙️ 配置选项

### 环境变量

```bash
# 必需配置
export AGENTOS_BASE_URL=http://localhost:8080
export AGENTOS_API_KEY=your-api-key-here

# 可选配置
export AGENTOS_TIMEOUT=30              # 请求超时（秒）
export AGENTOS_MAX_RETRIES=3           # 最大重试次数
export AGENTOS_LOG_LEVEL=INFO          # 日志级别
export AGENTOS_ENABLE_METRICS=true     # 启用 Prometheus 指标
```

### Python 日志配置

```python
import logging

# 配置 SDK 日志
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)

# 设置 agentos SDK 日志级别
logging.getLogger('agentos').setLevel(logging.DEBUG)
```

---

## 🧪 测试

### 单元测试

```bash
# 运行所有测试
pytest tests/test_python_sdk.py -v

# 运行特定测试模块
pytest tests/test_client.py -v
pytest tests/test_memory.py -v
pytest tests/test_agent.py -v
```

### Mock 测试示例

```python
from unittest.mock import Mock, patch
from agentos import AgentOSClient

def test_chat_with_mock():
    client = AgentOSClient(base_url="http://test")

    # Mock HTTP 响应
    with patch.object(client.session, 'post') as mock_post:
        mock_post.return_value.json.return_value = {
            "jsonrpc": "2.0",
            "result": {
                "choices": [{
                    "message": {"role": "assistant", "content": "Mock response"}
                }]
            },
            "id": "test"
        }

        agent = client.get_agent("test-agent-id")
        response = agent.chat("Hello!")

        assert response.content == "Mock response"
        mock_post.assert_called_once()
```

---

## 📊 性能优化建议

### 1. 连接池复用

```python
import requests

session = requests.Session()
session.mount('http://', requests.adapters.HTTPAdapter(
    pool_connections=10,
    pool_maxsize=100,
    max_retries=3
))

client = AgentOSClient(session=session)
```

### 2. 批量请求合并

```python
# 使用 batch 接口减少网络往返
client.memory.batch_store(large_dataset)
client.memory.batch_search(queries)
```

### 3. 缓存频繁访问的数据

```python
from functools import lru_cache

class CachedAgentOSClient(AgentOSClient):
    @lru_cache(maxsize=1000)
    def get_agent_cached(self, agent_id: str):
        return self.get_agent(agent_id)
```

### 4. 异步 I/O

对于高并发场景，使用 `AsyncAgentOSClient`：

```python
import aiohttp
from agentos import AsyncAgentOSClient

async def handle_many_requests(requests_list):
    client = AsyncAgentOSClient()
    tasks = [client.agent.chat_async(req) for req in requests_list]
    return await asyncio.gather(*tasks)
```

---

## 📚 相关文档

- **[内核 API](kernel-api.md)** — Syscall 接口参考
- **[守护进程 API](daemon-api.md)** — Daemon 服务接口
- **[Go SDK](go-sdk.md)** — Go 语言绑定
- **[错误码手册](error-codes.md)** — 错误码定义与处理
- **[Python SDK 规范](../../agentos/manuals/api/toolkit/python/README.md)** — 完整规范

---

## 🔗 快速链接

- **PyPI**: https://pypi.org/project/agentos-sdk/
- **GitHub Issues**: https://github.com/SpharxTeam/AgentOS/issues
- **文档站点**: https://docs.spharx.cn/agentos/sdk/python

---

**© 2026 SPHARX Ltd. All Rights Reserved.**

*"From data intelligence emerges."*
