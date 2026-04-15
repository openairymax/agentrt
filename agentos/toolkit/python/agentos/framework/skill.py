# AgentOS Skill Framework
# Version: 2.0.0
# Last updated: 2026-04-11

"""
技能开发框架

提供完整的技能生命周期管理能力，包括：
- 技能注册、发现和版本管理
- 统一的执行引擎和调度
- 结果缓存和性能优化
- 依赖管理和隔离执行

设计原则:
1. 契约优先 - 明确的接口定义和能力声明
2. 沙箱隔离 - 安全的技能执行环境
3. 可观测性 - 完整的执行追踪和监控
4. 可扩展性 - 灵活的插件和扩展机制
"""

import asyncio
import inspect
import logging
import re
import time
import uuid
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from datetime import datetime
from enum import Enum
from typing import Any, Callable, Dict, List, Optional, Tuple, Type, Union

logger = logging.getLogger(__name__)


class SkillStatus(Enum):
    """技能状态枚举"""
    REGISTERED = "registered"
    LOADED = "loaded"
    ACTIVE = "active"
    INACTIVE = "inactive"
    ERROR = "error"


@dataclass
class SkillMetadata:
    """技能元数据"""
    skill_id: str
    name: str
    version: str = "1.0.0"
    description: str = ""
    author: str = ""

    # 能力信息
    input_schema: Optional[Dict[str, Any]] = None
    output_schema: Optional[Dict[str, Any]] = None

    # 执行特性
    timeout: float = 30.0
    retry_count: int = 3
    is_async: bool = True

    # 分类标签
    tags: List[str] = field(default_factory=list)
    category: str = "general"

    # 依赖信息
    dependencies: List[str] = field(default_factory=list)
    required_capabilities: List[str] = field(default_factory=list)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "skill_id": self.skill_id,
            "name": self.name,
            "version": self.version,
            "description": self.description,
            "author": self.author,
            "timeout": self.timeout,
            "retry_count": self.retry_count,
            "is_async": self.is_async,
            "tags": self.tags,
            "category": self.category,
            "dependencies": self.dependencies,
            "status": SkillStatus.REGISTERED.value
        }


@dataclass
class SkillResult:
    """技能执行结果"""
    success: bool
    output: Any = None
    error: Optional[str] = None
    error_type: Optional[str] = None
    execution_time_ms: float = 0.0
    metadata: Dict[str, Any] = field(default_factory=dict)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "success": self.success,
            "output": self.output,
            "error": self.error,
            "error_type": self.error_type,
            "execution_time_ms": self.execution_time_ms,
            "metadata": self.metadata
        }


@dataclass
class SkillExecutionResult(SkillResult):
    """扩展的技能执行结果（包含缓存和重试信息）"""
    skill_name: str = ""
    execution_id: str = ""
    from_cache: bool = False
    attempt_number: int = 1
    cached_at: Optional[datetime] = None


@dataclass
class CacheKey:
    """缓存键"""
    skill_name: str
    parameters_hash: str
    version: str = ""


@dataclass
class CacheEntry:
    """缓存条目"""
    key: CacheKey
    result: SkillResult
    created_at: datetime = field(default_factory=datetime.now)
    expires_at: Optional[datetime] = None
    hit_count: int = 0


class BaseSkill(ABC):
    """
    技能基类

    所有自定义技能都应该继承此类并实现必要的方法。
    提供标准的技能接口契约。
    """

    @property
    @abstractmethod
    def name(self) -> str:
        """技能名称"""
        pass

    @property
    @abstractmethod
    def version(self) -> str:
        """技能版本"""
        pass

    @property
    def description(self) -> str:
        """技能描述"""
        return ""

    @abstractmethod
    async def execute(self, parameters: Dict[str, Any]) -> SkillResult:
        """
        执行技能

        Args:
            parameters: 输入参数

        Returns:
            执行结果
        """
        pass

    async def initialize(self, config: Optional[Dict[str, Any]] = None) -> None:
        """初始化技能（可选实现）"""
        pass

    async def cleanup(self) -> None:
        """清理资源（可选实现）"""
        pass

    def validate_parameters(self, parameters: Dict[str, Any]) -> Tuple[bool, str]:
        """
        验证参数

        Returns:
            (是否有效, 错误消息)
        """
        return True, ""

    def get_metadata(self) -> SkillMetadata:
        """获取技能元数据"""
        return SkillMetadata(
            skill_id=f"{self.name}@{self.version}",
            name=self.name,
            version=self.version,
            description=self.description,
            is_async=True
        )


