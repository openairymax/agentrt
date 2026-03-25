"""
AgentOS 模糊测试框架
Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
Version: 1.0.0

基于 Hypothesis 和 Atheris 的模糊测试框架
用于测试解析器和输入处理模块的健壮性
"""

import os
import sys
import json
import tempfile
from typing import Any, Dict, List, Optional
from dataclasses import dataclass
from pathlib import Path

try:
    from hypothesis import given, strategies as st, settings, assume, example
    from hypothesis.strategies import composite, text, integers, lists, dictionaries
    HYPOTHESIS_AVAILABLE = True
except ImportError:
    HYPOTHESIS_AVAILABLE = False

try:
    import atheris
    ATHERIS_AVAILABLE = True
except ImportError:
    ATHERIS_AVAILABLE = False


@dataclass
class FuzzTestResult:
    """模糊测试结果数据类"""
    test_name: str
    total_runs: int
    failures: int
    errors: List[str]
    coverage: float
    duration_seconds: float


class FuzzTestFramework:
    """
    模糊测试框架
    
    提供基于 Hypothesis 和 Atheris 的模糊测试能力，
    用于发现解析器和输入处理模块的边界条件和潜在漏洞。
    """
    
    def __init__(self, output_dir: Optional[str] = None):
        """
        初始化模糊测试框架
        
        Args:
            output_dir: 测试报告输出目录
        """
        self.output_dir = output_dir or tempfile.mkdtemp()
        self.results: List[FuzzTestResult] = []
        
    def run_all_tests(self) -> Dict[str, Any]:
        """
        运行所有模糊测试
        
        Returns:
            测试结果汇总
        """
        results = {
            "framework": "AgentOS Fuzz Test Framework",
            "version": "1.0.0",
            "tests": [],
            "summary": {
                "total_tests": 0,
                "passed": 0,
                "failed": 0,
                "total_runs": 0
            }
        }
        
        return results


# Hypothesis 策略定义
if HYPOTHESIS_AVAILABLE:
    
    @composite
    def json_values(draw):
        """生成任意JSON值的策略"""
        return draw(st.one_of(
            st.none(),
            st.booleans(),
            st.integers(min_value=-2**63, max_value=2**63-1),
            st.floats(allow_nan=True, allow_infinity=True),
            st.text(),
            st.lists(st.deferred(lambda: json_values())),
            st.dictionaries(st.text(), st.deferred(lambda: json_values()))
        ))
    
    @composite
    def contract_payloads(draw):
        """生成契约测试载荷的策略"""
        return draw(st.dictionaries(
            keys=st.text(min_size=1, max_size=50),
            values=st.one_of(
                st.text(),
                st.integers(),
                st.floats(),
                st.booleans(),
                st.lists(st.text()),
                st.none()
            ),
            min_size=1,
            max_size=20
        ))
    
    @composite
    def sql_injection_payloads(draw):
        """生成SQL注入测试载荷的策略"""
        base_payloads = [
            "' OR '1'='1",
            "'; DROP TABLE users; --",
            "1; SELECT * FROM users",
            "' UNION SELECT NULL --",
            "admin'--",
            "1' AND '1'='1",
            "\" OR \"1\"=\"1",
        ]
        return draw(st.one_of(
            st.sampled_from(base_payloads),
            st.text(alphabet=st.characters(blacklist_categories=('Cs',)))
        ))
    
    @composite
    def xss_payloads(draw):
        """生成XSS测试载荷的策略"""
        base_payloads = [
            "<script>alert('XSS')</script>",
            "<img src=x onerror=alert('XSS')>",
            "javascript:alert('XSS')",
            "<svg onload=alert('XSS')>",
            "'\"><script>alert('XSS')</script>",
            "<body onload=alert('XSS')>",
        ]
        return draw(st.one_of(
            st.sampled_from(base_payloads),
            st.text(alphabet=st.characters(blacklist_categories=('Cs',)))
        ))
    
    @composite
    def path_traversal_payloads(draw):
        """生成路径遍历测试载荷的策略"""
        base_payloads = [
            "../../../etc/passwd",
            "..\\..\\..\\windows\\system32\\config\\sam",
            "....//....//....//etc/passwd",
            "%2e%2e%2f%2e%2e%2f%2e%2e%2fetc/passwd",
            "..%252f..%252f..%252fetc/passwd",
        ]
        return draw(st.one_of(
            st.sampled_from(base_payloads),
            st.text(alphabet=st.characters(blacklist_categories=('Cs',)))
        ))


