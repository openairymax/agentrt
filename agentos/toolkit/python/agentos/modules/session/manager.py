# AgentOS Python SDK - Session Manager Implementation
# Version: 3.0.0
# Last updated: 2026-03-24

"""
Session manager implementation for managing session lifecycle.

Provides comprehensive session management including creation, query,
context management, and cleanup operations.

Corresponds to Go SDK: modules/session/manager.go
"""

from datetime import datetime
from typing import Any, Dict, List, Optional

from ...client.client import APIClient
from ...exceptions import AgentOSError, CODE_MISSING_PARAMETER, CODE_INVALID_RESPONSE
from ...types import Session, SessionStatus, ListOptions
from ...utils import (
    get_string, get_int, get_dict,
    extract_data_map, build_url, parse_time_from_map
)


class SessionManager:
    """
    会话管理器，管理会话完整生命周期。

    提供会话的创建、查询、上下文管理、清理等生命周期管理功能。

    对应 Go SDK: modules/session/manager.go SessionManager

    Example:
        >>> from agentos.client import Client
        >>> from agentos.modules import SessionManager
        >>> client = Client()
        >>> sess_mgr = SessionManager(client)
        >>> session = sess_mgr.create("user_123")
        >>> sess_mgr.set_context(session.id, "key", "value")
    """

    def __init__(self, api: APIClient):
        """
        初始化会话管理器。

        Args:
            api: APIClient 实例，用于 HTTP 通信
        """
        self._api = api

    def create(self, user_id: str) -> Session:
        """
        创建新的用户会话。

        Args:
            user_id: 用户 ID

        Returns:
            Session: 创建的会话对象

        Raises:
            AgentOSError: 请求失败

        Example:
            >>> session = manager.create("user_123")
            >>> print(session.id)
            'sess_456'
        """
        return self.create_with_options(user_id, None)

    def create_with_options(
        self,
        user_id: str,
        metadata: Optional[Dict[str, Any]]
    ) -> Session:
        """
        使用元数据选项创建新会话。

        Args:
            user_id: 用户 ID
            metadata: 元数据

        Returns:
            Session: 创建的会话对象

        Raises:
            AgentOSError: 请求失败

        Example:
            >>> session = manager.create_with_options(
            ...     "user_123",
            ...     metadata={"device": "mobile"}
            ... )
        """
        body = {"user_id": user_id}
        if metadata:
            body["metadata"] = metadata

        resp = self._api.post("/api/v1/sessions", body)

        data = extract_data_map(resp)
        if not data:
            raise AgentOSError("会话创建响应格式异常", error_code=CODE_INVALID_RESPONSE)

        return Session(
            id=get_string(data, "session_id"),
            user_id=user_id,
            status=SessionStatus.ACTIVE,
            context={},
            metadata=metadata or {},
            created_at=datetime.now(),
            last_activity=datetime.now(),
        )

    def get(self, session_id: str) -> Session:
        """
        获取指定会话的详细信息。

        Args:
            session_id: 会话 ID

        Returns:
            Session: 会话对象

        Raises:
            AgentOSError: 参数无效或请求失败

        Example:
            >>> session = manager.get("sess_456")
            >>> print(session.status)
            <SessionStatus.ACTIVE: 'active'>
        """
        if not session_id:
            raise AgentOSError("会话ID不能为空", error_code=CODE_MISSING_PARAMETER)

        resp = self._api.get(f"/api/v1/sessions/{session_id}")

        data = extract_data_map(resp)
        if not data:
            raise AgentOSError("会话详情响应格式异常", error_code=CODE_INVALID_RESPONSE)

        return self._parse_session_from_map(data)

    def set_context(
        self,
        session_id: str,
        key: str,
        value: Any
    ) -> None:
        """
        设置会话上下文中的指定键值对。

        Args:
            session_id: 会话 ID
            key: 上下文键
            value: 上下文值

        Raises:
            AgentOSError: 参数无效或请求失败

        Example:
            >>> manager.set_context("sess_456", "theme", "dark")
        """
        if not session_id:
            raise AgentOSError("会话ID不能为空", error_code=CODE_MISSING_PARAMETER)
        if not key:
            raise AgentOSError("上下文键不能为空", error_code=CODE_MISSING_PARAMETER)

        self._api.post(
            f"/api/v1/sessions/{session_id}/context",
            {"key": key, "value": value}
        )

    def get_context(self, session_id: str, key: str) -> Optional[Any]:
        """
        获取会话上下文中指定键的值。

        Args:
            session_id: 会话 ID
            key: 上下文键

        Returns:
            Optional[Any]: 上下文值

        Raises:
            AgentOSError: 参数无效或请求失败

        Example:
            >>> value = manager.get_context("sess_456", "theme")
            >>> print(value)
            'dark'
        """
        if not session_id:
            raise AgentOSError("会话ID不能为空", error_code=CODE_MISSING_PARAMETER)

        resp = self._api.get(f"/api/v1/sessions/{session_id}/context/{key}")

        data = extract_data_map(resp)
        if not data:
            return None
        return data.get("value")

    def get_all_context(self, session_id: str) -> Dict[str, Any]:
        """
        获取会话的全部上下文数据。

        Args:
            session_id: 会话 ID

        Returns:
            Dict[str, Any]: 上下文字典

        Raises:
            AgentOSError: 参数无效或请求失败

        Example:
            >>> context = manager.get_all_context("sess_456")
            >>> print(context)
            {'theme': 'dark', 'lang': 'zh'}
        """
        if not session_id:
            raise AgentOSError("会话ID不能为空", error_code=CODE_MISSING_PARAMETER)

        resp = self._api.get(f"/api/v1/sessions/{session_id}/context")

        data = extract_data_map(resp)
        if not data:
            return {}
        return get_dict(data, "context") or {}

    def delete_context(self, session_id: str, key: str) -> None:
        """
        删除会话上下文中的指定键。

        Args:
            session_id: 会话 ID
            key: 上下文键

        Raises:
            AgentOSError: 参数无效或请求失败

        Example:
            >>> manager.delete_context("sess_456", "theme")
        """
        if not session_id:
            raise AgentOSError("会话ID不能为空", error_code=CODE_MISSING_PARAMETER)

        self._api.delete(f"/api/v1/sessions/{session_id}/context/{key}")

    def close(self, session_id: str) -> None:
        """
        关闭指定会话。

        Args:
            session_id: 会话 ID

        Raises:
            AgentOSError: 参数无效或请求失败

        Example:
            >>> manager.close("sess_456")
        """
        if not session_id:
            raise AgentOSError("会话ID不能为空", error_code=CODE_MISSING_PARAMETER)

        self._api.delete(f"/api/v1/sessions/{session_id}")

    def list(self, opts: Optional[ListOptions] = None) -> List[Session]:
        """
        列出会话，支持分页和过滤。

        Args:
            opts: 列表查询选项

        Returns:
            List[Session]: 会话列表

        Example:
            >>> from agentos.types import ListOptions, PaginationOptions
            >>> opts = ListOptions(pagination=PaginationOptions(page=1, page_size=10))
            >>> sessions = manager.list(opts)
        """
        path = "/api/v1/sessions"
        if opts:
            path = build_url(path, opts.to_query_params())

        resp = self._api.get(path)
        return self._parse_session_list(resp)

    def list_by_user(
        self,
        user_id: str,
        opts: Optional[ListOptions] = None
    ) -> List[Session]:
        """
        列出指定用户的所有会话。

        Args:
            user_id: 用户 ID
            opts: 列表查询选项

        Returns:
            List[Session]: 会话列表

        Example:
            >>> sessions = manager.list_by_user("user_123")
        """
        params = {"user_id": user_id}
        if opts:
            params.update(opts.to_query_params())

        resp = self._api.get(build_url("/api/v1/sessions", params))
        return self._parse_session_list(resp)

    def list_active(self) -> List[Session]:
        """
        列出当前所有活跃会话。

        Returns:
            List[Session]: 活跃会话列表

        Example:
            >>> sessions = manager.list_active()
        """
        resp = self._api.get("/api/v1/sessions?status=active")
        return self._parse_session_list(resp)

    def update(
        self,
        session_id: str,
        metadata: Dict[str, Any]
    ) -> Session:
        """
        更新会话的元数据。

        Args:
            session_id: 会话 ID
            metadata: 新的元数据

        Returns:
            Session: 更新后的会话对象

        Raises:
            AgentOSError: 参数无效或请求失败

        Example:
            >>> session = manager.update("sess_456", {"device": "desktop"})
        """
        if not session_id:
            raise AgentOSError("会话ID不能为空", error_code=CODE_MISSING_PARAMETER)

        resp = self._api.put(
            f"/api/v1/sessions/{session_id}",
            {"metadata": metadata}
        )

        data = extract_data_map(resp)
        if not data:
            raise AgentOSError("会话更新响应格式异常", error_code=CODE_INVALID_RESPONSE)

        return self._parse_session_from_map(data)

    def refresh(self, session_id: str) -> None:
        """
        刷新会话的活跃时间，防止过期。

        Args:
            session_id: 会话 ID

        Raises:
            AgentOSError: 参数无效或请求失败

        Example:
            >>> manager.refresh("sess_456")
        """
        if not session_id:
            raise AgentOSError("会话ID不能为空", error_code=CODE_MISSING_PARAMETER)

        self._api.post(f"/api/v1/sessions/{session_id}/refresh", None)

    def is_expired(self, session_id: str) -> bool:
        """
        检查指定会话是否已过期。

        Args:
            session_id: 会话 ID

        Returns:
            bool: 是否已过期

        Raises:
            AgentOSError: 请求失败

        Example:
            >>> expired = manager.is_expired("sess_456")
            >>> print(expired)
            False
        """
        session = self.get(session_id)
        return session.status == SessionStatus.EXPIRED

    def count(self) -> int:
        """
        获取会话总数。

        Returns:
            int: 会话总数

        Example:
            >>> total = manager.count()
            >>> print(total)
            50
        """
        resp = self._api.get("/api/v1/sessions/count")
        data = extract_data_map(resp)
        if not data:
            return 0
        return get_int(data, "count")

    def count_active(self) -> int:
        """
        获取活跃会话数。

        Returns:
            int: 活跃会话数

        Example:
            >>> active = manager.count_active()
            >>> print(active)
            25
        """
        resp = self._api.get("/api/v1/sessions/count?status=active")
        data = extract_data_map(resp)
        if not data:
            return 0
        return get_int(data, "count")

    def clean_expired(self) -> int:
        """
        清理所有已过期的会话。

        Returns:
            int: 清理的会话数量

        Example:
            >>> cleaned = manager.clean_expired()
            >>> print(cleaned)
            10
        """
        resp = self._api.post("/api/v1/sessions/clean-expired", None)
        data = extract_data_map(resp)
        if not data:
            return 0
        return get_int(data, "cleaned")

    def _parse_session_from_map(self, data: Dict[str, Any]) -> Session:
        """
        从字典解析 Session 结构。

        Args:
            data: 数据字典

        Returns:
            Session: 解析的会话对象
        """
        return Session(
            id=get_string(data, "session_id"),
            user_id=get_string(data, "user_id"),
            status=SessionStatus(get_string(data, "status") or "active"),
            context=get_dict(data, "context") or {},
            metadata=get_dict(data, "metadata") or {},
            created_at=parse_time_from_map(data, "created_at"),
            last_activity=parse_time_from_map(data, "last_activity"),
        )

    def _parse_session_list(self, resp: Any) -> List[Session]:
        """
        从 APIResponse 解析 Session 列表。

        Args:
            resp: API 响应

        Returns:
            List[Session]: 会话列表
        """
        data = extract_data_map(resp)
        if not data:
            return []

        items = data.get("sessions", [])
        sessions = []
        for item in items:
            if isinstance(item, dict):
                sessions.append(self._parse_session_from_map(item))

        return sessions
