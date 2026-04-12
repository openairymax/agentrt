# AgentOS Task Framework
# Version: 2.0.0
# Last updated: 2026-04-11

"""
任务管理框架

提供复杂任务的编排、依赖管理、并发控制和容错处理能力。
支持从简单任务到复杂工作流的各种场景。

设计原则:
1. 声明式定义 - 通过配置或DSL定义工作流
2. 可视化追踪 - 完整的任务执行链路和状态
3. 弹性执行 - 自动重试、降级、熔断
4. 可观测性 - 性能指标、资源监控、瓶颈分析
"""

import asyncio
import enum
import logging
import time
import uuid
from dataclasses import dataclass, field
from datetime import datetime
from typing import Any, Callable, Dict, List, Optional, Set, Tuple, Union

logger = logging.getLogger(__name__)


class TaskStatus(enum.Enum):
    """任务状态枚举"""
    PENDING = "pending"
    RUNNING = "running"
    COMPLETED = "completed"
    FAILED = "failed"
    CANCELLED = "cancelled"
    SKIPPED = "skipped"
    TIMEOUT = "timeout"


@dataclass
class TaskNode:
    """工作流中的任务节点"""
    task_id: str
    name: str
    task_type: str = "skill"  # skill, sub_workflow, condition, parallel

    # 执行配置
    config: Dict[str, Any] = field(default_factory=dict)

    # 控制参数
    timeout: Optional[float] = None
    retry_policy: Dict[str, Any] = field(default_factory=dict)
    priority: int = 3  # 1-5, 5最高

    # 条件（可选）
    conditions: List[Dict[str, Any]] = field(default_factory=list)

    # 输入输出映射
    input_mapping: Dict[str, str] = field(default_factory=dict)
    output_mapping: Dict[str, str] = field(default_factory=dict)


@dataclass
class WorkflowDefinition:
    """工作流定义"""
    workflow_id: str
    name: str
    description: str = ""

    # 任务列表
    tasks: List[TaskNode] = field(default_factory=list)

    # 依赖关系 (task_id -> [dependency_task_ids])
    dependencies: Dict[str, List[str]] = field(default_factory=dict)

    # 全局变量
    variables: Dict[str, Any] = field(default_factory=dict)

    # 错误处理策略
    error_handling: Dict[str, Any] = field(default_factory=dict)

    # 并发控制
    max_concurrent_tasks: int = 10


@dataclass
class WorkflowResult:
    """工作流执行结果"""
    success: bool
    workflow_id: str = ""
    total_tasks: int = 0
    completed_tasks: int = 0
    failed_tasks: int = 0
    skipped_tasks: int = 0
    execution_time_ms: float = 0.0
    task_results: Dict[str, 'TaskExecutionResult'] = field(default_factory=dict)
    output: Dict[str, Any] = field(default_factory=dict)
    error: Optional[str] = None


@dataclass
class TaskExecutionResult:
    """单个任务执行结果"""
    task_id: str
    status: TaskStatus = TaskStatus.PENDING
    output: Any = None
    error: Optional[str] = None
    execution_time_ms: float = 0.0
    attempt_number: int = 1


