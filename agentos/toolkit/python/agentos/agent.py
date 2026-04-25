# AgentOS Python SDK Client
# Version: 2.0.0
# Last updated: 2026-03-23

"""
AgentOS Python SDK Client implementation.

This module provides the main client classes for interacting with the AgentOS system.
"""

import json
import time
import logging
import asyncio
from typing import Optional, Dict, Any, List
from urllib.parse import quote_plus
import requests
import aiohttp

from .exceptions import AgentOSError, NetworkError, AgentOSTimeoutError
from .task import Task
from .memory import Memory
from .session import Session
from .skill import Skill

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class AgentOS:
    """AgentOS synchronous client.
    
    This class provides a synchronous interface to interact with the AgentOS system.
    """
    
    def __init__(self, endpoint: str = "http://localhost:18789", timeout: int = 30, api_key: Optional[str] = None):
        """
        Initialize the AgentOS client.
        
        Args:
            endpoint: The AgentOS server endpoint.
            timeout: The request timeout in seconds.
            api_key: Optional API key for authentication.
        """
        self.endpoint = endpoint.rstrip('/')
        self.timeout = timeout
        self.api_key = api_key
        self.session = requests.Session()
        self.session.headers.update({"Content-Type": "application/json"})
        if api_key:
            self.session.headers.update({"Authorization": f"Bearer {api_key}"})
    
    def _request(self, method: str, path: str, data: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        """
        Make an HTTP request to the AgentOS server.
        
        Args:
            method: HTTP method (GET, POST, PUT, DELETE).
            path: API path.
            data: Request data.
        
        Returns:
            The response data as a dictionary.
        
        Raises:
            NetworkError: If there's a network error.
            TimeoutError: If the request times out.
            AgentOSError: If the server returns an error.
        """
        url = f"{self.endpoint}{path}"
        try:
            response = self._execute_http_method(method, url, data)
            response.raise_for_status()
            return response.json()
        except requests.exceptions.Timeout:
            raise AgentOSTimeoutError(operation=f"HTTP request (timeout={self.timeout}s)")
        except requests.exceptions.RequestException as e:
            raise NetworkError(f"Network error: {str(e)}", cause=e)
        except json.JSONDecodeError:
            raise AgentOSError("Invalid JSON response from server")
    
    def _execute_http_method(self, method: str, url: str, data: Optional[Dict[str, Any]]) -> requests.Response:
        """Execute the appropriate HTTP method"""
        if method == "GET":
            return self.session.get(url, timeout=self.timeout)
        elif method == "POST":
            return self.session.post(url, json=data, timeout=self.timeout)
        elif method == "PUT":
            return self.session.put(url, json=data, timeout=self.timeout)
        elif method == "DELETE":
            return self.session.delete(url, timeout=self.timeout)
        else:
            raise AgentOSError(f"Unsupported HTTP method: {method}")
    
    def submit_task(self, task_description: str) -> Task:
        """
        Submit a task to the AgentOS system.
        
        Args:
            task_description: The task description.
        
        Returns:
            A Task object representing the submitted task.
        """
        data = {"description": task_description}
        response = self._request("POST", "/api/v1/tasks", data)
        task_id = response.get("task_id")
        if not task_id:
            raise AgentOSError("Invalid response from server: missing task_id")
        return Task(self, task_id)
    
    def write_memory(self, content: str, metadata: Optional[Dict[str, Any]] = None) -> str:
        """
        Write a memory to the AgentOS system.
        
        Args:
            content: The memory content.
            metadata: Optional metadata.
        
        Returns:
            The memory ID.
        """
        data = {"content": content, "metadata": metadata or {}}
        response = self._request("POST", "/api/v1/memories", data)
        memory_id = response.get("memory_id")
        if not memory_id:
            raise AgentOSError("Invalid response from server: missing memory_id")
        return memory_id
    
    def search_memory(self, query: str, top_k: int = 5) -> List[Memory]:
        """
        Search memories in the AgentOS system.
        
        Args:
            query: The search query.
            top_k: The maximum number of results to return.
        
        Returns:
            A list of Memory objects.
        """
        params = f"?query={quote_plus(query)}&top_k={top_k}"
        response = self._request("GET", f"/api/v1/memories/search{params}")
        memories = []
        for mem_data in response.get("memories", []):
            memory = Memory(
                memory_id=mem_data.get("memory_id"),
                content=mem_data.get("content"),
                created_at=mem_data.get("created_at"),
                metadata=mem_data.get("metadata")
            )
            memories.append(memory)
        return memories
    
    def get_memory(self, memory_id: str) -> Memory:
        """
        Get a memory by ID.
        
        Args:
            memory_id: The memory ID.
        
        Returns:
            A Memory object.
        """
        response = self._request("GET", f"/api/v1/memories/{memory_id}")
        return Memory(
            memory_id=response.get("memory_id"),
            content=response.get("content"),
            created_at=response.get("created_at"),
            metadata=response.get("metadata")
        )
    
    def delete_memory(self, memory_id: str) -> bool:
        """
        Delete a memory by ID.
        
        Args:
            memory_id: The memory ID.
        
        Returns:
            True if the memory was deleted successfully.
        """
        response = self._request("DELETE", f"/api/v1/memories/{memory_id}")
        return response.get("success", False)
    
    def create_session(self) -> Session:
        """
        Create a new session.
        
        Returns:
            A Session object.
        """
        response = self._request("POST", "/api/v1/sessions")
        session_id = response.get("session_id")
        if not session_id:
            raise AgentOSError("Invalid response from server: missing session_id")
        return Session(self, session_id)
    
    def load_skill(self, skill_name: str) -> Skill:
        """
        Load a skill by name.
        
        Args:
            skill_name: The skill name.
        
        Returns:
            A Skill object.
        """
        return Skill(self, skill_name)
    
    def close(self):
        """
        Close the client session.
        """
        self.session.close()

class AsyncAgentOS:
    """AgentOS asynchronous client.
    
    This class provides an asynchronous interface to interact with the AgentOS system.
    """
    
    def __init__(self, endpoint: str = "http://localhost:18789", timeout: int = 30, api_key: Optional[str] = None):
        """
        Initialize the AsyncAgentOS client.
        
        Args:
            endpoint: The AgentOS server endpoint.
            timeout: The request timeout in seconds.
            api_key: Optional API key for authentication.
        """
        self.endpoint = endpoint.rstrip('/')
        self.timeout = timeout
        self.api_key = api_key
        self.session = None
    
    async def _request(self, method: str, path: str, data: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        """
        Make an asynchronous HTTP request to the AgentOS server.
        
        Args:
            method: HTTP method (GET, POST, PUT, DELETE).
            path: API path.
            data: Request data.
        
        Returns:
            The response data as a dictionary.
        
        Raises:
            NetworkError: If there's a network error.
            TimeoutError: If the request times out.
            AgentOSError: If the server returns an error.
        """
        url = f"{self.endpoint}{path}"
        try:
            await self._ensure_session()
            timeout = aiohttp.ClientTimeout(total=self.timeout)
            async with await self._execute_async_http_method(method, url, data, timeout) as resp:
                if resp.status >= 400:
                    body = await resp.text()
                    raise AgentOSError(message=f"HTTP {resp.status}: {body[:200]}")
                return await resp.json()
        except asyncio.TimeoutError:
            raise AgentOSTimeoutError(operation=f"HTTP request (timeout={self.timeout}s)")
        except aiohttp.ClientError as e:
            raise NetworkError(f"Network error: {str(e)}", cause=e)
        except json.JSONDecodeError:
            raise AgentOSError("Invalid JSON response from server")
    
    async def _ensure_session(self):
        """Ensure session is initialized"""
        if self.session is None:
            headers = {"Content-Type": "application/json"}
            if self.api_key:
                headers["Authorization"] = f"Bearer {self.api_key}"
            self.session = aiohttp.ClientSession(headers=headers)
    
    async def _execute_async_http_method(self, method: str, url: str, data: Optional[Dict[str, Any]], timeout) -> aiohttp.ClientResponse:
        if method == "GET":
            return self.session.get(url, timeout=timeout)
        elif method == "POST":
            return self.session.post(url, json=data, timeout=timeout)
        elif method == "PUT":
            return self.session.put(url, json=data, timeout=timeout)
        elif method == "DELETE":
            return self.session.delete(url, timeout=timeout)
        else:
            raise AgentOSError(message=f"Unsupported HTTP method: {method}")
    
    async def submit_task(self, task_description: str) -> Task:
        """
        Submit a task to the AgentOS system asynchronously.
        
        Args:
            task_description: The task description.
        
        Returns:
            A Task object representing the submitted task.
        """
        data = {"description": task_description}
        response = await self._request("POST", "/api/v1/tasks", data)
        task_id = response.get("task_id")
        if not task_id:
            raise AgentOSError("Invalid response from server: missing task_id")
        return Task(self, task_id)
    
    async def write_memory(self, content: str, metadata: Optional[Dict[str, Any]] = None) -> str:
        """
        Write a memory to the AgentOS system asynchronously.
        
        Args:
            content: The memory content.
            metadata: Optional metadata.
        
        Returns:
            The memory ID.
        """
        data = {"content": content, "metadata": metadata or {}}
        response = await self._request("POST", "/api/v1/memories", data)
        memory_id = response.get("memory_id")
        if not memory_id:
            raise AgentOSError("Invalid response from server: missing memory_id")
        return memory_id
    
    async def search_memory(self, query: str, top_k: int = 5) -> List[Memory]:
        """
        Search memories in the AgentOS system asynchronously.
        
        Args:
            query: The search query.
            top_k: The maximum number of results to return.
        
        Returns:
            A list of Memory objects.
        """
        params = f"?query={quote_plus(query)}&top_k={top_k}"
        response = await self._request("GET", f"/api/v1/memories/search{params}")
        memories = []
        for mem_data in response.get("memories", []):
            memory = Memory(
                memory_id=mem_data.get("memory_id"),
                content=mem_data.get("content"),
                created_at=mem_data.get("created_at"),
                metadata=mem_data.get("metadata")
            )
            memories.append(memory)
        return memories
    
    async def get_memory(self, memory_id: str) -> Memory:
        """
        Get a memory by ID asynchronously.
        
        Args:
            memory_id: The memory ID.
        
        Returns:
            A Memory object.
        """
        response = await self._request("GET", f"/api/v1/memories/{memory_id}")
        return Memory(
            memory_id=response.get("memory_id"),
            content=response.get("content"),
            created_at=response.get("created_at"),
            metadata=response.get("metadata")
        )
    
    async def delete_memory(self, memory_id: str) -> bool:
        """
        Delete a memory by ID asynchronously.
        
        Args:
            memory_id: The memory ID.
        
        Returns:
            True if the memory was deleted successfully.
        """
        response = await self._request("DELETE", f"/api/v1/memories/{memory_id}")
        return response.get("success", False)
    
    async def create_session(self) -> Session:
        """
        Create a new session asynchronously.
        
        Returns:
            A Session object.
        """
        response = await self._request("POST", "/api/v1/sessions")
        session_id = response.get("session_id")
        if not session_id:
            raise AgentOSError("Invalid response from server: missing session_id")
        return Session(self, session_id)
    
    async def load_skill(self, skill_name: str) -> Skill:
        """
        Load a skill by name asynchronously.
        
        Args:
            skill_name: The skill name.
        
        Returns:
            A Skill object.
        """
        return Skill(self, skill_name)
    
    async def close(self):
        """
        Close the asynchronous client session.
        """
        if self.session:
            await self.session.close()