class SkillRegistry:
    """
    技能注册表

    负责技能的注册、发现、查询和管理。
    支持多版本共存和语义化版本解析。
    """

    def __init__(self):
        self._skills: Dict[str, BaseSkill] = {}
        self._metadata: Dict[str, SkillMetadata] = {}
        self._instances: Dict[str, Any] = {}
        self._categories: Dict[str, List[str]] = {}

        logger.info("SkillRegistry initialized")

    async def register(
        self,
        skill_class: Type[BaseSkill],
        version: str = "1.0.0",
        metadata_override: Optional[Dict[str, Any]] = None
    ) -> bool:
        """
        注册技能

        Args:
            skill_class: 技能类
            version: 版本号
            metadata_override: 元数据覆盖

        Returns:
            是否成功注册
        """
        try:
            # 创建实例以获取元数据
            instance = skill_class()

            metadata = instance.get_metadata()
            if metadata_override:
                for key, value in metadata_override.items():
                    if hasattr(metadata, key):
                        setattr(metadata, key, value)

            # 生成唯一标识符
            skill_key = f"{instance.name}@{version}"

            # 检查是否已存在
            if skill_key in self._skills:
                logger.warning(f"Skill already registered: {skill_key}")
                return False

            # 注册
            self._skills[skill_key] = instance
            self._metadata[skill_key] = metadata

            # 分类索引
            category = metadata.category or "general"
            if category not in self._categories:
                self._categories[category] = []
            self._categories[category].append(skill_key)

            # 初始化技能
            await instance.initialize()
            self._instances[skill_key] = instance

            logger.info(f"Registered skill: {skill_key}")
            return True

        except Exception as e:
            logger.error(f"Failed to register skill {skill_class.__name__}: {e}")
            return False

    async def unregister(self, skill_name: str, version: Optional[str] = None) -> bool:
        """
        注销技能

        Args:
            skill_name: 技能名称
            version: 版本号（可选）

        Returns:
            是否成功注销
        """
        skill_key = f"{skill_name}@{version}" if version else skill_name

        if skill_key not in self._skills:
            # 尝试模糊匹配
            matches = [k for k in self._skills.keys() if k.startswith(skill_name)]
            if len(matches) == 1:
                skill_key = matches[0]
            elif len(matches) > 1:
                logger.warning(f"Multiple skills match '{skill_name}': {matches}")
                return False
            else:
                logger.warning(f"Skill not found: {skill_name}")
                return False

        # 清理
        instance = self._instances.pop(skill_key, None)
        if instance and hasattr(instance, 'cleanup'):
            await instance.cleanup()

        del self._skills[skill_key]
        self._metadata.pop(skill_key, None)

        # 更新分类索引
        for category, skills in self._categories.items():
            if skill_key in skills:
                skills.remove(skill_key)

        logger.info(f"Unregistered skill: {skill_key}")
        return True

    def get_skill(self, skill_name: str, version: Optional[str] = None) -> Optional[BaseSkill]:
        """
        获取技能实例

        Args:
            skill_name: 技能名称
            version: 版本号（可选，默认最新）

        Returns:
            技能实例或None
        """
        if version:
            skill_key = f"{skill_name}@{version}"
            return self._instances.get(skill_key)

        # 返回最新版本
        versions = [k for k in self._skills.keys() if k.startswith(skill_name + "@")]
        if not versions:
            # 尝试精确匹配
            return self._instances.get(skill_name)

        # 排序获取最新版本
        versions.sort(reverse=True)
        latest_key = versions[0]
        return self._instances.get(latest_key)

    def get_metadata(self, skill_name: str, version: Optional[str] = None) -> Optional[SkillMetadata]:
        """获取技能元数据"""
        if version:
            return self._metadata.get(f"{skill_name}@{version}")

        versions = [k for k in self._metadata.keys() if k.startswith(skill_name + "@")]
        if versions:
            versions.sort(reverse=True)
            return self._metadata.get(versions[0])

        return self._metadata.get(skill_name)

    def list_skills(
        self,
        category: Optional[str] = None,
        tag: Optional[str] = None,
        status_filter: Optional[SkillStatus] = None
    ) -> List[SkillMetadata]:
        """
        列出技能

        Args:
            category: 分类过滤
            tag: 标签过滤
            status_filter: 状态过滤

        Returns:
            匹配的技能元数据列表
        """
        results = []

        for key, metadata in self._metadata.items():
            # 分类过滤
            if category and metadata.category != category:
                continue

            # 标签过滤
            if tag and tag not in metadata.tags:
                continue

            results.append(metadata)

        return results

    def list_categories(self) -> Dict[str, int]:
        """列出所有分类及其技能数量"""
        return {cat: len(skills) for cat, skills in self._categories.items()}

    def search_skills(self, query: str) -> List[SkillMetadata]:
        """
        搜索技能

        Args:
            query: 搜索关键词

        Returns:
            匹配的技能列表
        """
        query_lower = query.lower()
        results = []

        for metadata in self._metadata.values():
            # 在名称、描述、标签中搜索
            searchable_text = (
                metadata.name.lower() + " " +
                metadata.description.lower() + " " +
                " ".join(tag.lower() for tag in metadata.tags)
            )

            if query_lower in searchable_text:
                results.append(metadata)

        return results

    def get_stats(self) -> Dict[str, Any]:
        """获取统计信息"""
        total = len(self._skills)
        categories = len(self._categories)

        return {
            "total_skills": total,
            "total_categories": categories,
            "category_distribution": self.list_categories(),
            "registered_ids": list(self._skills.keys())
        }


