Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
"From data intelligence emerges."

# AgentOS Python 编码规范

**版本**: Doc V1.8  
**最后更新**: 2026-04-09  
**作者**: LirenWang  
**适用范围**: AgentOS 所有 Python 代码  
**理论基础**: 工程两论（反馈闭环）、系统工程（模块化）、五维正交系统（系统观、内核观、认知观、工程观、设计美学）、双系统认知理论  
**原则映射**: S-1至S-4（系统设计）、C-1至C-4（认知设计）、E-1至E-8（工程设计）、A-1至A-4（设计美学）

---

## 一、概述

### 1.1 编制目的

本规范为 AgentOS 项目中的 Python 代码提供统一的编码标准。基于项目架构设计原则的五维正交系统，本规范聚焦于工程观维度（E-1至E-4），为开发者提供可操作的代码实现指南。

### 1.2 理论基础

本规范基于 AgentOS 架构设计原则的五维正交系统：

- **《工程控制论》**（原则 S-1, E-2）：通过错误处理、日志、健康检查构建反馈闭环
- **《论系统工程》**（原则 S-2）：模块化、接口驱动、边界清晰
- **双系统认知理论**（原则 C-1）：Python 简洁语法（System 1）与类型提示（System 2）

**双系统在 Python 中的体现**:

| 场景 | System 1（快速） | System 2（严谨） |
|------|-----------------|-----------------|
| 类型系统 | 动态类型 | 类型提示 (typing) |
| 错误处理 | 运行时异常 | 静态类型检查 (mypy) |
| 开发体验 | 快速原型 | 完整类型定义 |
| 性能优化 | 解释执行 | Cython/Numba 编译 |

### 1.3 适用范围

| 组件 | Python 版本 | 类型检查 |
|------|-------------|----------|
| agentos/daemon/ | Python 3.11+ | mypy |
| SDK Python | Python 3.10+ | pyright |
| 工具脚本 | Python 3.9+ | - |

### 1.4 与 AgentOS 架构的关系

Python 代码在 AgentOS 中主要应用于以下场景：

| 场景 | 位置 | 关联原则 | Python 特性 |
|------|------|---------|------------|
| 守护进程 | `agentos/daemon/` | S-3, K-3 | asyncio, multiprocessing |
| 工具脚本 | `agentos/toolkit/` | E-7 | 快速原型，脚本自动化 |
| SDK Python | `sdks/python/` | K-2, E-2 | 类型提示，数据类 |
| 机器学习 | `ml/` | C-1, C-4 | NumPy, PyTorch |

**层次纪律**（原则 S-2）:
- Python 守护进程必须通过 `syscalls.h` 的 C API 绑定与内核交互
- 禁止 Python 代码直接访问内核内部结构
- 所有跨语言边界必须进行参数验证和类型转换

---

## 二、文件组织

### 2.1 文件命名

- **Python 文件**：`.py` 扩展名，采用 `snake_case`
- **包目录**：包含 `__init__.py`
- **测试文件**：`test_*.py` 或 `*_test.py`

```
src/
├── agents/
│   ├── __init__.py
│   ├── task_scheduler.py
│   ├── memory_manager.py
│   └── cognition_engine.py
├── types/
│   ├── __init__.py
│   ├── agent_types.py
│   └── task_types.py
└── utils/
    ├── __init__.py
    ├── error_handler.py
    └── logger.py
```

### 2.2 模块结构

