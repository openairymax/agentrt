#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS Token Utilities
# Merged from scripts/operations/token_counter.py + token_budget.py

"""
AgentOS Token Utilities

Combined token counting and budget management:
- TokenCounter: Count tokens in text with multiple encoding strategies
- TokenBudget: Budget allocation, tracking, and alerting for LLM API usage

Usage:
    from scripts.toolkit import TokenCounter, TokenBudget, get_token_counter, get_token_budget
    
    counter = get_token_counter()
    count = counter.count_tokens("Hello world")
    
    budget = get_token_budget()
    budget.set_budget("task_1", max_tokens=10000, max_cost_usd=1.0)
"""

import json
import os
import logging
from dataclasses import dataclass, field, asdict
from datetime import datetime, timedelta
from enum import Enum
from pathlib import Path
from typing import Any, Dict, List, Optional, Callable
import threading

logger = logging.getLogger(__name__)


# ==================== TOKEN COUNTER ====================

class EncodingStrategy(Enum):
    """Token counting strategies"""
    APPROXIMATE = "approximate"
    WORD_BASED = "word_based"
    CHAR_BASED = "char_base"
    BYTE_PAIR = "byte_pair"


@dataclass
class TokenCountResult:
    """Result of a token count operation"""
    text: str
    token_count: int
    char_count: int
    word_count: int
    line_count: int
    strategy: str
    estimated: bool


@dataclass
class TokenUsageStats:
    """Aggregated token usage statistics"""
    total_tokens: int = 0
    total_chars: int = 0
    total_requests: int = 0
    average_tokens_per_request: float = 0.0
    max_single_request: int = 0
    min_single_request: int = 0


class TokenCounter:
    """
    Multi-strategy token counter for LLM text processing.
    
    Supports approximate counting (for when tiktoken is unavailable),
    word-based, character-based, and BPE-style estimation.
    """

    AVG_CHARS_PER_TOKEN = 4.0
    AVG_WORDS_PER_TOKEN = 0.75

    def __init__(
        self,
        default_strategy: EncodingStrategy = EncodingStrategy.APPROXIMATE,
        cache_results: bool = True
    ):
        self.default_strategy = default_strategy
        self.cache_results = cache_results
        self._cache: Dict[str, int] = {}
        self._stats = TokenUsageStats()

    def count_tokens(
        self,
        text: str,
        strategy: Optional[EncodingStrategy] = None
    ) -> TokenCountResult:
        """Count tokens in text using specified or default strategy"""
        strat = strategy or self.default_strategy
        
        if not text or not text.strip():
            return TokenCountResult(
                text=text, token_count=0, char_count=0,
                word_count=0, line_count=0, strategy=strat.value, estimated=True
            )
        
        if self.cache_results and text in self._cache:
            count = self._cache[text]
        else:
            count = self._count_with_strategy(text, strat)
            if self.cache_results:
                self._cache[text] = count
        
        result = TokenCountResult(
            text=text[:50] + ("..." if len(text) > 50 else ""),
            token_count=count,
            char_count=len(text),
            word_count=len(text.split()) if text.strip() else 0,
            line_count=text.count("\n") + 1,
            strategy=strat.value,
            estimated=(strat != EncodingStrategy.BYTE_PAIR)
        )
        
        self._update_stats(count)
        return result

    def _count_with_strategy(self, text: str, strategy: EncodingStrategy) -> int:
        if strategy == EncodingStrategy.APPROXIMATE:
            return max(1, int(len(text) / self.AVG_CHARS_PER_TOKEN))
        
        elif strategy == EncodingStrategy.WORD_BASED:
            words = text.split() if text.strip() else []
            return max(1, int(len(words) * self.AVG_WORDS_PER_TOKEN))
        
        elif strategy == EncodingStrategy.CHAR_BASE:
            return len(text)
        
        elif strategy == EncodingStrategy.BYTE_PAIR:
            try:
                import tiktoken
                enc = tiktoken.get_encoding("cl100k_base")
                return len(enc.encode(text))
            except ImportError:
                return max(1, int(len(text) / self.AVG_CHARS_PER_TOKEN))
        
        return max(1, int(len(text) / self.AVG_CHARS_PER_TOKEN))

    def _update_stats(self, count: int):
        s = self._stats
        s.total_tokens += count
        s.total_requests += 1
        s.average_tokens_per_request = s.total_tokens / s.total_requests
        s.max_single_request = max(s.max_single_request, count)
        s.min_single_request = min(s.min_single_request, count) if s.min_single_request > 0 else count

    def batch_count(self, texts: List[str]) -> List[TokenCountResult]:
        """Count tokens for multiple texts efficiently"""
        return [self.count_tokens(t) for t in texts]

    def estimate_cost(
        self,
        text: str,
        cost_per_1k_tokens: float = 0.002
    ) -> Dict[str, Any]:
        """Estimate API cost for given text"""
        result = self.count_tokens(text)
        cost = (result.token_count / 1000) * cost_per_1k_tokens
        return {
            "tokens": result.token_count,
            "estimated_cost_usd": round(cost, 6),
            "cost_per_1k_tokens": cost_per_1k_tokens,
        }

    def get_stats(self) -> TokenUsageStats:
        return self._stats

    def clear_cache(self):
        self._cache.clear()


