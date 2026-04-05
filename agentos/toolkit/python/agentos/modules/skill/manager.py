# AgentOS Python SDK - Skill Manager Implementation
# Version: 3.0.0
# Last updated: 2026-03-24

"""
Skill manager implementation for managing skill lifecycle.

Provides comprehensive skill management including registration, loading,
execution, unloading, and validation operations.

Corresponds to Go SDK: modules/skill/manager.go
"""

from dataclasses import dataclass
from datetime import datetime
from typing import Any, Dict, List, Optional, Tuple

from ...client.client import APIClient
from ...exceptions import AgentOSError, CODE_MISSING_PARAMETER, CODE_INVALID_RESPONSE
from ...types import Skill, SkillResult, SkillInfo, SkillStatus, ListOptions
from ...utils import (
    get_string, get_int, get_bool, get_dict, get_list,
    extract_data_map, build_url, parse_time_from_map, extract_int_stats
)


@dataclass
class SkillExecuteRequest:
    """
    批量执行时的单条请求。

    用于 BatchExecute 操作。
    """
    skill_id: str
    parameters: Optional[Dict[str, Any]] = None


class SkillManager:
    """
    技能管理器，管理技能完整生命周期。

    提供技能的注册、加载、执行、卸载及验证功能。

    对应 Go SDK: modules/skill/manager.go SkillManager

    Example:
        >>> from agentos.client import Client
        >>> from agentos.modules import SkillManager
        >>> client = Client()
        >>> skill_mgr = SkillManager(client)
        >>> skill = skill_mgr.register("my_skill", "description", {})
        >>> result = skill_mgr.execute(skill.id, {"input": "data"})
    """

    def __init__(self, api: APIClient):
        """
        初始化技能管理器。

        Args:
            api: APIClient 实例，用于 HTTP 通信
        """
        self._api = api

    def load(self, skill_id: str) -> Skill:
        """
        加载指定技能到运行时。

        Args:
            skill_id: 技能 ID

        Returns:
            Skill: 加载的技能对象

        Raises:
            AgentOSError: 参数无效或请求失败

        Example:
            >>> skill = manager.load("skill_123")
            >>> print(skill.status)
            <SkillStatus.ACTIVE: 'active'>
        """
        if not skill_id:
            raise AgentOSError("技能ID不能为空", error_code=CODE_MISSING_PARAMETER)

        resp = self._api.post(f"/api/v1/skills/{skill_id}/load", None)

        data = extract_data_map(resp)
        if not data:
            raise AgentOSError("技能加载响应格式异常", error_code=CODE_INVALID_RESPONSE)

        return Skill(
            id=skill_id,
            name=get_string(data, "name"),
            version=get_string(data, "version"),
            status=SkillStatus.ACTIVE,
            parameters=get_dict(data, "parameters") or {},
            metadata=get_dict(data, "metadata") or {},
        )

    def get(self, skill_id: str) -> Skill:
        """
        获取指定技能的详细信息。

        Args:
            skill_id: 技能 ID

        Returns:
            Skill: 技能对象

        Raises:
            AgentOSError: 参数无效或请求失败

        Example:
            >>> skill = manager.get("skill_123")
            >>> print(skill.name)
            'my_skill'
        """
        if not skill_id:
            raise AgentOSError("技能ID不能为空", error_code=CODE_MISSING_PARAMETER)

        resp = self._api.get(f"/api/v1/skills/{skill_id}")

        data = extract_data_map(resp)
        if not data:
            raise AgentOSError("技能详情响应格式异常", error_code=CODE_INVALID_RESPONSE)

        return self._parse_skill_from_map(data, skill_id)

    def execute(
        self,
        skill_id: str,
        parameters: Optional[Dict[str, Any]]
    ) -> SkillResult:
        """
        执行指定技能。

        Args:
            skill_id: 技能 ID
            parameters: 执行参数

        Returns:
            SkillResult: 执行结果

        Raises:
            AgentOSError: 参数无效或请求失败

        Example:
            >>> result = manager.execute("skill_123", {"input": "data"})
            >>> print(result.success)
            True
        """
        if not skill_id:
            raise AgentOSError("技能ID不能为空", error_code=CODE_MISSING_PARAMETER)

        resp = self._api.post(
            f"/api/v1/skills/{skill_id}/execute",
            {"parameters": parameters}
        )

        return self._parse_skill_result(resp)

    def execute_with_context(
        self,
        skill_id: str,
        parameters: Optional[Dict[str, Any]],
        session_id: str
    ) -> SkillResult:
        """
        在指定会话上下文中执行技能。

        Args:
            skill_id: 技能 ID
            parameters: 执行参数
            session_id: 会话 ID

        Returns:
            SkillResult: 执行结果

        Raises:
            AgentOSError: 参数无效或请求失败

        Example:
            >>> result = manager.execute_with_context(
            ...     "skill_123",
            ...     {"input": "data"},
            ...     "sess_456"
            ... )
        """
        if not skill_id:
            raise AgentOSError("技能ID不能为空", error_code=CODE_MISSING_PARAMETER)

        resp = self._api.post(
            f"/api/v1/skills/{skill_id}/execute",
            {"parameters": parameters, "session_id": session_id}
        )

        return self._parse_skill_result(resp)

    def unload(self, skill_id: str) -> None:
        """
        卸载指定技能，释放运行时资源。

        Args:
            skill_id: 技能 ID

        Raises:
            AgentOSError: 参数无效或请求失败

        Example:
            >>> manager.unload("skill_123")
        """
        if not skill_id:
            raise AgentOSError("技能ID不能为空", error_code=CODE_MISSING_PARAMETER)

        self._api.post(f"/api/v1/skills/{skill_id}/unload", None)

    def list(self, opts: Optional[ListOptions] = None) -> List[Skill]:
        """
        列出技能，支持分页和过滤。

        Args:
            opts: 列表查询选项

        Returns:
            List[Skill]: 技能列表

        Example:
            >>> from agentos.types import ListOptions, PaginationOptions
            >>> opts = ListOptions(pagination=PaginationOptions(page=1, page_size=10))
            >>> skills = manager.list(opts)
        """
        path = "/api/v1/skills"
        if opts:
            path = build_url(path, opts.to_query_params())

        resp = self._api.get(path)
        return self._parse_skill_list(resp)

    def list_loaded(self) -> List[Skill]:
        """
        列出当前已加载的技能。

        Returns:
            List[Skill]: 已加载的技能列表

        Example:
            >>> skills = manager.list_loaded()
        """
        resp = self._api.get("/api/v1/skills?status=loaded")
        return self._parse_skill_list(resp)

    def register(
        self,
        name: str,
        description: str,
        parameters: Optional[Dict[str, Any]]
    ) -> Skill:
        """
        注册新的技能。

        Args:
            name: 技能名称
            description: 技能描述
            parameters: 技能参数定义

        Returns:
            Skill: 创建的技能对象

        Raises:
            AgentOSError: 参数无效或请求失败

        Example:
            >>> skill = manager.register(
            ...     "my_skill",
            ...     "A sample skill",
            ...     {"input": {"type": "string"}}
            ... )
        """
        if not name:
            raise AgentOSError("技能名称不能为空", error_code=CODE_MISSING_PARAMETER)

        resp = self._api.post("/api/v1/skills", {
            "name": name,
            "description": description,
            "parameters": parameters or {},
        })

        data = extract_data_map(resp)
        if not data:
            raise AgentOSError("技能注册响应格式异常", error_code=CODE_INVALID_RESPONSE)

        return Skill(
            id=get_string(data, "skill_id"),
            name=name,
            description=description,
            parameters=parameters or {},
            status=SkillStatus.ACTIVE,
            metadata=get_dict(data, "metadata") or {},
        )

    def update(
        self,
        skill_id: str,
        description: str,
        parameters: Optional[Dict[str, Any]]
    ) -> Skill:
        """
        更新指定技能的描述和参数。

        Args:
            skill_id: 技能 ID
            description: 新描述
            parameters: 新参数定义

        Returns:
            Skill: 更新后的技能对象

        Raises:
            AgentOSError: 参数无效或请求失败

        Example:
            >>> skill = manager.update(
            ...     "skill_123",
            ...     "Updated description",
            ...     {"input": {"type": "object"}}
            ... )
        """
        if not skill_id:
            raise AgentOSError("技能ID不能为空", error_code=CODE_MISSING_PARAMETER)

        resp = self._api.put(
            f"/api/v1/skills/{skill_id}",
            {"description": description, "parameters": parameters or {}}
        )

        data = extract_data_map(resp)
        if not data:
            raise AgentOSError("技能更新响应格式异常", error_code=CODE_INVALID_RESPONSE)

        return self._parse_skill_from_map(data, skill_id)

    def delete(self, skill_id: str) -> None:
        """
        删除指定技能。

        Args:
            skill_id: 技能 ID

        Raises:
            AgentOSError: 参数无效或请求失败

        Example:
            >>> manager.delete("skill_123")
        """
        if not skill_id:
            raise AgentOSError("技能ID不能为空", error_code=CODE_MISSING_PARAMETER)

        self._api.delete(f"/api/v1/skills/{skill_id}")

    def get_info(self, skill_id: str) -> SkillInfo:
        """
        获取指定技能的只读元信息。

        Args:
            skill_id: 技能 ID

        Returns:
            SkillInfo: 技能信息

        Raises:
            AgentOSError: 参数无效或请求失败

        Example:
            >>> info = manager.get_info("skill_123")
            >>> print(info.name)
            'my_skill'
        """
        if not skill_id:
            raise AgentOSError("技能ID不能为空", error_code=CODE_MISSING_PARAMETER)

        resp = self._api.get(f"/api/v1/skills/{skill_id}/info")

        data = extract_data_map(resp)
        if not data:
            raise AgentOSError("技能信息响应格式异常", error_code=CODE_INVALID_RESPONSE)

        return SkillInfo(
            name=get_string(data, "skill_name"),
            description=get_string(data, "description"),
            version=get_string(data, "version"),
            parameters=get_dict(data, "parameters") or {},
        )

    def validate(
        self,
        skill_id: str,
        parameters: Optional[Dict[str, Any]]
    ) -> Tuple[bool, List[str]]:
        """
        验证技能参数是否合法。

        Args:
            skill_id: 技能 ID
            parameters: 待验证的参数

        Returns:
            Tuple[bool, List[str]]: (是否有效, 错误列表)

        Raises:
            AgentOSError: 参数无效或请求失败

        Example:
            >>> valid, errors = manager.validate("skill_123", {"input": "test"})
            >>> print(valid)
            True
        """
        if not skill_id:
            raise AgentOSError("技能ID不能为空", error_code=CODE_MISSING_PARAMETER)

        resp = self._api.post(
            f"/api/v1/skills/{skill_id}/validate",
            {"parameters": parameters}
        )

        data = extract_data_map(resp)
        if not data:
            raise AgentOSError("技能验证响应格式异常", error_code=CODE_INVALID_RESPONSE)

        valid = get_bool(data, "valid")
        errors = []
        error_list = get_list(data, "errors")
        for err in error_list:
            if isinstance(err, str):
                errors.append(err)

        return valid, errors

    def count(self) -> int:
        """
        获取技能总数。

        Returns:
            int: 技能总数

        Example:
            >>> total = manager.count()
            >>> print(total)
            20
        """
        resp = self._api.get("/api/v1/skills/count")
        data = extract_data_map(resp)
        if not data:
            return 0
        return get_int(data, "count")

    def count_loaded(self) -> int:
        """
        获取已加载技能数。

        Returns:
            int: 已加载技能数

        Example:
            >>> loaded = manager.count_loaded()
            >>> print(loaded)
            15
        """
        resp = self._api.get("/api/v1/skills/count?status=loaded")
        data = extract_data_map(resp)
        if not data:
            return 0
        return get_int(data, "count")

    def search(self, query: str, top_k: int = 10) -> List[Skill]:
        """
        搜索技能。

        Args:
            query: 搜索查询
            top_k: 返回结果数量

        Returns:
            List[Skill]: 技能列表

        Raises:
            AgentOSError: 参数无效或请求失败

        Example:
            >>> skills = manager.search("data processing", top_k=5)
        """
        if not query:
            raise AgentOSError("搜索查询不能为空", error_code=CODE_MISSING_PARAMETER)

        if top_k <= 0:
            top_k = 10

        path = build_url("/api/v1/skills/search", {
            "query": query,
            "top_k": str(top_k),
        })

        resp = self._api.get(path)
        return self._parse_skill_list(resp)

    def batch_execute(
        self,
        requests: List[SkillExecuteRequest]
    ) -> List[SkillResult]:
        """
        批量执行多个技能。

        Args:
            requests: 执行请求列表

        Returns:
            List[SkillResult]: 执行结果列表

        Raises:
            AgentOSError: 请求失败

        Example:
            >>> requests = [
            ...     SkillExecuteRequest("skill_1", {"input": "a"}),
            ...     SkillExecuteRequest("skill_2", {"input": "b"}),
            ... ]
            >>> results = manager.batch_execute(requests)
        """
        results = []
        for req in requests:
            result = self.execute(req.skill_id, req.parameters)
            results.append(result)
        return results

    def get_stats(self, skill_id: str) -> Dict[str, int]:
        """
        获取指定技能的执行统计数据。

        Args:
            skill_id: 技能 ID

        Returns:
            Dict[str, int]: 统计数据字典

        Raises:
            AgentOSError: 参数无效或请求失败

        Example:
            >>> stats = manager.get_stats("skill_123")
            >>> print(stats)
            {'executions': 100, 'successes': 95, 'failures': 5}
        """
        if not skill_id:
            raise AgentOSError("技能ID不能为空", error_code=CODE_MISSING_PARAMETER)

        resp = self._api.get(f"/api/v1/skills/{skill_id}/stats")
        data = extract_data_map(resp)
        if not data:
            return {}
        return extract_int_stats(data)

    def _parse_skill_from_map(
        self,
        data: Dict[str, Any],
        fallback_id: str = ""
    ) -> Skill:
        """
        从字典解析 Skill 结构。

        Args:
            data: 数据字典
            fallback_id: 默认 ID

        Returns:
            Skill: 解析的技能对象
        """
        skill_id = get_string(data, "skill_id") or fallback_id
        return Skill(
            id=skill_id,
            name=get_string(data, "name"),
            version=get_string(data, "version"),
            description=get_string(data, "description"),
            status=SkillStatus(get_string(data, "status") or "active"),
            parameters=get_dict(data, "parameters") or {},
            metadata=get_dict(data, "metadata") or {},
            created_at=parse_time_from_map(data, "created_at"),
        )

    def _parse_skill_list(self, resp: Any) -> List[Skill]:
        """
        从 APIResponse 解析 Skill 列表。

        Args:
            resp: API 响应

        Returns:
            List[Skill]: 技能列表
        """
        data = extract_data_map(resp)
        if not data:
            return []

        items = get_list(data, "skills")
        skills = []
        for item in items:
            if isinstance(item, dict):
                skills.append(self._parse_skill_from_map(item))

        return skills

    def _parse_skill_result(self, resp: Any) -> SkillResult:
        """
        从 APIResponse 解析 SkillResult。

        Args:
            resp: API 响应

        Returns:
            SkillResult: 执行结果
        """
        data = extract_data_map(resp)
        if not data:
            raise AgentOSError("技能执行响应格式异常", error_code=CODE_INVALID_RESPONSE)

        return SkillResult(
            success=get_bool(data, "success"),
            output=data.get("output"),
            error=get_string(data, "error"),
        )
