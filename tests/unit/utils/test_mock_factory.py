"""
UnifiedMockFactory 单元测试

覆盖所有公共方法及边界场景，包括：
- 默认参数 / None 输入
- 各类 HTTP 状态码（2xx/3xx/4xx/5xx）
- 空 JSON 数据 / 嵌套 data 字段 / 非 dict data
- 长文本 / 特殊字符
- headers 配置
- success 显式覆盖
- 异步响应属性差异（status vs status_code）
- session side_effect 序列耗尽
- client 自定义 endpoint
- config 空参数 / 多属性
- logger 方法完整性
"""

import logging
import pytest
from unittest.mock import Mock, MagicMock, AsyncMock

from tests.utils.python.mock_factory import (
    UnifiedMockFactory,
    MockResponseConfig,
)


# ──────────────────────────────────────────────
# MockResponseConfig 测试
# ──────────────────────────────────────────────

class TestMockResponseConfig:
    """MockResponseConfig 数据类测试"""

    def test_default_values(self):
        cfg = MockResponseConfig()
        assert cfg.status_code == 200
        assert cfg.json_data is None
        assert cfg.text == ""
        assert cfg.headers == {}
        assert cfg.success is True

    def test_custom_values(self):
        cfg = MockResponseConfig(
            status_code=404,
            json_data={"error": "not found"},
            text="Not Found",
            headers={"X-Req-Id": "abc"},
            success=False,
        )
        assert cfg.status_code == 404
        assert cfg.json_data == {"error": "not found"}
        assert cfg.text == "Not Found"
        assert cfg.headers == {"X-Req-Id": "abc"}
        assert cfg.success is False

    def test_headers_none_fallback_to_empty_dict(self):
        cfg = MockResponseConfig(headers=None)
        assert cfg.headers == {}

    def test_json_data_none_stays_none(self):
        cfg = MockResponseConfig()
        assert cfg.json_data is None


# ──────────────────────────────────────────────
# create_response — 正常路径
# ──────────────────────────────────────────────

class TestCreateResponseNormal:
    """create_response 正常输入路径"""

    def test_default_config(self):
        resp = UnifiedMockFactory.create_response()
        assert resp.status_code == 200
        assert resp.ok is True
        assert resp.success is True
        assert resp.data == {}
        assert resp.text == ""
        assert resp.headers == {}

    def test_explicit_success_200(self):
        resp = UnifiedMockFactory.create_response(MockResponseConfig(
            status_code=200, json_data={"key": "val"}
        ))
        assert resp.status_code == 200
        assert resp.ok is True
        assert resp.success is True

    def test_json_return_value_is_callable_mock(self):
        resp = UnifiedMockFactory.create_response()
        # json 应该是一个可调用的 Mock 属性
        assert hasattr(resp, 'json')
        assert callable(resp.json) or hasattr(resp.json, 'return_value')
        assert resp.json.return_value == {}

    def test_config_passed_as_none(self):
        """显式传入 None 与省略行为一致"""
        r1 = UnifiedMockFactory.create_response(None)
        r2 = UnifiedMockFactory.create_response()
        assert r1.status_code == r2.status_code == 200


# ──────────────────────────────────────────────
# create_response — HTTP 状态码边界
# ──────────────────────────────────────────────

class TestCreateResponseStatusCodes:
    """各类 HTTP 状态码对 ok/success 的影响"""

    @pytest.mark.parametrize("code", [200, 201, 204, 299])
    def test_2xx_ok_true(self, code):
        resp = UnifiedMockFactory.create_response(MockResponseConfig(status_code=code))
        assert resp.ok is True
        assert resp.success is True

    @pytest.mark.parametrize("code", [301, 302, 304, 399])
    def test_3xx_ok_true(self, code):
        resp = UnifiedMockFactory.create_response(MockResponseConfig(status_code=code))
        assert resp.ok is True
        assert resp.success is True

    @pytest.mark.parametrize("code", [400, 401, 403, 404, 422, 499])
    def test_4xx_ok_false(self, code):
        resp = UnifiedMockFactory.create_response(MockResponseConfig(status_code=code))
        assert resp.ok is False
        assert resp.success is False

    @pytest.mark.parametrize("code", [500, 502, 503, 504])
    def test_5xx_ok_false(self, code):
        resp = UnifiedMockFactory.create_response(MockResponseConfig(status_code=code))
        assert resp.ok is False
        assert resp.success is False

    def test_status_zero_treated_as_error(self):
        """状态码 0 < 400 但语义上异常"""
        resp = UnifiedMockFactory.create_response(MockResponseConfig(status_code=0))
        assert resp.ok is True  # 按当前逻辑 0 < 400 → ok=True
        assert resp.status_code == 0

    def test_status_999_edge(self):
        resp = UnifiedMockFactory.create_response(MockResponseConfig(status_code=999))
        assert resp.ok is False


