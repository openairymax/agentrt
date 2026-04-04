#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS Token 计数器
# 遵循 AgentOS 架构设计原则：E-2 可观测性、E-3 资源确定性、A-1 简约至上

"""
AgentOS Token 计数器

用于统计和分析 Token 使用量，支持：
- 实时 Token 计数
- 使用统计和趋势分析
- 多模型支持
- 成本估算

Usage:
    from scripts.ops.token_counter import TokenCounter, get_token_counter
    
    counter = get_token_counter()
    
    # 统计文本 Token 数量
    token_count = counter.count_tokens("Hello, world!", model="gpt-4")
    
    # 记录 Token 使用
    counter.record_usage(task_id="task_123", tokens=100, model="gpt-4")
    
    # 获取使用统计
    stats = counter.get_usage_stats(task_id="task_123")
    print(f"Total tokens: {stats.total_tokens}")
    print(f"Estimated cost: ${stats.estimated_cost}")
"""

import json
import os
import logging
from dataclasses import dataclass, field, asdict
from datetime import datetime, timedelta
from enum import Enum
from pathlib import Path
from typing import Any, Dict, List, Optional, Callable
from collections import defaultdict
import threading

logger = logging.getLogger(__name__)


class ModelProvider(Enum):
    """模型提供商"""
    OPENAI = "openai"
    ANTHROPIC = "anthropic"
    DEEPSEEK = "deepseek"
    LOCAL = "local"
    OTHER = "other"


@dataclass
class TokenUsageRecord:
    """Token 使用记录"""
    record_id: str
    task_id: str
    timestamp: str
    model: str
    provider: str
    input_tokens: int
    output_tokens: int
    total_tokens: int
    cost_usd: float
    metadata: Dict[str, Any] = field(default_factory=dict)


@dataclass
class TokenUsageStats:
    """Token 使用统计"""
    total_tokens: int = 0
    input_tokens: int = 0
    output_tokens: int = 0
    total_cost_usd: float = 0.0
    request_count: int = 0
    average_tokens_per_request: float = 0.0
    model_breakdown: Dict[str, int] = field(default_factory=dict)
    time_range_start: str = ""
    time_range_end: str = ""


@dataclass
class ModelPricing:
    """模型定价信息（每 1000 tokens）"""
    input_price_usd: float
    output_price_usd: float
    currency: str = "USD"