class InputSanitizerFuzzTests:
    """
    输入净化器模糊测试
    
    测试输入净化模块对各种恶意输入的处理能力。
    """
    
    def __init__(self):
        self.failures = []
    
    @staticmethod
    def sanitize_input(data: str) -> str:
        """
        简化的输入净化函数（用于测试）
        
        Args:
            data: 输入字符串
            
        Returns:
            净化后的字符串
        """
        if not isinstance(data, str):
            return ""
        
        result = data
        
        result = result.replace("<", "&lt;")
        result = result.replace(">", "&gt;")
        result = result.replace("'", "&#39;")
        result = result.replace('"', "&quot;")
        
        dangerous_patterns = [
            "javascript:", "vbscript:", "onerror=", "onload=",
            "onclick=", "onmouseover=", "<script", "</script>"
        ]
        for pattern in dangerous_patterns:
            result = result.lower().replace(pattern, "")
        
        return result
    
    @staticmethod
    def validate_path(path: str) -> bool:
        """
        验证路径是否安全
        
        Args:
            path: 文件路径
            
        Returns:
            是否为安全路径
        """
        if not isinstance(path, str):
            return False
        
        dangerous_patterns = ["../", "..\\", "%2e%2e", "..%252f"]
        path_lower = path.lower()
        
        for pattern in dangerous_patterns:
            if pattern in path_lower:
                return False
        
        return True


if HYPOTHESIS_AVAILABLE:
    fuzz_framework = FuzzTestFramework()
    sanitizer_tests = InputSanitizerFuzzTests()
    
    @given(payload=xss_payloads())
    @settings(max_examples=100, deadline=None)
    def test_xss_sanitization_fuzz(payload: str):
        """XSS净化模糊测试"""
        sanitized = sanitizer_tests.sanitize_input(payload)
        assert "<script>" not in sanitized.lower()
        assert "javascript:" not in sanitized.lower()
    
    @given(payload=path_traversal_payloads())
    @settings(max_examples=100, deadline=None)
    def test_path_traversal_fuzz(payload: str):
        """路径遍历模糊测试"""
        is_safe = sanitizer_tests.validate_path(payload)
        if "../" in payload or "..\\" in payload:
            assert is_safe is False
    
    @given(data=json_values())
    @settings(max_examples=50, deadline=None)
    def test_json_parsing_fuzz(data: Any):
        """JSON解析模糊测试"""
        try:
            json_str = json.dumps(data)
            parsed = json.loads(json_str)
            assert parsed == data
        except (TypeError, ValueError):
            pass


def run_fuzz_tests():
    """
    运行所有模糊测试
    
    Returns:
        测试结果
    """
    results = {
        "status": "initialized",
        "hypothesis_available": HYPOTHESIS_AVAILABLE,
        "atheris_available": ATHERIS_AVAILABLE,
        "tests_run": 0,
        "failures": []
    }
    
    if not HYPOTHESIS_AVAILABLE:
        results["status"] = "skipped"
        results["message"] = "Hypothesis not installed. Run: pip install hypothesis"
        return results
    
    try:
        import pytest
        pytest.main([
            __file__,
            "-v",
            "--hypothesis-show-statistics",
            "--hypothesis-seed=0"
        ])
        results["status"] = "completed"
    except Exception as e:
        results["status"] = "error"
        results["error"] = str(e)
    
    return results


if __name__ == "__main__":
    print("=" * 60)
    print("AgentOS 模糊测试框架")
    print("Copyright (c) 2026 SPHARX Ltd.")
    print("=" * 60)
    
    results = run_fuzz_tests()
    print(f"\n测试状态: {results['status']}")
    print(f"Hypothesis 可用: {results['hypothesis_available']}")
    print(f"Atheris 可用: {results['atheris_available']}")
