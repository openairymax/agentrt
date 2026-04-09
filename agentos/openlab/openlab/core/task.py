"""
OpenLab Core Task Module

浠诲姟璋冨害涓庣姸鎬佹満鏍稿績妯″潡
閬靛惊 AgentOS 鏋舵瀯璁捐鍘熷垯 V1.8
"""

from dataclasses import dataclass, field
from enum import Enum
from typing import Any, Dict, List, Optional, Callable, Awaitable
import asyncio
import time
import uuid


class TaskStatus(Enum):
    """浠诲姟鐘舵€佹灇涓?""
    PENDING = "pending"
    QUEUED = "queued"
    RUNNING = "running"
    PAUSED = "paused"
    COMPLETED = "completed"
    FAILED = "failed"
    CANCELLED = "cancelled"
    TIMEOUT = "timeout"


class TaskCategory(Enum):
    """浠诲姟绫诲埆鏋氫妇"""
    IMMEDIATE = "immediate"      # 鍗虫椂浠诲姟
    SCHEDULED = "scheduled"      # 璁″垝浠诲姟
    PERIODIC = "periodic"        # 鍛ㄦ湡浠诲姟
    LONG_RUNNING = "long_running"  # 闀挎椂浠诲姟


@dataclass
class TaskDefinition:
    """
    浠诲姟瀹氫箟
    
    閬靛惊鍘熷垯:
    - A-1 绠€绾﹁嚦涓婏細鏈€灏忔帴鍙ｆ渶澶т环鍊?    - E-5 鍛藉悕璇箟鍖栵細鍚嶇О鍗虫枃妗?    """
    task_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    name: str = ""
    description: str = ""
    category: TaskCategory = TaskCategory.IMMEDIATE
    priority: int = 0  # 鏁板€艰秺澶т紭鍏堢骇瓒婇珮
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
    浠诲姟鐘舵€?    
    鍖呭惈浠诲姟鐨勫畬鏁寸姸鎬佷俊鎭紝鏀寔妫€鏌ョ偣淇濆瓨/鎭㈠
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
        """搴忓垪鍖栦负瀛楀吀"""
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
        """浠庡瓧鍏稿弽搴忓垪鍖?""
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
    鎵ц璁″垝
    
    鍖呭惈浠诲姟鐨勮缁嗘墽琛屾楠ゅ拰璧勬簮闇€姹?    """
    plan_id: str
    task_id: str
    steps: List[Dict[str, Any]] = field(default_factory=list)
    resources: Dict[str, Any] = field(default_factory=dict)
    estimated_duration: Optional[float] = None
    actual_duration: Optional[float] = None
    created_at: float = field(default_factory=time.time)
    
    def add_step(self, step: Dict[str, Any]) -> None:
        """娣诲姞鎵ц姝ラ"""
        self.steps.append(step)
    
    def get_next_step(self, current_step_index: int) -> Optional[Dict[str, Any]]:
        """鑾峰彇涓嬩竴姝?""
        if current_step_index < len(self.steps):
            return self.steps[current_step_index]
        return None


