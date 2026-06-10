"""
统一的 Mock 对象工厂模块

提供标准化的 Mock 对象创建接口，消除测试框架中重复的 Mock 创建逻辑。
所有测试模块应通过此工厂创建 Mock 响应对象，确保接口一致性。

关键创建方法均附带详细日志，便于排查 Mock 数据异常。

Version: 0.1.0
"""

import logging
from typing import Dict, Any, Optional
from unittest.mock import Mock, MagicMock, AsyncMock

logger = logging.getLogger(__name__)


class MockResponseConfig:
    """Mock 响应配置"""

    def __init__(
        self,
        status_code: int = 200,
        json_data: Optional[Dict[str, Any]] = None,
        text: str = "",
        headers: Optional[Dict[str, str]] = None,
        success: bool = True
    ):
        self.status_code = status_code
        self.json_data = json_data
        self.text = text
        self.headers = headers or {}
        self.success = success


class UnifiedMockFactory:
    """统一的 Mock 对象工厂

    聚合了原先分散在 base_test.py、base_test_case.py、test_helpers.py
    三处的 Mock 创建逻辑，提供唯一的标准接口。
    """

    @staticmethod
    def create_response(config: MockResponseConfig = None) -> Mock:
        """
        创建标准化的同步 Mock 响应对象。

        兼容三种原有实现的属性：
        - ok (base_test.py 风格)
        - success + data (base_test_case.py 风格)
        - text (test_helpers.py 风格)

        Args:
            config: 响应配置，为 None 时使用默认值

        Returns:
            Mock: 标准化响应对象
        """
        if config is None:
            logger.debug("create_response: 使用默认配置 (status_code=200)")
            config = MockResponseConfig()
        else:
            logger.debug(
                "create_response: 配置参数 "
                "status_code=%s, json_data keys=%s, text length=%d, "
                "headers keys=%s, success=%s",
                config.status_code,
                list(config.json_data.keys()) if config.json_data else [],
                len(config.text),
                list(config.headers.keys()),
                config.success,
            )

        response = Mock()
        response.status_code = config.status_code
        response.json.return_value = config.json_data or {}
        response.text = config.text or ""
        response.ok = config.status_code < 400
        response.success = config.success and config.status_code < 400
        response.headers = config.headers

        # data 属性：兼容 base_test_case.py 的 APIResponse 用法
        has_data_key = config.json_data is not None and "data" in config.json_data
        if has_data_key:
            response.data = config.json_data["data"]
            logger.debug(
                "create_response: data 字段从 json_data['data'] 提取, type=%s",
                type(response.data).__name__,
            )
        else:
            response.data = config.json_data or {}
            logger.debug(
                "create_response: data 字段 fallback 为 json_data 本身或空字典, type=%s",
                type(response.data).__name__,
            )

        logger.info(
            "create_response 完成 → status_code=%d, ok=%s, success=%s, "
            "data keys=%s, text='%s', headers=%s",
            response.status_code,
            response.ok,
            response.success,
            list(response.data.keys()) if isinstance(response.data, dict) else "(non-dict)",
            repr(response.text[:80]) if response.text else "(empty)",
            response.headers or "(none)",
        )

        return response

    @staticmethod
    def create_async_response(config: MockResponseConfig = None) -> AsyncMock:
        """
        创建异步 Mock 响应对象。

        Args:
            config: 响应配置

        Returns:
            AsyncMock: 异步响应对象
        """
        if config is None:
            logger.debug("create_async_response: 使用默认配置")
            config = MockResponseConfig()
        else:
            logger.debug(
                "create_async_response: status_code=%s, success=%s",
                config.status_code, config.success,
            )

        response = AsyncMock()
        response.status = config.status_code
        response.json.return_value = config.json_data or {}
        response.text = config.text or ""
        response.ok = config.status_code < 400
        response.success = config.success and config.status_code < 400
        response.headers = config.headers

        has_data_key = config.json_data is not None and "data" in config.json_data
        if has_data_key:
            response.data = config.json_data["data"]
        else:
            response.data = config.json_data or {}

        logger.info(
            "create_async_response 完成 → status=%d, ok=%s, success=%s, data_type=%s",
            response.status, response.ok, response.success, type(response.data).__name__,
        )
        return response

    @staticmethod
    def create_session(responses=None) -> Mock:
        """
        创建模拟会话对象。

        Args:
            responses: 预设的响应列表（side_effect）

        Returns:
            Mock: 模拟会话对象
        """
        session = Mock()
        if responses:
            session.get.side_effect = responses
            session.post.side_effect = responses
            session.put.side_effect = responses
            session.delete.side_effect = responses
            logger.info(
                "create_session: 已设置 side_effect, 响应数量=%d", len(responses)
            )
        else:
            logger.debug("create_session: 无预设响应 (side_effect 未设置)")

        return session

    @staticmethod
    def create_client(endpoint: str = "http://localhost:18789") -> Mock:
        """
        创建模拟客户端。

        Args:
            endpoint: 服务端点

        Returns:
            Mock: 模拟客户端对象
        """
        client = Mock()
        client.endpoint = endpoint
        client.timeout = 30
        default_response = UnifiedMockFactory.create_response()
        client.get.return_value = default_response
        client.post.return_value = default_response
        client.put.return_value = default_response
        client.delete.return_value = default_response
        logger.info(
            "create_client 完成 → endpoint=%s, timeout=%d, 默认响应已绑定到 get/post/put/delete",
            endpoint, client.timeout,
        )
        return client

    @staticmethod
    def create_config(**kwargs) -> MagicMock:
        """
        创建模拟配置对象。

        Args:
            **kwargs: 配置键值对

        Returns:
            MagicMock: 模拟配置对象
        """
        mock_config = MagicMock()
        for key, value in kwargs.items():
            setattr(mock_config, key, value)
        logger.debug(
            "create_config: 设置了 %d 个属性: %s", len(kwargs), list(kwargs.keys())
        )
        return mock_config

    @staticmethod
    def create_logger() -> MagicMock:
        """
        创建模拟日志器。

        Returns:
            MagicMock: 模拟日志器
        """
        mock_logger = MagicMock()
        mock_logger.debug = MagicMock()
        mock_logger.info = MagicMock()
        mock_logger.warning = MagicMock()
        mock_logger.error = MagicMock()
        logger.debug("create_logger: 模拟日志器已创建 (debug/info/warning/error)")
        return mock_logger