class TokenCounter:
    """
    Token 计数器
    
    遵循架构原则：
    - E-2 可观测性：完整的 Token 使用统计和趋势分析
    - E-3 资源确定性：自动清理过期记录
    - A-1 简约至上：简洁的接口设计
    - C-1 双系统协同：支持快速估算和精确计数
    """
    
    DEFAULT_MODEL_PRICING: Dict[str, ModelPricing] = {
        "gpt-4": ModelPricing(input_price_usd=0.03, output_price_usd=0.06),
        "gpt-4-turbo": ModelPricing(input_price_usd=0.01, output_price_usd=0.03),
        "gpt-3.5-turbo": ModelPricing(input_price_usd=0.0005, output_price_usd=0.0015),
        "claude-3-opus": ModelPricing(input_price_usd=0.015, output_price_usd=0.075),
        "claude-3-sonnet": ModelPricing(input_price_usd=0.003, output_price_usd=0.015),
        "claude-3-haiku": ModelPricing(input_price_usd=0.00025, output_price_usd=0.00125),
        "deepseek-chat": ModelPricing(input_price_usd=0.00014, output_price_usd=0.00028),
        "deepseek-coder": ModelPricing(input_price_usd=0.00014, output_price_usd=0.00028),
    }
    
    def __init__(
        self,
        storage_dir: Optional[str] = None,
        custom_pricing: Optional[Dict[str, ModelPricing]] = None,
        auto_save: bool = True
    ):
        """
        初始化 Token 计数器
        
        Args:
            storage_dir: 存储目录，默认为 /var/lib/agentos/token_stats
            custom_pricing: 自定义定价信息
            auto_save: 是否自动保存到文件
        """
        self.storage_dir = storage_dir or "/var/lib/agentos/token_stats"
        self.custom_pricing = custom_pricing or {}
        self.auto_save = auto_save
        
        self._usage_records: List[TokenUsageRecord] = []
        self._task_stats: Dict[str, TokenUsageStats] = defaultdict(TokenUsageStats)
        self._lock = threading.Lock()
        
        self._ensure_storage_dir()
        self._load_records()
    
    def _ensure_storage_dir(self) -> None:
        """确保存储目录存在"""
        try:
            Path(self.storage_dir).mkdir(parents=True, exist_ok=True)
            logger.info(f"Token stats storage directory ensured: {self.storage_dir}")
        except OSError as e:
            logger.error(f"Failed to create storage directory: {e}")
            raise
    
    def count_tokens(
        self,
        text: str,
        model: str = "gpt-3.5-turbo",
        use_fast_estimator: bool = True
    ) -> int:
        """
        统计文本的 Token 数量
        
        Args:
            text: 输入文本
            model: 模型名称
            use_fast_estimator: 是否使用快速估算（4 字符≈1 token）
            
        Returns:
            int: Token 数量
        """
        if use_fast_estimator:
            return self._fast_token_estimate(text, model)
        else:
            return self._accurate_token_count(text, model)
    
    def _fast_token_estimate(self, text: str, model: str) -> int:
        """快速 Token 估算（4 字符≈1 token）"""
        char_count = len(text)
        tokens = char_count // 4
        return max(1, tokens)
    
    def _accurate_token_count(self, text: str, model: str) -> int:
        """
        精确 Token 计数
        
        注：实际项目中应集成 tiktoken 或其他 Tokenizer
        这里使用简化实现
        """
        try:
            import tiktoken
            encoding = tiktoken.encoding_for_model(model)
            tokens = len(encoding.encode(text))
            return tokens
        except ImportError:
            logger.warning("tiktoken not installed, using fast estimation")
            return self._fast_token_estimate(text, model)
        except Exception as e:
            logger.warning(f"Failed to count tokens accurately: {e}, using fast estimation")
            return self._fast_token_estimate(text, model)
    
    def record_usage(
        self,
        task_id: str,
        input_tokens: int,
        output_tokens: int,
        model: str,
        metadata: Optional[Dict[str, Any]] = None,
        cost_override: Optional[float] = None
    ) -> str:
        """
        记录 Token 使用
        
        Args:
            task_id: 任务 ID
            input_tokens: 输入 Token 数
            output_tokens: 输出 Token 数
            model: 模型名称
            metadata: 元数据
            cost_override: 覆盖自动计算的成本
            
        Returns:
            str: 记录 ID
        """
        import uuid
        
        total_tokens = input_tokens + output_tokens
        provider = self._detect_provider(model)
        
        if cost_override is not None:
            cost_usd = cost_override
        else:
            cost_usd = self._calculate_cost(input_tokens, output_tokens, model)
        
        record = TokenUsageRecord(
            record_id=str(uuid.uuid4()),
            task_id=task_id,
            timestamp=datetime.now().isoformat(),
            model=model,
            provider=provider.value,
            input_tokens=input_tokens,
            output_tokens=output_tokens,
            total_tokens=total_tokens,
            cost_usd=cost_usd,
            metadata=metadata or {}
        )
        
        with self._lock:
            self._usage_records.append(record)
            self._update_task_stats(task_id, record)
            
            if self.auto_save:
                self._save_records()
        
        logger.info(
            f"Recorded token usage: {total_tokens} tokens "
            f"(${cost_usd:.4f}) for task {task_id}"
        )
        
        return record.record_id
    
    def get_usage_stats(
        self,
        task_id: Optional[str] = None,
        start_time: Optional[datetime] = None,
        end_time: Optional[datetime] = None,
        model: Optional[str] = None
    ) -> TokenUsageStats:
        """
        获取 Token 使用统计
        
        Args:
            task_id: 任务 ID（可选，过滤特定任务）
            start_time: 开始时间
            end_time: 结束时间
            model: 模型名称（可选）
            
        Returns:
            TokenUsageStats: 使用统计
        """
        with self._lock:
            filtered_records = self._usage_records
            
            if task_id:
                filtered_records = [r for r in filtered_records if r.task_id == task_id]
            
            if start_time:
                filtered_records = [
                    r for r in filtered_records
                    if datetime.fromisoformat(r.timestamp) >= start_time
                ]
            
            if end_time:
                filtered_records = [
                    r for r in filtered_records
                    if datetime.fromisoformat(r.timestamp) <= end_time
                ]
            
            if model:
                filtered_records = [r for r in filtered_records if r.model == model]
            
            return self._calculate_stats(filtered_records)
    
    def get_model_breakdown(
        self,
        task_id: Optional[str] = None
    ) -> Dict[str, int]:
        """
        获取模型使用分布
        
        Args:
            task_id: 任务 ID（可选）
            
        Returns:
            Dict[str, int]: 模型 Token 使用分布
        """
        stats = self.get_usage_stats(task_id=task_id)
        return stats.model_breakdown
    
    def get_cost_estimate(
        self,
        task_id: Optional[str] = None,
        period_days: int = 30
    ) -> float:
        """
        获取成本估算
        
        Args:
            task_id: 任务 ID（可选）
            period_days: 统计周期（天）
            
        Returns:
            float: 估算成本（USD）
        """
        start_time = datetime.now() - timedelta(days=period_days)
        stats = self.get_usage_stats(task_id=task_id, start_time=start_time)
        return stats.total_cost_usd
    
    def reset_stats(self, task_id: Optional[str] = None) -> None:
        """
        重置统计
        
        Args:
            task_id: 任务 ID（可选，None 表示重置所有）
        """
        with self._lock:
            if task_id:
                self._usage_records = [
                    r for r in self._usage_records if r.task_id != task_id
                ]
                if task_id in self._task_stats:
                    del self._task_stats[task_id]
            else:
                self._usage_records.clear()
                self._task_stats.clear()
            
            if self.auto_save:
                self._save_records()
        
        logger.info(f"Reset token stats for task: {task_id or 'all'}")
    
    def export_report(
        self,
        output_path: str,
        format: str = "json"
    ) -> bool:
        """
        导出使用报告
        
        Args:
            output_path: 输出文件路径
            format: 导出格式（json/csv）
            
        Returns:
            bool: 是否成功
        """
        try:
            with self._lock:
                if format == "json":
                    data = [asdict(r) for r in self._usage_records]
                    with open(output_path, 'w', encoding='utf-8') as f:
                        json.dump(data, f, indent=2, ensure_ascii=False)
                else:
                    logger.error(f"Unsupported export format: {format}")
                    return False
            
            logger.info(f"Exported token usage report to: {output_path}")
            return True
            
        except Exception as e:
            logger.error(f"Failed to export report: {e}")
            return False
    
    def _detect_provider(self, model: str) -> ModelProvider:
        """检测模型提供商"""
        model_lower = model.lower()
        
        if "gpt" in model_lower or "openai" in model_lower:
            return ModelProvider.OPENAI
        elif "claude" in model_lower:
            return ModelProvider.ANTHROPIC
        elif "deepseek" in model_lower:
            return ModelProvider.DEEPSEEK
        elif "local" in model_lower:
            return ModelProvider.LOCAL
        else:
            return ModelProvider.OTHER
    
    def _calculate_cost(
        self,
        input_tokens: int,
        output_tokens: int,
        model: str
    ) -> float:
        """计算 Token 使用成本"""
        pricing = self.custom_pricing.get(model) or self.DEFAULT_MODEL_PRICING.get(model)
        
        if not pricing:
            logger.warning(f"No pricing info for model: {model}, using default")
            pricing = ModelPricing(input_price_usd=0.001, output_price_usd=0.002)
        
        input_cost = (input_tokens / 1000) * pricing.input_price_usd
        output_cost = (output_tokens / 1000) * pricing.output_price_usd
        
        return input_cost + output_cost
    
    def _update_task_stats(
        self,
        task_id: str,
        record: TokenUsageRecord
    ) -> None:
        """更新任务统计"""
        stats = self._task_stats[task_id]
        
        stats.total_tokens += record.total_tokens
        stats.input_tokens += record.input_tokens
        stats.output_tokens += record.output_tokens
        stats.total_cost_usd += record.cost_usd
        stats.request_count += 1
        stats.average_tokens_per_request = (
            stats.total_tokens / stats.request_count
        )
        
        if record.model not in stats.model_breakdown:
            stats.model_breakdown[record.model] = 0
        stats.model_breakdown[record.model] += record.total_tokens
        
        if not stats.time_range_start:
            stats.time_range_start = record.timestamp
        stats.time_range_end = record.timestamp
    
    def _calculate_stats(
        self,
        records: List[TokenUsageRecord]
    ) -> TokenUsageStats:
        """计算统计信息"""
        stats = TokenUsageStats()
        
        for record in records:
            stats.total_tokens += record.total_tokens
            stats.input_tokens += record.input_tokens
            stats.output_tokens += record.output_tokens
            stats.total_cost_usd += record.cost_usd
            stats.request_count += 1
            
            if record.model not in stats.model_breakdown:
                stats.model_breakdown[record.model] = 0
            stats.model_breakdown[record.model] += record.total_tokens
        
        if stats.request_count > 0:
            stats.average_tokens_per_request = (
                stats.total_tokens / stats.request_count
            )
        
        if records:
            stats.time_range_start = records[0].timestamp
            stats.time_range_end = records[-1].timestamp
        
        return stats
    
    def _save_records(self) -> None:
        """保存记录到文件"""
        try:
            records_file = os.path.join(self.storage_dir, "usage_records.json")
            data = [asdict(r) for r in self._usage_records]
            
            with open(records_file, 'w', encoding='utf-8') as f:
                json.dump(data, f, indent=2, ensure_ascii=False)
            
            logger.debug(f"Saved {len(self._usage_records)} token usage records")
            
        except Exception as e:
            logger.error(f"Failed to save token usage records: {e}")
    
    def _load_records(self) -> None:
        """从文件加载记录"""
        try:
            records_file = os.path.join(self.storage_dir, "usage_records.json")
            
            if not os.path.exists(records_file):
                logger.info("No existing token usage records found")
                return
            
            with open(records_file, 'r', encoding='utf-8') as f:
                data = json.load(f)
            
            self._usage_records = [
                TokenUsageRecord(**r) for r in data
            ]
            
            for record in self._usage_records:
                self._update_task_stats(record.task_id, record)
            
            logger.info(f"Loaded {len(self._usage_records)} token usage records")
            
        except Exception as e:
            logger.error(f"Failed to load token usage records: {e}")
            self._usage_records.clear()


def get_token_counter(
    storage_dir: Optional[str] = None,
    **kwargs
) -> TokenCounter:
    """
    获取全局 Token 计数器实例
    
    Args:
        storage_dir: 存储目录
        **kwargs: 其他参数
        
    Returns:
        TokenCounter: Token 计数器实例
    """
    global _global_token_counter
    
    if '_global_token_counter' not in globals():
        _global_token_counter = TokenCounter(
            storage_dir=storage_dir,
            **kwargs
        )
    
    return _global_token_counter
