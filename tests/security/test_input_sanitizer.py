# AgentOS 输入净化安全测试
# Version: 1.0.0.9
# Last updated: 2026-04-04

"""
AgentOS 输入净化安全测试模块。

测试输入净化器的各种安全防护能力，包括 XSS 防护、SQL 注入防护、
命令注入防护、路径遍历防护等。
"""

import pytest
import re
import html
from typing import Dict, Any, List, Optional, Callable
from unittest.mock import Mock, MagicMock, patch
from dataclasses import dataclass
from enum import Enum

import sys
import os
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', 'toolkit', 'python')))


# ============================================================
# 测试标记
# ============================================================

pytestmark = pytest.mark.security


# ============================================================
# 数据类定义
# ============================================================

@dataclass
class SanitizationResult:
    """输入净化结果数据类"""
    original: str
    sanitized: str
    threats_detected: List[str]
    is_safe: bool

    def to_dict(self) -> Dict[str, Any]:
        return {
            "original": self.original,
            "sanitized": self.sanitized,
            "threats_detected": self.threats_detected,
            "is_safe": self.is_safe
        }


class ThreatType(Enum):
    """威胁类型枚举"""
    XSS = "xss"
    SQL_INJECTION = "sql_injection"
    COMMAND_INJECTION = "command_injection"
    PATH_TRAVERSAL = "path_traversal"
    LDAP_INJECTION = "ldap_injection"
    XML_INJECTION = "xml_injection"


class SanitizationLevel(Enum):
    """净化级别枚举"""
    STRICT = "strict"
    MODERATE = "moderate"
    PERMISSIVE = "permissive"


# ============================================================
# 模拟的 InputSanitizer 类（用于测试）
# ============================================================

class InputSanitizer:
    """
    输入净化器模拟实现

    提供多级输入净化功能，用于测试验证。
    """

    def __init__(self, level: SanitizationLevel = SanitizationLevel.MODERATE):
        """
        初始化输入净化器

        Args:
            level: 净化级别
        """
        self.level = level
        self._threat_patterns = {
            ThreatType.XSS: [
                r'<script[^>]*>.*?</script>',
                r'javascript\s*:',
                r'on\w+\s*=',
                r'<img[^>]+onerror',
                r'<svg[^>]+onload',
                r'expression\s*\(',
            ],
            ThreatType.SQL_INJECTION: [
                r"'\s*OR\s*'[^']*'\s*=\s*'",
                r"'\s*;\s*DROP\s+TABLE",
                r"UNION\s+SELECT",
                r"'\s*--",
                r";\s*SELECT\s+.*FROM",
            ],
            ThreatType.COMMAND_INJECTION: [
                r'[;&|`$]',
                r'\$\(',
                r'`[^`]*`',
                r'\|\|',
                r'&&',
            ],
            ThreatType.PATH_TRAVERSAL: [
                r'\.\./|\.\.\\',
                r'%2e%2e[/\\%]',
                r'\.\.%252f',
                r'/etc/passwd',
                r'windows\\system32',
            ],
            ThreatType.LDAP_INJECTION: [
                r'\)\(.*\)',
                r'\|\(.*=.*\)',
                r'\(&.*=\)',
                r'\*\)',
            ],
            ThreatType.XML_INJECTION: [
                r'<!\[CDATA\[',
                r']]>',
                r'<!DOCTYPE\s',
                r'<!ENTITY\s',
            ],
        }

    def sanitize(self, input_data: str) -> SanitizationResult:
        """
        净化输入数据

        Args:
            input_data: 原始输入字符串

        Returns:
            SanitizationResult: 净化结果
        """
        if not isinstance(input_data, str):
            return SanitizationResult(
                original=str(input_data),
                sanitized="",
                threats_detected=["invalid_type"],
                is_safe=False
            )

        result = input_data
        threats_found = []

        for threat_type, patterns in self._threat_patterns.items():
            for pattern in patterns:
                if re.search(pattern, result, re.IGNORECASE):
                    if threat_type not in threats_found:
                        threats_found.append(threat_type)

                    # 根据级别进行不同处理
                    if self.level == SanitizationLevel.STRICT:
                        result = html.escape(result)
                    elif self.level == SanitizationLevel.MODERATE:
                        result = re.sub(pattern, '', result, flags=re.IGNORECASE)

        is_safe = len(threats_found) == 0

        return SanitizationResult(
            original=input_data,
            sanitized=result,
            threats_detected=[t.value for t in threats_found],
            is_safe=is_safe
        )

    def validate_path(self, path: str) -> bool:
        """
        验证路径安全性

        Args:
            path: 文件路径

        Returns:
            bool: 是否为安全路径
        """
        if not isinstance(path, str):
            return False

        dangerous_patterns = [
            '../', '..\\', '%2e%2e', '..%252f',
            '/etc/', 'windows\\', '..//'
        ]

        path_lower = path.lower()
        for pattern in dangerous_patterns:
            if pattern in path_lower:
                return False

        return True


