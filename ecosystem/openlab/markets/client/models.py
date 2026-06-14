# Copyright (c) 2026 SPHARX. All Rights Reserved.
"""
AgentRT OpenLab: Market Models

Python dataclass mappings for market_d C structures defined in
agentos/daemon/market_d/include/market_service.h.

Each dataclass directly corresponds to a C struct, ensuring contract
consistency between the C daemon and Python client.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import IntEnum
from typing import Optional


class AgentType(IntEnum):
    """Mirrors agent_type_t enum in market_service.h."""
    ASSISTANT = 0
    EXPERT = 1
    SPECIALIZED = 2
    CUSTOM = 3
    COUNT = 4


class AgentStatus(IntEnum):
    """Mirrors agent_status_t enum in market_service.h."""
    AVAILABLE = 0
    INSTALLING = 1
    ERROR = 2
    DISABLED = 3
    COUNT = 4


class SkillType(IntEnum):
    """Mirrors skill_type_t enum in market_service.h."""
    TOOL = 0
    KNOWLEDGE = 1
    INTEGRATION = 2
    CUSTOM = 3
    COUNT = 4


@dataclass
class AgentInfo:
    """Mirrors agent_info_t struct in market_service.h.

    Fields are in the same order and use the same names as the C struct
    for automatic serialization/deserialization consistency.
    """
    agent_id: str = ""
    name: str = ""
    version: str = ""
    description: str = ""
    type: AgentType = AgentType.ASSISTANT
    status: AgentStatus = AgentStatus.AVAILABLE
    author: str = ""
    repository: str = ""
    dependencies: str = ""
    rating: float = 0.0
    download_count: int = 0
    last_updated: int = 0

    @classmethod
    def from_json(cls, data: dict) -> "AgentInfo":
        """Create AgentInfo from JSON dict returned by market_d."""
        return cls(
            agent_id=data.get("agent_id", ""),
            name=data.get("name", ""),
            version=data.get("version", ""),
            description=data.get("description", ""),
            type=AgentType(data.get("type", AgentType.ASSISTANT)),
            status=AgentStatus(data.get("status", AgentStatus.AVAILABLE)),
            author=data.get("author", ""),
            repository=data.get("repository", ""),
            dependencies=data.get("dependencies", ""),
            rating=float(data.get("rating", 0.0)),
            download_count=int(data.get("download_count", 0)),
            last_updated=int(data.get("last_updated", 0)),
        )

    def to_json(self) -> dict:
        """Convert to JSON-serializable dict matching market_d expectations."""
        return {
            "agent_id": self.agent_id,
            "name": self.name,
            "version": self.version,
            "description": self.description,
            "type": int(self.type),
            "status": int(self.status),
            "author": self.author,
            "repository": self.repository,
            "dependencies": self.dependencies,
            "rating": self.rating,
            "download_count": self.download_count,
            "last_updated": self.last_updated,
        }


@dataclass
class SkillInfo:
    """Mirrors skill_info_t struct in market_service.h."""
    skill_id: str = ""
    name: str = ""
    version: str = ""
    description: str = ""
    type: SkillType = SkillType.TOOL
    author: str = ""
    repository: str = ""
    dependencies: str = ""
    rating: float = 0.0
    download_count: int = 0
    last_updated: int = 0

    @classmethod
    def from_json(cls, data: dict) -> "SkillInfo":
        return cls(
            skill_id=data.get("skill_id", ""),
            name=data.get("name", ""),
            version=data.get("version", ""),
            description=data.get("description", ""),
            type=SkillType(data.get("type", SkillType.TOOL)),
            author=data.get("author", ""),
            repository=data.get("repository", ""),
            dependencies=data.get("dependencies", ""),
            rating=float(data.get("rating", 0.0)),
            download_count=int(data.get("download_count", 0)),
            last_updated=int(data.get("last_updated", 0)),
        )

    def to_json(self) -> dict:
        return {
            "skill_id": self.skill_id,
            "name": self.name,
            "version": self.version,
            "description": self.description,
            "type": int(self.type),
            "author": self.author,
            "repository": self.repository,
            "dependencies": self.dependencies,
            "rating": self.rating,
            "download_count": self.download_count,
            "last_updated": self.last_updated,
        }


@dataclass
class InstallRequest:
    """Mirrors install_request_t struct in market_service.h."""
    id: str = ""
    version: str = ""
    force_update: bool = False
    install_path: str = ""

    def to_json(self) -> dict:
        return {
            "agent_id": self.id,
            "version": self.version or "latest",
            "force_update": self.force_update,
            "install_path": self.install_path,
        }


@dataclass
class InstallResult:
    """Mirrors install_result_t struct in market_service.h."""
    success: bool = False
    message: str = ""
    installed_version: str = ""
    install_path: str = ""
    error_code: int = 0

    @classmethod
    def from_json(cls, data: dict) -> "InstallResult":
        return cls(
            success=data.get("status") == "installed",
            message=data.get("message", ""),
            installed_version=data.get("installed_version", ""),
            install_path=data.get("install_path", ""),
            error_code=int(data.get("error_code", 0)),
        )


@dataclass
class SearchParams:
    """Mirrors search_params_t struct in market_service.h."""
    query: str = ""
    agent_type: Optional[AgentType] = None
    skill_type: Optional[SkillType] = None
    only_installed: bool = False
    sort_by_rating: bool = False
    sort_by_download: bool = False
    limit: int = 20
    offset: int = 0

    def to_json(self) -> dict:
        result: dict = {
            "keyword": self.query,
            "limit": self.limit,
            "offset": self.offset,
        }
        if self.agent_type is not None:
            result["agent_type"] = int(self.agent_type)
        if self.skill_type is not None:
            result["skill_type"] = int(self.skill_type)
        if self.only_installed:
            result["only_installed"] = True
        if self.sort_by_rating:
            result["sort_by_rating"] = True
        if self.sort_by_download:
            result["sort_by_download"] = True
        return result