class DependencyResolver:
    """
    任务依赖关系解析器

    构建依赖图，解析执行顺序，检测循环依赖。
    """

    def __init__(self):
        self._graph: Dict[str, Set[str]] = {}
        self._nodes: Set[str] = set()

    def build_graph(
        self,
        tasks: List[TaskNode],
        dependencies: Dict[str, List[str]]
    ) -> bool:
        """
        构建依赖图

        Args:
            tasks: 任务列表
            dependencies: 依赖关系

        Returns:
            是否成功构建
        """
        self._graph.clear()
        self._nodes.clear()

        for task in tasks:
            self._nodes.add(task.task_id)
            deps = set(dependencies.get(task.task_id, []))

            # 验证依赖存在
            for dep in deps:
                if dep not in {t.task_id for t in tasks}:
                    logger.warning(f"Unknown dependency '{dep}' for task '{task.task_id}'")
                    return False

            self._graph[task.task_id] = deps

        return True

    def resolve_execution_order(self) -> List[List[str]]:
        """
        解析执行顺序（拓扑排序）

        Returns:
            按批次返回可并行执行的任务ID列表
        """
        in_degree = {node: 0 for node in self._nodes}

        for node, deps in self._graph.items():
            for dep in deps:
                if dep in in_degree:
                    pass  # dep -> node

        # 计算入度
        for node in self._nodes:
            for dep in self._graph.get(node, set()):
                if dep in in_degree:
                    in_degree[node] = in_degree.get(node, 0) + 1

        batches = []
        remaining = set(self._nodes)

        while remaining:
            # 找到所有入度为0的节点
            batch = [
                node for node in remaining
                if in_degree.get(node, 0) == 0
            ]

            if not batch:
                # 存在循环依赖
                logger.error("Circular dependency detected")
                return []

            batches.append(batch)

            # 移除已处理的节点，更新入度
            for node in batch:
                remaining.remove(node)
                for other_node in remaining:
                    if node in self._graph.get(other_node, set()):
                        in_degree[other_node] -= 1

        return batches

    def detect_cycles(self) -> Optional[List[str]]:
        """
        检测循环依赖

        Returns:
            循环路径（如果存在），否则None
        """
        WHITE, GRAY, BLACK = 0, 1, 2
        color = {node: WHITE for node in self._nodes}
        path = []

        def dfs(node):
            color[node] = GRAY
            path.append(node)

            for neighbor in self._graph.get(node, set()):
                if neighbor not in color:
                    continue
                if color[neighbor] == GRAY:
                    cycle_start = path.index(neighbor)
                    return path[cycle_start:]
                if color[neighbor] == WHITE:
                    result = dfs(neighbor)
                    if result:
                        return result

            path.pop()
            color[node] = BLACK
            return None

        for node in self._nodes:
            if color[node] == WHITE:
                result = dfs(node)
                if result:
                    return result

        return None

    def get_critical_path(
        self,
        estimates: Dict[str, float]
    ) -> List[str]:
        """
        计算关键路径（基于预估时间）

        Args:
            estimates: 任务预估时间 {task_id: seconds}

        Returns:
            关键路径上的任务列表
        """
        if not self._nodes:
            return []

        # 简化的关键路径计算（从后向前）
        earliest_start = {}
        latest_finish = {}

        # 正向计算最早开始时间
        batches = self.resolve_execution_order()
        current_time = 0.0

        for batch in batches:
            max_est = max(estimates.get(t, 0) for t in batch) if batch else 0
            for task in batch:
                earliest_start[task] = current_time
            current_time += max_est

        # 反向计算最晚完成时间
        finish_time = current_time
        for batch in reversed(batches):
            for task in batch:
                latest_finish[task] = finish_time
            min_est = min(estimates.get(t, 0) for t in batch) if batch else 0
            finish_time -= min_est

        # 找出关键路径上的任务（无时间余量的）
        critical_path = []
        for task in self._nodes:
            if abs(earliest_start.get(task, 0) + estimates.get(task, 0) - latest_finish.get(task, 0)) < 0.001:
                critical_path.append(task)

        # 按开始时间排序
        critical_path.sort(key=lambda t: earliest_start.get(t, 0))

        return critical_path


class ConcurrencyController:
    """
    并发控制器

    管理任务的并发执行，支持优先级队列和限流。
    """

    def __init__(self, max_concurrent: int = 10, max_queue_size: int = 1000):
        self._semaphore: asyncio.Semaphore = asyncio.Semaphore(max_concurrent)
        self._max_concurrent = max_concurrent
        self._max_queue_size = max_queue_size
        self._active_count = 0
        self._queue_size = 0
        self._total_submitted = 0
        self._total_completed = 0

        logger.info(f"ConcurrencyController initialized (max={max_concurrent})")

    async def acquire(self) -> None:
        """获取执行许可"""
        await self._semaphore.acquire()
        self._active_count += 1
        self._total_submitted += 1

    def release(self) -> None:
        """释放执行许可"""
        self._semaphore.release()
        self._active_count = max(0, self._active_count - 1)
        self._total_completed += 1

    async def execute_with_control(
        self,
        func: Callable,
        *args,
        **kwargs
    ) -> Any:
        """在并发控制下执行函数"""
        await self.acquire()
        try:
            if asyncio.iscoroutinefunction(func):
                return await func(*args, **kwargs)
            else:
                return await asyncio.to_thread(func, *args, **kwargs)
        finally:
            self.release()

    def get_stats(self) -> Dict[str, Any]:
        """获取统计信息"""
        return {
            "active": self._active_count,
            "max_concurrent": self._max_concurrent,
            "utilization": f"{(self._active_count / self._max_concurrent * 100):.1f}%",
            "total_submitted": self._total_submitted,
            "total_completed": self._total_completed
        }


