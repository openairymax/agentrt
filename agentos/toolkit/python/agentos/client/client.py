# AgentOS Python SDK - Client Implementation
# Version: 3.0.0
# Last updated: 2026-04-05

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
    AgentOSTimeoutError,
    InvalidResponseError,
    ServerError,
    RateLimitError,
    http_status_to_error,
)

T = TypeVar('T')
logger = logging.getLogger(__name__)

MAX_RESPONSE_BODY_SIZE = 10 * 1024 * 1024


class _RetryableError(Exception):
    """内部异常，标记可重试的错误"""
    def __init__(self, original_error: Exception):
        self.original_error = original_error
        super().__init__(str(original_error))


@dataclass
class ClientConfig:
    """
    客户端配置
    
    对应 Go SDK: agentos/manager.go
    """
    endpoint: str = "http://localhost:18789"
    timeout: float = 30.0
    max_retries: int = 3
    retry_delay: float = 1.0
    max_connections: int = 100
    idle_conn_timeout: float = 90.0
    api_key: Optional[str] = None
    user_agent: str = "AgentOS-Python-tools/3.0.0"
    headers: Dict[str, str] = field(default_factory=dict)

    def validate(self) -> None:
        """验证配置有效性"""
        if not self.endpoint:
            raise AgentOSError("端点地址不能为空")
        if not self.endpoint.startswith(("http://", "https://")):
            raise AgentOSError("端点地址必须以 http:// 或 https:// 开头")
        if self.timeout <= 0:
            raise AgentOSError("超时时间必须大于 0")
        if self.max_retries < 0:
            raise AgentOSError("最大重试次数不能为负数")

    def clone(self) -> 'ClientConfig':
        """创建配置副本"""
        return ClientConfig(
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
    """单次请求的可选参数"""
    timeout: Optional[float] = None
    headers: Dict[str, str] = field(default_factory=dict)
    query_params: Dict[str, str] = field(default_factory=dict)


@dataclass
class APIResponse:
    """通用 API 响应结构"""
    success: bool
    data: Any = None
    message: str = ""

    @classmethod
    def from_dict(cls, d: Dict[str, Any]) -> 'APIResponse':
        """从字典创建响应对象"""
        return cls(
            success=d.get("success", False),
            data=d.get("data"),
            message=d.get("message", "")
        )


@dataclass
class HealthStatus:
    """健康检查返回状态"""
    status: str
    version: str
    uptime: int
    checks: Dict[str, str]
    timestamp: float


@dataclass
class Metrics:
    """系统运行指标快照"""
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
    """APIClient 接口定义"""

    @abstractmethod
    def get(self, path: str, opts: Optional[RequestOptions] = None) -> APIResponse:
        """执行 HTTP GET 请求"""
        pass

    @abstractmethod
    def post(self, path: str, body: Any = None, opts: Optional[RequestOptions] = None) -> APIResponse:
        """执行 HTTP POST 请求"""
        pass

    @abstractmethod
    def put(self, path: str, body: Any = None, opts: Optional[RequestOptions] = None) -> APIResponse:
        """执行 HTTP PUT 请求"""
        pass

    @abstractmethod
    def delete(self, path: str, opts: Optional[RequestOptions] = None) -> APIResponse:
        """执行 HTTP DELETE 请求"""
        pass


class Client(APIClient):
    """AgentOS Python SDK 核心客户端"""

    def __init__(self, config: Optional[ClientConfig] = None, **kwargs):
        """初始化客户端"""
        if config is None:
            config = ClientConfig()

        for key, value in kwargs.items():
            if hasattr(config, key):
                setattr(config, key, value)

        config.validate()
        self._config = config
        self._session = self._create_session()

    def _create_session(self) -> requests.Session:
        """创建带重试策略的 Session"""
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
    def config(self) -> ClientConfig:
        """获取配置副本"""
        return self._config.clone()

    @property
    def endpoint(self) -> str:
        """获取端点地址"""
        return self._config.endpoint

    def health(self) -> HealthStatus:
        """检查 AgentOS 服务的健康状态"""
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
        """获取 AgentOS 系统运行指标"""
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
        """关闭客户端"""
        if self._session:
            self._session.close()

    def __enter__(self) -> 'Client':
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        self.close()

    def __repr__(self) -> str:
        return f"AgentOS Client[endpoint={self._config.endpoint}, timeout={self._config.timeout}]"

    def _build_url(self, path: str, query_params: Optional[Dict[str, str]] = None) -> str:
        """构建完整 URL"""
        url = self._config.endpoint.rstrip('/') + path
        if query_params:
            params = "&".join(f"{k}={v}" for k, v in query_params.items())
            url = f"{url}?{params}"
        return url

    def _calculate_backoff(self, attempt: int) -> float:
        """计算指数退避延迟"""
        backoff = self._config.retry_delay * (2 ** (attempt - 1))
        jitter = random.uniform(0, backoff)
        return backoff + jitter

    def _should_retry(self, status_code: int) -> bool:
        """判断是否应重试"""
        return status_code >= 500 or status_code == 429

    def _generate_request_id(self) -> str:
        """生成唯一的请求 ID"""
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
        """执行底层 HTTP 请求"""
        request_config = self._prepare_request_config(method, path, body, opts)
        last_error = None
        
        for attempt in range(self._config.max_retries + 1):
            if attempt > 0:
                self._handle_retry_delay(attempt)
            
            try:
                return self._execute_single_request(request_config)
            except Exception as e:
                last_error = self._classify_request_error(e, request_config)
                if not self._is_retryable_error(last_error):
                    raise last_error
        
        raise last_error or NetworkError("未知网络错误")
    
    def _prepare_request_config(
        self,
        method: str,
        path: str,
        body: Any,
        opts: Optional[RequestOptions]
    ) -> Dict[str, Any]:
        """准备请求配置"""
        opts = opts or RequestOptions()
        url = self._build_url(path, opts.query_params)
        timeout = opts.timeout if opts.timeout else self._config.timeout
        
        headers = dict(opts.headers) if opts.headers else {}
        headers["X-Request-ID"] = self._generate_request_id()
        
        json_body = json.dumps(body) if body is not None else None
        
        return {
            'method': method,
            'path': path,
            'url': url,
            'headers': headers,
            'timeout': timeout,
            'json_body': json_body,
        }
    
    def _handle_retry_delay(self, attempt: int):
        """处理重试延迟"""
        delay = self._calculate_backoff(attempt)
        logger.warning(
            f"请求失败，{delay:.2f}s 后重试 (尝试 {attempt}/{self._config.max_retries})"
        )
        time.sleep(delay)
    
    def _execute_single_request(self, request_config: Dict[str, Any]) -> APIResponse:
        """执行单次请求"""
        response = self._session.request(
            method=request_config['method'],
            url=request_config['url'],
            data=request_config['json_body'],
            headers=request_config['headers'] if request_config['headers'] else None,
            timeout=request_config['timeout'],
        )
        
        if response.status_code >= 400:
            error = http_status_to_error(response.status_code, response.text)
            if not self._should_retry(response.status_code):
                raise error
            raise _RetryableError(error)
        
        if len(response.content) > MAX_RESPONSE_BODY_SIZE:
            raise InvalidResponseError("响应体超过最大限制")
        
        try:
            data = response.json()
        except json.JSONDecodeError as e:
            raise InvalidResponseError("响应 JSON 解析失败") from e
        
        return APIResponse.from_dict(data)
    
    def _classify_request_error(self, error: Exception, request_config: Dict[str, Any]) -> Exception:
        """分类请求错误"""
        if isinstance(error, requests.Timeout):
            return AgentOSTimeoutError(
                operation=f"{request_config['method']} {request_config['path']} (timeout={request_config['timeout']}s)"
            )
        elif isinstance(error, requests.ConnectionError):
            return NetworkError(f"连接错误：{error}")
        elif isinstance(error, requests.RequestException):
            return NetworkError(f"请求错误：{error}")
        elif isinstance(error, _RetryableError):
            return error.original_error
        return error
    
    def _is_retryable_error(self, error: Exception) -> bool:
        """判断错误是否可重试"""
        return isinstance(error, _RetryableError)

    def get(self, path: str, opts: Optional[RequestOptions] = None) -> APIResponse:
        """执行 HTTP GET 请求"""
        return self._request("GET", path, None, opts)

    def post(self, path: str, body: Any = None, opts: Optional[RequestOptions] = None) -> APIResponse:
        """执行 HTTP POST 请求"""
        return self._request("POST", path, body, opts)

    def put(self, path: str, body: Any = None, opts: Optional[RequestOptions] = None) -> APIResponse:
        """执行 HTTP PUT 请求"""
        return self._request("PUT", path, body, opts)

    def delete(self, path: str, opts: Optional[RequestOptions] = None) -> APIResponse:
        """执行 HTTP DELETE 请求"""
        return self._request("DELETE", path, None, opts)
