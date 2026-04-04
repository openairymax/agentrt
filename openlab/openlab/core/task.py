"""
OpenLab Core Task Module

任务调度与状态机核心模块
遵循 AgentOS 架构设计原则 V1.8
"""

from dataclasses import dataclass, field
from enum import Enum
from typing import Any, Dict, List, Optional, Callable, Awaitable
import asyncio
import time
import uuid


class TaskStatus(Enum):
    """任务状态枚举"""
    PENDING = "pending"
    QUEUED = "queued"
    RUNNING = "running"
    PAUSED = "paused"
    COMPLETED = "completed"
    FAILED = "failed"
    CANCELLED = "cancelled"
    TIMEOUT = "timeout"


class TaskCategory(Enum):
    """任务类别枚举"""
    IMMEDIATE = "immediate"      # 即时任务
    SCHEDULED = "scheduled"      # 计划任务
    PERIODIC = "periodic"        # 周期任务
    LONG_RUNNING = "long_running"  # 长时任务


@dataclass
class TaskDefinition:
    """
    任务定义
    
    遵循原则:
    - A-1 简约至上：最小接口最大价值
    - E-5 命名语义化：名称即文档
    """
    task_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    name: str = ""
    description: str = ""
    category: TaskCategory = TaskCategory.IMMEDIATE
    priority: int = 0  # 数值越大优先级越高
    input_data: Optional[Dict[str, Any]] = None
    metadata: Dict[str, Any] = field(default_factory=dict)
    timeout: Optional[float] = None
    max_retries: int = 0
    created_at: float = field(default_factory=time.time)
    scheduled_at: Optional[float] = None
    
    def __post_init__(self):
        if not self.task_id:
            self.task_id = str(uuid.uuid4())


@dataclass
class TaskState:
    """
    任务状态
    
    包含任务的完整状态信息，支持检查点保存/恢复
    """
    task_id: str
    status: TaskStatus = TaskStatus.PENDING
    progress: float = 0.0  # 0.0 - 1.0
    result: Optional[Any] = None
    error: Optional[str] = None
    error_code: Optional[str] = None
    retry_count: int = 0
    started_at: Optional[float] = None
    completed_at: Optional[float] = None
    checkpoint_data: Optional[Dict[str, Any]] = None
    metrics: Dict[str, Any] = field(default_factory=dict)
    
    def to_dict(self) -> Dict[str, Any]:
        """序列化为字典"""
        return {
            "task_id": self.task_id,
            "status": self.status.value,
            "progress": self.progress,
            "result": self.result,
            "error": self.error,
            "error_code": self.error_code,
            "retry_count": self.retry_count,
            "started_at": self.started_at,
            "completed_at": self.completed_at,
            "checkpoint_data": self.checkpoint_data,
            "metrics": self.metrics,
        }
    
    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> "TaskState":
        """从字典反序列化"""
        return cls(
            task_id=data["task_id"],
            status=TaskStatus(data["status"]),
            progress=data.get("progress", 0.0),
            result=data.get("result"),
            error=data.get("error"),
            error_code=data.get("error_code"),
            retry_count=data.get("retry_count", 0),
            started_at=data.get("started_at"),
            completed_at=data.get("completed_at"),
            checkpoint_data=data.get("checkpoint_data"),
            metrics=data.get("metrics", {}),
        )


@dataclass
class ExecutionPlan:
    """
    执行计划
    
    包含任务的详细执行步骤和资源需求
    """
    plan_id: str
    task_id: str
    steps: List[Dict[str, Any]] = field(default_factory=list)
    resources: Dict[str, Any] = field(default_factory=dict)
    estimated_duration: Optional[float] = None
    actual_duration: Optional[float] = None
    created_at: float = field(default_factory=time.time)
    
    def add_step(self, step: Dict[str, Any]) -> None:
        """添加执行步骤"""
        self.steps.append(step)
    
    def get_next_step(self, current_step_index: int) -> Optional[Dict[str, Any]]:
        """获取下一步"""
        if current_step_index < len(self.steps):
            return self.steps[current_step_index]
        return None