class VersionManager:
    """
    版本管理器

    处理语义化版本的解析、比较和兼容性检查。
    """

    VERSION_PATTERN = re.compile(
        r'^(?P<major>\d+)\.(?P<minor>\d+)\.(?P<patch>\d+)'
        r'(?:-(?P<pre>[a-zA-Z0-9]+))?(?:\+(?P<build>.*))?$'
    )

    class SemanticVersion:
        def __init__(self, major: int, minor: int, patch: int,
                     pre_release: Optional[str] = None, build: Optional[str] = None):
            self.major = major
            self.minor = minor
            self.patch = patch
            self.pre_release = pre_release
            self.build = build

        def __str__(self):
            v = f"{self.major}.{self.minor}.{self.patch}"
            if self.pre_release:
                v += f"-{self.pre_release}"
            if self.build:
                v += f"+{self.build}"
            return v

        def __eq__(self, other):
            return (self.major, self.minor, self.patch) == (other.major, other.minor, other.patch)

        def __gt__(self, other):
            return (self.major, self.minor, self.patch) > (other.major, other.minor, other.patch)

        def __ge__(self, other):
            return (self.major, self.minor, self.patch) >= (other.major, other.minor, other.patch)

        def __lt__(self, other):
            return (self.major, self.minor, self.patch) < (other.major, other.minor, other.patch)

        def __le__(self, other):
            return (self.major, self.minor, self.patch) <= (other.major, other.minor, other.patch)

        def __ne__(self, other):
            return not self == other

    def parse_version(self, version_str: str) -> Optional[SemanticVersion]:
        """解析版本字符串"""
        match = self.VERSION_PATTERN.match(version_str.strip())
        if not match:
            return None

        return self.SemanticVersion(
            major=int(match.group('major')),
            minor=int(match.group('minor')),
            patch=int(match.group('patch')),
            pre_release=match.group('pre'),
            build=match.group('build')
        )

    def satisfies_constraint(self, version: str, constraint: str) -> bool:
        """
        检查版本是否满足约束

        支持:
        - "^1.2.3" - 兼容版本 (>=1.2.3 <2.0.0)
        - "~1.2.3" - 近似版本 (>=1.2.3 <1.3.0)
        - ">=1.0.0,<2.0.0" - 范围约束
        - "1.2.3" - 精确匹配
        """
        parsed_ver = self.parse_version(version)
        if not parsed_ver:
            return False

        constraint = constraint.strip()

        # 处理范围约束
        if ',' in constraint:
            parts = [p.strip() for p in constraint.split(',')]
            return all(self.satisfies_constraint(version, p) for p in parts)

        # 兼容版本 ^
        if constraint.startswith('^'):
            base = self.parse_version(constraint[1:])
            if base:
                return (parsed_ver >= base and
                       parsed_ver.major == base.major)

        # 近似版本 ~
        if constraint.startswith('~'):
            base = self.parse_version(constraint[1:])
            if base:
                return (parsed_ver >= base and
                       parsed_ver.major == base.major and
                       parsed_ver.minor == base.minor)

        # 范围比较
        for op in ['>=', '<=', '>', '<', '=']:
            if constraint.startswith(op):
                target = self.parse_version(constraint[len(op):])
                if target:
                    if op == '>=': return parsed_ver >= target
                    if op == '<=': return parsed_ver <= target
                    if op == '>': return parsed_ver > target
                    if op == '<': return parsed_ver < target
                    if op == '=': return parsed_ver == target

        # 精确匹配
        exact = self.parse_version(constraint)
        if exact:
            return parsed_ver == exact

        return False