```python
"""
AgentOS 任务调度模块。

提供任务调度核心功能，包括任务提交、状态管理、
优先级队列和依赖解析。调度器采用双系统架构：
- System 1：快速路径，处理简单任务
- System 2：深度路径，处理复杂任务

Example:
    >>> from agentos.scheduler import TaskScheduler
    >>> scheduler = TaskScheduler(max_workers=4)
    >>> task_id = scheduler.submit(plan)
    >>> result = scheduler.wait(task_id)

Author:
    AgentOS Team

Version:
    1.5.0
"""

# 1. 标准库导入
from __future__ import annotations
import logging
from typing import Optional, List, Dict, Any
from dataclasses import dataclass, field
from enum import Enum, auto

# 2. 第三方库导入
import pydantic
from pydantic import BaseModel, Field

# 3. 项目内部导入
from agentos.types import TaskPlan, TaskResult
from agentos.errors import SchedulerError
from agentos.constants import DEFAULT_TIMEOUT

# 4. 公开导出
__all__ = ["TaskScheduler", "SchedulerConfig"]

# 5. 模块级变量和常量
logger = logging.getLogger(__name__)
DEFAULT_MAX_WORKERS = 4
```

---

## 三、命名规范

### 3.1 命名风格

| 类型 | 风格 | 示例 |
|------|------|------|
| 模块/包名 | snake_case | `task_scheduler.py`, `agentos` |
| 类名 | PascalCase | `class TaskScheduler`, `@dataclass TaskConfig` |
| 函数/方法 | snake_case | `submit_task()`, `get_task_status()` |
| 变量 | snake_case | `task_id`, `max_workers` |
| 常量 | UPPER_SNAKE_CASE | `MAX_RETRY_COUNT`, `DEFAULT_TIMEOUT` |
| 类型别名 | PascalCase | `TaskId = str`, `MetricsDict = Dict[str, float]` |
| 私有属性 | _leading_underscore | `_internal_state` |

### 3.2 命名示例

```python
# 正确的命名
MAX_TASK_NAME_LENGTH = 256
task_registry: Dict[str, Task] = {}
task_id = "task-001"

class TaskScheduler:
    """任务调度器。"""
    
    def __init__(self, max_workers: int = 4) -> None:
        self.max_workers = max_workers
        self._internal_state: Optional[State] = None

# 枚举
class TaskStatus(Enum):
    IDLE = auto()
    PENDING = auto()
    RUNNING = auto()
    COMPLETED = auto()
    FAILED = auto()

# 类型别名
TaskId = str
Timestamp = float
ByteBuffer = bytes

# 错误的命名
TaskRegistry = {}  # 类名应该 PascalCase
maxTaskNameLength = 256  # 常量应该 UPPER_SNAKE_CASE
def SubmitTask():  # 函数名应该 snake_case
    pass
```

---

## 四、类型设计

### 4.1 类型别名

```python
from typing import TypeAlias, Callable, Awaitable

# 基本类型别名
TaskId: TypeAlias = str
Timestamp: TypeAlias = float
ByteBuffer: TypeAlias = bytes

# 函数类型
TaskHandler: TypeAlias = Callable[[Any], Awaitable[Any]]
ErrorHandler: TypeAlias = Callable[[Exception], None]

# 复杂类型
MetricsDict: TypeAlias = Dict[str, float]
ConfigDict: TypeAlias = Dict[str, Any]
ResultTuple: TypeAlias = tuple[bool, Optional[str]]
```

### 4.2 数据类

```python
from dataclasses import dataclass, field
from typing import Optional, List
from enum import Enum, auto

class TaskStrategy(Enum):
    """任务执行策略。"""
    SEQUENTIAL = auto()
    PARALLEL = auto()
    ADAPTIVE = auto()

@dataclass
class TaskConfig:
    """任务配置。
    
    Attributes:
        name: 任务名称
        timeout: 超时时间（秒）
        max_retries: 最大重试次数
        strategy: 执行策略
        tags: 任务标签
    """
    name: str
    timeout: float = 30.0
    max_retries: int = 3
    strategy: TaskStrategy = TaskStrategy.SEQUENTIAL
    tags: List[str] = field(default_factory=list)
    
    def __post_init__(self) -> None:
        """验证配置参数。"""
        if self.timeout <= 0:
            raise ValueError(f"timeout must be positive, got {self.timeout}")
        if self.max_retries < 0:
            raise ValueError(f"max_retries must be non-negative, got {self.max_retries}")

@dataclass
class TaskResult:
    """任务结果。
    
    Attributes:
        task_id: 任务 ID
        success: 是否成功
        output: 输出数据
        error: 错误信息
        duration: 执行时长（秒）
    """
    task_id: str
    success: bool
    output: Optional[bytes] = None
    error: Optional[str] = None
    duration: Optional[float] = None
```

