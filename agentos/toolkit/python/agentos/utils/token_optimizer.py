# AgentOS Python SDK - Token 优化器
# Version: 3.0.0
# Last updated: 2026-04-05
#
# Token 使用效率优化模块
# 包含：LRU 缓存、Token 预算管理、上下文压缩
# 遵循 ARCHITECTURAL_PRINCIPLES.md A-1（简约至上）和 E-3（资源确定性）

import time
import hashlib
from collections import OrderedDict
from typing import Any, Dict, List, Optional, Tuple
from dataclasses import dataclass, field
from datetime import datetime, timedelta
from enum import IntEnum


class TokenModelType(IntEnum):
    """Token 计算模型类型（与 C 语言 token_standard.h 保持一致）"""
    GENERIC = 0      # 通用模型（默认）
    GPT4 = 1         # GPT-4 系列模型
    GPT35 = 2        # GPT-3.5 系列模型
    CLAUDE = 3       # Claude 系列模型
    LLAMA = 4        # LLaMA 系列模型
    CUSTOM = 5       # 自定义模型


class TokenPrecision(IntEnum):
    """Token 计算精度级别"""
    LOW = 0          # 低精度（快速估算）
    MEDIUM = 1       # 中等精度
    HIGH = 2         # 高精度（准确但较慢）


def standard_token_count(
    text: str,
    model_type: TokenModelType = TokenModelType.GENERIC,
    precision: TokenPrecision = TokenPrecision.LOW
) -> int:
    """
    标准化 Token 计算函数（与 C 语言实现保持一致）
    
    Args:
        text: 输入文本
        model_type: 模型类型
        precision: 计算精度
    
    Returns:
        Token 数量
    """
    if not text:
        return 0
    
    # 分析文本特征
    cjk_chars = sum(1 for c in text if '\u4e00' <= c <= '\u9fff')
    alpha_chars = sum(1 for c in text if c.isalpha())
    total_chars = len(text)
    
    # 计算 CJK 和字母字符比例
    cjk_ratio = cjk_chars / total_chars if total_chars > 0 else 0
    alpha_ratio = alpha_chars / total_chars if total_chars > 0 else 0
    
    # 根据精度设置阈值
    if precision == TokenPrecision.LOW:
        cjk_threshold = 0.3
        alpha_threshold = 0.5
    elif precision == TokenPrecision.MEDIUM:
        cjk_threshold = 0.2
        alpha_threshold = 0.4
    else:  # HIGH precision
        cjk_threshold = 0.1
        alpha_threshold = 0.3
    
    # 根据模型类型和精度选择计算策略
    token_count = 0
    
    if precision == TokenPrecision.HIGH:
        # 高精度模式：根据模型类型使用不同算法
        if model_type in (TokenModelType.GPT4, TokenModelType.GPT35):
            # GPT 系列：中文字符 1.5 字符/Token，英文 4 字符/Token
            token_count = int(cjk_chars / 1.5 + (total_chars - cjk_chars) / 4.0)
        elif model_type == TokenModelType.CLAUDE:
            # Claude 系列：统一 3.5 字符/Token
            token_count = int(total_chars / 3.5)
        elif model_type == TokenModelType.LLAMA:
            # LLaMA 系列：中文字符 2 字符/Token，英文 4 字符/Token
            token_count = int(cjk_chars / 2.0 + (total_chars - cjk_chars) / 4.0)
        else:
            # 通用模型：自适应算法
            if cjk_ratio > cjk_threshold:
                # 主要为中文文本
                token_count = int(cjk_chars / 1.5 + (total_chars - cjk_chars) / 4.0)
            elif alpha_ratio > alpha_threshold:
                # 主要为英文文本
                token_count = int(total_chars / 4.0)
            else:
                # 混合文本
                token_count = int(total_chars / 3.0)
    else:
        # 估算模式（低/中精度）：快速估算
        if cjk_ratio > cjk_threshold:
            # 主要为中文文本：1.5 字符/Token
            token_count = int(total_chars / 1.5)
        elif alpha_ratio > alpha_threshold:
            # 主要为英文文本：4 字符/Token
            token_count = int(total_chars / 4.0)
        else:
            # 混合文本：3 字符/Token
            token_count = int(total_chars / 3.0)
    
    # 确保至少返回 1 个 Token
    if token_count == 0 and total_chars > 0:
        token_count = 1
    
    return token_count