class FaultToleranceMechanism:
    """
    容错机制

    提供重试、熔断、降级等容错能力。
    """

    class RetryPolicy:
        def __init__(
            self,
            max_attempts: int = 3,
            initial_delay: float = 1.0,
            max_delay: float = 30.0,
            backoff_factor: float = 2.0,
            jitter: bool = True
        ):
            self.max_attempts = max_attempts
            self.initial_delay = initial_delay
            self.max_delay = max_delay
            self.backoff_factor = backoff_factor
            self.jitter = jitter

    class CircuitBreaker:
        def __init__(
            self,
            failure_threshold: int = 5,
            success_threshold: int = 3,
            timeout: float = 60.0
        ):
            self.failure_threshold = failure_threshold
            self.success_threshold = success_threshold
            self.timeout = timeout
            self.failure_count = 0
            self.success_count = 0
            self.state = "closed"
            self.last_failure_time: Optional[float] = None

        def record_success(self) -> None:
            self.success_count += 1
            if self.state == "half_open":
                if self.success_count >= self.success_threshold:
                    self.state = "closed"
                    self.failure_count = 0
                    self.success_count = 0

        def record_failure(self) -> None:
            self.failure_count += 1
            self.success_count = 0
            self.last_failure_time = time.time()
            if self.failure_count >= self.failure_threshold:
                self.state = "open"

        def allow_request(self) -> bool:
            if self.state == "closed":
                return True
            if self.state == "open":
                if self.last_failure_time and (
                    time.time() - self.last_failure_time >= self.timeout
                ):
                    self.state = "half_open"
                    self.success_count = 0
                    return True
                return False
            return True

    def __init__(self):
        self._default_retry_policy = self.RetryPolicy()
        self._circuit_breakers: Dict[str, 'FaultToleranceMechanism.CircuitBreaker'] = {}

    def get_circuit_breaker(self, name: str) -> 'FaultToleranceMechanism.CircuitBreaker':
        if name not in self._circuit_breakers:
            self._circuit_breakers[name] = self.CircuitBreaker()
        return self._circuit_breakers[name]

    async def execute_with_retry(
        self,
        func: Callable,
        policy: Optional[RetryPolicy] = None,
        circuit_breaker_name: Optional[str] = None,
        *args,
        **kwargs
    ) -> Any:
        """带重试机制和熔断保护的执行"""
        policy = policy or self._default_retry_policy
        last_error = None

        cb = None
        if circuit_breaker_name:
            cb = self.get_circuit_breaker(circuit_breaker_name)
            if not cb.allow_request():
                raise RuntimeError(
                    f"Circuit breaker '{circuit_breaker_name}' is open, "
                    f"requests are blocked"
                )

        for attempt in range(policy.max_attempts):
            try:
                if asyncio.iscoroutinefunction(func):
                    result = await func(*args, **kwargs)
                else:
                    result = await asyncio.to_thread(func, *args, **kwargs)

                if cb:
                    cb.record_success()

                return result

            except Exception as e:
                last_error = e

                if cb:
                    cb.record_failure()

                if attempt < policy.max_attempts - 1:
                    delay = min(
                        policy.initial_delay * (policy.backoff_factor ** attempt),
                        policy.max_delay
                    )

                    if policy.jitter:
                        import random
                        delay *= (0.5 + random.random())

                    logger.debug(f"Retry attempt {attempt + 1}/{policy.max_attempts} after {delay:.2f}s")
                    await asyncio.sleep(delay)

        raise last_error

    async def execute_with_fallback(
        self,
        primary_func: Callable,
        fallback_func: Callable,
        circuit_breaker_name: Optional[str] = None,
        *args,
        **kwargs
    ) -> Any:
        """带降级和熔断保护的执行"""
        try:
            return await self.execute_with_retry(
                primary_func,
                circuit_breaker_name=circuit_breaker_name,
                *args,
                **kwargs
            )
        except Exception as e:
            logger.warning(f"Primary function failed, using fallback: {e}")
            if asyncio.iscoroutinefunction(fallback_func):
                return await fallback_func(*args, **kwargs)
            else:
                return await asyncio.to_thread(fallback_func, *args, **kwargs)