def get_token_counter(**kwargs) -> TokenCounter:
    global _token_counter
    if '_token_counter' not in globals():
        _token_counter = TokenCounter(**kwargs)
    return _token_counter


# ==================== TOKEN BUDGET ====================

class BudgetStatus(Enum):
    WITHIN_BUDGET = "within_budget"
    WARNING = "warning"
    OVER_BUDGET = "over_budget"
    EXHAUSTED = "exhausted"


@dataclass
class BudgetConfig:
    task_id: str
    max_tokens: int
    max_cost_usd: float
    warning_threshold_percentage: float = 80.0
    soft_limit_percentage: float = 90.0
    hard_limit_percentage: float = 100.0
    created_at: str = ""
    updated_at: str = ""
    enabled: bool = True
    auto_renew: bool = False
    renewal_period_days: int = 30


@dataclass
class BudgetUsage:
    task_id: str
    used_tokens: int = 0
    used_cost_usd: float = 0.0
    request_count: int = 0
    last_used_at: str = ""
    average_tokens_per_request: float = 0.0


@dataclass
class BudgetCheckResult:
    task_id: str
    status: BudgetStatus
    used_tokens: int
    max_tokens: int
    used_cost_usd: float
    max_cost_usd: float
    remaining_tokens: int
    remaining_cost_usd: float
    remaining_percentage: float
    is_over_budget: bool
    is_warning: bool
    message: str = ""


@dataclass
class BudgetAlert:
    alert_id: str
    task_id: str
    alert_type: str
    threshold_percentage: float
    current_percentage: float
    timestamp: str
    message: str


