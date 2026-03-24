# AgentOS Python SDK - Client Implementation
# Version: 3.0.0
# Last updated: 2026-03-24

"""
HTTP client implementation following Go SDK architecture.

Provides:
    - APIClient interface (abstract base class)
    - Client implementation with retry and connection pooling
    - Configuration management
    - Request/Response types
"""

import json
import logging
import random
import time
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, TypeVar

import requests
from requests.adapters import HTTPAdapter
from urllib3.util.retry import Retry

from ..exceptions import (
    AgentOSError,
    NetworkError,
    AgentOSTimeoutError,  # 使用明确的类名避免与内置 TimeoutError 冲突
    InvalidResponseError,
    ServerError,
    RateLimitError,
    http_status_to_error,
)

T = TypeVar('T')
logger = logging.getLogger(__name__)

MAX_RESPONSE_BODY_SIZE = 10 * 1024 * 1024  # 10MB


@dataclass
class Config:
    """
    客户端配置
    
    对应 Go SDK: agentos/config.go
    """
    endpoint: str = "http://localhost:18789"
    timeout: float = 30.0
    max_retries: int = 3
    retry_delay: float = 1.0
    max_connections: int = 100
    idle_conn_timeout: float = 90.0
    api_key: Optional[str] = None
    user_agent: str = "AgentOS-Python-SDK/3.0.0"
    headers: Dict[str, str] = field(default_factory=dict)

    def validate(self) -> None:
        """
        验证配置有效性
        
        Raises:
            AgentOSError: 配置无效时抛出
        """
        if not self.endpoint:
            raise AgentOSError("端点地址不能为空")
        if not self.endpoint.startswith(("http://", "https://")):
            raise AgentOSError("端点地址必须以 http:// 或 https:// 开头")
        if self.timeout <= 0:
            raise AgentOSError("超时时间必须大于 0")
        if self.max_retries < 0:
            raise AgentOSError("最大重试次数不能为负数")

    def clone(self) -> 'Config':
        """
        创建配置副本
        
        Returns:
            Config: 配置副本
        """
        return Config(
            endpoint=self.endpoint,
            timeout=self.timeout,
            max_retries=self.max_retries,
            retry_delay=self.retry_delay,
            max_connections=self.max_connections,
            idle_conn_timeout=self.idle_conn_timeout,
            api_key=self.api_key,
            user_agent=self.user_agent,
            headers=dict(self.headers),
        )


@dataclass
class RequestOptions:
    """
    单次请求的可选参数
    
    对应 Go SDK: types/types.go RequestOptions
    """
    timeout: Optional[float] = None
    headers: Dict[str, str] = field(default_factory=dict)
    query_params: Dict[str, str] = field(default_factory=dict)


@dataclass
class APIResponse:
    """
    通用 API 响应结构
    
    对应 Go SDK: types/types.go APIResponse
    """
    success: bool
    data: Any = None
    message: str = ""

    @classmethod
    def from_dict(cls, d: Dict[str, Any]) -> 'APIResponse':
        """
        从字典创建响应对象
        
        Args:
            d: 响应字典
            
        Returns:
            APIResponse: 响应对象
        """
        return cls(
            success=d.get("success", False),
            data=d.get("data"),
            message=d.get("message", "")
        )


@dataclass
class HealthStatus:
    """
    健康检查返回状态
    
    对应 Go SDK: types/types.go HealthStatus
    """
    status: str
    version: str
    uptime: int
    checks: Dict[str, str]
    timestamp: float


@dataclass
class Metrics:
    """
    系统运行指标快照
    
    对应 Go SDK: types/types.go Metrics
    """
    tasks_total: int = 0
    tasks_completed: int = 0
    tasks_failed: int = 0
    memories_total: int = 0
    sessions_active: int = 0
    skills_loaded: int = 0
    cpu_usage: float = 0.0
    memory_usage: float = 0.0
    request_count: int = 0
    average_latency_ms: float = 0.0


class APIClient(ABC):
    """
    APIClient 接口定义
    
    对应 Go SDK: client/client.go APIClient interface
    
    所有 Manager 共同依赖的 HTTP 通信接口，实现依赖倒转。
    """

    @abstractmethod
    def get(self, path: str, opts: Optional[RequestOptions] = None) -> APIResponse:
        """
        执行 HTTP GET 请求
        
        Args:
            path: API 路径
            opts: 请求选项
            
        Returns:
            APIResponse: API 响应
        """
        pass

    @abstractmethod
    def post(self, path: str, body: Any = None, opts: Optional[RequestOptions] = None) -> APIResponse:
        """
        执行 HTTP POST 请求
        
        Args:
            path: API 路径
            body: 请求体
            opts: 请求选项
            
        Returns:
            APIResponse: API 响应
        """
        pass

    @abstractmethod
    def put(self, path: str, body: Any = None, opts: Optional[RequestOptions] = None) -> APIResponse:
        """
        执行 HTTP PUT 请求
        
        Args:
            path: API 路径
            body: 请求体
            opts: 请求选项
            
        Returns:
            APIResponse: API 响应
        """
        pass

    @abstractmethod
    def delete(self, path: str, opts: Optional[RequestOptions] = None) -> APIResponse:
        """
        执行 HTTP DELETE 请求
        
        Args:
            path: API 路径
            opts: 请求选项
            
        Returns:
            APIResponse: API 响应
        """
        pass