class TaskOrchestrationEngine:
    """
    任务编排引擎

    工作流的创建、执行和管理中心。
    支持复杂的工作流定义和灵活的执行控制。
    """

    def __init__(self, skill_engine=None):
        self._skill_engine = skill_engine
        self._workflows: Dict[str, WorkflowDefinition] = {}
        self._workflow_handles: Dict[str, 'WorkflowHandle'] = {}
        self._dependency_resolver = DependencyResolver()
        self._concurrency_controller = ConcurrencyController()
        self._fault_tolerance = FaultToleranceMechanism()

        logger.info("TaskOrchestrationEngine initialized")

    async def create_workflow(self, definition: WorkflowDefinition) -> str:
        """
        创建工作流实例

        Args:
            definition: 工作流定义

        Returns:
            工作流实例ID
        """
        handle_id = str(uuid.uuid4())

        # 验证并构建依赖图
        valid = self._dependency_resolver.build_graph(
            definition.tasks,
            definition.dependencies
        )

        if not valid:
            raise ValueError("Invalid workflow: dependency graph construction failed")

        # 检测循环依赖
        cycle = self._dependency_resolver.detect_cycles()
        if cycle:
            raise ValueError(f"Circular dependency detected: {' -> '.join(cycle)}")

        # 创建句柄
        handle = WorkflowHandle(
            handle_id=handle_id,
            definition=definition,
            status="created"
        )

        self._workflow_handles[handle_id] = handle
        self._workflows[definition.workflow_id] = definition

        logger.info(f"Created workflow instance: {handle_id[:8]}...")
        return handle_id

    async def execute_workflow(
        self,
        workflow_id_or_handle: str,
        variables: Optional[Dict[str, Any]] = None
    ) -> WorkflowResult:
        """
        执行工作流

        Args:
            workflow_id_or_handle: 工作流ID或句柄ID
            variables: 变量覆盖

        Returns:
            执行结果
        """
        start_time = time.time()

        # 获取工作流句柄
        handle = self._workflow_handles.get(workflow_id_or_handle)
        if not handle:
            # 尝试通过workflow_id查找
            for h in self._workflow_handles.values():
                if h.definition.workflow_id == workflow_id_or_handle:
                    handle = h
                    break

        if not handle:
            raise ValueError(f"Workflow not found: {workflow_id_or_handle}")

        definition = handle.definition
        merged_variables = {**definition.variables, **(variables or {})}

        result = WorkflowResult(
            success=True,
            workflow_id=definition.workflow_id,
            total_tasks=len(definition.tasks)
        )

        try:
            # 解析执行顺序
            execution_batches = self._dependency_resolver.resolve_execution_order()

            if not execution_batches:
                raise RuntimeError("Failed to resolve execution order")

            # 按批次执行
            for batch_idx, batch in enumerate(execution_batches):
                logger.info(f"Executing batch {batch_idx + 1}/{len(execution_batches)}: {batch}")

                # 并行执行当前批次的任务
                batch_results = await self._execute_batch(
                    batch,
                    definition,
                    merged_variables,
                    result
                )

                # 更新结果
                for task_result in batch_results:
                    result.task_results[task_result.task_id] = task_result

                    if task_result.status == TaskStatus.COMPLETED:
                        result.completed_tasks += 1
                    elif task_result.status == TaskStatus.FAILED:
                        result.failed_tasks += 1
                        if definition.error_handling.get("on_failure") == "fail_fast":
                            result.success = False
                            break
                    elif task_result.status == TaskStatus.SKIPPED:
                        result.skipped_tasks += 1

            # 收集最终输出
            result.output = merged_variables
            result.execution_time_ms = (time.time() - start_time) * 1000

            if result.failed_tasks > 0 and definition.error_handling.get("on_failure") != "continue":
                result.success = False

        except Exception as e:
            result.success = False
            result.error = str(e)
            result.execution_time_ms = (time.time() - start_time) * 1000
            logger.error(f"Workflow execution failed: {e}")

        return result

    async def _execute_batch(
        self,
        task_ids: List[str],
        definition: WorkflowDefinition,
        variables: Dict[str, Any],
        workflow_result: WorkflowResult
    ) -> List[TaskExecutionResult]:
        """并行执行一批任务"""
        results = []

        async def execute_single_task(task_id: str) -> TaskExecutionResult:
            start_time = time.time()
            result = TaskExecutionResult(task_id=task_id)

            # 查找任务定义
            task_def = next((t for t in definition.tasks if t.task_id == task_id), None)
            if not task_def:
                result.status = TaskStatus.FAILED
                result.error = f"Task definition not found: {task_id}"
                return result

            # 检查条件
            should_execute = True
            for condition in task_def.conditions:
                if not self._evaluate_condition(condition, variables):
                    should_execute = False
                    break

            if not should_execute:
                result.status = TaskStatus.SKIPPED
                return result

            try:
                # 使用容错机制执行
                output = await self._fault_tolerance.execute_with_retry(
                    lambda: self._execute_task_internal(task_def, variables),
                    retry_policy=self._fault_tolerance.RetryPolicy(**task_def.retry_policy) if task_def.retry_policy else None
                )

                result.status = TaskStatus.COMPLETED
                result.output = output

                # 更新变量
                if output and isinstance(output, dict):
                    for out_key, var_name in task_def.output_mapping.items():
                        if out_key in output:
                            variables[var_name] = output[out_key]

            except Exception as e:
                result.status = TaskStatus.FAILED
                result.error = str(e)
                logger.error(f"Task {task_id} failed: {e}")

            finally:
                result.execution_time_ms = (time.time() - start_time) * 1000

            return result

        # 并行执行所有任务
        coroutines = [execute_single_task(tid) for tid in task_ids]
        results = await asyncio.gather(*coroutines, return_exceptions=True)

        # 处理异常结果
        final_results = []
        for r in results:
            if isinstance(r, Exception):
                final_results.append(TaskExecutionResult(
                    task_id="unknown",
                    status=TaskStatus.FAILED,
                    error=str(r)
                ))
            else:
                final_results.append(r)

        return final_results

    async def _execute_task_internal(
        self,
        task_def: TaskNode,
        variables: Dict[str, Any]
    ) -> Any:
        """内部任务执行逻辑"""
        if task_def.task_type == "skill" and self._skill_engine:
            # 从技能引擎执行
            skill_name = task_def.config.get("skill_name", "")
            params = {
                k: variables.get(v, v)
                for k, v in task_def.input_mapping.items()
            }
            params.update(task_def.config.get("parameters", {}))

            result = await self._skill_engine.execute(skill_name, params)
            return result.output if result.success else None

        elif task_def.task_type == "sub_workflow":
            # 嵌套工作流执行
            sub_workflow_id = task_def.config.get("workflow_id")
            if sub_workflow_id:
                sub_result = await self.execute_workflow(sub_workflow_id, variables)
                return sub_result.output

        else:
            # 默认：简单返回配置
            return task_def.config

    def _evaluate_condition(self, condition: Dict, variables: Dict) -> bool:
        """评估条件表达式"""
        cond_type = condition.get("type", "equals")
        left = variables.get(condition.get("left", ""))
        right = condition.get("right")

        if cond_type == "equals":
            return left == right
        elif cond_type == "not_equals":
            return left != right
        elif cond_type == "exists":
            return left is not None
        elif cond_type == "greater_than":
            return left > right if isinstance(left, (int, float)) and isinstance(right, (int, float)) else False
        elif cond_type == "less_than":
            return left < right if isinstance(left, (int, float)) and isinstance(right, (int, float)) else False

        return True

    def list_workflows(self) -> List[Dict[str, Any]]:
        """列出所有工作流"""
        return [
            {
                "id": wf.workflow_id,
                "name": wf.name,
                "tasks_count": len(wf.tasks),
                "status": self._workflow_handles.get(hid, WorkflowHandle()).status
            }
            for wf, hid in zip(self._workflows.values(), self._workflow_handles.keys())
        ]


class WorkflowHandle:
    """工作流运行时句柄"""
    def __init__(self, handle_id: str, definition: WorkflowDefinition, status: str = "created"):
        self.handle_id = handle_id
        self.definition = definition
        self.status = status
        self.created_at = datetime.now()


__all__ = [
    "TaskOrchestrationEngine",
    "DependencyResolver",
    "ConcurrencyController",
    "FaultToleranceMechanism",
    "WorkflowDefinition",
    "WorkflowResult",
    "TaskNode",
    "TaskExecutionResult",
    "TaskStatus",
]