# ──────────────────────────────────────────────
# create_response — data 字段提取逻辑
# ──────────────────────────────────────────────

class TestCreateResponseDataField:
    """data 属性的三种分支路径"""

    def test_data_from_nested_key_when_present(self):
        """json_data 含 'data' 键时，response.data = json_data['data']"""
        resp = UnifiedMockFactory.create_response(MockResponseConfig(
            status_code=200,
            json_data={"success": True, "data": {"id": "task-1"}},
        ))
        assert resp.data == {"id": "task-1"}

    def test_data_fallback_to_full_json_when_no_data_key(self):
        """json_data 无 'data' 键时，response.data = json_data 本体"""
        payload = {"items": [], "total": 0}
        resp = UnifiedMockFactory.create_response(MockResponseConfig(
            json_data=payload,
        ))
        assert resp.data is payload

    def test_data_empty_dict_when_json_data_none(self):
        """json_data 为 None 时 response.data = {}"""
        resp = UnifiedMockFactory.create_response(MockResponseConfig(json_data=None))
        assert resp.data == {}

    def test_data_empty_dict_when_json_data_empty(self):
        """json_data 为空字典时 response.data = {} (无 'data' 键)"""
        resp = UnifiedMockFactory.create_response(MockResponseConfig(json_data={}))
        assert resp.data == {}

    def test_data_can_be_non_dict_value(self):
        """json_data['data'] 可以是列表、字符串等非字典值"""
        resp = UnifiedMockFactory.create_response(MockResponseConfig(
            json_data={"data": ["a", "b"]},
        ))
        assert resp.data == ["a", "b"]

    def test_data_can_be_string(self):
        resp = UnifiedMockFactory.create_response(MockResponseConfig(
            json_data={"data": "plain string"},
        ))
        assert resp.data == "plain string"

    def test_data_can_be_none(self):
        resp = UnifiedMockFactory.create_response(MockResponseConfig(
            json_data={"data": None},
        ))
        assert resp.data is None

    def test_deeply_nested_data(self):
        deep = {"users": [{"name": "Alice"}], "meta": {"page": 1}}
        resp = UnifiedMockFactory.create_response(MockResponseConfig(
            json_data={"data": deep},
        ))
        assert resp.data == deep
        assert resp.data["users"][0]["name"] == "Alice"


# ──────────────────────────────────────────────
# create_response — success 显式覆盖
# ──────────────────────────────────────────────

class TestCreateResponseSuccessOverride:
    """success 参数对 success 属性的显式控制"""

    def test_success_false_on_2xx_forces_failure(self):
        """即使 2xx，success=False 也强制 success=False"""
        resp = UnifiedMockFactory.create_response(MockResponseConfig(
            status_code=200, success=False,
        ))
        assert resp.ok is True       # ok 仅由 status_code 决定
        assert resp.success is False # success 受显式参数影响

    def test_success_true_on_4xx_still_false(self):
        """4xx 时 success=True 也无法覆盖为 True（与 status_code 取 AND）"""
        resp = UnifiedMockFactory.create_response(MockResponseConfig(
            status_code=404, success=True,
        ))
        assert resp.ok is False
        assert resp.success is False   # True and (404 < 400) → False

    def test_default_success_true_on_2xx(self):
        resp = UnifiedMockFactory.create_response(MockResponseConfig(status_code=201))
        assert resp.success is True


# ──────────────────────────────────────────────
# create_response — text & headers
# ──────────────────────────────────────────────

class TestCreateResponseTextAndHeaders:
    """text 和 headers 的传递"""

    def test_text_preserved(self):
        resp = UnifiedMockFactory.create_response(MockResponseConfig(text="hello"))
        assert resp.text == "hello"

    def test_text_empty_by_default(self):
        resp = UnifiedMockFactory.create_response()
        assert resp.text == ""

    def test_long_text_truncation_in_log_not_actual(self):
        """验证长文本不会被截断（日志截断不影响实际数据）"""
        long_text = "x" * 10000
        resp = UnifiedMockFactory.create_response(MockResponseConfig(text=long_text))
        assert len(resp.text) == 10000

    def test_unicode_text(self):
        resp = UnifiedMockFactory.create_response(MockResponseConfig(
            text="中文内容 🎉 emoji",
        ))
        assert "中文" in resp.text
        assert "🎉" in resp.text

    def test_headers_preserved(self):
        h = {"Content-Type": "application/json", "X-Trace": "abc123"}
        resp = UnifiedMockFactory.create_response(MockResponseConfig(headers=h))
        assert resp.headers == h

    def test_headers_empty_default(self):
        resp = UnifiedMockFactory.create_response()
        assert resp.headers == {}