### 4.3 Pydantic 模型

```python
from pydantic import BaseModel, Field, field_validator
from typing import Optional, List

class TaskPlanSchema(BaseModel):
    """任务计划 schema。
    
    Attributes:
        id: 任务唯一标识符
        name: 任务名称
        steps: 执行步骤列表
        strategy: 执行策略
    """
    id: str = Field(..., min_length=1, max_length=64, description="任务 ID")
    name: str = Field(..., min_length=1, max_length=256, description="任务名称")
    steps: List[StepSchema] = Field(..., min_length=1, description="执行步骤")
    strategy: str = Field(default="sequential", pattern="^(sequential|parallel|adaptive)$")
    
    @field_validator("id")
    @classmethod
    def validate_id(cls, v: str) -> str:
        """验证任务 ID 格式。"""
        if not v.replace("-", "").replace("_", "").isalnum():
            raise ValueError(f"Invalid task ID format: {v}")
        return v
    
    model_config = {
        "json_schema_extra": {
            "example": {
                "id": "task-001",
                "name": "data-processing",
                "steps": [],
                "strategy": "parallel"
            }
        }
    }
```

---

## 五、函数设计

### 5.1 函数签名规范

```python
from typing import Optional, List, Dict, Any
import logging

logger = logging.getLogger(__name__)

def submit_task(
    plan: TaskPlan,
    priority: int = 5,
    timeout: Optional[float] = None,
    tags: Optional[Dict[str, str]] = None,
) -> str:
    """提交任务计划。
    
    将任务计划加入调度队列，返回任务 ID 用于后续查询。
    调度器会根据优先级和依赖关系决定执行顺序。
    
    Args:
        plan: 任务计划对象
        priority: 任务优先级，1-10，默认 5
        timeout: 超时时间（秒），None 表示使用默认值
        tags: 任务标签字典，用于分类和过滤
        
    Returns:
        任务 ID 字符串
        
    Raises:
        ValueError: 当 priority 不在 1-10 范围内
        TypeError: 当 plan 不是 TaskPlan 类型
        SchedulerError: 当调度器已关闭或队列已满
        
    Example:
        >>> plan = TaskPlan(id="task-001", name="test")
        >>> task_id = submit_task(plan, priority=8)
        >>> print(task_id)
        'task-001'
        
    Note:
        - 提交后任务进入 PENDING 状态
        - 高优先级任务可能抢占低优先级任务
    """
    # 参数验证
    if not isinstance(priority, int) or priority < 1 or priority > 10:
        raise ValueError(f"priority must be in [1, 10], got {priority}")
    
    # 实现...
```

### 5.2 异步函数

```python
import asyncio
from typing import Optional
from dataclasses import dataclass

async def wait_for_task(
    task_id: str,
    timeout: Optional[float] = None,
) -> TaskResult:
    """等待任务完成。
    
    异步等待任务执行完成或超时。
    
    Args:
        task_id: 任务 ID
        timeout: 超时时间（秒），None 表示无限等待
        
    Returns:
        任务结果
        
    Raises:
        asyncio.TimeoutError: 当等待超时
        TaskNotFoundError: 当任务不存在
    """
    if timeout is not None and timeout <= 0:
        raise ValueError(f"timeout must be positive, got {timeout}")
    
    start_time = asyncio.get_event_loop().time()
    
    while True:
        result = await _get_task_result(task_id)
        
        if result is not None:
            return result
        
        if timeout is not None:
            elapsed = asyncio.get_event_loop().time() - start_time
            if elapsed >= timeout:
                raise asyncio.TimeoutError(f"Task {task_id} timed out after {timeout}s")
        
        await asyncio.sleep(0.1)  # 轮询间隔
```