@dataclass
class CacheEntry:
    """
    缓存条目
    
    Attributes:
        key: 缓存键
        value: 缓存值
        created_at: 创建时间
        last_accessed: 最后访问时间
        access_count: 访问次数
        size_bytes: 大小（字节）
        token_count: Token 数量
    """
    key: str
    value: Any
    created_at: float
    last_accessed: float
    access_count: int = 0
    size_bytes: int = 0
    token_count: int = 0


class LRUCache:
    """
    LRU（Least Recently Used）缓存实现
    
    用于减少重复的 LLM 调用，降低 Token 消耗
    
    特性:
        - 基于访问时间的淘汰策略
        - TTL（Time To Live）支持
        - 大小限制
        - 命中率统计
    
    Example:
        >>> cache = LRUCache(max_size_mb=100, ttl_seconds=3600)
        >>> 
        >>> # 写入缓存
        >>> cache.set("prompt-123", {"response": "answer"}, token_count=500)
        >>> 
        >>> # 读取缓存
        >>> result = cache.get("prompt-123")
        >>> if result:
        ...     print(f"缓存命中！节省 500 tokens")
        >>> 
        >>> # 查看统计
        >>> stats = cache.get_stats()
        >>> print(f"命中率：{stats['hit_rate_percent']}%")
    """
    
    def __init__(
        self,
        max_size_mb: int = 100,
        ttl_seconds: int = 3600,
        max_entries: int = 10000
    ):
        """
        初始化 LRU 缓存
        
        Args:
            max_size_mb: 最大缓存大小（MB）
            ttl_seconds: 默认 TTL（秒）
            max_entries: 最大条目数
        
        Example:
            >>> cache = LRUCache(
            ...     max_size_mb=50,
            ...     ttl_seconds=1800,
            ...     max_entries=5000
            ... )
        """
        self.max_size_bytes = max_size_mb * 1024 * 1024
        self.ttl = ttl_seconds
        self.max_entries = max_entries
        
        # OrderedDict 保证插入顺序，用于实现 LRU
        self.cache: OrderedDict[str, CacheEntry] = OrderedDict()
        self.current_size = 0
        
        # 统计信息
        self.hit_count = 0
        self.miss_count = 0
        self.eviction_count = 0
    
    def get(self, key: str) -> Optional[Any]:
        """
        获取缓存
        
        Args:
            key: 缓存键
        
        Returns:
            Any/None: 缓存值或 None
        
        Example:
            >>> cache = LRUCache()
            >>> cache.set("key1", "value1")
            >>> value = cache.get("key1")  # 返回 "value1"
        """
        if key not in self.cache:
            self.miss_count += 1
            return None
        
        entry = self.cache[key]
        
        # 检查 TTL
        if time.time() - entry.created_at > self.ttl:
            del self.cache[key]
            self.current_size -= entry.size_bytes
            self.miss_count += 1
            return None
        
        # 更新访问信息（LRU）
        entry.last_accessed = time.time()
        entry.access_count += 1
        self.cache.move_to_end(key)
        
        self.hit_count += 1
        return entry.value
    
    def set(
        self,
        key: str,
        value: Any,
        size_bytes: int = 0,
        token_count: int = 0,
        ttl: Optional[int] = None
    ):
        """
        写入缓存
        
        Args:
            key: 缓存键
            value: 缓存值
            size_bytes: 大小（字节）
            token_count: Token 数量
            ttl: 可选的 TTL（秒），默认使用构造函数的值
        
        Example:
            >>> cache = LRUCache()
            >>> cache.set(
            ...     "prompt-123",
            ...     {"response": "answer"},
            ...     size_bytes=1024,
            ...     token_count=500
            ... )
        """
        # 如果已存在，先删除旧条目
        if key in self.cache:
            old_entry = self.cache[key]
            self.current_size -= old_entry.size_bytes
            del self.cache[key]
        
        # 检查容量限制，淘汰最久未使用的条目
        while (
            self.current_size + size_bytes > self.max_size_bytes or
            len(self.cache) >= self.max_entries
        ):
            if not self.cache:
                break
            
            # 淘汰最旧的条目
            oldest_key, oldest_entry = self.cache.popitem(last=False)
            self.current_size -= oldest_entry.size_bytes
            self.eviction_count += 1
        
        # 写入新条目
        entry = CacheEntry(
            key=key,
            value=value,
            created_at=time.time(),
            last_accessed=time.time(),
            access_count=1,
            size_bytes=size_bytes,
            token_count=token_count
        )
        
        self.cache[key] = entry
        self.current_size += size_bytes
    
    def delete(self, key: str) -> bool:
        """
        删除缓存
        
        Args:
            key: 缓存键
        
        Returns:
            bool: 是否删除成功
        """
        if key in self.cache:
            entry = self.cache[key]
            self.current_size -= entry.size_bytes
            del self.cache[key]
            return True
        return False
    
    def clear(self):
        """清空缓存"""
        self.cache.clear()
        self.current_size = 0
    
    def get_stats(self) -> Dict[str, Any]:
        """
        获取缓存统计信息
        
        Returns:
            dict: 统计信息
        
        Example:
            >>> cache = LRUCache()
            >>> stats = cache.get_stats()
            >>> print(f"命中率：{stats['hit_rate_percent']}%")
            >>> print(f"淘汰数：{stats['eviction_count']}")
        """
        total_requests = self.hit_count + self.miss_count
        hit_rate = (self.hit_count / total_requests * 100) if total_requests > 0 else 0
        
        return {
            "total_entries": len(self.cache),
            "current_size_bytes": self.current_size,
            "current_size_mb": round(self.current_size / (1024 * 1024), 2),
            "max_size_bytes": self.max_size_bytes,
            "max_size_mb": self.max_size_bytes / (1024 * 1024),
            "hit_count": self.hit_count,
            "miss_count": self.miss_count,
            "eviction_count": self.eviction_count,
            "hit_rate_percent": round(hit_rate, 2),
            "utilization_percent": round(
                self.current_size / self.max_size_bytes * 100, 2
            ),
            "total_tokens_cached": sum(
                e.token_count for e in self.cache.values()
            )
        }
    
    def get_total_tokens_saved(self) -> int:
        """
        获取节省的 Token 总数
        
        Returns:
            int: 节省的 Token 数
        """
        return sum(
            e.token_count * (e.access_count - 1)
            for e in self.cache.values()
        )


