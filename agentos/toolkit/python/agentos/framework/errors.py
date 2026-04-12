# AgentOS Error Handling Framework
# Version: 2.0.0
# Last updated: 2026-04-11

"""
统一错误处理框架

提供结构化的错误处理、分类、恢复策略和日志记录能力。
确保所有组件的错误处理行为一致，提升系统的健壮性和可维护性。

设计原则:
1. 分级处理 - 不同严重级别的错误采用不同策略
2. 上下文感知 - 错误信息包含完整的上下文信息
3. 可恢复性 - 支持自动恢复和降级机制
4. 可观测性 - 完整的错误追踪和分析
"""

import asyncio
import enum
import logging
import traceback
import uuid
from dataclasses import dataclass, field
from datetime import datetime
from typing import Any, Callable, Dict, List, Optional, Type, TypeVar, Union

logger = logging.getLogger(__name__)

T = TypeVar('T')


class ErrorLevel(enum.Enum):
    """错误级别枚举"""
    INFO = "info"               # 信息性提示（非错误）
    WARNING = "warning"         # 警告（可能的问题）
    RECOVERABLE = "recoverable" # 可恢复错误（可以重试）
    FATAL = "fatal"             # 致命错误（需要人工干预）


@dataclass
class ErrorContext:
    """错误上下文信息"""
    error_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    timestamp: datetime = field(default_factory=datetime.now)
    level: ErrorLevel = ErrorLevel.RECOVERABLE
    error_type: str = ""
    message: str = ""
    stack_trace: str = ""
    context_data: Dict[str, Any] = field(default_factory=dict)

    # 恢复相关信息
    recovery_actions: List[str] = field(default_factory=list)
    retry_count: int = 0
    original_exception: Optional[Exception] = None

    def to_dict(self) -> Dict[str, Any]:
        """转换为可序列化的字典"""
        return {
            "error_id": self.error_id,
            "timestamp": self.timestamp.isoformat(),
            "level": self.level.value,
            "error_type": self.error_type,
            "message": self.message,
            "stack_trace": self.stack_trace[:1000] if self.stack_trace else "",  # 截断过长的堆栈
            "context_data": self.context_data,
            "recovery_actions": self.recovery_actions,
            "retry_count": self.retry_count
        }

    def __str__(self) -> str:
        return f"[{self.level.value.upper()}] {self.error_type}: {self.message}"


@dataclass
class RecoveryStrategy:
    """恢复策略定义"""
    strategy_name: str
    max_retries: int = 3
    initial_delay: float = 1.0
    max_delay: float = 30.0
    backoff_factor: float = 2.0
    jitter: bool = True
    retryable_exceptions: List[Type[Exception]] = field(default_factory=list)

    def should_retry(self, error: Exception, current_attempt: int) -> bool:
        """
        判断是否应该重试

        Args:
            error: 发生的异常
            current_attempt: 当前尝试次数

        Returns:
            是否应该重试
        """
        if current_attempt >= self.max_retries:
            return False

        if self.retryable_exceptions:
            return any(isinstance(error, exc_type) for exc_type in self.retryable_exceptions)

        return True

    def get_delay(self, attempt: int) -> float:
        """
        计算下次重试的延迟时间

        Args:
            attempt: 当前尝试次数（从0开始）

        Returns:
            延迟时间（秒）
        """
        import random

        delay = min(
            self.initial_delay * (self.backoff_factor ** attempt),
            self.max_delay
        )

        if self.jitter:
            delay *= (0.5 + random.random())

        return delay


@dataclass
class ErrorResult:
    """错误处理结果"""
    success: bool
    result: Any = None
    error_context: Optional[ErrorContext] = None
    recovery_applied: bool = False
    recovery_strategy_used: Optional[str] = None