### 5.3 返回类型约定

```python
from typing import TypeAlias, Generic, TypeVar, Union, Optional
from dataclasses import dataclass

# Result 类型
T = TypeVar("T")
E = TypeVar("E")

@dataclass
class Result(Generic[T, E]):
    """结果类型，支持成功/失败两种状态。"""
    success: bool
    value: Optional[T] = None
    error: Optional[E] = None
    
    @classmethod
    def ok(cls, value: T) -> "Result[T, None]":
        """创建成功结果。"""
        return cls(success=True, value=value)
    
    @classmethod
    def err(cls, error: E) -> "Result[None, E]":
        """创建错误结果。"""
        return cls(success=False, error=error)

# 使用示例
def process_task(task_id: str) -> Result[TaskResult, SchedulerError]:
    try:
        result = scheduler.execute(task_id)
        return Result.ok(result)
    except SchedulerError as e:
        return Result.err(e)
```

---

## 六、类设计

### 6.1 类结构模板

```python
import logging
from typing import Optional, List, Dict, Any
from dataclasses import dataclass, field
from abc import ABC, abstractmethod

logger = logging.getLogger(__name__)

class SchedulerBase(ABC):
    """调度器基类。
    
    定义调度器的抽象接口。子类必须实现所有抽象方法。
    
    Attributes:
        name: 调度器名称
        max_workers: 最大工作线程数
    """
    
    def __init__(self, name: str, max_workers: int) -> None:
        """初始化调度器。
        
        Args:
            name: 调度器名称
            max_workers: 最大工作线程数，必须为正整数
            
        Raises:
            ValueError: 当 max_workers <= 0 时
        """
        if max_workers <= 0:
            raise ValueError(f"max_workers must be positive, got {max_workers}")
        
        self.name = name
        self.max_workers = max_workers
        self._tasks: Dict[str, Any] = {}
        logger.info(f"Scheduler {name} initialized with {max_workers} workers")
    
    @abstractmethod
    async def submit(self, plan: TaskPlan) -> str:
        """提交任务计划。"""
        pass
    
    @abstractmethod
    async def wait(self, task_id: str, timeout: Optional[float] = None) -> TaskResult:
        """等待任务完成。"""
        pass
    
    @abstractmethod
    async def cancel(self, task_id: str) -> bool:
        """取消任务。"""
        pass

class TaskScheduler(SchedulerBase):
    """任务调度器。
    
    负责管理任务的生命周期和执行。调度器采用双系统架构，
    System 1 处理简单任务，System 2 处理复杂任务。
    
    Example:
        >>> scheduler = TaskScheduler(name="main", max_workers=4)
        >>> task_id = await scheduler.submit(plan)
        >>> result = await scheduler.wait(task_id)
    """
    
    def __init__(
        self,
        name: str = "default",
        max_workers: int = 4,
        enable_retry: bool = True,
    ) -> None:
        """初始化任务调度器。
        
        Args:
            name: 调度器名称
            max_workers: 最大工作线程数
            enable_retry: 是否启用自动重试
        """
        super().__init__(name, max_workers)
        self.enable_retry = enable_retry
        self._retry_policy = RetryPolicy()
    
    async def submit(self, plan: TaskPlan) -> str:
        """提交任务计划。"""
        task_id = plan.id
        self._tasks[task_id] = TaskContext(plan=plan, status=TaskStatus.PENDING)
        logger.debug(f"Task {task_id} submitted")
        return task_id
    
    async def wait(self, task_id: str, timeout: Optional[float] = None) -> TaskResult:
        """等待任务完成。"""
        # 实现...
        pass
    
    async def cancel(self, task_id: str) -> bool:
        """取消任务。"""
        if task_id not in self._tasks:
            return False
        self._tasks[task_id].status = TaskStatus.CANCELLED
        return True
```

### 6.2 上下文管理器