@dataclass
class TokenBudget:
    """
    Token 预算管理器
    
    用于控制和预测 Token 使用成本
    
    特性:
        - 每日/每月预算限制
        - 单次请求限制
        - 使用率告警
        - 预算重置
    
    Example:
        >>> budget = TokenBudget(
        ...     daily_limit=1000000,
        ...     per_request_limit=8000,
        ...     warning_threshold=0.8
        ... )
        >>> 
        >>> # 检查是否可以花费
        >>> if budget.can_spend(5000):
        ...     # 执行 LLM 调用
        ...     budget.spend(5000)
        >>> 
        >>> # 查看剩余预算
        >>> remaining = budget.get_remaining_budget()
        >>> print(f"剩余：{remaining} tokens")
    """
    
    daily_limit: int = 1000000
    per_request_limit: int = 8000
    warning_threshold: float = 0.8
    
    used_today: int = 0
    reset_time: float = field(default_factory=lambda: 0)
    
    def __post_init__(self):
        """初始化后设置重置时间"""
        if self.reset_time == 0:
            self.reset_time = self._get_midnight()
    
    def can_spend(self, token_count: int) -> bool:
        """
        检查是否可以花费指定数量的 Token
        
        Args:
            token_count: Token 数量
        
        Returns:
            bool: 是否可以花费
        
        Example:
            >>> budget = TokenBudget(daily_limit=100000)
            >>> if budget.can_spend(5000):
            ...     print("可以花费")
        """
        # 检查是否需要重置
        self._check_reset()
        
        # 检查单次请求限制
        if token_count > self.per_request_limit:
            return False
        
        # 检查每日限额
        if self.used_today + token_count > self.daily_limit:
            return False
        
        # 检查是否接近阈值
        usage_ratio = (self.used_today + token_count) / self.daily_limit
        if usage_ratio > self.warning_threshold:
            import warnings
            warnings.warn(
                f"Token 使用率已达 {usage_ratio*100:.1f}%，请注意控制"
            )
        
        return True
    
    def spend(self, token_count: int) -> bool:
        """
        花费 Token
        
        Args:
            token_count: Token 数量
        
        Returns:
            bool: 是否成功花费
        
        Example:
            >>> budget = TokenBudget()
            >>> if budget.spend(5000):
            ...     print("花费成功")
        """
        if not self.can_spend(token_count):
            return False
        
        self.used_today += token_count
        return True
    
    def refund(self, token_count: int):
        """
        退还 Token（用于错误回滚）
        
        Args:
            token_count: Token 数量
        
        Example:
            >>> budget = TokenBudget()
            >>> budget.spend(5000)
            >>> budget.refund(5000)  # 回滚
        """
        self.used_today = max(0, self.used_today - token_count)
    
    def get_remaining_budget(self) -> int:
        """
        获取剩余预算
        
        Returns:
            int: 剩余 Token 数
        """
        self._check_reset()
        return max(0, self.daily_limit - self.used_today)
    
    def get_usage_stats(self) -> Dict[str, Any]:
        """
        获取使用统计
        
        Returns:
            dict: 统计信息
        """
        self._check_reset()
        usage_ratio = self.used_today / self.daily_limit
        
        return {
            "used_today": self.used_today,
            "daily_limit": self.daily_limit,
            "remaining": self.get_remaining_budget(),
            "usage_percent": round(usage_ratio * 100, 2),
            "warning_threshold": self.warning_threshold * 100,
            "reset_time": datetime.fromtimestamp(self.reset_time).isoformat(),
            "is_near_limit": usage_ratio > self.warning_threshold
        }
    
    def reset(self):
        """手动重置预算"""
        self.used_today = 0
        self.reset_time = self._get_midnight()
    
    def _check_reset(self):
        """检查是否需要重置"""
        if time.time() >= self.reset_time:
            self.used_today = 0
            self.reset_time = self._get_midnight()
    
    def _get_midnight(self) -> float:
        """获取今晚午夜的时间戳"""
        now = datetime.now()
        midnight = now.replace(
            hour=23, minute=59, second=59, microsecond=0
        )
        return midnight.timestamp()