class SkillExecutionEngine:
    """
    技能执行引擎

    负责技能的调度、执行、超时控制和错误处理。
    支持同步/异步执行、重试机制和并发控制。
    """

    def __init__(
        self,
        registry: SkillRegistry,
        default_timeout: float = 30.0,
        max_concurrent: int = 10
    ):
        self._registry = registry
        self._default_timeout = default_timeout
        self._max_concurrent = max_concurrent
        self._semaphore: Optional[asyncio.Semaphore] = None
        self._execution_stats: Dict[str, int] = {}

        logger.info("SkillExecutionEngine initialized")

    async def execute(
        self,
        skill_name: str,
        parameters: Optional[Dict[str, Any]] = None,
        version: Optional[str] = None,
        timeout: Optional[float] = None,
        context: Optional[Dict[str, Any]] = None
    ) -> SkillExecutionResult:
        """
        执行技能

        Args:
            skill_name: 技能名称
            parameters: 输入参数
            version: 版本号
            timeout: 超时时间（秒）
            context: 执行上下文

        Returns:
            执行结果
        """
        start_time = time.time()
        execution_id = str(uuid.uuid4())
        parameters = parameters or {}
        timeout = timeout or self._default_timeout

        result = SkillExecutionResult(
            success=False,
            skill_name=skill_name,
            execution_id=execution_id
        )

        try:
            # 获取技能实例
            skill = self._registry.get_skill(skill_name, version)
            if not skill:
                raise ValueError(f"Skill not found: {skill_name}")

            # 验证参数
            is_valid, error_msg = skill.validate_parameters(parameters)
            if not is_valid:
                raise ValueError(f"Invalid parameters: {error_msg}")

            # 并发控制
            if self._semaphore:
                await self._semaphore.acquire()

            try:
                # 执行技能（带超时）
                actual_timeout = timeout or skill.get_metadata().timeout

                if asyncio.iscoroutinefunction(skill.execute):
                    output = await asyncio.wait_for(
                        skill.execute(parameters),
                        timeout=actual_timeout
                    )
                else:
                    output = await asyncio.to_thread(
                        skill.execute, parameters
                    )

                # 处理结果
                if isinstance(output, SkillResult):
                    result.success = output.success
                    result.output = output.output
                    result.error = output.error
                    result.metadata = output.metadata
                else:
                    result.success = True
                    result.output = output

            finally:
                if self._semaphore:
                    self._semaphore.release()

        except asyncio.TimeoutError:
            result.error = f"Execution timed out after {timeout}s"
            result.error_type = "TimeoutError"
            logger.error(f"Skill execution timeout: {skill_name}")

        except Exception as e:
            result.error = str(e)
            result.error_type = type(e).__name__
            logger.error(f"Skill execution failed: {skill_name}: {e}")

        # 计算执行时间
        result.execution_time_ms = (time.time() - start_time) * 1000

        # 更新统计
        self._execution_stats[skill_name] = \
            self._execution_stats.get(skill_name, 0) + 1

        return result

    async def execute_batch(
        self,
        tasks: List[Dict[str, Any]],
        mode: str = "sequential"
    ) -> List[SkillExecutionResult]:
        """
        批量执行技能

        Args:
            tasks: 任务列表 [{"skill_name": ..., "parameters": ...}, ...]
            mode: 执行模式 (sequential, parallel, concurrent)

        Returns:
            结果列表
        """
        if mode == "sequential":
            results = []
            for task in tasks:
                result = await self.execute(**task)
                results.append(result)
            return results

        elif mode == "parallel":
            tasks_coro = [self.execute(**task) for task in tasks]
            return await asyncio.gather(*tasks_coro)

        elif mode == "concurrent":
            if not self._semaphore:
                self._semaphore = asyncio.Semaphore(self._max_concurrent)

            tasks_coro = [self.execute(**task) for task in tasks]
            return await asyncio.gather(*tasks_coro)

        else:
            raise ValueError(f"Unknown execution mode: {mode}")

    def get_execution_stats(self) -> Dict[str, int]:
        """获取执行统计"""
        return dict(self._execution_stats)