class TokenBudget:
    """
    Token budget management for controlling LLM API costs.
    
    Features:
    - Per-task budget allocation (tokens + USD)
    - Usage tracking with automatic alerts
    - Threshold-based warning system
    - Persistent state storage
    """

    def __init__(
        self,
        storage_dir: Optional[str] = None,
        default_warning_threshold: float = 80.0,
        enable_alerts: bool = True,
        alert_callback: Optional[Callable[[BudgetAlert], None]] = None
    ):
        self.storage_dir = storage_dir or "/var/lib/agentos/budget"
        self.default_warning_threshold = default_warning_threshold
        self.enable_alerts = enable_alerts
        self.alert_callback = alert_callback
        
        self._budgets: Dict[str, BudgetConfig] = {}
        self._usage: Dict[str, BudgetUsage] = {}
        self._alerts: List[BudgetAlert] = []
        self._lock = threading.Lock()
        
        Path(self.storage_dir).mkdir(parents=True, exist_ok=True)
        self._load_state()

    def set_budget(
        self,
        task_id: str,
        max_tokens: int,
        max_cost_usd: float,
        warning_threshold: Optional[float] = None,
        auto_renew: bool = False,
        renewal_period_days: int = 30
    ) -> bool:
        try:
            timestamp = datetime.now().isoformat()
            config = BudgetConfig(
                task_id=task_id,
                max_tokens=max_tokens,
                max_cost_usd=max_cost_usd,
                warning_threshold_percentage=warning_threshold or self.default_warning_threshold,
                created_at=timestamp,
                updated_at=timestamp
            )
            
            with self._lock:
                self._budgets[task_id] = config
                if task_id not in self._usage:
                    self._usage[task_id] = BudgetUsage(task_id=task_id)
                self._save_state()
            
            logger.info(f"Budget set for {task_id}: {max_tokens} tokens, ${max_cost_usd:.2f}")
            return True
            
        except Exception as e:
            logger.error(f"Failed to set budget: {e}")
            return False

    def check_budget(self, task_id: str) -> BudgetCheckResult:
        with self._lock:
            if task_id not in self._budgets:
                return self._create_empty_result(task_id)
            
            config = self._budgets[task_id]
            usage = self._usage.get(task_id, BudgetUsage(task_id=task_id))
            
            remaining_tokens = max(0, config.max_tokens - usage.used_tokens)
            remaining_cost = max(0.0, config.max_cost_usd - usage.used_cost_usd)
            
            token_pct = (usage.used_tokens / config.max_tokens * 100) if config.max_tokens > 0 else 0
            cost_pct = (usage.used_cost_usd / config.max_cost_usd * 100) if config.max_cost_usd > 0 else 0
            max_pct = max(token_pct, cost_pct)
            
            status, over, msg = self._determine_status(max_pct, config)
            self._handle_alerts(task_id, status, max_pct, config)
            
            return BudgetCheckResult(
                task_id=task_id, status=status,
                used_tokens=usage.used_tokens, max_tokens=config.max_tokens,
                used_cost_usd=usage.used_cost_usd, max_cost_usd=config.max_cost_usd,
                remaining_tokens=remaining_tokens, remaining_cost_usd=remaining_cost,
                remaining_percentage=100.0 - max_pct,
                is_over_budget=over, is_warning=(status == BudgetStatus.WARNING),
                message=msg
            )

    def record_usage(
        self, task_id: str, tokens: int, cost_usd: float = 0.0
    ) -> BudgetCheckResult:
        with self._lock:
            if task_id not in self._usage:
                self._usage[task_id] = BudgetUsage(task_id=task_id)
            
            u = self._usage[task_id]
            u.used_tokens += tokens
            u.used_cost_usd += cost_usd
            u.request_count += 1
            u.last_used_at = datetime.now().isoformat()
            u.average_tokens_per_request = u.used_tokens / u.request_count
            self._save_state()
        
        return self.check_budget(task_id)

    def can_use_tokens(self, task_id: str, tokens: int, cost_usd: float = 0.0) -> bool:
        r = self.check_budget(task_id)
        return tokens <= r.remaining_tokens and cost_usd <= r.remaining_cost_usd

    def list_budgets(self) -> List[BudgetConfig]:
        with self._lock:
            return list(self._budgets.values())

    def get_usage_summary(self, task_id: Optional[str] = None) -> Dict[str, Any]:
        with self._lock:
            if task_id:
                u = self._usage.get(task_id)
                return asdict(u) if u else {}
            
            total_t = sum(u.used_tokens for u in self._usage.values())
            total_c = sum(u.used_cost_usd for u in self._usage.values())
            total_r = sum(u.request_count for u in self._usage.values())
            
            return {"total_tasks": len(self._usage), "total_tokens": total_t,
                    "total_cost_usd": total_c, "total_requests": total_r}

    def _determine_status(self, pct: float, cfg: BudgetConfig) -> tuple:
        if pct >= cfg.hard_limit_percentage:
            return BudgetStatus.EXHAUSTED, True, f"Budget exhausted ({pct:.1f}%)"
        if pct >= cfg.soft_limit_percentage:
            return BudgetStatus.OVER_BUDGET, True, f"Budget exceeded ({pct:.1f}%)"
        if pct >= cfg.warning_threshold_percentage:
            return BudgetStatus.WARNING, False, f"Budget warning ({pct:.1f}%)"
        return BudgetStatus.WITHIN_BUDGET, False, f"Budget OK ({pct:.1f}%)"

    def _handle_alerts(self, tid: str, st: BudgetStatus, pct: float, cfg: BudgetConfig):
        if not self.enable_alerts:
            return
        atype = {BudgetStatus.OVER_BUDGET: "over_budget",
                 BudgetStatus.EXHAUSTED: "over_budget"}.get(st)
        if not atype:
            return
        import uuid
        alert = BudgetAlert(
            alert_id=str(uuid.uuid4()), task_id=tid, alert_type=atype,
            threshold_percentage=cfg.warning_threshold_percentage,
            current_percentage=pct, timestamp=datetime.now().isoformat(),
            message=f"Budget {atype} for {tid}: {pct:.1f}%"
        )
        self._alerts.append(alert)
        if self.alert_callback:
            try:
                self.alert_callback(alert)
            except Exception as e:
                logger.error(f"Alert callback failed: {e}")

    @staticmethod
    def _create_empty_result(tid: str) -> BudgetCheckResult:
        return BudgetCheckResult(
            task_id=tid, status=BudgetStatus.WITHIN_BUDGET,
            used_tokens=0, max_tokens=0, used_cost_usd=0.0, max_cost_usd=0.0,
            remaining_tokens=0, remaining_cost_usd=0.0, remaining_percentage=100.0,
            is_over_budget=False, is_warning=False,
            message="No budget configured for this task"
        )

    def _save_state(self):
        try:
            bf = os.path.join(self.storage_dir, "budgets.json")
            uf = os.path.join(self.storage_dir, "usage.json")
            with open(bf, 'w', encoding='utf-8') as f:
                json.dump([asdict(b) for b in self._budgets.values()], f, indent=2)
            with open(uf, 'w', encoding='utf-8') as f:
                json.dump([asdict(u) for u in self._usage.values()], f, indent=2)
        except Exception as e:
            logger.error(f"Failed to save state: {e}")

    def _load_state(self):
        try:
            bf = os.path.join(self.storage_dir, "budgets.json")
            uf = os.path.join(self.storage_dir, "usage.json")
            if os.path.exists(bf):
                with open(bf, 'r', encoding='utf-8') as f:
                    self._budgets = {b["task_id"]: BudgetConfig(**b) for b in json.load(f)}
            if os.path.exists(uf):
                with open(uf, 'r', encoding='utf-8') as f:
                    self._usage = {u["task_id"]: BudgetUsage(**u) for u in json.load(f)}
        except Exception as e:
            logger.error(f"Failed to load state: {e}")


def get_token_budget(**kwargs) -> TokenBudget:
    global _global_token_budget
    if '_global_token_budget' not in globals():
        _global_token_budget = TokenBudget(**kwargs)
    return _global_token_budget