class ContextCompressor:
    """
    上下文压缩器
    
    用于减少上下文 Token 消耗
    
    策略:
        - 移除冗余信息
        - 摘要长文本
        - 优先级排序
    
    Example:
        >>> compressor = ContextCompressor(max_tokens=2000)
        >>> compressed = compressor.compress(long_context)
        >>> print(f"压缩后：{len(compressed)} tokens")
    """
    
    def __init__(self, max_tokens: int = 2000):
        """
        初始化压缩器
        
        Args:
            max_tokens: 最大 Token 数
        """
        self.max_tokens = max_tokens
    
    def compress(self, context: str) -> str:
        """
        压缩上下文
        
        Args:
            context: 原始上下文
        
        Returns:
            str: 压缩后的上下文
        
        Note:
            实际实现需要调用 LLM 进行摘要
            这里提供基础框架
        """
        # @future 智能压缩逻辑实现要点：
        # 1. 计算当前 token 数（基于实际模型分词器）
        # 2. 如果超出限制，进行摘要（提取关键实体和关系）
        # 3. 保留关键信息（基于重要性评分）
        # 4. 支持多种压缩策略（摘要、截断、抽取）
        
        return context[:self.max_tokens * 4]  # 粗略估计
    
    def estimate_tokens(self, text: str) -> int:
        """
        估算 Token 数量（使用标准化算法）
        
        Args:
            text: 文本
        
        Returns:
            int: Token 数量（估算）
        
        Note:
            使用与 C 语言实现相同的标准化算法，确保跨语言一致性。
            默认使用通用模型和中等精度，提供合理的估算结果。
        """
        # 使用标准化 Token 计算函数
        # 默认使用通用模型和中等精度
        return standard_token_count(
            text=text,
            model_type=TokenModelType.GENERIC,
            precision=TokenPrecision.MEDIUM
        )