class SkillResultCache:
    """
    技能结果缓存

    缓存技能执行结果以提高性能，支持TTL过期策略。
    """

    def __init__(
        self,
        max_size: int = 1000,
        default_ttl: float = 300.0
    ):
        self._cache: Dict[str, CacheEntry] = {}
        self._max_size = max_size
        self._default_ttl = default_ttl
        self._hits = 0
        self._misses = 0

        logger.info("SkillResultCache initialized")

    def _make_cache_key(self, skill_name: str, parameters: Dict[str, Any], version: str = "") -> str:
        """生成缓存键"""
        import hashlib
        params_str = str(sorted(parameters.items()))
        params_hash = hashlib.md5(params_str.encode()).hexdigest()[:12]
        return f"{skill_name}:{params_hash}"

    async def get(
        self,
        skill_name: str,
        parameters: Dict[str, Any],
        version: str = ""
    ) -> Optional[SkillExecutionResult]:
        """
        获取缓存的执行结果

        Returns:
        缓存的结果，如果不存在或已过期则返回None
        """
        cache_key = self._make_cache_key(skill_name, parameters, version)

        entry = self._cache.get(cache_key)
        if not entry:
            self._misses += 1
            return None

        # 检查是否过期
        if entry.expires_at and datetime.now() > entry.expires_at:
            del self._cache[cache_key]
            self._misses += 1
            return None

        # 命中
        entry.hit_count += 1
        self._hits += 1

        return SkillExecutionResult(
            **entry.result.to_dict(),
            skill_name=skill_name,
            from_cache=True,
            cached_at=entry.created_at
        )

    async def set(
        self,
        skill_name: str,
        parameters: Dict[str, Any],
        result: SkillResult,
        ttl: Optional[float] = None,
        version: str = ""
    ) -> None:
        """
        缓存执行结果
        """
        cache_key = self._make_cache_key(skill_name, parameters, version)

        # 如果已满，移除最旧的条目
        if len(self._cache) >= self._max_size and cache_key not in self._cache:
            oldest_key = min(self._cache.keys(),
                            key=lambda k: self._cache[k].created_at)
            del self._cache[oldest_key]

        # 创建缓存条目
        expires_at = None
        if ttl is not None:
            expires_at = datetime.now() + timedelta(seconds=ttl)
        elif self._default_ttl > 0:
            expires_at = datetime.now() + timedelta(seconds=self._default_ttl)

        self._cache[cache_key] = CacheEntry(
            key=CacheKey(skill_name=skill_name, parameters_hash="", version=version),
            result=result,
            expires_at=expires_at
        )

    def invalidate(self, skill_name: Optional[str] = None) -> int:
        """
        使缓存失效

        Args:
            skill_name: 技能名称（如果为None则清空所有）

        Returns:
            失效的条目数
        """
        if skill_name is None:
            count = len(self._cache)
            self._cache.clear()
            return count

        keys_to_remove = [
            k for k in self._cache.keys()
            if k.startswith(skill_name + ":")
        ]

        for key in keys_to_remove:
            del self._cache[key]

        return len(keys_to_remove)

    def get_stats(self) -> Dict[str, Any]:
        """获取缓存统计"""
        total_requests = self._hits + self._misses
        hit_rate = (self._hits / total_requests * 100) if total_requests > 0 else 0

        return {
            "size": len(self._cache),
            "max_size": self._max_size,
            "hits": self._hits,
            "misses": self._misses,
            "hit_rate": f"{hit_rate:.1f}%"
        }

    def clear(self) -> None:
        """清空缓存"""
        self._cache.clear()


from datetime import timedelta


__all__ = [
    # 核心类
    "BaseSkill",
    "SkillRegistry",
    "VersionManager",
    "SkillExecutionEngine",
    "SkillResultCache",

    # 数据类
    "SkillStatus",
    "SkillMetadata",
    "SkillResult",
    "SkillExecutionResult",
    "CacheKey",
    "CacheEntry",
]
