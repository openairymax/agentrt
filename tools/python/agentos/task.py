# AgentOS Python SDK Task
# Version: 1.0.0.5
# Last updated: 2026-03-21

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
            response = self.client._request("GET", f"/api/tasks/{self.task_id}")
            status_str = response.get("status")
            if not status_str:
                raise TaskError("Invalid response from server: missing status")
            try:
                return TaskStatus(status_str)
            except ValueError:
                raise TaskError(f"Invalid task status: {status_str}")
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
            response = await self.client._request("GET", f"/api/tasks/{self.task_id}")
            status_str = response.get("status")
            if not status_str:
                raise TaskError("Invalid response from server: missing status")
            try:
                return TaskStatus(status_str)
            except ValueError:
                raise TaskError(f"Invalid task status: {status_str}")
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
        start_time = time.time()
        while True:
            status = self.query()
            if status in (TaskStatus.COMPLETED, TaskStatus.FAILED, TaskStatus.CANCELLED):
                response = self.client._request("GET", f"/api/tasks/{self.task_id}")
                return TaskResult(
                    task_id=self.task_id,
                    status=status,
                    output=response.get("output"),
                    error=response.get("error")
                )
            
            if timeout and time.time() - start_time > timeout:
                raise TimeoutError(f"Task did not complete within {timeout} seconds")
            
            time.sleep(0.5)  # Wait for 500ms before querying again
    
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
        start_time = time.time()
        while True:
            status = await self.query_async()
            if status in (TaskStatus.COMPLETED, TaskStatus.FAILED, TaskStatus.CANCELLED):
                response = await self.client._request("GET", f"/api/tasks/{self.task_id}")
                return TaskResult(
                    task_id=self.task_id,
                    status=status,
                    output=response.get("output"),
                    error=response.get("error")
                )
            
            if timeout and time.time() - start_time > timeout:
                raise TimeoutError(f"Task did not complete within {timeout} seconds")
            
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
            response = self.client._request("POST", f"/api/tasks/{self.task_id}/cancel")
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
            response = await self.client._request("POST", f"/api/tasks/{self.task_id}/cancel")
            return response.get("success", False)
        except Exception as e:
            raise TaskError(f"Error cancelling task: {str(e)}")