class ErrorHandlingFramework:
    """
    统一错误处理框架

    提供完整的错误处理能力：
    - 结构化的错误捕获和记录
    - 基于级别的自动处理策略
    - 可配置的恢复和重试机制
    - 错误统计和分析
    - 全局和局部的错误处理器注册

    使用示例:
        framework = ErrorHandlingFramework()

        # 注册特定类型错误的处理器
        @framework.register_handler(NetworkError)
        async def handle_network_error(error):
            return await retry_with_backoff(error)

        # 使用框架执行代码
        result = await framework.execute_with_error_handling(
            risky_operation,
            fallback_result="default_value"
        )
    """

    def __init__(
        self,
        default_recovery_strategy: Optional[RecoveryStrategy] = None,
        enable_logging: bool = True,
        enable_metrics: bool = True
    ):
        """
        初始化错误处理框架

        Args:
            default_recovery_strategy: 默认恢复策略
            enable_logging: 是否启用错误日志
            enable_metrics: 是否启用错误指标收集
        """
        self._handlers: Dict[str, Callable] = {}
        self._recovery_strategies: Dict[str, RecoveryStrategy] = {}
        self._default_strategy: RecoveryStrategy = default_recovery_strategy or RecoveryStrategy()
        self._enable_logging: bool = enable_logging
        self._enable_metrics: bool = enable_metrics

        # 统计数据
        self._error_counts: Dict[str, int] = {}
        self._recovery_counts: Dict[str, int] = {}
        self._error_history: List[ErrorContext] = []
        self._max_history_size: int = 500

        logger.info("ErrorHandlingFramework initialized")

    async def handle_error(
        self,
        error: Exception,
        context: Optional[Dict[str, Any]] = None,
        level: ErrorLevel = ErrorLevel.RECOVERABLE
    ) -> ErrorContext:
        """
        处理错误并创建上下文对象

        Args:
            error: 捕获到的异常
            context: 额外的上下文信息
            level: 错误级别

        Returns:
            错误上下文对象
        """
        error_context = ErrorContext(
            level=level,
            error_type=type(error).__name__,
            message=str(error),
            stack_trace=traceback.format_exc(),
            context_data=context or {},
            original_exception=error
        )

        # 确定恢复动作
        error_context.recovery_actions = self._get_recovery_actions(error_context)

        # 记录错误
        if self._enable_logging:
            self._log_error(error_context)

        # 更新统计
        if self._enable_metrics:
            self._update_stats(error_context)

        # 尝试调用特定处理器
        handler = self._find_handler(error)
        if handler:
            try:
                if asyncio.iscoroutinefunction(handler):
                    await handler(error_context)
                else:
                    handler(error_context)
                error_context.recovery_actions.append("handler_executed")
            except Exception as handler_error:
                logger.error(f"Error handler failed: {handler_error}")

        return error_context

    async def execute_with_error_handling(
        self,
        func: Callable[..., T],
        *args,
        **kwargs
    ) -> Union[T, ErrorResult]:
        """
        带错误处理的执行包装器

        Args:
            func: 要执行的函数
            *args, **kwargs: 函数参数

        Returns:
            执行结果或错误结果
        """
        try:
            if asyncio.iscoroutinefunction(func):
                result = await func(*args, **kwargs)
            else:
                result = func(*args, **kwargs)

            return result

        except Exception as e:
            error_ctx = await self.handle_error(e)

            # 尝试恢复
            recovery_result = await self._attempt_recovery(func, args, kwargs, error_ctx)

            return ErrorResult(
                success=recovery_result is not None,
                result=recovery_result,
                error_context=error_ctx,
                recovery_applied=(recovery_result is not None),
                recovery_strategy_used=self._default_strategy.strategy_name if recovery_result else None
            )

    async def execute_with_retry(
        self,
        func: Callable[..., T],
        strategy: Optional[RecoveryStrategy] = None,
        *args,
        **kwargs
    ) -> T:
        """
        带重试机制的执行

        Args:
            func: 要执行的函数
            strategy: 重试策略（可选，使用默认策略如果未提供）
            *args, **kwargs: 函数参数

        Returns:
            执行结果

        Raises:
            最后一次尝试的异常
        """
        strategy = strategy or self._default_strategy
        last_error: Optional[Exception] = None

        for attempt in range(strategy.max_retries + 1):
            try:
                if asyncio.iscoroutinefunction(func):
                    return await func(*args, **kwargs)
                else:
                    return func(*args, **kwargs)

            except Exception as e:
                last_error = e

                # 检查是否应该重试
                if not strategy.should_retry(e, attempt):
                    break

                # 创建错误上下文
                error_ctx = ErrorContext(
                    level=ErrorLevel.RECOVERABLE,
                    error_type=type(e).__name__,
                    message=str(e),
                    retry_count=attempt,
                    original_exception=e
                )

                if self._enable_logging:
                    logger.warning(
                        f"Attempt {attempt + 1}/{strategy.max_retries + 1} failed: {e}. "
                        f"Retrying in {strategy.get_delay(attempt):.2f}s..."
                    )

                # 等待后重试
                await asyncio.sleep(strategy.get_delay(attempt))

        # 所有重试都失败
        if last_error:
            await self.handle_error(last_error, {"total_attempts": strategy.max_retries + 1})

        raise last_error

    def register_handler(self, error_class_or_name: Union[Type[Exception], str], handler: Callable) -> None:
        """
        注册特定类型的错误处理器

        Args:
            error_class_or_name: 异常类或名称字符串
            handler: 处理回调函数
        """
        name = error_class_or_name if isinstance(error_class_or_name, str) else error_class_or_name.__name__
        self._handlers[name] = handler
        logger.debug(f"Registered error handler for: {name}")

    def unregister_handler(self, error_class_or_name: Union[Type[Exception], str]) -> bool:
        """
        注销错误处理器

        Returns:
            是否成功移除
        """
        name = error_class_or_name if isinstance(error_class_or_name, str) else error_class_or_name.__name__
        if name in self._handlers:
            del self._handlers[name]
            return True
        return False

    def register_recovery_strategy(self, name: str, strategy: RecoveryStrategy) -> None:
        """
        注册命名恢复策略

        Args:
            name: 策略名称
            strategy: 策略定义
        """
        self._recovery_strategies[name] = strategy
        logger.debug(f"Registered recovery strategy: {name}")

    def get_recovery_strategy(self, name: str) -> Optional[RecoveryStrategy]:
        """获取指定名称的恢复策略"""
        return self._recovery_strategies.get(name)

    def get_error_statistics(self) -> Dict[str, Any]:
        """
        获取错误统计信息

        Returns:
            包含统计数据字典
        """
        total_errors = sum(self._error_counts.values())
        total_recoveries = sum(self._recovery_counts.values())

        return {
            "total_errors": total_errors,
            "total_recoveries": total_recoveries,
            "recovery_rate": (total_recoveries / total_errors * 100) if total_errors > 0 else 0,
            "errors_by_type": dict(self._error_counts),
            "recoveries_by_strategy": dict(self._recovery_counts),
            "registered_handlers": list(self._handlers.keys()),
            "registered_strategies": list(self._recovery_strategies.keys()),
            "history_size": len(self._error_history)
        }

    def get_recent_errors(self, limit: int = 20, level_filter: Optional[ErrorLevel] = None) -> List[ErrorContext]:
        """
        获取最近的错误记录

        Args:
            limit: 最大返回数量
            level_filter: 级别过滤

        Returns:
            错误上下文列表
        """
        errors = reversed(self._error_history[-limit:])

        if level_filter:
            errors = [e for e in errors if e.level == level_filter]

        return list(errors)

    def clear_history(self) -> None:
        """清空错误历史"""
        self._error_history.clear()
        self._error_counts.clear()
        self._recovery_counts.clear()
        logger.info("Error history cleared")

    def _find_handler(self, error: Exception) -> Optional[Callable]:
        """查找匹配的错误处理器"""
        for class_name in [type(error).__name__] + [base.__name__ for base in type(error).__mro__[:-1]]:
            if class_name in self._handlers:
                return self._handlers[class_name]
        return None

    async def _attempt_recovery(
        self,
        func: Callable,
        args: tuple,
        kwargs: dict,
        error_ctx: ErrorContext
    ) -> Optional[Any]:
        """
        尝试从错误中恢复

        Returns:
            恢复后的结果，或None表示无法恢复
        """
        strategy = self._default_strategy

        if not strategy.should_retry(error_ctx.original_exception, 0):
            return None

        try:
            result = await self.execute_with_retry(func, strategy, *args, **kwargs)

            # 记录成功恢复
            self._recovery_counts[strategy.strategy_name] = \
                self._recovery_counts.get(strategy.strategy_name, 0) + 1

            return result

        except Exception as final_error:
            logger.error(f"Recovery failed: {final_error}")
            return None

    def _get_recovery_actions(self, ctx: ErrorContext) -> List[str]:
        """根据错误上下文生成建议的恢复动作"""
        actions = []

        if ctx.level == ErrorLevel.INFO:
            actions.append("log_and_continue")
        elif ctx.level == ErrorLevel.WARNING:
            actions.extend(["log_warning", "continue_with_caution"])
        elif ctx.level == ErrorLevel.RECOVERABLE:
            actions.extend(["retry_operation", "use_fallback", "notify_user"])
        elif ctx.level == ErrorLevel.FATAL:
            actions.extend(["stop_operation", "alert_administrator", "save_state"])

        # 根据错误类型添加特定建议
        error_type_lower = ctx.error_type.lower()
        if "network" in error_type_lower or "connection" in error_type_lower:
            actions.append("check_network_connection")
        elif "timeout" in error_type_lower:
            actions.append("increase_timeout")
        elif "permission" in error_type_lower or "auth" in error_type_lower:
            actions.append("check_permissions")
        elif "memory" in error_type_lower or "resource" in error_type_lower:
            actions.append("free_resources")

        return actions

    def _log_error(self, ctx: ErrorContext) -> None:
        """根据级别记录错误"""
        log_message = f"{ctx} | ID: {ctx.error_id}"

        if ctx.level == ErrorLevel.INFO:
            logger.info(log_message)
        elif ctx.level == ErrorLevel.WARNING:
            logger.warning(log_message)
        elif ctx.level == ErrorLevel.RECOVERABLE:
            logger.error(log_message)
        elif ctx.level == ErrorLevel.FATAL:
            logger.critical(log_message)

    def _update_stats(self, ctx: ErrorContext) -> None:
        """更新统计数据"""
        key = ctx.error_type
        self._error_counts[key] = self._error_counts.get(key, 0) + 1

        # 记录到历史
        self._error_history.append(ctx)

        # 限制历史大小
        if len(self._error_history) > self._max_history_size:
            self._error_history = self._error_history[-self._max_history_size:]


# 预定义的常用恢复策略
DEFAULT_RETRY_STRATEGY = RecoveryStrategy(
    strategy_name="default_retry",
    max_retries=3,
    initial_delay=1.0,
    max_delay=10.0,
    backoff_factor=2.0,
    jitter=True
)

NETWORK_RETRY_STRATEGY = RecoveryStrategy(
    strategy_name="network_retry",
    max_retries=5,
    initial_delay=0.5,
    max_delay=30.0,
    backoff_factor=2.0,
    jitter=True,
    retryable_exceptions=[ConnectionError, TimeoutError]
)

NO_RETRY_STRATEGY = RecoveryStrategy(
    strategy_name="no_retry",
    max_retries=0
)


__all__ = [
    "ErrorHandlingFramework",
    "ErrorLevel",
    "ErrorContext",
    "RecoveryStrategy",
    "ErrorResult",
    # 预定义策略
    "DEFAULT_RETRY_STRATEGY",
    "NETWORK_RETRY_STRATEGY",
    "NO_RETRY_STRATEGY",
]