```python
from contextlib import contextmanager
from typing import Generator
import logging

logger = logging.getLogger(__name__)

class ResourceManager:
    """资源管理器。
    
    提供资源获取和释放的上下文管理器支持。
    """
    
    def __init__(self, manager: ResourceConfig) -> None:
        self.manager = manager
        self._resource = None
    
    def __enter__(self) -> "ResourceManager":
        """获取资源。"""
        self._resource = self._acquire()
        logger.debug(f"Resource acquired: {self._resource}")
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        """释放资源。"""
        if self._resource is not None:
            self._release(self._resource)
            logger.debug(f"Resource released")
    
    @contextmanager
    def transaction(self) -> Generator[Transaction, None, None]:
        """事务上下文管理器。"""
        tx = Transaction()
        try:
            yield tx
            tx.commit()
        except Exception as e:
            tx.rollback()
            raise
```

---

## 七、错误处理

### 7.1 异常类定义

```python
class AgentOSError(Exception):
    """AgentOS 错误基类。"""
    
    def __init__(self, message: str, code: str = "AGENTOS_ERROR") -> None:
        self.message = message
        self.code = code
        super().__init__(self.message)

class SchedulerError(AgentOSError):
    """调度器错误。"""
    pass

class SchedulerClosedError(SchedulerError):
    """调度器已关闭错误。"""
    
    def __init__(self) -> None:
        super().__init__("Scheduler is closed", "SCHEDULER_CLOSED")

class TaskNotFoundError(SchedulerError):
    """任务不存在错误。"""
    
    def __init__(self, task_id: str) -> None:
        super().__init__(f"Task not found: {task_id}", "TASK_NOT_FOUND")
        self.task_id = task_id

class ValidationError(AgentOSError):
    """验证错误。"""
    pass

class ResourceError(AgentOSError):
    """资源错误。"""
    pass
```

### 7.2 错误处理模式

```python
from typing import Optional, Callable
import logging
import functools

logger = logging.getLogger(__name__)

def handle_errors(
    error_type: type[Exception] = Exception,
    default_return: Optional[Any] = None,
    log_traceback: bool = True,
) -> Callable:
    """错误处理装饰器。"""
    def decorator(func: Callable) -> Callable:
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            try:
                return func(*args, **kwargs)
            except error_type as e:
                if log_traceback:
                    logger.exception(f"Error in {func.__name__}: {e}")
                else:
                    logger.error(f"Error in {func.__name__}: {e}")
                return default_return
        return wrapper
    return decorator

async def safe_execute(
    func: Callable,
    *args,
    **kwargs,
) -> Result:
    """安全执行异步函数。"""
    try:
        result = await func(*args, **kwargs)
        return Result.ok(result)
    except Exception as e:
        logger.exception(f"Execution failed: {e}")
        return Result.err(e)
```

---

## 八、异步编程

### 8.1 异步上下文管理器

```python
import asyncio
from contextlib import asynccontextmanager
from typing import AsyncGenerator

class AsyncResourcePool:
    """异步资源池。"""
    
    def __init__(self, max_size: int = 10) -> None:
        self.max_size = max_size
        self._available: asyncio.Queue = asyncio.Queue(max_size)
        self._used: set = set()
    
    @asynccontextmanager
    async def acquire(self) -> AsyncGenerator:
        """获取资源。"""
        resource = await self._available.get()
        self._used.add(resource)
        try:
            yield resource
        finally:
            self._used.remove(resource)
            await self._available.put(resource)
    
    async def __aenter__(self) -> "AsyncResourcePool":
        """进入异步上下文。"""
        return self
    
    async def __aexit__(self, exc_type, exc_val, exc_tb) -> None:
        """退出异步上下文。"""
        while not self._available.empty():
            self._available.get_nowait()
```

### 8.2 并发控制

