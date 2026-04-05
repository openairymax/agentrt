#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS Token 预算管理
# 遵循 AgentOS 架构设计原则：E-3 资源确定性、E-1 安全内生、A-1 简约至上

"""
AgentOS Token 预算管理

用于控制和优化 Token 使用成本，支持：
- Token 预算分配和跟踪
- 预算超限告警
- 预算使用趋势分析
- 自动限流控制

Usage:
    from scripts.ops.token_budget import TokenBudget, get_token_budget
    
    budget = get_token_budget()
    
    # 设置任务预算
    budget.set_budget(task_id="task_123", max_tokens=10000, max_cost_usd=1.0)
    
    # 检查预算使用情况
    status = budget.check_budget(task_id="task_123")
    print(f"Used: {status.used_tokens}/{status.max_tokens}")
    print(f"Remaining: {status.remaining_percentage:.1f}%")
    
    # 记录 Token 使用
    budget.record_usage(task_id="task_123", tokens=500, cost_usd=0.05)
    
    # 预算超限告警
    if status.is_over_budget:
        print("Budget exceeded!")
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


class BudgetStatus(Enum):
    """预算状态"""
    WITHIN_BUDGET = "within_budget"
    WARNING = "warning"
    OVER_BUDGET = "over_budget"
    EXHAUSTED = "exhausted"


@dataclass
class BudgetConfig:
    """预算配置"""
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
    """预算使用情况"""
    task_id: str
    used_tokens: int = 0
    used_cost_usd: float = 0.0
    request_count: int = 0
    last_used_at: str = ""
    average_tokens_per_request: float = 0.0


@dataclass
class BudgetCheckResult:
    """预算检查结果"""
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
    """预算告警"""
    alert_id: str
    task_id: str
    alert_type: str
    threshold_percentage: float
    current_percentage: float
    timestamp: str
    message: str


class TokenBudget:
    """
    Token 预算管理
    
    遵循架构原则：
    - E-3 资源确定性：预算限制确保资源可控
    - E-1 安全内生：防止 Token 滥用和成本超支
    - A-1 简约至上：简洁的接口设计
    - S-1 反馈闭环：实时跟踪和告警
    """
    
    def __init__(
        self,
        storage_dir: Optional[str] = None,
        default_warning_threshold: float = 80.0,
        enable_alerts: bool = True,
        alert_callback: Optional[Callable[[BudgetAlert], None]] = None
    ):
        """
        初始化 Token 预算管理
        
        Args:
            storage_dir: 存储目录
            default_warning_threshold: 默认告警阈值（百分比）
            enable_alerts: 是否启用告警
            alert_callback: 告警回调函数
        """
        self.storage_dir = storage_dir or "/var/lib/agentos/budget"
        self.default_warning_threshold = default_warning_threshold
        self.enable_alerts = enable_alerts
        self.alert_callback = alert_callback
        
        self._budgets: Dict[str, BudgetConfig] = {}
        self._usage: Dict[str, BudgetUsage] = {}
        self._alerts: List[BudgetAlert] = []
        self._lock = threading.Lock()
        
        self._ensure_storage_dir()
        self._load_state()
    
    def _ensure_storage_dir(self) -> None:
        """确保存储目录存在"""
        try:
            Path(self.storage_dir).mkdir(parents=True, exist_ok=True)
            logger.info(f"Token budget storage directory ensured: {self.storage_dir}")
        except OSError as e:
            logger.error(f"Failed to create storage directory: {e}")
            raise
    
    def set_budget(
        self,
        task_id: str,
        max_tokens: int,
        max_cost_usd: float,
        warning_threshold: Optional[float] = None,
        auto_renew: bool = False,
        renewal_period_days: int = 30
    ) -> bool:
        """
        设置任务 Token 预算
        
        Args:
            task_id: 任务 ID
            max_tokens: 最大 Token 数
            max_cost_usd: 最大成本（USD）
            warning_threshold: 告警阈值（百分比）
            auto_renew: 是否自动续期
            renewal_period_days: 续期周期（天）
            
        Returns:
            bool: 是否设置成功
        """
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
            
            logger.info(
                f"Budget set for task {task_id}: {max_tokens} tokens, "
                f"${max_cost_usd:.2f}"
            )
            
            return True
            
        except Exception as e:
            logger.error(f"Failed to set budget: {e}")
            return False
    
    def _calculate_usage_percentages(
        self,
        config: BudgetConfig,
        usage: BudgetUsage
    ) -> tuple:
        """计算使用百分比"""
        token_pct = (usage.used_tokens / config.max_tokens * 100) if config.max_tokens > 0 else 0
        cost_pct = (usage.used_cost_usd / config.max_cost_usd * 100) if config.max_cost_usd > 0 else 0
        return token_pct, cost_pct
    
    def _determine_budget_status(
        self,
        percentage: float,
        config: BudgetConfig
    ) -> tuple:
        """确定预算状态（简化条件逻辑）"""
        thresholds = [
            (config.hard_limit_percentage, BudgetStatus.EXHAUSTED, True, "Budget exhausted (100% used)"),
            (config.soft_limit_percentage, BudgetStatus.OVER_BUDGET, True, f"Budget exceeded ({percentage:.1f}% used)"),
            (config.warning_threshold_percentage, BudgetStatus.WARNING, False, f"Budget warning ({percentage:.1f}% used)")
        ]
        
        for threshold, status, over_budget, msg in thresholds:
            if percentage >= threshold:
                return status, over_budget, msg
        
        return BudgetStatus.WITHIN_BUDGET, False, f"Budget OK ({percentage:.1f}% used)"
    
    def _handle_budget_alerts(
        self,
        task_id: str,
        status: BudgetStatus,
        percentage: float,
        config: BudgetConfig
    ) -> None:
        """处理预算告警"""
        if not self.enable_alerts:
            return
        
        alert_type = None
        if status in (BudgetStatus.OVER_BUDGET, BudgetStatus.EXHAUSTED):
            alert_type = "over_budget"
        elif status == BudgetStatus.WARNING:
            alert_type = "warning"
        
        if alert_type:
            self._trigger_alert(task_id, alert_type, percentage, config)
    
    def check_budget(self, task_id: str) -> BudgetCheckResult:
        """
        检查预算使用情况
        
        Args:
            task_id: 任务 ID
            
        Returns:
            BudgetCheckResult: 预算检查结果
        """
        with self._lock:
            if task_id not in self._budgets:
                return self._create_empty_result(task_id)
            
            config = self._budgets[task_id]
            usage = self._usage.get(task_id, BudgetUsage(task_id=task_id))
            
            remaining_tokens = max(0, config.max_tokens - usage.used_tokens)
            remaining_cost_usd = max(0.0, config.max_cost_usd - usage.used_cost_usd)
            
            token_pct, cost_pct = self._calculate_usage_percentages(config, usage)
            max_percentage = max(token_pct, cost_pct)
            
            status, is_over_budget, message = self._determine_budget_status(max_percentage, config)
            
            self._handle_budget_alerts(task_id, status, max_percentage, config)
            
            return BudgetCheckResult(
                task_id=task_id,
                status=status,
                used_tokens=usage.used_tokens,
                max_tokens=config.max_tokens,
                used_cost_usd=usage.used_cost_usd,
                max_cost_usd=config.max_cost_usd,
                remaining_tokens=remaining_tokens,
                remaining_cost_usd=remaining_cost_usd,
                remaining_percentage=100.0 - max_percentage,
                is_over_budget=is_over_budget,
                is_warning=(status == BudgetStatus.WARNING),
                message=message
            )
    
    def record_usage(
        self,
        task_id: str,
        tokens: int,
        cost_usd: float = 0.0
    ) -> BudgetCheckResult:
        """
        记录 Token 使用
        
        Args:
            task_id: 任务 ID
            tokens: 使用的 Token 数
            cost_usd: 成本（USD）
            
        Returns:
            BudgetCheckResult: 记录后的预算状态
        """
        with self._lock:
            if task_id not in self._usage:
                self._usage[task_id] = BudgetUsage(task_id=task_id)
            
            usage = self._usage[task_id]
            usage.used_tokens += tokens
            usage.used_cost_usd += cost_usd
            usage.request_count += 1
            usage.last_used_at = datetime.now().isoformat()
            usage.average_tokens_per_request = (
                usage.used_tokens / usage.request_count
            )
            
            self._save_state()
        
        return self.check_budget(task_id)
    
    def can_use_tokens(
        self,
        task_id: str,
        tokens: int,
        cost_usd: float = 0.0
    ) -> bool:
        """
        检查是否可以使用指定数量的 Token
        
        Args:
            task_id: 任务 ID
            tokens: 计划使用的 Token 数
            cost_usd: 计划使用的成本
            
        Returns:
            bool: 是否可以使用
        """
        result = self.check_budget(task_id)
        
        if result.is_over_budget:
            return False
        
        remaining_tokens = result.remaining_tokens
        remaining_cost = result.remaining_cost_usd
        
        return tokens <= remaining_tokens and cost_usd <= remaining_cost
    
    def reset_budget(self, task_id: str) -> bool:
        """
        重置预算使用统计
        
        Args:
            task_id: 任务 ID
            
        Returns:
            bool: 是否重置成功
        """
        with self._lock:
            if task_id in self._usage:
                self._usage[task_id] = BudgetUsage(task_id=task_id)
                self._save_state()
                logger.info(f"Budget reset for task {task_id}")
                return True
            else:
                logger.warning(f"No budget found for task {task_id}")
                return False
    
    def delete_budget(self, task_id: str) -> bool:
        """
        删除预算配置
        
        Args:
            task_id: 任务 ID
            
        Returns:
            bool: 是否删除成功
        """
        with self._lock:
            if task_id in self._budgets:
                del self._budgets[task_id]
            if task_id in self._usage:
                del self._usage[task_id]
            
            self._save_state()
            logger.info(f"Budget deleted for task {task_id}")
            return True
    
    def list_budgets(self) -> List[BudgetConfig]:
        """
        列出所有预算配置
        
        Returns:
            List[BudgetConfig]: 预算配置列表
        """
        with self._lock:
            return list(self._budgets.values())
    
    def get_usage_summary(
        self,
        task_id: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        获取使用汇总
        
        Args:
            task_id: 任务 ID（可选）
            
        Returns:
            Dict: 使用汇总信息
        """
        with self._lock:
            if task_id:
                usage = self._usage.get(task_id)
                if not usage:
                    return {}
                
                return {
                    "task_id": task_id,
                    "used_tokens": usage.used_tokens,
                    "used_cost_usd": usage.used_cost_usd,
                    "request_count": usage.request_count,
                    "last_used_at": usage.last_used_at
                }
            else:
                total_tokens = sum(u.used_tokens for u in self._usage.values())
                total_cost = sum(u.used_cost_usd for u in self._usage.values())
                total_requests = sum(u.request_count for u in self._usage.values())
                
                return {
                    "total_tasks": len(self._usage),
                    "total_tokens": total_tokens,
                    "total_cost_usd": total_cost,
                    "total_requests": total_requests
                }
    
    @staticmethod
    def _create_empty_result(task_id: str) -> BudgetCheckResult:
        """创建空预算结果"""
        return BudgetCheckResult(
            task_id=task_id,
            status=BudgetStatus.WITHIN_BUDGET,
            used_tokens=0,
            max_tokens=0,
            used_cost_usd=0.0,
            max_cost_usd=0.0,
            remaining_tokens=0,
            remaining_cost_usd=0.0,
            remaining_percentage=100.0,
            is_over_budget=False,
            is_warning=False,
            message="No budget configured for this task"
        )
    
    def _trigger_alert(
        self,
        task_id: str,
        alert_type: str,
        current_percentage: float,
        config: BudgetConfig
    ) -> None:
        """触发告警"""
        import uuid
        
        alert = BudgetAlert(
            alert_id=str(uuid.uuid4()),
            task_id=task_id,
            alert_type=alert_type,
            threshold_percentage=config.warning_threshold_percentage,
            current_percentage=current_percentage,
            timestamp=datetime.now().isoformat(),
            message=f"Budget {alert_type} for task {task_id}: {current_percentage:.1f}%"
        )
        
        self._alerts.append(alert)
        
        logger.warning(
            f"Budget alert triggered: {alert_type} for task {task_id} "
            f"({current_percentage:.1f}% used)"
        )
        
        if self.alert_callback:
            try:
                self.alert_callback(alert)
            except Exception as e:
                logger.error(f"Alert callback failed: {e}")
    
    def _save_state(self) -> None:
        """保存状态到文件"""
        try:
            budgets_file = os.path.join(self.storage_dir, "budgets.json")
            usage_file = os.path.join(self.storage_dir, "usage.json")
            
            budgets_data = [asdict(b) for b in self._budgets.values()]
            usage_data = [asdict(u) for u in self._usage.values()]
            
            with open(budgets_file, 'w', encoding='utf-8') as f:
                json.dump(budgets_data, f, indent=2, ensure_ascii=False)
            
            with open(usage_file, 'w', encoding='utf-8') as f:
                json.dump(usage_data, f, indent=2, ensure_ascii=False)
            
            logger.debug(f"Saved budget state: {len(self._budgets)} budgets")
            
        except Exception as e:
            logger.error(f"Failed to save budget state: {e}")
    
    def _load_state(self) -> None:
        """从文件加载状态"""
        try:
            budgets_file = os.path.join(self.storage_dir, "budgets.json")
            usage_file = os.path.join(self.storage_dir, "usage.json")
            
            if os.path.exists(budgets_file):
                with open(budgets_file, 'r', encoding='utf-8') as f:
                    budgets_data = json.load(f)
                
                self._budgets = {
                    b["task_id"]: BudgetConfig(**b) for b in budgets_data
                }
            
            if os.path.exists(usage_file):
                with open(usage_file, 'r', encoding='utf-8') as f:
                    usage_data = json.load(f)
                
                self._usage = {
                    u["task_id"]: BudgetUsage(**u) for u in usage_data
                }
            
            logger.info(
                f"Loaded budget state: {len(self._budgets)} budgets, "
                f"{len(self._usage)} usage records"
            )
            
        except Exception as e:
            logger.error(f"Failed to load budget state: {e}")
            self._budgets.clear()
            self._usage.clear()


def get_token_budget(
    storage_dir: Optional[str] = None,
    **kwargs
) -> TokenBudget:
    """
    获取全局 Token 预算管理实例
    
    Args:
        storage_dir: 存储目录
        **kwargs: 其他参数
        
    Returns:
        TokenBudget: Token 预算管理实例
    """
    global _global_token_budget
    
    if '_global_token_budget' not in globals():
        _global_token_budget = TokenBudget(
            storage_dir=storage_dir,
            **kwargs
        )
    
    return _global_token_budget
