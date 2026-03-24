# AgentOS Python SDK - Task Manager Implementation
# Version: 3.0.0
# Last updated: 2026-03-24

"""
Task manager implementation for managing task lifecycle.

Provides comprehensive task management including submission, query,
wait, cancel, and batch operations.

Corresponds to Go SDK: modules/task/manager.go
"""

import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from datetime import datetime
from typing import Any, Dict, List, Optional, Tuple

from ...client.client import APIClient, RequestOptions
from ...exceptions import AgentOSError, CODE_MISSING_PARAMETER, CODE_INVALID_RESPONSE, CODE_TASK_TIMEOUT, CODE_TASK_FAILED
from ...types import Task, TaskResult, TaskStatus, ListOptions
from ...utils import (
    get_string, get_int, get_dict, get_list,
    extract_data_map, build_url, parse_time_from_map
)


@dataclass
class MemoryWriteItem:
    """
    批量写入时的单条记忆项。

    用于 BatchWrite 操作。
    """
    content: str
    layer: str
    metadata: Optional[Dict[str, Any]] = None


class TaskManager:
    """
    任务管理器，管理任务完整生命周期。

    提供任务的提交、查询、等待、取消、列表等生命周期管理功能。

    对应 Go SDK: modules/task/manager.go TaskManager

    Example:
        >>> from agentos.client import Client
        >>> from agentos.modules import TaskManager
        >>> client = Client()
        >>> task_mgr = TaskManager(client)
        >>> task = task_mgr.submit("analyze data")
        >>> result = task_mgr.wait(task.id, timeout=30)
    """

    def __init__(self, api: APIClient):
        """
        初始化任务管理器。

        Args:
            api: APIClient 实例，用于 HTTP 通信
        """
        self._api = api

    def submit(self, description: str) -> Task:
        """
        提交新的执行任务。

        Args:
            description: 任务描述

        Returns:
            Task: 创建的任务对象

        Raises:
            AgentOSError: 参数无效或请求失败

        Example:
            >>> task = manager.submit("analyze this dataset")
            >>> print(task.id)
            'task_123'
        """
        if not description:
            raise AgentOSError("任务描述不能为空", error_code=CODE_MISSING_PARAMETER)

        body = {"description": description}
        resp = self._api.post("/api/v1/tasks", body)

        data = extract_data_map(resp)
        if not data:
            raise AgentOSError("任务创建响应格式异常", error_code=CODE_INVALID_RESPONSE)

        return self._parse_task_from_map(data, description)

    def submit_with_options(
        self,
        description: str,
        priority: int = 0,
        metadata: Optional[Dict[str, Any]] = None
    ) -> Task:
        """
        使用扩展选项提交任务。

        Args:
            description: 任务描述
            priority: 任务优先级
            metadata: 任务元数据

        Returns:
            Task: 创建的任务对象

        Raises:
            AgentOSError: 参数无效或请求失败

        Example:
            >>> task = manager.submit_with_options(
            ...     "process data",
            ...     priority=10,
            ...     metadata={"source": "api"}
            ... )
        """
        if not description:
            raise AgentOSError("任务描述不能为空", error_code=CODE_MISSING_PARAMETER)

        body = {"description": description, "priority": priority}
        if metadata:
            body["metadata"] = metadata

        resp = self._api.post("/api/v1/tasks", body)

        data = extract_data_map(resp)
        if not data:
            raise AgentOSError("任务创建响应格式异常", error_code=CODE_INVALID_RESPONSE)

        return self._parse_task_from_map(data, description, priority, metadata)

    def get(self, task_id: str) -> Task:
        """
        获取指定任务的详细信息。

        Args:
            task_id: 任务 ID

        Returns:
            Task: 任务对象

        Raises:
            AgentOSError: 参数无效或请求失败

        Example:
            >>> task = manager.get("task_123")
            >>> print(task.status)
            <TaskStatus.RUNNING: 'running'>
        """
        if not task_id:
            raise AgentOSError("任务ID不能为空", error_code=CODE_MISSING_PARAMETER)

        resp = self._api.get(f"/api/v1/tasks/{task_id}")

        data = extract_data_map(resp)
        if not data:
            raise AgentOSError("任务详情响应格式异常", error_code=CODE_INVALID_RESPONSE)

        return self._parse_task_from_map(data)

    def query(self, task_id: str) -> TaskStatus:
        """
        查询任务的当前状态。

        Args:
            task_id: 任务 ID

        Returns:
            TaskStatus: 任务状态

        Raises:
            AgentOSError: 参数无效或请求失败

        Example:
            >>> status = manager.query("task_123")
            >>> print(status)
            <TaskStatus.RUNNING: 'running'>
        """
        task = self.get(task_id)
        return task.status

    def wait(self, task_id: str, timeout: float = 0) -> TaskResult:
        """
        阻塞等待任务到达终态。

        Args:
            task_id: 任务 ID
            timeout: 超时时间（秒），0 表示无限等待

        Returns:
            TaskResult: 任务结果

        Raises:
            AgentOSError: 超时或请求失败

        Example:
            >>> result = manager.wait("task_123", timeout=30)
            >>> print(result.status)
            <TaskStatus.COMPLETED: 'completed'>
        """
        start = time.time()

        while True:
            status = self.query(task_id)

            if status.is_terminal():
                task = self.get(task_id)
                return TaskResult(
                    id=task.id,
                    status=task.status,
                    output=task.output,
                    error=task.error,
                    start_time=datetime.fromtimestamp(start),
                    end_time=datetime.now(),
                    duration=time.time() - start,
                )

            if timeout > 0 and (time.time() - start) > timeout:
                raise AgentOSError(
                    f"任务 {task_id} 超时",
                    error_code=CODE_TASK_TIMEOUT
                )

            time.sleep(0.5)

    def cancel(self, task_id: str) -> None:
        """
        取消正在执行的任务。

        Args:
            task_id: 任务 ID

        Raises:
            AgentOSError: 参数无效或请求失败

        Example:
            >>> manager.cancel("task_123")
        """
        if not task_id:
            raise AgentOSError("任务ID不能为空", error_code=CODE_MISSING_PARAMETER)

        self._api.post(f"/api/v1/tasks/{task_id}/cancel", None)

    def list(self, opts: Optional[ListOptions] = None) -> List[Task]:
        """
        列出任务，支持分页和过滤。

        Args:
            opts: 列表查询选项

        Returns:
            List[Task]: 任务列表

        Example:
            >>> from agentos.types import ListOptions, PaginationOptions
            >>> opts = ListOptions(pagination=PaginationOptions(page=1, page_size=10))
            >>> tasks = manager.list(opts)
        """
        path = "/api/v1/tasks"
        if opts:
            path = build_url(path, opts.to_query_params())

        resp = self._api.get(path)
        return self._parse_task_list(resp)

    def delete(self, task_id: str) -> None:
        """
        删除指定任务。

        Args:
            task_id: 任务 ID

        Raises:
            AgentOSError: 参数无效或请求失败

        Example:
            >>> manager.delete("task_123")
        """
        if not task_id:
            raise AgentOSError("任务ID不能为空", error_code=CODE_MISSING_PARAMETER)

        self._api.delete(f"/api/v1/tasks/{task_id}")

    def get_result(self, task_id: str) -> TaskResult:
        """
        获取已完成任务的结果。

        Args:
            task_id: 任务 ID

        Returns:
            TaskResult: 任务结果

        Raises:
            AgentOSError: 任务尚未完成或请求失败

        Example:
            >>> result = manager.get_result("task_123")
            >>> print(result.output)
            'Analysis complete'
        """
        task = self.get(task_id)
        if not task.status.is_terminal():
            raise AgentOSError("任务尚未完成", error_code=CODE_MISSING_PARAMETER)

        return TaskResult(
            id=task.id,
            status=task.status,
            output=task.output,
            error=task.error,
        )

    def batch_submit(self, descriptions: List[str]) -> List[Task]:
        """
        批量提交多个任务。

        Args:
            descriptions: 任务描述列表

        Returns:
            List[Task]: 创建的任务列表

        Raises:
            AgentOSError: 请求失败

        Example:
            >>> tasks = manager.batch_submit(["task1", "task2", "task3"])
            >>> print(len(tasks))
            3
        """
        tasks = []
        for desc in descriptions:
            task = self.submit(desc)
            tasks.append(task)
        return tasks

    def count(self) -> int:
        """
        获取任务总数。

        Returns:
            int: 任务总数

        Example:
            >>> total = manager.count()
            >>> print(total)
            42
        """
        resp = self._api.get("/api/v1/tasks/count")
        data = extract_data_map(resp)
        if not data:
            return 0
        return get_int(data, "count")

    def wait_for_any(
        self,
        task_ids: List[str],
        timeout: float = 0
    ) -> TaskResult:
        """
        并发等待任一任务完成，返回最先到达终态的结果。

        Args:
            task_ids: 任务 ID 列表
            timeout: 超时时间（秒）

        Returns:
            TaskResult: 最先完成的任务结果

        Raises:
            AgentOSError: 参数无效、超时或所有任务失败

        Example:
            >>> result = manager.wait_for_any(["task1", "task2"], timeout=30)
            >>> print(result.id)
            'task1'
        """
        if not task_ids:
            raise AgentOSError("任务ID列表不能为空", error_code=CODE_MISSING_PARAMETER)

        with ThreadPoolExecutor(max_workers=len(task_ids)) as executor:
            futures = {
                executor.submit(self.wait, task_id, timeout): task_id
                for task_id in task_ids
            }

            for future in as_completed(futures):
                try:
                    result = future.result()
                    return result
                except AgentOSError:
                    continue

        raise AgentOSError("所有任务已完成但无结果", error_code=CODE_TASK_FAILED)

    def wait_for_all(
        self,
        task_ids: List[str],
        timeout: float = 0
    ) -> List[TaskResult]:
        """
        并发等待多个任务全部完成。

        Args:
            task_ids: 任务 ID 列表
            timeout: 超时时间（秒）

        Returns:
            List[TaskResult]: 所有任务结果列表

        Raises:
            AgentOSError: 超时

        Example:
            >>> results = manager.wait_for_all(["task1", "task2"], timeout=60)
            >>> print(len(results))
            2
        """
        if not task_ids:
            return []

        results = [None] * len(task_ids)

        with ThreadPoolExecutor(max_workers=len(task_ids)) as executor:
            future_to_idx = {
                executor.submit(self.wait, task_id, timeout): idx
                for idx, task_id in enumerate(task_ids)
            }

            for future in as_completed(future_to_idx):
                idx = future_to_idx[future]
                try:
                    results[idx] = future.result()
                except AgentOSError:
                    pass

        return [r for r in results if r is not None]

    def _parse_task_from_map(
        self,
        data: Dict[str, Any],
        description: str = "",
        priority: int = 0,
        metadata: Optional[Dict[str, Any]] = None
    ) -> Task:
        """
        从字典解析 Task 结构。

        Args:
            data: 数据字典
            description: 任务描述（默认值）
            priority: 优先级（默认值）
            metadata: 元数据（默认值）

        Returns:
            Task: 解析的任务对象
        """
        return Task(
            id=get_string(data, "task_id"),
            description=get_string(data, "description") or description,
            status=TaskStatus(get_string(data, "status") or "pending"),
            priority=get_int(data, "priority") or priority,
            output=get_string(data, "output"),
            error=get_string(data, "error"),
            metadata=get_dict(data, "metadata") or metadata or {},
            created_at=parse_time_from_map(data, "created_at"),
            updated_at=parse_time_from_map(data, "updated_at"),
        )

    def _parse_task_list(self, resp: Any) -> List[Task]:
        """
        从 APIResponse 解析 Task 列表。

        Args:
            resp: API 响应

        Returns:
            List[Task]: 任务列表
        """
        data = extract_data_map(resp)
        if not data:
            return []

        items = get_list(data, "tasks")
        tasks = []
        for item in items:
            if isinstance(item, dict):
                tasks.append(self._parse_task_from_map(item))

        return tasks