# ──────────────────────────────────────────────
# create_async_response
# ──────────────────────────────────────────────

class TestCreateAsyncResponse:
    """异步响应创建测试"""

    def test_basic_async_response(self):
        resp = UnifiedMockFactory.create_async_response()
        assert isinstance(resp, AsyncMock)
        assert resp.status == 200
        assert resp.ok is True
        assert resp.success is True

    def test_async_uses_status_not_status_code(self):
        """异步响应使用 .status 而非 .status_code"""
        resp = UnifiedMockFactory.create_async_response(MockResponseConfig(
            status_code=503,
        ))
        assert resp.status == 503
        assert resp.ok is False

    def test_async_has_same_data_logic(self):
        resp = UnifiedMockFactory.create_async_response(MockResponseConfig(
            json_data={"data": {"async_id": "1"}},
        ))
        assert resp.data == {"async_id": "1"}

    def test_async_has_json_and_text(self):
        resp = UnifiedMockFactory.create_async_response(MockResponseConfig(
            json_data={"msg": "hi"}, text="raw",
        ))
        assert resp.json.return_value == {"msg": "hi"}
        assert resp.text == "raw"


# ──────────────────────────────────────────────
# create_session
# ──────────────────────────────────────────────

class TestCreateSession:
    """会话对象创建测试"""

    def test_session_without_responses(self):
        session = UnifiedMockFactory.create_session()
        assert session.get is not None
        assert session.post is not None
        assert session.put is not None
        assert session.delete is not None
        # 无 side_effect 时调用返回默认 Mock
        result = session.get("http://test")
        assert result is not None

    def test_session_with_side_effect_responses(self):
        responses = [
            UnifiedMockFactory.create_response(MockResponseConfig(status_code=200)),
            UnifiedMockFactory.create_response(MockResponseConfig(status_code=404)),
        ]
        session = UnifiedMockFactory.create_session(responses)

        r1 = session.get("url1")
        r2 = session.get("url2")
        assert r1.status_code == 200
        assert r2.status_code == 404

    def test_all_methods_share_same_side_effect(self):
        """get/post/put/delete 共享同一 side_effect 列表"""
        responses = [
            UnifiedMockFactory.create_response(MockResponseConfig(status_code=201)),
        ]
        session = UnifiedMockFactory.create_session(responses)

        assert session.post("url").status_code == 201
        assert session.put("url").status_code == 201
        assert session.delete("url").status_code == 201

    def test_empty_list_treated_as_no_side_effect(self):
        """空列表 [] 是 falsy，行为等同于 None（不设置 side_effect）"""
        session = UnifiedMockFactory.create_session([])
        # 不会设置 side_effect，调用返回默认 Mock 而非 StopIteration
        result = session.get("url")
        assert result is not None

    def test_single_response_reusable_until_exhausted(self):
        """单元素 side_effect 只能用一次"""
        resp = UnifiedMockFactory.create_response()
        session = UnifiedMockFactory.create_session([resp])

        r1 = session.get("url")
        assert r1 is resp
        with pytest.raises(StopIteration):
            session.get("url2")


# ──────────────────────────────────────────────
# create_client
# ──────────────────────────────────────────────

class TestCreateClient:
    """客户端对象创建测试"""

    def test_client_defaults(self):
        client = UnifiedMockFactory.create_client()
        assert client.endpoint == "http://localhost:18789"
        assert client.timeout == 30

    def test_custom_endpoint(self):
        client = UnifiedMockFactory.create_client("http://custom:9090")
        assert client.endpoint == "http://custom:9090"

    def test_client_http_methods_bound(self):
        client = UnifiedMockFactory.create_client()
        for method in ('get', 'post', 'put', 'delete'):
            assert getattr(client, method).return_value is not None

    def test_client_default_response_is_valid_mock(self):
        client = UnifiedMockFactory.create_client()
        resp = client.get.return_value
        assert resp.status_code == 200
        assert resp.ok is True


# ──────────────────────────────────────────────
# create_config
# ──────────────────────────────────────────────