class TokenOptimizer:
    """
    Token 优化器（统一管理）
    
    整合 LRU 缓存、预算管理、上下文压缩
    
    Example:
        >>> optimizer = TokenOptimizer(
        ...     cache_size_mb=50,
        ...     daily_budget=1000000
        ... )
        >>> 
        >>> # 执行优化的 LLM 调用
        >>> response = optimizer.execute(
        ...     prompt="问题",
        ...     model="gpt-4"
        ... )
        >>> 
        >>> # 查看优化效果
        >>> stats = optimizer.get_stats()
        >>> print(f"节省率：{stats['savings_percent']}%")
    """
    
    def __init__(
        self,
        cache_size_mb: int = 50,
        cache_ttl: int = 3600,
        daily_budget: int = 1000000,
        per_request_limit: int = 8000
    ):
        """
        初始化优化器
        
        Args:
            cache_size_mb: 缓存大小
            cache_ttl: 缓存 TTL
            daily_budget: 每日预算
            per_request_limit: 单次请求限制
        """
        self.cache = LRUCache(
            max_size_mb=cache_size_mb,
            ttl_seconds=cache_ttl
        )
        
        self.budget = TokenBudget(
            daily_limit=daily_budget,
            per_request_limit=per_request_limit
        )
        
        self.compressor = ContextCompressor()
        
        # 统计信息
        self.total_tokens_used = 0
        self.total_tokens_saved = 0
    
    def execute(
        self,
        prompt: str,
        model: str = "gpt-4",
        use_cache: bool = True,
        **kwargs
    ) -> Dict[str, Any]:
        """
        执行优化的 LLM 调用
        
        Args:
            prompt: 提示词
            model: 模型名称
            use_cache: 是否使用缓存
            **kwargs: 其他参数
        
        Returns:
            dict: 响应结果
        
        Example:
            >>> optimizer = TokenOptimizer()
            >>> response = optimizer.execute(
            ...     prompt="翻译这句话",
            ...     model="gpt-4"
            ... )
        """
        # 生成缓存键
        cache_key = self._generate_cache_key(prompt, model, kwargs)
        
        # 尝试从缓存获取
        if use_cache:
            cached = self.cache.get(cache_key)
            if cached:
                self.total_tokens_saved += cached.get("token_count", 0)
                return cached["response"]
        
        # 检查预算
        estimated_tokens = self.compressor.estimate_tokens(prompt)
        if not self.budget.can_spend(estimated_tokens):
            raise RuntimeError("Token 预算不足")
        
        # 执行实际调用（需要集成 LLM 客户端）
        # response = llm_client.call(prompt, model)
        # token_count = response.usage.total_tokens
        
        # 模拟响应
        response = {
            "content": "模拟响应",
            "model": model
        }
        token_count = estimated_tokens
        
        # 更新预算
        self.budget.spend(token_count)
        self.total_tokens_used += token_count
        
        # 写入缓存
        self.cache.set(
            cache_key,
            {"response": response, "token_count": token_count},
            size_bytes=len(prompt) * 2,
            token_count=token_count
        )
        
        return response
    
    def get_stats(self) -> Dict[str, Any]:
        """
        获取优化统计
        
        Returns:
            dict: 统计信息
        """
        cache_stats = self.cache.get_stats()
        budget_stats = self.budget.get_usage_stats()
        
        total = self.total_tokens_used + self.total_tokens_saved
        savings_percent = (
            self.total_tokens_saved / total * 100
            if total > 0 else 0
        )
        
        return {
            "cache": cache_stats,
            "budget": budget_stats,
            "total_tokens_used": self.total_tokens_used,
            "total_tokens_saved": self.total_tokens_saved,
            "savings_percent": round(savings_percent, 2)
        }
    
    def _generate_cache_key(
        self,
        prompt: str,
        model: str,
        kwargs: dict
    ) -> str:
        """生成缓存键"""
        raw = f"{prompt}:{model}:{sorted(kwargs.items())}"
        return hashlib.md5(raw.encode()).hexdigest()
