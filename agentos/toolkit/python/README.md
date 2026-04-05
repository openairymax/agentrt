# AgentOS Python SDK

## 概述

AgentOS Python SDK 提供了一个简洁、易用的接口，用于与 AgentOS 系统进行交互。该 SDK 支持任务管理、记忆操作、会话管理和技能加载等功能。

## 安装

### 从 PyPI 安装

```bash
pip install agentos
```

### 从源码安装

```bash
cd agentos/toolkit/python
pip install -e .
```

## 快速开始

### 基本用法

```python
from agentos import AgentOS, Task, Memory

# 初始化客户端
client = AgentOS(endpoint="http://localhost:18789")

# 提交任务
task = client.submit_task("分析这份销售数据")
print(f"Task ID: {task.task_id}")

# 等待完成
result = task.wait(timeout=30)
print(f"Result: {result.output}")

# 写入记忆
memory_id = client.write_memory("用户偏好使用 Python")
print(f"Memory ID: {memory_id}")

# 搜索记忆
memories = client.search_memory("Python 编程", top_k=5)
for mem in memories:
    print(f"- {mem.content}")
```

### 异步支持

```python
import asyncio
from agentos import AsyncAgentOS

async def main():
    client = AsyncAgentOS()
    
    # 并发提交多个任务
    tasks = await asyncio.gather(
        client.submit_task("任务 1"),
        client.submit_task("任务 2"),
        client.submit_task("任务 3")
    )
    
    # 等待所有任务完成
    results = await asyncio.gather(*[t.wait() for t in tasks])

asyncio.run(main())
```

## API 文档

### AgentOS 类

#### 初始化

```python
def __init__(self, endpoint: str = "http://localhost:18789", timeout: int = 30):
    """
    初始化 AgentOS 客户端。
    
    参数:
        endpoint: AgentOS 服务器端点。
        timeout: 请求超时时间（秒）。
    """
```

#### submit_task

```python
def submit_task(self, task_description: str) -> Task:
    """
    提交任务到 AgentOS 系统。
    
    参数:
        task_description: 任务描述。
    
    返回:
        Task 对象，表示提交的任务。
    """
```

#### write_memory

```python
def write_memory(self, content: str, metadata: Optional[Dict[str, Any]] = None) -> str:
    """
    向 AgentOS 系统写入记忆。
    
    参数:
        content: 记忆内容。
        metadata: 可选的元数据。
    
    返回:
        记忆 ID。
    """
```

#### search_memory

```python
def search_memory(self, query: str, top_k: int = 5) -> List[Memory]:
    """
    在 AgentOS 系统中搜索记忆。
    
    参数:
        query: 搜索查询。
        top_k: 返回结果的最大数量。
    
    返回:
        Memory 对象列表。
    """
```

#### get_memory

```python
def get_memory(self, memory_id: str) -> Memory:
    """
    根据 ID 获取记忆。
    
    参数:
        memory_id: 记忆 ID。
    
    返回:
        Memory 对象。
    """
```

#### delete_memory

```python
def delete_memory(self, memory_id: str) -> bool:
    """
    根据 ID 删除记忆。
    
    参数:
        memory_id: 记忆 ID。
    
    返回:
        如果记忆删除成功，则为 True。
    """
```

#### create_session

```python
def create_session(self) -> Session:
    """
    创建新会话。
    
    返回:
        Session 对象。
    """
```

#### load_skill

```python
def load_skill(self, skill_name: str) -> Skill:
    """
    根据名称加载技能。
    
    参数:
        skill_name: 技能名称。
    
    返回:
        Skill 对象。
    """
```

### Task 类

#### query

```python
def query(self) -> TaskStatus:
    """
    查询任务状态。
    
    返回:
        当前任务状态。
    """
```

#### wait

```python
def wait(self, timeout: Optional[int] = None) -> TaskResult:
    """
    等待任务完成。
    
    参数:
        timeout: 最大等待时间（秒）。
    
    返回:
        任务结果。
    """
```

#### cancel

```python
def cancel(self) -> bool:
    """
    取消任务。
    
    返回:
        如果任务取消成功，则为 True。
    """
```

### Session 类

#### set_context

```python
def set_context(self, key: str, value: Any) -> bool:
    """
    为会话设置上下文值。
    
    参数:
        key: 上下文键。
        value: 上下文值。
    
    返回:
        如果上下文设置成功，则为 True。
    """
```

#### get_context

```python
def get_context(self, key: str) -> Optional[Any]:
    """
    从会话获取上下文值。
    
    参数:
        key: 上下文键。
    
    返回:
        上下文值，如果键不存在则为 None。
    """
```

#### close

```python
def close(self) -> bool:
    """
    关闭会话。
    
    返回:
        如果会话关闭成功，则为 True。
    """
```

### Skill 类

#### execute

```python
def execute(self, parameters: Optional[Dict[str, Any]] = None) -> SkillResult:
    """
    执行技能。
    
    参数:
        parameters: 技能参数。
    
    返回:
        技能执行结果。
    """
```

#### get_info

```python
def get_info(self) -> SkillInfo:
    """
    获取技能信息。
    
    返回:
        技能信息。
    """
```

## 错误处理

SDK 提供了以下异常类：

- `AgentOSError`: 基础异常类
- `TaskError`: 任务相关错误
- `MemoryError`: 记忆相关错误
- `SessionError`: 会话相关错误
- `SkillError`: 技能相关错误
- `NetworkError`: 网络相关错误
- `TimeoutError`: 超时错误

## 示例

### 完整示例

```python
from agentos import AgentOS, AsyncAgentOS
from agentos.exceptions import AgentOSError

# 同步示例
try:
    client = AgentOS()
    
    # 提交任务
    task = client.submit_task("分析销售数据")
    print(f"Task ID: {task.task_id}")
    
    # 等待任务完成
    result = task.wait(timeout=60)
    if result.status == "completed":
        print(f"Task completed: {result.output}")
    else:
        print(f"Task failed: {result.error}")
        
    # 写入记忆
    memory_id = client.write_memory("用户喜欢使用 Python 编程")
    print(f"Memory ID: {memory_id}")
    
    # 搜索记忆
    memories = client.search_memory("Python")
    print(f"Found {len(memories)} memories:")
    for mem in memories:
        print(f"- {mem.content}")
        
    # 创建会话
    session = client.create_session()
    print(f"Session ID: {session.session_id}")
    
    # 设置会话上下文
    session.set_context("user_id", "12345")
    
    # 获取会话上下文
    user_id = session.get_context("user_id")
    print(f"User ID: {user_id}")
    
    # 关闭会话
    session.close()
    
    # 加载技能
    skill = client.load_skill("browser_skill")
    
    # 执行技能
    result = skill.execute({"url": "https://example.com"})
    if result.success:
        print(f"Skill executed successfully: {result.output}")
    else:
        print(f"Skill execution failed: {result.error}")
        
except AgentOSError as e:
    print(f"Error: {e}")

# 异步示例
import asyncio

async def async_example():
    try:
        client = AsyncAgentOS()
        
        # 提交任务
        task = await client.submit_task("分析销售数据")
        print(f"Task ID: {task.task_id}")
        
        # 等待任务完成
        result = await task.wait(timeout=60)
        if result.status == "completed":
            print(f"Task completed: {result.output}")
        else:
            print(f"Task failed: {result.error}")
            
    except AgentOSError as e:
        print(f"Error: {e}")

asyncio.run(async_example())
```

## 版本信息

- **版本**: 1.0.0.5
- **最后更新**: 2026-03-21
- **许可证**: Apache License 2.0