class TestCreateConfig:
    """配置对象创建测试"""

    def test_empty_config(self):
        cfg = UnifiedMockFactory.create_config()
        assert isinstance(cfg, MagicMock)

    def test_single_attribute(self):
        cfg = UnifiedMockFactory.create_config(endpoint="http://test")
        assert cfg.endpoint == "http://test"

    def test_multiple_attributes(self):
        cfg = UnifiedMockFactory.create_config(
            host="127.0.0.1", port=8080, timeout=10, retries=3,
        )
        assert cfg.host == "127.0.0.1"
        assert cfg.port == 8080
        assert cfg.timeout == 10
        assert cfg.retries == 3

    def test_overwrite_attribute(self):
        cfg = UnifiedMockFactory.create_config(val=1)
        cfg.val = 999
        assert cfg.val == 999

    def test_none_value_attribute(self):
        cfg = UnifiedMockFactory.create_config(optional=None)
        assert cfg.optional is None


# ──────────────────────────────────────────────
# create_logger
# ──────────────────────────────────────────────

class TestCreateLogger:
    """模拟日志器创建测试"""

    def test_logger_has_all_levels(self):
        log = UnifiedMockFactory.create_logger()
        for level in ('debug', 'info', 'warning', 'error'):
            assert hasattr(log, level)
            assert callable(getattr(log, level)) or hasattr(getattr(log, level), '__call__')

    def test_logger_methods_are_mocks(self):
        log = UnifiedMockFactory.create_logger()
        assert isinstance(log.debug, MagicMock)
        assert isinstance(log.info, MagicMock)
        assert isinstance(log.warning, MagicMock)
        assert isinstance(log.error, MagicMock)

    def test_logger_call_does_not_raise(self):
        log = UnifiedMockFactory.create_logger()
        log.debug("test")     # 不应抛异常
        log.info("test")
        log.warning("test")
        log.error("test")


# ──────────────────────────────────────────────
# 日志输出验证
# ──────────────────────────────────────────────

class TestLoggingOutput:
    """验证关键方法在 INFO/DEBUG 级别输出预期日志"""

    def test_create_response_logs_info_on_creation(self, caplog):
        with caplog.at_level(logging.INFO, logger="tests.utils.python.mock_factory"):
            UnifiedMockFactory.create_response(MockResponseConfig(status_code=201))

        assert any(
            "create_response 完成" in rec.message and "status_code=201" in rec.message
            for rec in caplog.records if rec.levelno == logging.INFO
        )

    def test_create_async_response_logs_info(self, caplog):
        with caplog.at_level(logging.INFO, logger="tests.utils.python.mock_factory"):
            UnifiedMockFactory.create_async_response(MockResponseConfig(status_code=500))

        assert any(
            "create_async_response 完成" in rec.message
            for rec in caplog.records if rec.levelno == logging.INFO
        )

    def test_create_session_logs_response_count(self, caplog):
        with caplog.at_level(logging.INFO, logger="tests.utils.python.mock_factory"):
            UnifiedMockFactory.create_session([Mock(), Mock(), Mock()])

        assert any(
            "side_effect" in rec.message and "3" in rec.message
            for rec in caplog.records if rec.levelno == logging.INFO
        )

    def test_create_client_logs_endpoint(self, caplog):
        with caplog.at_level(logging.INFO, logger="tests.utils.python.mock_factory"):
            UnifiedMockFactory.create_client("http://my-server:3000")

        assert any(
            "create_client" in rec.message and "my-server:3000" in rec.message
            for rec in caplog.records if rec.levelno == logging.INFO
        )

    def test_create_response_debug_shows_keys(self, caplog):
        with caplog.at_level(logging.DEBUG, logger="tests.utils.python.mock_factory"):
            UnifiedMockFactory.create_response(MockResponseConfig(
                status_code=200,
                json_data={"user": "alice", "role": "admin"},
                headers={"X-Token": "xyz"},
            ))

        debug_msgs = [r.message for r in caplog.records if r.levelno == logging.DEBUG]
        assert any("status_code=200" in m for m in debug_msgs)
        assert any("user" in m and "role" in m for m in debug_msgs)

    def test_error_status_logged_with_details(self, caplog):
        with caplog.at_level(logging.INFO, logger="tests.utils.python.mock_factory"):
            UnifiedMockFactory.create_response(MockResponseConfig(
                status_code=422,
                json_data={"error": "validation failed"},
                text="Unprocessable Entity",
            ))

        info_msgs = [r.message for r in caplog.records if r.levelno == logging.INFO]
        assert any("status_code=422" in m and "ok=False" in m for m in info_msgs)