```python
import asyncio
from typing import List, TypeVar

T = TypeVar("T")

async def execute_with_concurrency(
    tasks: List[asyncio.Task[T]],
    max_concurrency: int,
) -> List[T]:
    """限制并发数执行任务。
    
    Args:
        tasks: 任务列表
        max_concurrency: 最大并发数
        
    Returns:
        所有任务的结果列表
    """
    semaphore = asyncio.Semaphore(max_concurrency)
    
    async def bounded_task(task: asyncio.Task[T]) -> T:
        async with semaphore:
            return await task
    
    bounded_tasks = [bounded_task(t) for t in tasks]
    return await asyncio.gather(*bounded_tasks)

async def execute_parallel(
    functions: List[Callable],
    concurrency: int = 10,
) -> List:
    """并行执行函数，限制并发数。"""
    semaphore = asyncio.Semaphore(concurrency)
    
    async def run_with_limit(func: Callable) -> Any:
        async with semaphore:
            return await func()
    
    return await asyncio.gather(*[run_with_limit(f) for f in functions])
```

---

## 九、模块与导入

### 9.1 导入语句顺序

```python
# 1. __future__ 导入（必须是第一行）
from __future__ import annotations

# 2. 标准库
import logging
from typing import Optional, List, Dict, Any, Callable
from dataclasses import dataclass, field
from enum import Enum, auto

# 3. 第三方库
import pydantic
from pydantic import BaseModel, Field
import numpy as np

# 4. 项目内部
from agentos.types import TaskPlan, TaskResult
from agentos.errors import SchedulerError
from agentos.constants import DEFAULT_TIMEOUT

# 5. 类型导入（使用 type: ignore 类型注释）
from agentos.manager import manager  # type: ignore[attr-defined]
```

### 9.2 导出模式

```python
# __init__.py
"""AgentOS 调度模块。

Example:
    >>> from agentos.scheduler import TaskScheduler
    >>> scheduler = TaskScheduler()
"""

# 显式导出
__all__ = [
    "TaskScheduler",
    "SchedulerConfig",
    "SchedulerError",
    "SchedulerClosedError",
    "TaskNotFoundError",
]

# 导入并重新导出
from agentos.scheduler.scheduler import TaskScheduler
from agentos.scheduler.manager import SchedulerConfig
from agentos.scheduler.errors import (
    SchedulerError,
    SchedulerClosedError,
    TaskNotFoundError,
)
```

---

## 十、测试集成

### 10.1 pytest 示例

```python
"""
AgentOS 任务调度器测试。
"""

import pytest
import asyncio
from typing import Optional
from agentos.scheduler import TaskScheduler, SchedulerConfig, TaskStatus

@pytest.fixture
def scheduler() -> TaskScheduler:
    """创建测试用调度器。"""
    return TaskScheduler(name="test", max_workers=2)

@pytest.fixture
def sample_plan() -> TaskPlan:
    """创建示例任务计划。"""
    return TaskPlan(
        id="task-001",
        name="test-task",
        steps=[],
        strategy="sequential"
    )

class TestTaskScheduler:
    """TaskScheduler 测试类。"""
    
    def test_submit_returns_task_id(self, scheduler: TaskScheduler, sample_plan: TaskPlan) -> None:
        """测试提交任务返回任务 ID。"""
        task_id = asyncio.run(scheduler.submit(sample_plan))
        assert task_id == "task-001"
    
    def test_submit_with_priority(self, scheduler: TaskScheduler, sample_plan: TaskPlan) -> None:
        """测试带优先级的任务提交。"""
        task_id = asyncio.run(scheduler.submit(sample_plan, priority=8))
        assert task_id == "task-001"
    
    def test_invalid_priority_raises_error(self, scheduler: TaskScheduler, sample_plan: TaskPlan) -> None:
        """测试无效优先级抛出错误。"""
        with pytest.raises(ValueError, match="priority must be in"):
            asyncio.run(scheduler.submit(sample_plan, priority=15))
    
    @pytest.mark.asyncio
    async def test_wait_returns_result(self, scheduler: TaskScheduler, sample_plan: TaskPlan) -> None:
        """测试等待返回结果。"""
        task_id = await scheduler.submit(sample_plan)
        result = await scheduler.wait(task_id, timeout=5.0)
        assert result.success is True

@pytest.mark.parametrize("strategy", [
    "sequential",
    "parallel",
    "adaptive"
])
def test_scheduler_with_strategy(strategy: str, scheduler: TaskScheduler) -> None:
    """参数化测试：不同策略。"""
    assert scheduler.strategy == strategy
```