class Client(APIClient):
    """
    AgentOS Python SDK 核心客户端
    
    对应 Go SDK: client/client.go Client
    
    Features:
        - Connection pooling with configurable limits
        - Automatic retry with exponential backoff
        - Request timeout management
        - API Key authentication
        - Health check and metrics endpoints
    """

    def __init__(self, config: Optional[Config] = None, **kwargs):
        """
        初始化客户端
        
        Args:
            config: 配置对象，如果为 None 则使用默认配置
            **kwargs: 配置参数，会覆盖 config 中的对应字段
        """
        if config is None:
            config = Config()

        for key, value in kwargs.items():
            if hasattr(config, key):
                setattr(config, key, value)

        config.validate()
        self._config = config
        self._session = self._create_session()

    def _create_session(self) -> requests.Session:
        """
        创建带重试策略的 Session
        
        Returns:
            requests.Session: 配置好的 Session 对象
        """
        session = requests.Session()

        retry_strategy = Retry(
            total=self._config.max_retries,
            backoff_factor=self._config.retry_delay,
            status_forcelist=[429, 500, 502, 503, 504],
            allowed_methods=["GET", "POST", "PUT", "DELETE"],
        )

        adapter = HTTPAdapter(
            max_retries=retry_strategy,
            pool_connections=self._config.max_connections,
            pool_maxsize=self._config.max_connections,
        )

        session.mount("http://", adapter)
        session.mount("https://", adapter)

        session.headers.update({
            "Content-Type": "application/json",
            "User-Agent": self._config.user_agent,
        })

        if self._config.api_key:
            session.headers["Authorization"] = f"Bearer {self._config.api_key}"

        for key, value in self._config.headers.items():
            session.headers[key] = value

        return session

    @property
    def config(self) -> Config:
        """
        获取配置副本
        
        Returns:
            Config: 配置副本
        """
        return self._config.clone()

    @property
    def endpoint(self) -> str:
        """
        获取端点地址
        
        Returns:
            str: 端点地址
        """
        return self._config.endpoint

    def health(self) -> HealthStatus:
        """
        检查 AgentOS 服务的健康状态
        
        Returns:
            HealthStatus: 健康状态对象
            
        Raises:
            InvalidResponseError: 响应格式异常
        """
        resp = self.get("/api/v1/health")
        if not resp.success or not isinstance(resp.data, dict):
            raise InvalidResponseError("健康检查响应格式异常")

        data = resp.data
        return HealthStatus(
            status=data.get("status", "unknown"),
            version=data.get("version", ""),
            uptime=data.get("uptime", 0),
            checks=data.get("checks", {}),
            timestamp=time.time(),
        )

    def metrics(self) -> Metrics:
        """
        获取 AgentOS 系统运行指标
        
        Returns:
            Metrics: 指标对象
            
        Raises:
            InvalidResponseError: 响应格式异常
        """
        resp = self.get("/api/v1/metrics")
        if not resp.success or not isinstance(resp.data, dict):
            raise InvalidResponseError("指标响应格式异常")

        data = resp.data
        return Metrics(
            tasks_total=data.get("tasks_total", 0),
            tasks_completed=data.get("tasks_completed", 0),
            tasks_failed=data.get("tasks_failed", 0),
            memories_total=data.get("memories_total", 0),
            sessions_active=data.get("sessions_active", 0),
            skills_loaded=data.get("skills_loaded", 0),
            cpu_usage=data.get("cpu_usage", 0.0),
            memory_usage=data.get("memory_usage", 0.0),
            request_count=data.get("request_count", 0),
            average_latency_ms=data.get("average_latency_ms", 0.0),
        )

    def close(self) -> None:
        """
        关闭客户端，释放连接池资源
        """
        if self._session:
            self._session.close()

    def __enter__(self) -> 'Client':
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        self.close()

    def __repr__(self) -> str:
        return f"AgentOS Client[endpoint={self._config.endpoint}, timeout={self._config.timeout}]"

    def _build_url(self, path: str, query_params: Optional[Dict[str, str]] = None) -> str:
        """
        构建完整 URL
        
        Args:
            path: API 路径
            query_params: 查询参数
            
        Returns:
            str: 完整 URL
        """
        url = self._config.endpoint.rstrip('/') + path
        if query_params:
            params = "&".join(f"{k}={v}" for k, v in query_params.items())
            url = f"{url}?{params}"
        return url

    def _calculate_backoff(self, attempt: int) -> float:
        """
        计算指数退避延迟（含抖动）
        
        Args:
            attempt: 重试次数
            
        Returns:
            float: 延迟秒数
        """
        backoff = self._config.retry_delay * (2 ** (attempt - 1))
        jitter = random.uniform(0, backoff)
        return backoff + jitter

    def _should_retry(self, status_code: int) -> bool:
        """
        判断是否应重试
        
        Args:
            status_code: HTTP 状态码
            
        Returns:
            bool: 是否应重试
        """
        return status_code >= 500 or status_code == 429

    def _generate_request_id(self) -> str:
        """
        生成唯一的请求 ID
        
        Returns:
            str: 请求 ID
        """
        import time
        timestamp = int(time.time() * 1000000)
        random_suffix = random.randint(0, 999999)
        return f"req-{timestamp}-{random_suffix:06d}"

    def _request(
        self,
        method: str,
        path: str,
        body: Any = None,
        opts: Optional[RequestOptions] = None
    ) -> APIResponse:
        """
        执行底层 HTTP 请求
        
        Args:
            method: HTTP 方法
            path: API 路径
            body: 请求体
            opts: 请求选项
            
        Returns:
            APIResponse: API 响应
            
        Raises:
            NetworkError: 网络错误
            TimeoutError: 超时错误
            InvalidResponseError: 响应格式错误
        """
        opts = opts or RequestOptions()
        url = self._build_url(path, opts.query_params)

        headers = dict(opts.headers) if opts.headers else {}
        timeout = opts.timeout if opts.timeout else self._config.timeout
        
        # 添加请求 ID 追踪
        request_id = self._generate_request_id()
        headers["X-Request-ID"] = request_id

        json_body = json.dumps(body) if body is not None else None

        last_error = None
        for attempt in range(self._config.max_retries + 1):
            if attempt > 0:
                delay = self._calculate_backoff(attempt)
                logger.warning(f"请求失败，{delay:.2f}s 后重试 (尝试 {attempt}/{self._config.max_retries})")
                time.sleep(delay)

            try:
                response = self._session.request(
                    method=method,
                    url=url,
                    data=json_body,
                    headers=headers if headers else None,
                    timeout=timeout,
                )

                if response.status_code >= 400:
                    error = http_status_to_error(response.status_code, response.text)
                    if not self._should_retry(response.status_code):
                        raise error
                    last_error = error
                    continue

                if len(response.content) > MAX_RESPONSE_BODY_SIZE:
                    raise InvalidResponseError("响应体超过最大限制")

                data = response.json()
                return APIResponse.from_dict(data)

            except requests.Timeout:
                last_error = AgentOSTimeoutError(timeout_ms=self.config.timeout * 1000, operation=f"{method} {path}")
            except requests.ConnectionError as e:
                last_error = NetworkError(f"连接错误: {e}")
            except requests.RequestException as e:
                last_error = NetworkError(f"请求错误: {e}")
            except json.JSONDecodeError:
                last_error = InvalidResponseError("响应 JSON 解析失败")

        if last_error:
            raise last_error
        raise NetworkError("未知网络错误")

    def get(self, path: str, opts: Optional[RequestOptions] = None) -> APIResponse:
        """
        执行 HTTP GET 请求
        
        Args:
            path: API 路径
            opts: 请求选项
            
        Returns:
            APIResponse: API 响应
        """
        return self._request("GET", path, None, opts)

    def post(self, path: str, body: Any = None, opts: Optional[RequestOptions] = None) -> APIResponse:
        """
        执行 HTTP POST 请求
        
        Args:
            path: API 路径
            body: 请求体
            opts: 请求选项
            
        Returns:
            APIResponse: API 响应
        """
        return self._request("POST", path, body, opts)

    def put(self, path: str, body: Any = None, opts: Optional[RequestOptions] = None) -> APIResponse:
        """
        执行 HTTP PUT 请求
        
        Args:
            path: API 路径
            body: 请求体
            opts: 请求选项
            
        Returns:
            APIResponse: API 响应
        """
        return self._request("PUT", path, body, opts)

    def delete(self, path: str, opts: Optional[RequestOptions] = None) -> APIResponse:
        """
        执行 HTTP DELETE 请求
        
        Args:
            path: API 路径
            opts: 请求选项
            
        Returns:
            APIResponse: API 响应
        """
        return self._request("DELETE", path, None, opts)
