# AgentOS Python SDK Task
# Version: 3.0.0
# Last updated: 2026-04-04

"""
Task class implementation for the AgentOS Python SDK.
"""

import time
from typing import Optional

from .types import TaskStatus, TaskResult
from .exceptions import TaskError, TimeoutError

class Task:
    """Task class for managing AgentOS tasks.
    
    This class provides methods to query, wait for, and cancel tasks.
    """
    
    def __init__(self, client, task_id: str):
        """
        Initialize a Task object.
        
        Args:
            client: The AgentOS client instance.
            task_id: The task ID.
        """
        self.client = client
        self.task_id = task_id
    
    def query(self) -> TaskStatus:
        """
        Query the task status.
        
        Returns:
            The current task status.
        
        Raises:
            TaskError: If there's an error querying the task status.
        """
        try:
            response = self.client._request("GET", f"/api/v1/tasks/{self.task_id}")
            status_str = response.get("status")
            if not status_str:
                raise TaskError("Invalid response from server: missing status")
            
            # Map string status to TaskStatus enum
            status_mapping = {
                "pending": TaskStatus.PENDING,
                "running": TaskStatus.RUNNING,
                "succeeded": TaskStatus.SUCCEEDED,
                "completed": TaskStatus.SUCCEEDED,  # Alternative name
                "failed": TaskStatus.FAILED,
                "cancelled": TaskStatus.CANCELLED,
            }
            
            status_lower = status_str.lower()
            if status_lower not in status_mapping:
                raise TaskError(f"Invalid task status: {status_str}")
            
            return status_mapping[status_lower]
        except Exception as e:
            raise TaskError(f"Error querying task status: {str(e)}")
    
    async def query_async(self) -> TaskStatus:
        """
        Query the task status asynchronously.
        
        Returns:
            The current task status.
        
        Raises:
            TaskError: If there's an error querying the task status.
        """
        try:
            response = await self.client._request("GET", f"/api/v1/tasks/{self.task_id}")
            status_str = response.get("status")
            if not status_str:
                raise TaskError("Invalid response from server: missing status")
            
            # Map string status to TaskStatus enum
            status_mapping = {
                "pending": TaskStatus.PENDING,
                "running": TaskStatus.RUNNING,
                "succeeded": TaskStatus.SUCCEEDED,
                "completed": TaskStatus.SUCCEEDED,  # Alternative name
                "failed": TaskStatus.FAILED,
                "cancelled": TaskStatus.CANCELLED,
            }
            
            status_lower = status_str.lower()
            if status_lower not in status_mapping:
                raise TaskError(f"Invalid task status: {status_str}")
            
            return status_mapping[status_lower]
        except Exception as e:
            raise TaskError(f"Error querying task status: {str(e)}")
    
    def wait(self, timeout: Optional[int] = None) -> TaskResult:
        """
        Wait for the task to complete.
        
        Args:
            timeout: The maximum time to wait in seconds.
        
        Returns:
            The task result.
        
        Raises:
            TimeoutError: If the task doesn't complete within the timeout.
            TaskError: If there's an error waiting for the task.
        """
        import time as time_module
        start_time = time_module.time()
        while True:
            status = self.query()
            if status in (TaskStatus.SUCCEEDED, TaskStatus.FAILED, TaskStatus.CANCELLED):
                response = self.client._request("GET", f"/api/v1/tasks/{self.task_id}")
                return TaskResult(
                    task_id=self.task_id,
                    status=status,
                    result=response.get("result"),
                    error=response.get("error")
                )
            
            if timeout and time_module.time() - start_time > timeout:
                raise TimeoutError(timeout_ms=int(timeout * 1000), operation="task wait")
            
            time_module.sleep(0.5)  # Wait for 500ms before querying again
    
    async def wait_async(self, timeout: Optional[int] = None) -> TaskResult:
        """
        Wait for the task to complete asynchronously.
        
        Args:
            timeout: The maximum time to wait in seconds.
        
        Returns:
            The task result.
        
        Raises:
            TimeoutError: If the task doesn't complete within the timeout.
            TaskError: If there's an error waiting for the task.
        """
        import asyncio
        import time as time_module
        start_time = time_module.time()
        while True:
            status = await self.query_async()
            if status in (TaskStatus.SUCCEEDED, TaskStatus.FAILED, TaskStatus.CANCELLED):
                response = await self.client._request("GET", f"/api/v1/tasks/{self.task_id}")
                return TaskResult(
                    task_id=self.task_id,
                    status=status,
                    result=response.get("result"),
                    error=response.get("error")
                )
            
            if timeout and time_module.time() - start_time > timeout:
                raise TimeoutError(timeout_ms=int(timeout * 1000), operation="task wait")
            
            await asyncio.sleep(0.5)  # Wait for 500ms before querying again
    
    def cancel(self) -> bool:
        """
        Cancel the task.
        
        Returns:
            True if the task was cancelled successfully.
        
        Raises:
            TaskError: If there's an error cancelling the task.
        """
        try:
            response = self.client._request("POST", f"/api/v1/tasks/{self.task_id}/cancel")
            return response.get("success", False)
        except Exception as e:
            raise TaskError(f"Error cancelling task: {str(e)}")
    
    async def cancel_async(self) -> bool:
        """
        Cancel the task asynchronously.
        
        Returns:
            True if the task was cancelled successfully.
        
        Raises:
            TaskError: If there's an error cancelling the task.
        """
        try:
            response = await self.client._request("POST", f"/api/v1/tasks/{self.task_id}/cancel")
            return response.get("success", False)
        except Exception as e:
            raise TaskError(f"Error cancelling task: {str(e)}")
