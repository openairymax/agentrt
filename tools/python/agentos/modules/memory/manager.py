# AgentOS Python SDK - Memory Manager Implementation
# Version: 3.0.0
# Last updated: 2026-03-24

"""
Memory manager implementation for managing memory lifecycle.

Provides comprehensive memory management including write, search,
update, delete, and batch operations.

Corresponds to Go SDK: modules/memory/manager.go
"""

from dataclasses import dataclass
from datetime import datetime
from typing import Any, Dict, List, Optional

from ...client.client import APIClient
from ...exceptions import AgentOSError, CODE_MISSING_PARAMETER, CODE_INVALID_RESPONSE
from ...types import Memory, MemoryLayer, MemorySearchResult, ListOptions
from ...utils import (
    get_string, get_int, get_float, get_dict, get_list,
    extract_data_map, build_url, parse_time_from_map, extract_int_stats
)


@dataclass
class MemoryWriteItem:
    """
    批量写入时的单条记忆项。

    用于 BatchWrite 操作。
    """
    content: str
    layer: MemoryLayer
    metadata: Optional[Dict[str, Any]] = None


class MemoryManager:
    """
    记忆管理器，管理记忆完整生命周期。

    提供记忆的写入、搜索、更新、删除及统计功能。

    对应 Go SDK: modules/memory/manager.go MemoryManager

    Example:
        >>> from agentos.client import Client
        >>> from agentos.modules import MemoryManager
        >>> client = Client()
        >>> mem_mgr = MemoryManager(client)
        >>> memory = mem_mgr.write("important fact", MemoryLayer.L1)
        >>> results = mem_mgr.search("fact", top_k=10)
    """

    def __init__(self, api: APIClient):
        """
        初始化记忆管理器。

        Args:
            api: APIClient 实例，用于 HTTP 通信
        """
        self._api = api

    def write(self, content: str, layer: MemoryLayer) -> Memory:
        """
        写入一条新记忆到指定层级。

        Args:
            content: 记忆内容
            layer: 记忆层级

        Returns:
            Memory: 创建的记忆对象

        Raises:
            AgentOSError: 参数无效或请求失败

        Example:
            >>> memory = manager.write("important fact", MemoryLayer.L1)
            >>> print(memory.id)
            'mem_123'
        """
        return self.write_with_options(content, layer, None)

    def write_with_options(
        self,
        content: str,
        layer: MemoryLayer,
        metadata: Optional[Dict[str, Any]]
    ) -> Memory:
        """
        使用元数据选项写入新记忆。

        Args:
            content: 记忆内容
            layer: 记忆层级
            metadata: 元数据

        Returns:
            Memory: 创建的记忆对象

        Raises:
            AgentOSError: 参数无效或请求失败

        Example:
            >>> memory = manager.write_with_options(
            ...     "fact",
            ...     MemoryLayer.L2,
            ...     metadata={"source": "user"}
            ... )
        """
        if not content:
            raise AgentOSError("记忆内容不能为空", error_code=CODE_MISSING_PARAMETER)

        body = {"content": content, "layer": layer.value}
        if metadata:
            body["metadata"] = metadata

        resp = self._api.post("/api/v1/memories", body)

        data = extract_data_map(resp)
        if not data:
            raise AgentOSError("记忆写入响应格式异常", error_code=CODE_INVALID_RESPONSE)

        return Memory(
            id=get_string(data, "memory_id"),
            content=content,
            layer=layer,
            score=1.0,
            metadata=metadata or {},
            created_at=datetime.now(),
            updated_at=datetime.now(),
        )

    def get(self, memory_id: str) -> Memory:
        """
        获取指定记忆的详细信息。

        Args:
            memory_id: 记忆 ID

        Returns:
            Memory: 记忆对象

        Raises:
            AgentOSError: 参数无效或请求失败

        Example:
            >>> memory = manager.get("mem_123")
            >>> print(memory.content)
            'important fact'
        """
        if not memory_id:
            raise AgentOSError("记忆ID不能为空", error_code=CODE_MISSING_PARAMETER)

        resp = self._api.get(f"/api/v1/memories/{memory_id}")

        data = extract_data_map(resp)
        if not data:
            raise AgentOSError("记忆详情响应格式异常", error_code=CODE_INVALID_RESPONSE)

        return self._parse_memory_from_map(data)

    def search(self, query: str, top_k: int = 10) -> MemorySearchResult:
        """
        搜索记忆，返回按相关度排序的结果。

        Args:
            query: 搜索查询
            top_k: 返回结果数量

        Returns:
            MemorySearchResult: 搜索结果

        Raises:
            AgentOSError: 参数无效或请求失败

        Example:
            >>> results = manager.search("important", top_k=5)
            >>> print(len(results.memories))
            5
        """
        if not query:
            raise AgentOSError("搜索查询不能为空", error_code=CODE_MISSING_PARAMETER)

        if top_k <= 0:
            top_k = 10

        path = build_url("/api/v1/memories/search", {
            "query": query,
            "top_k": str(top_k),
        })

        resp = self._api.get(path)
        return self._parse_memory_search_result(resp, query, top_k)

    def search_by_layer(
        self,
        query: str,
        layer: MemoryLayer,
        top_k: int = 10
    ) -> MemorySearchResult:
        """
        在指定层级内搜索记忆。

        Args:
            query: 搜索查询
            layer: 记忆层级
            top_k: 返回结果数量

        Returns:
            MemorySearchResult: 搜索结果

        Raises:
            AgentOSError: 参数无效或请求失败

        Example:
            >>> results = manager.search_by_layer("fact", MemoryLayer.L1, top_k=5)
        """
        if not query:
            raise AgentOSError("搜索查询不能为空", error_code=CODE_MISSING_PARAMETER)

        if top_k <= 0:
            top_k = 10

        path = build_url("/api/v1/memories/search", {
            "query": query,
            "layer": layer.value,
            "top_k": str(top_k),
        })

        resp = self._api.get(path)
        return self._parse_memory_search_result(resp, query, top_k)

    def update(self, memory_id: str, content: str) -> Memory:
        """
        更新指定记忆的内容。

        Args:
            memory_id: 记忆 ID
            content: 新内容

        Returns:
            Memory: 更新后的记忆对象

        Raises:
            AgentOSError: 参数无效或请求失败

        Example:
            >>> memory = manager.update("mem_123", "updated content")
        """
        if not memory_id:
            raise AgentOSError("记忆ID不能为空", error_code=CODE_MISSING_PARAMETER)

        resp = self._api.put(f"/api/v1/memories/{memory_id}", {"content": content})

        data = extract_data_map(resp)
        if not data:
            raise AgentOSError("记忆更新响应格式异常", error_code=CODE_INVALID_RESPONSE)

        return self._parse_memory_from_map(data)

    def delete(self, memory_id: str) -> None:
        """
        删除指定记忆。

        Args:
            memory_id: 记忆 ID

        Raises:
            AgentOSError: 参数无效或请求失败

        Example:
            >>> manager.delete("mem_123")
        """
        if not memory_id:
            raise AgentOSError("记忆ID不能为空", error_code=CODE_MISSING_PARAMETER)

        self._api.delete(f"/api/v1/memories/{memory_id}")

    def list(self, opts: Optional[ListOptions] = None) -> List[Memory]:
        """
        列出记忆，支持分页和过滤。

        Args:
            opts: 列表查询选项

        Returns:
            List[Memory]: 记忆列表

        Example:
            >>> from agentos.types import ListOptions, PaginationOptions
            >>> opts = ListOptions(pagination=PaginationOptions(page=1, page_size=10))
            >>> memories = manager.list(opts)
        """
        path = "/api/v1/memories"
        if opts:
            path = build_url(path, opts.to_query_params())

        resp = self._api.get(path)
        return self._parse_memory_list(resp)

    def list_by_layer(
        self,
        layer: MemoryLayer,
        opts: Optional[ListOptions] = None
    ) -> List[Memory]:
        """
        按层级列出记忆。

        Args:
            layer: 记忆层级
            opts: 列表查询选项

        Returns:
            List[Memory]: 记忆列表

        Example:
            >>> memories = manager.list_by_layer(MemoryLayer.L1)
        """
        params = {"layer": layer.value}
        if opts:
            params.update(opts.to_query_params())

        resp = self._api.get(build_url("/api/v1/memories", params))
        return self._parse_memory_list(resp)

    def count(self) -> int:
        """
        获取记忆总数。

        Returns:
            int: 记忆总数

        Example:
            >>> total = manager.count()
            >>> print(total)
            100
        """
        resp = self._api.get("/api/v1/memories/count")
        data = extract_data_map(resp)
        if not data:
            return 0
        return get_int(data, "count")

    def clear(self) -> None:
        """
        清空所有记忆数据。

        Example:
            >>> manager.clear()
        """
        self._api.delete("/api/v1/memories")

    def batch_write(self, memories: List[MemoryWriteItem]) -> List[Memory]:
        """
        批量写入多条记忆。

        Args:
            memories: 记忆项列表

        Returns:
            List[Memory]: 创建的记忆列表

        Raises:
            AgentOSError: 请求失败

        Example:
            >>> items = [
            ...     MemoryWriteItem("fact1", MemoryLayer.L1),
            ...     MemoryWriteItem("fact2", MemoryLayer.L2),
            ... ]
            >>> memories = manager.batch_write(items)
        """
        results = []
        for item in memories:
            memory = self.write_with_options(item.content, item.layer, item.metadata)
            results.append(memory)
        return results

    def evolve(self) -> None:
        """
        触发记忆演化过程（L1→L2→L3→L4 层级升华）。

        Example:
            >>> manager.evolve()
        """
        self._api.post("/api/v1/memories/evolve", None)

    def get_stats(self) -> Dict[str, int]:
        """
        获取各层级的记忆统计数据。

        Returns:
            Dict[str, int]: 统计数据字典

        Example:
            >>> stats = manager.get_stats()
            >>> print(stats)
            {'L1': 50, 'L2': 30, 'L3': 15, 'L4': 5}
        """
        resp = self._api.get("/api/v1/memories/stats")
        data = extract_data_map(resp)
        if not data:
            return {}
        return extract_int_stats(data)

    def _parse_memory_from_map(self, data: Dict[str, Any]) -> Memory:
        """
        从字典解析 Memory 结构。

        Args:
            data: 数据字典

        Returns:
            Memory: 解析的记忆对象
        """
        return Memory(
            id=get_string(data, "memory_id"),
            content=get_string(data, "content"),
            layer=MemoryLayer(get_string(data, "layer") or "L1"),
            score=get_float(data, "score"),
            metadata=get_dict(data, "metadata") or {},
            created_at=parse_time_from_map(data, "created_at"),
            updated_at=parse_time_from_map(data, "updated_at"),
        )

    def _parse_memory_list(self, resp: Any) -> List[Memory]:
        """
        从 APIResponse 解析 Memory 列表。

        Args:
            resp: API 响应

        Returns:
            List[Memory]: 记忆列表
        """
        data = extract_data_map(resp)
        if not data:
            return []

        items = get_list(data, "memories")
        memories = []
        for item in items:
            if isinstance(item, dict):
                memories.append(self._parse_memory_from_map(item))

        return memories

    def _parse_memory_search_result(
        self,
        resp: Any,
        query: str,
        top_k: int
    ) -> MemorySearchResult:
        """
        从 APIResponse 解析记忆搜索结果。

        Args:
            resp: API 响应
            query: 搜索查询
            top_k: 返回数量

        Returns:
            MemorySearchResult: 搜索结果
        """
        data = extract_data_map(resp)
        if not data:
            raise AgentOSError("记忆搜索响应格式异常", error_code=CODE_INVALID_RESPONSE)

        memories = self._parse_memory_list(resp)

        return MemorySearchResult(
            memories=memories,
            total=get_int(data, "total"),
            query=query,
            top_k=top_k,
        )
