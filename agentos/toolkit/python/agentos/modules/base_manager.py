# AgentOS Python SDK - Base Manager Implementation
# Version: 3.0.0
# Last updated: 2026-04-05
#
# 所有 Manager 的基类，提供统一的初始化和配置
# 遵循 DRY 原则和 ARCHITECTURAL_PRINCIPLES.md E-3（资源确定性）

import logging
from abc import ABC, abstractmethod
from typing import Any, Dict, Optional

from ..client.client import APIClient, manager, RequestOptions
from ..utils import InputValidator


class BaseManager(ABC):
    """
    所有 Manager 的基类，提供统一的初始化和配置
    
    遵循 ARCHITECTURAL_PRINCIPLES.md:
        - K-2 (接口契约化): 明确的接口定义
        - E-3 (资源确定性): 资源生命周期明确
        - A-1 (简约至上): 最小化公共接口
    
    Subclasses:
        - TaskManager
        - MemoryManager
        - SessionManager
        - SkillManager
    
    Example:
        >>> class TaskManager(BaseManager):
        ...     def submit(self, content: str) -> Task:
        ...         self._validate_required_params(content=content)
        ...         req_opts = self._build_request_options()
        ...         return self._api.post("/tasks", json={"content": content})
    """
    
    def __init__(self, client: APIClient, config: Optional[manager] = None):
        """
        初始化 Manager
        
        Args:
            client: APIClient 实例
            config: 可选的配置对象
        """
        self._api = client
        self._config = config or manager()
        self._logger = logging.getLogger(self.__class__.__name__)
        self._validator = InputValidator()
    
    def _validate_required_params(self, **kwargs):
        """
        验证必需参数
        
        Args:
            **kwargs: 参数键值对
        
        Raises:
            AgentOSError: 参数验证失败
        
        Example:
            >>> self._validate_required_params(content="test", priority=5)
        """
        for key, value in kwargs.items():
            if value is None:
                from ..exceptions import AgentOSError, CODE_MISSING_PARAMETER
                raise AgentOSError(
                    CODE_MISSING_PARAMETER,
                    f"缺少必需参数: {key}"
                )
            
            # 使用验证器进行类型检查
            validate_method = getattr(self._validator, f'validate_{key}', None)
            if validate_method and value is not None:
                validate_method(value)
    
    def _build_request_options(self, **kwargs) -> RequestOptions:
        """
        构建请求选项
        
        Args:
            **kwargs: 额外的请求选项
        
        Returns:
            RequestOptions: 请求选项对象
        
        Example:
            >>> opts = self._build_request_options(timeout=60)
        """
        headers = self._config.headers.copy() if hasattr(self._config, 'headers') else {}
        
        # 合并额外的 headers
        if 'headers' in kwargs:
            headers.update(kwargs['headers'])
        
        return RequestOptions(
            timeout=kwargs.get('timeout', self._config.timeout),
            headers=headers,
        )
    
    def _log_operation(self, operation: str, **context):
        """
        记录操作日志
        
        Args:
            operation: 操作名称
            **context: 上下文信息
        
        Example:
            >>> self._log_operation("submit_task", task_id="123", status="pending")
        """
        self._logger.info(
            f"[{self.__class__.__name__}] {operation}",
            extra={"context": context}
        )
    
    def _log_error(self, operation: str, error: Exception, **context):
        """
        记录错误日志
        
        Args:
            operation: 操作名称
            error: 异常对象
            **context: 上下文信息
        
        Example:
            >>> try:
            ...     raise ValueError("test error")
            ... except Exception as e:
            ...     self._log_error("submit_task", e, task_id="123")
        """
        self._logger.error(
            f"[{self.__class__.__name__}] {operation} failed: {error}",
            extra={"context": context, "error": str(error)},
            exc_info=True
        )
    
    @property
    def api(self) -> APIClient:
        """
        获取 API 客户端
        
        Returns:
            APIClient: API 客户端实例
        """
        return self._api
    
    @property
    def config(self) -> manager:
        """
        获取配置对象
        
        Returns:
            manager: 配置对象
        """
        return self._config
    
    def __repr__(self) -> str:
        """
        返回 Manager 的字符串表示
        
        Returns:
            str: 字符串表示
        """
        return (
            f"<{self.__class__.__name__} "
            f"endpoint={self._config.endpoint}>"
        )