---

## 十一、AgentOS 模块 Python 编码示例

### 11.1 daemon（守护层）Python 实现
Backs模块作为系统服务守护进程，需要高可靠性和可观测性：

#### 11.1.1 IPC通信守护进程（映射原则：E-3 通信基础设施）
```python
"""
IPC通信守护进程 - 体现系统观（S-3）和工程观（E-4）原则

实现与Atoms模块的高性能进程间通信。
集成OpenTelemetry可观测性和消息加密。
"""
import asyncio
from typing import Optional
from dataclasses import dataclass
from contextlib import asynccontextmanager

@dataclass
class SecureIpcMessage:
    """安全IPC消息 - 体现防御深度（D-3）原则"""
    message_id: str
    sender: str
    operation: str
    payload: bytes
    signature: bytes
    timestamp: float
    
    def validate(self) -> bool:
        """消息验证 - 多层安全校验"""
        # 层次1：消息完整性
        if not self.verify_signature():
            return False
        
        # 层次2：时间戳有效性（防重放）
        if not self.check_timestamp():
            return False
        
        # 层次3：操作权限验证
        if not self.validate_operation():
            return False
        
        return True
    
    def verify_signature(self) -> bool:
        """验证消息签名"""
        # 实际签名验证逻辑
        return True

class IpcDaemon:
    """IPC守护进程 - 体现工程观（E-2）和设计美学（A-1）原则"""
    
    def __init__(self, manager: dict):
        self.manager = manager
        self.logger = self.setup_logger()
        self.metrics = self.setup_metrics()
        self.crypto = self.setup_crypto()
        
    async def process_message(self, message: SecureIpcMessage) -> dict:
        """处理安全消息 - 结构化错误处理和审计"""
        self.logger.info(
            "IPC message received",
            extra={
                "message_id": message.message_id,
                "sender": message.sender,
                "operation": message.operation,
                "size": len(message.payload),
                "log_type": "ipc_request"
            }
        )
        
        try:
            # 消息验证
            if not message.validate():
                self.logger.warning(
                    "Invalid IPC message",
                    extra={"message_id": message.message_id, "log_type": "security_audit"}
                )
                raise SecurityError("Message validation failed")
            
            # 处理消息
            result = await self._do_process(message)
            
            # 成功日志
            self.logger.debug(
                "IPC message processed",
                extra={
                    "message_id": message.message_id,
                    "duration_ms": result.get("duration_ms", 0),
                    "log_type": "ipc_response"
                }
            )
            
            return result
            
        except SecurityError as e:
            # 安全异常详细记录
            self.logger.error(
                "IPC security violation",
                extra={
                    "message_id": message.message_id,
                    "error": str(e),
                    "log_type": "security_audit"
                },
                exc_info=True
            )
            raise
            
        except Exception as e:
            # 一般异常处理
            self.logger.error(
                "IPC processing failed",
                extra={
                    "message_id": message.message_id,
                    "error": str(e),
                    "log_type": "error"
                },
                exc_info=True
            )
            raise
    
    @asynccontextmanager
    async def secure_session(self, client_id: str):
        """安全会话上下文管理器 - 体现资源确定性原则"""
        session_key = await self.crypto.establish_session(client_id)
        try:
            yield session_key
        finally:
            await self.crypto.close_session(client_id)
            self.logger.debug(f"Session closed for client: {client_id}")
```

### 11.2 commons（公共库层）Python 实现
Common模块提供跨层基础设施，强调通用性和性能：