# ============================================================
# 单元测试用例
# ============================================================

class TestInputSanitizerBasic:
    """
    输入净化器基础功能测试
    """

    @pytest.fixture
    def sanitizer(self):
        """创建净化器实例"""
        return InputSanitizer(level=SanitizationLevel.STRICT)

    def test_sanitize_empty_string(self, sanitizer):
        """测试空字符串净化"""
        result = sanitizer.sanitize("")
        assert result.sanitized == ""
        assert result.is_safe is True

    def test_sanitize_normal_text(self, sanitizer):
        """测试正常文本净化"""
        normal_text = "Hello, World! This is a test."
        result = sanitizer.sanitize(normal_text)
        assert result.sanitized == normal_text
        assert result.is_safe is True

    def test_sanitize_non_string_input(self, sanitizer):
        """测试非字符串输入"""
        result = sanitizer.sanitize(12345)
        assert result.sanitized == ""
        assert result.is_safe is False
        assert "invalid_type" in result.threats_detected


class TestXSSProtection:
    """
    XSS 防护测试
    """

    @pytest.fixture
    def sanitizer(self):
        return InputSanitizer(level=SanitizationLevel.STRICT)

    @pytest.mark.parametrize("payload", [
        '<script>alert("XSS")</script>',
        '<img src=x onerror=alert("XSS")>',
        'javascript:alert("XSS")',
        '<svg onload=alert("XSS")>',
        "'\"><script>alert('XSS')</script>",
        "<body onload=alert('XSS')>",
        "<div onmouseover='alert(1)'>test</div>",
        "javascript:void(document.cookie)",
        "<iframe src='javascript:alert(1)'></iframe>",
        "<a href='javascript:alert(1)'>click</a>",
        "<style>body{background:url('javascript:alert(1)')}</style>",
        "<object data='javascript:alert(1)'></object>",
        "<embed src='javascript:alert(1)'>",
    ])
    def test_xss_payload_detection(self, sanitizer, payload):
        """检测XSS攻击载荷"""
        result = sanitizer.sanitize(payload)
        assert result.is_safe is False or "xss" in result.threats_detected

    def test_xss_strict_sanitization(self, sanitizer):
        """严格模式下的XSS净化"""
        payload = "<script>alert('XSS')</script>"
        result = sanitizer.sanitize(payload)
        assert "<script>" not in result.sanitized.lower()
        assert "</script>" not in result.sanitized.lower()


class TestSQLInjectionProtection:
    """
    SQL 注入防护测试
    """

    @pytest.fixture
    def sanitizer(self):
        return InputSanitizer(level=SanitizationLevel.MODERATE)

    @pytest.mark.parametrize("payload", [
        "' OR '1'='1",
        "'; DROP TABLE users; --",
        "1; SELECT * FROM users",
        "' UNION SELECT NULL --",
        "admin'--",
        "1' AND '1'='1",
        "\" OR \"1\"=\"1",
        "' OR 1=1 --",
        "'; INSERT INTO users VALUES('hacked'); --",
        "1'; UPDATE users SET password='hacked' WHERE username='admin'; --",
    ])
    def test_sql_injection_detection(self, sanitizer, payload):
        """检测SQL注入载荷"""
        result = sanitizer.sanitize(payload)
        assert result.is_safe is False or "sql_injection" in result.threats_detected