class TaskScheduler:
    """
    浠诲姟璋冨害鍣?    
    閬靛惊鍘熷垯:
    - K-4 鍙彃鎷旂瓥鐣ワ細鏀寔涓嶅悓鐨勮皟搴︾瓥鐣?    - S-1 鍙嶉闂幆锛氬畬鏁寸殑鎵ц鍙嶉
    - E-3 璧勬簮纭畾鎬э細鏄庣‘鐨勮祫婧愮鐞?    """
    
    def __init__(self, max_concurrent: int = 100, max_queue_size: int = 10000):
        """
        鍒濆鍖栦换鍔¤皟搴﹀櫒
        
        Args:
            max_concurrent: 鏈€澶у苟鍙戜换鍔℃暟
            max_queue_size: 鏈€澶ч槦鍒楀ぇ灏?        """
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
        鎻愪氦浠诲姟
        
        Args:
            definition: 浠诲姟瀹氫箟
            
        Returns:
            str: 浠诲姟 ID
        """
        if self._shutdown:
            raise RuntimeError("Scheduler is shutting down")
        
        if self._queue.qsize() >= self.max_queue_size:
            raise RuntimeError("Task queue is full")
        
        # 鍒濆鍖栦换鍔＄姸鎬?        state = TaskState(task_id=definition.task_id)
        async with self._lock:
            self._task_states[definition.task_id] = state
        
        # 鍔犲叆闃熷垪
        priority = -definition.priority  # 浼樺厛绾ч珮鐨勫厛鎵ц
        await self._queue.put((priority, definition))
        
        return definition.task_id
    
    async def schedule(
        self,
        definition: TaskDefinition,
        executor: Callable[[TaskDefinition], Awaitable[Any]]
    ) -> asyncio.Task:
        """
        璋冨害浠诲姟鎵ц
        
        Args:
            definition: 浠诲姟瀹氫箟
            executor: 鎵ц鍑芥暟
            
        Returns:
            asyncio.Task: 寮傛浠诲姟
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
                    # 鏇存柊鐘舵€?                    state.status = TaskStatus.RUNNING
                    state.started_at = time.time()
                    
                    # 鎵ц浠诲姟
                    result = await asyncio.wait_for(
                        executor(definition),
                        timeout=definition.timeout
                    )
                    
                    # 瀹屾垚
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
        
        # 鍒涘缓寮傛浠诲姟
        async_task = asyncio.create_task(run_task())
        
        async with self._lock:
            self._running_tasks[definition.task_id] = async_task
        
        return async_task
    
    async def get_state(self, task_id: str) -> Optional[TaskState]:
        """
        鑾峰彇浠诲姟鐘舵€?        
        Args:
            task_id: 浠诲姟 ID
            
        Returns:
            Optional[TaskState]: 浠诲姟鐘舵€?        """
        async with self._lock:
            return self._task_states.get(task_id)
    
    async def cancel(self, task_id: str) -> bool:
        """
        鍙栨秷浠诲姟
        
        Args:
            task_id: 浠诲姟 ID
            
        Returns:
            bool: 鏄惁鎴愬姛鍙栨秷
        """
        async with self._lock:
            task = self._running_tasks.get(task_id)
            if task and not task.done():
                task.cancel()
                return True
            return False
    
    async def save_checkpoint(self, task_id: str, checkpoint_data: Dict[str, Any]) -> bool:
        """
        淇濆瓨浠诲姟妫€鏌ョ偣
        
        Args:
            task_id: 浠诲姟 ID
            checkpoint_data: 妫€鏌ョ偣鏁版嵁
            
        Returns:
            bool: 鏄惁鎴愬姛淇濆瓨
        """
        async with self._lock:
            state = self._task_states.get(task_id)
            if state:
                state.checkpoint_data = checkpoint_data
                return True
            return False
    
    async def load_checkpoint(self, task_id: str) -> Optional[Dict[str, Any]]:
        """
        鍔犺浇浠诲姟妫€鏌ョ偣
        
        Args:
            task_id: 浠诲姟 ID
            
        Returns:
            Optional[Dict[str, Any]]: 妫€鏌ョ偣鏁版嵁
        """
        async with self._lock:
            state = self._task_states.get(task_id)
            if state:
                return state.checkpoint_data
            return None
    
    async def shutdown(self, wait: bool = True, timeout: Optional[float] = None) -> None:
        """
        鍏抽棴璋冨害鍣?        
        Args:
            wait: 鏄惁绛夊緟杩愯涓殑浠诲姟瀹屾垚
            timeout: 绛夊緟瓒呮椂鏃堕棿
        """
        self._shutdown = True
        
        # 鍙栨秷鎵€鏈夎繍琛屼腑鐨勪换鍔?        async with self._lock:
            for task_id, task in self._running_tasks.items():
                if not task.done():
                    task.cancel()
            
            if wait:
                # 绛夊緟浠诲姟瀹屾垚
                if self._running_tasks:
                    await asyncio.wait(
                        self._running_tasks.values(),
                        timeout=timeout
                    )
    
    def get_stats(self) -> Dict[str, Any]:
        """
        鑾峰彇璋冨害鍣ㄧ粺璁′俊鎭?        
        Returns:
            Dict[str, Any]: 缁熻淇℃伅
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