class TaskScheduler:
    """
    任务调度器
    
    遵循原则:
    - K-4 可插拔策略：支持不同的调度策略
    - S-1 反馈闭环：完整的执行反馈
    - E-3 资源确定性：明确的资源管理
    """
    
    def __init__(self, max_concurrent: int = 100, max_queue_size: int = 10000):
        """
        初始化任务调度器
        
        Args:
            max_concurrent: 最大并发任务数
            max_queue_size: 最大队列大小
        """
        self.max_concurrent = max_concurrent
        self.max_queue_size = max_queue_size
        self._queue: asyncio.PriorityQueue = asyncio.PriorityQueue()
        self._running_tasks: Dict[str, asyncio.Task] = {}
        self._task_states: Dict[str, TaskState] = {}
        self._execution_plans: Dict[str, ExecutionPlan] = {}
        self._lock = asyncio.Lock()
        self._semaphore: asyncio.Semaphore = asyncio.Semaphore(max_concurrent)
        self._shutdown = False
    
    async def submit(self, definition: TaskDefinition) -> str:
        """
        提交任务
        
        Args:
            definition: 任务定义
            
        Returns:
            str: 任务 ID
        """
        if self._shutdown:
            raise RuntimeError("Scheduler is shutting down")
        
        if self._queue.qsize() >= self.max_queue_size:
            raise RuntimeError("Task queue is full")
        
        # 初始化任务状态
        state = TaskState(task_id=definition.task_id)
        async with self._lock:
            self._task_states[definition.task_id] = state
        
        # 加入队列
        priority = -definition.priority  # 优先级高的先执行
        await self._queue.put((priority, definition))
        
        return definition.task_id
    
    async def schedule(
        self,
        definition: TaskDefinition,
        executor: Callable[[TaskDefinition], Awaitable[Any]]
    ) -> asyncio.Task:
        """
        调度任务执行
        
        Args:
            definition: 任务定义
            executor: 执行函数
            
        Returns:
            asyncio.Task: 异步任务
        """
        async def run_task():
            async with self._semaphore:
                if self._shutdown:
                    return
                
                task_id = definition.task_id
                state = self._task_states.get(task_id)
                if not state:
                    return
                
                try:
                    # 更新状态
                    state.status = TaskStatus.RUNNING
                    state.started_at = time.time()
                    
                    # 执行任务
                    result = await asyncio.wait_for(
                        executor(definition),
                        timeout=definition.timeout
                    )
                    
                    # 完成
                    state.status = TaskStatus.COMPLETED
                    state.result = result
                    state.progress = 1.0
                    state.completed_at = time.time()
                    
                except asyncio.TimeoutError:
                    state.status = TaskStatus.TIMEOUT
                    state.error = "Task timeout"
                    
                except asyncio.CancelledError:
                    state.status = TaskStatus.CANCELLED
                    raise
                    
                except Exception as e:
                    state.status = TaskStatus.FAILED
                    state.error = str(e)
                    
                finally:
                    state.completed_at = time.time()
        
        # 创建异步任务
        async_task = asyncio.create_task(run_task())
        
        async with self._lock:
            self._running_tasks[definition.task_id] = async_task
        
        return async_task
    
    async def get_state(self, task_id: str) -> Optional[TaskState]:
        """
        获取任务状态
        
        Args:
            task_id: 任务 ID
            
        Returns:
            Optional[TaskState]: 任务状态
        """
        async with self._lock:
            return self._task_states.get(task_id)
    
    async def cancel(self, task_id: str) -> bool:
        """
        取消任务
        
        Args:
            task_id: 任务 ID
            
        Returns:
            bool: 是否成功取消
        """
        async with self._lock:
            task = self._running_tasks.get(task_id)
            if task and not task.done():
                task.cancel()
                return True
            return False
    
    async def save_checkpoint(self, task_id: str, checkpoint_data: Dict[str, Any]) -> bool:
        """
        保存任务检查点
        
        Args:
            task_id: 任务 ID
            checkpoint_data: 检查点数据
            
        Returns:
            bool: 是否成功保存
        """
        async with self._lock:
            state = self._task_states.get(task_id)
            if state:
                state.checkpoint_data = checkpoint_data
                return True
            return False
    
    async def load_checkpoint(self, task_id: str) -> Optional[Dict[str, Any]]:
        """
        加载任务检查点
        
        Args:
            task_id: 任务 ID
            
        Returns:
            Optional[Dict[str, Any]]: 检查点数据
        """
        async with self._lock:
            state = self._task_states.get(task_id)
            if state:
                return state.checkpoint_data
            return None
    
    async def shutdown(self, wait: bool = True, timeout: Optional[float] = None) -> None:
        """
        关闭调度器
        
        Args:
            wait: 是否等待运行中的任务完成
            timeout: 等待超时时间
        """
        self._shutdown = True
        
        # 取消所有运行中的任务
        async with self._lock:
            for task_id, task in self._running_tasks.items():
                if not task.done():
                    task.cancel()
            
            if wait:
                # 等待任务完成
                if self._running_tasks:
                    await asyncio.wait(
                        self._running_tasks.values(),
                        timeout=timeout
                    )
    
    def get_stats(self) -> Dict[str, Any]:
        """
        获取调度器统计信息
        
        Returns:
            Dict[str, Any]: 统计信息
        """
        return {
            "queue_size": self._queue.qsize(),
            "running_tasks": len(self._running_tasks),
            "total_tasks": len(self._task_states),
            "max_concurrent": self.max_concurrent,
            "max_queue_size": self.max_queue_size,
        }


__all__ = [
    "TaskStatus",
    "TaskCategory",
    "TaskDefinition",
    "TaskState",
    "ExecutionPlan",
    "TaskScheduler",
]