class TestCommandInjectionProtection:
    """
    命令注入防护测试
    """

    @pytest.fixture
    def sanitizer(self):
        return InputSanitizer(level=SanitizationLevel.STRICT)

    @pytest.mark.parametrize("payload", [
        "; ls -la",
        "| cat /etc/passwd",
        "$(whoami)",
        "`id`",
        "&& rm -rf /",
        "|| echo hacked",
        "$HOME",
        "${IFS}cat${IFS}/etc/passwd",
    ])
    def test_command_injection_detection(self, sanitizer, payload):
        """检测命令注入载荷"""
        result = sanitizer.sanitize(payload)
        assert result.is_safe is False or "command_injection" in result.threats_detected


class TestPathTraversalProtection:
    """
    路径遍历防护测试
    """

    @pytest.fixture
    def sanitizer(self):
        return InputSanitizer()

    @pytest.mark.parametrize("path", [
        "../../../etc/passwd",
        "..\\..\\..\\windows\\system32\\config\\sam",
        "....//....//....//etc/passwd",
        "%2e%2e%2f%2e%2e%2f%2e%2e%2fetc/passwd",
        "..%252f..%252f..%252fetc/passwd",
        "/var/www/../../etc/shadow",
        "....\\\\....\\\\....\\\\windows\\\\system32",
    ])
    def test_path_traversal_detection(self, sanitizer, path):
        """检测路径遍历载荷"""
        is_safe = sanitizer.validate_path(path)
        assert is_safe is False

    @pytest.mark.parametrize("path", [
        "/home/user/documents/file.txt",
        "./relative/path/to/file.txt",
        "C:\\Users\\user\\Documents\\file.txt",
        "/var/log/app.log",
        "",
    ])
    def test_valid_path_validation(self, sanitizer, path):
        """验证合法路径"""
        is_safe = sanitizer.validate_path(path)
        assert is_safe is True


class TestSanitizationLevels:
    """
    净化级别测试
    """

    def test_strict_level(self):
        """严格级别测试"""
        strict_sanitizer = InputSanitizer(level=SanitizationLevel.STRICT)
        payload = "<script>alert('XSS')</script>"
        result = strict_sanitizer.sanitize(payload)
        assert "&lt;" in result.sanitized
        assert "&gt;" in result.sanitized

    def test_moderate_level(self):
        """中等级别测试"""
        moderate_sanitizer = InputSanitizer(level=SanitizationLevel.MODERATE)
        payload = "<script>alert('XSS')</script>"
        result = moderate_sanitizer.sanitize(payload)
        assert "<script>" not in result.sanitized.lower()

    def test_permissive_level(self):
        """宽松级别测试"""
        permissive_sanitizer = InputSanitizer(level=SanitizationLevel.PERMISSIVE)
        safe_text = "This is safe text"
        result = permissive_sanitizer.sanitize(safe_text)
        assert result.sanitized == safe_text


class TestEdgeCases:
    """
    边界条件测试
    """

    @pytest.fixture
    def sanitizer(self):
        return InputSanitizer()

    def test_unicode_characters(self, sanitizer):
        """Unicode字符处理"""
        unicode_input = "你好世界 🌍 日本語 한국어 العربية"
        result = sanitizer.sanitize(unicode_input)
        assert result.sanitized == unicode_input

    def test_very_long_string(self, sanitizer):
        """超长字符串处理"""
        long_string = "A" * 10000
        result = sanitizer.sanitize(long_string)
        assert len(result.sanitized) > 0

    def test_special_characters(self, sanitizer):
        """特殊字符处理"""
        special_chars = "!@#$%^&*()_+-=[]{}|;:,.<>?/~`"
        result = sanitizer.sanitize(special_chars)
        assert result.is_safe is True

    def test_null_bytes(self, sanitizer):
        """空字节处理"""
        null_input = "test\x00string"
        result = sanitizer.sanitize(null_input)
        assert "\x00" not in result.sanitized

    def test_html_entities(self, sanitizer):
        """HTML实体处理"""
        html_entity = "&lt;script&gt;alert('XSS')&lt;/script&gt;"
        result = sanitizer.sanitize(html_entity)
        assert result.is_safe is True


if __name__ == "__main__":
    pytest.main([__file__, "-v", "--tb=short"])