#### 11.2.1 向量数据库客户端（映射原则：E-1 基础设施）
```python
"""
向量数据库客户端 - 体现工程观（E-1, E-3）和认知观（C-3）原则

封装FAISS和HNSW向量索引，支持持久化存储和拓扑分析。
集成HDBSCAN聚类算法，自动发现数据中的拓扑结构。
"""
import numpy as np
from typing import List, Optional, Tuple
from dataclasses import dataclass
from contextlib import contextmanager
from functools import lru_cache

@dataclass
class VectorSearchResult:
    """向量搜索结果 - 类型安全的数据结构"""
    id: str
    score: float
    metadata: dict
    vector: np.ndarray
    
    def __post_init__(self):
        """数据验证 - 体现防御性编程"""
        if not 0 <= self.score <= 1:
            raise ValueError(f"Invalid score: {self.score}")
        if len(self.vector.shape) != 1:
            raise ValueError(f"Vector must be 1D, got shape: {self.vector.shape}")

class VectorDBClient:
    """向量数据库客户端 - 高性能和类型安全设计"""
    
    def __init__(self, index_path: str, dimension: int):
        self.index_path = index_path
        self.dimension = dimension
        self.index = self._load_index()
        self.metadata_store = MetadataStore()
        self.cache = LRUCache(maxsize=1000)
        
    @lru_cache(maxsize=100)
    def search(
        self,
        query: np.ndarray,
        k: int = 10,
        **kwargs
    ) -> List[VectorSearchResult]:
        """
        向量搜索 - 性能优化和缓存友好
        
        参数:
            query: 查询向量，形状为(dimension,)
            k: 返回结果数量
            **kwargs: 搜索选项
            
        返回:
            搜索结果列表，按相似度排序
        """
        # 参数验证
        self._validate_query(query)
        if k <= 0:
            raise ValueError(f"k must be positive, got: {k}")
        
        # 缓存查询
        cache_key = self._hash_vector(query)
        if cached := self.cache.get(cache_key):
            return cached
        
        # 执行搜索
        start_time = time.perf_counter()
        indices, scores = self.index.search(query.reshape(1, -1), k)
        search_time = time.perf_counter() - start_time
        
        # 批量获取元数据
        metadata_list = self.metadata_store.batch_get(indices[0])
        
        # 构造结果
        results = [
            VectorSearchResult(
                id=str(idx),
                score=float(score),
                metadata=metadata,
                vector=self.index.reconstruct(idx)
            )
            for idx, score, metadata in zip(indices[0], scores[0], metadata_list)
        ]
        
        # 更新缓存
        self.cache[cache_key] = results
        
        # 记录性能指标
        self.metrics.record_search(search_time, len(results))
        
        return results
    
    def _validate_query(self, query: np.ndarray) -> None:
        """查询向量验证 - 防御性编程"""
        if query.shape != (self.dimension,):
            raise ValueError(
                f"Query shape mismatch: expected ({self.dimension},), "
                f"got {query.shape}"
            )
        if not np.isfinite(query).all():
            raise ValueError("Query vector contains NaN or infinite values")
```

---

## 十二、参考文献

1. **AgentOS 架构设计原则**: [architectural_design_principles.md](../../architecture/folder/architectural_design_principles.md)
2. **PEP 8 Style Guide**: https://pep8.org/
3. **Google Python Style Guide**: https://google.github.io/styleguide/pyguide.html
4. **Python typing documentation**: https://docs.python.org/3/library/typing.html
5. **AgentOS 核心架构文档**:
   - [coreloopthree.md](../../architecture/folder/coreloopthree.md)
   - [memoryrovol.md](../../architecture/folder/memoryrovol.md)
   - [microkernel.md](../../architecture/folder/microkernel.md)
   - [ipc.md](../../architecture/folder/ipc.md)
   - [logging_system.md](../../architecture/folder/logging_system.md)

---

© 2026 SPHARX Ltd. All Rights Reserved.  
"From data intelligence emerges."